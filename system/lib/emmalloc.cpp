#if 0
/*
 * Copyright 2018 The Emscripten Authors.  All rights reserved.
 * Emscripten is available under two separate licenses, the MIT license and the
 * University of Illinois/NCSA Open Source License.  Both these licenses can be
 * found in the LICENSE file.
 *
 * Simple minimalistic but efficient malloc/free.
 *
 * Assumptions:
 *
 *  - Pointers are 32-bit.
 *  - Single-threaded.
 *  - sbrk() is used, and nothing else.
 *  - sbrk() will not be accessed by anyone else.
 *  - sbrk() is very fast in most cases (internal wasm call).
 *
 * Invariants:
 *
 *  - Metadata is 8 bytes, allocation payload is a
 *    multiple of 8 bytes.
 *  - All regions of memory are adjacent.
 *  - Due to the above, after initial alignment fixing, all
 *    regions are aligned.
 *  - A region is either in use (used payload > 0) or not.
 *    Used regions may be adjacent, and a used and unused region
 *    may be adjacent, but not two unused ones - they would be
 *    merged.
 *  - A used region always has minimal space at the end - we
 *    split off extra space when possible immediately.
 *
 * Debugging:
 *
 *  - If not NDEBUG, runtime assert()s are in use.
 *  - If EMMALLOC_DEBUG is defined, a large amount of extra checks are done.
 *  - If EMMALLOC_DEBUG_LOG is defined, a lot of operations are logged
 *    out, in addition to EMMALLOC_DEBUG.
 *  - Debugging and logging uses EM_ASM, not printf etc., to minimize any
 *    risk of debugging or logging depending on malloc.
 *
 * TODO
 *
 *  - Optimizations for small allocations that are not multiples of 8, like
 *    12 and 20 (which take 24 and 32 bytes respectively)
 *
 */

#include <assert.h>
#include <emscripten.h>
#include <limits.h> // CHAR_BIT
#include <malloc.h> // mallinfo
#include <string.h> // for memcpy, memset
#include <unistd.h> // for sbrk()

#define EMMALLOC_EXPORT __attribute__((__weak__, __visibility__("default")))

// Assumptions

static_assert(sizeof(void*) == 4, "32-bit system");
static_assert(sizeof(size_t) == 4, "32-bit system");
static_assert(sizeof(int) == 4, "32-bit system");

#define SIZE_T_BIT (sizeof(size_t) * CHAR_BIT)

static_assert(CHAR_BIT == 8, "standard char bit size");
static_assert(SIZE_T_BIT == 32, "standard size_t bit size");

// Debugging

#ifdef EMMALLOC_DEBUG_LOG
#ifndef EMMALLOC_DEBUG
#define EMMALLOC_DEBUG
#endif
#endif

#ifdef EMMALLOC_DEBUG
// Forward declaration for convenience.
static void emmalloc_validate_all();
#endif
#ifdef EMMALLOC_DEBUG
// Forward declaration for convenience.
static void emmalloc_dump_all();
#endif

// Math utilities

static bool isPowerOf2(size_t x) { return __builtin_popcount(x) == 1; }

static size_t lowerBoundPowerOf2(size_t x) {
  if (x == 0)
    return 1;
  // e.g. 5 is 0..0101, so clz is 29, and we want
  // 4 which is 1 << 2, so the result should be 2
  return SIZE_T_BIT - 1 - __builtin_clz(x);
}

// Constants

// All allocations are aligned to this value.
static const size_t ALIGNMENT = 8;

// Even allocating 1 byte incurs this much actual payload
// allocation. This is our minimum bin size.
static const size_t ALLOC_UNIT = ALIGNMENT;

// How big the metadata is in each region. It is convenient
// that this is identical to the above values.
static const size_t METADATA_SIZE = ALLOC_UNIT;

// How big a minimal region is.
static const size_t MIN_REGION_SIZE = METADATA_SIZE + ALLOC_UNIT;

static_assert(ALLOC_UNIT == ALIGNMENT, "expected size of allocation unit");
static_assert(METADATA_SIZE == ALIGNMENT, "expected size of metadata");

// Constant utilities

// Align a pointer, increasing it upwards as necessary
static size_t alignUp(size_t ptr) { return (ptr + ALIGNMENT - 1) & -ALIGNMENT; }

static void* alignUpPointer(void* ptr) { return (void*)alignUp(size_t(ptr)); }

//
// Data structures
//

struct Region;

// Information memory that is a free list, i.e., may
// be reused.
// Note how this can fit instead of the payload (as
// the payload is a multiple of MIN_ALLOC).
struct FreeInfo {
  // free lists are doubly-linked lists
  FreeInfo* _prev;
  FreeInfo* _next;

  FreeInfo*& prev() { return _prev; }
  FreeInfo*& next() { return _next; }
};

static_assert(sizeof(FreeInfo) == ALLOC_UNIT, "expected size of free info");

// The first region of memory.
static Region* firstRegion = nullptr;

// The last region of memory. It's important to know the end
// since we may append to it.
static Region* lastRegion = nullptr;

// A contiguous region of memory. Metadata at the beginning describes it,
// after which is the "payload", the sections that user code calling
// malloc can use.
struct Region {
  // Whether this region is in use or not.
  size_t _used : 1;

  // The total size of the section of memory this is associated
  // with and contained in.
  // That includes the metadata itself and the payload memory after,
  // which includes the used and unused portions of it.
  // FIXME: Shift by 1, as our size is even anyhow?
  //        Or, disallow allocation of half the total space or above.
  //        Browsers barely allow allocating 2^31 anyhow, so inside that
  //        space we can just allocate something smaller than it.
  size_t _totalSize : 31;

  // Each memory area knows its previous neighbor, as we hope to merge them.
  // To compute the next neighbor we can use the total size, and to know
  // if a neighbor exists we can compare the region to lastRegion
  Region* _prev;

  // Up to here was the fixed metadata, of size 16. The rest is either
  // the payload, or freelist info.
  union {
    FreeInfo _freeInfo;
    char _payload[];
  };

  size_t getTotalSize() { return _totalSize; }
  void setTotalSize(size_t x) { _totalSize = x; }
  void incTotalSize(size_t x) { _totalSize += x; }
  void decTotalSize(size_t x) { _totalSize -= x; }

  size_t getUsed() { return _used; }
  void setUsed(size_t x) { _used = x; }

  Region*& prev() { return _prev; }
  // The next region is not, as we compute it on the fly
  Region* next() {
    if (this != lastRegion) {
      return (Region*)((char*)this + getTotalSize());
    } else {
      return nullptr;
    }
  }
  FreeInfo& freeInfo() { return _freeInfo; }
  // The payload is special, we just return its address, as we
  // never want to modify it ourselves.
  char* payload() { return &_payload[0]; }
};

// Region utilities

static void* getPayload(Region* region) {
  assert(((char*)&region->freeInfo()) - ((char*)region) == METADATA_SIZE);
  assert(region->getUsed());
  return region->payload();
}

static Region* fromPayload(void* payload) { return (Region*)((char*)payload - METADATA_SIZE); }

static Region* fromFreeInfo(FreeInfo* freeInfo) {
  return (Region*)((char*)freeInfo - METADATA_SIZE);
}

static size_t getMaxPayload(Region* region) { return region->getTotalSize() - METADATA_SIZE; }

// TODO: move into class, make more similar to next()
static void* getAfter(Region* region) { return ((char*)region) + region->getTotalSize(); }

// Globals

// TODO: For now we have a single global space for all allocations,
//       but for multithreading etc. we may want to generalize that.

// A freelist (a list of Regions ready for re-use) for all
// power of 2 payload sizes (only the ones from ALIGNMENT
// size and above are relevant, though). The freelist at index
// K contains regions of memory big enough to contain at least
// 2^K bytes.
//
// Note that there is no freelist for 2^32, as that amount can
// never be allocated.

static const size_t MIN_FREELIST_INDEX = 3;  // 8 == ALLOC_UNIT
static const size_t MAX_FREELIST_INDEX = 32; // uint32_t

static FreeInfo* freeLists[MAX_FREELIST_INDEX] = {nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
  nullptr, nullptr, nullptr, nullptr, nullptr};

// Global utilities

// The freelist index is where we would appear in a freelist if
// we were one. It is a list of items of size at least the power
// of 2 that lower bounds us.
static size_t getFreeListIndex(size_t size) {
  assert(1 << MIN_FREELIST_INDEX == ALLOC_UNIT);
  assert(size > 0);
  if (size < ALLOC_UNIT)
    size = ALLOC_UNIT;
  // We need a lower bound here, as the list contains things
  // that can contain at least a power of 2.
  size_t index = lowerBoundPowerOf2(size);
  assert(MIN_FREELIST_INDEX <= index && index < MAX_FREELIST_INDEX);
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.getFreeListIndex " + [ $0, $1 ])}, size, index);
#endif
  return index;
}

// The big-enough freelist index is the index of the freelist of
// items that are all big enough for us. This is computed using
// an upper bound power of 2.
static size_t getBigEnoughFreeListIndex(size_t size) {
  assert(size > 0);
  size_t index = getFreeListIndex(size);
  // If we're a power of 2, the lower and upper bounds are the
  // same. Otherwise, add one.
  if (!isPowerOf2(size))
    index++;
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.getBigEnoughFreeListIndex " + [ $0, $1 ])}, size, index);
#endif
  return index;
}

// Items in the freelist at this index must be at least this large.
static size_t getMinSizeForFreeListIndex(size_t index) { return 1 << index; }

// Items in the freelist at this index must be smaller than this.
static size_t getMaxSizeForFreeListIndex(size_t index) { return 1 << (index + 1); }

static void removeFromFreeList(Region* region) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.removeFromFreeList " + $0)}, region);
#endif
  size_t index = getFreeListIndex(getMaxPayload(region));
  FreeInfo* freeInfo = &region->freeInfo();
  if (freeLists[index] == freeInfo) {
    freeLists[index] = freeInfo->next();
  }
  if (freeInfo->prev()) {
    freeInfo->prev()->next() = freeInfo->next();
  }
  if (freeInfo->next()) {
    freeInfo->next()->prev() = freeInfo->prev();
  }
}

