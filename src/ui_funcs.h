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
#include <getopt.h>

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

#define STATUSPOS (COLS - 16)

/* Number of entries to jump on RVK_JUMP_DOWN and RVK_JUMP_UP. */
#define RV_JUMP 10

/* Colors available: DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE, BLACK. */
#define RVC_CWD       GREEN
#define RVC_STATUS    CYAN
#define RVC_BORDER    BLUE
#define RVC_SCROLLBAR CYAN
#define RVC_LINK      YELLOW
#define RVC_HIDDEN    BLUE
#define RVC_EXEC      CYAN
#define RVC_REG       GREEN
#define RVC_DIR       WHITE
#define RVC_CHR       MAGENTA
#define RVC_BLK       MAGENTA
#define RVC_FIFO      BLUE
#define RVC_SOCK      MAGENTA
#define RVC_PROMPT    DEFAULT
#define RVC_TABNUM    DEFAULT
#define RVC_MARKS     YELLOW

/* KEY not defined by curses.h */
#ifndef KEY_SPACE
#define KEY_SPACE 0x20
#endif
#ifndef KEY_ESC
#define KEY_ESC 0x1B
#endif
#ifndef KEY_TAB
#define KEY_TAB 0x09
#endif
#ifndef KEY_RETURN
#define KEY_RETURN 0x0D
#endif
#ifndef KEY_CTRL_DEL
#define KEY_CTRL_DEL 0x207
#endif
#ifndef KEY_CTRL_BS
#define KEY_CTRL_BS 0x08
#endif
#ifndef KEY_CTRL_LEFT
#define KEY_CTRL_LEFT 0x221
#endif
#ifndef KEY_CTRL_RIGHT
#define KEY_CTRL_RIGHT 0x230
#endif
#ifndef KEY_CTRL_X
#define KEY_CTRL_X 0x18
#endif
#ifndef KEY_CTRL_C
#define KEY_CTRL_C 0x03
#endif
#ifndef KEY_CTRL_V
#define KEY_CTRL_V 0x16
#endif
#ifndef KEY_CTRL_A
#define KEY_CTRL_A 0x01
#endif
#ifndef KEY_CTRL_S
#define KEY_CTRL_S 0x13
#endif
#ifndef KEY_CTRL_D
#define KEY_CTRL_D 0x04
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
int rover_getch(void);
void message(Color color, char *fmt, ...);
void update_view(void);
void init_marks(Marks *marks);
void free_marks(Marks *marks);
void del_mark(Marks *marks, char *entry);
void main_menu(void);

#endif // _UI_FUNCS_H