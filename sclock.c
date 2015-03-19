/* $Id: sclock.c,v 1.2 2007/05/22 14:30:11 pyr Exp $ */
/*
 * Copyright (c) 2007 Pierre-Yves Ritschard <pyr@spootnik.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/keysym.h>

#ifndef APPLOADDIR
#define APPLOADDIR	"/etc/X11/app-defaults/"
#endif
#define	CLASS_NAME	"SClock"

#define DEFAULT_GEOM	"80x15"
#define DEFAULT_FG	"green"
#define DEFAULT_BG	"#2d2d2d"
#define DEFAULT_FONT	"fixed"
#define DIGIT_BORDER	2
#define DIGIT_SEP	1
#define DIGIT_WIDTH	6
#define DIGIT_HEIGHT	10
#define COL_WIDTH	1

#define BH_TOP	0x01
#define BH_MID	0x02
#define BH_BOT	0x04
#define BV_TLF	0x08
#define BV_TRT	0x10
#define BV_BLF	0x20
#define BV_BRT	0x40

#define D0		BH_TOP|BH_BOT|BV_TLF|BV_TRT|BV_BLF|BV_BRT
#define	D1		BV_TRT|BV_BRT
#define D2		BH_TOP|BH_MID|BH_BOT|BV_TRT|BV_BLF
#define D3		BH_TOP|BH_MID|BH_BOT|BV_TRT|BV_BRT
#define D4		BH_MID|BV_TLF|BV_TRT|BV_BRT
#define D5		BH_TOP|BH_MID|BH_BOT|BV_TLF|BV_BRT
#define D6		BH_TOP|BH_MID|BH_BOT|BV_TLF|BV_BLF|BV_BRT
#define	D7		BH_TOP|BV_TRT|BV_BRT
#define D8		BH_TOP|BH_MID|BH_BOT|BV_TLF|BV_TRT|BV_BLF|BV_BRT
#define D9		BH_TOP|BH_MID|BH_BOT|BV_TLF|BV_TRT|BV_BRT

struct area {
	int		 x;
	int		 y;
	int		 w;
	int		 h;
};

struct widget {
	int			 flags;
#define F_TWELVE	 	 0x01
#define F_BLINK		 	 0x02
#define F_SECONDS		 0x04
#define F_OUTLINE		 0x08
#define F_SHADE			 0x10
	int			 screen;
	Display			*dpy;
	Window			 root;
	Window			 win;
	XColor			 fg;
	XColor			 bg;
	XWindowAttributes	 wa;
	XFontStruct		*font;
	GC			 gc;
	struct area		 geom;
#define NAREAS			 9
	struct area		 areas[NAREAS];
	time_t			 before;
	int			 sep;
	int			 db;
	int			 dw;
	int			 dh;
	int			 cw;
	int			 xofft;
	int			 yofft;
};

void	resource_init(struct widget *, int, char *[]);
void	widget_init(struct widget *);
void	widget_setup(struct widget *);
void	widget_calcsize(struct widget *, int, int, int, int);
void	widget_draw(struct widget *, int);
void	widget_draw_column(struct widget *, int);
void	widget_draw_digit(struct widget *, int, int);
void	widget_event(struct widget *, XEvent *);

void
usage(void)
{
	extern const char	*__progname;

	fprintf(stderr, "usage: %s", __progname);
	exit(1);
}

void
widget_init(struct widget *w)
{
	if ((w->dpy = XOpenDisplay(NULL)) == NULL)
		errx(1, "cannot open display");
	w->screen = DefaultScreen(w->dpy);
	w->root = RootWindow(w->dpy, w->screen);
	XGetWindowAttributes(w->dpy, w->root, &w->wa);
}

void
widget_areas(struct widget *w)
{
	int			 width;
	struct area		*last_area;

	width = (w->dw + w->db) * 4 + w->cw;

	if (w->flags & F_SECONDS)
		width += w->db + (w->dw + w->db) * 2 + w->cw;

	if (w->flags & F_TWELVE)
		width += w->db + MAX(XTextWidth(w->font, "am", 2),
		    XTextWidth(w->font, "pm", 2));

	if (width > w->geom.w)
		errx(1, "invalid geometry");

	w->xofft = (w->geom.w / 2) - (width / 2);
	w->yofft = (w->geom.h / 2) - (w->dh / 2);

	w->areas[0].x = w->xofft;;
	w->areas[0].y = w->yofft;
	w->areas[0].w = w->dw;
	w->areas[0].h = w->dh;

	w->areas[1].x = w->areas[0].x + w->areas[0].w + w->db;
	w->areas[1].y = w->yofft;
	w->areas[1].w = w->dw;
	w->areas[1].h = w->dh;

	w->areas[2].x = w->areas[1].x + w->areas[1].w + w->db;
	w->areas[2].y = w->yofft;
	w->areas[2].w = w->cw;
	w->areas[2].h = w->dh;

	w->areas[3].x = w->areas[2].x + w->areas[2].w + w->db;
	w->areas[3].y = w->yofft;
	w->areas[3].w = w->dw;
	w->areas[3].h = w->dh;

	w->areas[4].x = w->areas[3].x + w->areas[3].w + w->db;
	w->areas[4].y = w->yofft;
	w->areas[4].w = w->dw;;
	w->areas[4].h = w->dh;

	last_area = &w->areas[4];

	if (w->flags & F_SECONDS) {
		w->areas[5].x = w->areas[4].x + w->areas[4].w + w->db;
		w->areas[5].y = w->yofft;
		w->areas[5].w = w->cw;
		w->areas[5].h = w->dh;

		w->areas[6].x = w->areas[5].x + w->areas[5].w + w->db;
		w->areas[6].y = w->yofft;
		w->areas[6].w = w->dw;;
		w->areas[6].h = w->dh;

		w->areas[7].x = w->areas[6].x + w->areas[6].w + w->db;
		w->areas[7].y = w->yofft;
		w->areas[7].w = w->dw;;
		w->areas[7].h = w->dh;

		last_area = &w->areas[7];
	}

	if (w->flags & F_TWELVE) {
		w->areas[8].x = last_area->x + last_area->w + w->db;
		w->areas[8].y = w->yofft + w->dh - w->font->descent;
	}
}

void
widget_setup(struct widget *w)
{
	XSetWindowAttributes	swa;
	XGCValues		gcv;

	bzero(&swa, sizeof(swa));
	bzero(&gcv, sizeof(gcv));
	w->win = XCreateWindow(w->dpy, w->root, w->geom.x, w->geom.y,
	    w->geom.w, w->geom.h, 0,
	    CopyFromParent, CopyFromParent, CopyFromParent, 0, &swa);
	XStoreName(w->dpy, w->win, "sclock");

	widget_areas(w);

	w->gc = XCreateGC(w->dpy, w->win, 0, &gcv);
	XSetForeground(w->dpy, w->gc, w->fg.pixel);
	XSetBackground(w->dpy, w->gc, w->bg.pixel);
	XSetFont(w->dpy, w->gc, w->font->fid);
	
	XSelectInput(w->dpy, w->win,
	    ExposureMask | StructureNotifyMask | KeyReleaseMask);
	XSetWindowBackground(w->dpy, w->win, w->bg.pixel);
	XSetWindowBackground(w->dpy, w->win, w->bg.pixel);
	XMapWindow(w->dpy, w->win);
}

void
widget_draw(struct widget *w, int forced)
{
	time_t		 now;
	struct tm	*tm;
	int		hour;
	int		am;
	XTextItem	ti;

	now = time(NULL);
	if (now == w->before && !forced)
		return;
	w->before = now;
	XClearWindow(w->dpy, w->win);

	if (w->flags & F_OUTLINE) {
		XDrawLine(w->dpy, w->win, w->gc, 0, 0, 0, w->geom.h - 1);
		XDrawLine(w->dpy, w->win, w->gc, 0, 0, w->geom.w - 1, 0);
		XDrawLine(w->dpy, w->win, w->gc, w->geom.w - 1, 0,
		    w->geom.w - 1, w->geom.h - 1);
		XDrawLine(w->dpy, w->win, w->gc, 0, w->geom.h - 1,
		    w->geom.w - 1, w->geom.h - 1);
	}

	now = time(NULL);
	tm = localtime(&now);
	w->before = now;

	hour = tm->tm_hour;
	if (hour > 12)
		am = 0;
	else
		am = 1;
	if (w->flags & F_TWELVE)
		hour = tm->tm_hour % 12;

	widget_draw_digit(w, 0, hour / 10);
	widget_draw_digit(w, 1, hour % 10);
	if (!(w->flags & F_BLINK) || now % 2 == 0)
		widget_draw_column(w, 2);
	widget_draw_digit(w, 3, tm->tm_min / 10);
	widget_draw_digit(w, 4, tm->tm_min % 10);
	if (w->flags & F_SECONDS) {
		if (!(w->flags & F_BLINK) || now % 2 == 0)
			widget_draw_column(w, 5);
		widget_draw_digit(w, 6, tm->tm_sec / 10);
		widget_draw_digit(w, 7, tm->tm_sec % 10);
	}
	if (w->flags & F_TWELVE) {
		bzero(&ti, sizeof(ti));
		ti.chars = (am)?"am":"pm";
		ti.nchars = 2;
		ti.font = w->font->fid;
		XDrawText(w->dpy, w->win, w->gc,
		    w->areas[8].x, w->areas[8].y, &ti, 1);
	}

	XFlush(w->dpy);
	XSync(w->dpy, 0);
}

void
widget_draw_column(struct widget *w, int a_idx)
{
	struct area	*a;

	a = &w->areas[a_idx];
	
	XFillRectangle(w->dpy, w->win, w->gc,
	    a->x, a->y + (a->h / 2) - (w->cw * 2), a->w, w->cw);
	XFillRectangle(w->dpy, w->win, w->gc,
	    a->x, a->y + (a->h / 2) + w->cw, a->w, w->cw);
}

void
widget_draw_digit(struct widget *w, int a_idx, int digit)
{
	int		 mask;
	struct area	*a;
	static int	 digits[] = { D0, D1, D2, D3, D4, D5, D6, D7, D8, D9 };

	a = &w->areas[a_idx];
	mask = digits[digit];

	/* top */
	if (mask & BH_TOP)
		XDrawLine(w->dpy, w->win, w->gc, a->x + w->sep, a->y,
		    a->x + a->w - (w->sep * 2), a->y);

	/* top vert left */
	if (mask & BV_TLF)
		XDrawLine(w->dpy, w->win, w->gc, a->x, a->y + w->sep,
		    a->x, a->y + (a->h / 2) - w->sep);

	/* top vert right */
	if (mask & BV_TRT)
		XDrawLine(w->dpy, w->win, w->gc,
		    a->x + a->w - w->sep, a->y + w->sep,
		    a->x + a->w - w->sep, a->y + (a->h / 2) - w->sep);

	/* middle */
	if (mask & BH_MID)
		XDrawLine(w->dpy, w->win, w->gc, a->x + w->sep,
		    a->y + (a->h / 2), a->x + a->w - (w->sep * 2),
		    a->y + (a->h / 2));

	/* bottom vert left */
	if (mask & BV_BLF)
		XDrawLine(w->dpy, w->win, w->gc, a->x,
		    a->y + (a->h / 2) + w->sep,
		    a->x, a->y + a->h - w->sep);

	/* bottom vert right */
	if (mask & BV_BRT)
		XDrawLine(w->dpy, w->win, w->gc, a->x + a->w - w->sep,
		    a->y + (a->h / 2) + w->sep, a->x + a->w - w->sep,
		    a->y + a->h - w->sep);

	/* bottom */
	if (mask & BH_BOT)
		XDrawLine(w->dpy, w->win, w->gc, a->x + w->sep,
		    a->y + a->h,
		    a->x + a->w - (w->sep * 2), a->y + a->h);
}

