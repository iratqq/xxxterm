# $xxxterm: Makefile,v 1.9 2011/01/03 00:53:15 marco Exp $

PREFIX?=/usr/local
BINDIR=${PREFIX}/bin

PROG=xxxterm
MAN=xxxterm.1

SRCS= xxxterm.c
COPT+= -O2
DEBUG= -ggdb3
LDADD= -lutil
LIBS+= gtk+-2.0
LIBS+= webkit-1.0
LIBS+= libsoup-2.4
GTK_CFLAGS!= pkg-config --cflags $(LIBS)
GTK_LDFLAGS!= pkg-config --libs $(LIBS)
CFLAGS+= $(GTK_CFLAGS) -Wall -pthread
LDFLAGS+= $(GTK_LDFLAGS) -pthread

MANDIR= ${PREFIX}/man/cat

CLEANFILES += javascript.h

javascript.h: hinting.js input-focus.js
	perl ${.CURDIR}/js-merge-helper.pl ${.CURDIR}/hinting.js \
	    ${.CURDIR}/input-focus.js >  ${.CURDIR}/javascript.h

beforeinstall:
	mkdir -p ${PREFIX}/share/xxxterm
	cp ${.CURDIR}/fightsoap16.jpg ${PREFIX}/share/xxxterm
	cp ${.CURDIR}/fightsoap32.jpg ${PREFIX}/share/xxxterm
	cp ${.CURDIR}/fightsoap48.jpg ${PREFIX}/share/xxxterm
	cp ${.CURDIR}/fightsoap64.jpg ${PREFIX}/share/xxxterm
	cp ${.CURDIR}/fightsoap128.jpg ${PREFIX}/share/xxxterm

${PROG} ${OBJS} beforedepend: javascript.h

.include <bsd.prog.mk>
