#define _XOPEN_SOURCE       700
#define _XOPEN_SOURCE_EXTENDED
#define _FILE_OFFSET_BITS   64

#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <wchar.h>
#include <wctype.h>
#include <string.h>
#include <sys/types.h>  /* pid_t, ... */
#include <stdio.h>
#include <limits.h>     /* PATH_MAX */
#include <locale.h>     /* setlocale(), LC_ALL */
#include <unistd.h>     /* chdir(), getcwd(), read(), close(), ... */
#include <dirent.h>     /* DIR, struct dirent, opendir(), ... */
#include <sys/stat.h>
#include <fcntl.h>      /* open() */
#include <sys/wait.h>   /* waitpid() */
#include <signal.h>     /* struct sigaction, sigaction() */
#include <errno.h>
#include <stdarg.h>
#include <curses.h>

#include "config.h"

/*  This signal is not defined by POSIX, but should be
   present on all systems that have resizable terminals. */
#ifndef SIGWINCH
#define SIGWINCH  28
#endif

/* String buffers. */
#define BUFLEN  PATH_MAX
static char BUF1[BUFLEN];
static char BUF2[BUFLEN];
static char INPUT[BUFLEN];
static wchar_t WBUF[BUFLEN];

/* Argument buffers for execvp(). */
#define MAXARGS 256
static char *ARGS[MAXARGS];

/* Listing view parameters. */
#define HEIGHT      (LINES-4)
#define STATUSPOS   (COLS-16)

/* Listing view flags. */
#define SHOW_FILES      0x01u
#define SHOW_DIRS       0x02u
#define SHOW_HIDDEN     0x04u

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
    wchar_t buffer[BUFLEN+1];
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
static struct Rover {
    int tab;
    int nfiles;
    Row *rows;
    WINDOW *window;
    Marks marks;
    Edit edit;
    int edit_scroll;
    volatile sig_atomic_t pending_winch;
    Prog prog;
    Tab tabs[10];
} rover;

/* Macros for accessing global state. */
#define ENAME(I)    rover.rows[I].name
#define ESIZE(I)    rover.rows[I].size
#define EMODE(I)    rover.rows[I].mode
#define ISLINK(I)   rover.rows[I].islink
#define MARKED(I)   rover.rows[I].marked
#define SCROLL      rover.tabs[rover.tab].scroll
#define ESEL        rover.tabs[rover.tab].esel
#define FLAGS       rover.tabs[rover.tab].flags
#define CWD         rover.tabs[rover.tab].cwd

/* Helpers. */
#define MIN(A, B)   ((A) < (B) ? (A) : (B))
#define MAX(A, B)   ((A) > (B) ? (A) : (B))
#define ISDIR(E)    (strchr((E), '/') != NULL)

/* Line Editing Macros. */
#define EDIT_FULL(E)       ((E).left == (E).right)
#define EDIT_CAN_LEFT(E)   ((E).left)
#define EDIT_CAN_RIGHT(E)  ((E).right < BUFLEN-1)
#define EDIT_LEFT(E)       (E).buffer[(E).right--] = (E).buffer[--(E).left]
#define EDIT_RIGHT(E)      (E).buffer[(E).left++] = (E).buffer[++(E).right]
#define EDIT_INSERT(E, C)  (E).buffer[(E).left++] = (C)
#define EDIT_BACKSPACE(E)  (E).left--
#define EDIT_DELETE(E)     (E).right++
#define EDIT_CLEAR(E)      do { (E).left = 0; (E).right = BUFLEN-1; } while(0)

typedef enum EditStat {CONTINUE, CONFIRM, CANCEL} EditStat;
typedef enum Color {DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE, BLACK} Color;
typedef int (*PROCESS)(const char *path);

static void
init_marks(Marks *marks)
{
    strcpy(marks->dirpath, "");
    marks->bulk = BULK_INIT;
    marks->nentries = 0;
    marks->entries = calloc(marks->bulk, sizeof *marks->entries);
}

/* Unmark all entries. */
static void
mark_none(Marks *marks)
{
    int i;

    strcpy(marks->dirpath, "");
    for (i = 0; i < marks->bulk && marks->nentries; i++)
        if (marks->entries[i]) {
            free(marks->entries[i]);
            marks->entries[i] = NULL;
            marks->nentries--;
        }
    if (marks->bulk > BULK_THRESH) {
        /* Reset bulk to free some memory. */
        free(marks->entries);
        marks->bulk = BULK_INIT;
        marks->entries = calloc(marks->bulk, sizeof *marks->entries);
    }
}

