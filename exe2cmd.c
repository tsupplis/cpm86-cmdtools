/*    Convert the Personal CP/M-86 BDOS (compiled as a small-model EXE)
 *    to CMD.
 *    Copyright (C) 2003, 2007  John Elliott <jce@seasip.demon.co.uk>
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
#include <stdlib.h>
#include <string.h>
#endif

#include <errno.h>
#include <stdio.h>
#include "filecmp.h"
#if defined(__GNUC__) || defined(__clang__)
#define BINARY_READ "rb"
#define BINARY_WRITE "wb"
#else
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif

#define DS_LEN 0xF0 /* Length of data segment (paras) */

/* Compile-time check, including on Aztec 3.x without #error support. */
typedef char ds_len_check[(DS_LEN * 16 >= 72) ? 1 : -1];

int gl_base = 0x60;

/* EXE header (plus first 8 bytes of EXE file) */
unsigned char exe_header[0x208];

/* CMD header */
unsigned char cmd_header[0x80];

/* Read a word from the EXE header */
#ifdef __STDC__
unsigned short peek(int offset)
#else
unsigned short peek(offset)
int offset;
#endif
{
    unsigned short l = exe_header[offset];
    unsigned short h = exe_header[offset + 1];
    return (h << 8) | l;
}

/* Add a group to the CMD header */
#ifdef __STDC__
void addgroup(int type, unsigned short paras, unsigned short base,
              unsigned short minparas, unsigned short maxparas)
#else
addgroup(type, paras, base, minparas, maxparas) int type;
unsigned short paras;
unsigned short base;
unsigned short minparas;
unsigned short maxparas;
#endif
{
    static unsigned char *group = cmd_header;
    *group++ = type;
    *group++ = (paras & 0xFF);
    *group++ = (paras >> 8);
    *group++ = (base & 0xFF);
    *group++ = (base >> 8);
    *group++ = (minparas & 0xFF);
    *group++ = (minparas >> 8);
    *group++ = (maxparas & 0xFF);
    *group++ = (maxparas >> 8);
    fprintf(stderr, "INF: Group %d: len=%04x base=%04x min=%04x max=%04x\n",
            type, paras, base, minparas, maxparas);
}

/* Copy bytes from one file to another. */
#ifdef __STDC__
int mvbytes(FILE *fpexe, FILE *fpcmd, long count)
#else
int mvbytes(fpexe, fpcmd, count) FILE *fpexe;
FILE *fpcmd;
long count;
#endif
{
    int c;

    while (count) {
        c = fgetc(fpexe);
        if (c == EOF) {
            fprintf(stderr,
                    "ERR: Cannot read required input bytes\n");
            return -1;
        }
        if (fputc(c, fpcmd) == EOF) {
            fprintf(stderr, "ERR: Cannot write output (%d)\n", errno);
            return -1;
        }
        --count;
    }
    return 0;
}

