#include <stdio.h>
#include <string.h>
#include "digit.h"
#include "mvc_console.h"

int main(int argc, char **argv)
{
    if (argc == 1) return digit_initialize();
    if (argc == 2 && strcmp(argv[1], "--mvc") == 0)
        return digit_mvc_console();
    fputs("Usage: ./bin/digit [--mvc]\n", stderr);
    return 2;
}
