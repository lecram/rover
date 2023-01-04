#include "rover.h"
#include "ui_funcs.h"
#include "os_funcs.h"

void logfile(const char *format, ...)
{
	char filelog[PATH_MAX];
	FILE *fd;
	time_t current_time;
	struct tm *local;
	static unsigned int logcount = 0; // each session start from 0
	va_list args;

	sprintf(filelog, "%s/%s.log", rover_home_path, ROVER);
	fd = fopen(filelog, "a+"); // a+ create or append that is useful in a log file
	if (!fd) {
		message(RED, "ERROR APPEND/WRITE LOG FILE \"%s.log\"", ROVER);
		rover_getch();
		CLEAR_MESSAGE();

		return;
	}

	time(&current_time); // get time
	local = localtime(&current_time); // convert to local
	fprintf(fd, "%d/%02d/%02d ", local->tm_year + 1900, local->tm_mon + 1, local->tm_mday); // write current date yyyy/mm/dd
	fprintf(fd, "%02d:%02d:%02d ", local->tm_hour, local->tm_min, local->tm_sec); // write local time hh:mm:ss
	fprintf(fd, "[%03u] ", logcount++); // write the index log of current session

	va_start(args, format);
	vfprintf(fd, format, args); // write the info recieved from function arguments
	va_end(args);

	fclose(fd);

	return;
}

/* atexit func */
int endsession(void)
{
	handlers(false);
	if (rover.nfiles)
		free_rows(&rover.rows, rover.nfiles);
	delwin(rover.window);
/* 
	curs_set(TRUE);
	keypad(stdscr, FALSE);
	intrflush(stdscr, TRUE);
	nl();
	echo();
	nodelay(stdscr, FALSE);
	nocbreak();
 */
	clear();
	refresh();
	endwin();

	return EXIT_SUCCESS;
}

