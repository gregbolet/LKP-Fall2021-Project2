#!/bin/bash

sudo insmod proj2.ko int_str="1,2,3,4,5"
cat /proc/proj2
sudo rmmod proj2.ko
dmesg | tail -n 5

