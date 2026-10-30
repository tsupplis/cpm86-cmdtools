/* Convert Digital Research H86 / Intel HEX input to a CP/M-86 CMD file.
 * CMD descriptors follow cmdinfo. This hosted implementation uses checked
 * input/output and deterministic padding.
 *
 * Command line:
 *   h862cmd name[.h86] name.cmd [-code[:b<hex>[:m<hex>[:x<hex>[:a<hex>]]]]
 *            [-data:...] [-extra:...] [-stack:...] [-x1:...] ... [-8080] [-nhead]
 *
 * The output name is derived from the input basename when omitted.
 *
 * Segment flags, colon-separated after the first ':', e.g. -code:b0:m20:x80:a0
 *   b<hex>  Intel HEX begin paragraph (default: 0 for CODE, FFFFh for DATA)
 *   m<hex>  minimum paragraphs in the CMD descriptor
 *   x<hex>  maximum paragraphs in the CMD descriptor (0 = no constraint)
 *   a<hex>  absolute paragraph base (0 = relocatable)
 *
 * Examples:
 *   h862cmd prog
 *   h862cmd prog.h86 prog.cmd
 *   h862cmd prog.h86 prog.cmd -code:b0:m20 -data:b1000:m10:x80
 *   h862cmd prog.h86 prog.cmd -8080 -code:m20
 *   h862cmd prog.h86 prog.cmd -nhead -code:b0:m20
 */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__) || defined(__GNUC__)
#include <unistd.h>
#define INMODE "rb"
#define OUTMODE "wb"
#else
#define INMODE "r"
#define OUTMODE "w"
#endif

#define GRPS 8
#define MAXADDR 65536L
#define HDR 128
#define REC 128
#define DESC 9
#define HEXREC 260

static int group_used[GRPS];
static long endpos[GRPS];
static long dataoff[GRPS];
static long seglen_buf[GRPS];
static unsigned char output_hdr[HDR], output_zero[REC];
static char output_tmp[1024];
static unsigned char input_rec[HEXREC];
static long lineno = 1, record_line = 1;
static const char *infile;
static unsigned long bytes_read;
static int segadj;
static int opt_base[GRPS], opt_max[GRPS];
static unsigned int baseval[GRPS], beginval[GRPS], minval[GRPS], maxval[GRPS];
static int min_set[GRPS];
static int mode8080, nohead;

