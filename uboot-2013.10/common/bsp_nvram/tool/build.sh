#!/bin/sh

g++ -I../include/ -I.. -I. -D__TOOL__ ../aes.c ../uni_base64.c file_flash.c file_nvram.c gen_nvram_zone.cpp -o gen_nvram_zone
