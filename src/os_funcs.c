#include "rover.h"
#include "os_funcs.h"
#include "ui_funcs.h"

/*
assign at param size the total size in bytes 
returns the type mode of file specified in fname
*/
mode_t fileinfo(const char *fname, off_t *size)
{
	struct stat sb;

	if (lstat(fname, &sb) == -1) {
		return 0;
	}
	if (size) //check in NULL pointer
		*size = sb.st_size;

	return sb.st_mode;
}

/*
check if file or directory is accessible
in case of error write detailed error message in log file
return 0 if accesible otherwise -1
*/
int fileexist(const char *fname)
{
	int result;
	char *msgerr;

	errno = 0;

	result = access(fname, F_OK);
	switch (errno) {
	case EACCES:
		msgerr = "The requested access would be denied to the file, or search permission is denied for one of the directories in the path prefix of pathname.";
		break;
	case EFAULT:
		msgerr = "pathname points outside your accessible address space.";
		break;
	case EINVAL:
		msgerr = "mode was incorrectly specified.";
		break;
	case EIO:
		msgerr = "An I/O error occurred.";
		break;
	case ELOOP:
		msgerr = "Too many symbolic links were encountered in resolving pathname.";
		break;
	case ENAMETOOLONG:
		msgerr = "pathname is too long.";
		break;
	case ENOENT:
		msgerr = "A component of pathname does not exist or is a dangling symbolic link.";
		break;
	case ENOMEM:
		msgerr = "Insufficient kernel memory was available.";
		break;
	case ENOTDIR:
		msgerr = "A component used as a directory in pathname is not, in fact, a directory.";
		break;
	case EROFS:
		msgerr = "Write permission was requested for a file on a read-only filesystem.";
		break;
	case ETXTBSY:
		msgerr = "Write access was requested to an executable which is being executed.";
		break;
	default:
		msgerr = "Unspecified error.";
		break;
	}
	if (errno) //error is occurred
		LOG(LOG_ERR, "Unable to access: \"%s\" errno[%d]: %s", fname, errno, msgerr); //write detailed error message to log file

	return result;
}

/*
Delete fname even is file, symbolic link or directory
On success, zero is returned.  On error, -1 is returned
*/
int rm(const char *fname)
{
	int result = -1;
	mode_t mode;
	char *msgerr;

	errno = 0;

	mode = fileinfo(fname, NULL);
	if (S_ISREG(mode) || S_ISLNK(mode))
		result = unlink(fname);
	else if (S_ISDIR(mode))
		result = rmdir(fname);
	else
		LOG(LOG_ERR, "\"%s\" is not a regular file, symbolic link or directory.", fname);

	switch (errno) {
	case EACCES:
		msgerr = "Write access to the file or directory is not allowed for the process's effective UID, or one of the directories in pathname did not allow search permission.";
		break;
	case EBUSY:
		msgerr = "The file or directory cannot be unlinked because it is being used by the system or another process that prevents its removal.";
		break;
	case EFAULT:
		msgerr = "The file or directory points outside your accessible address space.";
		break;
	case EIO:
		msgerr = "An I/O error occurred.";
		break;
	case ELOOP:
		msgerr = "Too many symbolic links were encountered in translating.";
		break;
	case ENAMETOOLONG:
		msgerr = "pathname was too long.";
		break;
	case ENOENT:
		msgerr = "A component of path does not exist or is a dangling symbolic link, or pathname is empty.";
		break;
	case ENOMEM:
		msgerr = "Insufficient kernel memory was available.";
		break;
	case ENOTDIR:
		msgerr = "A component used as a directory in pathname is not, in fact, a directory.";
		break;
	case EPERM:
		msgerr = "The system does not allow unlinking of directories, or unlinking of directories requires privileges that the calling process doesn't have.";
		break;
	case EROFS:
		msgerr = "The file or directory refers to a file on a read-only filesystem.";
		break;
	case EBADF:
		msgerr = "pathname is relative but dirfd is neither AT_FDCWD nor a valid file descriptor.";
		break;
	case EINVAL:
		msgerr = "An invalid flag value was specified in flags.";
		break;
	case EISDIR:
		msgerr = "pathname refers to a directory, and AT_REMOVEDIR was not specified in flags.";
		break;
	case ENOTEMPTY:
		msgerr = "Dirctory not empty.";
		break;
	default:
		msgerr = "Generic error not specified by OS.";
		break;
	}
	if (errno) //error is occurred
		LOG(LOG_ERR, "Error removing \"%s\" errno[%d]: %s", fname, errno, msgerr); //write detailed error message to log file

	return result;
}

