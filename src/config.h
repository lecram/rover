#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#define _XOPEN_SOURCE_EXTENDED
#define _FILE_OFFSET_BITS 64

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <sys/types.h> /* pid_t, ... */
#include <stdio.h>
#include <limits.h> /* PATH_MAX */
#include <locale.h> /* setlocale(), LC_ALL */
#include <unistd.h> /* chdir(), getcwd(), read(), close(), ... */
#include <dirent.h> /* DIR, struct dirent, opendir(), ... */
#include <libgen.h>
#include <sys/stat.h>
#include <fcntl.h> /* open() */
#include <sys/wait.h> /* waitpid() */
#include <signal.h> /* struct sigaction, sigaction() */
#include <errno.h>
#include <stdarg.h>
#include <curses.h>
#include <stdbool.h>

#define RV_VERSION "1.0.1"

/* CTRL+X: "^X"
   ALT+X: "M-X" */
#define RVK_QUIT        "q"
#define RVK_HELP        "?"
#define RVK_DOWN        "j"
#define RVK_UP          "k"
#define RVK_JUMP_DOWN   "J"
#define RVK_JUMP_UP     "K"
#define RVK_JUMP_TOP    "g"
#define RVK_JUMP_BOTTOM "G"
#define RVK_CD_DOWN     "l"
#define RVK_CD_UP       "h"
#define RVK_HOME        "H"
#define RVK_TARGET      "t"
#define RVK_COPY_PATH   "y"
#define RVK_PASTE_PATH  "p"
#define RVK_REFRESH     "r"
#define RVK_SHELL       "^M"
#define RVK_VIEW        " "
#define RVK_EDIT        "e"
#define RVK_OPEN        "o"
#define RVK_SEARCH      "/"
#define RVK_TG_FILES    "f"
#define RVK_TG_DIRS     "d"
#define RVK_TG_HIDDEN   "s"
#define RVK_NEW_FILE    "n"
#define RVK_NEW_DIR     "N"
#define RVK_RENAME      "R"
#define RVK_TG_EXEC     "E"
#define RVK_DELETE      "D"
#define RVK_TG_MARK     "m"
#define RVK_INVMARK     "M"
#define RVK_MARKALL     "a"
#define RVK_MARK_DELETE "X"
#define RVK_MARK_COPY   "C"
#define RVK_MARK_MOVE   "V"

/* Special symbols used by the TUI. See <curses.h> for available constants. */
#define RVS_SCROLLBAR ACS_CKBOARD
#define RVS_MARK      ACS_DIAMOND

/* Prompt strings for line input. */
#define RV_PROMPT(S) S ": "
#define RVP_SEARCH   RV_PROMPT("search")
#define RVP_NEW_FILE RV_PROMPT("new file")
#define RVP_NEW_DIR  RV_PROMPT("new dir")
#define RVP_RENAME   RV_PROMPT("rename")

/* Number of entries to jump on RVK_JUMP_DOWN and RVK_JUMP_UP. */
#define RV_JUMP 10

/* Default listing view flags.
   May include SHOW_FILES, SHOW_DIRS and SHOW_HIDDEN. */
#define RV_FLAGS SHOW_FILES | SHOW_DIRS

/* Optional macro to be executed when a batch operation finishes. */
#define RV_ALERT() beep()

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

/*  This signal is not defined by POSIX, but should be
   present on all systems that have resizable terminals. */
#ifndef SIGWINCH
#define SIGWINCH 28
#endif

/* Listing view parameters. */
#define HEIGHT (LINES - 4)

/* Listing view flags. */
#define SHOW_FILES  0x01u
#define SHOW_DIRS   0x02u
#define SHOW_HIDDEN 0x04u

/* Marks parameters. */
#define BULK_INIT   5
#define BULK_THRESH 256

/* Information associated to each entry in listing. */
typedef struct Row {
	char *name;
	off_t size;
	mode_t mode;
	int islink;
	int marked;
} Row;

/* Dynamic array of marked entries. */
typedef struct Marks {
	char dirpath[PATH_MAX];
	int bulk;
	int nentries;
	char **entries;
} Marks;

/* Line editing state. */
typedef struct Edit {
	wchar_t buffer[PATH_MAX + 1];
	int left, right;
} Edit;

/* Each tab only stores the following information. */
typedef struct Tab {
	int scroll;
	int esel;
	uint8_t flags;
	char cwd[PATH_MAX];
} Tab;

typedef struct Prog {
	off_t partial;
	off_t total;
	const char *msg;
} Prog;

/* Global state. */
struct Rover {
	int tab;
	int nfiles;
	Row *rows;
	WINDOW *window;
	Marks marks;
	Edit edit;
	int edit_scroll;
	volatile sig_atomic_t pending_usr1;
	volatile sig_atomic_t pending_winch;
	Prog prog;
	Tab tabs[10];
};

extern struct Rover rover;

/* Macros for accessing global state. */
#define ENAME(I)  rover.rows[I].name
#define ESIZE(I)  rover.rows[I].size
#define EMODE(I)  rover.rows[I].mode
#define ISLINK(I) rover.rows[I].islink
#define MARKED(I) rover.rows[I].marked
#define SCROLL    rover.tabs[rover.tab].scroll
#define ESEL      rover.tabs[rover.tab].esel
#define FLAGS     rover.tabs[rover.tab].flags
#define CWD       rover.tabs[rover.tab].cwd

/* Helpers. */
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define ISDIR(E)  (strchr((E), '/') != NULL)

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
/* Add / at the end of path */
#define ADDSLASH(path)                     \
	{                                      \
		if (path[strlen(path) - 1] != '/') \
			strcat(path, "/");             \
	}
/* Safe version of free() don't need assign NULL after free */
#define FREE(p)        \
	{                  \
		if ((p))       \
			free((p)); \
		(p) = NULL;    \
	}
typedef enum EditStat {
	CONTINUE,
	CONFIRM,
	CANCEL
} EditStat;

typedef int (*PROCESS)(const char *path);

// FUnction declarations
void init_marks(Marks *marks);
void mark_none(Marks *marks);
void add_mark(Marks *marks, char *dirpath, char *entry);
void del_mark(Marks *marks, char *entry);
void free_marks(Marks *marks);
void handle_usr1(int sig);
void handle_winch(int sig);
void enable_handlers();
void disable_handlers();
void reload();
void sync_signals();
int rover_getch();
int rover_get_wch(wint_t *wch);
void spawn(char **args);
void shell_escaped_cat(char *buf, char *str, size_t n);
int open_with_env(char *program, char *path);
void update_view();
int rowcmp(const void *a, const void *b);
int ls(Row **rowsp, uint8_t flags);
void free_rows(Row **rowsp, int nfiles);
void cd(bool reset);
void try_to_sel(const char *target);
void reload();
off_t count_dir(const char *path);
off_t count_marked();
int process_dir(PROCESS pre, PROCESS proc, PROCESS pos, const char *path);
void process_marked(PROCESS pre, PROCESS proc, PROCESS pos, const char *msg_doing, const char *msg_done);
void update_progress(off_t delta);
int delfile(const char *path);
int addfile(const char *path);
int cpyfile(const char *srcpath);
int adddir(const char *path);
int movfile(const char *srcpath);
void start_line_edit(const char *init_input);
EditStat get_line_edit();

#endif // _CONFIG_H