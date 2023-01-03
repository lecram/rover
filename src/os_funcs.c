#include "rover.h"
#include "os_funcs.h"
#include "ui_funcs.h"

mode_t filetype(const char *filename)
{
	struct stat sb;

	if (lstat(filename, &sb) == -1) {
		return false;
	}

	return (sb.st_mode & S_IFMT);
}

/* Wrappers for file operations. */
int rm(const char *fname)
{
	int result;
	char *errnomsg;

	errno  = 0;

    result = filetype(fname);	
	if (result == S_IFREG || result == S_IFLNK)
		result = unlink(fname);
	else if (result == S_IFDIR)
		result = rmdir(fname);
	else
		LOG(LOG_ERR, "\"%s\" is not a regular file, symbolic link or directory.", fname);

	switch (errno) {
	case EACCES:
		errnomsg = "Write access to the file or directory is not allowed for the process's effective UID, or one of the directories in pathname did not allow search permission.";
		break;
	case EBUSY:
		errnomsg = "The file or directory cannot be unlinked because it is being used by the system or another process that prevents its removal.";
		break;
	case EFAULT:
		errnomsg = "The file or directory points outside your accessible address space.";
		break;
	case EIO:
		errnomsg = "An I/O error occurred.";
		break;
	case ELOOP:
		errnomsg = "Too many symbolic links were encountered in translating.";
		break;
	case ENAMETOOLONG:
		errnomsg = "pathname was too long.";
		break;
	case ENOENT:
		errnomsg = "A component of path does not exist or is a dangling symbolic link, or pathname is empty.";
		break;
	case ENOMEM:
		errnomsg = "Insufficient kernel memory was available.";
		break;
	case ENOTDIR:
		errnomsg = "A component used as a directory in pathname is not, in fact, a directory.";
		break;
	case EPERM:
		errnomsg = "The system does not allow unlinking of directories, or unlinking of directories requires privileges that the calling process doesn't have.";
		break;
	case EROFS:
		errnomsg = "The file or directory refers to a file on a read-only filesystem.";
		break;
	case EBADF:
		errnomsg = "pathname is relative but dirfd is neither AT_FDCWD nor a valid file descriptor.";
		break;
	case EINVAL:
		errnomsg = "An invalid flag value was specified in flags.";
		break;
	case EISDIR:
		errnomsg = "pathname refers to a directory, and AT_REMOVEDIR was not specified in flags.";
		break;
	default:
		errnomsg = "Generic error not specified by OS.";
		break;
	}
	if (errno) //error is occurred
		LOG(LOG_ERR, "Error removing \"%s\" errno[%d]: %s", fname, errno, errnomsg); //write detailed error message to log file

	return result;
}

bool isvalidfilename(const char *filename, bool wildcard)
{
	mode_t st_mode;
	char dir_template[] = "/tmp/rover-tmpdir.XXXXXX", forbidden[] = "/<>\"|:&", extmatch[] = "?*+@!", temp_file[FILENAME_MAX], *temp_dir = NULL;
	FILE *fd;

	if (strpbrk(filename, forbidden)) //check for invalid characters for filename
		return false;
	if (strlen(filename) > 255) //check if the lenght of file exceed the max acceppted by filesystem
		return false;
	if (strpbrk(filename, extmatch)) { //check if filname contains wildchars FNM_EXTMATCH
        if(wildcard)
            return true;

        return false;
	}
	temp_dir = mkdtemp(dir_template);
	if (temp_dir == NULL)
		return false;
	if (!mkdir(temp_dir, S_IRWXU | S_IRWXG | S_IRWXO)) //creating temp dir 0777
		return false;

	sprintf(temp_file, "%s/%s", temp_dir, filename);
	fd = fopen(temp_file, "w"); //try to create an empty file using "filename"
	if (fd == NULL) {
		rm(temp_dir);

		return false;
	}
	fclose(fd);

	st_mode = filetype(temp_file); //check if the tempo file is regular
	if (rm(temp_file))
		return false;
	if (rm(temp_dir))
		return false;

	return (st_mode == S_IFREG);
}

