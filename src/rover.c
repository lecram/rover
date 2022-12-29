#include "config.h"
#include "ui_funcs.h"

void init_marks(Marks *marks)
{
	strcpy(marks->dirpath, "");
	marks->bulk     = BULK_INIT;
	marks->nentries = 0;
	marks->entries  = (char **)calloc(marks->bulk, sizeof(*marks->entries));
}

/* Unmark all entries. */
void mark_none(Marks *marks)
{
	int i;

	strcpy(marks->dirpath, "");
	for (i = 0; i < marks->bulk && marks->nentries; i++)
		if (marks->entries[i]) {
			FREE(marks->entries[i]);
			marks->nentries--;
		}
	if (marks->bulk > BULK_THRESH) {
		/* Reset bulk to free some memory. */
		FREE(marks->entries);
		marks->bulk    = BULK_INIT;
		marks->entries = calloc(marks->bulk, sizeof *marks->entries);
	}
}

void add_mark(Marks *marks, char *dirpath, char *entry)
{
	int i;

	if (!strcmp(marks->dirpath, dirpath)) {
		/* Append mark to directory. */
		if (marks->nentries == marks->bulk) {
			/* Expand bulk to accomodate new entry. */
			int extra = marks->bulk / 2;
			marks->bulk += extra; /* bulk *= 1.5; */
			marks->entries = realloc(marks->entries,
			                         marks->bulk * sizeof *marks->entries);
			memset(&marks->entries[marks->nentries], 0,
			       extra * sizeof *marks->entries);
			i = marks->nentries;
		} else {
			/* Search for empty slot (there must be one). */
			for (i = 0; i < marks->bulk; i++)
				if (!marks->entries[i])
					break;
		}
	} else {
		/* Directory changed. Discard old marks. */
		mark_none(marks);
		strcpy(marks->dirpath, dirpath);
		i = 0;
	}
	marks->entries[i] = malloc(strlen(entry) + 1);
	strcpy(marks->entries[i], entry);
	marks->nentries++;
}

void del_mark(Marks *marks, char *entry)
{
	int i;

	if (marks->nentries > 1) {
		for (i = 0; i < marks->bulk; i++)
			if (marks->entries[i] && !strcmp(marks->entries[i], entry))
				break;
		FREE(marks->entries[i]);
		marks->nentries--;
	} else
		mark_none(marks);
}

void free_marks(Marks *marks)
{
	int i;

	for (i = 0; i < marks->bulk && marks->nentries; i++)
		if (marks->entries[i]) {
			FREE(marks->entries[i]);
			marks->nentries--;
		}
	FREE(marks->entries);
}

void handle_usr1(int sig)
{
	rover.pending_usr1 = 1;
}

void handle_winch(int sig)
{
	rover.pending_winch = 1;
}

void enable_handlers()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_usr1;
	sigaction(SIGUSR1, &sa, NULL);
	sa.sa_handler = handle_winch;
	sigaction(SIGWINCH, &sa, NULL);
}

void disable_handlers()
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);
}

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

/* This function must be used in place of getch().
   It handles signals while waiting for user input. */
int rover_getch()
{
	int ch;

	while ((ch = getch()) == ERR)
		sync_signals();

	return ch;
}

/* This function must be used in place of get_wch().
   It handles signals while waiting for user input. */
int rover_get_wch(wint_t *wch)
{
	wint_t ret;

	while ((ret = get_wch(wch)) == (wint_t)ERR)
		sync_signals();
	return ret;
}

/* Do a fork-exec to external program (e.g. $EDITOR). */
void spawn(char **args)
{
	pid_t pid;
	int status;

	setenv("RVSEL", rover.nfiles ? ENAME(ESEL) : "", 1);
	pid = fork();
	if (pid > 0) {
		/* fork() succeeded. */
		disable_handlers();
		endwin();
		waitpid(pid, &status, 0);
		enable_handlers();
		kill(getpid(), SIGWINCH);
	} else if (pid == 0) {
		/* Child process. */
		execvp(args[0], args);
	}
}

