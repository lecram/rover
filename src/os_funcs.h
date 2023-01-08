#ifndef _OS_FUNCS_H
#define _OS_FUNCS_H

#include <sys/stat.h> /* lstat() mkdir() */
#include <sys/types.h> /* mode_t */
#include <string.h>
#include <stdlib.h> /* mkdtemp() */
#include <stdio.h> /* sprintf() fopen() fclose() */
#include <fcntl.h> /* AT_REMOVEDIR */
#include <unistd.h> /* unlinkat() */
#include <errno.h>
#include <stdbool.h>
#include <sys/sendfile.h>

#define DEFAULT_CHUNK ((ssize_t)262144L) /* https://stackoverflow.com/questions/42156041/copying-a-huge-file-using-read-write-and-open-apis-in-linux */

mode_t fileinfo(const char *fname, off_t *size);
int fileexist(const char *fname);
int rm(const char *fname);
int addfile(const char *path);
int adddir(const char *path);
bool isvalidfilename(const char *filename, bool wildcard);
int cpyfile(const char *srcpath);
int movfile(const char *srcpath);
bool cd(bool reset);

#endif // _OS_FUNCS_H