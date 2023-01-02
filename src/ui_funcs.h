#ifndef _UI_FUNCS_H
#define _UI_FUNCS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* for asprintf() environ */
#endif

#include <curses.h>
#include <locale.h> /* setlocale(), LC_ALL */
#include <limits.h> /* PATH_MAX */
#include <stdlib.h>
#include <unistd.h> /* environ PAGER SHELL EDITOR VISUAL */
#include <libgen.h> /* dirname() */

#define STATUSPOS (COLS - 16)

/* Shell used to launch external  programs.
   Defining   this  macro   will  force   Rover  to   launch  external
   programs with  `sh -c  "$EXTERNAL_PROGRAM [arg]"`. This  gives more
   flexibility,  allowing command-line  arguments  to  be embedded  in
   environment variables  (e.g. PAGER="less  -N"). On the  other hand,
   this requires the presence of a  shell and will spawn an additional
   process each time an external  program is invoked. Leave this macro
   undefined if you prefer external  programs to be launched with just
   `$EXTERNAL_PROGRAM [arg]`. */
#define RV_SHELL "/bin/sh"

/* Number of entries to jump on RVK_JUMP_DOWN and RVK_JUMP_UP. */
#define RV_JUMP 10

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

/* KEY not defined by curses.h */
#ifndef KEY_SPACE
#define KEY_SPACE 32
#endif
#ifndef KEY_ESC
#define KEY_ESC 27
#endif
#ifndef KEY_TAB
#define KEY_TAB 9
#endif
#ifndef KEY_RETURN
#define KEY_RETURN '\r'
#endif
#ifndef KEY_CTRL_DEL
#define KEY_CTRL_DEL 519
#endif
#ifndef KEY_CTRL_BS
#define KEY_CTRL_BS 8
#endif
#ifndef KEY_CTRL_LEFT
#define KEY_CTRL_LEFT 545
#endif
#ifndef KEY_CTRL_RIGHT
#define KEY_CTRL_RIGHT 560
#endif

/* Line Editing Macros. */
#define EDIT_FULL(E)      ((E).left == (E).right)
#define EDIT_CAN_LEFT(E)  ((E).left)
#define EDIT_CAN_RIGHT(E) ((E).right < PATH_MAX - 1)
#define EDIT_LEFT(E)      (E).buffer[(E).right--] = (E).buffer[--(E).left]
#define EDIT_RIGHT(E)     (E).buffer[(E).left++] = (E).buffer[++(E).right]
#define EDIT_INSERT(E, C) (E).buffer[(E).left++] = (C)
#define EDIT_BACKSPACE(E) (E).left--
#define EDIT_DELETE(E)    (E).right++
#define EDIT_CLEAR(E)             \
	{                             \
		(E).left  = 0;            \
		(E).right = PATH_MAX - 1; \
	}
	
/* Get user programs from the environment vars */
#define ROVER_ENV(dst, src)                        \
	{                                              \
		if ((dst = getenv("ROVER_" #src)) == NULL) \
			dst = getenv(#src);                    \
	}

/* Clear message line */
#define CLEAR_MESSAGE() mvhline(LINES - 1, 0, ' ', STATUSPOS)

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

struct User {
	char *Shell;
	char *Pager;
	char *Editor;
	char *Open;
};

typedef enum EditStat {
	CONTINUE,
	CONFIRM,
	CANCEL
} EditStat;

// Function declarations
void handle_usr1(int sig);
void handle_winch(int sig);
void handlers(bool enable);
void message(Color color, char *fmt, ...);
void update_view(void);
void main_menu(void);

#endif // _UI_FUNCS_H