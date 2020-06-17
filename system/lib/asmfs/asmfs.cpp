#include <emscripten/asmfs.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

typedef struct AsmfsFile
{
  const char *filename;
  uint32_t size; // Length in bytes of the data pointer. (If file is backed on JS side, data == size == 0)
  uint8_t *data;
  AsmfsFile *child;
  AsmfsFile *sibling;
  EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS residencyFlags;
#ifdef ASMFS_MULTITHREADING
  pthread_t residentJsThread;
#endif
} AsmfsFile;

extern "C"
{
	void emscripten_asmfs_async_fetch_file_js(const char *url, AsmfsFile *file, EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS residencyFlags, asmfs_fetch_finished_callback onComplete, void *userData);
	size_t emscripten_asmfs_get_file_size_js(const AsmfsFile *file);
}

AsmfsFile root = { "/" };

// Given "[http://]/path/to/file.png?someQuery", returns pointer to "file.png" part.
static const char *FindUrlBasename(const char *url)
{
	assert(url);
	const char *basename = url;
	while(*url && *url != '?')
	{
		if (*url++ == '/') basename = url;
	}
	return basename;
}

// Special form of strcpy() that terminates on null character and on '?' character,
// to avoid extra/temp mallocs.
static void StrCpyUntilSearchQueryStart(char *dst, const char *src)
{
	assert(src);
	assert(dst);

	while(*src && *src != '?')
	{
		*dst++ = *src++;
	}
	*dst = '\0';
}

static char *StrdupPathJoin(const char *base, const char *relativeUrl)
{
	size_t baseLen = strlen(base);
	size_t relativeLen = strlen(relativeUrl);
	char *str = (char*)malloc(baseLen+relativeLen+1);
	strcpy(str, base);
	StrCpyUntilSearchQueryStart(str + baseLen, relativeUrl);
	return str;
}

uint8_t *emscripten_asmfs_get_file_ptr(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->data; // Can be null if file does not currently reside in Wasm.
}

size_t emscripten_asmfs_get_file_size(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;

	if (file->data)
	{
		return file->size;
	}
	return emscripten_asmfs_get_file_size_js(file);
}

const char *emscripten_asmfs_get_file_name(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->filename;	
}

uint8_t *emscripten_asmfs_copy_file_to_wasm_memory(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	free(file->data);
	file->size = emscripten_asmfs_get_file_size_js(file);
	file->data = (uint8_t*)malloc(file->size);
	emscripten_asmfs_copy_file_chunk_to_wasm_memory(file, 0, file->data, file->size);
	return file->data;
}

void emscripten_asmfs_dump_tree()
{
	printf("TODO emscripten_asmfs_dump_tree\n");
}

void emscripten_asmfs_async_file_fetch(const char *url, const char *dst, EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS residencyFlags, asmfs_fetch_finished_callback cb, void *userData)
{
	assert(url);
	assert(dst);
	AsmfsFile *file = (AsmfsFile *)malloc(sizeof(AsmfsFile));
	memset(file, 0, sizeof(AsmfsFile));
	size_t dstLen = strlen(dst);
	assert(dst[dstLen-1] != '\\' && "Use forward slash as the path delimiter!");
	if (dst[dstLen-1] == '/')
		file->filename = StrdupPathJoin(dst, FindUrlBasename(url));
	else
		file->filename = strdup(dst);

	emscripten_asmfs_async_fetch_file_js(url, file, residencyFlags, cb, userData);
}

void emscripten_asmfs_discard_file_from_wasm_memory(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	free(file->data);
	file->data = 0;
	file->size = 0;	
}


EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS emscripten_asmfs_get_file_residency(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->residencyFlags;
}
