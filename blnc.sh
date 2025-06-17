#!/bin/sh

./build/blnc.exe $@
# nasm -f win64 -g -F cv8 a.s -o ./build/a.obj
fasm ./a.s ./build/a.obj
clang -g -o ./build/a.exe ./build/a.obj
