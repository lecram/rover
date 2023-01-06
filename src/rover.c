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

/* Recursively process a source directory using CWD as destination root.
   For each node (i.e. directory), do the following:
    1. call pre(destination);
    2. call proc() on every child leaf (i.e. files);
    3. recurse into every child node;
    4. call pos(source).
   E.g. to move directory /src/ (and all its contents) inside /dst/:
    strcpy(CWD, "/dst/");
    process_dir(adddir, movfile, rm, "/src/"); */
static int process_dir(PROCESS pre, PROCESS proc, PROCESS pos, const char *path)
{
	int result = 0;
	DIR *dp;
	struct dirent *ep;
	char subpath[PATH_MAX], dstpath[PATH_MAX];

	if (pre) {
		strcpy(dstpath, CWD);
		strcat(dstpath, path + strlen(rover.marks.dirpath));
		result |= pre(dstpath);
	}

	dp = opendir(path);
	if (dp == NULL)
		return -1;

	while ((ep = readdir(dp))) {
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;

		snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);
		if (S_ISDIR(fileinfo(subpath, NULL))) {
			ADDSLASH(subpath);
			result |= process_dir(pre, proc, pos, subpath);
		} else
			result |= proc(subpath);
	}
	closedir(dp);

	if (pos)
		result |= pos(path);

	return result;
}

/* Process all marked entries using CWD as destination root.
   All marked entries that are directories will be recursively processed.
   See process_dir() for details on the parameters. */
void process_marked(PROCESS pre, PROCESS proc, PROCESS pos, const char *msg_doing, const char *msg_done)
{
	int i, result;
	char *entry;
	char path[PATH_MAX];

	CLEAR_MESSAGE();
	message(CYAN, "%s...", msg_doing);
	refresh();
	for (i = 0; i < rover.marks.bulk; i++) {
		entry = rover.marks.entries[i];
		if (entry) {
			result = 0;
			snprintf(path, PATH_MAX, "%s%s", rover.marks.dirpath, entry);
			if (ISDIR(entry)) {
				if (!strncmp(path, CWD, strlen(path)))
					result = -1;
				else
					result = process_dir(pre, proc, pos, path);
			} else {
				result = proc(path);
			}

			if (!result) {
				del_mark(&rover.marks, entry);
				reload();
			}
		}
	}
	reload();
	if(result == -3)
		message(YELLOW, "%s aborted.", msg_doing) ;
	else if (result < 0) //rover.marks.nentries
		message(RED, "Some errors occured while %s.", msg_doing);
	else
		message(GREEN, "%s all marked entries.", msg_done);

	RV_ALERT();
}
