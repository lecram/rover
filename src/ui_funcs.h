#ifndef _UI_FUNCS_H
#define _UI_FUNCS_H

#include <curses.h>
#include <locale.h> /* setlocale(), LC_ALL */
#include <limits.h> /* PATH_MAX */

#define STATUSPOS (COLS - 16)

/* Colors available: DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE, BLACK. */
#define RVC_CWD       GREEN
#define RVC_STATUS    CYAN
#define RVC_BORDER    BLUE
#define RVC_SCROLLBAR CYAN
#define RVC_LINK      CYAN
#define RVC_HIDDEN    YELLOW
#define RVC_EXEC      GREEN
#define RVC_REG       DEFAULT
#define RVC_DIR       DEFAULT
#define RVC_CHR       MAGENTA
#define RVC_BLK       MAGENTA
#define RVC_FIFO      BLUE
#define RVC_SOCK      MAGENTA
#define RVC_PROMPT    DEFAULT
#define RVC_TABNUM    DEFAULT
#define RVC_MARKS     YELLOW

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

// Function declarations
void init_term();
void update_input(const char *prompt, Color color, const char *input);
void message(Color color, char *fmt, ...);
void update_view();
void clear_message();

#endif // _UI_FUNCS_H