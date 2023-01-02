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
    result = remove(fname);
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