void shell_escaped_cat(char *buf, char *str, size_t n)
{
	char *p = buf + strlen(buf);
	*p++    = '\'';
	for (n--; n; n--, str++) {
		switch (*str) {
		case '\'':
			if (n < 4)
				goto done;
			strcpy(p, "'\\''");
			n -= 4;
			p += 4;
			break;
		case '\0':
			goto done;
		default:
			*p = *str;
			p++;
		}
	}
done:
	strncat(p, "'", n);
}

int open_with_env(char *program, char *path)
{
	char buffer[PATH_MAX];

	if (program) {
#ifdef RV_SHELL
		strncpy(buffer, program, PATH_MAX - 1);
		strncat(buffer, " ", PATH_MAX - strlen(program) - 1);
		shell_escaped_cat(buffer, path, PATH_MAX - strlen(program) - 2);
		spawn((char *[]){ RV_SHELL, "-c", buffer, NULL });
#else
		spawn((char *[]){ program, path, NULL });
#endif
		return 1;
	}
	return 0;
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
				rows[i].name = (char *) malloc(strlen(ep->d_name) +2);
				strcpy(rows[i].name, ep->d_name);
				if (!rows[i].islink)
					ADDSLASH(rows[i].name);
				rows[i].mode = statbuf.st_mode;
				i++;
			}
		} else if (flags & SHOW_FILES) {
			rows[i].name = (char *) malloc(strlen(ep->d_name) +1);
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
		getcwd(CWD, PATH_MAX -1);
		ADDSLASH(CWD);

		clear_message();
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
				if ( rover.marks.entries[j] && !strcmp(rover.marks.entries[j], ENAME(i)))
					break;

			MARKED(i) = j < rover.marks.bulk;
		}
	} else
		for (i = 0; i < rover.nfiles; i++)
			MARKED(i) = false;

	clear_message();
	update_view();

	return;
}

/* Select a target entry, if it is present. */
void try_to_sel(const char *target)
{
	ESEL = 0;
	if (!ISDIR(target))
		while ((ESEL +1) < rover.nfiles && S_ISDIR(EMODE(ESEL)))
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
			strcat(subpath, "/");
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

/* Recursively process a source directory using CWD as destination root.
   For each node (i.e. directory), do the following:
    1. call pre(destination);
    2. call proc() on every child leaf (i.e. files);
    3. recurse into every child node;
    4. call pos(source).
   E.g. to move directory /src/ (and all its contents) inside /dst/:
    strcpy(CWD, "/dst/");
    process_dir(adddir, movfile, deldir, "/src/"); */
int process_dir(PROCESS pre, PROCESS proc, PROCESS pos, const char *path)
{
	int ret;
	DIR *dp;
	struct dirent *ep;
	struct stat statbuf;
	char subpath[PATH_MAX];

	ret = 0;
	if (pre) {
		char dstpath[PATH_MAX];
		strcpy(dstpath, CWD);
		strcat(dstpath, path + strlen(rover.marks.dirpath));
		ret |= pre(dstpath);
	}
	if (!(dp = opendir(path)))
		return -1;
	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);
		lstat(subpath, &statbuf);
		if (S_ISDIR(statbuf.st_mode)) {
			strcat(subpath, "/");
			ret |= process_dir(pre, proc, pos, subpath);
		} else
			ret |= proc(subpath);
	}
	closedir(dp);
	if (pos)
		ret |= pos(path);
	return ret;
}

/* Process all marked entries using CWD as destination root.
   All marked entries that are directories will be recursively processed.
   See process_dir() for details on the parameters. */
