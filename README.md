Introduction
============

 Rover is a file browser for the terminal.

![Rover screenshot](/../screenshots/screenshot.png?raw=true "Screenshot")

 The main goal is to provide a faster way to explore a file system from the
terminal, compared to what's possible by using `cd`, `ls`, etc. Rover
is designed to be simple and portable. It was originally written to be
used on a headless Raspberry Pi accessed via ssh. The [Ranger file manager](http://ranger.nongnu.org/)
was a major inspiration for the user interface design, but Rover has
significantly less features and dependencies.


Quick Start
===========

 Building:
 ```
 $ make
 ```

 Installing:
 ```
 $ sudo make install
 ```

 Running:
 ```
 $ rover
 ```

 Specify path for some (up to 9) tabs at startup:
 ```
 $ rover [DIR1 [DIR2 [DIR3 [...]]]]
 ```

 Using:
 ```
       q - quit Rover
     j/k - move cursor down/up
     J/K - move cursor down/up 10 lines
       l - enter selected directory
       h - go to parent directory
       H - go to $HOME directory
  RETURN - open $SHELL on the current directory
   SPACE - open $PAGER with the selected file
       e - open $EDITOR with the selected file
       / - start incremental search (RETURN to finish)
   f/d/s - toggle file/directory/hidden listing
     n/N - create new file/directory
       r - rename selected file or directory
       m - toggle mark on the selected entry
       M - toggle mark on all visible entries
       a - mark all visible entries
   X/C/V - delete/copy/move all marked entries
     0-9 - change tab
 ```

**Important Note**: Currently, Rover never asks for confirmation on any file
operation. Please be careful to not accidentally remove/overwrite your files.


Dependencies
============

 Rover is supposed to run on any Unix-like system with a curses implementation.
To build Rover, you need a C compiler (supporting at least C89) and a `curses.h`
header file.


Configuration
=============

 By default, rover is installed to `/usr/local/bin/rover`. To change this and other
build options, such as the name of the curses library, please edit `Makefile`
before executing `make` or specify the options during invocation. For example,
to link against `libncurses.so` and install to `/opt/bin/rover`:
 ```
 make LDLIBS=-lncurses PREFIX=/opt install
 ```

 Rover configuration (mostly key bindings and colors) can only be changed
by editing the file `config.h` and rebuilding the binary (with `make`).

 Note that the external programs executed by some Rover commands may be changed
via the appropriate environment variables. For example, to specify an editor:
 ```
 $ EDITOR=vi rover
 ```


Copyright
=========

 All of the code and documentation in Rover has been dedicated to the
   public domain.
