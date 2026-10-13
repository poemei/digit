#include <stdio.h>
#include <string.h>
#include "digit.h"
#include "mvc_console.h"

int main(int argc, char **argv)
{
    if (argc == 1) return digit_mvc_console();
    if (argc == 2 && strcmp(argv[1], "--core") == 0)
        return digit_initialize();
    fputs("Usage: ./bin/digit [--core]\n", stderr);
    return 2;
}
