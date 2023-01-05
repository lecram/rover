#include "rover.h"
#include "ui_funcs.h"
#include "os_funcs.h"

void handle_usr1(int sig)
{
	rover.pending_usr1 = 1;
}

void handle_winch(int sig)
{
	rover.pending_winch = 1;
}

void handlers(bool enable)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));

	if (enable) {
		sa.sa_handler = handle_usr1;
		sigaction(SIGUSR1, &sa, NULL);

		sa.sa_handler = handle_winch;
		sigaction(SIGWINCH, &sa, NULL);
	} else {
		sa.sa_handler = SIG_DFL;
		sigaction(SIGUSR1, &sa, NULL);
		sigaction(SIGWINCH, &sa, NULL);
	}
}

/* Update line input on the screen. */
static void update_input(const char *prompt, Color color, const char *input)
{
	int plen, ilen, maxlen;
	wchar_t wbuffer[PATH_MAX];

	plen   = strlen(prompt);
	ilen   = mbstowcs(NULL, input, 0);
	maxlen = STATUSPOS - plen - 2;
	if ((ilen - rover.edit_scroll) < maxlen)
		rover.edit_scroll = MAX(ilen - maxlen, 0);
	else if (rover.edit.left > (rover.edit_scroll + maxlen - 1))
		rover.edit_scroll = rover.edit.left - maxlen;
	else if (rover.edit.left < rover.edit_scroll)
		rover.edit_scroll = MAX(rover.edit.left - maxlen, 0);

	color_set(RVC_PROMPT, NULL);
	mvaddstr(LINES - 1, 0, prompt);
	color_set(color, NULL);
	mbstowcs(wbuffer, input, COLS);
	mvaddnwstr(LINES - 1, plen, &wbuffer[rover.edit_scroll], maxlen);
	mvaddch(LINES - 1, plen + MIN(ilen - rover.edit_scroll, maxlen + 1), ' ');
	color_set(DEFAULT, NULL);
	if (rover.edit_scroll)
		mvaddch(LINES - 1, plen - 1, '<');
	if (ilen > (rover.edit_scroll + maxlen))
		mvaddch(LINES - 1, plen + maxlen, '>');
	move(LINES - 1, plen + rover.edit.left - rover.edit_scroll);
}

/* Show a message on the status bar. */
void message(Color color, char *fmt, ...)
{
	int pos;
	va_list args;
	char buffer[PATH_MAX];

	va_start(args, fmt);
	vsnprintf(buffer, STATUSPOS, fmt, args);
	va_end(args);

	pos = (STATUSPOS - (int)strlen(buffer)) / 2;
	attr_on(A_BOLD, NULL);
	color_set(color, NULL);
	mvaddstr(LINES - 1, pos, buffer);
	color_set(DEFAULT, NULL);
	attr_off(A_BOLD, NULL);
}

/* Do a fork-exec to external program (e.g. $EDITOR). */
static void spawn(char **args)
{
	pid_t pid;
	int status;

	setenv("RVSEL", rover.nfiles ? ENAME(ESEL) : "", 1);
	pid = fork();
	if (pid > 0) {
		/* fork() succeeded. */
		handlers(false); //disable handlers
		endwin();
		waitpid(pid, &status, 0);
		handlers(true); //enable handlers
		kill(getpid(), SIGWINCH);
	} else if (pid == 0) {
		/* Child process. */
		execvp(args[0], args);
	}
}

static int open_with_env(char *program, char *path)
{
	char buffer[PATH_MAX];

	if (program) {
#ifdef RV_SHELL
		snprintf(buffer, PATH_MAX, "%s \'%s\'", program, path);
		spawn((char *[]){ RV_SHELL, "-c", buffer, NULL });
#else
		spawn((char *[]){ program, path, NULL });
#endif
		return 1;
	}

	return 0;
}

static void start_line_edit(const char *init_input)
{
	char input[PATH_MAX];

	curs_set(TRUE);
	strncpy(input, init_input, PATH_MAX);
	rover.edit.left             = mbstowcs(rover.edit.buffer, init_input, PATH_MAX);
	rover.edit.right            = PATH_MAX - 1;
	rover.edit.buffer[PATH_MAX] = L'\0'; //buffuer lenght is defined with PATH_MAX +1
	rover.edit_scroll           = 0;
}

