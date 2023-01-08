# Frequently Asked Questions

## How to use Rover to change the current directory of a shell?

Rover cannot change  the working directory of its  calling shell directly.
However, we can use the option `--save-cwd` to write the last visited path
to a temporary file. Then we can `cd` to that path from the shell itself.

The following shell script can be used to automate this mechanism.
Note that it needs to be sourced directly from the shell.

```
#! /bin/sh

# Based on ranger launcher.

# Usage:
#     . ./cdrover.sh [/path/to/rover]

tempfile="$(mktemp 2> /dev/null || printf "/tmp/rover-cwd.%s" $$)"
if [ $# -gt 0 ]; then
    rover="$1"
    shift
else
    rover="rover"
fi
"$rover" --save-cwd "$tempfile" "$@"
returnvalue=$?
test -f "$tempfile" &&
if [ "$(cat -- "$tempfile")" != "$(echo -n `pwd`)" ]; then
    cd "$(cat "$tempfile")"
fi
rm -f -- "$tempfile"
return $returnvalue
```

## How to open files with appropriate applications?

Rover doesn't have any built-in functionality to associate file types with
applications. This  is delegated  to an external  tool, designated  by the
environmental variable  `$ROVER_OPEN`. This  tool must  be a  command that
takes a filename as argument and runs the appropriate program, opening the
given file.

As an example, the following shell script may be used as `$ROVER_OPEN`:

```
#! /bin/sh

# Usage:
#     ./open.sh /path/to/file

case "$1" in
  *.htm|*.html)
    fmt="elinks %s" ;;
  *.pdf|*.xps|*.cbz|*.epub)
    fmt="mutool draw -F txt %s | less" ;;
  *.ogg|*.flac|*.wav|*.mp3)
    fmt="play %s" ;;
  *.[1-9])
    fmt="man -l %s" ;;
  *.c|*.h|*.sh|*.lua|*.py|*.ml|*[Mm]akefile)
    fmt="vim %s" ;;
  *)
    fmt="less %s"
esac

exec sh -c "$(printf "$fmt" "\"$1\"")"
```
