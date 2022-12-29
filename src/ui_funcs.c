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
	int len, pos;
	va_list args;
	char buffer[PATH_MAX];

	va_start(args, fmt);
	vsnprintf(buffer, MIN(PATH_MAX, STATUSPOS), fmt, args);
	va_end(args);
	len = strlen(buffer);
	pos = (STATUSPOS - len) / 2;
	attr_on(A_BOLD, NULL);
	color_set(color, NULL);
	mvaddstr(LINES - 1, pos, buffer);
	color_set(DEFAULT, NULL);
	attr_off(A_BOLD, NULL);
}
