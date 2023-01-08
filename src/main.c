#include "rover.h"
#include "ui_funcs.h"
#include "os_funcs.h"

struct Rover rover;
char rover_home_path[RV_PATH_MAX];

int main(int argc, char *argv[])
{
	int i, opt, long_index = 0;
	char clipboard[PATH_MAX];
	char *entry;
	DIR *d;
	FILE *save_cwd_file = NULL, *save_marks_file = NULL;
	static struct option long_options[] = {
		{ "version", no_argument, NULL, 'v' },
		{ "help", no_argument, NULL, 'h' },
		{ "save-cwd", required_argument, NULL, 'c' },
		{ "save-marks", required_argument, NULL, 'm' },
		{ NULL, 0, NULL, 0 }
	};

	while ((opt = getopt_long(argc, argv, "vhc:m:", long_options, &long_index)) != -1) {
		switch (opt) {
		case 'v':
			printf("rover %s\n", RV_VERSION);

			return EXIT_SUCCESS;
			break;
		case '?': // in case of missing arguments
			if (save_cwd_file)
				fclose(save_cwd_file);
			if (save_marks_file)
				fclose(save_marks_file);
			fprintf(stderr, "read following instructions:\n");
			getchar();
		case 'h':
			puts("rover - file browser for the terminal\n\n"
			     "SYNOPSIS\n"
			     "\trover [-c|--save-cwd FILE] [-m|--save-marks FILE] [DIR [DIR [DIR [...]]]]\n"
			     "\trover -h|--help\n"
			     "\trover -v|--version\n\n"
			     "DESCRIPTION\n"
			     "\tRover browse current working directory or the ones specified.\n\n"
			     "OPTIONS\n"
			     "\t-c, --save-cwd\n"
			     "\t\twrite last visited path to FILE before exiting.\n\n"
			     "\t-m, --save-marks\n"
			     "\t\tappend path of marked entries to FILE before exiting; if FILE doesn't exist, it'll be created.\n\n"
			     "\t-h, --help\n"
			     "\t\tprint help message and exit.\n\n"
			     "\t-v, --version\n"
			     "\t\tprint program version and exit.\n");

			return EXIT_SUCCESS;
			break;
		case 'c':
			if (isvalidfilename(optarg, false))
				save_cwd_file = fopen(optarg, "w");
			else {
				LOG(LOG_ERR, "\"%s\" invalid filename to store last working directory.", optarg);
				fprintf(stderr, "\"%s\" invalid filename to store last working directory.\n", optarg);

				return EXIT_FAILURE;
			}
			break;
		case 'm':
			if (isvalidfilename(optarg, false))
				save_marks_file = fopen(optarg, "a");
			else {
				LOG(LOG_ERR, "\"%s\" invalid filename to store last marked entries.", optarg);
				fprintf(stderr, "\"%s\" invalid filename to store last marked entries.\n", optarg);

				return EXIT_FAILURE;
			}
			break;
		default:
			LOG(LOG_ERR, "getopt_long() Invalid arguments/options\n");
			fprintf(stderr, "Invalid arguments/options\n");
			return EXIT_FAILURE;
			break;
		}
	}

	init_term();
	INIT_ROVER(rover); //initialize rover struct

	strcpy(rover.tabs[0].cwd, getenv("HOME"));
	opt = (argc - optind);
	for (i = 0; i < opt && i < 10; i++) {
		if ((d = opendir(argv[optind + i]))) {
			realpath(argv[optind + i], rover.tabs[i].cwd);
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
	if (!cd(true))
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
