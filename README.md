Introduction
============

 Rover is a file browser for the terminal.

![Rover screenshot](/../screenshots/screenshot.png?raw=true "Screenshot")

 The main goal is to provide a faster way to explore a file system from the
terminal, compared to what's possible by using `cd`, `ls`, etc. Rover has
vi-like key bindings for navigation and can open files in $PAGER and $EDITOR.
Basic file system operations are also implemented (see rover(1) for details).
Rover is designed to be simple, fast and portable.


Quick Start
===========

 Building and Installing:
 ```
 $ make
 $ sudo make install
 ```

 Running:
 ```
 $ rover [DIR1 [DIR2 [DIR3 [...]]]]
 ```

 Using:
 ```
       q - quit Rover
       ? - show Rover manual
     j/k - move cursor down/up
     J/K - move cursor down/up 10 lines
     g/G - move cursor to top/bottom of listing
       l - enter selected directory
       h - go to parent directory
       H - go to $HOME directory
       R - refresh directory listing
  RETURN - open $SHELL on the current directory
   SPACE - open $PAGER with the selected file
       e - open $EDITOR with the selected file
       / - start incremental search (RETURN to finish)
   f/d/s - toggle file/directory/hidden listing
     n/N - create new file/directory
       r - rename selected file or directory
       x - delete selected file or (empty) directory
       m - toggle mark on the selected entry
       M - toggle mark on all visible entries
       a - mark all visible entries
   X/C/V - delete/copy/move all marked entries
     0-9 - change tab
 ```

**Important Note**: Currently, Rover never asks for confirmation before
overwriting existing files while copying/moving marked entries. Please be
careful to not accidentally lose your data.


Dependencies
============

 Rover is supposed to run on any Unix-like system with a curses implementation.
To build Rover, you need a C compiler and a curses library with the corresponding
header file. A makefile is provided, but since all the code is in a single C
source file, it shouldn't be hard to build Rover without make(1).


Configuration
=============

 By default, rover is installed to `/usr/local/bin/rover`. To change this and other
build options, such as the name of the curses library, please edit `Makefile`
before executing `make` or specify the options during invocation. For example,
to link against `libncurses.so` and install to `/opt/bin/rover`:
 ```
 $ make LDLIBS=-lncurses PREFIX=/opt install
 ```

 Rover runtime configuration (mostly key bindings and colors) can only be
changed by editing the file `config.h` and rebuilding the binary.

 Note that the external programs executed by some Rover commands may be changed
via the appropriate environment variables. For example, to specify an editor:
 ```
 $ EDITOR=vi rover
 ```

 Please read rover(1) for more information.


Copying
=======

 All of the source code and documentation for Rover is released into the public
domain and provided without warranty of any kind.