static void addToFreeList(Region* region) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.addToFreeList " + $0)}, region);
#endif
  assert(getAfter(region) <= sbrk(0));
  size_t index = getFreeListIndex(getMaxPayload(region));
  FreeInfo* freeInfo = &region->freeInfo();
  FreeInfo* last = freeLists[index];
  freeLists[index] = freeInfo;
  freeInfo->prev() = nullptr;
  freeInfo->next() = last;
  if (last) {
    last->prev() = freeInfo;
  }
}

// Receives a region that has just become free (and is not yet in a freelist).
// Tries to merge it into a region before or after it to which it is adjacent.
static int mergeIntoExistingFreeRegion(Region* region) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.mergeIntoExistingFreeRegion " + $0)}, region);
#endif
  assert(getAfter(region) <= sbrk(0));
  int merged = 0;
  Region* prev = region->prev();
  Region* next = region->next();
  if (prev && !prev->getUsed()) {
    // Merge them.
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.mergeIntoExistingFreeRegion merge into prev " + $0)}, prev);
#endif
    removeFromFreeList(prev);
    prev->incTotalSize(region->getTotalSize());
    if (next) {
      next->prev() = prev; // was: region
    } else {
      assert(region == lastRegion);
      lastRegion = prev;
    }
    if (next) {
      // We may also be able to merge with the next, keep trying.
      if (!next->getUsed()) {
#ifdef EMMALLOC_DEBUG_LOG
        EM_ASM({out("  emmalloc.mergeIntoExistingFreeRegion also merge into next " + $0)}, next);
#endif
        removeFromFreeList(next);
        prev->incTotalSize(next->getTotalSize());
        if (next != lastRegion) {
          next->next()->prev() = prev;
        } else {
          lastRegion = prev;
        }
      }
    }
    addToFreeList(prev);
    return 1;
  }
  if (next && !next->getUsed()) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.mergeIntoExistingFreeRegion merge into next " + $0)}, next);
#endif
    // Merge them.
    removeFromFreeList(next);
    region->incTotalSize(next->getTotalSize());
    if (next != lastRegion) {
      next->next()->prev() = region;
    } else {
      lastRegion = region;
    }
    addToFreeList(region);
    return 1;
  }
  return 0;
}

static void stopUsing(Region* region) {
  region->setUsed(0);
  if (!mergeIntoExistingFreeRegion(region)) {
    addToFreeList(region);
  }
}

// Grow a region. If not in use, we may need to be in another
// freelist.
// TODO: We can calculate that, to save some work.
static void growRegion(Region* region, size_t sizeDelta) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.growRegion " + [ $0, $1 ])}, region, sizeDelta);
#endif
  if (!region->getUsed()) {
    removeFromFreeList(region);
  }
  region->incTotalSize(sizeDelta);
  if (!region->getUsed()) {
    addToFreeList(region);
  }
}

// Extends the last region to a certain payload size. Returns 1 if successful,
// 0 if an error occurred in sbrk().
static int extendLastRegion(size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.extendLastRegionToSize " + $0)}, size);
#endif
  size_t reusable = getMaxPayload(lastRegion);
  size_t sbrkSize = alignUp(size) - reusable;
  void* ptr = sbrk(sbrkSize);
  if (ptr == (void*)-1) {
    // sbrk() failed, we failed.
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.extendLastRegion sbrk failure")});
#endif
    return 0;
  }
  // sbrk() should give us new space right after the last region.
  assert(ptr == getAfter(lastRegion));
  // Increment the region's size.
  growRegion(lastRegion, sbrkSize);
  return 1;
}

static void possiblySplitRemainder(Region* region, size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.possiblySplitRemainder " + [ $0, $1 ])}, region, size);
#endif
  size_t payloadSize = getMaxPayload(region);
  assert(payloadSize >= size);
  size_t extra = payloadSize - size;
  // Room for a minimal region is definitely worth splitting. Otherwise,
  // if we don't have room for a full region, but we do have an allocation
  // unit's worth, and we are the last region, it's worth allocating some
  // more memory to create a region here. The next allocation can reuse it,
  // which is better than leaving it as unused and unreusable space at the
  // end of this region.
  if (region == lastRegion && extra >= ALLOC_UNIT && extra < MIN_REGION_SIZE) {
    // Yes, this is a small-but-useful amount of memory in the final region,
    // extend it.
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("    emmalloc.possiblySplitRemainder pre-extending")});
#endif
    if (extendLastRegion(payloadSize + ALLOC_UNIT)) {
      // Success.
      extra += ALLOC_UNIT;
      assert(extra >= MIN_REGION_SIZE);
    } else {
      return;
    }
  }
  if (extra >= MIN_REGION_SIZE) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("    emmalloc.possiblySplitRemainder is splitting")});
#endif
    // Worth it, split the region
    // TODO: Consider not doing it, may affect long-term fragmentation.
    void* after = getAfter(region);
    Region* split = (Region*)alignUpPointer((char*)getPayload(region) + size);
    region->setTotalSize((char*)split - (char*)region);
    size_t totalSplitSize = (char*)after - (char*)split;
    assert(totalSplitSize >= MIN_REGION_SIZE);
    split->setTotalSize(totalSplitSize);
    split->prev() = region;
    if (region != lastRegion) {
      split->next()->prev() = split;
    } else {
      lastRegion = split;
    }
    stopUsing(split);
  }
}

// Sets the used payload of a region, and does other necessary work when
// starting to use a region, such as splitting off a remainder if there is
// any.
static void useRegion(Region* region, size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.useRegion " + [ $0, $1 ])}, region, size);
#endif
  assert(size > 0);
  region->setUsed(1);
  // We may not be using all of it, split out a smaller
  // region into a free list if it's large enough.
  possiblySplitRemainder(region, size);
}

static Region* useFreeInfo(FreeInfo* freeInfo, size_t size) {
  Region* region = fromFreeInfo(freeInfo);
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.useFreeInfo " + [ $0, $1 ])}, region, size);
#endif
  // This region is no longer free
  removeFromFreeList(region);
  // This region is now in use
  useRegion(region, size);
  return region;
}

// Debugging

// Mostly for testing purposes, wipes everything.
EMMALLOC_EXPORT
void emmalloc_blank_slate_from_orbit() {
  for (int i = 0; i < MAX_FREELIST_INDEX; i++) {
    freeLists[i] = nullptr;
  }
  firstRegion = nullptr;
  lastRegion = nullptr;
}

#ifdef EMMALLOC_DEBUG
// For testing purposes, validate a region.
static void emmalloc_validate_region(Region* region) {
  assert(getAfter(region) <= sbrk(0));
  assert(getMaxPayload(region) < region->getTotalSize());
  if (region->prev()) {
    assert(getAfter(region->prev()) == region);
    assert(region->prev()->next() == region);
  }
  if (region->next()) {
    assert(getAfter(region) == region->next());
    assert(region->next()->prev() == region);
  }
}

// For testing purposes, check that everything is valid.
static void emmalloc_validate_all() {
  void* end = sbrk(0);
  // Validate regions.
  Region* curr = firstRegion;
  Region* prev = nullptr;
  EM_ASM({ Module.emmallocDebug = {regions : {}}; });
  while (curr) {
    // Note all region, so we can see freelist items are in the main list.
    EM_ASM(
      {
        var region = $0;
        assert(!Module.emmallocDebug.regions[region], "dupe region");
        Module.emmallocDebug.regions[region] = 1;
      },
      curr);
    assert(curr->prev() == prev);
    if (prev) {
      assert(getAfter(prev) == curr);
      // Adjacent free regions must be merged.
      assert(!(!prev->getUsed() && !curr->getUsed()));
    }
    assert(getAfter(curr) <= end);
    prev = curr;
    curr = curr->next();
  }
  if (prev) {
    assert(prev == lastRegion);
  } else {
    assert(!lastRegion);
  }
  if (lastRegion) {
    assert(getAfter(lastRegion) == end);
  }
  // Validate freelists.
  for (int i = 0; i < MAX_FREELIST_INDEX; i++) {
    FreeInfo* curr = freeLists[i];
    if (!curr)
      continue;
    FreeInfo* prev = nullptr;
    while (curr) {
      assert(curr->prev() == prev);
      Region* region = fromFreeInfo(curr);
      // Regions must be in the main list.
      EM_ASM(
        {
          var region = $0;
          assert(Module.emmallocDebug.regions[region], "free region not in list");
        },
        region);
      assert(getAfter(region) <= end);
      assert(!region->getUsed());
      assert(getMaxPayload(region) >= getMinSizeForFreeListIndex(i));
      assert(getMaxPayload(region) < getMaxSizeForFreeListIndex(i));
      prev = curr;
      curr = curr->next();
    }
  }
  // Validate lastRegion.
  if (lastRegion) {
    assert(lastRegion->next() == nullptr);
    assert(getAfter(lastRegion) <= end);
    assert(firstRegion);
  } else {
    assert(!firstRegion);
  }
}

#ifdef EMMALLOC_DEBUG_LOG
// For testing purposes, dump out a region.
static void emmalloc_dump_region(Region* region) {
  EM_ASM({out("      [" + $0 + " - " + $1 + " (" + $2 + " bytes" + ($3 ? ", used" : "") + ")]")},
    region, getAfter(region), getMaxPayload(region), region->getUsed());
}

// For testing purposes, dumps out the entire global state.
static void emmalloc_dump_all() {
  EM_ASM({out("  emmalloc_dump_all:\n    sbrk(0) = " + $0)}, sbrk(0));
  Region* curr = firstRegion;
  EM_ASM({out("    all regions:")});
  while (curr) {
    emmalloc_dump_region(curr);
    curr = curr->next();
  }
  for (int i = 0; i < MAX_FREELIST_INDEX; i++) {
    FreeInfo* curr = freeLists[i];
    if (!curr)
      continue;
    EM_ASM({out("    freeList[" + $0 + "] sizes: [" + $1 + ", " + $2 + ")")}, i,
      getMinSizeForFreeListIndex(i), getMaxSizeForFreeListIndex(i));
    FreeInfo* prev = nullptr;
    while (curr) {
      Region* region = fromFreeInfo(curr);
      emmalloc_dump_region(region);
      prev = curr;
      curr = curr->next();
    }
  }
}
#endif // EMMALLOC_DEBUG_LOG
#endif // EMMALLOC_DEBUG

