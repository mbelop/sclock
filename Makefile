# $Id: Makefile,v 1.6 2007/05/24 07:07:28 pyr Exp $

PREFIX	?=	/usr/local

BINDIR	=	${PREFIX}/bin
MANDIR	=	${PREFIX}/man/cat
X11BASE	?=	/usr/X11R6

PROG	=	sclock
CFLAGS	+=	-I${X11BASE}/include -Wall -Werror
LDFLAGS	+=	-L${X11BASE}/lib
LDADD	=	-lX11

.include <bsd.prog.mk>
