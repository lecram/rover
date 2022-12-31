#include "config.h"
#include "ui_funcs.h"

/* Handle any signals received since last call. */
void sync_signals()
{
	if (rover.pending_usr1) {
		/* SIGUSR1 received: refresh directory listing. */
		reload();
		rover.pending_usr1 = 0;
	}
	if (rover.pending_winch) {
		/* SIGWINCH received: resize application accordingly. */
		delwin(rover.window);
		endwin();
		refresh();
		clear();
		rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
		if (HEIGHT < rover.nfiles && SCROLL + HEIGHT > rover.nfiles)
			SCROLL = ESEL - HEIGHT;

		update_view();
		rover.pending_winch = 0;
	}
}

/* Comparison used to sort listing entries. */
int rowcmp(const void *a, const void *b)
{
	int isdir1, isdir2, cmpdir;
	const Row *r1 = a;
	const Row *r2 = b;
	isdir1        = S_ISDIR(r1->mode);
	isdir2        = S_ISDIR(r2->mode);
	cmpdir        = isdir2 - isdir1;
	return cmpdir ? cmpdir : strcoll(r1->name, r2->name);
}

/* Get all entries in current working directory. */
int ls(Row **rowsp, uint8_t flags)
{
	DIR *dp;
	struct dirent *ep;
	struct stat statbuf;
	Row *rows;
	int i, n;

	if (!(dp = opendir(".")))
		return -1;

	n = -2; /* We don't want the entries "." and "..". */
	while (readdir(dp))
		n++;

	if (n == 0) {
		closedir(dp);

		return 0;
	}

	rewinddir(dp);
	rows = malloc(n * sizeof(*rows));
	i    = 0;
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		if (!(flags & SHOW_HIDDEN) && ep->d_name[0] == '.')
			continue;
		lstat(ep->d_name, &statbuf);
		rows[i].islink = S_ISLNK(statbuf.st_mode);
		stat(ep->d_name, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) {
			if (flags & SHOW_DIRS) {
				rows[i].name = (char *)malloc(strlen(ep->d_name) + 2);
				strcpy(rows[i].name, ep->d_name);
				if (!rows[i].islink)
					ADDSLASH(rows[i].name);
				rows[i].mode = statbuf.st_mode;
				i++;
			}
		} else if (flags & SHOW_FILES) {
			rows[i].name = (char *)malloc(strlen(ep->d_name) + 1);
			strcpy(rows[i].name, ep->d_name);
			rows[i].size = statbuf.st_size;
			rows[i].mode = statbuf.st_mode;
			i++;
		}
	}
	n = i; /* Ignore unused space in array caused by filters. */
	qsort(rows, n, sizeof(*rows), rowcmp);
	closedir(dp);
	*rowsp = rows;

	return n;
}

void free_rows(Row **rowsp, int nfiles)
{
	int i;

	for (i = 0; i < nfiles; i++)
		FREE((*rowsp)[i].name);
	FREE(*rowsp);
}

/* Change working directory to the path in CWD. */
void cd(bool reset)
{
	int i, j;

	message(CYAN, "Loading \"%s\"...", CWD);
	refresh();
	if (chdir(CWD) == -1) {
		getcwd(CWD, PATH_MAX - 1);
		ADDSLASH(CWD);

		mvhline(LINES - 1, 0, ' ', STATUSPOS);
		update_view();

		return;
	}

	if (reset)
		ESEL = SCROLL = 0;

	if (rover.nfiles)
		free_rows(&rover.rows, rover.nfiles);

	rover.nfiles = ls(&rover.rows, FLAGS);
	if (!strcmp(CWD, rover.marks.dirpath)) {
		for (i = 0; i < rover.nfiles; i++) {
			for (j = 0; j < rover.marks.bulk; j++)
				if (rover.marks.entries[j] && !strcmp(rover.marks.entries[j], ENAME(i)))
					break;

			MARKED(i) = j < rover.marks.bulk;
		}
	} else
		for (i = 0; i < rover.nfiles; i++)
			MARKED(i) = false;

	mvhline(LINES - 1, 0, ' ', STATUSPOS);
	update_view();

	return;
}

/* Select a target entry, if it is present. */
void try_to_sel(const char *target)
{
	ESEL = 0;
	if (!ISDIR(target))
		while ((ESEL + 1) < rover.nfiles && S_ISDIR(EMODE(ESEL)))
			ESEL++;

	while ((ESEL + 1) < rover.nfiles && strcoll(ENAME(ESEL), target) < 0)
		ESEL++;
}

/* Reload CWD, but try to keep selection. */
void reload()
{
	char input[PATH_MAX];

	if (rover.nfiles) {
		strcpy(input, ENAME(ESEL));
		cd(false);
		try_to_sel(input);
		update_view();
	} else
		cd(true);
}

off_t count_dir(const char *path)
{
	DIR *dp;
	struct dirent *ep;
	struct stat statbuf;
	char subpath[PATH_MAX];
	off_t total;

	if (!(dp = opendir(path)))
		return 0;
	total = 0;
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);
		lstat(subpath, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) {
			ADDSLASH(subpath);
			total += count_dir(subpath);
		} else
			total += statbuf.st_size;
	}
	closedir(dp);
	return total;
}

off_t count_marked()
{
	int i;
	char *entry;
	off_t total;
	struct stat statbuf;

	total = 0;
	chdir(rover.marks.dirpath);
	for (i = 0; i < rover.marks.bulk; i++) {
		entry = rover.marks.entries[i];
		if (entry) {
			if (ISDIR(entry)) {
				total += count_dir(entry);
			} else {
				lstat(entry, &statbuf);
				total += statbuf.st_size;
			}
		}
	}
	chdir(CWD);
	return total;
}

void update_progress(off_t delta)
{
	int percent;

	if (!rover.prog.total)
		return;

	rover.prog.partial += delta;
	percent = (int)(rover.prog.partial * 100 / rover.prog.total);
	message(CYAN, "%s...%d%%", rover.prog.msg, percent);
	refresh();

	return;
}

int cpyfile(const char *srcpath)
{
	int src, dst, ret;
	size_t size;
	struct stat st;
	char buf[BUFSIZ], dstpath[PATH_MAX], buffer[PATH_MAX];

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
	ret = lstat(srcpath, &st);
	if (ret < 0)
		return ret;
	if (S_ISLNK(st.st_mode)) {
		ret = readlink(srcpath, buffer, PATH_MAX - 1);
		if (ret < 0)
			return ret;
		buffer[ret] = '\0';
		ret         = symlink(buffer, dstpath);
	} else {
		ret = src = open(srcpath, O_RDONLY);
		if (ret < 0)
			return ret;
		ret = dst = creat(dstpath, st.st_mode);
		if (ret < 0)
			return ret;
		while ((size = read(src, buf, BUFSIZ)) > 0) {
			write(dst, buf, size);
			update_progress(size);
			sync_signals();
		}
		close(src);
		close(dst);
		ret = 0;
	}
	return ret;
}

int movfile(const char *srcpath)
{
	int ret;
	struct stat st;
	char dstpath[PATH_MAX];

	strcpy(dstpath, CWD);
	strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
	ret = rename(srcpath, dstpath);
	if (ret == 0) {
		ret = lstat(dstpath, &st);
		if (ret < 0)
			return ret;
		update_progress(st.st_size);
	} else if (errno == EXDEV) {
		ret = cpyfile(srcpath);
		if (ret < 0)
			return ret;
		ret = unlink(srcpath);
	}
	return ret;
}