static ino_t inodeof(const char *filename)
{
	struct stat sb;

	if (lstat(filename, &sb) == -1)
		LOG(LOG_INFO, "Error reading i-node number of \"%s\"", filename);

	return (ino_t)sb.st_ino;
}

static int opennew(const char *fname, mode_t mode, ino_t source_inode)
{
	int handle = -1L;
	bool ok = true;

	if (access(fname, F_OK) == 0) { //check if the target file already exists
		if (inodeof(fname) == source_inode)
			return -2;

		message(RED, "Warning destination file already exist. Overwrite (Y/n)?");
		if (rover_getch() == 'Y')
			rm(fname);
		else
			ok = false;

		CLEAR_MESSAGE();
	}
	if(ok)
		handle = open(fname, O_CREAT | O_WRONLY | O_TRUNC, mode);

	return handle;
}

static ssize_t filecopy(const char *source, const char *todir)
{
	int input, output;
	struct stat fileinfo = { 0 };
	ssize_t result       = -1L;
	off_t chunk, bytesCount    = 0;
	char *errnomsg, *filename, *data, *ptr, *end, destination[FILENAME_MAX];
	bool trayAgain = false; /* Applications may wish to fall back to read(2)/write(2) in
								the case where sendfile() fails with EINVAL or ENOSYS. */

	filename = strdup(source); // duplicate because of basename() modify the string
	sprintf(destination, "%s%s", todir, basename(filename)); // build complete destination path dir+fname

	input = open(source, O_RDONLY); //try opening the source file
	if (input == -1) {
		LOG(LOG_ERR, "File open error: %s", source);

		return result;
	}

	errno = 0;
	if (fstat(input, &fileinfo)) { //read the attributes of the source file
		switch (errno) {
		case EACCES:
			errnomsg = "Search permission is denied for one of the directories in the path prefix of source file.";
			break;
		case EBADF:
			errnomsg = "fd is not a valid open file descriptor.";
			break;
		case EFAULT:
			errnomsg = "Bad address.";
			break;
		case ELOOP:
			errnomsg = "Too many symbolic links encountered while traversing the path.";
			break;
		case ENAMETOOLONG:
			errnomsg = "Source pathname is too long.";
			break;
		case ENOENT:
			errnomsg = "A component of source pathname does not exist or is a dangling symbolic link.";
			break;
		case ENOMEM:
			errnomsg = "Out of memory (i.e., kernel memory).";
			break;
		case ENOTDIR:
			errnomsg = "A component of the path prefix of pathname is not a directory.";
			break;
		case EOVERFLOW:
			errnomsg = "Source pathname refers to a file whose size, inode number, or number of blocks cannot be represented in, respectively, the types off_t, ino_t, or blkcnt_t.\nThis error can occur when, for example, an application compiled on a 32-bit platform without -D_FILE_OFFSET_BITS=64 calls stat() on a file whose size exceeds (1<<31)-1 bytes.";
			break;
		case 0:
		default:
			errnomsg = NULL;
			break;
		}
		LOG(LOG_ERR, "Error reading info from \"%s\" errno[%d]: %s", source, errno, errnomsg);
		message(RED, "Error reading info from source file. more info in \"%s.log\"", ROVER);
		rover_getch();
		CLEAR_MESSAGE();

		return result;
	}

	output = opennew(destination, fileinfo.st_mode, inodeof(source)); //try to open the destinatiion file with same mode of source
	if (output < 0) { //try to open the destinatiion file
		close(input);
		errnomsg = (output == -1 ? "Error opening destination file" : "Destination file must be in a different directory of the source file");

		LOG(LOG_ERR, "%s", errnomsg);
		message(RED, "%s", errnomsg);
		rover_getch();
		CLEAR_MESSAGE();

		return result;
	}

	errno  = 0;
	bytesCount = fileinfo.st_size;
	result = sendfile(output, input, NULL, bytesCount); //sendfile will work with non-socket output (i.e. regular file) on Linux 2.6.33+

	switch (errno) {
	case EAGAIN:
		errnomsg = "Nonblocking I/O has been selected using O_NONBLOCK and the write would block.";
		break;
	case EBADF:
		errnomsg = "The input file was not opened for reading or the output file was not opened for writing.";
		break;
	case EFAULT:
		errnomsg = "Bad address.";
		break;
	case EINVAL:
	case ENOSYS:
		errnomsg = "Descriptor is not valid or locked, or an mmap(2)-like operation is not available for in_fd, or count is negative.";
		trayAgain = true; /* read NOTES at https://man7.org/linux/man-pages/man2/sendfile.2.html */
		break;
	case EIO:
		errnomsg = "Unspecified error while reading from in_fd.";
		break;
	case ENOMEM:
		errnomsg = "Insufficient memory to read from in_fd.";
		break;
	case EOVERFLOW:
		errnomsg = "count is too large, the operation would result in exceeding the maximum size of either the input file or the output file.";
		break;
	case ESPIPE:
		errnomsg = "offset is not NULL but the input file is not seekable.";
		break;
	case 0:
	default:
		errnomsg = NULL;
		break;
	}
	if (errno) //if an error has occurred
		LOG(LOG_ERR, "File copy error errno[%d]: %s", errno, errnomsg);
	
	if (result != fileinfo.st_size) { //verify that all bytes have been copied
		message(RED, "Error not all data was copied! Writed %ld bytes on %ld bytes", result, fileinfo.st_size);
		rover_getch();
		CLEAR_MESSAGE();		
	}

	if(trayAgain) {
		LOG(LOG_INFO, "%s try to copy again with read()/write() instead of sendfile().", ROVER);
		errno = 0;
		chunk = MIN(fileinfo.st_size, DEFAULT_CHUNK); //set better performance for copy
		data = malloc((size_t) chunk); /* Allocate temporary data buffer. */
		if (data) {
			do { /* Copy loop. */
				bytesCount = read(input, data, chunk);
				if (bytesCount <= 0) {
					FREE(data);
					break;
				}
				/* Write that same chunk. */
				ptr = data;
				for(end = (char *) (data + bytesCount); ptr < end; ptr += bytesCount) {
					bytesCount = write(output, ptr, (size_t) (end - ptr));
					if (bytesCount <= 0) {
						FREE(data);
						trayAgain = false; // exit from do while
						break; // exit from for
					}
				}
			} while(trayAgain);
			if(!errno)
				LOG(LOG_INFO, "%s copy with success using read()/write() instead of sendfile().", ROVER);
		}
	}
	close(input); //close the handle
	close(output); //close the handle
	if(errno)
		unlink(destination); /* Remove output file. */

	return result; //return the number of bytes copied
}

