#!/bin/sh

# Based on ranger launcher.
#
# Usage: ". ./rover.sh [/path/to/rover]"

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
