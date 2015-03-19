# $Id: Makefile,v 1.6 2007/05/24 07:07:28 pyr Exp $

TRUEPREFIX?=	/usr/local

BINDIR	=	${TRUEPREFIX}/bin
MANDIR	=	${TRUEPREFIX}/man/cat

PROG	=	sclock
CFLAGS	+=	-I/usr/X11R6/include -Wall -Werror -g3
LDFLAGS	+=	-L/usr/X11R6/lib
LDADD	=	-lX11

.include <bsd.prog.mk>