/*
create an empty file with 0644 mode
return 0 on success or -1 in case of error
*/
int addfile(const char *path)
{
	int fd;

	fd = creat(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd < 0)
		return fd;

	return close(fd);
}

/*
create an empty dir with 0644 mode
return 0 on success or -1 in case of error
*/
int adddir(const char *path)
{
	int result;
	char *msgerr;

	errno = 0;

	result = mkdir(path, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	switch (errno) {
	case EACCES:
		msgerr = "The parent directory does not allow write permission to the process, or one of the directories in pathname did not allow search permission.";
		break;
	case EDQUOT:
		msgerr = "The user's quota of disk blocks or inodes on the filesystem has been exhausted.";
		break;
	case EEXIST:
		msgerr = "pathname already exists (not necessarily as a directory). This includes the case where pathname is a symbolic link, dangling or not.";
		break;
	case EFAULT:
		msgerr = "pathname points outside your accessible address space.";
		break;
	case EINVAL:
		msgerr = "The final component (basename) of the new directory's pathname is invalid (e.g., it contains characters not permitted by the underlying filesystem).";
		break;
	case ELOOP:
		msgerr = "Too many symbolic links were encountered in resolving pathname.";
		break;
	case EMLINK:
		msgerr = "The number of links to the parent directory would exceed LINK_MAX.";
		break;
	case ENAMETOOLONG:
		msgerr = "pathname was too long.";
	case ENOENT:
		msgerr = "A directory component in pathname does not exist or is a dangling symbolic link.";
		break;
	case ENOMEM:
		msgerr = "Insufficient kernel memory was available.";
		break;
	case ENOSPC:
		msgerr = "The new directory cannot be created because the user's disk quota is exhausted.";
		break;
	case ENOTDIR:
		msgerr = "A component used as a directory in pathname is not, in fact, a directory.";
		break;
	case EPERM:
		msgerr = "The filesystem containing pathname does not support the creation of directories.";
		break;
	case EROFS:
		msgerr = "pathname refers to a file on a read-only filesystem.";
		break;
	default:
		msgerr = "Generic error not specified by OS.";
		break;
	}
	if (errno) //error is occurred
		LOG(LOG_ERR, "Error creating dir \"%s\" errno[%d]: %s", path, errno, msgerr); //write detailed error message to log file

	return result;
}

/*
check if the specified name is a valid file name
return true if is valid or false if is not valid
*/
bool isvalidfilename(const char *filename, bool wildcard)
{
	char dir_template[] = "/tmp/rover-tmpdir.XXXXXX", forbidden[] = "/<>\"|:&", extmatch[] = "?*+@!", temp_file[FILENAME_MAX], *temp_dir = NULL;
	FILE *fd;
	bool result = false;

	if (strpbrk(filename, forbidden)) //check for invalid characters for filename
		return result;
	if (strlen(filename) > 255) //check if the lenght of file exceed the max acceppted by filesystem
		return result;
	if (strpbrk(filename, extmatch)) { //check if filname contains wildchars FNM_EXTMATCH
		if (wildcard)
			return result;

		return result;
	}
	temp_dir = mkdtemp(dir_template);
	if (temp_dir == NULL)
		return result;
	if (!mkdir(temp_dir, S_IRWXU | S_IRWXG | S_IRWXO)) //creating temp dir 0777
		return result;

	sprintf(temp_file, "%s/%s", temp_dir, filename);
	fd = fopen(temp_file, "w"); //try to create an empty file using "filename"
	if (fd == NULL) {
		rm(temp_dir);

		return result;
	}
	fclose(fd);

	result = S_ISREG(fileinfo(temp_file, NULL)); //check if the temp file is regular
	if (rm(temp_file))
		return false;
	if (rm(temp_dir))
		return false;

	return result; //return true if filename its valid
}

/*
returns the inode of filename
*/
static ino_t inodeof(const char *filename)
{
	struct stat sb;

	if (lstat(filename, &sb) == -1)
		LOG(LOG_INFO, "Error reading i-node number of \"%s\"", filename);

	return (ino_t)sb.st_ino;
}

/*
try to open a new file with specfied mode,
check via inode if file is the same of source,
also check iffname already exist and ask for overwriting

return file descriptor no if success, -1 in case of error or -2 if same inode
*/
static int opennew(const char *fname, mode_t mode, ino_t source_inode)
{
	int fd  = -3;
	bool ok = true;

	if (access(fname, F_OK) == 0) { //check if the target file already exists
		if (inodeof(fname) == source_inode)
			return -2;

		message(RED, "Warning: \"%s\" already exist. Overwrite (Y/n)?", FILENAME(fname);
		if (rover_getch() == 'Y')
			rm(fname);
		else
			ok = false;

		CLEAR_MESSAGE();
	}
	if (ok)
		fd = open(fname, O_CREAT | O_WRONLY | O_TRUNC, mode);

	return fd;
}

/*
copyng srcfile file to destination dir using sendfile() with better performance because of kernel
in case of EINVAL or ENOSYS it try to use classic read()/write() funcs with best chunk of memory

return total bytes copied if success, -1L in case of error, -2L if srcfile == dstfile, -3L user abort overwriting
*/
static ssize_t filecopy(const char *srcfile, const char *dstfile)
{
	int input, output;
	struct stat sb = { 0 };
	ssize_t chunk, bytesCopied, bytesTocopy = 0;
	char *msgerr, *data, *ptr, *end;
	bool trayAgain = false; /* Applications may wish to fall back to read(2)/write(2) in
								the case where sendfile() fails with EINVAL or ENOSYS. */

	input = open(srcfile, O_RDONLY); //try opening the srcfile file
	if (input == -1) {
		LOG(LOG_ERR, "File open error: %s", srcfile);

		return -1L;
	}

	errno = 0;
	if (fstat(input, &sb)) { //read the attributes of the srcfile file
		switch (errno) {
		case EACCES:
			msgerr = "Search permission is denied for one of the directories in the path prefix of srcfile file.";
			break;
		case EBADF:
			msgerr = "fd is not a valid open file descriptor.";
			break;
		case EFAULT:
			msgerr = "Bad address.";
			break;
		case ELOOP:
			msgerr = "Too many symbolic links encountered while traversing the path.";
			break;
		case ENAMETOOLONG:
			msgerr = "Source pathname is too long.";
			break;
		case ENOENT:
			msgerr = "A component of source pathname does not exist or is a dangling symbolic link.";
			break;
		case ENOMEM:
			msgerr = "Out of memory (i.e., kernel memory).";
			break;
		case ENOTDIR:
			msgerr = "A component of the path prefix of pathname is not a directory.";
			break;
		case EOVERFLOW:
			msgerr = "Source pathname refers to a file whose size, inode number, or number of blocks cannot be represented in, respectively, the types off_t, ino_t, or blkcnt_t.\nThis error can occur when, for example, an application compiled on a 32-bit platform without -D_FILE_OFFSET_BITS=64 calls stat() on a file whose size exceeds (1<<31)-1 bytes.";
			break;
		case 0:
		default:
			msgerr = NULL;
			break;
		}
		LOG(LOG_ERR, "Error reading info from \"%s\" errno[%d]: %s", srcfile, errno, msgerr);
		message(RED, "Error reading info from source file. more info in \"%s.log\"", ROVER);
		rover_getch();
		CLEAR_MESSAGE();

		return -1L;
	}

	output = opennew(dstfile, sb.st_mode, inodeof(srcfile)); //try to open the destinatiion file with same mode of srcfile
	if (output < 0) { //try to open the destinatiion file
		close(input);
		if (output == -1)
			msgerr = "Error opening destination file";
		else if (output == -2)
			msgerr = "Destination file must be in a different directory of the source file";
		else
			msgerr = NULL;

		if (msgerr) {
			LOG(LOG_ERR, "%s", msgerr);
			message(RED, "%s", msgerr);
			rover_getch();
			CLEAR_MESSAGE();
		}

		return (ssize_t)output;
	}

	errno       = 0;
	bytesTocopy = sb.st_size;
	bytesCopied = sendfile(output, input, NULL, bytesTocopy); //sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+
	switch (errno) {
	case EAGAIN:
		msgerr = "Nonblocking I/O has been selected using O_NONBLOCK and the write would block.";
		break;
	case EBADF:
		msgerr = "The input file was not opened for reading or the output file was not opened for writing.";
		break;
	case EFAULT:
		msgerr = "Bad address.";
		break;
	case EINVAL:
	case ENOSYS:
		msgerr    = "Descriptor is not valid or locked, or an mmap(2)-like operation is not available for in_fd, or count is negative.";
		trayAgain = true; /* read NOTES at https://man7.org/linux/man-pages/man2/sendfile.2.html */
		break;
	case EIO:
		msgerr = "Unspecified error while reading from in_fd.";
		break;
	case ENOMEM:
		msgerr = "Insufficient memory to read from in_fd.";
		break;
	case EOVERFLOW:
		msgerr = "count is too large, the operation would result in exceeding the maximum size of either the input file or the output file.";
		break;
	case ESPIPE:
		msgerr = "offset is not NULL but the input file is not seekable.";
		break;
	case 0:
	default:
		msgerr = NULL;
		break;
	}
	if (errno) //if an error has occurred
		LOG(LOG_ERR, "File copy error errno[%d]: %s", errno, msgerr);

	if (bytesCopied != bytesTocopy) { //verify that all bytes have been copied
		message(RED, "Error not all data was copied! Writed %ld bytes on %ld bytes", bytesCopied, bytesTocopy);
		rover_getch();
		CLEAR_MESSAGE();
	}

	if (trayAgain) {
		LOG(LOG_INFO, "%s try to copy again with read()/write() instead of sendfile().", ROVER);
		errno = 0;
		chunk = MIN(bytesTocopy, DEFAULT_CHUNK); //set better performance for copy
		data  = malloc((size_t)chunk); // Allocate temporary data buffer.
		if (data) {
			bytesCopied = 0L;
			do { // read/write loop
				bytesTocopy = read(input, data, chunk);
				if (bytesTocopy <= 0)
					break; // exit from do while loop

				ptr = data;
				end = (char *)(data + bytesTocopy);
				while (ptr < end) { //write data loop
					bytesTocopy = write(output, ptr, (size_t)(end - ptr));
					if (bytesTocopy <= 0) {
						trayAgain = false; // exit from do while loop
						break; // exit from while loop
					}
					bytesCopied += bytesTocopy;
					ptr += bytesTocopy;
				}
			} while (trayAgain);
			FREE(data);
			if (!errno)
				LOG(LOG_INFO, "%s copy with success using read()/write() instead of sendfile().", ROVER);
		}
	}
	close(input); //close the file descriptor
	close(output); //close the file descriptor
	if (errno)
		unlink(dstfile); // in case of error remove output file.

	if (sb.st_size != bytesCopied) { // verify that all bytes have been copied
		LOG(LOG_ERR, "Error not all data was copied! Writed %ld bytes on %ld bytes: errno[%d]", bytesCopied, sb.st_size, errno);
		message(RED, "Error not all data was copied! Writed %ld bytes on %ld bytes", bytesCopied, sb.st_size);
		rover_getch();
		CLEAR_MESSAGE();
	}

	return bytesCopied; //return the number of bytes copied
}

/*
copy source file in CWD using filecopy() func
returns 0 if success
*/
int cpyfile(const char *srcpath)
{
	char dstpath[PATH_MAX], buffer[PATH_MAX];
	ssize_t bytesTocopy, bytesCopied;
	int result;
	mode_t mode;

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));

	mode = fileinfo(srcpath, &bytesTocopy);
	if (!mode)
		result = -2;
	else if (S_ISLNK(mode)) {
		result = readlink(srcpath, buffer, PATH_MAX);
		if (result < 0) {
			message(RED, "Error reading symbolic link");
			rover_getch();
			CLEAR_MESSAGE();
		} else {
			buffer[result] = '\0';
			result         = symlink(buffer, dstpath);
		}
	} else if (S_ISREG(mode)) {
		bytesCopied = filecopy(srcpath, dstpath);
		result      = (int)((bytesCopied == bytesTocopy) ? false : bytesCopied);
	} else {
		message(RED, "Error: source file must be a regular file or symbolic link");
		rover_getch();
		CLEAR_MESSAGE();
		result = -4;
	}
	return result;
}