static int hdigit(int c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static int white(int c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}
static int readhex(FILE *f, const char *name, long rec_line) {
    int c, d, n = 0, half = -1, sum = 0;
    for (;;) {
        c = fgetc(f);
        if (c == EOF) {
            if (ferror(f))
                fprintf(stderr, "ERR: Can't read input '%s' (%d)\n", name,
                        errno);
            return ferror(f) ? -1 : 0;
        }
        if (c == '\n') {
            lineno++;
            continue;
        }
        if (!white(c))
            break;
    }
    if (c != ':') {
        fprintf(stderr, "ERR: %s:%ld: expected ':' at start of record\n", name,
                lineno);
        return -1;
    }
    rec_line = lineno;
    record_line = rec_line;
    half = -1;
    while ((c = fgetc(f)) != EOF && c != '\n') {
        if (c == '\r')
            continue;
        if (white(c)) {
            if (half >= 0)
                goto malformed;
            continue;
        }
        d = hdigit(c);
        if (d < 0)
            goto malformed;
        if (half < 0)
            half = d;
        else {
            if (n >= HEXREC) {
                fprintf(stderr, "ERR: %s:%ld: record too long\n", name,
                        rec_line);
                return -1;
            }
            input_rec[n] = (unsigned char)(half * 16 + d);
            sum = (sum + input_rec[n]) & 255;
            n++;
            half = -1;
        }
    }
    if (ferror(f)) {
        fprintf(stderr, "ERR: Can't read input '%s' (%d)\n", name, errno);
        return -1;
    }
    if (c == '\n')
        lineno++;
    if (half >= 0)
        goto malformed;
    if (n < 5 || n != input_rec[0] + 5) {
        fprintf(stderr, "ERR: %s:%ld: record length mismatch\n", name,
                rec_line);
        return -1;
    }
    if (sum) {
        fprintf(stderr, "ERR: %s:%ld: checksum error\n", name, rec_line);
        return -1;
    }
    bytes_read += (unsigned long)input_rec[0];
    return 1;
malformed:
    fprintf(stderr, "ERR: %s:%ld: malformed hexadecimal record\n", name,
            rec_line);
    return -1;
}

static int putbytes(FILE *out, int group, long offset, const unsigned char *p,
                    int n, long line) {
    if (group < 0 || group >= GRPS || offset < 0 || offset + n > MAXADDR) {
        fprintf(
            stderr,
            "ERR: %s:%ld: data outside 64K segment (group %d, offset %04lx)\n",
            infile, line, group, offset);
        return -1;
    }
    if (fseek(out, dataoff[group] + offset, SEEK_SET) != 0 ||
        fwrite(p, 1, (size_t)n, out) != (size_t)n) {
        fprintf(stderr, "ERR: Cannot write group %d data\n", group);
        return -1;
    }
    return 0;
}

/* First scan records segment lengths; a later scan writes to final group
 * offsets. */
static int scan_input(FILE *f, int load, FILE *out) {
    int r, len, rt, group, translated, k, ended = 0;
    unsigned int addr;
    long line, sa, off, adj = 0, e;
    if (!load)
        memset(seglen_buf, 0, sizeof seglen_buf);
    while ((r = readhex(f, infile, lineno)) > 0) {
        len = input_rec[0];
        addr = ((unsigned int)input_rec[1] << 8) | input_rec[2];
        rt = input_rec[3];
        line = record_line;
        if (rt == 1) {
            if (len != 0) {
                fprintf(stderr, "ERR: %s:%ld: EOF record must be empty\n",
                        infile, line);
                return -1;
            }
            ended = 1;
            break;
        }
        if (rt == 3)
            continue;
        if (rt == 2) {
            if (len != 2) {
                fprintf(stderr,
                        "ERR: %s:%ld: segment-adjust record must contain two "
                        "bytes\n",
                        infile, line);
                return -1;
            }
            adj = ((long)input_rec[4] << 8) | input_rec[5];
            continue;
        }
        if (rt >= 0x85)
            continue; /* GENCMD ignores record types >=85h here. */
        if (rt >= 0x81 && rt <= 0x84) {
            /* 81h is GENCMD's CODE record, loaded at its raw offset.
               82h..84h are additional groups unless 8080 mode is active. */
            if (mode8080 || rt == 0x81) {
                group = 0;
                off = (long)addr;
            } else {
                translated = rt;
                if (translated == 0x83)
                    translated = 0x84;
                else if (translated == 0x84)
                    translated = 0x83;
                group = translated - 0x81;
                off = (long)addr;
            }
            if (!load) {
                group_used[group] = 1;
                e = off + len;
                if (e > seglen_buf[group])
                    seglen_buf[group] = e;
            } else if (putbytes(out, group, off, input_rec + 4, len, line) < 0)
                return -1;
            continue;
        }
        if (rt != 0)
            continue; /* GENCMD ignores other non-data record types. */
        if ((long)addr + len > MAXADDR) {
            fprintf(stderr, "ERR: %s:%ld: record crosses 64K boundary\n",
                    infile, line);
            return -1;
        }
        sa = (adj + (addr >> 4)) & 0xFFFFL;
        group = (sa >= (long)beginval[1]) ? 1 : 0;
        /* GENCMD ignores ordinary records below the CODE begin address. */
        if (sa < (long)beginval[0])
            continue;
        off = (((sa - (long)beginval[group]) << 4) + (addr & 15)) & 0xFFFFL;
        if (!load) {
            group_used[group] = 1;
            e = off + len;
            if (e > seglen_buf[group])
                seglen_buf[group] = e;
        } else {
            /* Group data is emitted separately in descriptor order. */
            if (putbytes(out, group, off, input_rec + 4, len, line) < 0)
                return -1;
        }
    }
    if (r < 0)
        return -1;
    if (!ended) {
        fprintf(stderr, "ERR: %s: missing end-of-file record\n", infile);
        return -1;
    }
    if (!load) {
        for (k = 0; k < GRPS; k++) {
            if (seglen_buf[k] > 65536L) {
                fprintf(stderr, "ERR: Segment %d exceeds address space\n", k);
                return -1;
            }
            if (seglen_buf[k] > endpos[k])
                endpos[k] = seglen_buf[k];
        }
        /* endpos is the canonical paragraph-unrounded segment size. */
    }
    return 0;
}

static int parse_hex_num(const char *s, unsigned int *v) {
    unsigned long x = 0;
    int d;
    if (!*s)
        return -1;
    while (*s) {
        d = hdigit((unsigned char)*s++);
        if (d < 0 || x > 0xFFFFUL / 16)
            return -1;
        x = x * 16 + d;
        if (x > 0xFFFFUL)
            return -1;
    }
    *v = (unsigned int)x;
    return 0;
}

/* Map a segment name (case-insensitive) to its group index 0-7, or
 * return 8 for "8080", 9 for "nzero", 10 for "nhead". */
static int segname_to_idx(const char *s) {
    static const char *names[] = {
        "code", "data", "extra", "stack", "x1", "x2", "x3", "x4",
        "8080", "nzero", "nhead"
    };
    char b[8];
    int i, n;
    for (n = 0; s[n] && n < 7; n++)
        b[n] = (char)tolower((unsigned char)s[n]);
    b[n] = 0;
    if (s[n]) return -1; /* too long */
    for (i = 0; i < 11; i++) {
        if (strcmp(b, names[i]) == 0)
            return i;
    }
    return -1;
}

/* Parse the flag string after the first ':' for a segment option.
 * Format: colon-separated letter:hex pairs, e.g. "b55:m66:x44:a77".
 * Letters: b=begin, m=min, x=max, a=base (absolute).
 * Returns 0 on success, -1 on error. */
static int parse_seg_flags(int idx, const char *flags) {
    const char *p = flags;
    char sw;
    char vbuf[8];
    int vn;
    unsigned int v;
    while (*p) {
        /* Each pair: <letter><hex>[:<letter><hex>...] */
        sw = (char)tolower((unsigned char)*p++);
        if (sw != 'a' && sw != 'b' && sw != 'm' && sw != 'x') {
            fprintf(stderr, "ERR: Unknown segment flag '%c'\n", (char)toupper((unsigned char)sw));
            return -1;
        }
        /* Collect hex digits up to next ':' or end */
        vn = 0;
        while (*p && *p != ':' && vn < 6)
            vbuf[vn++] = *p++;
        vbuf[vn] = 0;
        if (vn == 0) {
            fprintf(stderr, "ERR: Flag '%c' has no hex value\n", (char)toupper((unsigned char)sw));
            return -1;
        }
        if (parse_hex_num(vbuf, &v) < 0) {
            fprintf(stderr, "ERR: Invalid hex value '%s' for flag '%c'\n",
                    vbuf, (char)toupper((unsigned char)sw));
            return -1;
        }
        if (sw == 'a') {
            opt_base[idx] = 1;
            baseval[idx] = v;
        } else if (sw == 'b') {
            beginval[idx] = v;
        } else if (sw == 'm') {
            min_set[idx] = 1;
            minval[idx] = v;
        } else { /* x */
            maxval[idx] = v;
            opt_max[idx] = 1;
        }
        /* skip the ':' separator between pairs */
        if (*p == ':') p++;
    }
    return 0;
}

/* Parse an array of option strings, each starting with '-'.
 * opts[0..optc-1], no argv[0] slot. */
static int parse_args(int optc, char **opts) {
    int a, idx;
    const char *arg, *colon, *segpart, *flagpart;
    char segbuf[8];
    int n;

    for (a = 0; a < optc; a++) {
        arg = opts[a];

        if (arg[0] != '-') {
            fprintf(stderr, "ERR: Unexpected argument '%s'\n", arg);
            return -1;
        }

        /* Strip the leading '-' */
        arg++;

        /* Split at optional ':' */
        colon = strchr(arg, ':');
        if (colon) {
            n = (int)(colon - arg);
            flagpart = colon + 1;
        } else {
            n = (int)strlen(arg);
            flagpart = "";
        }
        if (n == 0 || n >= 8) {
            fprintf(stderr, "ERR: Unknown option '-%s'\n", opts[a] + 1);
            return -1;
        }
        /* Copy segment/flag name, lowercase it */
        {
            int k;
            for (k = 0; k < n; k++)
                segbuf[k] = (char)tolower((unsigned char)arg[k]);
            segbuf[n] = 0;
        }
        segpart = segbuf;
        idx = segname_to_idx(segpart);
        if (idx < 0) {
            fprintf(stderr, "ERR: Unknown option '-%s'\n", opts[a] + 1);
            return -1;
        }
        if (idx == 8) { /* -8080 */
            if (*flagpart) {
                fprintf(stderr, "ERR: -8080 takes no flags\n");
                return -1;
            }
            mode8080 = 1;
            continue;
        }
        if (idx == 9) { /* -nzero */
            fprintf(stderr,
                    "ERR: -nzero is unsupported: legacy behavior preserves "
                    "pre-existing target RAM, which a file converter cannot "
                    "reproduce\n");
            return -1;
        }
        if (idx == 10) { /* -nhead */
            if (*flagpart) {
                fprintf(stderr, "ERR: -nhead takes no flags\n");
                return -1;
            }
            nohead = 1;
            continue;
        }
        /* Segment option (0-7) */
        if (parse_seg_flags(idx, flagpart) < 0)
            return -1;
    }
    return 0;
}

static void usage(void) {
    fprintf(stderr,
            "INF: h862cmd name[.h86] name.cmd [-code[:b<hex>[:m<hex>[:x<hex>[:a<hex>]]]]]\n"
            "              [-data:...] [-extra:...] [-stack:...]\n"
            "              [-x1:...] [-x2:...] [-x3:...] [-x4:...]\n"
            "              [-8080] [-nhead]\n"
            "     creates/replaces name.cmd from name.h86\n"
            "     if name.cmd is omitted, the output is the input basename with .cmd\n"
            "     options are case-insensitive\n"
            "     segment flags are colon-separated pairs after the first ':'\n"
            "       b<hex>  Intel HEX begin paragraph (default: 0 CODE, FFFFh DATA)\n"
            "       m<hex>  minimum paragraphs in the CMD descriptor\n"
            "       x<hex>  maximum paragraphs (0 = no constraint)\n"
            "       a<hex>  absolute paragraph base (0 = relocatable)\n"
            "     -8080     treat all H86 segment records as CODE (8080 model)\n"
            "     -nhead    suppress the 128-byte CMD header\n"
            "     -nzero    rejected (depends on pre-existing target RAM)\n"
            "     examples:\n"
            "       h862cmd prog\n"
            "       h862cmd prog.h86 prog.cmd -code:b0:m20 -data:b1000:m10:x80\n"
            "       h862cmd prog.h86 prog.cmd -8080 -code:m20\n");
}

static void put16(unsigned char *p, unsigned long v) {
    p[0] = (unsigned char)v;
    p[1] = (unsigned char)(v >> 8);
}
static int write_all(FILE *f, const void *p, size_t n) {
    return fwrite(p, 1, n, f) == n ? 0 : -1;
}
static int output_cmd(const char *outname, FILE *in) {
    long lengths[GRPS], total, padded;
    int i, any = 0;
    FILE *f = NULL;
    char *dot = NULL;
    const char *p;
    memset(output_hdr, 0, sizeof output_hdr);
    memset(output_zero, 0, sizeof output_zero);
    for (i = 0; i < GRPS; i++) {
        lengths[i] = (endpos[i] + 15) / 16;
        if (i == 0 && lengths[i] == 0)
            lengths[i] = 1;
        if (lengths[i] > 4096) {
            fprintf(stderr, "ERR: Group %d exceeds 64K\n", i);
            return -1;
        }
        if (lengths[i] || group_used[i] || min_set[i])
            any = 1;
        output_hdr[i * DESC] =
            (group_used[i] || min_set[i] || (i == 0 && !nohead)
                 ? (unsigned char)(i + 1)
                 : 0);
        put16(output_hdr + i * DESC + 1, (unsigned long)lengths[i]);
        put16(output_hdr + i * DESC + 3, opt_base[i] ? (unsigned long)baseval[i] : 0UL);
        if (min_set[i] && minval[i])
            put16(output_hdr + i * DESC + 5, (unsigned long)minval[i]);
        else
            put16(output_hdr + i * DESC + 5, (unsigned long)lengths[i]);
        put16(output_hdr + i * DESC + 7, opt_max[i] ? (unsigned long)maxval[i] : 0UL);
    }
    if (!any) {
        fprintf(stderr, "ERR: No output segments\n");
        return -1;
    }
    if (strlen(outname) > sizeof(output_tmp) - 5) {
        fprintf(stderr, "ERR: Output name too long\n");
        return -1;
    }
    strcpy(output_tmp, outname);
    for (p = output_tmp; *p; p++) {
        if (*p == '.')
            dot = (char *)p;
        else if (*p == '/' || *p == '\\' || *p == ':')
            dot = NULL;
    }
    if (dot)
        *dot = 0;
    if (strlen(output_tmp) + 5 > sizeof(output_tmp)) {
        fprintf(stderr, "ERR: Output name too long\n");
        return -1;
    }
    strcat(output_tmp, ".$$$");
    f = fopen(output_tmp, OUTMODE);
    if (!f) {
        fprintf(stderr, "ERR: Cannot create '%s' (%d)\n", output_tmp, errno);
        return -1;
    }
    total = nohead ? 0 : HDR;
    if (!nohead && write_all(f, output_hdr, HDR) < 0)
        goto ioerr;
    for (i = 0; i < GRPS; i++) {
        long n = lengths[i] * 16, z;
        dataoff[i] = total;
        if (!output_hdr[i * DESC] || n == 0)
            continue;
        for (z = 0; z < n; z += (long)sizeof output_zero) {
            size_t k =
                (size_t)((n - z) > (long)sizeof output_zero ? sizeof output_zero
                                                            : (n - z));
            if (write_all(f, output_zero, k) < 0)
                goto ioerr;
        }
        total += n;
    }
    padded = (REC - total % REC) % REC;
    while (padded > 0) {
        size_t k = (size_t)(padded > REC ? REC : padded);
        if (write_all(f, output_zero, k) < 0)
            goto ioerr;
        padded -= (long)k;
    }
    if (fseek(in, 0L, SEEK_SET) != 0) {
        fprintf(stderr, "ERR: Cannot rewind source '%s'\n", infile);
        goto ioerr;
    }
    lineno = 1;
    bytes_read = 0;
    if (scan_input(in, 1, f) < 0) {
        fclose(f);
        f = NULL;
        unlink(output_tmp);
        return -1;
    }
    if (fclose(f)) {
        f = NULL;
        fprintf(stderr, "ERR: Cannot close '%s' (%d)\n", output_tmp, errno);
        unlink(output_tmp);
        return -1;
    }
    f = NULL;
#if defined(__unix__) || defined(__APPLE__) || defined(__GNUC__)
    if (rename(output_tmp, outname)) {
        fprintf(stderr, "ERR: Cannot replace '%s' (%d)\n", outname, errno);
        unlink(output_tmp);
        return -1;
    }
#else
    unlink(outname);
    if (rename(output_tmp, outname)) {
        fprintf(stderr, "ERR: Cannot replace '%s' (%d)\n", outname, errno);
        unlink(output_tmp);
        return -1;
    }
#endif
    fprintf(stderr, "INF: FILE(%s) BYTES(%lu) RECORDS(%ld)\n", infile,
            bytes_read, (total + (REC - total % REC) % REC) / REC);
    fprintf(stderr, "INF: OUTPUT(%s)\n", outname);
    return 0;
ioerr:
    fprintf(stderr, "ERR: Write failed for '%s' (%d)\n", output_tmp, errno);
    if (f)
        fclose(f);
    unlink(output_tmp);
    return -1;
}

/* Derive the source (.h86) name from a bare name or a name with .h86.
 * Returns 0 on success, -1 on error. */
static int derive_src(const char *arg, char *src, size_t sn) {
    char base[768], *dot = NULL, *p;
    size_t n;
    if (strlen(arg) >= sizeof base)
        return -1;
    strcpy(base, arg);
    for (p = base; *p; p++) {
        if (*p == '.')
            dot = p;
        else if (*p == '/' || *p == '\\' || *p == ':')
            dot = NULL;
    }
    if (dot) {
        /* Check extension is .h86 / .H86 */
        n = strlen(dot + 1);
        if (n != 3 ||
            tolower((unsigned char)dot[1]) != 'h' ||
            dot[2] != '8' ||
            dot[3] != '6') {
            fprintf(stderr, "ERR: Input name must have no extension or .h86\n");
            return -1;
        }
        if (strlen(arg) + 1 > sn)
            return -1;
        strcpy(src, arg);
    } else {
        if (strlen(base) + 5 >= sn)
            return -1;
        strcpy(src, base);
        strcat(src, ".h86");
        /* Also try uppercase .H86 if the lower case form does not open */
    }
    return 0;
}

/* Derive the destination (.cmd) name from the input name. */
static int derive_dst(const char *arg, char *dst, size_t dn) {
    char base[768], *dot = NULL, *p;
    if (strlen(arg) >= sizeof base)
        return -1;
    strcpy(base, arg);
    for (p = base; *p; p++) {
        if (*p == '.')
            dot = p;
        else if (*p == '/' || *p == '\\' || *p == ':')
            dot = NULL;
    }
    if (dot)
        *dot = 0; /* strip extension */
    if (strlen(base) + 5 >= dn)
        return -1;
    strcpy(dst, base);
    strcat(dst, ".cmd");
    return 0;
}

int main(int argc, char **argv) {
    /* opts[] holds only the option arguments; posv[] the positional filenames */
    char src[1024], dst[1024];
    static char *opts[64];
    int optc = 0;
    const char *posv[2];
    int posc = 0;
    FILE *f;
    int i, rc;

    if (argc < 2) {
        usage();
        return 1;
    }

    /* Separate positional arguments (no leading '-') from options */
    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            if (optc < 63) opts[optc++] = argv[i];
        } else {
            if (posc < 2) posv[posc++] = argv[i];
            else {
                fprintf(stderr, "ERR: Too many arguments\n");
                return 1;
            }
        }
    }
    opts[optc] = NULL;

    if (posc < 1) {
        usage();
        return 1;
    }

    /* First positional is the input (.h86) */
    if (derive_src(posv[0], src, sizeof src) < 0) {
        fprintf(stderr, "ERR: Filename too long or bad extension\n");
        return 1;
    }
    /* Second positional (if given) is the output; otherwise derive from input */
    if (posc >= 2) {
        if (strlen(posv[1]) + 1 > sizeof dst) {
            fprintf(stderr, "ERR: Output filename too long\n");
            return 1;
        }
        strcpy(dst, posv[1]);
    } else {
        if (derive_dst(posv[0], dst, sizeof dst) < 0) {
            fprintf(stderr, "ERR: Filename too long\n");
            return 1;
        }
    }

    infile = src;
    for (i = 0; i < GRPS; i++) {
        baseval[i] = 0;
    }
    beginval[1] = 0xFFFF; /* GENCMD default DATA begin paragraph */

    if (parse_args(optc, opts) < 0)
        return 1;

    /* Case-insensitive name collision check */
    {
        const char *a = src, *b = dst;
        int eq = 1;
        while (*a && *b) {
            if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
                eq = 0; break;
            }
            a++; b++;
        }
        if (eq && !*a && !*b) {
            fprintf(stderr, "ERR: Input and output names collide\n");
            return 1;
        }
    }

    f = fopen(src, INMODE);
    if (!f) {
        /* Try uppercase .H86 if lowercase failed */
        char src2[1024];
        size_t slen = strlen(src);
        if (slen >= 4 &&
            src[slen-4] == '.' &&
            tolower((unsigned char)src[slen-3]) == 'h' &&
            src[slen-2] == '8' && src[slen-1] == '6') {
            strcpy(src2, src);
            src2[slen-3] = 'H';
            f = fopen(src2, INMODE);
            if (f)
                infile = src2;
        }
        if (!f) {
            fprintf(stderr, "ERR: Cannot open source '%s' (%d)\n", src, errno);
            return 1;
        }
    }
    rc = scan_input(f, 0, NULL);
    if (rc == 0)
        rc = output_cmd(dst, f);
    if (fclose(f) != 0 && rc == 0) {
        fprintf(stderr, "ERR: Error closing source '%s'\n", src);
        rc = -1;
    }
    if (rc < 0)
        return 1;
    return 0;
}