void
widget_event(struct widget *w, XEvent *ev)
{
	KeySym	ks;

	switch (ev->type) {
	case Expose:
		if (ev->xexpose.count == 0)
			widget_draw(w, 1);
		break;
	case KeyRelease:
		ks = XKeycodeToKeysym(w->dpy, ev->xkey.keycode, 0);
		if (ks == XK_q)
			exit(1);
		break;
	case ConfigureNotify:
		w->geom.x = ev->xconfigure.x;
		w->geom.y = ev->xconfigure.y;
		w->geom.w = ev->xconfigure.width;
		w->geom.h = ev->xconfigure.height;
		widget_areas(w);
		break;
	case DestroyNotify:
		XFreeGC(w->dpy, w->gc);
		XDestroyWindow(w->dpy, w->win);
		XCloseDisplay(w->dpy);
		exit(0);
	}
}

void
resource_init(struct widget *w, int argc, char *argv[])
{
	const char		*estr;
	char			*dbstr;
	char			*dummy;
	char			*sarg;
	XrmDatabase		 db;
	XrmDatabase		 filedb;
	XrmValue		 val;
	XColor			 dump;

	static XrmOptionDescRec	 opts[] = {
		{ "-twelve",	"twelve",	XrmoptionNoArg, "on" },
		{ "-blink",	"blink",	XrmoptionNoArg, "on" },
		{ "-seconds",	"seconds",	XrmoptionNoArg, "on" },
		{ "-outline",	"outline",	XrmoptionNoArg, "on" },
		{ "-sep",	"digit.sep",	XrmoptionSepArg, NULL },
		{ "-db",	"digit.border",	XrmoptionSepArg, NULL },
		{ "-dw",	"digit.width",	XrmoptionSepArg, NULL },
		{ "-dh",	"digit.height",	XrmoptionSepArg, NULL },
		{ "-fg",	"foreground",	XrmoptionSepArg, NULL },
		{ "-bg",	"background",	XrmoptionSepArg, NULL },
		{ "-cw",	"column.width",	XrmoptionSepArg, NULL },
		{ "-geometry",	"geometry",	XrmoptionSepArg, NULL }
	};

	XrmInitialize();
	db = NULL;

	if ((dbstr = XResourceManagerString(w->dpy)) != NULL)
		db = XrmGetStringDatabase(dbstr);

	if ((filedb = XrmGetFileDatabase(APPLOADDIR CLASS_NAME)) != NULL)
		XrmMergeDatabases(filedb, &db);

	XrmParseCommand(&db, opts, sizeof(opts) / sizeof(opts[0]), "sclock",
	    &argc, argv);

	if (XrmGetResource(db, "sclock.twelve", "SClock.Twelve", &dummy, &val))
		if (strcmp((char *)val.addr, "on") == 0)
			w->flags |= F_TWELVE;
	if (XrmGetResource(db, "sclock.blink", "SClock.Blink", &dummy, &val))
		if (strcmp((char *)val.addr, "on") == 0)
			w->flags |= F_BLINK;
	if (XrmGetResource(db, "sclock.seconds", "SClock.Seconds",
	    &dummy, &val))
		if (strcmp((char *)val.addr, "on") == 0)
			w->flags |= F_SECONDS;
	if (XrmGetResource(db, "sclock.outline", "SClock.Outline",
	    &dummy, &val))
		if (strcmp((char *)val.addr, "on") == 0)
			w->flags |= F_OUTLINE;

	if (XrmGetResource(db, "sclock.foreground", "SClock.Foreground",
	    &dummy, &val))
		sarg = (char *)val.addr;
	else
		sarg = DEFAULT_FG;
	XAllocNamedColor(w->dpy, w->wa.colormap, sarg, &dump, &w->fg);

	if (XrmGetResource(db, "sclock.background", "SClock.Background",
	    &dummy, &val))
		sarg = (char *)val.addr;
	else
		sarg = DEFAULT_BG;
	XAllocNamedColor(w->dpy, w->wa.colormap, sarg, &dump, &w->bg);

	if (XrmGetResource(db, "sclock.font", "SClock.Font", &dummy, &val))
		sarg = (char *)val.addr;
	else
		sarg = DEFAULT_FONT;
	w->font = XLoadQueryFont(w->dpy, sarg);

	if (XrmGetResource(db, "sclock.geometry", "SClock.Geometry",
	    &dummy, &val))
		sarg = (char *)val.addr;
	else
		sarg = DEFAULT_GEOM;
	XParseGeometry(sarg, &w->geom.x, &w->geom.y,
	    &w->geom.w, &w->geom.h);

	if (XrmGetResource(db, "sclock.digit.width", "SClock.Digit.Width",
	    &dummy, &val)) {
		w->dw = strtonum((char *)val.addr, 6, w->geom.w, &estr);
		if (estr)
			errx(1, "digit width %s is %s", (char *)val.addr, estr);
	} else
		w->dw = DIGIT_WIDTH;

	if (XrmGetResource(db, "sclock.digit.height", "SClock.Digit.Height",
	    &dummy, &val)) {
		w->dh = strtonum((char *)val.addr, 4, w->geom.h, &estr);
		if (estr)
			errx(1, "digit height %s is %s",
			    (char *)val.addr, estr);
	} else
		w->dh = DIGIT_HEIGHT;

	if (XrmGetResource(db, "sclock.digit.sep", "SClock.Digit.Sep",
	    &dummy, &val)) {
		w->sep = strtonum((char *)val.addr, 0, w->geom.h, &estr);
		if (estr)
			errx(1, "digit height %s is %s",
			    (char *)val.addr, estr);
	} else
		w->sep = DIGIT_SEP;

	if (XrmGetResource(db, "sclock.digit.border", "SClock.Digit.Border",
	    &dummy, &val)) {
		w->db = strtonum((char *)val.addr, 1, w->geom.w, &estr);
		if (estr)
			errx(1, "digit border %s is %s",
			    (char *)val.addr, estr);
	} else
		w->db = DIGIT_BORDER;

	if (XrmGetResource(db, "sclock.column.width", "SClock.Column.Width",
	    &dummy, &val)) {
		w->cw = strtonum((char *)val.addr, 1, w->geom.w, &estr);
		if (estr)
			errx(1, "column width %s is %s",
			    (char *)val.addr, estr);
	} else
		w->cw = COL_WIDTH;
}

int
main(int argc, char *argv[])
{
	struct widget		w;
	XEvent			ev;
	struct timeval		tv;

	bzero(&w, sizeof(w));
	widget_init(&w);
	resource_init(&w, argc, argv);
	widget_setup(&w);

	while (1) {
		while (XPending(w.dpy)) {
			XNextEvent(w.dpy, &ev);
			widget_event(&w, &ev);
		}
		tv.tv_sec = 0;
		tv.tv_usec = 250;
		select(0, NULL, NULL, NULL, &tv);
		widget_draw(&w, 0);
	}
	return (0);	
}