/*
move source file in CWD using rename() func
returns 0 if success
*/
int movfile(const char *srcpath)
{
	char dstpath[PATH_MAX], buffer[PATH_MAX];
	ssize_t bytesTocopy;
	int result;
	mode_t mode;

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));

	mode = fileinfo(srcpath, &bytesTocopy);
	if (!mode)
		result = -2;
	else if (S_ISLNK(mode)) {
		result = readlink(srcpath, buffer, PATH_MAX);
		if (result < 0) {
			message(RED, "Error reading symbolic link");
			rover_getch();
			CLEAR_MESSAGE();
		} else {
			buffer[result] = '\0';
			result         = symlink(buffer, dstpath);
			result         = rm(srcpath);
		}
	} else if (S_ISREG(mode)) {
		errno  = 0;
		result = rename(srcpath, dstpath);
		if (errno == EXDEV) { // see man page at https://man7.org/linux/man-pages/man2/rename.2.html
			result = (filecopy(srcpath, dstpath) != bytesTocopy);
			result = rm(srcpath);
		}
	} else {
		message(RED, "Error: source file must be a regular file or symbolic link");
		rover_getch();
		CLEAR_MESSAGE();
		result = -3;
	}

	return result;
}

/* Comparison used to sort listing entries. */
static int rowcmp(const void *a, const void *b)
{
	int isdir1, isdir2, cmpdir;
	const Row *r1 = a, *r2 = b;

	isdir1 = S_ISDIR(r1->mode);
	isdir2 = S_ISDIR(r2->mode);
	cmpdir = isdir2 - isdir1;

	return (cmpdir ? cmpdir : strcoll(r1->name, r2->name));
}

