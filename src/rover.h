#ifndef _ROVER_H
#define _ROVER_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#ifndef _XOPEN_SOURCE_EXTENDED
#define _XOPEN_SOURCE_EXTENDED
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

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
#include <sys/stat.h>
#include <fcntl.h> /* open() */
#include <sys/wait.h> /* waitpid() */
#include <signal.h> /* struct sigaction, sigaction() */
#include <errno.h>
#include <stdarg.h>
#include <curses.h>
#include <stdbool.h>
#include <time.h>

#define RV_VERSION "2.0.1" //Sandroid75

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

#define ROVER       "rover"
#define LOG_INFO    false
#define LOG_ERR     true
#define RV_PATH_MAX (PATH_MAX - 16)

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

/* Macro to initialize the rover struct */
#define INIT_ROVER(_r)                       \
	{                                        \
		_r.nfiles = 0;                       \
		for (int idx = 0; idx < 10; idx++) { \
			_r.tabs[idx].esel   = 0;         \
			_r.tabs[idx].scroll = 0;         \
			_r.tabs[idx].flags  = RV_FLAGS;  \
		}                                    \
	}

/* Optional macro to be executed when a batch operation finishes. */
#define RV_ALERT() beep()

/* Macro to manage logfile() func in order to read detailed info from source files */
#define LOG(flag, msg, ...)                                                                                                      \
	{                                                                                                                            \
		logfile("{%s} %s %s() <%d>: " msg "\n", (flag ? "ERR" : "INFO"), FILENAME(__FILE__), __func__, __LINE__, ##__VA_ARGS__); \
	}

/* Get file or dir name without path */
#define FILENAME(_path) (strrchr(_path, '/') + 1)

/* Check if not root dir than add / at the end of path */
#define ADDSLASH(_path)                      \
	{                                        \
		if (strcmp(_path, "/") &&            \
		    _path[strlen(_path) - 1] != '/') \
			strcat(_path, "/");              \
	}

#define DELSLASH(_path)                      \
	{                                        \
		if (strcmp(_path, "/") &&            \
		    _path[strlen(_path) - 1] == '/') \
			_path[strlen(_path) - 1] = '\0'; \
	}

/* Safe version of free() don't need assign NULL after free */
#define FREE(p)        \
	{                  \
		if ((p))       \
			free((p)); \
		(p) = NULL;    \
	}

typedef int (*PROCESS)(const char *path);

extern struct Rover rover;
extern char rover_home_path[RV_PATH_MAX];

// Functions declaration
void logfile(const char *format, ...);
void init_term(void);
int endsession(void);
void reload(void);
void free_rows(Row **rowsp, int nfiles);
void try_to_sel(const char *target);
void process_marked(PROCESS pre, PROCESS proc, PROCESS pos, const char *msg_doing, const char *msg_done);

#endif // _ROVER_H