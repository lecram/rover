#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>  /* pid_t, ... */
#include <stdio.h>      /* FILENAME_MAX */
#include <locale.h>     /* setlocale(), LC_ALL */
#include <unistd.h>     /* chdir(), getcwd(), read(), close(), ... */
#include <dirent.h>     /* DIR, struct dirent, opendir(), ... */
#include <sys/stat.h>
#include <fcntl.h>      /* open() */
#include <sys/wait.h>   /* waitpid() */
#include <signal.h>     /* struct sigaction, sigaction() */
#include <curses.h>

#include "config.h"

/* String buffers. */
#define ROWSZ 256
static char ROW[ROWSZ];
#define STATUSSZ 256
static char STATUS[STATUSSZ];
#define SEARCHSZ 256
static char SEARCH[SEARCHSZ];

/* Argument buffers for execvp(). */
#define MAXARGS 256
static char *ARGS[MAXARGS];

typedef enum {DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE} color_t;

/* Height of listing view. */
#define HEIGHT (LINES-4)

/* Listing view flags. */
#define SHOW_FILES      0x01u
#define SHOW_DIRS       0x02u
#define SHOW_HIDDEN     0x04u

#define BULK_INIT   5
#define BULK_THRESH 256

/* Information associated to each entry in listing. */
typedef struct {
    char *name;
    off_t size;
    int marked;
} row_t;

typedef struct {
    char dirpath[FILENAME_MAX];
    int bulk;
    int nentries;
    char **entries;
} marks_t;

/* Global state. Some basic info is allocated for ten tabs. */
static struct rover_t {
    int tab;
    int nfiles;
    int scroll[10];
    int fsel[10];
    uint8_t flags[10];
    row_t *rows;
    WINDOW *window;
    char cwd[10][FILENAME_MAX];
    marks_t marks;
} rover;

/* Macros for accessing global state. */
#define FNAME(I)    rover.rows[I].name
#define FSIZE(I)    rover.rows[I].size
#define MARKED(I)   rover.rows[I].marked
#define SCROLL      rover.scroll[rover.tab]
#define FSEL        rover.fsel[rover.tab]
#define FLAGS       rover.flags[rover.tab]
#define CWD         rover.cwd[rover.tab]

typedef int (*PROCESS)(const char *path);

static void
init_marks(marks_t *marks)
{
    strcpy(marks->dirpath, "");
    marks->bulk = BULK_INIT;
    marks->nentries = 0;
    marks->entries = (char **) calloc(marks->bulk, sizeof(char *));
}

static void
mark_none(marks_t *marks)
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
        marks->entries = (char **) calloc(marks->bulk, sizeof(char *));
    }
}

static void
add_mark(marks_t *marks, char *dirpath, char *entry)
{
    int i;

    if (!strcmp(marks->dirpath, dirpath)) {
        /* Append mark to directory. */
        if (marks->nentries == marks->bulk) {
            /* Expand bulk to accomodate new entry. */
            int extra = marks->bulk >> 1;
            marks->bulk += extra; /* bulk *= 1.5; */
            marks->entries = (char **) realloc(
                marks->entries, marks->bulk * sizeof(char *)
            );
            memset(&marks->entries[marks->nentries], 0, extra);
            i = marks->nentries;
        }
        else {
            /* Search for empty slot (there must be one). */
            for (i = 0; i < marks->bulk; i++)
                if (!marks->entries[i])
                    break;
        }
    }
    else {
        /* Directory changed. Discard old marks. */
        mark_none(marks);
        strcpy(marks->dirpath, dirpath);
        i = 0;
    }
    marks->entries[i] = (char *) malloc(strlen(entry) + 1);
    strcpy(marks->entries[i], entry);
    marks->nentries++;
}

static void
del_mark(marks_t *marks, char *entry)
{
    int i;

    if (marks->nentries > 1) {
        for (i = 0; i < marks->bulk; i++)
            if (marks->entries[i] && !strcmp(marks->entries[i], entry))
                break;
        free(marks->entries[i]);
        marks->entries[i] = NULL;
        marks->nentries--;
    }
    else mark_none(marks);
}