// When we free something of size 100, we put it in the
// freelist for items of size 64 and above. Then when something
// needs 64 bytes, we know the things in that list are all
// suitable. However, note that this means that if we then
// try to allocate something of size 100 once more, we will
// look in the freelist for items of size 128 or more (again,
// so we know all items in the list are big enough), which means
// we may not reuse the perfect region we just freed. It's hard
// to do a perfect job on that without a lot more work (memory
// and/or time), so instead, we use a simple heuristic to look
// at the one-lower freelist, which *may* contain something
// big enough for us. We look at just a few elements, but that is
// enough if we are alloating/freeing a lot of such elements
// (since the recent items are there).
// TODO: Consider more optimizations, e.g. slow bubbling of larger
//       items in each freelist towards the root, or even actually
//       keep it sorted by size.
// Consider also what happens to the very largest allocations,
// 2^32 - a little. That goes in the freelist of items of size
// 2^31 or less. >2 tries is enough to go through that entire
// freelist because even 2 can't exist, they'd exhaust memory
// (together with metadata overhead). So we should be able to
// free and allocate such largest allocations (barring fragmentation
// happening in between).
static const size_t SPECULATIVE_FREELIST_TRIES = 32;

static Region* tryFromFreeList(size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.tryFromFreeList " + $0)}, size);
#endif
  // Look in the freelist of items big enough for us.
  size_t index = getBigEnoughFreeListIndex(size);
  // If we *may* find an item in the index one
  // below us, try that briefly in constant time;
  // see comment on algorithm on the declaration of
  // SPECULATIVE_FREELIST_TRIES.
  if (index > MIN_FREELIST_INDEX && size < getMinSizeForFreeListIndex(index)) {
    FreeInfo* freeInfo = freeLists[index - 1];
    size_t tries = 0;
    while (freeInfo && tries < SPECULATIVE_FREELIST_TRIES) {
      Region* region = fromFreeInfo(freeInfo);
      if (getMaxPayload(region) >= size) {
        // Success, use it
#ifdef EMMALLOC_DEBUG_LOG
        EM_ASM({out("  emmalloc.tryFromFreeList try succeeded")});
#endif
        return useFreeInfo(freeInfo, size);
      }
      freeInfo = freeInfo->next();
      tries++;
    }
  }
  // Note that index may start out at MAX_FREELIST_INDEX,
  // if it is almost the largest allocation possible,
  // 2^32 minus a little. In that case, looking in the lower
  // freelist is our only hope, and it can contain at most 1
  // element (see discussion above), so we will find it if
  // it's there). If not, and we got here, we'll never enter
  // the loop at all.
  while (index < MAX_FREELIST_INDEX) {
    FreeInfo* freeInfo = freeLists[index];
    if (freeInfo) {
      // We found one, use it.
#ifdef EMMALLOC_DEBUG_LOG
      EM_ASM({out("  emmalloc.tryFromFreeList had item to use")});
#endif
      return useFreeInfo(freeInfo, size);
    }
    // Look in a freelist of larger elements.
    // TODO This does increase the risk of fragmentation, though,
    //      and maybe the iteration adds runtime overhead.
    index++;
  }
  // No luck, no free list.
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.tryFromFreeList no luck")});
#endif
  return nullptr;
}

// Allocate a completely new region.
static Region* allocateRegion(size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.allocateRegion")});
#endif
  size_t sbrkSize = METADATA_SIZE + alignUp(size);
  void* ptr = sbrk(sbrkSize);
  if (ptr == (void*)-1) {
    // sbrk() failed, we failed.
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("    emmalloc.allocateRegion sbrk failure")});
#endif
    return nullptr;
  }
  // sbrk() results might not be aligned. We assume single-threaded sbrk()
  // access here in order to fix that up
  void* fixedPtr = alignUpPointer(ptr);
  if (ptr != fixedPtr) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("    emmalloc.allocateRegion fixing alignment")});
#endif
    size_t extra = (char*)fixedPtr - (char*)ptr;
    void* extraPtr = sbrk(extra);
    if (extraPtr == (void*)-1) {
      // sbrk() failed, we failed.
#ifdef EMMALLOC_DEBUG_LOG
      EM_ASM({out("    emmalloc.newAllocation sbrk failure")});
      ;
#endif
      return nullptr;
    }
    // Verify the sbrk() assumption, no one else should call it.
    // If this fails, it means we also leak the previous allocation,
    // so we don't even try to handle it.
    assert((char*)extraPtr == (char*)ptr + sbrkSize);
    // After the first allocation, everything must remain aligned forever.
    assert(!lastRegion);
    // We now have a contiguous block of memory from ptr to
    // ptr + sbrkSize + fixedPtr - ptr = fixedPtr + sbrkSize.
    // fixedPtr is aligned and starts a region of the right
    // amount of memory.
  }
  Region* region = (Region*)fixedPtr;
  // Apply globally
  if (!lastRegion) {
    assert(!firstRegion);
    firstRegion = region;
    lastRegion = region;
  } else {
    assert(firstRegion);
    region->prev() = lastRegion;
    lastRegion = region;
  }
  // Success, we have new memory
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("    emmalloc.newAllocation success")});
  ;
#endif
  region->setTotalSize(sbrkSize);
  region->setUsed(1);
  return region;
}

// Allocate new memory. This may reuse part of the last region, only
// allocating what we need.
static Region* newAllocation(size_t size) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.newAllocation " + $0)}, size);
#endif
  assert(size > 0);
  if (lastRegion) {
    // If the last region is free, we can extend it rather than leave it
    // as fragmented free spce between allocated regions. This is also
    // more efficient and simple as well.
    if (!lastRegion->getUsed()) {
#ifdef EMMALLOC_DEBUG_LOG
      EM_ASM({out("    emmalloc.newAllocation extending lastRegion at " + $0)}, lastRegion);
#endif
      // Remove it first, before we adjust the size (which affects which list
      // it should be in). Also mark it as used so extending it doesn't do
      // freelist computations; we'll undo that if we fail.
      lastRegion->setUsed(1);
      removeFromFreeList(lastRegion);
      if (extendLastRegion(size)) {
        return lastRegion;
      } else {
        lastRegion->setUsed(0);
        return nullptr;
      }
    }
  }
  // Otherwise, get a new region.
  return allocateRegion(size);
}

// Internal mirror of public API.

static void* emmalloc_malloc(size_t size) {
  // for consistency with dlmalloc, for malloc(0), allocate a block of memory,
  // though returning nullptr is permitted by the standard.
  if (size == 0)
    size = 1;
  // Look in the freelist first.
  Region* region = tryFromFreeList(size);
  if (!region) {
    // Allocate some new memory otherwise.
    region = newAllocation(size);
    if (!region) {
      // We failed to allocate, sadly.
      return nullptr;
    }
  }
  assert(getAfter(region) <= sbrk(0));
  return getPayload(region);
}

static void emmalloc_free(void* ptr) {
  if (ptr == nullptr)
    return;
  stopUsing(fromPayload(ptr));
}

static void* emmalloc_calloc(size_t nmemb, size_t size) {
  // TODO If we know no one else is using sbrk(), we can assume that new
  //      memory allocations are zero'd out.
  void* ptr = emmalloc_malloc(nmemb * size);
  if (!ptr)
    return nullptr;
  memset(ptr, 0, nmemb * size);
  return ptr;
}

static void* emmalloc_realloc(void* ptr, size_t size) {
  if (!ptr)
    return emmalloc_malloc(size);
  if (!size) {
    emmalloc_free(ptr);
    return nullptr;
  }
  Region* region = fromPayload(ptr);
  assert(region->getUsed());
  // Grow it. First, maybe we can do simple growth in the current region.
  if (size <= getMaxPayload(region)) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.emmalloc_realloc use existing payload space")});
#endif
    region->setUsed(1);
    // There might be enough left over to split out now.
    possiblySplitRemainder(region, size);
    return ptr;
  }
  // Perhaps right after us is free space we can merge to us.
  Region* next = region->next();
  if (next && !next->getUsed()) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.emmalloc_realloc merge in next")});
#endif
    removeFromFreeList(next);
    region->incTotalSize(next->getTotalSize());
    if (next != lastRegion) {
      next->next()->prev() = region;
    } else {
      lastRegion = region;
    }
  }
  // We may now be big enough.
  if (size <= getMaxPayload(region)) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.emmalloc_realloc use existing payload space after merge")});
#endif
    region->setUsed(1);
    // There might be enough left over to split out now.
    possiblySplitRemainder(region, size);
    return ptr;
  }
  // We still aren't big enough. If we are the last, we can extend ourselves - however, that
  // definitely means increasing the total sbrk(), and there may be free space lower down, so
  // this is a tradeoff between speed (avoid the memcpy) and space. It's not clear what's
  // better here; for now, check for free space first.
  Region* newRegion = tryFromFreeList(size);
  if (!newRegion && region == lastRegion) {
#ifdef EMMALLOC_DEBUG_LOG
    EM_ASM({out("  emmalloc.emmalloc_realloc extend last region")});
#endif
    if (extendLastRegion(size)) {
      // It worked. We don't need the formerly free region.
      if (newRegion) {
        stopUsing(newRegion);
      }
      return ptr;
    } else {
      // If this failed, we can also try the normal
      // malloc path, which may find space in a freelist;
      // fall through.
    }
  }
  // We need new space, and a copy
  if (!newRegion) {
    newRegion = newAllocation(size);
    if (!newRegion)
      return nullptr;
  }
  memcpy(getPayload(newRegion), getPayload(region),
    size < getMaxPayload(region) ? size : getMaxPayload(region));
  stopUsing(region);
  return getPayload(newRegion);
}

static struct mallinfo emmalloc_mallinfo() {
  struct mallinfo info;
  info.arena = 0;
  info.ordblks = 0;
  info.smblks = 0;
  info.hblks = 0;
  info.hblkhd = 0;
  info.usmblks = 0;
  info.fsmblks = 0;
  info.uordblks = 0;
  info.ordblks = 0;
  info.keepcost = 0;
  if (firstRegion) {
    info.arena = (char*)sbrk(0) - (char*)firstRegion;
    Region* region = firstRegion;
    while (region) {
      if (region->getUsed()) {
        info.uordblks += getMaxPayload(region);
      } else {
        info.fordblks += getMaxPayload(region);
        info.ordblks++;
      }
      region = region->next();
    }
  }
  return info;
}

