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
#ifdef __STDC__
#include <string.h>
#endif
#include <ctype.h>
#include <stdio.h>
#ifdef __STDC__
#if defined(__APPLE__) || defined(__gnu_linux__) || defined(__GNUC__) || defined(__clang__)
#include <unistd.h>
#include <libgen.h>
#define PACK __attribute__((packed))
#endif
#endif
#ifdef __STDC__
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

/*
The .CMD file is the general executable format used by CP/M-86 and its derivatives. As well as standalone executable files, it is also used for other files containing code under CP/M-86, such as:

System files — for example, the DOS Plus boot file DOSPLUS.SYS is in this format.
GSX-86 drivers for both CP/M and DOS.
The filetype for a standalone program is usually .CMD. Under "Speedstart CP/M", a cut-down version of CP/M-86 intended for embedded use, it is instead .STM, and the first group is obfuscated (see below).

A .CMD file begins with a 128-byte header. The first byte will always be 1-9, of which 1 is by far the most common. If a file has a .CMD extension and starts with any other byte, it is most likely a Windows NT script file rather than a CMD file in this format.
*/

/*
	DB	type	        ;1-4 for Code/Data/Extra/Stack
				        ;5-8 for Aux1/Aux2/Aux3/Aux4
				        ;9 for "pure" code (that can be shared 
				        ;between processes).
	DW	length		    ;}
	DW	base		    ;} In paragraphs
	DW	minimum size	;}
	DW	maximum size	;}
;
; The "base" of a group is 0 if a segment is relocatable. Normally it is
; nonzero only in the operating system file (DOSPLUS.SYS / PCPM.SYS / CPM.SYS).
;
*/
struct PACK _header_t {
    uint8_t form;
    uint16_t length;
    uint16_t base;
    uint16_t min;
    uint16_t max;
};

typedef struct _header_t header_t;

/*
Offset 0: 8 group descriptors [72 bytes]. Each descriptor is:

;
; The following fields are not supported in CP/M-86 v1.x, but are in the 4.x
; kernel:
;
Offset 7Bh: DW RSX index record, 0 if no RSX index
Offset 7Dh: DW 1st record with fixups, 0 if no fixups
Offset 7Fh: DB flags
	Bit 4 set: This file is an RSX, not a CMD file
	Bit 5 set: Attempt to allocate the 8087 to this program, but only if
                  there is an 8087 present.
	Bit 6 set: Attempt to allocate the 8087 to this program. Even if 
                  there isn't one in the computer, allocate an imaginary
                  resource for it.
	Bit 7 set: Do segment fixups.
*/
struct PACK _header_block_t {
    header_t header[8];
    uint8_t padding[51];
    uint16_t rsx_index_rec;
    uint16_t fixup_first_rec;
    uint8_t flags;
};

typedef struct _header_block_t header_block_t;

/*
	DB	xyH	;x=Source group, 1-8. y=Dest group, 1-8.
	DW	offset to add to source segment register
	DB	offset to add to source offset, 0-15.
*/
struct PACK _fixup_record_t {
    uint8_t source_dest_group;
    uint16_t source_segment_offset;
    uint8_t source_offset_offset;
};

typedef struct _fixup_record_t fixup_record_t;

/*
	DW	xxxx	    ;Offset of RSX from the end of the CMD header,
				    ;in the file, in 128-byte records minus 1.
				    ;0000h  indicates the RSX is linked 
				    ;       dynamically and must be loaded from 
				    ;       disc when the file is loaded.
				    ;0FFFFh indicates end of list
	DB	'RSXNAME '	;Name of the .RSX. If the RSX is linked 
				    ;dynamically, this is its filename on disc.
	DW	0,0,0		;Unused
*/
struct PACK _rsx_record_t {
    uint16_t rsx_offset;
    uint8_t rsx_name[8];
    uint16_t unused[3];
};

typedef struct _rsx_record_t rsx_record_t;


char *g_type[16] = {"NULL",      "CODE",        "DATA",      "EXTRA",
                    "STACK",     "AUX #1",      "AUX #2",    "AUX #3",
                    "AUX #4",    "SHARED CODE", "UNUSED 10", "UNUSED 11",
                    "UNUSED 12", "UNUSED 13",   "UNUSED 14", "ESCAPE CODE"};

