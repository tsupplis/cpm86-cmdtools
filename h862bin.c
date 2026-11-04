/* Convert Intel HEX / ASM-86 H86 to a contiguous binary image. */
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <stdio.h>
#include "hexbin.h"

#ifdef __STDC__
int main(int argc, char **argv) {
#else
int main(argc, argv) int argc; char **argv; {
#endif
    if (argc != 3) {
        fprintf(stderr, "ERR: Invalid command line\n");
        fprintf(stderr, "INF: h862bin file.h86 file.bin\n");
        fprintf(stderr, "     - converts Intel HEX / ASM-86 H86 to a flat binary image\n");
        fprintf(stderr, "     - strips the origin; rejects holes, overlaps and bad records\n");
        exit(1);
    }
    exit(hexbin_run(argv, 1));
    return 0;
}
