/*    Display CP/M-86 .CMD Structure 
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
#ifndef __LEGACY__
#include <string.h>
#endif
#include <ctype.h>
#include <stdio.h>
#ifndef __LEGACY__
#if defined(__APPLE__) || defined(__gnu_linux__)
#include <unistd.h>
#include <libgen.h>
#define PACK __attribute__((packed))
#endif
#endif
#ifndef __LEGACY__
#include <stdint.h>
#include <stdlib.h>
#else
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int size_t;
#define SEEK_SET 0
#endif

#ifdef __CPM86__
#define PACK
#define basename(a) (a)
#endif

#ifdef __MSDOS__
#define PACK
#define basename(a) (a)
#endif

typedef struct PACK _header_t {
    uint8_t form;
    uint16_t length;
    uint16_t base;
    uint16_t min;
    uint16_t max;
} header_t;

typedef struct PACK _header_block_t {
    header_t header[8];
    uint8_t padding[56];
} header_block_t;

char *g_type[16] = {"NULL",      "CODE",        "DATA",      "EXTRA",
                    "STACK",     "AUX #1",      "AUX #2",    "AUX #3",
                    "AUX #4",    "SHARED CODE", "UNUSED 10", "UNUSED 11",
                    "UNUSED 12", "UNUSED 13",   "UNUSED 14", "ESCAPE CODE"};

#ifndef __LEGACY__
void dump(FILE *out, int index, char type, uint16_t base, uint16_t length,
          size_t offset, FILE *fin) {
#else
void dump(out, index, type, base, length, offset, fin) 
          FILE *out; 
          int index; 
          char type; 
          uint16_t base; 
          uint16_t length;
          size_t offset; 
          FILE *fin; {
#endif
    char file_name[12 + 1];
    FILE *fout;
    uint16_t left = length;
    char buffer[128];

    if(!fin) {
        return;
    }
    sprintf(file_name, "%c%d-%04x.bin", type, index, base);
    fprintf(out, "INF: Output to %s\n", file_name);
    unlink(file_name);
    fout = fopen(file_name, "w");
    if (!fout) {
        fprintf(stderr, "ERR: Cannot write extract file (%s)\n", file_name);
        fclose(fout);
        unlink(file_name);
        return;
    }
    if (fseek(fin, offset, SEEK_SET)) {
        fprintf(stderr, "ERR: Cannot seek to offset position (%d,%lu)\n", index,
                offset);
        fclose(fout);
        unlink(file_name);
        return;
    }
    while (left > 0) {
        size_t mo;
        size_t mi = fread(buffer, 1, 128>left?left:128, fin);
        if (mi == 0) {
            if (ferror(fin)) {
                fprintf(stderr, "ERR: Cannot read input (%d,%lu)\n", index,
                        offset);
                fclose(fout);
                unlink(file_name);
                return;
            }
            break;
        }
        mo = fwrite(buffer, 1, mi>left?left:mi, fout);
        if (mo != mi) {
            fprintf(stderr, "ERR: Cannot rite output fully to (%s)\n",
                    file_name);
            fclose(fout);
            unlink(file_name);
            return;
        }
        left -= mi;
    }
    fclose(fout);
}

#ifndef __LEGACY__
void display_header(FILE *out, const char *name, int index, header_t *header,
                    size_t *offset, FILE *fin) {
#else
display_header(out, name, index, header, offset, fin) 
        FILE *out; 
        char *name; 
        int index; 
        header_t *header;
        size_t *offset;
        FILE *fin; 
{
#endif
    int type;
    if (!header->form) {
        return;
    }
    type = header->form & 0xF;
    fprintf(out, "INF: HDR(%d)",index);
    fprintf(out, "TYP(0x%02x", header->form);
    fprintf(out, ",%s)", g_type[header->form]);
    fprintf(out, "BAS(%04xh)", header->base);
    if (header->min)
        fprintf(out, "MN(%.1fk=%lu)", header->min / 64.0, (unsigned long)header->min * 16);
    if (header->max)
        fprintf(out, "MX(%.1fk=%lu)", header->max / 64.0, (unsigned long)header->max * 16);
    fprintf(out, "LEN(%lu)", (unsigned long)header->length*16);
    fprintf(out, "\n");
    if (type == 1 || type == 2) {
        dump(out, index, tolower(g_type[type][0]), header->base, header->length*16,
             *offset, fin);
    }
    *offset += header->length*16;
}

#ifndef __LEGACY__
void display_header_block(FILE *out, const char *name, header_block_t *block,
                          FILE *fin) {
#else
display_header_block(out, name,  block, fin) 
        FILE *out; 
        char *name; 
        header_block_t *block;
        FILE *fin; 
{
#endif
    int i;
    size_t offset = 128+256;

    for (i = 0; i < 8; i++) {
        if((block->header[i].form & 0xF) == 2) {
            offset=128;
        }
    }
    for (i = 0; i < 8; i++) {
        display_header(out, name, i, block->header + i, &offset, fin);
    }
}

#ifndef __LEGACY__
int display_file(FILE *out, const char *name, int extract) {
#else
int display_file(out, name, extract) 
        FILE *out; 
        char *name; 
        int extract; 
{
#endif
    FILE *f;
    size_t r;
    header_block_t block;

    f = fopen((char *)name, "r");
    if (!f) {
        fprintf(stderr, "ERR: Cannot open file '%s'\n", name);
        return -1;
    }
    r = fread(&block, 1, sizeof(header_block_t), f);
    if (r != sizeof(header_block_t)) {
        fclose(f);
        fprintf(stderr, "ERR: Cannot read header fully (%lu bytes)\n", r);
        return -1;
    }
    display_header_block(out, name, &block, extract ? f : 0);
    fclose(f);
    return 0;
}

#ifndef __LEGACY__
void usage() {
#else
usage() {
#endif
    fprintf(stderr, "ERR: Invalid command line\n");
    fprintf(stderr, "INF: cmdinfo file.cmd [...]\n");
    fprintf(stderr, "     - displays CMD headers\n");
    fprintf(stderr, "     cmdinfo -e file.cmd\n");
    fprintf(stderr, "     - extracts code segments (c<index>-<base>.bin)\n");
    fprintf(stderr, "     - extracts data segments (d<index>-<base>.bin)\n");
}

#ifndef __LEGACY__
int main(int argc, char **argv) {
#else
int main(argc, argv) 
        int argc; 
        char **argv; 
{
#endif
    int i;
    if (argc < 2) {
        usage();
        return -1;
    }
    if (!strcmp("-e", argv[1]) || !strcmp("-E", argv[1])) {
        if (argc < 2) {
            usage();
            return -1;
        }
        display_file(stdout, argv[2], 1);
    } else {
        for (i = 1; i < argc; i++) {
            display_file(stdout, argv[i], 0);
        }
    }
    return 0;
}
