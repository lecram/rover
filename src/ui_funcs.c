#include "config.h"
#include "ui_funcs.h"

/* Curses setup. */
void init_term()
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
	maxlen = STATUSPOS - plen -2;
	if (ilen - rover.edit_scroll < maxlen)
		rover.edit_scroll = MAX(ilen - maxlen, 0);
	else if (rover.edit.left > rover.edit_scroll + maxlen -1)
		rover.edit_scroll = rover.edit.left - maxlen;
	else if (rover.edit.left < rover.edit_scroll)
		rover.edit_scroll = MAX(rover.edit.left - maxlen, 0);
	color_set(RVC_PROMPT, NULL);
	mvaddstr(LINES -1, 0, prompt);
	color_set(color, NULL);
	mbstowcs(wbuffer, input, COLS);
	mvaddnwstr(LINES -1, plen, &wbuffer[rover.edit_scroll], maxlen);
	mvaddch(LINES -1, plen + MIN(ilen - rover.edit_scroll, maxlen +1), ' ');
	color_set(DEFAULT, NULL);
	if (rover.edit_scroll)
		mvaddch(LINES -1, plen -1, '<');
	if (ilen > rover.edit_scroll + maxlen)
		mvaddch(LINES -1, plen + maxlen, '>');
	move(LINES -1, plen + rover.edit.left - rover.edit_scroll);
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
	
	pos = (STATUSPOS - (int) strlen(buffer)) /2;
	attr_on(A_BOLD, NULL);
	color_set(color, NULL);
	mvaddstr(LINES -1, pos, buffer);
	color_set(DEFAULT, NULL);
	attr_off(A_BOLD, NULL);
}

/* Update the listing view. */
void update_view()
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

/* Clear message area, leaving only status info. */
void clear_message()
{
	mvhline(LINES -1, 0, ' ', STATUSPOS);
}