static void
add_mark(Marks *marks, char *dirpath, char *entry)
{
    int i;

    if (!strcmp(marks->dirpath, dirpath)) {
        /* Append mark to directory. */
        if (marks->nentries == marks->bulk) {
            /* Expand bulk to accomodate new entry. */
            int extra = marks->bulk / 2;
            marks->bulk += extra; /* bulk *= 1.5; */
            marks->entries = realloc(marks->entries,
                                     marks->bulk * sizeof *marks->entries);
            memset(&marks->entries[marks->nentries], 0,
                   extra * sizeof *marks->entries);
            i = marks->nentries;
        } else {
            /* Search for empty slot (there must be one). */
            for (i = 0; i < marks->bulk; i++)
                if (!marks->entries[i])
                    break;
        }
    } else {
        /* Directory changed. Discard old marks. */
        mark_none(marks);
        strcpy(marks->dirpath, dirpath);
        i = 0;
    }
    marks->entries[i] = malloc(strlen(entry) + 1);
    strcpy(marks->entries[i], entry);
    marks->nentries++;
}

static void
del_mark(Marks *marks, char *entry)
{
    int i;

    if (marks->nentries > 1) {
        for (i = 0; i < marks->bulk; i++)
            if (marks->entries[i] && !strcmp(marks->entries[i], entry))
                break;
        free(marks->entries[i]);
        marks->entries[i] = NULL;
        marks->nentries--;
    } else
        mark_none(marks);
}

static void
free_marks(Marks *marks)
{
    int i;

    for (i = 0; i < marks->bulk && marks->nentries; i++)
        if (marks->entries[i]) {
            free(marks->entries[i]);
            marks->nentries--;
        }
    free(marks->entries);
}

static void
handle_winch(int sig)
{
    rover.pending_winch = 1;
}

static void
enable_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);
}

static void
disable_handlers()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof (struct sigaction));
    sa.sa_handler = SIG_DFL;
    sigaction(SIGWINCH, &sa, NULL);
}

static void update_view();

/* Handle any signals received since last call. */
static void
sync_signals()
{
    if (rover.pending_winch) {
        /* SIGWINCH received: resize application accordingly. */
        delwin(rover.window);
        endwin();
        refresh();
        clear();
        rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
        if (HEIGHT < rover.nfiles && SCROLL + HEIGHT > rover.nfiles)
            SCROLL = ESEL - HEIGHT;
        update_view();
        rover.pending_winch = 0;
    }
}

/* This function must be used in place of getch().
   It handles signals while waiting for user input. */
static int
rover_getch()
{
    int ch;

    while ((ch = getch()) == ERR)
        sync_signals();
    return ch;
}

/* This function must be used in place of get_wch().
   It handles signals while waiting for user input. */
static int
rover_get_wch(wint_t *wch)
{
    wint_t ret;

    while ((ret = get_wch(wch)) == (wint_t) ERR)
        sync_signals();
    return ret;
}

/* Do a fork-exec to external program (e.g. $EDITOR). */
static void
spawn()
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
        execvp(ARGS[0], ARGS);
    }
}

/* Curses setup. */
static void
init_term()
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
    atexit((void (*)(void)) endwin);
    enable_handlers();
}

/* Update the listing view. */
static void
update_view()
{
    int i, j;
    int numsize;
    int ishidden;
    int marking;

    mvhline(0, 0, ' ', COLS);
    attr_on(A_BOLD, NULL);
    color_set(RVC_TABNUM, NULL);
    mvaddch(0, COLS - 2, rover.tab + '0');
    attr_off(A_BOLD, NULL);
    if (rover.marks.nentries) {
        numsize = snprintf(BUF1, BUFLEN, "%d", rover.marks.nentries);
        color_set(RVC_MARKS, NULL);
        mvaddstr(0, COLS - 3 - numsize, BUF1);
    } else
        numsize = -1;
    color_set(RVC_CWD, NULL);
    mbstowcs(WBUF, CWD, PATH_MAX);
    mvaddnwstr(0, 0, WBUF, COLS - 4 - numsize);
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
        if (!S_ISDIR(EMODE(j))) {
            char *suffix, *suffixes = "BKMGTPEZY";
            off_t human_size = ESIZE(j) * 10;
            int length = mbstowcs(NULL, ENAME(j), 0);
            for (suffix = suffixes; human_size >= 10240; suffix++)
                human_size = (human_size + 512) / 1024;
            if (*suffix == 'B')
                swprintf(WBUF, PATH_MAX, L"%s%*d %c", ENAME(j),
                         (int) (COLS - length - 6),
                         (int) human_size / 10, *suffix);
            else
                swprintf(WBUF, PATH_MAX, L"%s%*d.%d %c", ENAME(j),
                         (int) (COLS - length - 8),
                         (int) human_size / 10, (int) human_size % 10, *suffix);
        } else
            mbstowcs(WBUF, ENAME(j), PATH_MAX);
        mvwhline(rover.window, i + 1, 1, ' ', COLS - 2);
        mvwaddnwstr(rover.window, i + 1, 2, WBUF, COLS - 4);
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
        height = (HEIGHT-1) * HEIGHT / rover.nfiles;
        if (!height) height = 1;
        wcolor_set(rover.window, RVC_SCROLLBAR, NULL);
        mvwvline(rover.window, center-height/2+1, COLS-1, RVS_SCROLLBAR, height);
    }
    BUF1[0] = FLAGS & SHOW_FILES  ? 'F' : ' ';
    BUF1[1] = FLAGS & SHOW_DIRS   ? 'D' : ' ';
    BUF1[2] = FLAGS & SHOW_HIDDEN ? 'H' : ' ';
    if (!rover.nfiles)
        strcpy(BUF2, "0/0");
    else
        snprintf(BUF2, BUFLEN, "%d/%d", ESEL + 1, rover.nfiles);
    snprintf(BUF1+3, BUFLEN-3, "%12s", BUF2);
    color_set(RVC_STATUS, NULL);
    mvaddstr(LINES - 1, STATUSPOS, BUF1);
    wrefresh(rover.window);
}

