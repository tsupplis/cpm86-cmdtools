#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__APPLE__) || defined(__gnu_linux__)
#include <unistd.h>
#define BINARY "b"
#else
#define BINARY ""
#endif

unsigned char header[384];

int main_alt(int argc, char **argv) {
    FILE *fin, *fout;
    long pos, paras, maxsize;
    int c;

    if (argc < 3) {
        fprintf(stderr,"ERR: Invalid command line\n");
        fprintf(stderr,"ERR: cmdinfo file.bin file.cmd\n");
       return 1;
    }
    /* Open the input file, and seek to the end to get its size */
    fin = fopen(argv[1], "r" BINARY);
    if (!fin) {
        fprintf(stderr,"ERR: Can't open input '%s' (%d)\n",argv[1],errno);
        return 1;
    }
    if (fseek(fin, 0L, SEEK_END) < 0) {
        fclose(fin);
        fprintf(stderr,"ERR: Can't seek to end of input (%d)\n",errno);
        return 1;
    }
    /* Get size */
    pos = ftell(fin);
    /* Seek back to the beginning */
    if (pos < 0 || fseek(fin, 0L, SEEK_SET) < 0) {
        fprintf(stderr,"ERR: Can't seek to beginning of input (%d)\n",errno);
        return 1;
    }

    /* Calculate size in paragraphs. Add 16 paragraphs for the
     * Zero Page. */
    paras = (pos + 15) / 16 + 0x10;

    /* The CP/M-86 CMD format does not allow groups larger than 1M */
    if (paras > 0xFFFF) {
        fclose(fin);
        fprintf(stderr, "ERR: Code group size would exceed 1MB\n");
        return 1;
    }
    /* If the file is smaller than 64k, request a 64k segment. If
     * it's bigger, request all the memory CP/M-86 is prepared to
     * give. */
    if (paras < 0x1000) {
        maxsize = 0x1000;
    } else {
        maxsize = 0xFFFF;
    }
    memset(header, 0, sizeof(header));
    header[0] = 1; /* Code group */
    header[1] = (paras & 0xFF);
    header[2] = (paras >> 8) & 0xFF; /* Group length */
    header[5] = (paras & 0xFF);
    header[6] = (paras >> 8) & 0xFF; /* Minimum size */
    header[7] = (maxsize & 0xFF);
    header[8] = (maxsize >> 8) & 0xFF; /* Maximum size */

    /* Open output file */
    fout = fopen(argv[2], "w" BINARY);
    if (!fout) {
        fprintf(stderr,"ERR: Can't open output '%s' (%d)\n",argv[2], errno);
        fclose(fin);
        return 1;
    }
    /* Write header and Zero Page */
    if (fwrite(header, 1, sizeof(header), fout) < sizeof(header)) {
        fprintf(stderr,"ERR: Can't write header to output (%d)\n",errno);
        fclose(fout);
        unlink(argv[2]);
        fclose(fin);
        return 1;
    }
    /* Copy data */
    while ((c = fgetc(fin)) != EOF) {
        if (fputc(c, fout) == EOF) {
            fprintf(stderr,"ERR: Can't write content to output (%d)\n",errno);
            fclose(fout);
            unlink(argv[2]);
            fclose(fin);
            return 1;
        }
    }
    if (fclose(fout)) {
        fprintf(stderr,"ERR: Can't close output properly (%d)\n",errno);
        unlink(argv[2]);
        fclose(fin);
        return 1;
    }
    fclose(fin);
    return 0;
}

int main(int argc, char **argv) {
    exit(main_alt(argc,argv));
    return 0;
}
