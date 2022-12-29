#include "config.h"
#include "ui_funcs.h"

/* Get user programs from the environment vars */
#define ROVER_ENV(dst, src)                        \
	{                                              \
		if ((dst = getenv("ROVER_" #src)) == NULL) \
			dst = getenv(#src);                    \
	}

static PROCESS deldir = rmdir;

int main(int argc, char *argv[])
{
	int i, ch;
	char buffer_one[PATH_MAX], buffer_two[PATH_MAX], input[PATH_MAX], clipboard[PATH_MAX];
	char *program, *entry, *user_shell, *user_pager, *user_editor, *user_open;
	const char *key, *clip_path;
	DIR *d;
	EditStat edit_stat;
	FILE *save_cwd_file   = NULL, *save_marks_file = NULL, *clip_file;

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

	ROVER_ENV(user_shell, SHELL);
	ROVER_ENV(user_pager, PAGER);
	ROVER_ENV(user_editor, VISUAL);
	if (!user_editor)
		ROVER_ENV(user_editor, EDITOR);
	ROVER_ENV(user_open, OPEN);

	init_term();
	rover.nfiles = 0;
	for (i = 0; i < 10; i++) {
		rover.tabs[i].esel = rover.tabs[i].scroll = 0;
		rover.tabs[i].flags                       = RV_FLAGS;
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
		strcpy(rover.tabs[i].cwd, rover.tabs[i - 1].cwd);
	for (i = 0; i < 10; i++)
		if (rover.tabs[i].cwd[strlen(rover.tabs[i].cwd) - 1] != '/')
			strcat(rover.tabs[i].cwd, "/");
	rover.tab    = 1;
	rover.window = subwin(stdscr, LINES - 2, COLS, 1, 0);
	init_marks(&rover.marks);
	cd(1);
	strcpy(clipboard, CWD);
	if (rover.nfiles > 0)
		strcat(clipboard, ENAME(ESEL));
	while (1) {
		ch  = rover_getch();
		key = keyname(ch);
		clear_message();
		if (!strcmp(key, RVK_QUIT))
			break;
		else if (ch >= '0' && ch <= '9') {
			rover.tab = ch - '0';
			cd(0);
		} else if (!strcmp(key, RVK_HELP)) {
			spawn((char *[]){ "man", "rover", NULL });
		} else if (!strcmp(key, RVK_DOWN)) {
			if (!rover.nfiles)
				continue;
			ESEL = MIN(ESEL + 1, rover.nfiles - 1);
			update_view();
		} else if (!strcmp(key, RVK_UP)) {
			if (!rover.nfiles)
				continue;
			ESEL = MAX(ESEL - 1, 0);
			update_view();
		} else if (!strcmp(key, RVK_JUMP_DOWN)) {
			if (!rover.nfiles)
				continue;
			ESEL = MIN(ESEL + RV_JUMP, rover.nfiles - 1);
			if (rover.nfiles > HEIGHT)
				SCROLL = MIN(SCROLL + RV_JUMP, rover.nfiles - HEIGHT);
			update_view();
		} else if (!strcmp(key, RVK_JUMP_UP)) {
			if (!rover.nfiles)
				continue;
			ESEL   = MAX(ESEL - RV_JUMP, 0);
			SCROLL = MAX(SCROLL - RV_JUMP, 0);
			update_view();
		} else if (!strcmp(key, RVK_JUMP_TOP)) {
			if (!rover.nfiles)
				continue;
			ESEL = 0;
			update_view();
		} else if (!strcmp(key, RVK_JUMP_BOTTOM)) {
			if (!rover.nfiles)
				continue;
			ESEL = rover.nfiles - 1;
			update_view();
		} else if (!strcmp(key, RVK_CD_DOWN)) {
			if (!rover.nfiles || !S_ISDIR(EMODE(ESEL)))
				continue;
			if (chdir(ENAME(ESEL)) == -1) {
				message(RED, "Cannot access \"%s\".", ENAME(ESEL));
				continue;
			}
			strcat(CWD, ENAME(ESEL));
			cd(1);
		} else if (!strcmp(key, RVK_CD_UP)) {
			char *dirname, first;
			if (!strcmp(CWD, "/"))
				continue;
			CWD[strlen(CWD) - 1] = '\0';
			dirname              = strrchr(CWD, '/') + 1;
			first                = dirname[0];
			dirname[0]           = '\0';
			cd(1);
			dirname[0]               = first;
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
		} else if (!strcmp(key, RVK_TARGET)) {
			char *bname, first;
			int is_dir  = S_ISDIR(EMODE(ESEL));
			ssize_t len = readlink(ENAME(ESEL), buffer_one, PATH_MAX -1);
			if (len == -1)
				continue;
			buffer_one[len] = '\0';
			if (access(buffer_one, F_OK) == -1) {
				char *msg;
				switch (errno) {
				case EACCES:
					msg = "Cannot access \"%s\".";
					break;
				case ENOENT:
					msg = "\"%s\" does not exist.";
					break;
				default:
					msg = "Cannot navigate to \"%s\".";
				}
				strcpy(buffer_two, buffer_one); /* message() uses buffer_one. */
				message(RED, msg, buffer_two);
				continue;
			}
			realpath(buffer_one, CWD);
			len = strlen(CWD);
			if (CWD[len - 1] == '/')
				CWD[len - 1] = '\0';
			bname  = strrchr(CWD, '/') + 1;
			first  = *bname;
			*bname = '\0';
			cd(1);
			*bname = first;
			if (is_dir)
				strcat(CWD, "/");
			try_to_sel(bname);
			*bname = '\0';
			update_view();
		} else if (!strcmp(key, RVK_COPY_PATH)) {
			clip_path = getenv("CLIP");
			if (!clip_path)
				goto copy_path_fail;
			clip_file = fopen(clip_path, "w");
			if (!clip_file)
				goto copy_path_fail;
			fprintf(clip_file, "%s%s\n", CWD, ENAME(ESEL));
			fclose(clip_file);
			goto copy_path_done;
copy_path_fail:
			strcpy(clipboard, CWD);
			strcat(clipboard, ENAME(ESEL));
copy_path_done:;
		} else if (!strcmp(key, RVK_PASTE_PATH)) {
			clip_path = getenv("CLIP");
			if (!clip_path)
				goto paste_path_fail;
			clip_file = fopen(clip_path, "r");
			if (!clip_file)
				goto paste_path_fail;
			fscanf(clip_file, "%s\n", clipboard);
			fclose(clip_file);
paste_path_fail:
			strcpy(buffer_one, clipboard);
			strcpy(CWD, dirname(buffer_one));
			if (strcmp(CWD, "/"))
				strcat(CWD, "/");
			cd(1);
			strcpy(buffer_one, clipboard);
			try_to_sel(strstr(clipboard, basename(buffer_one)));
			update_view();
		} else if (!strcmp(key, RVK_REFRESH)) {
			reload();
		} else if (!strcmp(key, RVK_SHELL)) {
			program = user_shell;
			if (program) {
#ifdef RV_SHELL
				spawn((char *[]){ RV_SHELL, "-c", program, NULL });
#else
				spawn((char *[]){ program, NULL });
#endif
				reload();
			}
		} else if (!strcmp(key, RVK_VIEW)) {
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(user_pager, ENAME(ESEL)))
				cd(0);
		} else if (!strcmp(key, RVK_EDIT)) {
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(user_editor, ENAME(ESEL)))
				cd(0);
		} else if (!strcmp(key, RVK_OPEN)) {
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (open_with_env(user_open, ENAME(ESEL)))
				cd(0);
		} else if (!strcmp(key, RVK_SEARCH)) {
			int oldsel, oldscroll, length;
			if (!rover.nfiles)
				continue;
			oldsel    = ESEL;
			oldscroll = SCROLL;
			start_line_edit("");
			update_input(RVP_SEARCH, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int sel;
				Color color = RED;
				length      = strlen(input);
				if (length) {
					for (sel = 0; sel < rover.nfiles; sel++)
						if (!strncmp(ENAME(sel), input, length))
							break;
					if (sel < rover.nfiles) {
						color = GREEN;
						ESEL  = sel;
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
					ESEL   = oldsel;
					SCROLL = oldscroll;
				}
				update_view();
				update_input(RVP_SEARCH, color, input);
			}
			if (edit_stat == CANCEL) {
				ESEL   = oldsel;
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
			update_input(RVP_NEW_FILE, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int length = strlen(input);
				ok         = length;
				for (i = 0; i < rover.nfiles; i++) {
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				}
				update_input(RVP_NEW_FILE, (ok ? GREEN : RED), input);
			}
			clear_message();
			if (edit_stat == CONFIRM) {
				if (ok) {
					if (addfile(input) == 0) {
						cd(1);
						try_to_sel(input);
						update_view();
					} else
						message(RED, "Could not create \"%s\".", input);
				} else
					message(RED, "\"%s\" already exists.", input);
			}
		} else if (!strcmp(key, RVK_NEW_DIR)) {
			int ok = 0;
			start_line_edit("");
			update_input(RVP_NEW_DIR, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int length = strlen(input);
				ok         = length;
				for (i = 0; i < rover.nfiles; i++) {
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				}
				update_input(RVP_NEW_DIR, (ok ? GREEN : RED), input);
			}
			clear_message();
			if (edit_stat == CONFIRM) {
				if (ok) {
					if (adddir(input) == 0) {
						cd(1);
						strcat(input, "/");
						try_to_sel(input);
						update_view();
					} else
						message(RED, "Could not create \"%s/\".", input);
				} else
					message(RED, "\"%s\" already exists.", input);
			}
		} else if (!strcmp(key, RVK_RENAME)) {
			int ok = 0;
			char *last;
			int isdir;
			strcpy(input, ENAME(ESEL));
			last = input + strlen(input) - 1;
			if ((isdir = *last == '/'))
				*last = '\0';
			start_line_edit(input);
			update_input(RVP_RENAME, RED, input);
			while ((edit_stat = get_line_edit()) == CONTINUE) {
				int length = strlen(input);
				ok         = length;
				for (i = 0; i < rover.nfiles; i++)
					if (
						!strncmp(ENAME(i), input, length) &&
						(!strcmp(ENAME(i) + length, "") ||
					     !strcmp(ENAME(i) + length, "/"))) {
						ok = 0;
						break;
					}
				update_input(RVP_RENAME, (ok ? GREEN : RED), input);
			}
			clear_message();
			if (edit_stat == CONFIRM) {
				if (isdir)
					strcat(input, "/");
				if (ok) {
					if (!rename(ENAME(ESEL), input) && MARKED(ESEL)) {
						del_mark(&rover.marks, ENAME(ESEL));
						add_mark(&rover.marks, CWD, input);
					}
					cd(1);
					try_to_sel(input);
					update_view();
				} else
					message(RED, "\"%s\" already exists.", input);
			}
		} else if (!strcmp(key, RVK_TG_EXEC)) {
			if (!rover.nfiles || S_ISDIR(EMODE(ESEL)))
				continue;
			if (S_IXUSR & EMODE(ESEL))
				EMODE(ESEL) &= ~(S_IXUSR | S_IXGRP | S_IXOTH);
			else
				EMODE(ESEL) |= S_IXUSR | S_IXGRP | S_IXOTH;
			if (chmod(ENAME(ESEL), EMODE(ESEL))) {
				message(RED, "Failed to change mode of \"%s\".", ENAME(ESEL));
			} else {
				message(GREEN, "Changed mode of \"%s\".", ENAME(ESEL));
				update_view();
			}
		} else if (!strcmp(key, RVK_DELETE)) {
			if (rover.nfiles) {
				message(YELLOW, "Delete \"%s\"? (Y/n)", ENAME(ESEL));
				if (rover_getch() == 'Y') {
					const char *name = ENAME(ESEL);
					int ret          = ISDIR(ENAME(ESEL)) ? deldir(name) : delfile(name);
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
			ESEL         = (ESEL + 1) % rover.nfiles;
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
				message(YELLOW, "Delete all marked entries? (Y/n)");
				if (rover_getch() == 'Y')
					process_marked(NULL, delfile, deldir, "Deleting", "Deleted");
				else
					clear_message();
			} else
				message(RED, "No entries marked for deletion.");
		} else if (!strcmp(key, RVK_MARK_COPY)) {
			if (rover.marks.nentries) {
				if (strcmp(CWD, rover.marks.dirpath))
					process_marked(adddir, cpyfile, NULL, "Copying", "Copied");
				else
					message(RED, "Cannot copy to the same path.");
			} else
				message(RED, "No entries marked for copying.");
		} else if (!strcmp(key, RVK_MARK_MOVE)) {
			if (rover.marks.nentries) {
				if (strcmp(CWD, rover.marks.dirpath))
					process_marked(adddir, movfile, deldir, "Moving", "Moved");
				else
					message(RED, "Cannot move to the same path.");
			} else
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