static void
finish_marks(marks_t *marks)
{
    int i;

    for (i = 0; i < marks->bulk && marks->nentries; i++)
        if (marks->entries[i]) {
            free(marks->entries[i]);
            marks->nentries--;
        }
    free(marks->entries);
}

/* Curses clean up. Must be called before exiting browser. */
static void
clean_term()
{
    endwin();
}

static void handle_segv(int sig);
static void handle_winch(int sig);

/* Curses setup. */
static void
init_term()
{
    struct sigaction sa;

    setlocale(LC_ALL, "");
    initscr();
    cbreak(); /* Get one character at a time. */
    noecho();
    nonl(); /* No NL->CR/NL on output. */
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    curs_set(FALSE); /* Hide blinking cursor. */
    memset(&sa, 0, sizeof(struct sigaction));
    /* Setup SIGSEGV handler. */
    sa.sa_handler = handle_segv;
    sigaction(SIGSEGV, &sa, NULL);
    /* Setup SIGWINCH handler. */
    sa.sa_handler = handle_winch;
    sigaction(SIGWINCH, &sa, NULL);
    if (has_colors()) {
        start_color();
        init_pair(RED, COLOR_RED, COLOR_BLACK);
        init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
        init_pair(YELLOW, COLOR_YELLOW,COLOR_BLACK);
        init_pair(BLUE, COLOR_BLUE, COLOR_BLACK);
        init_pair(CYAN, COLOR_CYAN, COLOR_BLACK);
        init_pair(MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);
    }
    atexit(clean_term);
}

/* Update the listing view. */
static void
update()
{
    int i, j;
    int ishidden, isdir;
    int marking;

    mvhline(0, 0, ' ', COLS);
    color_set(RVC_CWD, NULL);
    mvaddnstr(0, 0, CWD, COLS);
    color_set(DEFAULT, NULL);
    attr_on(A_BOLD, NULL);
    color_set(RVC_TABNUM, NULL);
    mvaddch(0, COLS-4, rover.tab + '0');
    color_set(DEFAULT, NULL);
    attr_off(A_BOLD, NULL);
    wclear(rover.window);
    wcolor_set(rover.window, RVC_BORDER, NULL);
    wborder(rover.window, 0, 0, 0, 0, 0, 0, 0, 0);
    wcolor_set(rover.window, DEFAULT, NULL);
    /* Selection might not be visible, due to window shrinking.
       In that case the scroll must be moved to make it visible again. */
    if (FSEL < SCROLL)
        SCROLL = FSEL;
    else if (FSEL >= SCROLL + HEIGHT)
        SCROLL = FSEL - HEIGHT + 1;
    marking = !strcmp(CWD, rover.marks.dirpath);
    for (i = 0, j = SCROLL; i < HEIGHT && j < rover.nfiles; i++, j++) {
        ishidden = FNAME(j)[0] == '.';
        isdir = strchr(FNAME(j), '/') != NULL;
        if (j == FSEL)
            wattr_on(rover.window, A_REVERSE, NULL);
        if (ishidden)
            wcolor_set(rover.window, RVC_HIDDEN, NULL);
        else if (isdir)
            wcolor_set(rover.window, RVC_DIR, NULL);
        else
            wcolor_set(rover.window, RVC_FILE, NULL);
        if (!isdir)
            sprintf(ROW, "%s%*d", FNAME(j),
                    COLS - strlen(FNAME(j)) - 4, (int) FSIZE(j));
        else
            strcpy(ROW, FNAME(j));
        mvwhline(rover.window, i + 1, 1, ' ', COLS - 2);
        if (marking && MARKED(j))
            mvwaddch(rover.window, i + 1, 1, ACS_DIAMOND);
        else
            mvwaddch(rover.window, i + 1, 1, ' ');
        mvwaddnstr(rover.window, i + 1, 3, ROW, COLS - 4);
        wcolor_set(rover.window, DEFAULT, NULL);
        if (j == FSEL)
            wattr_off(rover.window, A_REVERSE, NULL);
    }
    if (rover.nfiles > HEIGHT) {
        int center, height;
        center = (SCROLL + (HEIGHT >> 1)) * HEIGHT / rover.nfiles;
        height = (HEIGHT-1) * HEIGHT / rover.nfiles;
        if (!height) height = 1;
        wcolor_set(rover.window, RVC_BORDER, NULL);
        wborder(rover.window, 0, 0, 0, 0, 0, 0, 0, 0);
        wcolor_set(rover.window, RVC_SCROLLBAR, NULL);
        mvwvline(rover.window, center-(height>>1)+1, COLS-1, RVS_SCROLLBAR, height);
        wcolor_set(rover.window, DEFAULT, NULL);
    }
    wrefresh(rover.window);
    STATUS[0] = FLAGS & SHOW_FILES  ? 'F' : ' ';
    STATUS[1] = FLAGS & SHOW_DIRS   ? 'D' : ' ';
    STATUS[2] = FLAGS & SHOW_HIDDEN ? 'H' : ' ';
    if (!rover.nfiles)
        strcpy(ROW, "0/0");
    else
        sprintf(ROW, "%d/%d", FSEL + 1, rover.nfiles);
    sprintf(STATUS+3, "%*s", 12, ROW);
    color_set(RVC_STATUS, NULL);
    mvaddstr(LINES - 1, COLS - 16, STATUS);
    color_set(DEFAULT, NULL);
    refresh();
}

