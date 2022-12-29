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

 Basic Usage:
 ```
       q - quit Rover
       ? - show Rover manual
     j/k - move cursor down/up
     J/K - move cursor down/up 10 lines
     g/G - move cursor to top/bottom of listing
       l - enter selected directory
       h - go to parent directory
       H - go to $HOME directory
     0-9 - change tab
  RETURN - open $SHELL on the current directory
   SPACE - open $PAGER with the selected file
       e - open $VISUAL or $EDITOR with the selected file
       / - start incremental search (RETURN to finish)
     n/N - create new file/directory
       R - rename selected file or directory
       D - delete selected file or (empty) directory
 ```

 Please read rover(1) for more information.


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
 $ VISUAL=vi rover
 ```

 Rover will first check for variables prefixed  with ROVER_. This can be used to
change Rover behavior without interfering with the global environment:
 ```
 $ ROVER_VISUAL=vi rover
 ```

 Please read rover(1) for more information.


Copying
=======

 All of the source code and documentation for Rover is released into the public
domain and provided without warranty of any kind.
