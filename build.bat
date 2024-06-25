@echo off

set defines=-D_CRT_SECURE_NO_WARNINGS
set opts=-FC -GR- -EHa- -W4 -nologo -O2
set code=%cd%\src

IF NOT EXIST bin mkdir bin
pushd bin
cl %defines% %opts% %code%\main.c -Feala.exe /link -incremental:no
popd