/* SIGSEGV handler: clean up curses before exiting. */
static void
handle_segv(int sig)
{
    (void) sig;
    clean_term();
    puts("Received SIGSEGV (segmentation fault).");
    exit(1);
}

/* SIGWINCH handler: resize application according to new terminal settings. */
static void
handle_winch(int sig)
{
    (void) sig;
    delwin(rover.window);
    endwin();
    refresh();
    clear();
    rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
    update();
}

/* Comparison used to sort listing entries. */
static int
rowcmp(const void *a, const void *b)
{
    int isdir1, isdir2, cmpdir;
    const row_t *r1 = a;
    const row_t *r2 = b;
    isdir1 = strchr(r1->name, '/') != NULL;
    isdir2 = strchr(r2->name, '/') != NULL;
    cmpdir = isdir2 - isdir1;
    return cmpdir ? cmpdir : strcoll(r1->name, r2->name);
}

/* Get all entries for a given path (usually cwd). */
static int
ls(char *path, row_t **rowsp, uint8_t flags)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    row_t *rows;
    int i, n;

    if((dp = opendir(path)) == NULL)
        return -1;
    n = -2; /* We don't want the entries "." and "..". */
    while (readdir(dp)) n++;
    rewinddir(dp);
    rows = (row_t *) malloc(n * sizeof(row_t));
    i = 0;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        if (!(flags & SHOW_HIDDEN) && ep->d_name[0] == '.')
            continue;
        /* FIXME: ANSI C doesn't have lstat(). How do we handle symlinks? */
        stat(ep->d_name, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            if (flags & SHOW_DIRS) {
                rows[i].name = (char *) malloc(strlen(ep->d_name) + 2);
                strcpy(rows[i].name, ep->d_name);
                strcat(rows[i].name, "/");
                i++;
            }
        }
        else if (flags & SHOW_FILES) {
            rows[i].name = (char *) malloc(strlen(ep->d_name) + 1);
            strcpy(rows[i].name, ep->d_name);
            rows[i].size = statbuf.st_size;
            i++;
        }
    }
    n = i; /* Ignore unused space in array caused by filters. */
    qsort(rows, n, sizeof(row_t), rowcmp);
    closedir(dp);
    *rowsp = rows;
    return n;
}

/* Deallocate entries. */
static void
free_rows(row_t **rowsp, int nfiles)
{
    int i;

    for (i = 0; i < nfiles; i++)
        free((*rowsp)[i].name);
    free(*rowsp);
    *rowsp = NULL;
}

/* Change working directory. */
/* NOTE: The caller needs to write the new path to CWD
 *  *before* calling this function. */
