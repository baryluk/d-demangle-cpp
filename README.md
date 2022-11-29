D programming language (dlang) symbol name demangler for C++
============================================================


```cpp
std::cout << demangle_d::demangle("_D3std5stdio__T8writeflnTaTiZQoFNfxAaiZv") << std::endl;
// Will print: void std.stdio.writefln!(char, int)(const char[], int) @safe
```

Goals
=====

* Easy of integration into any C++ codebase
* Flexible licensing
* Code simplicity following the spec closely in shape

Status
======

Alpha. It mostly works.

Simple test script is provided to test few symbols.

A full fuzzing testing would also be good to have.

Missing features:
* Underspecified features of demangling spec
* some complex delegate type parameter types are not formatted correctly
* wstring and dstring literals
* assosciative array literals
* inner functions are not demangled properly fully (but it is okish)
* 64-bit integers and float values are untested (should work, but
  validation is not complete).

Building
========


Just take `d-demangle.h` and `d-demangle.cc` and integrate into your code
base.

It can also be built as a library and linked statically or dynamically.

You can also combine both files and use if you want.

Example working `Makefile` is provided.

Test program
============

A simple command line interface (similar to `c++filt`) is provided.


```shell
$ make d-demangle
$ ./d-demangle _D3std5stdio__T8writeflnTaTiZQoFNfxAaiZv
void std.stdio.writefln!(char, int)writefln(const char[], int) @safe
$
```

Check `./d-demangle --help` for extra options.

API stability
=============

`demangle_d::demangle(const std::string&)` will be provided.

Extra parameters or overrides are not guaranteed to exist long term. (But
are likely to be easy to change later).

Compatibility
=============

We strive to support C++98 and anything after that. If C++11 is detected,
it will internally use `std::unordered_map` for back references.
Otherwise it will use `std::map`. So no modern features like `auto`,
`brace initalization`, range based `for` loops, move semantics, etc.

The intent is code execution correctness and compatibility, not speed.

Routinly tested on gcc and clang on Linux.

Library facilities required: `size_t`, `std::string`,
`std::to_string(size_t)` (For C++11. For C++98 it will use `sprintf` from
`cstdio`), `std::runtime_error`, standard allocator.

It would be nice to not require exceptions to be enabled. Patches
welcome.

If desired `std::string` can be swapped for any other type with similar
interface.

Note: On 32-bit systems, some extremally large or malformed mangled
symbols, might fail to demangle. Hopefully just throw exception or return
back the input without changes, without crashing or corrupting anything.

The code does not use too many C++ specific or complex features, which
should make it easy to port to other languages like C, Go, Rust, Java,
JavaScript, Python, PHP or even D, while keeping most of the code the
same.

License
=======

To facilitate wide adoption in any other software, the code is multi
licensed.

Select any non-empty subset of licenses available for your project.

- The Boost Source License (BSL)
- MIT License
- BSD 3-Clouse license
- LGPL License
- GPL v2 or later license

Contributing
============

Contributions, especially for compiler compatibility and correctess are
welcome. By submitting pull requests you agree for the code to be
released using any of the above licenses (including Copyright lines). You
name will remain in the Git history, but not necassarily in a
redistributed code.

Coding style follow Google style guidelines in formatting with 80 columns
width, and `*`/`&` in types attached to the variable name. Function names
are lower snake case tho.
