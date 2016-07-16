NAME = dwmstatus
VERSION = 1.2

# Customize below to fit your system

# paths
PREFIX = /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/local/include/X11
X11LIB = /usr/local/lib/X11

MPDLIB = -lmpdclient
MPDFLAGS = -DMPD

# includes and libs
INCS = -I. -I/usr/local/include -I${X11INC}
LIBS = -L/usr/local/lib -lc -L${X11LIB} -lX11 -lasound ${MPDLIB}

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" ${MPDFLAGS}
#CFLAGS = -g -std=c99 -pedantic -Wall -O2 ${INCS} ${CPPFLAGS}
CFLAGS = -std=c99 -pedantic -Wall -Wextra -Os ${INCS} ${CPPFLAGS}
#LDFLAGS = -g ${LIBS}
LDFLAGS = -s ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = clang