static void
cd(int reset)
{
    int i, j;

    if (reset)
        FSEL = SCROLL = 0;
    chdir(CWD);
    if (rover.nfiles)
        free_rows(&rover.rows, rover.nfiles);
    rover.nfiles = ls(CWD, &rover.rows, FLAGS);
    if (!strcmp(CWD, rover.marks.dirpath)) {
        for (i = 0; i < rover.nfiles; i++) {
            for (j = 0; j < rover.marks.bulk; j++)
                if (
                    rover.marks.entries[j] &&
                    !strcmp(rover.marks.entries[j], FNAME(i))
                )
                    break;
            MARKED(i) = j < rover.marks.bulk;
        }
    }
    else
        for (i = 0; i < rover.nfiles; i++)
            MARKED(i) = 0;
    update();
}

static void
process_dir(PROCESS pre, PROCESS proc, PROCESS pos, const char *path)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    char subpath[FILENAME_MAX];

    if (pre) {
        char dstpath[FILENAME_MAX];
        strcpy(dstpath, CWD);
        strcat(dstpath, path + strlen(rover.marks.dirpath));
        pre(dstpath);
    }
    if((dp = opendir(path)) == NULL)
        return;
    while ((ep = readdir(dp))) {
        if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
            continue;
        sprintf(subpath, "%s%s", path, ep->d_name);
        stat(subpath, &statbuf);
        if (S_ISDIR(statbuf.st_mode)) {
            strcat(subpath, "/");
            process_dir(pre, proc, pos, subpath);
        }
        else
            proc(subpath);
    }
    closedir(dp);
    if (pos) pos(path);
}

static void
process_marked(PROCESS pre, PROCESS proc, PROCESS pos)
{
    int i;
    char path[FILENAME_MAX];

    for (i = 0; i < rover.marks.bulk; i++)
        if (rover.marks.entries[i]) {
            sprintf(path, "%s%s", rover.marks.dirpath, rover.marks.entries[i]);
            if (strchr(rover.marks.entries[i], '/'))
                process_dir(pre, proc, pos, path);
            else
                proc(path);
        }
}

/* Wrappers for file operations. */
static PROCESS delfile = unlink;
static PROCESS deldir = rmdir;
static int cpyfile(const char *srcpath) {
    int src, dst, ret;
    size_t size;
    struct stat st;
    char buf[BUFSIZ];
    char dstpath[FILENAME_MAX];

    ret = src = open(srcpath, O_RDONLY);
    if (ret < 0) return ret;
    ret = fstat(src, &st);
    if (ret < 0) return ret;
    strcpy(dstpath, CWD);
    strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
    ret = dst = creat(dstpath, st.st_mode);
    if (ret < 0) return ret;
    while ((size = read(src, buf, BUFSIZ)) > 0)
        write(dst, buf, size);
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
    char dstpath[FILENAME_MAX];

    strcpy(dstpath, CWD);
    strcat(dstpath, srcpath + strlen(rover.marks.dirpath));
    return rename(srcpath, dstpath);
}

/* Do a fork-exec to external program (e.g. $EDITOR). */
static void
spawn()
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid > 0) {
        /* fork() succeeded. */
        clean_term();
        waitpid(pid, &status, 0);
        init_term();
        doupdate();
    }
    else if (pid == 0) {
        /* Child process. */
        execvp(ARGS[0], ARGS);
    }
}

/* Interactive getstr(). */
static int
igetstr(char *buffer, int maxlen)
{
    int ch, length;

    length = strlen(buffer);
    ch = getch();
    if (ch == '\r' || ch == '\n' || ch == KEY_DOWN || ch == KEY_ENTER)
        return 0;
    else if (ch == erasechar() || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
        if (length)
            buffer[--length] = '\0';
    }
    else if (ch == killchar()) {
        length = 0;
        buffer[0] = '\0';
    }
    else if (length < maxlen - 1 && isprint(ch)) {
        buffer[length++] = ch;
        buffer[length] = '\0';
    }
    return 1;
}

