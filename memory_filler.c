#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *strings[] = {
    "If you can read this, this is really bad",
    "Burn after reading this string, it is a secret string",
    "Congratulations, you just spied on an application",
    "Wow, you broke the security boundary between user space and kernel",
    "Welcome to the wonderful world of microarchitectural attacks",
    "Please wait while we steal your secrets...",
    "Don't panic... But your CPU is broken and your data is not safe",
    "How can you read this? You should not read this!"};

int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("Usage: %s <gigabytes to fill>\n", argv[0]);
    return 0;
  }

  size_t size = strtoull(argv[1], NULL, 0);
  if (!size) {
    printf("\x1b[31;1m[!]\x1b[0m Invalid size!\n");
    return 1;
  }

  size_t bytesize = size * 1024ull * 1024ull * 1024ull;
  char *memory = malloc(bytesize);
  if (!memory) {
    printf("\x1b[31;1m[!]\x1b[0m Could not allocate %zd GB memory, try less!\n", size);
    return 1;
  }

  int lens[sizeof(strings) / sizeof(strings[0])];
  int i;
  for (i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
    lens[i] = strlen(strings[i]);
  }

  size_t pos = 0;
  while (pos < bytesize) {
    int string_index = rand() % (sizeof(strings) / sizeof(strings[0]));
    if (lens[string_index] + pos < bytesize) {
      memcpy(memory + pos, strings[string_index], lens[string_index]);
      pos += lens[string_index];
    } else {
      break;
    }
  }

  printf("\x1b[32;1m[+]\x1b[0m Press any key if you are done reading the secret\n");
  getchar();
  printf("\x1b[32;1m[+]\x1b[0m Done!\n");

  free(memory);
}