/* Get all entries in current working directory. */
static int ls(Row **rowsp, uint8_t flags)
{
	DIR *dp;
	struct dirent *ep;
	Row *rows;
	mode_t mode;
	off_t size = -1L;
	int n, i = 0;

	if (!(dp = opendir(".")))
		return -1;

	for (n = 0; (ep = readdir(dp)); n++)
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			n--; // We don't want the entries "." and "..".

	if (n == 0) {
		closedir(dp);

		return 0;
	}

	rewinddir(dp);
	rows = malloc(n * sizeof *rows);
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		if (!(flags & SHOW_HIDDEN) && ep->d_name[0] == '.')
			continue;

		mode           = fileinfo(ep->d_name, &size);
		rows[i].islink = (S_ISLNK(mode));
		if (S_ISDIR(mode)) {
			if (flags & SHOW_DIRS) {
				rows[i].name = (char *)malloc(strlen(ep->d_name) + 2);
				strcpy(rows[i].name, ep->d_name);
				if (!rows[i].islink)
					ADDSLASH(rows[i].name);
				rows[i].mode = mode;
				i++;
			}
		} else if (flags & SHOW_FILES) {
			rows[i].name = malloc(strlen(ep->d_name) + 1);
			strcpy(rows[i].name, ep->d_name);
			rows[i].size = size;
			rows[i].mode = mode;
			i++;
		}
	}
	n = i; // Ignore unused space in array caused by filters.
	qsort(rows, n, sizeof(*rows), rowcmp);
	closedir(dp);
	*rowsp = rows;

	return n;
}

/* Change working directory to the path in CWD. */
bool cd(bool reset)
{
	int i, j;

	message(CYAN, "Loading \"%s\"...", CWD);
	refresh();

	if (chdir(CWD) == -1) {
		getcwd(CWD, PATH_MAX - 1);
		ADDSLASH(CWD);
	} else {
		if (reset)
			ESEL = SCROLL = 0;
		if (rover.nfiles)
			free_rows(&rover.rows, rover.nfiles);

		rover.nfiles = ls(&rover.rows, FLAGS);
		if (rover.nfiles < 0) {
			message(RED, "Error reading current directory");
			rover_getch();
			CLEAR_MESSAGE();

			return false;
		}

		if (!strcmp(CWD, rover.marks.dirpath)) {
			for (i = 0; i < rover.nfiles; i++) {
				for (j = 0; j < rover.marks.bulk; j++) {
					if (rover.marks.entries[j] &&
					    !strcmp(rover.marks.entries[j], ENAME(i)))
						break;
				}
				MARKED(i) = j < rover.marks.bulk;
			}
		} else {
			for (i = 0; i < rover.nfiles; i++)
				MARKED(i) = false;
		}
	}
	CLEAR_MESSAGE();
	update_view();

	return true;
}
