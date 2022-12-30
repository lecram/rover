#ifndef _CONFIG_H
#define _CONFIG_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#define _XOPEN_SOURCE_EXTENDED
#define _FILE_OFFSET_BITS 64

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

#define RV_VERSION "2.0.1"

/* Special symbols used by the TUI. See <curses.h> for available constants. */
#define RVS_SCROLLBAR ACS_CKBOARD
#define RVS_MARK      ACS_DIAMOND

/* Prompt strings for line input. */
#define RV_PROMPT(S) S ": "
#define RVP_SEARCH   RV_PROMPT("search")
#define RVP_NEW_FILE RV_PROMPT("new file")
#define RVP_NEW_DIR  RV_PROMPT("new dir")
#define RVP_RENAME   RV_PROMPT("rename")

/* Default listing view flags.
   May include SHOW_FILES, SHOW_DIRS and SHOW_HIDDEN. */
#define RV_FLAGS SHOW_FILES | SHOW_DIRS

/* Optional macro to be executed when a batch operation finishes. */
#define RV_ALERT() beep()

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
	bool islink;
	bool marked;
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
void reload();
void sync_signals();
int rover_getch();
int rover_get_wch(wint_t *wch);
void shell_escaped_cat(char *buf, char *str, size_t n);
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