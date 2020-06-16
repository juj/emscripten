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
	void emscripten_asmfs_async_fetch_file_js(const char *url, AsmfsFile *fileHandle, asmfs_fetch_finished_callback onComplete, void *userData);
	size_t emscripten_asmfs_get_file_size_js(const char *path);
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

uint8_t *emscripten_asmfs_get_file_ptr(AsmfsFileHandle file)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->data; // Can be null if file does not currently reside in Wasm.
}

size_t emscripten_asmfs_get_file_size(AsmfsFileHandle file)
{
	if (file->data)
	{
		return file->size;
	}
	return emscripten_asmfs_get_file_size_js(file);
}

const char *emscripten_asmfs_get_file_name(AsmfsFileHandle file)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->filename;	
}

uint8_t *emscripten_asmfs_copy_file_to_wasm_memory(AsmfsFileHandle file)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	free(file->data);
	size_t jsSize = emscripten_asmfs_get_file_size_js(file);
	file->data = malloc(jsSize);
	emscripten_asmfs_copy_file_chunk_to_wasm_memory(file, 0, file->data, jsSize);
	return file->data;
}

void emscripten_asmfs_dump_tree()
{
	printf("TODO emscripten_asmfs_dump_tree\n");
}

void emscripten_asmfs_async_file_fetch_to_js(const char *url, const char *dst, asmfs_fetch_finished_callback cb, void *userData)
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

	emscripten_asmfs_async_fetch_file_js(url, file, cb, userData);
}

EMSCRIPTEN_ASMFS_FILE_RESIDENCY_FLAGS emscripten_asmfs_get_file_residency(AsmfsFileHandle fileHandle)
{
	assert(fileHandle);
	AsmfsFile *file = (AsmfsFile*)fileHandle;
	return file->residencyFlags;
}

void emscripten_asmfs_dump_tree(void);
