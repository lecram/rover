#include "config.h"
#include "ui_funcs.h"

static void handle_usr1(int sig)
{
	rover.pending_usr1 = 1;
}

static void handle_winch(int sig)
{
	rover.pending_winch = 1;
}

static void enable_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	
	sa.sa_handler = handle_usr1;
	sigaction(SIGUSR1, &sa, NULL);
	
	sa.sa_handler = handle_winch;
	sigaction(SIGWINCH, &sa, NULL);
}

static void disable_handlers(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(struct sigaction));
	
	sa.sa_handler = SIG_DFL;
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGWINCH, &sa, NULL);
}

/* Curses setup. */
void init_term(void)
{
	setlocale(LC_ALL, "");
	initscr();
	cbreak(); /* Get one character at a time. */
	timeout(100); /* For getch(). */
	noecho();
	nonl(); /* No NL->CR/NL on output. */
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE); /* Hide blinking cursor. */
	if (has_colors()) {
		short bg;
		start_color();
#ifdef NCURSES_EXT_FUNCS
		use_default_colors();
		bg = -1;
#else
		bg = COLOR_BLACK;
#endif
		init_pair(RED, COLOR_RED, bg);
		init_pair(GREEN, COLOR_GREEN, bg);
		init_pair(YELLOW, COLOR_YELLOW, bg);
		init_pair(BLUE, COLOR_BLUE, bg);
		init_pair(CYAN, COLOR_CYAN, bg);
		init_pair(MAGENTA, COLOR_MAGENTA, bg);
		init_pair(WHITE, COLOR_WHITE, bg);
		init_pair(BLACK, COLOR_BLACK, bg);
	}
	atexit((void (*)(void))endwin);
	enable_handlers();
}

/* Update line input on the screen. */
void update_input(const char *prompt, Color color, const char *input)
{
	int plen, ilen, maxlen;
	wchar_t wbuffer[PATH_MAX];

	plen   = strlen(prompt);
	ilen   = mbstowcs(NULL, input, 0);
	maxlen = STATUSPOS - plen - 2;
	if (ilen - rover.edit_scroll < maxlen)
		rover.edit_scroll = MAX(ilen - maxlen, 0);
	else if (rover.edit.left > rover.edit_scroll + maxlen - 1)
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
	if (ilen > rover.edit_scroll + maxlen)
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
	// vsnprintf(buffer, MIN(PATH_MAX, STATUSPOS), fmt, args);
	vsnprintf(buffer, STATUSPOS, fmt, args);
	va_end(args);

	pos = (STATUSPOS - (int)strlen(buffer)) / 2;
	attr_on(A_BOLD, NULL);
	color_set(color, NULL);
	mvaddstr(LINES - 1, pos, buffer);
	color_set(DEFAULT, NULL);
	attr_off(A_BOLD, NULL);
}

