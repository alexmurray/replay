#!/bin/sh
# Run this to generate all the initial makefiles, etc.

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

PKG_NAME="replay"

(test -f $srcdir/configure.ac) || {
    echo -n "**Error**: Directory "\`$srcdir\'" does not look like the"
    echo " top-level $PKG_NAME directory"
    exit 1
}

which gnome-autogen.sh || {
	echo "Required package gnome-common not found. Please install gnome-common before running this script"
	exit 1
}

GTKDOCIZE="gtkdocize --flavour no-tmpl" REQUIRED_AUTOMAKE_VERSION=1.10 USE_GNOME2_MACROS=1 . gnome-autogen.sh
