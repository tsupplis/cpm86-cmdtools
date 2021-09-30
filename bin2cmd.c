/*    Convert .COM/.BIN to CP/M-86 CMD.
 *
 *    This library is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Library General Public
 *    License as published by the Free Software Foundation; either
 *    version 2 of the License, or (at your option) any later version.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Library General Public License for more details.
 *
 *    You should have received a copy of the GNU Library General Public
 *    License along with this library; if not, write to the Free
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifdef __STDC__
#include <string.h>
#include <stdlib.h>
#endif
#include <stdio.h>
#include <errno.h>

#ifdef __STDC__
#if defined(__APPLE__) || defined(__gnu_linux__)
#include <unistd.h>
#define BINARY_READ "rb"
#define BINARY_WRITE "wb"
#else
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif
#else
#define SEEK_SET 0
#define SEEK_END 2
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif

unsigned char header[384];

#ifdef __STDC__
int main_alt(int argc, char **argv) {
#else
int main_alt(argc, argv) 
        int argc; 
        char **argv; 
{
#endif
    FILE *fin, *fout;
    long pos, paras, maxsize;
    int c;
    int rest=0;

    if (argc < 3) {
        fprintf(stderr,"ERR: Invalid command line\n");
        fprintf(stderr,"ERR: cmdinfo file.bin file.cmd\n");
       return 1;
    }
    /* Open the input file, and seek to the end to get its size */
    fin = fopen(argv[1], BINARY_READ);
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
    rest=(paras-1)*16-pos;

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
    fout = fopen(argv[2], BINARY_WRITE);
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
    while(rest--) {
        if (fputc(0, fout) == EOF) {
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

#ifdef __STDC__
int main(int argc, char **argv) {
#else
int main(argc, argv) 
        int argc; 
        char **argv; 
{
#endif
    exit(main_alt(argc,argv));
    return 0;
}
