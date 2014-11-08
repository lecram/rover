#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>  /* ? */
#include <stdio.h>      /* FILENAME_MAX */
#include <locale.h>     /* setlocale(), LC_ALL */
#include <unistd.h>     /* chdir(), getcwd() */
#include <dirent.h>     /* DIR, struct dirent, opendir(), ... */
#include <sys/stat.h>
#include <sys/wait.h>   /* waitpid() */
#include <curses.h>

#include "config.h"

#define ROWSZ 256
char ROW[ROWSZ];
#define STATUSSZ 256
char STATUS[STATUSSZ];
#define SEARCHSZ 256
char SEARCH[SEARCHSZ];
#define MAXARGS 256
char *args[MAXARGS];

typedef enum {DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE} color_t;

#define HEIGHT (LINES-4)

#define SHOW_FILES      0x01u
#define SHOW_DIRS       0x02u
#define SHOW_HIDDEN     0x04u

typedef struct {
    char *name;
    off_t size;
} row_t;

struct rover_t {
    int nfiles;
    int scroll;
    int fsel;
    uint8_t flags;
    row_t *rows;
    WINDOW *window;
    char cwd[FILENAME_MAX];
} rover;

#define FNAME(I) rover.rows[I].name
#define FSIZE(I) rover.rows[I].size

static int
rowcmp(const void *a, const void *b)
{
    int isdir1, isdir2, cmpdir;
    const row_t *r1 = a;
    const row_t *r2 = b;
    isdir1 = strchr(r1->name, '/') != NULL;
    isdir2 = strchr(r2->name, '/') != NULL;
    cmpdir = isdir2 - isdir1;
    /* FIXME: why doesn't `return cmpdir || strcoll(s1, s2)` work here? */
    return cmpdir ? cmpdir : strcoll(r1->name, r2->name);
}

void
free_rows(row_t **rowsp, int nfiles)
{
    int i;

    for (i = 0; i < nfiles; i++)
        free((*rowsp)[i].name);
    free(*rowsp);
    *rowsp = NULL;
}

int
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
        (void) stat(ep->d_name, &statbuf);
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
    (void) closedir(dp);
    *rowsp = rows;
    return n;
}

static void
clean_term()
{
    endwin();
}

static void
init_term()
{
    setlocale(LC_ALL, "");
    initscr();
    cbreak(); /* Get one character at a time. */
    noecho();
    nonl(); /* No NL->CR/NL on output. */
    intrflush(stdscr, FALSE);
    keypad(stdscr, TRUE);
    curs_set(FALSE); /* Hide blinking cursor. */
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

static void
update_browser()
{
    int i, j;
    int ishidden, isdir;

    for (i = 0, j = rover.scroll; i < HEIGHT && j < rover.nfiles; i++, j++) {
        ishidden = FNAME(j)[0] == '.';
        isdir = strchr(FNAME(j), '/') != NULL;
        if (j == rover.fsel)
            wattr_on(rover.window, A_REVERSE, NULL);
        if (ishidden)
            wcolor_set(rover.window, RVC_HIDDEN, NULL);
        else if (isdir)
            wcolor_set(rover.window, RVC_DIR, NULL);
        else
            wcolor_set(rover.window, RVC_FILE, NULL);
        if (!isdir)
            sprintf(ROW, "%s%*d", FNAME(j),
                    COLS - strlen(FNAME(j)) - 2, (int) FSIZE(j));
        else
            strcpy(ROW, FNAME(j));
        (void) mvwhline(rover.window, i + 1, 1, ' ', COLS - 2);
        (void) mvwaddnstr(rover.window, i + 1, 1, ROW, COLS - 2);
        wcolor_set(rover.window, DEFAULT, NULL);
        if (j == rover.fsel)
            wattr_off(rover.window, A_REVERSE, NULL);
    }
    wrefresh(rover.window);
    STATUS[0] = rover.flags & SHOW_FILES  ? 'F' : ' ';
    STATUS[1] = rover.flags & SHOW_DIRS   ? 'D' : ' ';
    STATUS[2] = rover.flags & SHOW_HIDDEN ? 'H' : ' ';
    if (!rover.nfiles)
        strcpy(ROW, "0/0");
    else
        sprintf(ROW, "%d/%d", rover.fsel + 1, rover.nfiles);
    sprintf(STATUS+3, "%*s", 12, ROW);
    color_set(RVC_STATUS, NULL);
    mvaddstr(LINES - 1, COLS - 15, STATUS);
    color_set(DEFAULT, NULL);
    refresh();
}

/* NOTE: The caller needs to write the new path to rover.cwd
 *  *before* calling this function. */
static void
cd()
{
    rover.fsel = 0;
    rover.scroll = 0;
    (void) chdir(rover.cwd);
    (void) mvhline(0, 0, ' ', COLS);
    color_set(RVC_CWD, NULL);
    (void) mvaddnstr(0, 0, rover.cwd, COLS);
    color_set(DEFAULT, NULL);
    if (rover.nfiles)
        free_rows(&rover.rows, rover.nfiles);
    rover.nfiles = ls(rover.cwd, &rover.rows, rover.flags);
    (void) wclear(rover.window);
    wcolor_set(rover.window, RVC_BORDER, NULL);
    wborder(rover.window, 0, 0, 0, 0, 0, 0, 0, 0);
    wcolor_set(rover.window, DEFAULT, NULL);
    update_browser();
}

static void
spawn()
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid > 0) {
        /* fork() succeeded. */
        clean_term();
        (void) waitpid(pid, &status, 0);
        init_term();
        doupdate();
    }
    else if (pid == 0) {
        /* Child process. */
        execvp(args[0], args);
    }
}

