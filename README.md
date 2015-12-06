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
     j/k or down/up - move cursor down/up
     J/K - move cursor down/up 10 lines
     g/G - move cursor to top/bottom of listing
       l or right - enter selected directory
       h or left - go to parent directory
       H - go to $HOME directory
       r - refresh directory listing
  RETURN - open $SHELL on the current directory
   SPACE - open $PAGER with the selected file
       e - open $EDITOR with the selected file
       / - start incremental search (RETURN to finish)
   f/d/s - toggle file/directory/hidden listing
     n/N - create new file/directory
       R - rename selected file or directory
       D - delete selected file or (empty) directory
       m - toggle mark on the selected entry
       M - toggle mark on all visible entries
       a - mark all visible entries
   X/C/V - delete/copy/move all marked entries
     0-9 - change tab
 ```

**Important Note**: Currently, Rover never asks for confirmation before
overwriting existing files while copying/moving marked entries. Please be
careful to not accidentally lose your data.


Requirements
============

 * Unix-like system;
 * curses library.


Configuration
=============

 Rover configuration (mostly key bindings and colors) can only be changed by
editing the file `config.h` and rebuilding the binary.

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