/* Show a message on the status bar. */
static void
message(Color color, char *fmt, ...)
{
    int len, pos;
    va_list args;

    va_start(args, fmt);
    vsnprintf(BUF1, MIN(BUFLEN, STATUSPOS), fmt, args);
    va_end(args);
    len = strlen(BUF1);
    pos = (STATUSPOS - len) / 2;
    attr_on(A_BOLD, NULL);
    color_set(color, NULL);
    mvaddstr(LINES - 1, pos, BUF1);
    color_set(DEFAULT, NULL);
    attr_off(A_BOLD, NULL);
}

/* Clear message area, leaving only status info. */
static void
clear_message()
{
    mvhline(LINES - 1, 0, ' ', STATUSPOS);
}

/* Comparison used to sort listing entries. */
static int
rowcmp(const void *a, const void *b)
{
    int isdir1, isdir2, cmpdir;
    const Row *r1 = a;
    const Row *r2 = b;
    isdir1 = S_ISDIR(r1->mode);
    isdir2 = S_ISDIR(r2->mode);
    cmpdir = isdir2 - isdir1;
    return cmpdir ? cmpdir : strcoll(r1->name, r2->name);
}

/* Get all entries in current working directory. */
static int
ls(Row **rowsp, uint8_t flags)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    Row *rows;
    int i, n;

    if(!(dp = opendir("."))) return -1;
    n = -2; /* We don't want the entries "." and "..". */
    while (readdir(dp)) n++;
    rewinddir(dp);
    rows = malloc(n * sizeof *rows);
    i = 0;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        if (!(flags & SHOW_HIDDEN) && ep->d_name[0] == '.')
            continue;
        lstat(ep->d_name, &statbuf);
        rows[i].islink = S_ISLNK(statbuf.st_mode);
        stat(ep->d_name, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            if (flags & SHOW_DIRS) {
                rows[i].name = malloc(strlen(ep->d_name) + 2);
                strcpy(rows[i].name, ep->d_name);
                strcat(rows[i].name, "/");
                rows[i].mode = statbuf.st_mode;
                i++;
            }
        } else if (flags & SHOW_FILES) {
            rows[i].name = malloc(strlen(ep->d_name) + 1);
            strcpy(rows[i].name, ep->d_name);
            rows[i].size = statbuf.st_size;
            rows[i].mode = statbuf.st_mode;
            i++;
        }
    }
    n = i; /* Ignore unused space in array caused by filters. */
    qsort(rows, n, sizeof (*rows), rowcmp);
    closedir(dp);
    *rowsp = rows;
    return n;
}

static void
free_rows(Row **rowsp, int nfiles)
{
    int i;

    for (i = 0; i < nfiles; i++)
        free((*rowsp)[i].name);
    free(*rowsp);
    *rowsp = NULL;
}

/* Change working directory to the path in CWD. */
static void
cd(int reset)
{
    int i, j;

    message(CYAN, "Loading...");
    refresh();
    if (reset) ESEL = SCROLL = 0;
    chdir(CWD);
    if (rover.nfiles)
        free_rows(&rover.rows, rover.nfiles);
    rover.nfiles = ls(&rover.rows, FLAGS);
    if (!strcmp(CWD, rover.marks.dirpath)) {
        for (i = 0; i < rover.nfiles; i++) {
            for (j = 0; j < rover.marks.bulk; j++)
                if (
                    rover.marks.entries[j] &&
                    !strcmp(rover.marks.entries[j], ENAME(i))
                )
                    break;
            MARKED(i) = j < rover.marks.bulk;
        }
    } else
        for (i = 0; i < rover.nfiles; i++)
            MARKED(i) = 0;
    clear_message();
    update_view();
}

