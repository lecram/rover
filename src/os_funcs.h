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

/*
returns the file type with mask of & S_IFMT
*/
mode_t filetype(const char *filename);
/*
remove file from disk
return 0 if success or -1 in case of error
*/
int rm(const char *fname);
/*
check if the specified name is a valid file name
return true if is valid or false if is not valid
*/
bool isvalidfilename(const char *filename, bool wildcard);
/*
copy source file in CWD using filecopy() func
*/
int cpyfile(const char *srcpath);
/*
move source file in CWD using rename() or the combination of filecopy() + rm() funcs
*/
int movfile(const char *srcpath);


#endif // _OS_FUNCS_H