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

#define  DEFAULT_CHUNK  262144  /* https://stackoverflow.com/questions/42156041/copying-a-huge-file-using-read-write-and-open-apis-in-linux */

mode_t fileinfo(const char *fname, off_t *size);
int rm(const char *fname);
bool isvalidfilename(const char *filename, bool wildcard);
int cpyfile(const char *srcpath);
int movfile(const char *srcpath);
bool cd(bool reset);

#endif // _OS_FUNCS_H