// An aligned allocation. This is a rarer allocation path, and is
// much less optimized - the assumption is that it is used for few
// large allocations.
static void* alignedAllocation(size_t size, size_t alignment) {
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("  emmalloc.alignedAllocation")});
#endif
  assert(alignment > ALIGNMENT);
  assert(alignment % ALIGNMENT == 0);
  // Try from the freelist first. We may be lucky and get something
  // properly aligned.
  // TODO: Perhaps look more carefully, checking alignment as we go,
  //       using multiple tries?
  Region* fromFreeList = tryFromFreeList(size + alignment);
  if (fromFreeList && size_t(getPayload(fromFreeList)) % alignment == 0) {
    // Luck has favored us.
    return getPayload(fromFreeList);
  } else if (fromFreeList) {
    stopUsing(fromFreeList);
  }
  // No luck from free list, so do a new allocation which we can
  // force to be aligned.
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("    emmalloc.alignedAllocation new allocation")});
#endif
  // Ensure a region before us, which we may enlarge as necessary.
  if (!lastRegion) {
    // This allocation is not freeable, but there is one at most.
    void* prev = emmalloc_malloc(MIN_REGION_SIZE);
    if (!prev)
      return nullptr;
  }
  // See if we need to enlarge the previous region in order to get
  // us properly aligned. Take into account that our region will
  // start with METADATA_SIZE of space.
  size_t address = size_t(getAfter(lastRegion)) + METADATA_SIZE;
  size_t error = address % alignment;
  if (error != 0) {
    // E.g. if we want alignment 24, and have address 16, then we
    // need to add 8.
    size_t extra = alignment - error;
    assert(extra % ALIGNMENT == 0);
    if (!extendLastRegion(getMaxPayload(lastRegion) + extra)) {
      return nullptr;
    }
    address = size_t(getAfter(lastRegion)) + METADATA_SIZE;
    error = address % alignment;
    assert(error == 0);
  }
  Region* region = allocateRegion(size);
  if (!region)
    return nullptr;
  void* ptr = getPayload(region);
  assert(size_t(ptr) == address);
  assert(size_t(ptr) % alignment == 0);
  return ptr;
}

static int isMultipleOfSizeT(size_t size) { return (size & 3) == 0; }

static int emmalloc_posix_memalign(void** memptr, size_t alignment, size_t size) {
  *memptr = nullptr;
  if (!isPowerOf2(alignment) || !isMultipleOfSizeT(alignment)) {
    return 22; // EINVAL
  }
  if (size == 0) {
    return 0;
  }
  if (alignment <= ALIGNMENT) {
    // Use normal allocation path, which will provide that alignment.
    *memptr = emmalloc_malloc(size);
  } else {
    // Use more sophisticaed alignment-specific allocation path.
    *memptr = alignedAllocation(size, alignment);
  }
  if (!*memptr) {
    return 12; // ENOMEM
  }
  return 0;
}

static void* emmalloc_memalign(size_t alignment, size_t size) {
  void* ptr;
  if (emmalloc_posix_memalign(&ptr, alignment, size) != 0) {
    return nullptr;
  }
  return ptr;
}

// Public API. This is a thin wrapper around our mirror of it, adding
// logging and validation when debugging. Otherwise it should inline
// out.

extern "C" {

EMMALLOC_EXPORT
void* malloc(size_t size) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.malloc " + $0)}, size);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  void* ptr = emmalloc_malloc(size);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.malloc ==> " + $0)}, ptr);
#endif
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
  return ptr;
}

EMMALLOC_EXPORT
void free(void* ptr) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.free " + $0)}, ptr);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  emmalloc_free(ptr);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
}

EMMALLOC_EXPORT
void* calloc(size_t nmemb, size_t size) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.calloc " + $0)}, size);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  void* ptr = emmalloc_calloc(nmemb, size);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.calloc ==> " + $0)}, ptr);
#endif
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
  return ptr;
}

EMMALLOC_EXPORT
void* realloc(void* ptr, size_t size) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.realloc " + [ $0, $1 ])}, ptr, size);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  void* newPtr = emmalloc_realloc(ptr, size);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.realloc ==> " + $0)}, newPtr);
#endif
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
  return newPtr;
}

EMMALLOC_EXPORT
int posix_memalign(void** memptr, size_t alignment, size_t size) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.posix_memalign " + [ $0, $1, $2 ])}, memptr, alignment, size);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  int result = emmalloc_posix_memalign(memptr, alignment, size);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.posix_memalign ==> " + $0)}, result);
#endif
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
  return result;
}

EMMALLOC_EXPORT
void* memalign(size_t alignment, size_t size) {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.memalign " + [ $0, $1 ])}, alignment, size);
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  void* ptr = emmalloc_memalign(alignment, size);
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.memalign ==> " + $0)}, ptr);
#endif
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
  emmalloc_validate_all();
#endif
  return ptr;
}

EMMALLOC_EXPORT
struct mallinfo mallinfo() {
#ifdef EMMALLOC_DEBUG
#ifdef EMMALLOC_DEBUG_LOG
  EM_ASM({out("emmalloc.mallinfo")});
#endif
  emmalloc_validate_all();
#ifdef EMMALLOC_DEBUG_LOG
  emmalloc_dump_all();
#endif
#endif
  return emmalloc_mallinfo();
}

// Export malloc and free as duplicate names emscripten_builtin_malloc and
// emscripten_builtin_free so that applications can replace malloc and free
// in their code, and make those replacements refer to the original malloc
// and free from this file.
// This allows an easy mechanism for hooking into memory allocation.
#if defined(__EMSCRIPTEN__)
extern __typeof(malloc) emscripten_builtin_malloc __attribute__((alias("malloc")));
extern __typeof(free) emscripten_builtin_free __attribute__((alias("free")));
extern __typeof(memalign) emscripten_builtin_memalign __attribute__((alias("memalign")));
#endif

} // extern "C"
#else


#ifdef __EMSCRIPTEN__

#include <stdint.h>
#include <unistd.h>
#include <memory.h>
#include <assert.h>
#include <malloc.h>
#include <emscripten.h>
#include <emscripten/heap.h>

#ifdef __EMSCRIPTEN_TRACING__
#include <emscripten/trace.h>
#endif

// Behavior of right shifting a signed integer is compiler implementation defined.
static_assert((((int32_t)0x80000000U) >> 31) == -1, "This malloc implementation requires that right-shifting a signed integer produces a sign-extending (arithmetic) shift!");

