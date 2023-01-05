#include "rover.h"
#include "ui_funcs.h"
#include "os_funcs.h"

struct Rover rover;
char rover_home_path[RV_PATH_MAX];

/* Curses setup. */
static void init_term(void)
{
	setlocale(LC_ALL, "");
	initscr();

	cbreak(); /* Get one character at a time. */
	nodelay(stdscr, TRUE); /* For getch(). */
	noecho();
	nonl(); /* No NL->CR/NL on output. */
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	curs_set(FALSE); /* Hide blinking cursor. */
	raw();

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
	atexit((void (*)(void))endsession);
	handlers(true); //enable handlers
}

static void init_marks(Marks *marks)
{
	strcpy(marks->dirpath, "");
	marks->bulk     = BULK_INIT;
	marks->nentries = 0;
	marks->entries  = (char **)calloc(marks->bulk, sizeof(*marks->entries));
}

static void free_marks(Marks *marks)
{
	int i;

	for (i = 0; marks->nentries && i < marks->bulk; i++)
		if (marks->entries[i]) {
			FREE(marks->entries[i]);
			marks->nentries--;
		}

	FREE(marks->entries);
}

int main(int argc, char *argv[])
{
	int i;
	char clipboard[PATH_MAX];
	char *entry;
	DIR *d;
	FILE *save_cwd_file = NULL, *save_marks_file = NULL;

	if (argc >= 2) {
		if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
			printf("rover %s\n", RV_VERSION);

			return 0;
		} else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
			printf(
				"Usage: rover [OPTIONS] [DIR [DIR [...]]]\n"
				"       Browse current directory or the ones specified.\n\n"
				"  or:  rover -h|--help\n"
				"       Print this help message and exit.\n\n"
				"  or:  rover -v|--version\n"
				"       Print program version and exit.\n\n"
				"See rover(1) for more information.\n"
				"Rover homepage: <https://github.com/lecram/rover>.\n");

			return 0;
		} else if (!strcmp(argv[1], "-d") || !strcmp(argv[1], "--save-cwd")) {
			if (argc > 2) {
				save_cwd_file = fopen(argv[2], "w");
				argc -= 2;
				argv += 2;
			} else {
				fprintf(stderr, "error: missing argument to %s\n", argv[1]);

				return 1;
			}
		} else if (!strcmp(argv[1], "-m") || !strcmp(argv[1], "--save-marks")) {
			if (argc > 2) {
				save_marks_file = fopen(argv[2], "a");
				argc -= 2;
				argv += 2;
			} else {
				fprintf(stderr, "error: missing argument to %s\n", argv[1]);

				return 1;
			}
		}
	}

	init_term();
	rover.nfiles = 0;
	for (i = 0; i < 10; i++) {
		rover.tabs[i].esel   = 0;
		rover.tabs[i].scroll = 0;
		rover.tabs[i].flags  = RV_FLAGS;
	}

	strcpy(rover.tabs[0].cwd, getenv("HOME"));
	for (i = 1; i < argc && i < 10; i++) {
		if ((d = opendir(argv[i]))) {
			realpath(argv[i], rover.tabs[i].cwd);
			closedir(d);
		} else
			strcpy(rover.tabs[i].cwd, rover.tabs[0].cwd);
	}

	getcwd(rover_home_path, RV_PATH_MAX); //get the current work directory for rover.log and rover manual file
	strcpy(rover.tabs[i].cwd, rover_home_path);

	for (i++; i < 10; i++)
		strcpy(rover.tabs[i].cwd, rover.tabs[i - 1].cwd);

	for (i = 0; i < 10; i++)
		ADDSLASH(rover.tabs[i].cwd);

	rover.tab    = 1;
	rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
	init_marks(&rover.marks);
	if(!cd(true))
		exit(EXIT_FAILURE);
		
	strcpy(clipboard, CWD);
	if (rover.nfiles > 0)
		strcat(clipboard, ENAME(ESEL));

	main_menu();

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
	
	return EXIT_SUCCESS;
}
