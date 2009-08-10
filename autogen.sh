#! /bin/sh


AUTOCONF=autoconf;

uname=`uname`;

if [ "$(uname)" == "FreeBSD" -o "$(uname)" == "OpenBSD" ]; then 
    if which autoconf259 >/dev/null; then \
	AUTOCONF=autoconf259;
    fi
fi

autoreconf --install --force  || exit 1
./configure --enable-maintainer-mode "$@"