extern "C"
{

#define MALLOC_ALIGNMENT 8

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifdef EMMALLOC_USE_64BIT_OPS
#define NUM_FREE_BUCKETS 64
#define BUCKET_BITMASK_T uint64_t
#else
#define NUM_FREE_BUCKETS 32
#define BUCKET_BITMASK_T uint32_t
#endif

// Dynamic memory is subdivided into regions, in the format

// <size:uint32_t> ..... <size:uint32_t> | <size:uint32_t> ..... <size:uint32_t> | <size:uint32_t> ..... <size:uint32_t> | .....

// That is, at the bottom and top end of each memory region, the size of that region is stored. That allows traversing the
// memory regions backwards and forwards.

// A free region has the following structure:
// <size:uint32_t> <prevptr> <nextptr> ... <size:uint32_t>

struct Region
{
  uint32_t size;
  // Use a circular doubly linked list to represent free region data.
  Region *prev, *next;
  // ... N bytes of free data
  uint32_t _at_the_end_of_this_struct_size; // do not dereference, this is present for convenient struct sizeof() computation only
};

#if defined(__EMSCRIPTEN_PTHREADS__)
// In multithreaded builds, use a simple global spinlock strategy to acquire/release access to the memory allocator.
static volatile uint8_t multithreadingLock = 0;
#define MALLOC_ACQUIRE() while(__sync_lock_test_and_set(&multithreadingLock, 1)) { while(multithreadingLock) { /*nop*/ } }
#define MALLOC_RELEASE() __sync_lock_release(&multithreadingLock)
// Test code to ensure we have tight malloc acquire/release guards in place.
#define ASSERT_MALLOC_IS_ACQUIRED() assert(multithreadingLock == 1)
#else
// In singlethreaded builds, no need for locking.
#define MALLOC_ACQUIRE() ((void)0)
#define MALLOC_RELEASE() ((void)0)
#define ASSERT_MALLOC_IS_ACQUIRED() ((void)0)
#endif

// A region that contains as payload a single forward linked list of pointers to head regions of each disjoint region blocks.
static Region *listOfAllRegions = 0;

// For each of the buckets, maintain a linked list head node. The head node for each
// free region is a sentinel node that does not actually represent any free space, but
// the sentinel is used to avoid awkward testing against (if node == freeRegionHeadNode)
// when adding and removing elements from the linked list, i.e. we are guaranteed that
// the sentinel node is always fixed and there, and the actual free region list elements
// start at freeRegionBuckets[i].next each.
static Region freeRegionBuckets[NUM_FREE_BUCKETS];

// A bitmask that tracks the population status for each of the 32 distinct memory regions:
// a zero at bit position i means that the free list bucket i is empty. This bitmask is
// used to avoid redundant scanning of the 32 different free region buckets: instead by
// looking at the bitmask we can find in constant time an index to a free region bucket
// that contains free memory of desired size.
static BUCKET_BITMASK_T freeRegionBucketsUsed = 0;

// Amount of bytes taken up by allocation header data
#define REGION_HEADER_SIZE (2*sizeof(uint32_t))

// Smallest allocation size that is possible is 2*pointer size, since payload of each region must at least contain space
// to store the free region linked list prev and next pointers.
#define SMALLEST_ALLOCATION_SIZE (2*sizeof(void*))

// Allocations larger than this limit are considered to be large allocations, meaning that any free memory regions that
// are at least this large will be grouped to the same "large alloc" memory regions.
#define LARGE_ALLOCATION_SIZE (16*1024*1024)

/* Subdivide regions of free space into distinct circular doubly linked lists, where each linked list
represents a range of free space blocks. The following function compute_free_list_bucket() converts
an allocation size to the bucket index that should be looked at.

When using 32 buckets, this function produces a subdivision/grouping as follows:
  Bucket 0: [8-15], range size=8
  Bucket 1: [16-23], range size=8
  Bucket 2: [24-31], range size=8
  Bucket 3: [32-39], range size=8
  Bucket 4: [40-47], range size=8
  Bucket 5: [48-63], range size=16
  Bucket 6: [64-127], range size=64
  Bucket 7: [128-255], range size=128
  Bucket 8: [256-511], range size=256
  Bucket 9: [512-1023], range size=512
  Bucket 10: [1024-2047], range size=1024
  Bucket 11: [2048-3071], range size=1024
  Bucket 12: [3072-4095], range size=1024
  Bucket 13: [4096-6143], range size=2048
  Bucket 14: [6144-8191], range size=2048
  Bucket 15: [8192-12287], range size=4096
  Bucket 16: [12288-16383], range size=4096
  Bucket 17: [16384-24575], range size=8192
  Bucket 18: [24576-32767], range size=8192
  Bucket 19: [32768-49151], range size=16384
  Bucket 20: [49152-65535], range size=16384
  Bucket 21: [65536-98303], range size=32768
  Bucket 22: [98304-131071], range size=32768
  Bucket 23: [131072-196607], range size=65536
  Bucket 24: [196608-262143], range size=65536
  Bucket 25: [262144-393215], range size=131072
  Bucket 26: [393216-524287], range size=131072
  Bucket 27: [524288-786431], range size=262144
  Bucket 28: [786432-1048575], range size=262144
  Bucket 29: [1048576-1572863], range size=524288
  Bucket 30: [1572864-2097151], range size=524288
  Bucket 31: 2097152 bytes and larger.

When using 64 buckets, this function produces a grouping as follows:
  Bucket 0: [8, 15], range size=8
  Bucket 1: [16, 23], range size=8
  Bucket 2: [24, 31], range size=8
  Bucket 3: [32, 39], range size=8
  Bucket 4: [40, 47], range size=8
  Bucket 5: [48, 55], range size=8
  Bucket 6: [56, 63], range size=8
  Bucket 7: [64, 71], range size=8
  Bucket 8: [72, 79], range size=8
  Bucket 9: [80, 87], range size=8
  Bucket 10: [88, 95], range size=8
  Bucket 11: [96, 103], range size=8
  Bucket 12: [104, 111], range size=8
  Bucket 13: [112, 119], range size=8
  Bucket 14: [120, 159], range size=40
  Bucket 15: [160, 191], range size=32
  Bucket 16: [192, 223], range size=32
  Bucket 17: [224, 255], range size=32
  Bucket 18: [256, 319], range size=64
  Bucket 19: [320, 383], range size=64
  Bucket 20: [384, 447], range size=64
  Bucket 21: [448, 511], range size=64
  Bucket 22: [512, 639], range size=128
  Bucket 23: [640, 767], range size=128
  Bucket 24: [768, 895], range size=128
  Bucket 25: [896, 1023], range size=128
  Bucket 26: [1024, 1279], range size=256
  Bucket 27: [1280, 1535], range size=256
  Bucket 28: [1536, 1791], range size=256
  Bucket 29: [1792, 2047], range size=256
  Bucket 30: [2048, 2559], range size=512
  Bucket 31: [2560, 3071], range size=512
  Bucket 32: [3072, 3583], range size=512
  Bucket 33: [3584, 6143], range size=2560
  Bucket 34: [6144, 8191], range size=2048
  Bucket 35: [8192, 12287], range size=4096
  Bucket 36: [12288, 16383], range size=4096
  Bucket 37: [16384, 24575], range size=8192
  Bucket 38: [24576, 32767], range size=8192
  Bucket 39: [32768, 49151], range size=16384
  Bucket 40: [49152, 65535], range size=16384
  Bucket 41: [65536, 98303], range size=32768
  Bucket 42: [98304, 131071], range size=32768
  Bucket 43: [131072, 196607], range size=65536
  Bucket 44: [196608, 262143], range size=65536
  Bucket 45: [262144, 393215], range size=131072
  Bucket 46: [393216, 524287], range size=131072
  Bucket 47: [524288, 786431], range size=262144
  Bucket 48: [786432, 1048575], range size=262144
  Bucket 49: [1048576, 1572863], range size=524288
  Bucket 50: [1572864, 2097151], range size=524288
  Bucket 51: [2097152, 3145727], range size=1048576
  Bucket 52: [3145728, 4194303], range size=1048576
  Bucket 53: [4194304, 6291455], range size=2097152
  Bucket 54: [6291456, 8388607], range size=2097152
  Bucket 55: [8388608, 12582911], range size=4194304
  Bucket 56: [12582912, 16777215], range size=4194304
  Bucket 57: [16777216, 25165823], range size=8388608
  Bucket 58: [25165824, 33554431], range size=8388608
  Bucket 59: [33554432, 50331647], range size=16777216
  Bucket 60: [50331648, 67108863], range size=16777216
  Bucket 61: [67108864, 100663295], range size=33554432
  Bucket 62: [100663296, 134217727], range size=33554432
  Bucket 63: 134217728 bytes and larger. */
static int compute_free_list_bucket(uint32_t allocSize)
{
#if NUM_FREE_BUCKETS == 32
  if (allocSize < 48) return (allocSize >> 3) - 1;
  allocSize = MIN(allocSize >> 4, 131072);
  int clz = __builtin_clz(allocSize);
  int bucketIndex = (clz > 24) ? 35 - clz : 59 - (clz<<1) + ((allocSize >> (30-clz)) ^ 2);
#elif NUM_FREE_BUCKETS == 64
  if (allocSize < 128) return (allocSize >> 3) - 1;
  allocSize = MIN(allocSize >> 5, 4194304);
  int clz = __builtin_clz(allocSize);
  int bucketIndex = (clz > 24) ? 130 - (clz<<2) + ((allocSize >> (29-clz)) ^ 4) : 81 - (clz<<1) + ((allocSize >> (30-clz)) ^ 2);
#else
#error Invalid size chosen for NUM_FREE_BUCKETS
#endif
  assert(bucketIndex >= 0);
  assert(bucketIndex < NUM_FREE_BUCKETS);
  return bucketIndex;
}

#define DECODE_CEILING_SIZE(size) (uint32_t)(((size) ^ (((int32_t)(size)) >> 31)))

static Region *prev_region(Region *region)
{
  uint32_t prevRegionSize = ((uint32_t*)region)[-1];
  prevRegionSize = DECODE_CEILING_SIZE(prevRegionSize);
  return (Region*)((uint8_t*)region - prevRegionSize);
}

static Region *next_region(Region *region)
{
  return (Region*)((uint8_t*)region + region->size);
}

static uint32_t region_ceiling_size(Region *region)
{
  return ((uint32_t*)((uint8_t*)region + region->size))[-1];
}

static bool region_is_free(Region *r)
{
  return region_ceiling_size(r) >> 31;
}

static bool region_is_in_use(Region *r)
{
  return r->size == region_ceiling_size(r);
}

static uint32_t size_of_region_from_ceiling(Region *r)
{
  uint32_t size = region_ceiling_size(r);
  return DECODE_CEILING_SIZE(size);
}

static bool debug_region_is_consistent(Region *r)
{
  assert(r);
  uint32_t sizeAtBottom = r->size;
  uint32_t sizeAtCeiling = size_of_region_from_ceiling(r);
  return sizeAtBottom == sizeAtCeiling;
}

static uint8_t *region_payload_start_ptr(Region *region)
{
  return (uint8_t*)region + sizeof(uint32_t);
}

static uint8_t *region_payload_end_ptr(Region *region)
{
  return (uint8_t*)region + region->size - sizeof(uint32_t);
}

#define IS_POWER_OF_2(val) (((val) & ((val)-1)) == 0)
#define ALIGN_UP(ptr, alignment) ((uint8_t*)((((uintptr_t)(ptr)) + ((alignment)-1)) & ~((alignment)-1)))
#define HAS_ALIGNMENT(ptr, alignment) ((((uintptr_t)(ptr)) & ((alignment)-1)) == 0)

static void create_used_region(void *ptr, uint32_t size)
{
  assert(ptr);
  assert(HAS_ALIGNMENT(ptr, sizeof(uint32_t)));
  assert(HAS_ALIGNMENT(size, sizeof(uint32_t)));
  assert(size >= sizeof(Region));
  *(uint32_t*)ptr = size;
  ((uint32_t*)ptr)[(size>>2)-1] = size;
}

static void create_free_region(void *ptr, uint32_t size)
{
  assert(ptr);
  assert(HAS_ALIGNMENT(ptr, sizeof(uint32_t)));
  assert(HAS_ALIGNMENT(size, sizeof(uint32_t)));
  assert(size >= sizeof(Region));
  Region *freeRegion = (Region*)ptr;
  freeRegion->size = size;
  ((uint32_t*)ptr)[(size>>2)-1] = ~size;
}

static void resize_region(void *ptr, uint32_t size)
{
  assert(ptr);
  assert(HAS_ALIGNMENT(ptr, sizeof(uint32_t)));
  assert(HAS_ALIGNMENT(size, sizeof(uint32_t)));
  assert(size >= sizeof(Region));
  *(uint32_t*)ptr = size;
  uint32_t *sizeAtEnd = (uint32_t*)ptr + (size>>2) - 1;
  uint32_t usedMask = (*(int32_t*)sizeAtEnd) >> 31;
  *sizeAtEnd = size ^ usedMask;
}

static void prepend_to_free_list(Region *region, Region *prependTo)
{
  assert(region);
  assert(prependTo);
  // N.b. the region we are prepending to is always the sentinel node,
  // which represents a dummy node that is technically not a free node, so
  // region_is_free(prependTo) does not hold.
  assert(region_is_free((Region*)region));
  region->next = prependTo;
  region->prev = prependTo->prev;
  assert(region->prev);
  prependTo->prev = region;
  region->prev->next = region;
}

static void unlink_from_free_list(Region *region)
{
  assert(region);
  assert(region_is_free((Region*)region));
  assert(region->prev);
  assert(region->next);
  region->prev->next = region->next;
  region->next->prev = region->prev;
}

static void link_to_free_list(Region *freeRegion)
{
  assert(freeRegion);
  assert(freeRegion->size >= sizeof(Region));
  int bucketIndex = compute_free_list_bucket(freeRegion->size-REGION_HEADER_SIZE);
  Region *freeListHead = freeRegionBuckets + bucketIndex;
  freeRegion->prev = freeListHead;
  freeRegion->next = freeListHead->next;
  assert(freeRegion->next);
  freeListHead->next = freeRegion;
  freeRegion->next->prev = freeRegion;
  freeRegionBucketsUsed |= ((BUCKET_BITMASK_T)1) << bucketIndex;
}

static void dump_memory_regions()
{
  ASSERT_MALLOC_IS_ACQUIRED();
  Region *root = listOfAllRegions;
  MAIN_THREAD_ASYNC_EM_ASM(console.log('All memory regions:'));
  while(root)
  {
    Region *r = root;
    assert(debug_region_is_consistent(r));
    uint8_t *lastRegionEnd = (uint8_t*)(((uint32_t*)root)[2]);
    MAIN_THREAD_ASYNC_EM_ASM(console.log('Region block '+$0.toString(16)+'-'+$1.toString(16)+ ' ('+$2+' bytes):'),
      r, lastRegionEnd, lastRegionEnd-(uint8_t*)r);
    while((uint8_t*)r < lastRegionEnd)
    {
      MAIN_THREAD_ASYNC_EM_ASM(console.log('Region '+$0.toString(16)+', size: '+$1+' ('+($2?"used":"--FREE--")+')'),
        r, r->size, region_ceiling_size(r) == r->size);

      assert(debug_region_is_consistent(r));
      uint32_t sizeFromCeiling = size_of_region_from_ceiling(r);
      if (sizeFromCeiling != r->size)
        MAIN_THREAD_ASYNC_EM_ASM(console.log('SIZE FROM END: '+$0), sizeFromCeiling);
      if (r->size == 0)
        break;
      r = next_region(r);
    }
    root = ((Region*)((uint32_t*)root)[1]);
    MAIN_THREAD_ASYNC_EM_ASM(console.log(""));
  }
  MAIN_THREAD_ASYNC_EM_ASM(console.log('Free regions:'));
  for(int i = 0; i < NUM_FREE_BUCKETS; ++i)
  {
    Region *prev = &freeRegionBuckets[i];
    Region *fr = freeRegionBuckets[i].next;
    while(fr != &freeRegionBuckets[i])
    {
      MAIN_THREAD_ASYNC_EM_ASM(console.log('In bucket '+$0+', free region '+$1.toString(16)+', size: ' + $2 + ' (size at ceiling: '+$3+'), prev: ' + $4.toString(16) + ', next: ' + $5.toString(16)),
        i, fr, fr->size, size_of_region_from_ceiling((Region*)fr), fr->prev, fr->next);
      assert(debug_region_is_consistent(fr));
      assert(region_is_free((Region*)fr));
      assert(fr->prev == prev);
      prev = fr;
      assert(fr->next != fr);
      assert(fr->prev != fr);
      fr = fr->next;
    }
  }
#if NUM_FREE_BUCKETS == 64
  MAIN_THREAD_ASYNC_EM_ASM(console.log('Free bucket index map: ' + ($0>>>0).toString(2) + ' ' + ($1>>>0).toString(2)), (uint32_t)(freeRegionBucketsUsed >> 32), (uint32_t)freeRegionBucketsUsed);
#else
  MAIN_THREAD_ASYNC_EM_ASM(console.log('Free bucket index map: ' + ($0>>>0).toString(2)), freeRegionBucketsUsed);
#endif
  MAIN_THREAD_ASYNC_EM_ASM(console.log(""));
}

void emmalloc_dump_memory_regions()
{
  MALLOC_ACQUIRE();
  dump_memory_regions();
  MALLOC_RELEASE();
}

static bool claim_more_memory(size_t numBytes)
{
  // Claim memory via sbrk
  uint8_t *startPtr = (uint8_t*)sbrk(numBytes);
//  MAIN_THREAD_ASYNC_EM_ASM(console.log('Claimed ' + $0 + '-' + $1 + ' via sbrk()'), startPtr, startPtr + numBytes);
  if ((intptr_t)startPtr <= 0) return false;
  assert(HAS_ALIGNMENT(startPtr, 4));
  uint8_t *endPtr = startPtr + numBytes;

  // Create a sentinel region at the end of the new heap block
  Region *endSentinelRegion = (Region*)(endPtr - sizeof(Region));
  create_used_region(endSentinelRegion, sizeof(Region));

  // If we are the sole user of sbrk(), it will feed us continuous/consecutive memory addresses - take advantage
  // of that if so: instead of creating two disjoint memory regions blocks, expand the previous one to a larger size.
  uint8_t *previousSbrkEndAddress = listOfAllRegions ? (uint8_t*)((uint32_t*)listOfAllRegions)[2] : 0;
  if (startPtr == previousSbrkEndAddress)
  {
    Region *prevEndSentinel = prev_region((Region*)startPtr);
    assert(debug_region_is_consistent(prevEndSentinel));
    assert(region_is_in_use(prevEndSentinel));
    Region *prevRegion = prev_region(prevEndSentinel);
    assert(debug_region_is_consistent(prevRegion));

    ((uint32_t*)listOfAllRegions)[2] = (uint32_t)endPtr;

    // Two scenarios, either the last region of the previous block was in use, in which case we need to create
    // a new free region in the newly allocated space; or it was free, in which case we can extend that region
    // to cover a larger size.
    if (region_is_free(prevRegion))
    {
      size_t newFreeRegionSize = (uint8_t*)endSentinelRegion - (uint8_t*)prevRegion;
      unlink_from_free_list(prevRegion);
      create_free_region(prevRegion, newFreeRegionSize);
      link_to_free_list(prevRegion);
      return true;
    }
    // else: last region of the previous block was in use. Since we are joining two consecutive sbrk() blocks,
    // we can swallow the end sentinel of the previous block away.
    startPtr -= sizeof(Region);
  }
  else
  {
    // Create a sentinel region at the start of the heap block
    create_used_region(startPtr, sizeof(Region));

    // Dynamic heap start region:
    Region *newRegionBlock = (Region*)startPtr;
    ((uint32_t*)newRegionBlock)[1] = (uint32_t)listOfAllRegions; // Pointer to next region block head
    ((uint32_t*)newRegionBlock)[2] = (uint32_t)endPtr; // Pointer to the end address of this region block
    listOfAllRegions = newRegionBlock;
    startPtr += sizeof(Region);
  }

  // Create a new memory region for the new claimed free space.
  create_free_region(startPtr, (uint8_t*)endSentinelRegion - startPtr);
  link_to_free_list((Region*)startPtr);
  return true;
}

// Initialize malloc during static initialization with highest constructor priority,
// so that it initializes before any other static initializers in compilation units.
static void EMSCRIPTEN_KEEPALIVE __attribute__((constructor(0))) initialize_malloc_heap()
{
#if __EMSCRIPTEN_PTHREADS__
  // This function should be called on the main thread before any pthreads have been
  // established to initialize the malloc subsystem. (so no lock acquire needed)
  assert(emscripten_is_main_runtime_thread());
#endif

  // Initialize circular doubly linked lists representing free space
  for(int i = 0; i < NUM_FREE_BUCKETS; ++i)
    freeRegionBuckets[i].prev = freeRegionBuckets[i].next = &freeRegionBuckets[i];

  // Start with a tiny dynamic region.
  claim_more_memory(3*sizeof(Region));
}

void emmalloc_blank_slate_from_orbit()
{
  listOfAllRegions = 0;
  freeRegionBucketsUsed = 0;
  initialize_malloc_heap();
}

static void *attempt_allocate(Region *freeRegion, size_t alignment, size_t size)
{
  ASSERT_MALLOC_IS_ACQUIRED();
  assert(freeRegion);
  // Look at the next potential free region to allocate into.
  // First, we should check if the free region has enough of payload bytes contained
  // in it to accommodate the new allocation. This check needs to take account the
  // requested allocation alignment, so the payload memory area needs to be rounded
  // upwards to the desired alignment.
  uint8_t *payloadStartPtr = region_payload_start_ptr(freeRegion);
  uint8_t *payloadStartPtrAligned = ALIGN_UP(payloadStartPtr, alignment);
  uint8_t *payloadEndPtr = region_payload_end_ptr(freeRegion);

  // Do we have enough free space, taking into account alignment?
  if (payloadStartPtrAligned + size > payloadEndPtr)
    return 0;

  // We have enough free space, so the memory allocation will be made into this region. Remove this free region
  // from the list of free regions: whatever slop remains will be later added back to the free region pool.
  unlink_from_free_list(freeRegion);

  // Before we proceed further, fix up the boundary of this region and the region that precedes this one,
  // so that the boundary between the two regions happens at a right spot for the payload to be aligned.
  if (payloadStartPtr != payloadStartPtrAligned)
  {
    Region *prevRegion = prev_region((Region*)freeRegion);
    size_t regionBoundaryBumpAmount = payloadStartPtrAligned - payloadStartPtr;
    size_t newThisRegionSize = freeRegion->size - regionBoundaryBumpAmount;
    resize_region(prevRegion, prevRegion->size + regionBoundaryBumpAmount);
    freeRegion = (Region *)((uint8_t*)freeRegion + regionBoundaryBumpAmount);
    freeRegion->size = newThisRegionSize;
  }
  // Next, we need to decide whether this region is so large that it should be split into two regions,
  // one representing the newly used memory area, and at the high end a remaining leftover free area.
  // This splitting to two is done always if there is enough space for the high end to fit a region.
  // Carve 'size' bytes of payload off this region. So, 
  // [sz prev next sz]
  // becomes 
  // [sz payload sz] [sz prev next sz]
  if (sizeof(Region) + REGION_HEADER_SIZE + size <= freeRegion->size)
  {
    // There is enough space to keep a free region at the end of the carved out block
    // -> construct the new block
    Region *newFreeRegion = (Region *)((uint8_t*)freeRegion + REGION_HEADER_SIZE + size);
    create_free_region(newFreeRegion, freeRegion->size - size - REGION_HEADER_SIZE);
    link_to_free_list(newFreeRegion);

    // Recreate the resized Region under its new size.
    create_used_region(freeRegion, size + REGION_HEADER_SIZE);
  }
  else
  {
    // There is not enough space to split the free memory region into used+free parts, so consume the whole
    // region as used memory, not leaving a free memory region behind.
    // Initialize the free region as used by resetting the ceiling size to the same value as the size at bottom.
    ((uint32_t*)((uint8_t*)freeRegion + freeRegion->size))[-1] = freeRegion->size;
  }

//  dump_memory_regions();
//  EM_ASM(console.error('POST malloc align='+$0+', size='+$1+', returning ptr='+$2.toString(16)),
//    alignment, size, (uint8_t*)freeRegion + sizeof(uint32_t));

#ifdef __EMSCRIPTEN_TRACING__
  emscripten_trace_record_allocation(freeRegion, freeRegion->size);
#endif

  return (uint8_t*)freeRegion + sizeof(uint32_t);
}

static size_t validate_alloc_alignment(size_t alignment)
{
  // Cannot perform allocations that are less than 4 byte aligned, because the Region
  // control structures need to be aligned.
  alignment = MAX(alignment, sizeof(uint32_t));
  // Alignments must be power of 2 in general, no silly 24 byte alignments or otherwise.
  // We could silently round up the alignment to next pow-2, but better to treat such alignments
  // as a programming error.
  assert(IS_POWER_OF_2(alignment));
  // Arbitrary upper limit on alignment - very likely a programming bug if alignment is higher than this.
  assert(alignment <= 1024*1024);
  return alignment;
}

static size_t validate_alloc_size(size_t size)
{
  // Allocation sizes must be a multiple of pointer sizes, and at least 2*sizeof(pointer).
  return size > 2*sizeof(Region*) ? (size_t)ALIGN_UP(size, sizeof(Region*)) : 2*sizeof(Region*);
}

static void *allocate_memory(size_t alignment, size_t size)
{
  ASSERT_MALLOC_IS_ACQUIRED();
//  dump_memory_regions();
//  EM_ASM(console.error('allocate_memory(align='+$0+', size=' + $1 + ')'), alignment, size);

  if (!IS_POWER_OF_2(alignment))
    return 0;

  alignment = validate_alloc_alignment(alignment);
  size = validate_alloc_size(size);

  // Attempt to allocate memory starting from smallest bucket that can contain the required amount of memory.
  // Under normal alignment conditions this should always be the first or second bucket we look at, but if
  // performing an allocation with complex alignment, we may need to look at multiple buckets.
  int bucketIndex = compute_free_list_bucket(size);
  BUCKET_BITMASK_T bucketMask = freeRegionBucketsUsed >> bucketIndex;

  // Loop through each bucket that has free regions in it, based on bits set in freeRegionBucketsUsed bitmap.
  while(bucketMask)
  {
#ifdef EMMALLOC_USE_64BIT_OPS
    BUCKET_BITMASK_T indexAdd = __builtin_ctzll(bucketMask);
#else
    BUCKET_BITMASK_T indexAdd = __builtin_ctz(bucketMask);
#endif
    bucketIndex += indexAdd;
    bucketMask >>= indexAdd;
    assert(bucketIndex >= 0);
    assert(bucketIndex <= NUM_FREE_BUCKETS-1);
    assert(freeRegionBucketsUsed & (((BUCKET_BITMASK_T)1) << bucketIndex));

    Region *freeRegion = freeRegionBuckets[bucketIndex].next;
    assert(freeRegion);
    if (freeRegion != &freeRegionBuckets[bucketIndex])
    {
      void *ptr = attempt_allocate(freeRegion, alignment, size);
      if (ptr)
        return ptr;

      // We were not able to allocate from the first region found in this bucket, so penalize
      // the region by cycling it to the end of the doubly circular linked list. (constant time)
      // This provides a randomized guarantee that when performing allocations of size k to a
      // bucket of [k-something, k+something] range, we will not always attempt to satisfy the
      // allocation from the same available region at the front of the list, but we try each
      // region in turn.
      unlink_from_free_list(freeRegion);
      prepend_to_free_list(freeRegion, &freeRegionBuckets[bucketIndex]);
      // But do not stick around to attempt to look at other regions in this bucket - move
      // to search the next populated bucket index if this did not fit. This gives a practical
      // "allocation in constant time" guarantee, since the next higher bucket will only have
      // regions that are all of strictly larger size than the requested allocation. Only if
      // there is a difficult alignment requirement we may fail to perform the allocation from
      // a region in the next bucket, and if so, we keep trying higher buckets until one of them
      // works.
      ++bucketIndex;
      bucketMask >>= 1;
    }
    else
    {
      // This bucket was not populated after all with any regions,
      // but we just had a stale bit set to mark a populated bucket.
      // Reset the bit to update latest status so that we do not
      // redundantly look at this bucket again.
      freeRegionBucketsUsed &= ~(((BUCKET_BITMASK_T)1) << bucketIndex);
      bucketMask ^= 1;
    }
    // Instead of recomputing bucketMask from scratch at the end of each loop, it is updated as we go,
    // to avoid undefined behavior with (x >> 32)/(x >> 64) when bucketIndex reaches 32/64, (the shift would comes out as a no-op instead of 0).
    assert((bucketIndex == NUM_FREE_BUCKETS && bucketMask == 0) || (bucketMask == freeRegionBucketsUsed >> bucketIndex));
  }

  // None of the buckets were able to accommodate an allocation. If this happens we are almost out of memory.
  // The largest bucket might contain some suitable regions, but we only looked at one region in that bucket, so
  // as a last resort, loop through more free regions in the bucket that represents the largest allocations available.
  // But only if the bucket representing largest allocations available is not any of the first ten buckets (thirty buckets
  // in 64-bit buckets build), these represent allocatable areas less than <1024 bytes - which could be a lot of scrap.
  // In such case, prefer to sbrk() in more memory right away.
#ifdef EMMALLOC_USE_64BIT_OPS
  if (freeRegionBucketsUsed >> 10)
#else
  if (freeRegionBucketsUsed >> 30)
#endif
  {
#ifdef EMMALLOC_USE_64BIT_OPS
    int largestBucketIndex = NUM_FREE_BUCKETS - 1 - __builtin_clzll(freeRegionBucketsUsed);
#else
    int largestBucketIndex = NUM_FREE_BUCKETS - 1 - __builtin_clz(freeRegionBucketsUsed);
#endif
    // Look only at a constant number of regions in this bucket max, to avoid any bad worst case behavior.
    const int maxRegionsToTryBeforeGivingUp = 100;
    int numTriesLeft = maxRegionsToTryBeforeGivingUp;
    for(Region *freeRegion = freeRegionBuckets[largestBucketIndex].next;
      freeRegion != &freeRegionBuckets[largestBucketIndex] && numTriesLeft-- > 0;
      freeRegion = freeRegion->next)
    {
      void *ptr = attempt_allocate(freeRegion, alignment, size);
      if (ptr)
        return ptr;
    }
  }

  // We were unable to find a free memory region. Must sbrk() in more memory!
  size_t numBytesToClaim = size+sizeof(Region)*3;
  bool success = claim_more_memory(numBytesToClaim);

//  dump_memory_regions();
//  EM_ASM(console.error('claimed more memory when allocating align='+$0+', size='+$1),
//    alignment, size);

  if (success)
    return allocate_memory(alignment, size); // Recurse back to itself to try again
  return 0;
}

void *emmalloc_memalign(size_t alignment, size_t size)
{
  MALLOC_ACQUIRE();
  void *ptr = allocate_memory(alignment, size);
  MALLOC_RELEASE();
  return ptr;
}
extern __typeof(emmalloc_memalign) emscripten_builtin_memalign __attribute__((alias("emmalloc_memalign")));
extern __typeof(emmalloc_memalign) memalign __attribute__((weak, alias("emmalloc_memalign")));

void *emmalloc_malloc(size_t size)
{
  return emmalloc_memalign(MALLOC_ALIGNMENT, size);
}
extern __typeof(emmalloc_malloc) emscripten_builtin_malloc __attribute__((alias("emmalloc_malloc")));
extern __typeof(emmalloc_malloc) malloc __attribute__((weak, alias("emmalloc_malloc")));

void emmalloc_free(void *ptr)
{
  if (!ptr)
    return;

//  dump_memory_regions();
//  EM_ASM(console.error('free(ptr='+$0+')'), ptr);

  uint8_t *regionStartPtr = (uint8_t*)ptr - sizeof(uint32_t);
  Region *region = (Region*)(regionStartPtr);
  assert(HAS_ALIGNMENT(region, sizeof(uint32_t)));

  MALLOC_ACQUIRE();

  uint32_t size = region->size;
  assert(size >= sizeof(Region));
  assert(region_is_in_use(region));

#ifdef __EMSCRIPTEN_TRACING__
  emscripten_trace_record_free(region);
#endif

  // Check merging with left side
  uint32_t prevRegionSize = ((uint32_t*)region)[-1];
  uint32_t prevRegionSizeMask = (uint32_t)((int32_t)prevRegionSize >> 31);
  if (prevRegionSizeMask)
  {
    prevRegionSize ^= prevRegionSizeMask;
    Region *prevRegion = (Region*)((uint8_t*)region - prevRegionSize);
    assert(debug_region_is_consistent(prevRegion));
    unlink_from_free_list(prevRegion);
    regionStartPtr = (uint8_t*)prevRegion;
    size += prevRegionSize;
  }

  // Check merging with right side
  Region *nextRegion = next_region(region);
  assert(debug_region_is_consistent(nextRegion));
  uint32_t sizeAtEnd = *(uint32_t*)region_payload_end_ptr(nextRegion);
  if (nextRegion->size != sizeAtEnd)
  {
    unlink_from_free_list(nextRegion);
    size += nextRegion->size;
  }

  create_free_region(regionStartPtr, size);
  link_to_free_list((Region*)regionStartPtr);

//  dump_memory_regions();
//  EM_ASM(console.error('Post free!'));

  MALLOC_RELEASE();
}
extern __typeof(emmalloc_free) emscripten_builtin_free __attribute__((alias("emmalloc_free")));
extern __typeof(emmalloc_free) free __attribute__((weak, alias("emmalloc_free")));

// Can be called to attempt to increase or decrease the size of the given region
// to a new size (in-place). Returns 1 if resize succeeds, and 0 on failure.
static int attempt_region_resize(Region *region, size_t size)
{
  ASSERT_MALLOC_IS_ACQUIRED();
  assert(size > 0);
  assert(HAS_ALIGNMENT(size, sizeof(uint32_t)));

  // First attempt to resize this region, if the next region that follows this one
  // is a free region.
  Region *nextRegion = next_region(region);
  uint8_t *nextRegionEndPtr = (uint8_t*)nextRegion + nextRegion->size;
  size_t sizeAtCeiling = ((uint32_t*)nextRegionEndPtr)[-1];
  if (nextRegion->size != sizeAtCeiling) // Next region is free?
  {
    assert(region_is_free(nextRegion));
    uint8_t *newNextRegionStartPtr = (uint8_t*)region + size;
    assert(HAS_ALIGNMENT(newNextRegionStartPtr, sizeof(uint32_t)));
    // Next region does not shrink to too small size?
    if (newNextRegionStartPtr + sizeof(Region) <= nextRegionEndPtr)
    {
      unlink_from_free_list(nextRegion);
      create_free_region(newNextRegionStartPtr, nextRegionEndPtr - newNextRegionStartPtr);
      link_to_free_list((Region*)newNextRegionStartPtr);
      create_used_region(region, newNextRegionStartPtr - (uint8_t*)region);
      return 1;
    }
    // If we remove the next region altogether, allocation is satisfied?
    if (newNextRegionStartPtr <= nextRegionEndPtr)
    {
      unlink_from_free_list(nextRegion);
      create_used_region(region, region->size + nextRegion->size);
      return 1;
    }
  }
  else
  {
    // Next region is an used region - we cannot change its starting address. However if we are shrinking the
    // size of this region, we can create a new free region between this and the next used region.
    if (size + sizeof(Region) <= region->size)
    {
      size_t freeRegionSize = region->size - size;
      create_used_region(region, size);
      Region *freeRegion = (Region *)((uint8_t*)region + size);
      create_free_region(freeRegion, freeRegionSize);
      link_to_free_list(freeRegion);
      return 1;
    }
    else if (size <= region->size)
    {
      // Caller was asking to shrink the size, but due to not being able to fit a full Region in the shrunk
      // area, we cannot actually do anything. This occurs if the shrink amount is really small. In such case,
      // just call it success without doing any work.
      return 1;
    }
  }
  return 0;
}

static int locked_attempt_region_resize(Region *region, size_t size)
{
  MALLOC_ACQUIRE();
  int success = attempt_region_resize(region, size);
  MALLOC_RELEASE();
  return success;
}

void *emmalloc_aligned_realloc(void *ptr, size_t alignment, size_t size)
{
  if (!ptr)
    return emmalloc_memalign(alignment, size);

  if (size == 0)
  {
    free(ptr);
    return 0;
  }

  assert(IS_POWER_OF_2(alignment));

  // aligned_realloc() cannot be used to ask to change the alignment of a pointer.
  assert(HAS_ALIGNMENT(ptr, alignment));

  size = validate_alloc_size(size);

  // Calculate the region start address of the original allocation
  Region *region = (Region*)((uint8_t*)ptr - sizeof(uint32_t));

  // First attempt to resize the given region to avoid having to copy memory around
  if (locked_attempt_region_resize(region, size + REGION_HEADER_SIZE))
  {
#ifdef __EMSCRIPTEN_TRACING__
    emscripten_trace_record_reallocation(ptr, ptr, size);
#endif
    return ptr;
  }

  // If resize failed, we must allocate a new region, copy the data over, and then
  // free the old region.
  void *newptr = emmalloc_memalign(alignment, size);
  if (newptr)
  {
    memcpy(newptr, ptr, MIN(size, region->size - REGION_HEADER_SIZE));
    free(ptr);
  }
  return newptr;
}
extern __typeof(emmalloc_aligned_realloc) aligned_realloc __attribute__((weak, alias("emmalloc_aligned_realloc")));

// realloc_try() is like realloc(), but only attempts to try to resize the existing memory
// area. If resizing the existing memory area fails, then realloc_try() will return 0
// (the original memory block is not freed or modified). If resizing succeeds, previous
// memory contents will be valid up to min(old length, new length) bytes.
void *emmalloc_realloc_try(void *ptr, size_t size)
{
  if (!ptr)
    return 0;

  if (size == 0)
  {
    free(ptr);
    return 0;
  }
  size = validate_alloc_size(size);

  // Calculate the region start address of the original allocation
  Region *region = (Region*)((uint8_t*)ptr - sizeof(uint32_t));

  // Attempt to resize the given region to avoid having to copy memory around
  int success = locked_attempt_region_resize(region, size + REGION_HEADER_SIZE);
#ifdef __EMSCRIPTEN_TRACING__
  if (success)
    emscripten_trace_record_reallocation(ptr, ptr, size);
#endif
  return success ? ptr : 0;
}

// emmalloc_aligned_realloc_uninitialized() is like aligned_realloc(), but old memory contents
// will be undefined after reallocation. (old memory is not preserved in any case)
void *emmalloc_aligned_realloc_uninitialized(void *ptr, size_t alignment, size_t size)
{
  if (!ptr)
    return emmalloc_memalign(alignment, size);

  if (size == 0)
  {
    free(ptr);
    return 0;
  }

  size = validate_alloc_size(size);

  // Calculate the region start address of the original allocation
  Region *region = (Region*)((uint8_t*)ptr - sizeof(uint32_t));

  // First attempt to resize the given region to avoid having to copy memory around
  if (locked_attempt_region_resize(region, size + REGION_HEADER_SIZE))
  {
#ifdef __EMSCRIPTEN_TRACING__
    emscripten_trace_record_reallocation(ptr, ptr, size);
#endif
    return ptr;
  }

  // If resize failed, drop the old region and allocate a new region. Memory is not
  // copied over
  free(ptr);
  return emmalloc_memalign(alignment, size);
}

void *emmalloc_realloc(void *ptr, size_t size)
{
  return emmalloc_aligned_realloc(ptr, MALLOC_ALIGNMENT, size);
}
extern __typeof(emmalloc_realloc) realloc __attribute__((weak, alias("emmalloc_realloc")));

// realloc_uninitialized() is like realloc(), but old memory contents
// will be undefined after reallocation. (old memory is not preserved in any case)
void *emmalloc_realloc_uninitialized(void *ptr, size_t size)
{
  return emmalloc_aligned_realloc_uninitialized(ptr, MALLOC_ALIGNMENT, size);
}

extern __typeof(emmalloc_memalign) aligned_alloc __attribute__((weak, alias("emmalloc_memalign")));

int emmalloc_posix_memalign(void **memptr, size_t alignment, size_t size)
{
  assert(memptr);
  *memptr = emmalloc_memalign(alignment, size);
  return *memptr ?  0 : 12/*ENOMEM*/;
}
extern __typeof(emmalloc_posix_memalign) posix_memalign __attribute__((weak, alias("emmalloc_posix_memalign")));

void *emmalloc_calloc(size_t num, size_t size)
{
  size_t bytes = num*size;
  void *ptr = emmalloc_memalign(MALLOC_ALIGNMENT, bytes);
  if (ptr)
    memset(ptr, 0, bytes);
  return ptr;
}
extern __typeof(emmalloc_calloc) calloc __attribute__((weak, alias("emmalloc_calloc")));

static int count_linked_list_size(Region *list)
{
  int size = 1;
  for(Region *i = list->next; i != list; list = list->next)
    ++size;
  return size;
}

static size_t count_linked_list_space(Region *list)
{
  size_t space = 0;
  for(Region *i = list->next; i != list; list = list->next)
    space += region_payload_end_ptr(i) - region_payload_start_ptr(i);
  return space;
}

struct mallinfo emmalloc_mallinfo()
{
  MALLOC_ACQUIRE();

  struct mallinfo info;
  // Non-mmapped space allocated (bytes): For emmalloc,
  // let's define this as the difference between heap size and dynamic top end.
  info.arena = emscripten_get_heap_size() - (size_t)*emscripten_get_sbrk_ptr();
  // Number of "ordinary" blocks. Let's define this as the number of highest
  // size blocks. (subtract one from each, since there is a sentinel node in each list)
  info.ordblks = count_linked_list_size(&freeRegionBuckets[NUM_FREE_BUCKETS-1])-1;
  // Number of free "fastbin" blocks. For emmalloc, define this as the number
  // of blocks that are not in the largest pristine block.
  info.smblks = 0;
  // The total number of bytes in free "fastbin" blocks.
  info.fsmblks = 0;
  for(int i = 0; i < NUM_FREE_BUCKETS-1; ++i)
  {
    info.smblks += count_linked_list_size(&freeRegionBuckets[i])-1;
    info.fsmblks += count_linked_list_space(&freeRegionBuckets[i]);
  }

  info.hblks = 0; // Number of mmapped regions: always 0. (no mmap support)
  info.hblkhd = 0; // Amount of bytes in mmapped regions: always 0. (no mmap support)

  // Walk through all the heap blocks to report the following data:
  // The "highwater mark" for allocated space—that is, the maximum amount of
  // space that was ever allocated. Emmalloc does not want to pay code to 
  // track this, so this is only reported from current allocation data, and
  // may not be accurate.
  info.usmblks = 0;
  info.uordblks = 0; // The total number of bytes used by in-use allocations.
  info.fordblks = 0; // The total number of bytes in free blocks.
  // The total amount of releasable free space at the top of the heap.
  // This is the maximum number of bytes that could ideally be released by malloc_trim(3).
  info.keepcost = 0;
  Region *highestFreeRegion = 0;

  Region *root = listOfAllRegions;
  while(root)
  {
    Region *r = root;
    assert(debug_region_is_consistent(r));
    uint8_t *lastRegionEnd = (uint8_t*)(((uint32_t*)root)[2]);
    while((uint8_t*)r < lastRegionEnd)
    {
      assert(debug_region_is_consistent(r));

      if (region_is_free(r))
      {
        if (r > highestFreeRegion)
        {
          // Note: we report keepcost even though malloc_trim() does not
          // actually trim any memory.
          highestFreeRegion = r;
          info.keepcost = region_payload_end_ptr(r) - region_payload_start_ptr(r);
        }
        // Count only the payload of the free block towards free memory.
        info.fordblks += region_payload_end_ptr(r) - region_payload_start_ptr(r);
        // But the header data of the free block goes towards used memory.
        info.uordblks += REGION_HEADER_SIZE;
      }
      else
      {
        info.uordblks += r->size;
      }
      // Update approximate watermark data
      info.usmblks = MAX(info.usmblks, (int)(r + r->size));

      if (r->size == 0)
        break;
      r = next_region(r);
    }
    root = ((Region*)((uint32_t*)root)[1]);
  }

  MALLOC_RELEASE();
  return info;
}
extern __typeof(emmalloc_mallinfo) mallinfo __attribute__((weak, alias("emmalloc_mallinfo")));

int emmalloc_malloc_trim(size_t pad)
{
  // NOTE: malloc_trim() is not currently implemented. Currently sbrk() does
  // not allow shrinking the heap size, and neither does WebAssembly allow
  // shrinking the heap. We could apply trimming to DYNAMICTOP though.
  return 0;
}
extern __typeof(emmalloc_malloc_trim) malloc_trim __attribute__((weak, alias("emmalloc_malloc_trim")));

} // extern "C"

#endif // __EMSCRIPTEN__

#endif