/* Select a target entry, if it is present. */
static void
try_to_sel(const char *target)
{
    ESEL = 0;
    if (!ISDIR(target))
        while ((ESEL+1) < rover.nfiles && S_ISDIR(EMODE(ESEL)))
            ESEL++;
    while ((ESEL+1) < rover.nfiles && strcoll(ENAME(ESEL), target) < 0)
        ESEL++;
}

/* Reload CWD, but try to keep selection. */
static void
reload()
{
    if (rover.nfiles) {
        strcpy(INPUT, ENAME(ESEL));
        cd(0);
        try_to_sel(INPUT);
        update_view();
    } else
        cd(1);
}

static off_t
count_dir(const char *path)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    char subpath[PATH_MAX];
    off_t total;

    if(!(dp = opendir(path))) return 0;
    total = 0;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);
        lstat(subpath, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            strcat(subpath, "/");
            total += count_dir(subpath);
        } else
            total += statbuf.st_size;
    }
    closedir(dp);
    return total;
}

static off_t
count_marked()
{
    int i;
    char *entry;
    off_t total;
    struct stat statbuf;

    total = 0;
    chdir(rover.marks.dirpath);
    for (i = 0; i < rover.marks.bulk; i++) {
        entry = rover.marks.entries[i];
        if (entry) {
            if (ISDIR(entry)) {
                total += count_dir(entry);
            } else {
                lstat(entry, &statbuf);
                total += statbuf.st_size;
            }
        }
    }
    chdir(CWD);
    return total;
}

/* Recursively process a source directory using CWD as destination root.
   For each node (i.e. directory), do the following:
    1. call pre(destination);
    2. call proc() on every child leaf (i.e. files);
    3. recurse into every child node;
    4. call pos(source).
   E.g. to move directory /src/ (and all its contents) inside /dst/:
    strcpy(CWD, "/dst/");
    process_dir(adddir, movfile, deldir, "/src/"); */
static int
process_dir(PROCESS pre, PROCESS proc, PROCESS pos, const char *path)
{
    int ret;
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    char subpath[PATH_MAX];

    ret = 0;
    if (pre) {
        char dstpath[PATH_MAX];
        strcpy(dstpath, CWD);
        strcat(dstpath, path + strlen(rover.marks.dirpath));
        ret |= pre(dstpath);
    }
    if(!(dp = opendir(path))) return -1;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        snprintf(subpath, PATH_MAX, "%s%s", path, ep->d_name);
        stat(subpath, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            strcat(subpath, "/");
            ret |= process_dir(pre, proc, pos, subpath);
        } else
            ret |= proc(subpath);
    }
    closedir(dp);
    if (pos) ret |= pos(path);
    return ret;
}

/* Process all marked entries using CWD as destination root.
   All marked entries that are directories will be recursively processed.
   See process_dir() for details on the parameters. */
static void
process_marked(PROCESS pre, PROCESS proc, PROCESS pos,
               const char *msg_doing, const char *msg_done)
{
    int i, ret;
    char *entry;
    char path[PATH_MAX];

    clear_message();
    message(CYAN, "%s...", msg_doing);
    refresh();
    rover.prog = (Prog) {0, count_marked(), msg_doing};
    for (i = 0; i < rover.marks.bulk; i++) {
        entry = rover.marks.entries[i];
        if (entry) {
            ret = 0;
            snprintf(path, PATH_MAX, "%s%s", rover.marks.dirpath, entry);
            if (ISDIR(entry)) {
                if (!strncmp(path, CWD, strlen(path)))
                    ret = -1;
                else
                    ret = process_dir(pre, proc, pos, path);
            } else
                ret = proc(path);
            if (!ret) {
                del_mark(&rover.marks, entry);
                reload();
            }
        }
    }
    rover.prog.total = 0;
    reload();
    if (!rover.marks.nentries)
        message(GREEN, "%s all marked entries.", msg_done);
    else
        message(RED, "Some errors occured while %s.", msg_doing);
    RV_ALERT();
}

static void
update_progress(off_t delta)
{
    int percent;

    if (!rover.prog.total) return;
    rover.prog.partial += delta;
    percent = (int) (rover.prog.partial * 100 / rover.prog.total);
    message(CYAN, "%s...%d%%", rover.prog.msg, percent);
    refresh();
}

