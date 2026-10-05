/*    Modify a group (segment) of a CP/M-86 .CMD file in place.
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

/*    Layout handled (plain CMD files only, no RSX, no fixups):
 *
 *      Offset  Size   Content
 *      ------  -----  -------
 *      0       128    CMD header: 8 group descriptors of 9 bytes (72 bytes),
 *                     then 56 bytes (RSX/fixup/flags) preserved untouched
 *      128     N      Group data, in descriptor order, each a multiple of 16
 *      end     Q      Padding to a 512-byte record boundary
 *
 *    Descriptor: DB type, DW length, DW base, DW min, DW max (paragraphs).
 *
 *    The file is rewritten to a temporary file (extension .$$$) that then
 *    replaces the original, so a failure never leaves a half-written CMD.
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
#define ATOMIC_RENAME
#else
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif
#else
#define SEEK_SET 0
#define BINARY_READ "r"
#define BINARY_WRITE "w"
#endif

#define HDR_SIZE 128
#define DESC_SIZE 9
#define NGROUPS 8
#define RSX_OFF 0x7B
#define FIXUP_OFF 0x7D
#define RECORD 512
#define MAXPARAS 0xFFFFL

#define GET16(p) ((unsigned)((p)[0]) | ((unsigned)((p)[1]) << 8))
#define PUT16(p, v) ((p)[0] = (unsigned char)((v) & 0xFF), \
                     (p)[1] = (unsigned char)(((v) >> 8) & 0xFF))

#define SRC_OLD 0
#define SRC_BIN 1
#define SRC_NONE 2

static unsigned char zeros[RECORD];
static unsigned char iobuf[RECORD];

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
        if (val > 0xFFFFFFL) return -1;
        s++;
    }
    return val;
}

/* Copy up to n bytes; returns bytes copied, or -1 on write error. */
#ifdef __STDC__
long copyn(FILE *fin, FILE *fout, long n) {
#else
long copyn(fin, fout, n) FILE *fin; FILE *fout; long n; {
#endif
    long left = n;
    unsigned want, got;
    while (left > 0) {
        want = left > RECORD ? RECORD : (unsigned)left;
        got = (unsigned)fread(iobuf, 1, want, fin);
        if (got == 0) break;
        if ((unsigned)fwrite(iobuf, 1, got, fout) != got) return -1;
        left -= got;
    }
    return n - left;
}

/* Write n zero bytes; returns 0, or -1 on write error. */
#ifdef __STDC__
int padn(FILE *fout, long n) {
#else
int padn(fout, n) FILE *fout; long n; {
#endif
    unsigned want;
    while (n > 0) {
        want = n > RECORD ? RECORD : (unsigned)n;
        if ((unsigned)fwrite(zeros, 1, want, fout) != want) return -1;
        n -= want;
    }
    return 0;
}

static char *tnames[] = {"code", "data", "extra", "stack", "aux1", "aux2",
                         "aux3", "aux4", "sharedcode", "shared", NULL};
static int tvalues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 9};

/* Parse a group type: 1-9, or a cmdinfo mnemonic (CODE, DATA, EXTRA, STACK,
 * AUX1-AUX4, SHARED CODE), case-insensitive, spaces and '#' ignored.
 * Returns -1 on error. */
#ifdef __STDC__
int parsetype(const char *s) {
#else
int parsetype(s) char *s; {
#endif
    char buf[16];
    int i, n = 0;
    for (; *s; s++) {
        if (*s == ' ' || *s == '#') continue;
        if (n >= (int)sizeof(buf) - 1) return -1;
        buf[n++] = (*s >= 'A' && *s <= 'Z') ? *s - 'A' + 'a' : *s;
    }
    buf[n] = 0;
    for (i = 0; tnames[i]; i++) {
        if (strcmp(buf, tnames[i]) == 0) return tvalues[i];
    }
    if (n == 1 && buf[0] >= '1' && buf[0] <= '9') return buf[0] - '0';
    return -1;
}

/*
 * 8080 model, as decided by the CP/M-86 1.1 loader (bdos.a86, function 59):
 * a file without any DATA group (type 2) is 8080 model. Then only the first
 * descriptor gets a base; other relocatable groups keep base 0 and are not
 * recorded in the base page (content would be loaded at segment 0), type 9
 * is not turned into code, and CS=DS=ES with IP=0100h so the code group
 * starts with a 100h-byte base page (an EXTRA group does not set ES). The stack
 * is not set either, so a program needs its stack inside the code group.
 */
#ifdef __STDC__
void check_8080(unsigned char nd[][DESC_SIZE]) {
#else
check_8080(nd) unsigned char nd[][DESC_SIZE]; {
#endif
    int i, has_data = 0, code = -1;
    for (i = 0; i < NGROUPS; i++) {
        if (nd[i][0] == 2) has_data = 1;
        if (nd[i][0] == 1 && code < 0) code = i;
    }
    if (has_data) return;
    fprintf(stderr, "WRN: no DATA group, 8080 model (CS=DS=ES, IP=0100h)\n");
    for (i = 1; i < NGROUPS; i++) {
        if (!nd[i][0]) continue;
        fprintf(stderr, "WRN: HDR(%d) TYPE(%02d) is ignored by the loader in 8080 model\n", i, nd[i][0]);
        if (!GET16(nd[i] + 3) && GET16(nd[i] + 1)) {
            fprintf(stderr, "WRN: HDR(%d) content would be loaded at segment 0\n", i);
        }
    }
    if (code < 0) {
        fprintf(stderr, "WRN: no CODE group in 8080 model (type 9 is not converted), CS would be 0\n");
    } else if (GET16(nd[code] + 1) < 0x10) {
        fprintf(stderr, "WRN: HDR(%d) CODE group is shorter than the 100h-byte base page of 8080 model\n", code);
    }
    if (code >= 0 && !GET16(nd[code] + 7) && GET16(nd[code] + 5) <= GET16(nd[code] + 1)) {
        fprintf(stderr, "WRN: 8080 model without room beyond the image (min=length, no max): the stack must be inside the image, or raise min with -n\n");
    }
}

#ifdef __STDC__
void usage() {
#else
usage() {
#endif
    fprintf(stderr, "ERR: Invalid command line\n");
    fprintf(stderr, "INF: cmdmod [-s file.bin] [-t type] [-b basehex] [-n minhex] [-m maxhex] file.cmd index\n");
    fprintf(stderr, "     cmdmod -d file.cmd index\n");
    fprintf(stderr, "     - modifies, creates (empty slot or new file) or deletes group <index> (0-7)\n");
    fprintf(stderr, "       of file.cmd in place; view the result with cmdinfo\n");
    fprintf(stderr, "     -s file.bin  replace the group content (padded to 16 bytes)\n");
    fprintf(stderr, "     -t type      group type 1-9 or CODE(1) DATA(2) EXTRA(3) STACK(4) AUX1-AUX4(5-8)\n");
    fprintf(stderr, "                  SHARED(9, shared code), as displayed by cmdinfo\n");
    fprintf(stderr, "                  default: unchanged, or index+1 for a new group\n");
    fprintf(stderr, "     -b basehex   base in hex paragraphs, 0=relocatable (default: unchanged, or 0)\n");
    fprintf(stderr, "     -n minhex    min size in hex bytes (default: unchanged, or length;\n");
    fprintf(stderr, "                  raised to the group length if smaller, so -n 0 resets it to the length)\n");
    fprintf(stderr, "     -m maxhex    max size in hex bytes, 0=any, clamped to 10000h=64K\n");
    fprintf(stderr, "                  (default: unchanged, or 0)\n");
    fprintf(stderr, "     -d           delete the group, later groups move down one index\n");
    fprintf(stderr, "     a new group without -s has length 0 and needs -n (e.g. a stack group)\n");
    fprintf(stderr, "     a missing file.cmd is created and then index must be 0\n");
}

#ifdef __STDC__
int main_alt(int argc, char **argv) {
#else
int main_alt(argc, argv)
        int argc;
        char **argv;
{
#endif
    FILE *fin = NULL, *fsrc = NULL, *fout = NULL;
    char *name, *srcname = NULL;
    static char tmpname[300];
    unsigned char hdr[HDR_SIZE];
    unsigned char nd[NGROUPS][DESC_SIZE];
    long ooff[NGROUPS];
    int srck[NGROUPS];
    int del = 0, have_s = 0, have_t = 0, have_b = 0, have_n = 0, have_m = 0;
    int idx, i, newslot, used = 0;
    long v, t = 0, b = 0, n = 0, m = 0, srcsize = 0, off, total, len;
    long length, min, max;
    char *action;
    char *p, *dot;
    unsigned char *d;
    unsigned rd;

    while (argc > 1 && argv[1][0] == '-') {
        /* CP/M upper-cases the command line */
        if (argv[1][1] >= 'A' && argv[1][1] <= 'Z' && !argv[1][2]) {
            argv[1][1] += 'a' - 'A';
        }
        if (strcmp(argv[1], "-d") == 0) {
            del = 1;
            argc--; argv++;
            continue;
        }
        if (argc < 3) { usage(); return 1; }
        if (strcmp(argv[1], "-s") == 0) {
            have_s = 1; srcname = argv[2];
        } else if (strcmp(argv[1], "-t") == 0) {
            t = parsetype(argv[2]);
            if (t < 1) {
                fprintf(stderr, "ERR: -t value must be 1-9 or CODE, DATA, EXTRA, STACK, AUX1-AUX4, SHARED\n");
                return 1;
            }
            have_t = 1;
        } else if (strcmp(argv[1], "-b") == 0) {
            b = parsehex(argv[2]);
            if (b < 0 || b > 0xFFFF) {
                fprintf(stderr, "ERR: -b value must be a hex number up to FFFFh\n");
                return 1;
            }
            have_b = 1;
        } else if (strcmp(argv[1], "-n") == 0 || strcmp(argv[1], "-m") == 0) {
            int isn = (argv[1][1] == 'n');
            v = parsehex(argv[2]);
            if (v < 0) {
                fprintf(stderr, "ERR: -%c value is not a valid hex number\n", argv[1][1]);
                return 1;
            }
            if (v > 0x10000L) {
                fprintf(stderr, "WRN: -%c value clamped to 10000h (64K)\n", argv[1][1]);
                v = 0x10000L;
            }
            if (isn) { n = (v + 15) / 16; have_n = 1; }
            else     { m = (v + 15) / 16; have_m = 1; }
        } else {
            fprintf(stderr, "ERR: Unknown option '%s'\n", argv[1]);
            usage();
            return 1;
        }
        argc -= 2; argv += 2;
    }

    if (argc != 3 || argv[2][0] < '0' || argv[2][0] >= '0' + NGROUPS || argv[2][1]) {
        usage();
        return 1;
    }
    name = argv[1];
    idx = argv[2][0] - '0';

    if (del && (have_s || have_t || have_b || have_n || have_m)) {
        fprintf(stderr, "ERR: -d cannot be combined with other options\n");
        return 1;
    }
    if (!del && !have_s && !have_t && !have_b && !have_n && !have_m) {
        fprintf(stderr, "ERR: Nothing to do\n");
        usage();
        return 1;
    }
    if (strlen(name) > 250) {
        fprintf(stderr, "ERR: File name too long\n");
        return 1;
    }

    /* Load the existing header, or start from an empty one */
    fin = fopen(name, BINARY_READ);
    if (!fin) {
#ifdef ATOMIC_RENAME
        /* the CP/M and DOS libraries do not set a reliable errno */
        if (errno != ENOENT) {
            fprintf(stderr, "ERR: Can't open '%s' (%d)\n", name, errno);
            return 1;
        }
#endif
        if (del) {
            fprintf(stderr, "ERR: Can't open '%s' to delete a group (%d)\n", name, errno);
            return 1;
        }
        if (idx != 0) {
            fprintf(stderr, "ERR: '%s' does not exist, index must be 0\n", name);
            return 1;
        }
        memset(hdr, 0, sizeof(hdr));
    } else {
        if (fread(hdr, 1, HDR_SIZE, fin) != HDR_SIZE) {
            fprintf(stderr, "ERR: Cannot read header fully from '%s'\n", name);
            goto fail;
        }
        if (hdr[0] < 1 || hdr[0] > 9) {
            fprintf(stderr, "ERR: '%s' is not a CMD file (first byte %d)\n", name, hdr[0]);
            goto fail;
        }
        if (GET16(hdr + RSX_OFF) || GET16(hdr + FIXUP_OFF)) {
            fprintf(stderr, "ERR: '%s' has RSX or fixup records, not supported\n", name);
            goto fail;
        }
    }

    off = HDR_SIZE;
    for (i = 0; i < NGROUPS; i++) {
        memcpy(nd[i], hdr + i * DESC_SIZE, DESC_SIZE);
        if (nd[i][0]) {
            ooff[i] = off;
            srck[i] = SRC_OLD;
            off += GET16(nd[i] + 1) * 16L;
            used++;
        } else {
            ooff[i] = -1;
            srck[i] = SRC_NONE;
        }
    }
    newslot = (nd[idx][0] == 0);

    if (del) {
        if (newslot) {
            fprintf(stderr, "ERR: Group %d is not used\n", idx);
            goto fail;
        }
        if (used == 1) {
            fprintf(stderr, "ERR: Cannot delete the only group\n");
            goto fail;
        }
        for (i = idx; i < NGROUPS - 1; i++) {
            memcpy(nd[i], nd[i + 1], DESC_SIZE);
            ooff[i] = ooff[i + 1];
            srck[i] = srck[i + 1];
        }
        memset(nd[NGROUPS - 1], 0, DESC_SIZE);
        ooff[NGROUPS - 1] = -1;
        srck[NGROUPS - 1] = SRC_NONE;
        action = "DELETED";
    } else {
        d = nd[idx];
        if (have_s) {
            fsrc = fopen(srcname, BINARY_READ);
            if (!fsrc) {
                fprintf(stderr, "ERR: Can't open input '%s' (%d)\n", srcname, errno);
                goto fail;
            }
            /* Count by reading: ftell at the end is only a 128-byte record
             * multiple on CP/M, which has no exact file length */
            srcsize = 0;
            while ((rd = (unsigned)fread(iobuf, 1, RECORD, fsrc)) > 0) {
                srcsize += rd;
            }
            if (ferror(fsrc) || fseek(fsrc, 0L, SEEK_SET) < 0) {
                fprintf(stderr, "ERR: Can't get size of input '%s' (%d)\n", srcname, errno);
                goto fail;
            }
            length = (srcsize + 15) / 16;
            if (length > MAXPARAS) {
                fprintf(stderr, "ERR: Group size would exceed 1MB\n");
                goto fail;
            }
            srck[idx] = SRC_BIN;
        } else if (newslot) {
            length = 0;
            srck[idx] = SRC_NONE;
        } else {
            length = GET16(d + 1);
        }

        if (!have_t && newslot) t = idx + 1;
        else if (!have_t)       t = d[0];
        if (!have_b) b = newslot ? 0 : GET16(d + 3);
        min = have_n ? n : (newslot ? length : GET16(d + 5));
        max = have_m ? m : (newslot ? 0 : GET16(d + 7));

        if ((have_s || have_n || newslot) && min < length) {
            if (have_n && n) {
                fprintf(stderr, "WRN: -n value < group length, raised to %ldh paras\n", length);
            }
            min = length;
        }
        if (max && max < length) {
            fprintf(stderr, "ERR: max size is smaller than the group length (%ld bytes)\n", length * 16);
            goto fail;
        }
        if ((have_s || have_n || have_m) && max && max < min) {
            fprintf(stderr, "ERR: max size is smaller than the min size (%ld bytes)\n", min * 16);
            goto fail;
        }
        if (newslot && length == 0 && min == 0) {
            fprintf(stderr, "ERR: A new empty group needs -n or -s\n");
            goto fail;
        }
        d[0] = (unsigned char)t;
        PUT16(d + 1, length);
        PUT16(d + 3, b);
        PUT16(d + 5, min);
        PUT16(d + 7, max);
        action = newslot ? "CREATED" : "UPDATED";
    }

    check_8080(nd);

    /* Build the temporary file name: replace the extension with .$$$ */
    strcpy(tmpname, name);
    dot = NULL;
    for (p = tmpname; *p; p++) {
        if (*p == '.') dot = p;
        else if (*p == '/' || *p == '\\' || *p == ':') dot = NULL;
    }
    strcpy(dot ? dot : p, ".$$$");
    if (strcmp(tmpname, name) == 0) {
        fprintf(stderr, "ERR: Can't derive a temporary name from '%s'\n", name);
        goto fail;
    }

    fout = fopen(tmpname, BINARY_WRITE);
    if (!fout) {
        fprintf(stderr, "ERR: Can't open output '%s' (%d)\n", tmpname, errno);
        goto fail;
    }
    for (i = 0; i < NGROUPS; i++) {
        memcpy(hdr + i * DESC_SIZE, nd[i], DESC_SIZE);
    }
    if (fwrite(hdr, 1, HDR_SIZE, fout) != HDR_SIZE) {
        fprintf(stderr, "ERR: Can't write header to output (%d)\n", errno);
        goto fail;
    }
    total = HDR_SIZE;
    for (i = 0; i < NGROUPS; i++) {
        long got = 0;
        if (!nd[i][0]) continue;
        len = GET16(nd[i] + 1) * 16L;
        if (srck[i] == SRC_OLD) {
            if (fseek(fin, ooff[i], SEEK_SET)) {
                fprintf(stderr, "ERR: Cannot seek to group %d (%ld)\n", i, ooff[i]);
                goto fail;
            }
            got = copyn(fin, fout, len);
        } else if (srck[i] == SRC_BIN) {
            got = copyn(fsrc, fout, srcsize);
        }
        if (got < 0 || padn(fout, len - got) < 0) {
            fprintf(stderr, "ERR: Can't write group %d to output (%d)\n", i, errno);
            goto fail;
        }
        if (srck[i] == SRC_OLD && got < len) {
            fprintf(stderr, "WRN: Group %d is truncated in '%s' (%ld bytes missing)\n",
                    i, name, len - got);
        }
        total += len;
    }
    if (padn(fout, (RECORD - (total % RECORD)) % RECORD) < 0) {
        fprintf(stderr, "ERR: Can't write padding to output (%d)\n", errno);
        goto fail;
    }
    if (fclose(fout)) {
        fout = NULL;
        fprintf(stderr, "ERR: Can't close output properly (%d)\n", errno);
        goto fail;
    }
    fout = NULL;
    if (fin) fclose(fin);
    fin = NULL;
    if (fsrc) fclose(fsrc);
    fsrc = NULL;
#ifndef ATOMIC_RENAME
    unlink(name);
#endif
    if (rename(tmpname, name)) {
        fprintf(stderr, "ERR: Can't replace '%s' with '%s' (%d)\n", name, tmpname, errno);
        goto fail;
    }
    if (del) {
        fprintf(stderr, "INF: FILE(%s) HDR(%d) %s\n", name, idx, action);
    } else {
        fprintf(stderr, "INF: FILE(%s) HDR(%d) %s TYPE(%02d) BASE(%04xh) LEN(%ld) MIN(%ld) MAX(%ld)\n",
                name, idx, action, (int)t, (unsigned)b, length * 16, min * 16, max * 16);
    }
    return 0;

fail:
    if (fout) {
        fclose(fout);
        unlink(tmpname);
    }
    if (fin) fclose(fin);
    if (fsrc) fclose(fsrc);
    return 1;
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