/* Update the listing view. */
void update_view(void)
{
	int i, j, numsize, ishidden, marking;
	char buffer_one[PATH_MAX], buffer_two[PATH_MAX];
	wchar_t wbuffer[PATH_MAX];

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
		ishidden = ENAME(j)[0] == '.';
		if (j == ESEL)
			wattr_on(rover.window, A_REVERSE, NULL);
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
			char *suffix, *suffixes = "BKMGTPEZY";
			off_t human_size = ESIZE(j) * 10;
			int length       = mbstowcs(wbuffer, ENAME(j), PATH_MAX);
			int namecols     = wcswidth(wbuffer, length);
			for (suffix = suffixes; human_size >= 10240; suffix++)
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
		int center, height;
		center = (SCROLL + HEIGHT / 2) * HEIGHT / rover.nfiles;
		height = (HEIGHT - 1) * HEIGHT / rover.nfiles;
		if (!height)
			height = 1;
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

/* Do a fork-exec to external program (e.g. $EDITOR). */
static void spawn(char **args)
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

static int open_with_env(char *program, char *path)
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

static PROCESS deldir = rmdir;

void main_menu(void)
{
	int i, ch, oldsel, oldscroll, length, ok, sel;
	bool quit = false, isdir;
	char buffer_one[PATH_MAX], buffer_two[PATH_MAX], input[PATH_MAX], clipboard[PATH_MAX];
	char *program, *dir_name, *bname, *last, first;
	const char *clip_path;
	ssize_t len = readlink(ENAME(ESEL), buffer_one, PATH_MAX - 1);
	Color color = RED;
	FILE *clip_file;
	EditStat edit_stat;
	struct User prg;

	ROVER_ENV(prg.Shell, SHELL);
	ROVER_ENV(prg.Pager, PAGER);
	ROVER_ENV(prg.Editor, VISUAL);
	if (!(prg.Editor))
		ROVER_ENV(prg.Editor, EDITOR);
	ROVER_ENV(prg.Open, OPEN);

	do {
		ch = rover_getch();
		mvhline(LINES - 1, 0, ' ', STATUSPOS);
		switch (ch) {
		case KEY_ESC: //Quit Rover
			quit = true;
			break;
		case '0' ... '9': //Change Tab
			rover.tab = ch - '0';
			cd(false);
			break;
		case '?': //Help
			//spawn((char *[]){ "man", "rover", NULL });
			spawn((char *[]){ "man", "./rover.1", NULL });
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
		case KEY_SPACE: //Enter selected directory
			if (!rover.nfiles || !S_ISDIR(EMODE(ESEL)))
				continue;
			if (chdir(ENAME(ESEL)) == -1) {
				message(RED, "Cannot access \"%s\".", ENAME(ESEL));
				continue;
			}
			strcat(CWD, ENAME(ESEL));
			cd(true);
			break;
		case KEY_BACKSPACE: //Go to parent directory
			if (!strcmp(CWD, "/"))
				continue;
			CWD[strlen(CWD) - 1] = '\0';
			dir_name             = strrchr(CWD, '/') + 1;
			first                = dir_name[0];
			dir_name[0]          = '\0';
			cd(true);
			dir_name[0]                = first;
			dir_name[strlen(dir_name)] = '/';
			try_to_sel(dir_name);
			dir_name[0] = '\0';
			if (rover.nfiles > HEIGHT)
				SCROLL = ESEL - HEIGHT / 2;
			update_view();
			break;
		case '\\': //Go to home directory
			strcpy(CWD, getenv("HOME"));
			ADDSLASH(CWD);
			cd(true);
			break;
		case 't': //Go to the target of the selected link
			isdir = S_ISDIR(EMODE(ESEL));
			len   = readlink(ENAME(ESEL), buffer_one, PATH_MAX - 1);
			if (len == -1)
				continue;
			buffer_one[len] = '\0';
			if (access(buffer_one, F_OK) == -1) {
				char *msg;
				switch (errno) {
				case EACCES:
					msg = "Cannot access \"%s\".";
					break;
				case ENOENT:
					msg = "\"%s\" does not exist.";
					break;
				default:
					msg = "Cannot navigate to \"%s\".";
				}
				strcpy(buffer_two, buffer_one); /* message() uses buffer_one. */
				message(RED, msg, buffer_two);
				continue;
			}
			realpath(buffer_one, CWD);
			len = strlen(CWD);
			if (CWD[len - 1] == '/')
				CWD[len - 1] = '\0';
			bname  = strrchr(CWD, '/') + 1;
			first  = *bname;
			*bname = '\0';
			cd(true);
			*bname = first;
			if (isdir)
				ADDSLASH(CWD);
			try_to_sel(bname);
			*bname = '\0';
			update_view();
			break;
		case 'y': //Copy location to clipboard
			clip_path = getenv("CLIP");
			if (!clip_path)
				goto copy_path_fail;
			clip_file = fopen(clip_path, "w");
			if (!clip_file)
				goto copy_path_fail;
			fprintf(clip_file, "%s%s\n", CWD, ENAME(ESEL));
			fclose(clip_file);
			goto copy_path_done;
copy_path_fail:
			strcpy(clipboard, CWD);
			strcat(clipboard, ENAME(ESEL));
copy_path_done:;
			break;
		case 'p': //Go to location in clipboard
			clip_path = getenv("CLIP");
			if (!clip_path)
				goto paste_path_fail;
			clip_file = fopen(clip_path, "r");
			if (!clip_file)
				goto paste_path_fail;
			fscanf(clip_file, "%s\n", clipboard);
			fclose(clip_file);
paste_path_fail:
			strcpy(buffer_one, clipboard);
			strcpy(CWD, dirname(buffer_one));
			if (strcmp(CWD, "/"))
				ADDSLASH(CWD);
			cd(true);
			strcpy(buffer_one, clipboard);
			try_to_sel(strstr(clipboard, basename(buffer_one)));
			update_view();
			break;
		case 'r': //Refresh directory listing
			reload();
			break;
		case 's': //Open SHELL on the current directory
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
		case 'v': //Open PAGER with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Pager, ENAME(ESEL)))
				cd(false);
			break;
		case 'e': //Open VISUAL or EDITOR with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Editor, ENAME(ESEL)))
				cd(false);
			break;
		case 'o': //Open OPEN with the selected file
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(prg.Open, ENAME(ESEL)))
				cd(false);
			break;
		case 'i': //Start incremental search
			if (!rover.nfiles)
				continue;
			oldsel    = ESEL;
			oldscroll = SCROLL;
			start_line_edit("");
			update_input(RVP_SEARCH, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
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
			mvhline(LINES - 1, 0, ' ', STATUSPOS);
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
		case 'n': //Create new file
			ok = 0;
			start_line_edit("");
			update_input(RVP_NEW_FILE, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int length = strlen(input);
				ok         = length;
				for (i = 0; i < rover.nfiles; i++) {
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				}
				update_input(RVP_NEW_FILE, (ok ? GREEN : RED), input);
			}
			mvhline(LINES - 1, 0, ' ', STATUSPOS);
			if (edit_stat == CONFIRM) {
				if (ok) {
					if (addfile(input) == 0) {
						cd(true);
						try_to_sel(input);
						update_view();
					} else
						message(RED, "Could not create \"%s\".", input);
				} else
					message(RED, "\"%s\" already exists.", input);
			}
			break;
		case 'N': //Create new dir
			ok = 0;
			start_line_edit("");
			update_input(RVP_NEW_DIR, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int length = strlen(input);
				ok         = length;
				for (i = 0; i < rover.nfiles; i++) {
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				}
				update_input(RVP_NEW_DIR, (ok ? GREEN : RED), input);
			}
			mvhline(LINES - 1, 0, ' ', STATUSPOS);
			if (edit_stat == CONFIRM) {
				if (ok) {
					if (adddir(input) == 0) {
						cd(true);
						ADDSLASH(input);
						try_to_sel(input);
						update_view();
					} else
						message(RED, "Could not create \"%s/\".", input);
				} else
					message(RED, "\"%s\" already exists.", input);
			}
			break;
		case KEY_F(2): //Rename selected file or directory
			ok = 0;
			strcpy(input, ENAME(ESEL));
			last = input + strlen(input) - 1;
			if ((isdir = *last == '/'))
				*last = '\0';
			start_line_edit(input);
			update_input(RVP_RENAME, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				length = strlen(input);
				ok     = length;
				for (i = 0; i < rover.nfiles; i++)
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				update_input(RVP_RENAME, (ok ? GREEN : RED), input);
			}
			mvhline(LINES - 1, 0, ' ', STATUSPOS);
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
		case 'E': //Toggle execute permission of the selected file
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
					const char *name = ENAME(ESEL);
					int ret          = ISDIR(ENAME(ESEL)) ? deldir(name) : delfile(name);
					reload();
					if (ret)
						message(RED, "Could not delete \"%s\".", ENAME(ESEL));
				} else
					mvhline(LINES - 1, 0, ' ', STATUSPOS);
			} else
				message(RED, "No entry selected for deletion.");
			break;
		case 'm': //Toggle mark on the selected entry
			if (MARKED(ESEL))
				del_mark(&rover.marks, ENAME(ESEL));
			else
				add_mark(&rover.marks, CWD, ENAME(ESEL));
			MARKED(ESEL) = !MARKED(ESEL);
			ESEL         = (ESEL + 1) % rover.nfiles;
			update_view();
			break;
		case 'M': //Toggle mark on all visible entries
			for (i = 0; i < rover.nfiles; i++) {
				if (MARKED(i))
					del_mark(&rover.marks, ENAME(i));
				else
					add_mark(&rover.marks, CWD, ENAME(i));
				MARKED(i) = !MARKED(i);
			}
			update_view();
			break;
		case 'a': //Mark all visible entries
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
					process_marked(NULL, delfile, deldir, "Deleting", "Deleted");
				else
					mvhline(LINES - 1, 0, ' ', STATUSPOS);
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
						process_marked(adddir, movfile, deldir, "Moving", "Moved");
					else
						message(RED, "Cannot move to the same path.");
				} else
					message(RED, "No entries marked for moving.");
				break;

		}
	} while(!quit);

	return;
}