void update_view()
{
	int i, j, numsize, ishidden, marking, length, namecols, center, height;
	char buffer_one[PATH_MAX], buffer_two[PATH_MAX], *suffix;
	wchar_t wbuffer[PATH_MAX];
	off_t human_size;

	mvhline(0, 0, ' ', COLS);
	attr_on(A_BOLD, NULL);
	color_set(RVC_TABNUM, NULL);
	mvaddch(0, COLS - 2, rover.tab + '0');
	attr_off(A_BOLD, NULL);
	if (rover.marks.nentries) {
		numsize = snprintf(buffer_one, PATH_MAX, "%d", rover.marks.nentries);
		color_set(RVC_MARKS, NULL);
		mvaddstr(0, COLS - 3 - numsize, buffer_one);
	} else
		numsize = -1;
	color_set(RVC_CWD, NULL);
	mbstowcs(wbuffer, CWD, PATH_MAX);
	mvaddnwstr(0, 0, wbuffer, COLS - 4 - numsize);
	wcolor_set(rover.window, RVC_BORDER, NULL);
	wborder(rover.window, 0, 0, 0, 0, 0, 0, 0, 0);
	ESEL = MAX(MIN(ESEL, rover.nfiles - 1), 0);
	/* Selection might not be visible, due to cursor wrapping or window
       shrinking. In that case, the scroll must be moved to make it visible. */
	if (rover.nfiles > HEIGHT) {
		SCROLL = MAX(MIN(SCROLL, ESEL), ESEL - HEIGHT + 1);
		SCROLL = MIN(MAX(SCROLL, 0), rover.nfiles - HEIGHT);
	} else
		SCROLL = 0;
	marking = !strcmp(CWD, rover.marks.dirpath);
	for (i = 0, j = SCROLL; i < HEIGHT && j < rover.nfiles; i++, j++) {
		ishidden = (ENAME(j)[0] == '.');
		if (j == ESEL)
			wattr_on(rover.window, A_REVERSE, NULL);
		/* following a series of if else in order to set colors of listing */
		if (ISLINK(j))
			wcolor_set(rover.window, RVC_LINK, NULL);
		else if (ishidden)
			wcolor_set(rover.window, RVC_HIDDEN, NULL);
		else if (S_ISREG(EMODE(j))) {
			if (EMODE(j) & (S_IXUSR | S_IXGRP | S_IXOTH))
				wcolor_set(rover.window, RVC_EXEC, NULL);
			else
				wcolor_set(rover.window, RVC_REG, NULL);
		} else if (S_ISDIR(EMODE(j)))
			wcolor_set(rover.window, RVC_DIR, NULL);
		else if (S_ISCHR(EMODE(j)))
			wcolor_set(rover.window, RVC_CHR, NULL);
		else if (S_ISBLK(EMODE(j)))
			wcolor_set(rover.window, RVC_BLK, NULL);
		else if (S_ISFIFO(EMODE(j)))
			wcolor_set(rover.window, RVC_FIFO, NULL);
		else if (S_ISSOCK(EMODE(j)))
			wcolor_set(rover.window, RVC_SOCK, NULL);

		if (S_ISDIR(EMODE(j))) {
			mbstowcs(wbuffer, ENAME(j), PATH_MAX);
			if (ISLINK(j))
				wcscat(wbuffer, L"/");
		} else {
			human_size = ESIZE(j) * 10;
			length     = mbstowcs(wbuffer, ENAME(j), PATH_MAX);
			namecols   = wcswidth(wbuffer, length);

			for (suffix = "BKMGTPEZY"; human_size >= 10240; suffix++)
				human_size = (human_size + 512) / 1024;

			if (*suffix == 'B')
				swprintf(wbuffer + length, PATH_MAX - length, L"%*d %c",
				         (int)(COLS - namecols - 6),
				         (int)human_size / 10, *suffix);
			else
				swprintf(wbuffer + length, PATH_MAX - length, L"%*d.%d %c",
				         (int)(COLS - namecols - 8),
				         (int)human_size / 10, (int)human_size % 10, *suffix);
		}
		mvwhline(rover.window, i + 1, 1, ' ', COLS - 2);
		mvwaddnwstr(rover.window, i + 1, 2, wbuffer, COLS - 4);
		if (marking && MARKED(j)) {
			wcolor_set(rover.window, RVC_MARKS, NULL);
			mvwaddch(rover.window, i + 1, 1, RVS_MARK);
		} else
			mvwaddch(rover.window, i + 1, 1, ' ');

		if (j == ESEL)
			wattr_off(rover.window, A_REVERSE, NULL);
	}
	for (; i < HEIGHT; i++)
		mvwhline(rover.window, i + 1, 1, ' ', COLS - 2);

	if (rover.nfiles > HEIGHT) {
		center = (SCROLL + HEIGHT / 2) * HEIGHT / rover.nfiles;
		height = (HEIGHT - 1) * HEIGHT / rover.nfiles;
		height = height ? height : 1;
		wcolor_set(rover.window, RVC_SCROLLBAR, NULL);
		mvwvline(rover.window, center - height / 2 + 1, COLS - 1, RVS_SCROLLBAR, height);
	}
	buffer_one[0] = FLAGS & SHOW_FILES ? 'F' : ' ';
	buffer_one[1] = FLAGS & SHOW_DIRS ? 'D' : ' ';
	buffer_one[2] = FLAGS & SHOW_HIDDEN ? 'H' : ' ';

	if (!rover.nfiles)
		strcpy(buffer_two, "0/0");
	else
		snprintf(buffer_two, PATH_MAX, "%d/%d", ESEL + 1, rover.nfiles);

	snprintf(buffer_one + 3, PATH_MAX, "%12s", buffer_two);
	color_set(RVC_STATUS, NULL);
	mvaddstr(LINES - 1, STATUSPOS, buffer_one);
	wrefresh(rover.window);
}

void free_rows(Row **rowsp, int nfiles)
{
	for (int i = 0; i < nfiles; i++)
		FREE((*rowsp)[i].name);
	FREE(*rowsp);
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
void reload(void)
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

static off_t count_dir(const char *path)
{
	DIR *dp;
	struct dirent *ep;
	char subpath[PATH_MAX];
	off_t total = 0L, size = -1L;

	if (!(dp = opendir(path)))
		return total;

	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);

		if (S_ISDIR(fileinfo(subpath, &size))) {
			ADDSLASH(subpath);
			total += count_dir(subpath);
		} else
			total += size;
	}
	closedir(dp);

	return total;
}

off_t count_marked()
{
	char *entry;
	off_t total = 0L, size = -1L;

	chdir(rover.marks.dirpath);
	for (int i = 0; i < rover.marks.bulk; i++) {
		entry = rover.marks.entries[i];
		if (entry) {
			if (ISDIR(entry)) {
				total += count_dir(entry);
			} else {
				fileinfo(entry, &size);
				total += size;
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
