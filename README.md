Introduction
============

 Rover is an interactive application for file system navigation.

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

 Running:
 ```
 $ ./rover
 ```

 Using:
 ```
       q - quit Rover
     j/k - move cursor up/down
     J/K - move cursor up/down 10 times
       l - enter selected directory
       h - go to parent directory
       H - go to $HOME directory
  RETURN - open $SHELL on the current directory
   SPACE - open $PAGER on the selected file
       e - open $EDITOR on the selected file
       / - start incremental search (RETURN to finish)
       f - toggle file listing
       d - toggle directory listing
       s - toggle hidden file/directory listing
 ```


Dependencies
============

 Rover is supposed to run on any Unix-like system with a curses implementation.
To build Rover, you need a C compiler (supporting at least C89) and a `curses.h`
header file.

Configuration
=============

 Rover configuration (mostly key bindings and colors) can only be changed
by editing the file `config.h` and rebuilding the binary (with `make`).

 Note that the external programs executed by some Rover commands may be changed
via the appropriated environment variables. For example, to specify an editor:
 ```
 $ EDITOR=vi ./rover
 ```

 You may also want to "install" Rover by copying it somewhere on your $PATH. The
only file that needs to be copied is `rover`:
 ```
 $ cp rover /usr/bin
 $ # Now the "./" prefix is no longer necessary.
 $ rover
 ```


Copyright
=========

 All of the code and documentation in Rover has been dedicated to the
   public domain.