int cpyfile(const char *srcpath)
{
	char dstpath[PATH_MAX], buffer[PATH_MAX];
	ssize_t result = -1L;
	mode_t mode;

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));

	mode = filetype(srcpath);
	if (!mode)
		return (int) result;

	if (mode == S_IFLNK) {
		result = readlink(srcpath, buffer, PATH_MAX);
		if (result < 0) {
			message(RED, "Error reading symbolic link");
			rover_getch();
			CLEAR_MESSAGE();
		} else {
			buffer[result] = '\0';
			result         = symlink(buffer, dstpath);
		}
	} else if (mode == S_IFREG) {
		result = filecopy(srcpath, dstpath);
	} else {
		message(RED, "Error: source file must be a regular file or symbolic link");
		rover_getch();
		CLEAR_MESSAGE();	
	}
	
	return (int) result;
}

int movfile(const char *srcpath)
{
	char dstpath[PATH_MAX];
	ssize_t result = -1L;
	mode_t mode;

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));

	if (rename(srcpath, dstpath) == 0) {
		mode = filetype(dstpath);
		if (!mode)
			return result;
	} else if (errno == EXDEV) {
		result = filecopy(srcpath, dstpath);
		if (result < 0)
			return result;
		result = rm(srcpath);
	}

	return result;
}