/* Wrappers for file operations. */
static int delfile(const char *path) {
    int ret;
    struct stat st;

    ret = lstat(path, &st);
    if (ret < 0) return ret;
    update_progress(st.st_size);
    return unlink(path);
}
static PROCESS deldir = rmdir;
static int addfile(const char *path) {
    /* Using creat(2) because mknod(2) doesn't seem to be portable. */
    int ret;

    ret = creat(path, 0644);
    if (ret < 0) return ret;
    return close(ret);
}
static int cpyfile(const char *srcpath) {
    int src, dst, ret;
    size_t size;
    struct stat st;
    char buf[BUFSIZ];
    char dstpath[PATH_MAX];

    ret = src = open(srcpath, O_RDONLY);
    if (ret < 0) return ret;
    ret = fstat(src, &st);
    if (ret < 0) return ret;
    strcpy(dstpath, CWD);
    strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
    ret = dst = creat(dstpath, st.st_mode);
    if (ret < 0) return ret;
    while ((size = read(src, buf, BUFSIZ)) > 0) {
        write(dst, buf, size);
        update_progress(size);
        sync_signals();
    }
    close(src);
    close(dst);
    return 0;
}
static int adddir(const char *path) {
    int ret;
    struct stat st;

    ret = stat(CWD, &st);
    if (ret < 0) return ret;
    return mkdir(path, st.st_mode);
}
static int movfile(const char *srcpath) {
    int ret;
    struct stat st;
    char dstpath[PATH_MAX];

    strcpy(dstpath, CWD);
    strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
    ret = rename(srcpath, dstpath);
    if (ret == 0) {
        ret = lstat(dstpath, &st);
        if (ret < 0) return ret;
        update_progress(st.st_size);
    } else if (errno == EXDEV) {
        ret = cpyfile(srcpath);
        if (ret < 0) return ret;
        ret = unlink(srcpath);
    }
    return ret;
}

static void
start_line_edit(const char *init_input)
{
    curs_set(TRUE);
    strncpy(INPUT, init_input, BUFLEN);
    rover.edit.left = mbstowcs(rover.edit.buffer, init_input, BUFLEN);
    rover.edit.right = BUFLEN - 1;
    rover.edit.buffer[BUFLEN] = L'\0';
    rover.edit_scroll = 0;
}

/* Read input and change editing state accordingly. */
static EditStat
get_line_edit()
{
    wchar_t eraser, killer, wch;
    int ret, length;

    ret = rover_get_wch((wint_t *) &wch);
    erasewchar(&eraser);
    killwchar(&killer);
    if (ret == KEY_CODE_YES) {
        if (wch == KEY_ENTER) {
            curs_set(FALSE);
            return CONFIRM;
        } else if (wch == KEY_LEFT) {
            if (EDIT_CAN_LEFT(rover.edit)) EDIT_LEFT(rover.edit);
        } else if (wch == KEY_RIGHT) {
            if (EDIT_CAN_RIGHT(rover.edit)) EDIT_RIGHT(rover.edit);
        } else if (wch == KEY_UP) {
            while (EDIT_CAN_LEFT(rover.edit)) EDIT_LEFT(rover.edit);
        } else if (wch == KEY_DOWN) {
            while (EDIT_CAN_RIGHT(rover.edit)) EDIT_RIGHT(rover.edit);
        } else if (wch == KEY_BACKSPACE) {
            if (EDIT_CAN_LEFT(rover.edit)) EDIT_BACKSPACE(rover.edit);
        } else if (wch == KEY_DC) {
            if (EDIT_CAN_RIGHT(rover.edit)) EDIT_DELETE(rover.edit);
        }
    } else {
        if (wch == L'\r' || wch == L'\n') {
            curs_set(FALSE);
            return CONFIRM;
        } else if (wch == L'\t') {
            curs_set(FALSE);
            return CANCEL;
        } else if (wch == eraser) {
            if (EDIT_CAN_LEFT(rover.edit)) EDIT_BACKSPACE(rover.edit);
        } else if (wch == killer) {
            EDIT_CLEAR(rover.edit);
            clear_message();
        } else if (iswprint(wch)) {
            if (!EDIT_FULL(rover.edit)) EDIT_INSERT(rover.edit, wch);
        }
    }
    /* Encode edit contents in INPUT. */
    rover.edit.buffer[rover.edit.left] = L'\0';
    length = wcstombs(INPUT, rover.edit.buffer, BUFLEN);
    wcstombs(&INPUT[length], &rover.edit.buffer[rover.edit.right+1],
             BUFLEN-length);
    return CONTINUE;
}

/* Update line input on the screen. */
static void
update_input(const char *prompt, Color color)
{
    int plen, ilen, maxlen;

    plen = strlen(prompt);
    ilen = mbstowcs(NULL, INPUT, 0);
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
    mbstowcs(WBUF, INPUT, COLS);
    mvaddnwstr(LINES - 1, plen, &WBUF[rover.edit_scroll], maxlen);
    mvaddch(LINES - 1, plen + MIN(ilen - rover.edit_scroll, maxlen + 1), ' ');
    color_set(DEFAULT, NULL);
    if (rover.edit_scroll)
        mvaddch(LINES - 1, plen - 1, '<');
    if (ilen > rover.edit_scroll + maxlen)
        mvaddch(LINES - 1, plen + maxlen, '>');
    move(LINES - 1, plen + rover.edit.left - rover.edit_scroll);
}

