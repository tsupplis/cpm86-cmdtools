#ifndef FILECMP_H
#define FILECMP_H

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

#ifdef __STDC__
static int same_file(const char *a, const char *b) {
#else
static int same_file(a, b) char *a, *b; {
#endif
#if defined(__unix__) || defined(__APPLE__)
    struct stat sa, sb;
    if (stat(a, &sa) == 0 && stat(b, &sb) == 0)
        return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
    return strcmp(a, b) == 0;
#else
    int x, y, component = 1;
    do {
        if (component) {
            while (a[0] == '.' && (a[1] == '/' || a[1] == '\\')) a += 2;
            while (b[0] == '.' && (b[1] == '/' || b[1] == '\\')) b += 2;
        }
        x = *a++;
        y = *b++;
        if (x == '\\') x = '/';
        if (y == '\\') y = '/';
        if (x >= 'a' && x <= 'z') x -= 'a' - 'A';
        if (y >= 'a' && y <= 'z') y -= 'a' - 'A';
        if (x != y) return 0;
        component = x == '/' || x == ':';
    } while (x);
    return 1;
#endif
}

#endif
