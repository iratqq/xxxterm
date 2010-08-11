# $xxxterm: Makefile,v 1.4 2010/06/15 23:50:10 jacekm Exp $

PROG=xxxterm
MAN=xxxterm.1
BINDIR=/usr/local/bin

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

.include <bsd.prog.mk>
