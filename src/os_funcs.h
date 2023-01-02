#ifndef _OS_FUNCS_H
#define _OS_FUNCS_H

#include <sys/stat.h> /* lstat() mkdir() */
#include <sys/types.h> /* mode_t */
#include <string.h>
#include <stdlib.h> /* mkdtemp() */
#include <stdio.h> /* sprintf() fopen() fclose() */
#include <unistd.h> /* remove() */
#include <errno.h>
#include <stdbool.h>

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

#endif // _OS_FUNCS_H