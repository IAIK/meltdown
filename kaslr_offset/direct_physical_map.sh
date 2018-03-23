#!/bin/bash

if [ ! -f kernel_module/direct_physical_map.ko ]; then
    echo -e "\x1b[32;1m[+]\x1b[0m Building kernel module"
    cd kernel_module && make
    cd -
fi
if [ ! -f kernel_module/direct_physical_map.ko ]; then
    echo -e "\x1b[31;1m[-]\x1b[0m Failed building kernel module"
    exit
fi

if ! grep -q direct_physical_map /proc/modules; then
    echo -e "\x1b[32;1m[+]\x1b[0m Loading kernel module"
    insmod kernel_module/direct_physical_map.ko
fi
if ! grep -q direct_physical_map /proc/modules; then
    echo -e "\x1b[31;1m[-]\x1b[0m Failed loading kernel module"
    exit
fi

read OFFSET </proc/direct_physical_map

echo -e "\x1b[32;1m[+]\x1b[0m Direct physical map offset at \x1b[33;1m$OFFSET\x1b[0m"

