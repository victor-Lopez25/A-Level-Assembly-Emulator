test [ ! -d ./bin] && mkdir ./bin
pushd bin
gcc ../src/main.c -O2 -Wall -Wno-format -Wno-dangling-else -o ala.exe
popd