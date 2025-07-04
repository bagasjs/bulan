# Bulan
A small programming language inspired by [Tsoding's B implementation](https://github.com/tsoding/b). The language starts as something that feels like B but with a lot of syntactical sugars, now I am planning to add a lot of feature other than syntax sugars.

## Features
1. Manual memory management and pointers
2. Easy C integration, You can just call C function right away
2. (planned) Composite data, like struct (but still weakly typed)
3. (planned) Codegen backend as a plugin/DLL i.e. `blnc -t ./plugin/codegen_GBA.dll ./demo/hello.bln`
4. (planned) More platform (WebAssembly, Aarch64, and older platform)

## Building
This project is built with [nob](https://github.com/tsoding/nob.h). This is how you build the project
```sh
# Bootstrap nob
$ cc -o nob.exe ./nob.c
# Run nob
$ ./nob.exe
# Run the compiler
$ ./build/blnc.exe ./demo/main.bln
```