#ifdef __STDC__
int main_alt(int argc, char **argv)
#else
int main_alt(argc, argv)
int argc;
char **argv;
#endif
{
    FILE *fpexe, *fpcmd;
    unsigned short cseg, dseg, sseg, dstop;
    unsigned short dsmin, dsmax, exeparas;
    unsigned long exelen, dsbytes;
    long pos;
    int n;
    unsigned long base;
    char *p;
    int digit;

    if (argc < 3) {
        fprintf(stderr, "ERR: Invalid command line\n");
        fprintf(stderr, "ERR: exe2cmd file.exe file.cmd\n");
        return 1;
    }
    for (n = 3; n < argc; n++) {
        if (strncmp(argv[n], "base=", 5) && strncmp(argv[n], "BASE=", 5)) {
            fprintf(stderr, "ERR: Invalid base= option '%s'\n", argv[n]);
            return 1;
        }
        p = argv[n] + 5;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
        base = 0;
        if (!*p) {
            fprintf(stderr, "ERR: Invalid base= option '%s'\n", argv[n]);
            return 1;
        }
        for (; *p; p++) {
            if (*p >= '0' && *p <= '9') digit = *p - '0';
            else if (*p >= 'a' && *p <= 'f') digit = *p - 'a' + 10;
            else if (*p >= 'A' && *p <= 'F') digit = *p - 'A' + 10;
            else digit = -1;
            if (digit < 0 || base > (0xFFFFUL - digit) / 16) {
                fprintf(stderr, "ERR: Invalid base= option '%s'\n", argv[n]);
                return 1;
            }
            base = base * 16 + digit;
        }
        gl_base = (int)base;
    }
    if (same_file(argv[1], argv[2])) {
        fprintf(stderr, "ERR: Input and output must be different files\n");
        return 1;
    }

    /* Load EXE header, and extract the segment info. This code depends
     * on the word at CS:6 being the length of the code segment in
     * paragraphs. */
    fpexe = fopen(argv[1], BINARY_READ);
    if (!fpexe) {
        fprintf(stderr, "ERR: Can't open input '%s' (%d)\n", argv[1], errno);
        return 1;
    }
    if (fread(exe_header, 1, sizeof(exe_header), fpexe) <
            (int)sizeof(exe_header)
        /* Check for CP/M-86 BDOS:
         *   MZ signature
         *   32 header paragraphs
         *   First relocation is at address 6 or less */
        || peek(0) != 0x5A4D || peek(8) != 32 || peek(32) > 6) {
        fprintf(stderr, "ERR: %s is not a BDOS image\n", argv[1]);
        fclose(fpexe);
        return 1;
    }
    fprintf(stderr, "INF: Signature      = %x\n", peek(0));
    fprintf(stderr, "INF: Reloc Entries  = %d\n", peek(6));
    cseg = 0;
    dseg = peek(0x206);
    sseg = peek(14);
    exelen = 512L * peek(4);
    if (peek(2))
        exelen -= (512 - peek(2));
    exeparas = (exelen + 15) >> 4;
    /*
        dstop = exeparas - peek(8);
        ssparas = dstop - sseg;
        dsmin = dstop - dseg + peek(10);
        dsmax = dstop - dseg + peek(12);
    */
    /* This is CP/M-86, the DSEG is a fixed size */
    dsmin = dsmax = DS_LEN;
    if (dseg > 0xFFFFU - DS_LEN) {
        fprintf(stderr, "ERR: Data segment top exceeds FFFFh paragraphs\n");
        fclose(fpexe);
        return 1;
    }
    dstop = dseg + DS_LEN;
    fprintf(stderr, "INF: EXE length     = %04lx\n", exelen);
    fprintf(stderr, "INF: EXE header len = %04x\n", peek(8));
    fprintf(stderr, "INF: EXE paras      = %04x\n", exeparas);
    fprintf(stderr, "INF: DS top         = %04x\n", dstop);
    fprintf(stderr, "INF: CS: %04x length %04x\n", cseg, dseg - cseg);
    fprintf(stderr, "INF: DS: %04x length %04x\n", dseg, dstop - dseg);
    if (dseg == 0) {
        fprintf(
            stderr,
            "ERR: CS:6 word is zero; cannot determine code segment length\n");
        fclose(fpexe);
        return 1;
    }
    if (exeparas < dstop) {
        fprintf(stderr,
                "WRN: EXE image (%04x paras) shorter than expected dstop "
                "(%04x); output will contain 0xFF bytes\n",
                exeparas, dstop);
    }

    /* Now generate CP/M-86 header. */
    addgroup(1, dseg - cseg, 0, dseg - cseg, 0);
    addgroup(2, dstop - dseg, 0, dsmin, 0);

    fpcmd = fopen(argv[2], BINARY_WRITE);
    if (fpcmd == NULL) {
        fprintf(stderr, "ERR: Can't open output '%s' (%d)\n", argv[2], errno);
        fclose(fpexe);
        return 2;
    }
    if (fwrite(cmd_header, 1, sizeof(cmd_header), fpcmd) <
            (int)sizeof(cmd_header) ||
        fwrite(exe_header + 0x200, 1, 8, fpcmd) < 8) {
        fprintf(stderr, "ERR: Can't write output '%s' (%d)\n", argv[2], errno);
        fclose(fpcmd);
        fclose(fpexe);
        return 2;
    }
    /* Write the code segment. The first 8 bytes have been done. */
    if (mvbytes(fpexe, fpcmd, 16L * (long)(dseg - cseg) - 8L)) goto fail;
    /* Write the data segment. */
    dsbytes = 16L * (dstop - dseg);
    if (mvbytes(fpexe, fpcmd, 40L)) goto fail;
    /* Apply fixups to offsets within the data segment */
    for (n = 0; n < 8; n++) {
        unsigned seg;
        if (n == 6) {
            if (mvbytes(fpexe, fpcmd, 4L)) goto fail; /* Address of DOS module */
            continue;
        }
        if (mvbytes(fpexe, fpcmd, 2L)) goto fail; /* Offset */
        if (fread(exe_header, 1, 2, fpexe) != 2) {
            fprintf(stderr, "ERR: Cannot read fixup word\n");
            goto fail;
        }
        seg = peek(0) + gl_base;
        if (fputc(seg & 0xFF, fpcmd) == EOF ||
            fputc((seg >> 8) & 0xFF, fpcmd) == EOF) {
            fprintf(stderr, "ERR: Cannot write fixup word\n");
            goto fail;
        }
    }
    if (mvbytes(fpexe, fpcmd, dsbytes - 72L)) goto fail;
    /* Pad the CMD file out to a whole number of records */
    pos = ftell(fpcmd);
    if (pos < 0) {
        fprintf(stderr, "ERR: Cannot get output position\n");
        goto fail;
    }
    while (pos & 0x7F) {
        if (fputc(0, fpcmd) == EOF) {
            fprintf(stderr, "ERR: Cannot write output padding\n");
            goto fail;
        }
        ++pos;
    }
    if (fclose(fpcmd)) {
        fprintf(stderr, "ERR: Cannot close output '%s' (%d)\n", argv[2], errno);
        fclose(fpexe);
        return 2;
    }
    fclose(fpexe);
    return 0;
fail:
    fclose(fpcmd);
    fclose(fpexe);
    return 2;
}

#ifdef __STDC__
int main(int argc, char **argv)
#else
int main(argc, argv)
int argc;
char **argv;
#endif
{
    exit(main_alt(argc, argv));
    return 0;
}
