#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <emscripten/emscripten.h>

void read_file()
{
  FILE *file = fopen("hello_file.txt", "rb");
  assert(file);
  fseek(file, 0, SEEK_END);

  long size = ftell(file);
  rewind(file);
  printf("size: %ld\n", size);

  char *buffer = (char*) malloc (sizeof(char)*(size+1));
  assert(buffer);
  buffer[size] = '\0';

  size_t read = fread(buffer, 1, size, file);
  printf("File contents: %s\n", buffer);
  assert(size == 6);
  assert(!strcmp(buffer, "Hello!"));

  fclose(file);
}

#include <emscripten/pthread_proxy_main.h>
int emscripten_main(int argc, char **argv)
{
  read_file();
  read_file();
#ifdef REPORT_RESULT
  REPORT_RESULT(0);
#endif
}
