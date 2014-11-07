/* ToDo
 *  - incremental search inside directory;
 *  - filters (show/hide diretories/files, apply glob pattern)
 *  - tabs (only store paths?);
 *  - browsing history (use keys < & > to navigate);
 */

/* POSIX 2008: http://pubs.opengroup.org/onlinepubs/9699919799/toc.htm */

#include <stdlib.h>
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

#define STATUSSZ 256
char STATUS[STATUSSZ];
#define SEARCHSZ 256
char SEARCH[SEARCHSZ];
#define MAXARGS 256
char *args[MAXARGS];

typedef enum {DEFAULT, RED, GREEN, YELLOW, BLUE, CYAN, MAGENTA, WHITE} color_t;

struct skit_t {
    int nfiles;
    int scroll;
    int fsel;
    char **fnames;
    WINDOW *window;
    char cwd[FILENAME_MAX];
} skit;

static int
spcmp(const void *a, const void *b)
{
    int isdir1, isdir2, cmpdir;
    const char *s1 = *(const char **) a;
    const char *s2 = *(const char **) b;
    isdir1 = strchr(s1, '/') != NULL;
    isdir2 = strchr(s2, '/') != NULL;
    cmpdir = isdir2 - isdir1;
    /* FIXME: why doesn't `return cmpdir || strcoll(s1, s2)` work here? */
    return cmpdir ? cmpdir : strcoll(s1, s2);
}

int
ls(char *path, char ***namesp)
{
    DIR *dp;
    struct dirent *ep;
    struct stat statbuf;
    char **names;
    int i, n;

    if((dp = opendir(path)) == NULL)
        return -1;
    n = -2; /* We don't want the entries "." and "..". */
    while (readdir(dp)) n++;
    rewinddir(dp);
    names = (char **) malloc(n * sizeof(char *));
    i = 0;
    while ((ep = readdir(dp))) {
        if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
            continue;
        /* FIXME: ANSI C doesn't have lstat(). How do we handle symlinks? */
        (void) stat(ep->d_name, &statbuf);
        names[i] = (char *) malloc(strlen(ep->d_name) + 2);
        strcpy(names[i], ep->d_name);
        if (S_ISDIR(statbuf.st_mode))
            strcat(names[i], "/");
        i++;
    }
    qsort(names, n, sizeof(char *), spcmp);
    (void) closedir(dp);
    *namesp = names;
    return n;
}

static void
sk_clean()
{
    endwin();
}

static void
sk_init()
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
    atexit(sk_clean);
}

static void
update_browser()
{
    int i, j;

    for (i = 0, j = skit.scroll; i < LINES - 4 && j < skit.nfiles; i++, j++) {
        if (j == skit.fsel)
            wattr_on(skit.window, A_REVERSE, NULL);
        (void) mvwhline(skit.window, i + 1, 1,
                        ' ', COLS - 2);
        (void) mvwaddnstr(skit.window, i + 1, 1,
                          skit.fnames[j], COLS - 2);
        if (j == skit.fsel)
            wattr_off(skit.window, A_REVERSE, NULL);
    }
    wrefresh(skit.window);
    /* C89 doesn't have snprintf(), but a buffer overrun will only occur here
     *  if the number of files reach 10 ^ (STATUSSZ / 2), which is unlikely. */
    sprintf(STATUS, "% 10d/%d", skit.fsel + 1, skit.nfiles);
    mvaddstr(LINES - 1, COLS - strlen(STATUS), STATUS);
    refresh();
}

/* NOTE: The caller needs to write the new path to skit.cwd
 *  *before* calling this function. */
static void
cd()
{
    int i;

    skit.fsel = 0;
    skit.scroll = 0;
    (void) chdir(skit.cwd);
    (void) mvhline(0, 0, ' ', COLS);
    (void) mvaddnstr(0, 0, skit.cwd, COLS);
    for (i = 0; i < skit.nfiles; i++)
        free(skit.fnames[i]);
    if (skit.nfiles)
        free(skit.fnames);
    skit.nfiles = ls(skit.cwd, &skit.fnames);
    (void) wclear(skit.window);
    wborder(skit.window, 0, 0, 0, 0, 0, 0, 0, 0);
    update_browser();
    refresh();
}