void process_marked(PROCESS pre, PROCESS proc, PROCESS pos, const char *msg_doing, const char *msg_done)
{
	int i, ret;
	char *entry;
	char path[PATH_MAX];

	clear_message();
	message(CYAN, "%s...", msg_doing);
	refresh();
	rover.prog = (Prog){ 0, count_marked(), msg_doing };
	for (i = 0; i < rover.marks.bulk; i++) {
		entry = rover.marks.entries[i];
		if (entry) {
			ret = 0;
			snprintf(path, PATH_MAX, "%s%s", rover.marks.dirpath, entry);
			if (ISDIR(entry)) {
				if (!strncmp(path, CWD, strlen(path)))
					ret = -1;
				else
					ret = process_dir(pre, proc, pos, path);
			} else
				ret = proc(path);
			if (!ret) {
				del_mark(&rover.marks, entry);
				reload();
			}
		}
	}
	rover.prog.total = 0;
	reload();
	if (!rover.marks.nentries)
		message(GREEN, "%s all marked entries.", msg_done);
	else
		message(RED, "Some errors occured while %s.", msg_doing);
	RV_ALERT();
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
}

/* Wrappers for file operations. */
int delfile(const char *path)
{
	int ret;
	struct stat st;

	ret = lstat(path, &st);
	if (ret < 0)
		return ret;
	update_progress(st.st_size);
	return unlink(path);
}

int addfile(const char *path)
{
	/* Using creat(2) because mknod(2) doesn't seem to be portable. */
	int ret;

	ret = creat(path, 0644);
	if (ret < 0)
		return ret;
	return close(ret);
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

int adddir(const char *path)
{
	int ret;
	struct stat st;

	ret = stat(CWD, &st);
	if (ret < 0)
		return ret;
	return mkdir(path, st.st_mode);
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

void start_line_edit(const char *init_input)
{
	char input[PATH_MAX];

	curs_set(TRUE);
	strncpy(input, init_input, PATH_MAX);
	rover.edit.left             = mbstowcs(rover.edit.buffer, init_input, PATH_MAX);
	rover.edit.right            = PATH_MAX - 1;
	rover.edit.buffer[PATH_MAX] = L'\0';
	rover.edit_scroll           = 0;
}

/* Read input and change editing state accordingly. */
EditStat get_line_edit()
{
	wchar_t eraser, killer, wch;
	int ret, length;
	char input[PATH_MAX];

	ret = rover_get_wch((wint_t *)&wch);
	erasewchar(&eraser);
	killwchar(&killer);
	if (ret == KEY_CODE_YES) {
		if (wch == KEY_ENTER) {
			curs_set(FALSE);
			return CONFIRM;
		} else if (wch == KEY_LEFT) {
			if (EDIT_CAN_LEFT(rover.edit))
				EDIT_LEFT(rover.edit);
		} else if (wch == KEY_RIGHT) {
			if (EDIT_CAN_RIGHT(rover.edit))
				EDIT_RIGHT(rover.edit);
		} else if (wch == KEY_UP) {
			while (EDIT_CAN_LEFT(rover.edit))
				EDIT_LEFT(rover.edit);
		} else if (wch == KEY_DOWN) {
			while (EDIT_CAN_RIGHT(rover.edit))
				EDIT_RIGHT(rover.edit);
		} else if (wch == KEY_BACKSPACE) {
			if (EDIT_CAN_LEFT(rover.edit))
				EDIT_BACKSPACE(rover.edit);
		} else if (wch == KEY_DC) {
			if (EDIT_CAN_RIGHT(rover.edit))
				EDIT_DELETE(rover.edit);
		}
	} else {
		if (wch == L'\r' || wch == L'\n') {
			curs_set(FALSE);
			return CONFIRM;
		} else if (wch == L'\t') {
			curs_set(FALSE);
			return CANCEL;
		} else if (wch == eraser) {
			if (EDIT_CAN_LEFT(rover.edit))
				EDIT_BACKSPACE(rover.edit);
		} else if (wch == killer) {
			EDIT_CLEAR(rover.edit);
			clear_message();
		} else if (iswprint(wch)) {
			if (!EDIT_FULL(rover.edit))
				EDIT_INSERT(rover.edit, wch);
		}
	}
	/* Encode edit contents in input. */
	rover.edit.buffer[rover.edit.left] = L'\0';
	length                             = wcstombs(input, rover.edit.buffer, PATH_MAX);
	wcstombs(&input[length], &rover.edit.buffer[rover.edit.right + 1],
	         PATH_MAX - length);
	return CONTINUE;
}
