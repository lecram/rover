#!/bin/sh

# Based on ranger launcher.
#
# Usage: ". ./rover.sh [/path/to/rover]"

tempfile="$(mktemp)"
rover="${1:-rover}"
test -z "$1" || shift
"$rover" --save-cwd "$tempfile" "${@:-$(pwd)}"
returnvalue=$?
test -f "$tempfile" &&
if [ "$(cat -- "$tempfile")" != "$(echo -n `pwd`)" ]; then
    cd "$(cat "$tempfile")"
fi
rm -f -- "$tempfile"
return $returnvalue
