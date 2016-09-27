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
# it has to be sourced instead of being run
# otherwise when it comes back from the sub shell, 
# cd command does not make any difference in the original shell

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
A better solution would be putting the following function in your `.bashrc`:

```
l () {
  tempfile=$(mktemp 2> /dev/null)
  rover --save-cwd "$tempfile" "$PWD" /some /other /directories
  cd "$(cat $tempfile)"
  rm -f $tempfile
}
```

So that you can type `l` to launch rover to navigate to the desired path and `q` to quit rover but stay in the desired path.

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
