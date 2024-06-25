## A Level Assembly Emulator

This is an assembly emulator for the assembly language shown in cambridge A level computer science. Find a list of instructions in instruction_list.txt. Find example code in the examples directory. Note there are a couple of extra instructions (which can only be used by specifying the '-extra' flag.

Known limitation: cannot make a label called 'B' or '#' since they indicate the start of a number

# Building

To build ala.exe you need either MSVC or gcc installed.
To build in linux, use gcc.

Using MSVC:
1. make a cmd instance
2. find vcvarsall.bat (in a folder in visual studio files)
3. run "vcvarsall.bat x64"
3. go back to this directory (without closing cmd)
4. run build.bat

Using gcc:
1. run build_gcc.bat or build.sh

It is recommended to put ala.exe in a directory that is in the PATH if you want to use it frequently.

You can download pre-built binaries for the debugger [here](https://github.com/victor-Lopez25/A-Level-Assembly-Emulator/releases).

## Licenses

[A Level Assembly Emulator]() © 2024 by [Víctor López Cortés](https://github.com/victor-Lopez25) is licensed under [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)

[String_View implementation](https://github.com/tsoding/sv) for C provided by Alexey Kutepov (Tsoding)
