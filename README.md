Introduction
============

 This is a complete review of Rover is a file browser for the terminal.

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
              <F1> or ?   Show this manual.
                  <ESC>   Quit rover.
      Arrow <UP>/<DOWN>   Move cursor up/down.
       Page <UP>/<DOWN>   Move cursor up/down 10 lines.
           <HOME>/<END>   Move cursor to top/bottom of the list.
   Arrow <RIGHT>/<LEFT>   Enter selected directory/Go to parent directory.
                      /   Go to $HOME directory.
                      l   Go to the target of the selected symbolic link.
                      t   Open terminal $SHELL on the current directory.
                   <F6>   Open $PAGER with the selected file.
                   <F7>   Open $VISUAL or $EDITOR with the selected file.
                   <F8>   Open $OPEN with the selected file.
                  F/D/H   Toggle file/directory/hidden listing.
             <F9>/<F12>   Create new file/directory.
                   <F2>   Rename selected file or directory.
                 <CANC>   Delete selected file or (empty) directory.
                <SPACE>   Toggle mark on the selected entry.
 ```

 Please read rover(1) for more information.


Requirements
============

 * Unix-like system;
 * curses library.


Configuration
=============

 Rover configuration (mostly key bindings and colors) can only be changed by
editing the file `rover.h` and rebuilding the binary.

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
