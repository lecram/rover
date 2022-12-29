#ifndef _UI_FUNCS_H
#define _UI_FUNCS_H

#include <curses.h>
#include <locale.h> /* setlocale(), LC_ALL */
#include <limits.h> /* PATH_MAX */

typedef enum Color {
	DEFAULT,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	CYAN,
	MAGENTA,
	WHITE,
	BLACK
} Color;

void init_term();
void update_input(const char *prompt, Color color, const char *input);
void message(Color color, char *fmt, ...);

#endif // _UI_FUNCS_H