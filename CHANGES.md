# Change Log

## [0.3.0] - 2015-06-21

### New Features

- Unicode support.
- New commands 'g' & 'G' to jump to top & bottom of listing.
- New option `--save-cwd` to save last visited path to a file before exiting.
- New helper script `rover.sh` to use Rover as "interactive cd".

### Bug Fixes

- Handle symbolic links to directories as such, rather than regular files.
- Add missing make target for uninstalling Rover.
- Fix unsafe behavior on terminal resizing.

## [0.2.0] - 2015-06-03

### New Features

- Better line editing (for search, rename, etc):
  - Allow cursor movement, insertion and deletion.
  - Horizontal scrolling for long lines in small terminals.
- New command 'R' to refresh directory listing.
- New command 'x' to delete selected file or (empty) directory.
- Show file sizes in human readable format ("1.4 K" instead of "1394").
- Set environment variable $RVSEL to selection before running a subprocess.

## [0.1.1] - 2015-04-17

### Bug Fixes

- Fix flashing on slow terminals.
- Fix crash on terminal resizing during subprocess execution.
- Accept relative paths as arguments.
- Don't overwrite default background color.
- When listing symbolic links, show link size instead of target size.
- Remove possible buffer overflows.

## [0.1.0] - 2015-03-07

First version.
