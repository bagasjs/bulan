#!/bin/sh

./build/blnc.exe $@
fasm a.s a.obj
clang -o a.exe a.obj
./a.exe