#ifdef __STDC__
void dump(FILE *out, int index, char type, uint16_t base, uint16_t length,
          long offset, FILE *fin) {
#else
void dump(out, index, type, base, length, offset, fin) 
          FILE *out; 
          int index; 
          char type; 
          uint16_t base; 
          uint16_t length;
          long offset; 
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
    fprintf(out, "INF: Output to %s (%u bytes)\n", file_name,length);
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
    /*fprintf(stderr,"offset:%lu\n",offset);*/
    while (left > 0) {
        size_t mo;
        size_t mi = fread(buffer, 1, 128>left?left:128, fin);
        /*fprintf(stderr,"left:%lu %lu mi:%lu\n",left,128>left?left:128, mi);*/
        if (mi == 0) {
            if (ferror(fin)) {
                fprintf(stderr, "ERR: Cannot read input (%d,%lu)\n", index,
                        offset);
                fclose(fout);
                unlink(file_name);
                return;
            }
            if(left) {
                fprintf(stderr, "WRN: Full length not available (%u missing)\n",left);
            }
            break;
        }
        mo = fwrite(buffer, 1, mi>left?left:mi, fout);
        if (mo != mi) {
            fprintf(stderr, "ERR: Cannot write output fully to (%s)\n",
                    file_name);
            fclose(fout);
            unlink(file_name);
            return;
        }
        left -= mi;
    }
    fclose(fout);
}

#ifdef __STDC__
void display_header(FILE *out, const char *name, int index, header_t *header,
                    long *offset, FILE *fin) {
#else
display_header(out, name, index, header, offset, fin) 
        FILE *out; 
        char *name; 
        int index; 
        header_t *header;
        long *offset;
        FILE *fin; 
{
#endif
    int type;
    if (!header->form) {
        return;
    }
    type = header->form & 0xF;
    fprintf(out, "INF:     HDR(%d)",index);
    fprintf(out, " TYPE(%02d", header->form);
    fprintf(out, ",%s)", g_type[header->form]);
    if(header->base) {
    fprintf(out, " BASE(FIXED,%04xh)", header->base);
    } else {
        fprintf(out, " BASE(RELOC)");
    }
    if (header->min)
        fprintf(out, " MIN(%.1fk=%lu)", header->min / 64.0, (unsigned long)header->min * 16);
    if (header->max)
        fprintf(out, " MAX(%.1fk=%lu)", header->max / 64.0, (unsigned long)header->max * 16);
    fprintf(out, " LEN(%lu)", (unsigned long)header->length*16);
    fprintf(out, "\n");
    if (type == 1 || type == 2 || type == 3) {
        dump(out, index, (char)tolower(g_type[type][0]),
            header->base, header->length*16,
             *offset, fin);
    }
    *offset += header->length*16;
}

#ifdef __STDC__
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
    long offset = 128;

    fprintf(out,"INF: FILE(%s)\n",name);
    fprintf(out,"INF:   BLOCK");
    if(block->rsx_index_rec) {
        fprintf(out, " RSX(%u)", block->rsx_index_rec);
    }
    if(block->fixup_first_rec){
        fprintf(out, " FIXUP(%u)", block->fixup_first_rec);
    }
    if(block->flags&0x08) {
        fprintf(out, " FLAGS(RSX,");
    } else {
        fprintf(out, " FLAGS(CMD,");
    }
    if(block->flags&0x10) {
        fprintf(out, "P8087,");
    } else {
        fprintf(out, "N8087,");
    }
    if(block->flags&0x20) {
        fprintf(out, "F8087,");
    } else {
        fprintf(out, "N8087,");
    }
    if(block->flags&0x40) {
        fprintf(out, "FIXUP");
        offset+=block->fixup_first_rec*4;
    } else {
        fprintf(out, "NFIXUP");
    }
    fprintf(out,")\n");
    /* dump fixups if any */
   if(block->flags&0x40) {
        offset+=block->fixup_first_rec*4;
    } 
    /* dump rsx if any */
    if(block->rsx_index_rec) {
        offset+=block->rsx_index_rec*16;
    }
    for (i = 0; i < 8; i++) {
        display_header(out, name, i, block->header + i, &offset, fin);
    }
}

#ifdef __STDC__
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

#ifdef __STDC__
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
    fprintf(stderr, "     - extracts extra segments (e<index>-<base>.bin)\n");
}

#ifdef __STDC__
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