/* Handle any signals received since last call. */
static void sync_signals(void)
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
		rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
		if (HEIGHT < rover.nfiles &&
		    SCROLL + HEIGHT > rover.nfiles)
			SCROLL = ESEL - HEIGHT;

		update_view();
		rover.pending_winch = 0;
	}
}

/* This function must be used in place of getch().
   It handles signals while waiting for user input. */
int rover_getch(void)
{
	int ch;

	while ((ch = getch()) == ERR)
		sync_signals();

	return ch;
}

/* Read input and change editing state accordingly. */
static EditStat get_line_edit(char *string)
{
	int ch, length;
	ch = rover_getch();
	switch (ch) {
	case KEY_ENTER:
	case KEY_RETURN:	
		curs_set(FALSE);
		return CONFIRM;
		break;
	case KEY_ESC:
		if(strlen(string)) {
			EDIT_CLEAR(rover.edit);
			CLEAR_MESSAGE();
		} else {
			curs_set(FALSE);
			return CANCEL;
		}
		break;
	case KEY_TAB:
		EDIT_CLEAR(rover.edit);
		CLEAR_MESSAGE();
		break;
	case KEY_LEFT:
		if (EDIT_CAN_LEFT(rover.edit))
			EDIT_LEFT(rover.edit);
		break;
	case KEY_RIGHT:
		if (EDIT_CAN_RIGHT(rover.edit))
			EDIT_RIGHT(rover.edit);
		break;
	case KEY_HOME:
	case KEY_PPAGE:
	case KEY_UP:
		while (EDIT_CAN_LEFT(rover.edit))
			EDIT_LEFT(rover.edit);
		break;
	case KEY_END:
	case KEY_NPAGE:
	case KEY_DOWN:
		while (EDIT_CAN_RIGHT(rover.edit))
			EDIT_RIGHT(rover.edit);
		break;
	case KEY_BACKSPACE:
		if (EDIT_CAN_LEFT(rover.edit))
			EDIT_BACKSPACE(rover.edit);
		break;
	case KEY_DC:
		if (EDIT_CAN_RIGHT(rover.edit))
			EDIT_DELETE(rover.edit);
		break;
	case KEY_CANCEL:		
		break;
	case KEY_F(1)... KEY_F(12):
	case KEY_IC:
		break;
	default:
		if (iswprint(ch) && !EDIT_FULL(rover.edit))
			EDIT_INSERT(rover.edit, ch);
		break;
	}

	//Encode edit contents in input.
	rover.edit.buffer[rover.edit.left] = L'\0';
	length                             = wcstombs(string, rover.edit.buffer, PATH_MAX);
	wcstombs(&string[length], &rover.edit.buffer[rover.edit.right + 1], PATH_MAX - length);

	return CONTINUE;
}

static int addfile(const char *path)
{ /* Using creat(2) because mknod(2) doesn't seem to be portable. */
	int ret;

	ret = creat(path, 0644);
	if (ret < 0)
		return ret;

	return close(ret);
}

static int adddir(const char *path)
{
	mode_t mode;

	mode = fileinfo(CWD, NULL);
	if (!mode)
		return -1;

	return mkdir(path, mode);
}

/* Unmark all entries. */
static void mark_none(Marks *marks)
{
	int i;

	strcpy(marks->dirpath, "");
	for (i = 0; marks->nentries && i < marks->bulk; i++)
		if (marks->entries[i]) {
			FREE(marks->entries[i]);
			marks->nentries--;
		}

	if (marks->bulk > BULK_THRESH) {
		/* Reset bulk to free some memory. */
		FREE(marks->entries);
		marks->bulk    = BULK_INIT;
		marks->entries = (char **)calloc(marks->bulk, sizeof *marks->entries);
	}
}

