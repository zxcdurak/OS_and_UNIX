#!/bin/sh
set -eu

usage() {
    echo "usage: $0 <source-file>" >&2
    exit 1
}

[ $# -eq 1 ] || usage

srcfile=$1

[ -f "$srcfile" ] || { echo "$0: '$srcfile' is not a regular file" >&2; exit 1; }
[ -r "$srcfile" ] || { echo "$0: '$srcfile' is not readable" >&2; exit 1; }

srcbase=$(basename -- "$srcfile")

case "$srcfile" in
    /*) srcarg=$srcfile ;;
    *)  srcarg="./$srcfile" ;;
esac

outname=$(grep -m1 -o 'Output:[[:space:]]*[^[:space:]]*' -- "$srcfile" | sed 's/^Output:[[:space:]]*//')

if [ -z "$outname" ]; then
    echo "$0: no 'Output:' comment found in '$srcfile'" >&2
    exit 3
fi

TMPDIR=$(mktemp -d) || { echo "$0: failed to create temporary directory" >&2; exit 1; }

cleanup() {
    rc=$?
    trap - EXIT HUP INT QUIT PIPE TERM
    rm -rf -- "$TMPDIR"
    exit "$rc"
}
trap cleanup EXIT HUP INT QUIT PIPE TERM

case "$srcbase" in
    *.c)
        if ! gcc -Wall -Wextra -O2 -o "$TMPDIR/$outname" "$srcarg" 2>"$TMPDIR/build.log"; then
            cat -- "$TMPDIR/build.log" >&2
            exit 2
        fi
        ;;
    *.cpp|*.cc|*.cxx)
        if ! g++ -Wall -Wextra -O2 -o "$TMPDIR/$outname" "$srcarg" 2>"$TMPDIR/build.log"; then
            cat -- "$TMPDIR/build.log" >&2
            exit 2
        fi
        ;;
    *.tex)
        if ! pdflatex -interaction=nonstopmode -halt-on-error \
                -output-directory "$TMPDIR" "$srcarg" >"$TMPDIR/build.log" 2>&1; then
            cat -- "$TMPDIR/build.log" >&2
            exit 2
        fi
        texbase=$(basename -- "$srcbase" .tex)
        if [ ! -s "$TMPDIR/$texbase.pdf" ]; then
            cat -- "$TMPDIR/build.log" >&2
            exit 2
        fi
        if [ "$texbase.pdf" != "$outname" ]; then
            mv -- "$TMPDIR/$texbase.pdf" "$TMPDIR/$outname"
        fi
        ;;
    *)
        echo "$0: unrecognized source file type for '$srcfile'" >&2
        exit 4
        ;;
esac

cp -- "$TMPDIR/$outname" "./$outname"