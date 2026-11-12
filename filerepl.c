/* Shared output transaction. Only the caller's newly created scratch file may
 * be discarded before commit. Commit failures retain complete recovery data. */
#include "filerepl.h"
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include "filecmp.h"

#ifdef __STDC__
FILE *output_create(const char *path, const char *mode)
#else
FILE *output_create(path, mode) char *path, *mode;
#endif
{
    int fd;
    FILE *fp;
    fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0) {
        fprintf(stderr, "ERR: Cannot reserve '%s' (%d); existing files retained\n", path, errno);
        return NULL;
    }
    fp = fdopen(fd, mode);
    if (!fp) { close(fd); unlink(path); }
    return fp;
}

#ifdef __STDC__
int output_install(const char *temporary, const char *name)
#else
int output_install(temporary, name) char *temporary, *name;
#endif
{
#if (!defined(__unix__) && !defined(__APPLE__)) || defined(FILE_REPLACE_NONATOMIC)
    static char backup[1024];
    int fd, existing;
    char *p, *dot;
    existing = 0;
    fd = open(name, O_RDONLY);
    if (fd >= 0) {
        existing = 1;
        if (close(fd) < 0) goto failed;
    } else if (errno != ENOENT) goto failed;
    if (existing) {
        if (strlen(name) > sizeof(backup)-5) goto failed;
        strcpy(backup, name);
        dot = NULL;
        for (p=backup; *p; p++) {
            if (*p=='.') dot=p;
            else if (*p=='/' || *p=='\\' || *p==':') dot=NULL;
        }
        strcpy(dot ? dot : p, ".BAK");
        if (same_file(backup,name) || same_file(backup,temporary)) goto failed;
        fd = open(backup, O_RDWR | O_CREAT | O_EXCL, 0666);
        if (fd < 0) goto failed;
        if (close(fd) < 0 || unlink(backup) < 0) goto failed;
        if (rename(name,backup) < 0) goto failed;
    }
    if (rename(temporary,name) < 0) {
        if (existing && rename(backup,name) < 0)
            fprintf(stderr,"ERR: Restore failed; original retained in '%s'\n",backup);
        goto failed;
    }
    if (existing && unlink(backup) < 0) {
        fprintf(stderr,"ERR: Saved '%s'; cannot remove backup '%s'\n",name,backup);
        return -1;
    }
#else
    if (rename(temporary,name) < 0) goto failed;
#endif
    return 0;
failed:
    fprintf(stderr,"ERR: Cannot install '%s'; retain '%s' (%d)\n",name,temporary,errno);
    return -1;
}