int
main()
{
    char *program;
    char *key;

    init_term();
    /* Avoid invalid free() calls in cd() by zeroing the tally. */
    rover.nfiles = 0;
    rover.flags = SHOW_FILES | SHOW_DIRS;
    (void) getcwd(rover.cwd, FILENAME_MAX);
    strcat(rover.cwd, "/");
    rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
    cd();
    while (1) {
        key = keyname(getch());
        if (!strcmp(key, RVK_QUIT))
            break;
        else if (!strcmp(key, RVK_DOWN)) {
            if (!rover.nfiles) continue;
            if (rover.fsel == rover.nfiles - 1)
                rover.scroll = rover.fsel = 0;
            else {
                rover.fsel++;
                if ((rover.fsel - rover.scroll) == HEIGHT)
                    rover.scroll++;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_UP)) {
            if (!rover.nfiles) continue;
            if (rover.fsel == 0) {
                rover.fsel = rover.nfiles - 1;
                rover.scroll = rover.nfiles - HEIGHT;
                if (rover.scroll < 0)
                    rover.scroll = 0;
            }
            else {
                rover.fsel--;
                if (rover.fsel < rover.scroll)
                    rover.scroll--;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_JUMP_DOWN)) {
            if (!rover.nfiles) continue;
            rover.fsel += RV_JUMP;
            if (rover.fsel >= rover.nfiles)
                rover.fsel = rover.nfiles - 1;
            if (rover.nfiles > HEIGHT) {
                rover.scroll += RV_JUMP;
                if (rover.scroll > rover.nfiles - HEIGHT)
                    rover.scroll = rover.nfiles - HEIGHT;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_JUMP_UP)) {
            if (!rover.nfiles) continue;
            rover.fsel -= RV_JUMP;
            if (rover.fsel < 0)
                rover.fsel = 0;
            rover.scroll -= RV_JUMP;
            if (rover.scroll < 0)
                rover.scroll = 0;
            update_browser();
        }
        else if (!strcmp(key, RVK_CD_DOWN)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(rover.fsel), '/') == NULL)
                continue;
            strcat(rover.cwd, FNAME(rover.fsel));
            cd();
        }
        else if (!strcmp(key, RVK_CD_UP)) {
            char *dirname, first;
            if (strlen(rover.cwd) == 1)
                continue;
            rover.cwd[strlen(rover.cwd) - 1] = '\0';
            dirname = strrchr(rover.cwd, '/') + 1;
            first = dirname[0];
            dirname[0] = '\0';
            cd();
            if ((rover.flags & SHOW_DIRS) &&
                ((rover.flags & SHOW_HIDDEN) || (first != '.'))
               ) {
                dirname[0] = first;
                dirname[strlen(dirname)] = '/';
                while (strcmp(FNAME(rover.fsel), dirname))
                    rover.fsel++;
                if (rover.nfiles > HEIGHT) {
                    rover.scroll = rover.fsel - (HEIGHT >> 1);
                    if (rover.scroll < 0)
                        rover.scroll = 0;
                    if (rover.scroll > rover.nfiles - HEIGHT)
                        rover.scroll = rover.nfiles - HEIGHT;
                }
                dirname[0] = '\0';
                update_browser();
            }
        }
        else if (!strcmp(key, RVK_HOME)) {
            strcpy(rover.cwd, getenv("HOME"));
            if (rover.cwd[strlen(rover.cwd) - 1] != '/')
                strcat(rover.cwd, "/");
            cd();
        }
        else if (!strcmp(key, RVK_SHELL)) {
            program = getenv("SHELL");
            if (program) {
                args[0] = program;
                args[1] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_VIEW)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(rover.fsel), '/') != NULL)
                continue;
            program = getenv("PAGER");
            if (program) {
                args[0] = program;
                args[1] = FNAME(rover.fsel);
                args[2] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_EDIT)) {
            if (!rover.nfiles) continue;
            if (strchr(FNAME(rover.fsel), '/') != NULL)
                continue;
            program = getenv("EDITOR");
            if (program) {
                args[0] = program;
                args[1] = FNAME(rover.fsel);
                args[2] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_SEARCH)) {
            if (!rover.nfiles) continue;
            int length, sel, oldsel, oldscroll;
            int ch, erasec, killc;
            color_t color;
            mvaddstr(LINES - 1, 0, "search:");
            oldsel = rover.fsel;
            oldscroll = rover.scroll;
            *SEARCH = '\0';
            length = 0;
            erasec = erasechar();
            killc = killchar();
            while (1) {
                ch = getch();
                if (ch == '\r' || ch == '\n' || ch == KEY_DOWN || ch == KEY_ENTER)
                    break;
                else if (ch == erasec || ch == KEY_LEFT || ch == KEY_BACKSPACE) {
                    if (length)
                        SEARCH[--length] = '\0';
                    if (!length) {
                        rover.fsel = oldsel;
                        rover.scroll = oldscroll;
                    }
                }
                else if (ch == killc) {
                    length = 0;
                    SEARCH[0] = '\0';
                    SEARCH[1] = '\0';
                    rover.fsel = oldsel;
                    rover.scroll = oldscroll;
                }
                else if (length < SEARCHSZ - 2 && isprint(ch)) {
                    SEARCH[length++] = ch;
                    SEARCH[length+1] = '\0';
                }
                if (length) {
                    for (sel = 0; sel < rover.nfiles; sel++)
                        if (!strncmp(FNAME(sel), SEARCH, length))
                            break;
                    if (sel < rover.nfiles) {
                        color = GREEN;
                        rover.fsel = sel;
                        if (rover.nfiles > HEIGHT) {
                            if (sel > rover.nfiles - HEIGHT)
                                rover.scroll = rover.nfiles - HEIGHT;
                            else
                                rover.scroll = sel;
                        }
                    }
                    else
                        color = RED;
                }
                update_browser();
                SEARCH[length] = ' ';
                color_set(color, NULL);
                mvaddstr(LINES - 1, 8, SEARCH);
                color_set(DEFAULT, NULL);
            }
            move(LINES - 1, 0);
            clrtoeol();
            update_browser();
        }
        else if (!strcmp(key, RVK_TG_FILES)) {
            rover.flags ^= SHOW_FILES;
            cd();
        }
        else if (!strcmp(key, RVK_TG_DIRS)) {
            rover.flags ^= SHOW_DIRS;
            cd();
        }
        else if (!strcmp(key, RVK_TG_HIDDEN)) {
            rover.flags ^= SHOW_HIDDEN;
            cd();
        }
    }
    if (rover.nfiles) {
        free_rows(&rover.rows, rover.nfiles);
    }
    delwin(rover.window);
    return 0;
}
