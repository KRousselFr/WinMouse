windres -i WinMouse.rc -o WinMouseRes.o
gcc -std=c11 -c WinMouse.c -o WinMouse.o -Wall -Wextra -Wconversion -pedantic
gcc WinMouse.o WinMouseRes.o -o WinMouse.exe -Wall -Wextra -Wconversion -O -mwindows
strip WinMouse.exe
