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

/*    Output file layout:
 *
 *      Offset  Size   Content
 *      ------  -----  -------
 *      0       128    CMD header (group descriptors + flags)
 *      128     256    Zero page (all zeros); omitted with -n
 *      384     N      Binary code (pos bytes)
 *      384+N   P      Paragraph padding: P = (16 - (pos % 16)) % 16
 *      end     Q      512-byte record padding: Q = (512 - (total % 512)) % 512
 *
 *    With -n the zero page is not inserted and not counted in the group size.
 *
 *    Examples (with zero page):
 *      pos= 57: paras=20, rest= 7, total=448, pad= 64 -> file=  512 bytes (1 record)
 *      pos=110: paras=23, rest= 2, total=496, pad= 16 -> file=  512 bytes (1 record)
 *      pos=125: paras=24, rest= 3, total=512, pad=  0 -> file=  512 bytes (1 record)
 *      pos=256: paras=32, rest= 0, total=640, pad=384 -> file= 1024 bytes (2 records)
 */
 
#ifdef __STDC__
#include <string.h>
#include <stdlib.h>
#endif
#include <stdio.h>
#include <errno.h>

#ifdef __STDC__
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__GNUC__) || defined(__clang__)
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

unsigned char header[128];

/* Parse an unsigned hex string (no 0x prefix). Returns -1 on error. */
#ifdef __STDC__
long parsehex(const char *s) {
#else
long parsehex(s) char *s; {
#endif
    long val = 0;
    if (!s || !*s) return -1;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9')      d = *s - '0';
        else if (*s >= 'a' && *s <= 'f') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') d = *s - 'A' + 10;
        else return -1;
        val = val * 16 + d;
        s++;
    }
    return val;
}

#ifdef __STDC__
void usage() {
#else
usage() {
#endif
    fprintf(stderr, "ERR: Invalid command line\n");
    fprintf(stderr, "INF: bin2cmd [-n] [-m maxhex] file.bin file.cmd\n");
    fprintf(stderr, "     - converts a .COM/.BIN to a CP/M-86 .CMD file\n");
    fprintf(stderr, "     -n         do not insert a 256-byte zero page after the header\n");
    fprintf(stderr, "     -m maxhex  max segment size in hex bytes (0=any, clamped to 10000h=64K)\n");
    fprintf(stderr, "                ignored if <= code size\n");
}

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
    long total_written;
    int c;
    int rest=0;
    int zeropage=1;
    static unsigned char zpbuf[256];

    maxsize = 0;
    while (argc > 1 && argv[1][0] == '-') {
        if (strcmp(argv[1], "-n") == 0) {
            zeropage = 0;
            argc--; argv++;
        } else if (strcmp(argv[1], "-m") == 0) {
            if (argc < 3) { usage(); return 1; }
            maxsize = parsehex(argv[2]);
            if (maxsize < 0) {
                fprintf(stderr, "ERR: -m value is not a valid hex number\n");
                return 1;
            }
            if (maxsize > 0x10000) {
                fprintf(stderr, "WRN: -m value clamped to 10000h (64K)\n");
                maxsize = 0x10000;
            }
            argc -= 2; argv += 2;
        } else {
            fprintf(stderr, "ERR: Unknown option '%s'\n", argv[1]);
            usage();
            return 1;
        }
    }

    if (argc < 3) {
        usage();
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
     * Zero Page (written separately, not counted in rest), unless
     * -n was given: the input then already provides it. */
    paras = (pos + 15) / 16 + (zeropage ? 0x10 : 0);
    rest = (16 - (pos % 16)) % 16;

    /* The CP/M-86 CMD format does not allow groups larger than 1M */
    if (paras > 0xFFFF) {
        fclose(fin);
        fprintf(stderr, "ERR: Code group size would exceed 1MB\n");
        return 1;
    }
    /* convert -m bytes to paragraphs; ignore if <= code size */
    {
        long maxparas = maxsize > 0 ? (maxsize + 15) / 16 : 0;
        if (maxparas > 0 && maxparas <= paras) {
            fprintf(stderr, "WRN: -m value <= code size (%ldh paras), ignored\n", paras);
            maxparas = 0;
        }
        /* group descriptor layout (header_t):
         *   +0  DB  form        (1=code)
         *   +1  DW  length      in paragraphs
         *   +3  DW  base        0 = relocatable
         *   +5  DW  min size    in paragraphs
         *   +7  DW  max size    in paragraphs (0=no constraint, max 1000h=64K) */
        memset(header, 0, sizeof(header));
        header[0] = 1;                          /* form: code group */
        header[1] = (paras & 0xFF);
        header[2] = (paras >> 8) & 0xFF;       /* length in paragraphs */
        /* header[3,4] = base, 0 = relocatable (already zeroed) */
        header[5] = (paras & 0xFF);
        header[6] = (paras >> 8) & 0xFF;       /* min size in paragraphs */
        header[7] = (maxparas & 0xFF);
        header[8] = (maxparas >> 8) & 0xFF;    /* max size in paragraphs, 0 = no constraint */
    }

    fprintf(stderr, "INF: paras(%ld), size(%ld)\n",paras,paras*16);
    /* Open output file */
    fout = fopen(argv[2], BINARY_WRITE);
    if (!fout) {
        fprintf(stderr,"ERR: Can't open output '%s' (%d)\n",argv[2], errno);
        fclose(fin);
        return 1;
    }
    fprintf(stderr, "INF: header size(%ld)\n",sizeof(header));
    /* Write CMD header */
    if (fwrite(header, 1, sizeof(header), fout) < sizeof(header)) {
        fprintf(stderr,"ERR: Can't write header to output (%d)\n",errno);
        fclose(fout);
        unlink(argv[2]);
        fclose(fin);
        return 1;
    }
    /* Write zero page (256 bytes of zeros) unless -n was given */
    if (zeropage) {
        memset(zpbuf, 0, sizeof(zpbuf));
        if (fwrite(zpbuf, 1, sizeof(zpbuf), fout) < sizeof(zpbuf)) {
            fprintf(stderr,"ERR: Can't write zero page to output (%d)\n",errno);
            fclose(fout);
            unlink(argv[2]);
            fclose(fin);
            return 1;
        }
    }
    /* Total bytes that will be written: header + zero page (if any) + data rounded up to paragraph */
    total_written = 128 + (zeropage ? 256 : 0) + pos + rest;
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
    /* Pad output to a 512-byte record boundary */
    {
        long pad = (512 - (total_written % 512)) % 512;
        while(pad--) {
            if (fputc(0, fout) == EOF) {
                fprintf(stderr,"ERR: Can't write padding to output (%d)\n",errno);
                fclose(fout);
                unlink(argv[2]);
                fclose(fin);
                return 1;
            }
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