static void
spawn()
{
    pid_t pid;
    int status;

    pid = fork();
    if (pid > 0) {
        /* fork() succeeded. */
        sk_clean();
        (void) waitpid(pid, &status, 0);
        sk_init();
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

    sk_init();
    /* Avoid invalid free() calls in cd() by zeroing the tally. */
    skit.nfiles = 0;
    (void) getcwd(skit.cwd, FILENAME_MAX);
    strcat(skit.cwd, "/");
    skit.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
    cd();
    while (1) {
        key = keyname(getch());
        if (!strcmp(key, RVK_QUIT))
            break;
        else if (!strcmp(key, RVK_DOWN)) {
            if (skit.fsel == skit.nfiles - 1)
                skit.scroll = skit.fsel = 0;
            else {
                skit.fsel++;
                if ((skit.fsel - skit.scroll) == (LINES - 4))
                    skit.scroll++;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_UP)) {
            if (skit.fsel == 0) {
                skit.fsel = skit.nfiles - 1;
                skit.scroll = skit.nfiles - LINES + 4;
                if (skit.scroll < 0)
                    skit.scroll = 0;
            }
            else {
                skit.fsel--;
                if (skit.fsel < skit.scroll)
                    skit.scroll--;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_JUMP_DOWN)) {
            skit.fsel += RV_JUMP;
            if (skit.fsel >= skit.nfiles)
                skit.fsel = skit.nfiles - 1;
            if (skit.nfiles > LINES - 4) {
                skit.scroll += RV_JUMP;
                if (skit.scroll > skit.nfiles - LINES + 4)
                    skit.scroll = skit.nfiles - LINES + 4;
            }
            update_browser();
        }
        else if (!strcmp(key, RVK_JUMP_UP)) {
            skit.fsel -= RV_JUMP;
            if (skit.fsel < 0)
                skit.fsel = 0;
            skit.scroll -= RV_JUMP;
            if (skit.scroll < 0)
                skit.scroll = 0;
            update_browser();
        }
        else if (!strcmp(key, RVK_CD_DOWN)) {
            if (strchr(skit.fnames[skit.fsel], '/') == NULL)
                continue;
            strcat(skit.cwd, skit.fnames[skit.fsel]);
            cd();
        }
        else if (!strcmp(key, RVK_CD_UP)) {
            if (strlen(skit.cwd) == 1)
                continue;
            skit.cwd[strlen(skit.cwd) - 1] = '\0';
            *(strrchr(skit.cwd, '/') + 1) = '\0';
            cd();
        }
        else if (!strcmp(key, RVK_HOME)) {
            strcpy(skit.cwd, getenv("HOME"));
            if (skit.cwd[strlen(skit.cwd) - 1] != '/')
                strcat(skit.cwd, "/");
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
        else if (!strcmp(key, RVK_EDIT)) {
            if (strchr(skit.fnames[skit.fsel], '/') != NULL)
                continue;
            program = getenv("EDITOR");
            if (program) {
                args[0] = program;
                args[1] = skit.fnames[skit.fsel];
                args[2] = NULL;
                spawn();
            }
        }
        else if (!strcmp(key, RVK_SEARCH)) {
            int ch, length, sel, oldsel, oldscroll;
            color_t color;
            oldsel = skit.fsel;
            oldscroll = skit.scroll;
            *SEARCH = '\0';
            length = 0;
            while ((ch = getch()) != '\r') {
                switch (ch) {
                    case 8:
                    case 127:
                        if (length)
                            SEARCH[--length] = '\0';
                        if (!length) {
                            skit.fsel = oldsel;
                            skit.scroll = oldscroll;
                        }
                        break;
                    default:
                        if (length < SEARCHSZ - 2)
                            SEARCH[length++] = ch;
                }
                if (length) {
                    for (sel = 0; sel < skit.nfiles; sel++)
                        if (!strncmp(skit.fnames[sel], SEARCH, length))
                            break;
                    if (sel < skit.nfiles) {
                        color = GREEN;
                        skit.fsel = sel;
                        if (skit.nfiles > LINES - 4) {
                            if (sel > skit.nfiles - LINES + 4)
                                skit.scroll = skit.nfiles - LINES + 4;
                            else
                                skit.scroll = sel;
                        }
                    }
                    else
                        color = RED;
                }
                update_browser();
                SEARCH[length] = ' ';
                color_set(color, NULL);
                mvaddstr(LINES - 1, 0, SEARCH);
                color_set(DEFAULT, NULL);
            }
            move(LINES - 1, 0);
            clrtoeol();
            update_browser();
        }
    }
    while (skit.nfiles--) free(skit.fnames[skit.nfiles]);
    free(skit.fnames);
    delwin(skit.window);
    return 0;
}