int
main(int argc, char *argv[])
{
    int i, ch;
    char *program, *key;
    DIR *d;

    init_term();
    /* Avoid invalid free() calls in cd() by zeroing the tally. */
    rover.nfiles = 0;
    for (i = 0; i < 10; i++) {
        rover.fsel[i] = rover.scroll[i] = 0;
        rover.flags[i] = SHOW_FILES | SHOW_DIRS;
    }
    strcpy(rover.cwd[0], getenv("HOME"));
    for (i = 1; i < argc && i < 10; i++) {
        d = opendir(argv[i]);
        if (d) {
            strcpy(rover.cwd[i], argv[i]);
            closedir(d);
        }
        else strcpy(rover.cwd[i], rover.cwd[0]);
    }
    getcwd(rover.cwd[i], FILENAME_MAX);
    for (i++; i < 10; i++)
        strcpy(rover.cwd[i], rover.cwd[i-1]);
    for (i = 0; i < 10; i++)
        if (rover.cwd[i][strlen(rover.cwd[i]) - 1] != '/')
            strcat(rover.cwd[i], "/");
    rover.tab = 1;
    rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
    init_marks(&rover.marks);
    cd(1);
    while (1) {
        ch = getch();
        key = keyname(ch);
        if (!strcmp(key, RVK_QUIT))
            break;
        else if (ch >= '0' && ch <= '9') {
            rover.tab = ch - '0';
            cd(0);
        }
        else if (!strcmp(key, RVK_DOWN)) {
            if (!rover.nfiles) continue;
            if (FSEL == rover.nfiles - 1)
                SCROLL = FSEL = 0;
            else {
                FSEL++;
                if ((FSEL - SCROLL) == HEIGHT)
                    SCROLL++;
            }
            update();
        }
        else if (!strcmp(key, RVK_UP)) {
            if (!rover.nfiles) continue;
            if (FSEL == 0) {
                FSEL = rover.nfiles - 1;
                SCROLL = rover.nfiles - HEIGHT;
                if (SCROLL < 0)
                    SCROLL = 0;
            }
            else {
                FSEL--;
                if (FSEL < SCROLL)
                    SCROLL--;
            }
            update();
        }
        else if (!strcmp(key, RVK_JUMP_DOWN)) {
            if (!rover.nfiles) continue;
            FSEL += RV_JUMP;
            if (FSEL >= rover.nfiles)
                FSEL = rover.nfiles - 1;
            if (rover.nfiles > HEIGHT) {
                SCROLL += RV_JUMP;
                if (SCROLL > rover.nfiles - HEIGHT)
                    SCROLL = rover.nfiles - HEIGHT;
            }
            update();
        }
        else if (!strcmp(key, RVK_JUMP_UP)) {
            if (!rover.nfiles) continue;
            FSEL -= RV_JUMP;
            if (FSEL < 0)
                FSEL = 0;
            SCROLL -= RV_JUMP;
            if (SCROLL < 0)
                SCROLL = 0;
            update();
        }
        else if (!strcmp(key, RVK_CD_DOWN)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(FSEL), '/') == NULL)
                continue;
            strcat(CWD, FNAME(FSEL));
            cd(1);
        }
        else if (!strcmp(key, RVK_CD_UP)) {
            char *dirname, first;
            if (strlen(CWD) == 1)
                continue;
            CWD[strlen(CWD) - 1] = '\0';
            dirname = strrchr(CWD, '/') + 1;
            first = dirname[0];
            dirname[0] = '\0';
            cd(1);
            if ((FLAGS & SHOW_DIRS) &&
                ((FLAGS & SHOW_HIDDEN) || (first != '.'))
               ) {
                dirname[0] = first;
                dirname[strlen(dirname)] = '/';
                while (strcmp(FNAME(FSEL), dirname))
                    FSEL++;
                if (rover.nfiles > HEIGHT) {
                    SCROLL = FSEL - (HEIGHT >> 1);
                    if (SCROLL < 0)
                        SCROLL = 0;
                    if (SCROLL > rover.nfiles - HEIGHT)
                        SCROLL = rover.nfiles - HEIGHT;
                }
                dirname[0] = '\0';
                update();
            }
        }
        else if (!strcmp(key, RVK_HOME)) {
            strcpy(CWD, getenv("HOME"));
            if (CWD[strlen(CWD) - 1] != '/')
                strcat(CWD, "/");
            cd(1);
        }
        else if (!strcmp(key, RVK_SHELL)) {
            program = getenv("SHELL");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_VIEW)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(FSEL), '/') != NULL)
                continue;
            program = getenv("PAGER");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = FNAME(FSEL);
                ARGS[2] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_EDIT)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(FSEL), '/') != NULL)
                continue;
            program = getenv("EDITOR");
            if (program) {
                ARGS[0] = program;
                ARGS[1] = FNAME(FSEL);
                ARGS[2] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_SEARCH)) {
            int oldsel, oldscroll;
            if (!rover.nfiles) continue;
            oldsel = FSEL;
            oldscroll = SCROLL;
            *SEARCH = '\0';
            color_set(RVC_PROMPT, NULL);
            mvaddstr(LINES - 1, 0, "search: ");
            curs_set(TRUE);
            color_set(DEFAULT, NULL);
            while (igetstr(SEARCH, SEARCHSZ)) {
                int length, sel;
                color_t color;
                length = strlen(SEARCH);
                if (length) {
                    for (sel = 0; sel < rover.nfiles; sel++)
                        if (!strncmp(FNAME(sel), SEARCH, length))
                            break;
                    if (sel < rover.nfiles) {
                        color = GREEN;
                        FSEL = sel;
                        if (rover.nfiles > HEIGHT) {
                            if (sel < 3)
                                SCROLL = 0;
                            else if (sel - 3 > rover.nfiles - HEIGHT)
                                SCROLL = rover.nfiles - HEIGHT;
                            else
                                SCROLL = sel - 3;
                        }
                    }
                    else
                        color = RED;
                }
                else {
                    FSEL = oldsel;
                    SCROLL = oldscroll;
                }
                update();
                color_set(color, NULL);
                mvaddstr(LINES - 1, 8, SEARCH);
                mvaddch(LINES - 1, length + 8, ' ');
                move(LINES - 1, length + 8);
                color_set(DEFAULT, NULL);
            }
            curs_set(FALSE);
            move(LINES - 1, 0);
            clrtoeol();
            update();
        }
        else if (!strcmp(key, RVK_TG_FILES)) {
            FLAGS ^= SHOW_FILES;
            cd(1);
        }
        else if (!strcmp(key, RVK_TG_DIRS)) {
            FLAGS ^= SHOW_DIRS;
            cd(1);
        }
        else if (!strcmp(key, RVK_TG_HIDDEN)) {
            FLAGS ^= SHOW_HIDDEN;
            cd(1);
        }
        else if (!strcmp(key, RVK_TG_MARK)) {
            if (MARKED(FSEL))
                del_mark(&rover.marks, FNAME(FSEL));
            else
                add_mark(&rover.marks, CWD, FNAME(FSEL));
            MARKED(FSEL) = !MARKED(FSEL);
            FSEL = (FSEL + 1) % rover.nfiles;
            update();
        }
        else if (!strcmp(key, RVK_INVMARK)) {
            for (i = 0; i < rover.nfiles; i++) {
                if (MARKED(i))
                    del_mark(&rover.marks, FNAME(i));
                else
                    add_mark(&rover.marks, CWD, FNAME(i));
                MARKED(i) = !MARKED(i);
            }
            update();
        }
        else if (!strcmp(key, RVK_MARKALL)) {
            for (i = 0; i < rover.nfiles; i++)
                if (!MARKED(i)) {
                    add_mark(&rover.marks, CWD, FNAME(i));
                    MARKED(i) = 1;
                }
            update();
        }
        else if (!strcmp(key, RVK_DELETE)) {
            process_marked(NULL, delfile, deldir);
            mark_none(&rover.marks);
            cd(1);
        }
        else if (!strcmp(key, RVK_COPY)) {
            process_marked(adddir, cpyfile, NULL);
            mark_none(&rover.marks);
            cd(1);
        }
        else if (!strcmp(key, RVK_MOVE)) {
            process_marked(adddir, movfile, NULL);
            mark_none(&rover.marks);
            cd(1);
        }
    }
    if (rover.nfiles)
        free_rows(&rover.rows, rover.nfiles);
    finish_marks(&rover.marks);
    delwin(rover.window);
    return 0;
}
