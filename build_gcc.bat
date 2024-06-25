@echo off

set code=%cd%\src
IF NOT EXIST bin mkdir bin
pushd bin
gcc %code%\main.c -O2 -Wall -Wno-format -Wno-dangling-else -o ala.exe
popd