int
main(int argc, char *argv[])
{
    int i, ch;
    char *program;
    char *entry;
    const char *key;
    DIR *d;
    EditStat edit_stat;
    FILE *save_cwd_file = NULL;
    FILE *save_marks_file = NULL;

    if (argc >= 2) {
        if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
            printf("rover %s\n", RV_VERSION);
            return 0;
        } else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            printf(
                "Usage: rover"
                " [-d|--save-cwd FILE]"
                " [-m|--save-marks FILE]"
                " [DIR [DIR [DIR [...]]]]\n"
                "       Browse current directory or the ones specified.\n"
                "       If FILE is given, write last visited path to it.\n\n"
                "  or:  rover -h|--help\n"
                "       Print this help message and exit.\n\n"
                "  or:  rover -v|--version\n"
                "       Print program version and exit.\n\n"
                "See rover(1) for more information.\n\n"
                "Rover homepage: <https://github.com/lecram/rover>.\n"
            );
            return 0;
        } else if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--save-cwd")) {
            if (argc > 2) {
                save_cwd_file = fopen(argv[2], "w");
                argc -= 2; argv += 2;
            } else {
                fprintf(stderr, "error: missing argument to %s\n", argv[1]);
                return 1;
            }
        } else if (!strcmp(argv[1], "-m") || !strcmp(argv[1], "--save-marks")) {
            if (argc > 2) {
                save_marks_file = fopen(argv[2], "a");
                argc -= 2; argv += 2;
            } else {
                fprintf(stderr, "error: missing argument to %s\n", argv[1]);
                return 1;
            }
        }
    }
    init_term();
    rover.nfiles = 0;
    for (i = 0; i < 10; i++) {
        rover.tabs[i].esel = rover.tabs[i].scroll = 0;
        rover.tabs[i].flags = SHOW_FILES | SHOW_DIRS;
    }
    strcpy(rover.tabs[0].cwd, getenv("HOME"));
    for (i = 1; i < argc && i < 10; i++) {
        if ((d = opendir(argv[i]))) {
            realpath(argv[i], rover.tabs[i].cwd);
            closedir(d);
        } else
            strcpy(rover.tabs[i].cwd, rover.tabs[0].cwd);
    }
    getcwd(rover.tabs[i].cwd, PATH_MAX);
    for (i++; i < 10; i++)
        strcpy(rover.tabs[i].cwd, rover.tabs[i-1].cwd);
    for (i = 0; i < 10; i++)
        if (rover.tabs[i].cwd[strlen(rover.tabs[i].cwd) - 1] != '/')
            strcat(rover.tabs[i].cwd, "/");
    rover.tab = 1;
    rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
    init_marks(&rover.marks);
    cd(1);
    while (1) {
        ch = rover_getch();
        key = keyname(ch);
        clear_message();
        if (!strcmp(key, RVK_QUIT)) break;
        else if (ch >= '0' && ch <= '9') {
            rover.tab = ch - '0';
            cd(0);
        } else if (!strcmp(key, RVK_HELP)) {
            ARGS[0] = "man";
            ARGS[1] = "rover";
            ARGS[2] = NULL;
            spawn();
        } else if (!strcmp(key, RVK_DOWN) || !strcmp(key, "KEY_DOWN")) {
            if (!rover.nfiles) continue;
            ESEL = MIN(ESEL + 1, rover.nfiles - 1);
            update_view();
        } else if (!strcmp(key, RVK_UP) || !strcmp(key, "KEY_UP")) {
            if (!rover.nfiles) continue;
            ESEL = MAX(ESEL - 1, 0);
            update_view();
        } else if (!strcmp(key, RVK_JUMP_DOWN)) {
            if (!rover.nfiles) continue;
            ESEL = MIN(ESEL + RV_JUMP, rover.nfiles - 1);
            if (rover.nfiles > HEIGHT)
                SCROLL = MIN(SCROLL + RV_JUMP, rover.nfiles - HEIGHT);
            update_view();
        } else if (!strcmp(key, RVK_JUMP_UP)) {
            if (!rover.nfiles) continue;
            ESEL = MAX(ESEL - RV_JUMP, 0);
            SCROLL = MAX(SCROLL - RV_JUMP, 0);
            update_view();
        } else if (!strcmp(key, RVK_JUMP_TOP)) {
            if (!rover.nfiles) continue;
            ESEL = 0;
            update_view();
        } else if (!strcmp(key, RVK_JUMP_BOTTOM)) {
            if (!rover.nfiles) continue;
            ESEL = rover.nfiles - 1;
            update_view();
        } else if (!strcmp(key, RVK_CD_DOWN) || !strcmp(key, "KEY_RIGHT")) {
            if (!rover.nfiles || !S_ISDIR(EMODE(ESEL))) continue;
            if (chdir(ENAME(ESEL)) == -1) {
                message(RED, "Cannot access \"%s\".", ENAME(ESEL));
                continue;
            }
            strcat(CWD, ENAME(ESEL));
            cd(1);
        } else if (!strcmp(key, RVK_CD_UP) || !strcmp(key, "KEY_LEFT")) {
            char *dirname, first;
            if (!strcmp(CWD, "/")) continue;
            CWD[strlen(CWD) - 1] = '\0';
            dirname = strrchr(CWD, '/') + 1;
            first = dirname[0];
            dirname[0] = '\0';
            cd(1);
            dirname[0] = first;
            dirname[strlen(dirname)] = '/';
            try_to_sel(dirname);
            dirname[0] = '\0';
            if (rover.nfiles > HEIGHT)
                SCROLL = ESEL - HEIGHT / 2;
            update_view();
        } else if (!strcmp(key, RVK_HOME)) {
            strcpy(CWD, getenv("HOME"));
            if (CWD[strlen(CWD) - 1] != '/')
                strcat(CWD, "/");
            cd(1);
        } else if (!strcmp(key, RVK_REFRESH)) {
            reload();
        } else if (!strcmp(key, RVK_SHELL)) {
            program = getenv("SHELL");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = NULL;
                spawn();
                reload();
            }
        } else if (!strcmp(key, RVK_VIEW)) {
            if (!rover.nfiles || S_ISDIR(EMODE(ESEL))) continue;
            program = getenv("PAGER");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = ENAME(ESEL);
                ARGS[2] = NULL;
                spawn();
            }
        } else if (!strcmp(key, RVK_EDIT)) {
            if (!rover.nfiles || S_ISDIR(EMODE(ESEL))) continue;
            program = getenv("EDITOR");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = ENAME(ESEL);
                ARGS[2] = NULL;
                spawn();
                cd(0);
            }
        } else if (!strcmp(key, RVK_SEARCH)) {
            int oldsel, oldscroll, length;
            if (!rover.nfiles) continue;
            oldsel = ESEL;
            oldscroll = SCROLL;
            start_line_edit("");
            update_input(RVP_SEARCH, RED);
            while ((edit_stat = get_line_edit()) == CONTINUE) {
                int sel;
                Color color = RED;
                length = strlen(INPUT);
                if (length) {
                    for (sel = 0; sel < rover.nfiles; sel++)
                        if (!strncmp(ENAME(sel), INPUT, length))
                            break;
                    if (sel < rover.nfiles) {
                        color = GREEN;
                        ESEL = sel;
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
                    ESEL = oldsel;
                    SCROLL = oldscroll;
                }
                update_view();
                update_input(RVP_SEARCH, color);
            }
            if (edit_stat == CANCEL) {
                ESEL = oldsel;
                SCROLL = oldscroll;
            }
            clear_message();
            update_view();
        } else if (!strcmp(key, RVK_TG_FILES)) {
            FLAGS ^= SHOW_FILES;
            reload();
        } else if (!strcmp(key, RVK_TG_DIRS)) {
            FLAGS ^= SHOW_DIRS;
            reload();
        } else if (!strcmp(key, RVK_TG_HIDDEN)) {
            FLAGS ^= SHOW_HIDDEN;
            reload();
        } else if (!strcmp(key, RVK_NEW_FILE)) {
            int ok = 0;
            start_line_edit("");
            update_input(RVP_NEW_FILE, RED);
            while ((edit_stat = get_line_edit()) == CONTINUE) {
                int length = strlen(INPUT);
                ok = length;
                for (i = 0; i < rover.nfiles; i++) {
                    if (
                        !strncmp(ENAME(i), INPUT, length) &&
                        (!strcmp(ENAME(i) + length, "") ||
                         !strcmp(ENAME(i) + length, "/"))
                    ) {
                        ok = 0;
                        break;
                    }
                }
                update_input(RVP_NEW_FILE, ok ? GREEN : RED);
            }
            clear_message();
            if (edit_stat == CONFIRM) {
                if (ok) {
                    addfile(INPUT);
                    cd(1);
                    try_to_sel(INPUT);
                    update_view();
                } else
                    message(RED, "\"%s\" already exists.", INPUT);
            }
        } else if (!strcmp(key, RVK_NEW_DIR)) {
            int ok = 0;
            start_line_edit("");
            update_input(RVP_NEW_DIR, RED);
            while ((edit_stat = get_line_edit()) == CONTINUE) {
                int length = strlen(INPUT);
                ok = length;
                for (i = 0; i < rover.nfiles; i++) {
                    if (
                        !strncmp(ENAME(i), INPUT, length) &&
                        (!strcmp(ENAME(i) + length, "") ||
                         !strcmp(ENAME(i) + length, "/"))
                    ) {
                        ok = 0;
                        break;
                    }
                }
                update_input(RVP_NEW_DIR, ok ? GREEN : RED);
            }
            clear_message();
            if (edit_stat == CONFIRM) {
                if (ok) {
                    adddir(INPUT);
                    cd(1);
                    strcat(INPUT, "/");
                    try_to_sel(INPUT);
                    update_view();
                } else
                    message(RED, "\"%s\" already exists.", INPUT);
            }
        } else if (!strcmp(key, RVK_RENAME)) {
            int ok = 0;
            char *last;
            int isdir;
            strcpy(INPUT, ENAME(ESEL));
            last = INPUT + strlen(INPUT) - 1;
            if ((isdir = *last == '/'))
                *last = '\0';
            start_line_edit(INPUT);
            update_input(RVP_RENAME, RED);
            while ((edit_stat = get_line_edit()) == CONTINUE) {
                int length = strlen(INPUT);
                ok = length;
                for (i = 0; i < rover.nfiles; i++)
                    if (
                        !strncmp(ENAME(i), INPUT, length) &&
                        (!strcmp(ENAME(i) + length, "") ||
                         !strcmp(ENAME(i) + length, "/"))
                    ) {
                        ok = 0;
                        break;
                    }
                update_input(RVP_RENAME, ok ? GREEN : RED);
            }
            clear_message();
            if (edit_stat == CONFIRM) {
                if (isdir)
                    strcat(INPUT, "/");
                if (ok) {
                    if (!rename(ENAME(ESEL), INPUT) && MARKED(ESEL)) {
                        del_mark(&rover.marks, ENAME(ESEL));
                        add_mark(&rover.marks, CWD, INPUT);
                    }
                    cd(1);
                    try_to_sel(INPUT);
                    update_view();
                } else
                    message(RED, "\"%s\" already exists.", INPUT);
            }
        } else if (!strcmp(key, RVK_DELETE)) {
            if (rover.nfiles) {
                message(YELLOW, "Delete \"%s\"? (Y to confirm)", ENAME(ESEL));
                if (rover_getch() == 'Y') {
                    const char *name = ENAME(ESEL);
                    int ret = S_ISDIR(EMODE(ESEL)) ? deldir(name) : delfile(name);
                    reload();
                    if (ret)
                        message(RED, "Could not delete \"%s\".", ENAME(ESEL));
                } else
                    clear_message();
            } else
                  message(RED, "No entry selected for deletion.");
        } else if (!strcmp(key, RVK_TG_MARK)) {
            if (MARKED(ESEL))
                del_mark(&rover.marks, ENAME(ESEL));
            else
                add_mark(&rover.marks, CWD, ENAME(ESEL));
            MARKED(ESEL) = !MARKED(ESEL);
            ESEL = (ESEL + 1) % rover.nfiles;
            update_view();
        } else if (!strcmp(key, RVK_INVMARK)) {
            for (i = 0; i < rover.nfiles; i++) {
                if (MARKED(i))
                    del_mark(&rover.marks, ENAME(i));
                else
                    add_mark(&rover.marks, CWD, ENAME(i));
                MARKED(i) = !MARKED(i);
            }
            update_view();
        } else if (!strcmp(key, RVK_MARKALL)) {
            for (i = 0; i < rover.nfiles; i++)
                if (!MARKED(i)) {
                    add_mark(&rover.marks, CWD, ENAME(i));
                    MARKED(i) = 1;
                }
            update_view();
        } else if (!strcmp(key, RVK_MARK_DELETE)) {
            if (rover.marks.nentries) {
                message(YELLOW, "Delete all marked entries? (Y to confirm)");
                if (rover_getch() == 'Y')
                    process_marked(NULL, delfile, deldir, "Deleting", "Deleted");
                else
                    clear_message();
            } else
                message(RED, "No entries marked for deletion.");
        } else if (!strcmp(key, RVK_MARK_COPY)) {
            if (rover.marks.nentries)
                process_marked(adddir, cpyfile, NULL, "Copying", "Copied");
            else
                message(RED, "No entries marked for copying.");
        } else if (!strcmp(key, RVK_MARK_MOVE)) {
            if (rover.marks.nentries)
                process_marked(adddir, movfile, deldir, "Moving", "Moved");
            else
                message(RED, "No entries marked for moving.");
        }
    }
    if (rover.nfiles)
        free_rows(&rover.rows, rover.nfiles);
    delwin(rover.window);
    if (save_cwd_file != NULL) {
        fputs(CWD, save_cwd_file);
        fclose(save_cwd_file);
    }
    if (save_marks_file != NULL) {
        for (i = 0; i < rover.marks.bulk; i++) {
            entry = rover.marks.entries[i];
            if (entry)
                fprintf(save_marks_file, "%s%s\n", rover.marks.dirpath, entry);
        }
        fclose(save_marks_file);
    }
    free_marks(&rover.marks);
    return 0;
}