static void add_mark(Marks *marks, char *dirpath, char *entry)
{
	int i, extra;

	if (!strcmp(marks->dirpath, dirpath)) {
		/* Append mark to directory. */
		if (marks->nentries == marks->bulk) {
			/* Expand bulk to accomodate new entry. */
			extra = marks->bulk / 2;
			marks->bulk += extra; /* bulk *= 1.5; */
			marks->entries = (char **)realloc(marks->entries, marks->bulk * sizeof(*marks->entries));
			memset(&marks->entries[marks->nentries], 0, extra * sizeof *marks->entries);
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

	marks->entries[i] = (char *)malloc(strlen(entry) + 1);
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
	int ret;
	DIR *dp;
	struct dirent *ep;
	char subpath[PATH_MAX], dstpath[PATH_MAX];

	ret = 0;
	if (pre) {
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
		if (S_ISDIR(fileinfo(subpath, NULL))) {
			ADDSLASH(subpath);
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
static void process_marked(PROCESS pre, PROCESS proc, PROCESS pos, const char *msg_doing, const char *msg_done)
{
	int i, ret;
	char *entry;
	char path[PATH_MAX];

	CLEAR_MESSAGE();
	message(CYAN, "%s...", msg_doing);
	refresh();
	rover.prog = (Prog){ 0, count_marked(), msg_doing }; // init progress
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
	rover.prog.total = 0; // reset progress
	reload();
	if (!rover.marks.nentries)
		message(GREEN, "%s all marked entries.", msg_done);
	else
		message(RED, "Some errors occured while %s.", msg_doing);
	RV_ALERT();
}

void main_menu(void)
{
	int i, ch, oldsel, oldscroll, length, ok, sel;
	bool quit = false, isdir;
	char buffer[PATH_MAX], input[PATH_MAX], clipboard[PATH_MAX];
	char *program, *last;
	const char *clip_path;
	ssize_t link;
	Color color = RED;
	FILE *clip_file;
	EditStat edit_stat;
	struct User prg;
	mode_t mode;

	ROVER_ENV(prg.Shell, SHELL);
	ROVER_ENV(prg.Pager, PAGER);
	ROVER_ENV(prg.Editor, VISUAL);
	if (!(prg.Editor))
		ROVER_ENV(prg.Editor, EDITOR);
	ROVER_ENV(prg.Open, OPEN);

	do {
		ch = rover_getch();
		CLEAR_MESSAGE();
		switch (ch) {
		case KEY_ESC: //Quit Rover
			message(RED, "Press any key to exit or ESC to back");
			if (rover_getch() != KEY_ESC)
				quit = true;
			CLEAR_MESSAGE();
			break;
		case '0' ... '9': //Change Tab
			rover.tab = ch - '0';
			cd(false);
			break;
		case '?': //Help
		case KEY_F(1):
			sprintf(buffer, "%s/%s.1", rover_home_path, ROVER); // local manfile
			spawn((char *[]){ "man", access(buffer, R_OK) ? ROVER : buffer, NULL });
			break;
		case KEY_DOWN: //Move cursor down
			if (!rover.nfiles)
				continue;
			ESEL = MIN(ESEL + 1, rover.nfiles - 1);
			update_view();
			break;
		case KEY_UP: //Move cursor up
			if (!rover.nfiles)
				continue;
			ESEL = MAX(ESEL - 1, 0);
			update_view();
			break;
		case KEY_NPAGE: //Move cursordown 10 lines
			if (!rover.nfiles)
				continue;
			ESEL = MIN(ESEL + RV_JUMP, rover.nfiles - 1);
			if (rover.nfiles > HEIGHT)
				SCROLL = MIN(SCROLL + RV_JUMP, rover.nfiles - HEIGHT);
			update_view();
			break;
		case KEY_PPAGE: //Move cursor up 10 lines
			if (!rover.nfiles)
				continue;
			ESEL   = MAX(ESEL - RV_JUMP, 0);
			SCROLL = MAX(SCROLL - RV_JUMP, 0);
			update_view();
			break;
		case KEY_HOME: //Move cursor to top of listing
			if (!rover.nfiles)
				continue;
			ESEL = 0;
			update_view();
			break;
		case KEY_END: //Move cursor to bottom of listing
			if (!rover.nfiles)
				continue;
			ESEL = rover.nfiles - 1;
			update_view();
			break;
		case KEY_RIGHT: //Enter selected directory
			if (!rover.nfiles || !S_ISDIR(EMODE(ESEL)))
				continue;
			if (chdir(ENAME(ESEL)) == -1) {
				message(RED, "Cannot access \"%s\".", ENAME(ESEL));
				continue;
			}
			strcat(CWD, ENAME(ESEL));
			cd(true);
			break;
		case KEY_LEFT: //Go to parent directory
			if (!strcmp(CWD, "/"))
				continue;
			DELSLASH(CWD);
			strcpy(buffer, strrchr(CWD, '/') + 1); //copy the cwd for try_to_sel
			ADDSLASH(buffer);
			dirname(CWD);
			ADDSLASH(CWD);
			cd(true);
			try_to_sel(buffer);
			if (rover.nfiles > HEIGHT)
				SCROLL = ESEL - HEIGHT / 2;
			update_view();
			break;
		case '/': //Go to home directory
			strcpy(CWD, getenv("HOME"));
			ADDSLASH(CWD);
			cd(true);
			break;
		case 'l': //Go to the target of the selected link
			link = readlink(ENAME(ESEL), buffer, PATH_MAX - 1);
			if (link == -1)
				continue;
			buffer[link] = '\0'; // needed because of readlink return not null-terminated string
			if (fileexist(buffer) != 0) {
				message(RED, "File doesn't exist, more detail in %s.log", ROVER);
				rover_getch();
				CLEAR_MESSAGE();
				continue;
			}
			mode = fileinfo(buffer, NULL);
			strcpy(CWD, buffer);		
			if (S_ISREG(mode)) {
				strcpy(CWD, buffer);
				strcpy(buffer, strrchr(CWD, '/') +1 );
				dirname(CWD);
				ADDSLASH(CWD);
			}
			cd(true);
			if(S_ISREG(mode))
				try_to_sel(buffer);
			if (rover.nfiles > HEIGHT)
				SCROLL = ESEL - HEIGHT / 2;
			update_view();
			break;
		case KEY_CTRL_C: //Copy location to clipboard
			clip_path = getenv("CLIP");
			if (clip_path) {
				clip_file = fopen(clip_path, "w");
				if (clip_file) {
					fprintf(clip_file, "%s%s\n", CWD, ENAME(ESEL));
					fclose(clip_file);
				}
			}
			if (!clip_path || !clip_file)
				sprintf(clipboard, "%s%s", CWD, ENAME(ESEL));
			break;
		case KEY_CTRL_V: //Go to location in clipboard
			clip_path = getenv("CLIP");
			if (clip_path) {
				clip_file = fopen(clip_path, "r");
				if (clip_file) {
					fscanf(clip_file, "%s\n", clipboard);
					fclose(clip_file);
				}
			}
			strcpy(buffer, clipboard);
			strcpy(CWD, dirname(buffer));
			ADDSLASH(CWD);
			cd(true);
			strcpy(buffer, clipboard);
			try_to_sel(strstr(clipboard, basename(buffer)));
			update_view();
			break;
		case KEY_F(5): //Refresh directory listing
			reload();
			break;
		case 't': //Open SHELL on the current directory
			program = prg.Shell;
			if (program) {
#ifdef RV_SHELL
				spawn((char *[]){ RV_SHELL, "-c", program, NULL });
#else
				spawn((char *[]){ program, NULL });
#endif
				reload();
			}
			break;
		case KEY_F(6): //Open PAGER with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Pager, ENAME(ESEL)))
				cd(false);
			break;
		case KEY_F(7): //Open VISUAL or EDITOR with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Editor, ENAME(ESEL)))
				cd(false);
			break;
		case KEY_F(8): //Open OPEN with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Open, ENAME(ESEL)))
				cd(false);
			break;
		case KEY_F(3): //Start incremental search
			if (!rover.nfiles)
				continue;
			oldsel    = ESEL;
			oldscroll = SCROLL;
			start_line_edit("");
			update_input(RVP_SEARCH, RED, input);
			while ((edit_stat = get_line_edit(input)) == CONTINUE) {
				length = strlen(input);
				if (length) {
					for (sel = 0; sel < rover.nfiles; sel++)
						if (!strncmp(ENAME(sel), input, length))
							break;
					if (sel < rover.nfiles) {
						color = GREEN;
						ESEL  = sel;
						if (rover.nfiles > HEIGHT) {
							if (sel < 3)
								SCROLL = 0;
							else if (sel - 3 > rover.nfiles - HEIGHT)
								SCROLL = rover.nfiles - HEIGHT;
							else
								SCROLL = sel - 3;
						}
					}
				} else {
					ESEL   = oldsel;
					SCROLL = oldscroll;
				}
				update_view();
				update_input(RVP_SEARCH, color, input);
			}
			if (edit_stat == CANCEL) {
				ESEL   = oldsel;
				SCROLL = oldscroll;
			}
			CLEAR_MESSAGE();
			update_view();
			break;
		case 'F': //Toggle file listing
			FLAGS ^= SHOW_FILES;
			reload();
			break;
		case 'D': //Toggle dir listing
			FLAGS ^= SHOW_DIRS;
			reload();
			break;
		case 'H': //Toggle hide listing
			FLAGS ^= SHOW_HIDDEN;
			reload();
			break;
		case KEY_F(9): //Create new file
			start_line_edit("");
			DELSLASH(input);
			update_input(RVP_NEW_FILE, YELLOW, input);
			do {
				edit_stat = get_line_edit(input);
				ok        = strlen(input);
				for (i = 0; ok && i < rover.nfiles; i++) {
					if (!strcmp(ENAME(i), input)) {
						ok = 0;
						update_input(RVP_NEW_FILE, RED, input);
						if (edit_stat == CONFIRM) {
							CLEAR_MESSAGE();
							message(RED, "\"%s\" already exists!", input);
							rover_getch();
							CLEAR_MESSAGE();
							edit_stat = CONTINUE;
						}
					} else if (!isvalidfilename(input, false)) {
						ok = 0;
						update_input(RVP_NEW_FILE, RED, input);
						if (edit_stat == CONFIRM) {
							CLEAR_MESSAGE();
							message(RED, "\"%s\" invalid filename!", input);
							rover_getch();
							CLEAR_MESSAGE();
							edit_stat = CONTINUE;
						}
					}
				}
				update_input(RVP_NEW_FILE, (ok ? GREEN : RED), input);
			} while (edit_stat == CONTINUE);
			CLEAR_MESSAGE();

			if (ok && edit_stat == CONFIRM) {
				if (addfile(input) == 0) {
					cd(true);
					try_to_sel(input);
					update_view();
				} else {
					message(RED, "Could not create \"%s\".", input);
					rover_getch();
					CLEAR_MESSAGE();
				}
			}
			break;
		case KEY_F(10): //Create new dir
			start_line_edit("");
			DELSLASH(input);
			update_input(RVP_NEW_DIR, YELLOW, input);
			do {
				edit_stat = get_line_edit(input);
				ok        = strlen(input);
				for (i = 0; i < rover.nfiles; i++) {
					if (!strncmp(ENAME(i), input, length)) {
						ok = 0;
						update_input(RVP_NEW_DIR, RED, input);
						if (edit_stat == CONFIRM) {
							CLEAR_MESSAGE();
							message(RED, "\"%s\" already exists!", input);
							rover_getch();
							CLEAR_MESSAGE();
							edit_stat = CONTINUE;
						}
					} else if (!isvalidfilename(input, false)) {
						ok = 0;
						update_input(RVP_NEW_DIR, RED, input);
						if (edit_stat == CONFIRM) {
							CLEAR_MESSAGE();
							message(RED, "\"%s\" invalid dirname!", input);
							rover_getch();
							CLEAR_MESSAGE();
							edit_stat = CONTINUE;
						}
					}
				}
				update_input(RVP_NEW_DIR, (ok ? GREEN : RED), input);
			} while (edit_stat == CONTINUE);

			CLEAR_MESSAGE();
			if (ok && edit_stat == CONFIRM) {
				if (adddir(input) == 0) {
					cd(true);
					ADDSLASH(input);
					try_to_sel(input);
					update_view();
				} else {
					message(RED, "Could not create \"%s/\".", input);
					rover_getch();
					CLEAR_MESSAGE();
				}
			}
			break;
		case KEY_F(2): //Rename selected file or directory
			ok = 0;
			strcpy(input, ENAME(ESEL));
			last  = input + strlen(input) - 1;
			isdir = *last;
			if (*last == '/')
				*last = '\0';
			start_line_edit(input);
			update_input(RVP_RENAME, RED, input);
			while ((edit_stat = get_line_edit(input)) == CONTINUE) {
				length = strlen(input);
				ok     = length;
				for (i = 0; i < rover.nfiles; i++)
					if (!strncmp(ENAME(i), input, length) &&
					    (!strcmp(ENAME(i) + length, "") || !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						i  = rover.nfiles; //in order to exit from nested loop without using break;
					}
				update_input(RVP_RENAME, (ok ? GREEN : RED), input);
			}
			CLEAR_MESSAGE();
			if (edit_stat == CONFIRM) {
				if (isdir)
					ADDSLASH(input);
				if (ok) {
					if (!rename(ENAME(ESEL), input) && MARKED(ESEL)) {
						del_mark(&rover.marks, ENAME(ESEL));
						add_mark(&rover.marks, CWD, input);
					}
					cd(true);
					try_to_sel(input);
					update_view();
				} else
					message(RED, "\"%s\" already exists.", input);
			}
			break;
		case KEY_F(4): //Toggle execute permission of the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (S_IXUSR & EMODE(ESEL))
				EMODE(ESEL) &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
			else
				EMODE(ESEL) |= S_IXUSR | S_IXGRP | S_IXOTH;
			if (chmod(ENAME(ESEL), EMODE(ESEL))) {
				message(RED, "Failed to change mode of \"%s\".", ENAME(ESEL));
			} else {
				message(GREEN, "Changed mode of \"%s\".", ENAME(ESEL));
				update_view();
			}
			break;
		case KEY_DC: //Delete selected file or (empty) directory
			if (rover.nfiles) {
				message(YELLOW, "Delete \"%s\"? (Y/n)", ENAME(ESEL));
				if (rover_getch() == 'Y') {
					strcpy(buffer, ENAME(ESEL));
					if (MARKED(ESEL))
						del_mark(&rover.marks, ENAME(ESEL));
					ok = rm(buffer);
					reload();
					if (ok == 0) {
						CLEAR_MESSAGE();
						message(YELLOW, "\"%s\" deleted!", buffer);
						rover_getch();
					} else {
						CLEAR_MESSAGE();
						message(RED, "Error removing \"%s\", to get more info read \"%s.log\"", buffer, ROVER); //print error message
						rover_getch();
						CLEAR_MESSAGE();
					}
				}
				CLEAR_MESSAGE();
			} else {
				message(RED, "No entry for deletion.");
				rover_getch();
			}
			break;
		case KEY_SPACE: //Toggle mark on the selected entry
			if (MARKED(ESEL))
				del_mark(&rover.marks, ENAME(ESEL));
			else
				add_mark(&rover.marks, CWD, ENAME(ESEL));
			MARKED(ESEL) = !MARKED(ESEL);
			ESEL         = (ESEL + 1) % rover.nfiles;
			update_view();
			break;
		case KEY_CTRL_S: //Toggle mark on all visible entries
			for (i = 0; i < rover.nfiles; i++) {
				if (MARKED(i))
					del_mark(&rover.marks, ENAME(i));
				else
					add_mark(&rover.marks, CWD, ENAME(i));
				MARKED(i) = !MARKED(i);
			}
			update_view();
			break;
		case KEY_CTRL_A: //Mark all visible entries
			for (i = 0; i < rover.nfiles; i++)
				if (!MARKED(i)) {
					add_mark(&rover.marks, CWD, ENAME(i));
					MARKED(i) = true;
				}
			update_view();
			break;
		case 'X': //Delete all marked entries
			if (rover.marks.nentries) {
				message(YELLOW, "Delete all marked entries? (Y/n)");
				if (rover_getch() == 'Y')
					process_marked(NULL, rm, rm, "Deleting", "Deleted");
				else
					CLEAR_MESSAGE();
			} else
				message(RED, "No entries marked for deletion.");
			break;
		case 'C': //Copy all marked entries
			if (rover.marks.nentries) {
				if (strcmp(CWD, rover.marks.dirpath))
					process_marked(adddir, cpyfile, NULL, "Copying", "Copied");
				else
					message(RED, "Cannot copy to the same path.");
			} else
				message(RED, "No entries marked for copying.");
			break;
		case 'V': //Move all marked entries
			if (rover.marks.nentries) {
				if (strcmp(CWD, rover.marks.dirpath))
					process_marked(adddir, movfile, rm, "Moving", "Moved");
				else
					message(RED, "Cannot move to the same path.");
			} else
				message(RED, "No entries marked for moving.");
			break;
		default:
			LOG(LOG_INFO, "0x%02X", ch);
		}
	} while (!quit);

	return;
}