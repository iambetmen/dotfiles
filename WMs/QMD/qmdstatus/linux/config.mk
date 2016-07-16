NAME = dwmstatus
VERSION = 1.2

# Customize below to fit your system

# paths
PREFIX = /usr
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/include/X11
X11LIB = /usr/lib64/X11

MPDLIB = -lmpdclient
MPDFLAGS = -DMPD

# includes and libs
INCS = -I. -I/usr/include -I${X11INC}
LIBS = -L/usr/lib64 -lc -L${X11LIB} -lX11 -lasound ${MPDLIB}

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" ${MPDFLAGS}
CFLAGS = -g -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
#CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}
#LDFLAGS = -s ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = gcc -m64

