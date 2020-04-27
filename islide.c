/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include <X11/Xft/Xft.h>
#include <X11/cursorfont.h>

#include "drw.h"
#include "util.h"

/* macros */
#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define LENGTH(X)             (sizeof X / sizeof X[0])
#define TEXTW(X)              (drw_fontset_getwidth(drw, (X)) + lrpad)
#define MOUSEMASK               (BUTTONMASK|PointerMotionMask)
#define BUTTONMASK              (ButtonPressMask|ButtonReleaseMask)

/* enums */
enum { SchemeNorm, SchemeSel, SchemeOut, SchemeLast }; /* color schemes */
enum { CurNormal, CurMove, CurLast }; /* cursor */

struct item {
	char *text;
	struct item *left, *right;
	int out;
};

static char text[BUFSIZ] = "";
static int value = 50;
static char *embed;
static int bh, mw, mh;
static int inputw = 0, promptw;
static int lrpad; /* sum of left and right padding */
static size_t cursor;
static struct item *items = NULL;
static struct item *matches, *matchend;
static struct item *prev, *curr, *next, *sel;
static int mon = -1, screen;

static Atom clip, utf8;
static Display *dpy;
static Window root, parentwin, win;
static XIC xic;

static Drw *drw;
static Clr *scheme[SchemeLast];
static Cur *mcursor[CurLast];

#include "config.h"

static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
static char *(*fstrstr)(const char *, const char *) = strstr;


static void
cleanup(void)
{
	size_t i;

	XUngrabKey(dpy, AnyKey, AnyModifier, root);
	for (i = 0; i < SchemeLast; i++)
		free(scheme[i]);
	drw_free(drw);
	XSync(dpy, False);
	XCloseDisplay(dpy);
}

static void
drawmenu(void)
{
	unsigned int curpos;
	struct item *item;
	int x = 0, y = 0, w;
	char valuestr[100];
	char outputstr[100];
	sprintf(valuestr, "  %d", value); 

	if (prompt) {
		strcpy(outputstr, prompt);
		strncat(outputstr, valuestr, 10);
	} else {
		strcpy(outputstr, valuestr);
	}

	drw_setscheme(drw, scheme[SchemeNorm]);
	drw_rect(drw, 0, 0, mw, mh, 1, 1);
	drw_setscheme(drw, scheme[SchemeSel]);
	drw_rect(drw, 0, 0, value * (mw / maxvalue), mh, 1, 1);

	drw_text(drw, 0,0,TEXTW(outputstr), mh, 10, outputstr, 0);
	drw_map(drw, win, 0, 0, mw, mh);
}

static void
grabfocus(void)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 10000000  };
	Window focuswin;
	int i, revertwin;

	for (i = 0; i < 100; ++i) {
		XGetInputFocus(dpy, &focuswin, &revertwin);
		if (focuswin == win)
			return;
		XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
		nanosleep(&ts, NULL);
	}
	die("cannot grab focus");
}

static void
grabkeyboard(void)
{
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 1000000  };
	int i;

	if (embed)
		return;
	/* try to grab keyboard, we may have to wait for another process to ungrab */
	for (i = 0; i < 1000; i++) {
		if (XGrabKeyboard(dpy, DefaultRootWindow(dpy), True, GrabModeAsync,
		                  GrabModeAsync, CurrentTime) == GrabSuccess)
			return;
		nanosleep(&ts, NULL);
	}
	die("cannot grab keyboard");
}

static void spawn(char *executable) {
	if (fork() == 0) {
		setsid();
		system(executable);
		exit(0);
	}
}

static void valuetrigger() {
	char valuestring[100];
	char finalcmd[1024];

	sprintf(valuestring, "%d", value);
	if (command)
		strcpy(finalcmd, command);
	else
		strcpy(finalcmd, "/opt/instantos/menus/ex/p.sh ");		
	strncat(finalcmd, valuestring, 10);
	if (suffix)
		strncat(finalcmd, suffix, 1000);
	spawn(finalcmd);
}

static void incvalue(int increment)  {

	if (value + increment >= 0 && value + increment <= maxvalue)
		value+=increment;
	else
		return;
	valuetrigger();
	drawmenu();
}

int
getrootptr(int *x, int *y)
{
	int di;
	unsigned int dui;
	Window dummy;

	return XQueryPointer(dpy, root, &dummy, &dummy, x, y, &di, &di, &dui);
}

static void
dragmouse() {
	int x, y;
	XEvent ev;
	Time lasttime = 0;
	int lastx;
	if (XGrabPointer(dpy, root, False, MOUSEMASK, GrabModeAsync, GrabModeAsync,
		None, mcursor[CurMove]->cursor, CurrentTime) != GrabSuccess)
		return;
	if (!getrootptr(&x, &y))
		return;
	do {
		XMaskEvent(dpy, MOUSEMASK|ExposureMask|SubstructureRedirectMask, &ev);
		switch(ev.type) {
		case ConfigureRequest:
		case Expose:
		case MapRequest:
		case MotionNotify:
			if ((ev.xmotion.time - lasttime) <= (1000 / 60))
				continue;
			lasttime = ev.xmotion.time;
			if (!lastx)
				lastx = ev.xmotion.x_root;
			
			if (abs(lastx - ev.xmotion.x_root) > (mw / maxvalue)) {
				value = ev.xmotion.x_root / (mw / maxvalue);
				drawmenu();
				valuetrigger();
				lastx = ev.xmotion.x_root;
			}
			break;
		}
	} while (ev.type != ButtonRelease);
	if (ev.xmotion.y_root > 100) {
		cleanup();
		exit(1);
	}
	XUngrabPointer(dpy, CurrentTime);
}

static void typenumber(int digit) {
	if (keyboardvalue * 10 + digit >= maxvalue) {
		keyboardvalue = maxvalue;
	} else {
		keyboardvalue = keyboardvalue * 10;
		keyboardvalue += digit;
	}
}

static void
keypress(XKeyEvent *ev)
{
	char buf[32];

	int len;

	KeySym ksym;
	Status status;

	len = XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
	switch (ksym)
	{
	case XK_h:
	case XK_j:
	case XK_Left:
		incvalue(-5);
		break;
	case XK_l:
	case XK_k:
	case XK_Right:
		incvalue(5);
		break;
	case XK_Up:
		incvalue(20);
		break;
	case XK_Down:
		incvalue(-20);
		break;
	case XK_plus:
		incvalue(1);
		break;
	case XK_minus:
		incvalue(-1);
		break;
	case XK_0:
		if (!keyboardvalue) {
			value = 0;
			valuetrigger();
			drawmenu();
		} else {
			typenumber(0);
		}
		break;
	case XK_1:
		typenumber(1);
		break;
	case XK_2:
		typenumber(2);
		break;
	case XK_3:
		typenumber(3);
		break;
	case XK_4:
		typenumber(4);
		break;
	case XK_5:
		typenumber(5);
		break;
	case XK_6:
		typenumber(6);
		break;
	case XK_7:
		typenumber(7);
		break;
	case XK_8:
		typenumber(8);
		break;
	case XK_9:
		typenumber(9);
		break;
	case XK_Return:
		if (!keyboardvalue)
			break;
		value = keyboardvalue;
		keyboardvalue = 0;
		valuetrigger();
		drawmenu();
		break;
	default:
		cleanup();
		exit(0);
		break;
	}
}

static void
buttonpress(XEvent *ev)
{
		switch (ev->xbutton.button)
			{
			case Button3:
				cleanup();
				exit(0);
				break;
			case Button1:
				value = ev->xbutton.x_root / (mw / maxvalue);
				valuetrigger();
				drawmenu();
				dragmouse();
				break;
			case Button2:
				value = startvalue;
				valuetrigger();
				drawmenu();
				break;
			case Button5:
				incvalue(-5);
				break;
			case Button4:
				incvalue(5);
				break;

			default:
				cleanup();
				exit(0);
				break;
			}
}

static void
run(void)
{
	XEvent ev;

	while (!XNextEvent(dpy, &ev)) {
		if (XFilterEvent(&ev, win))
			continue;
		switch(ev.type) {
		case DestroyNotify:
			if (ev.xdestroywindow.window != win)
				break;
			cleanup();
			exit(1);
		case Expose:
			if (ev.xexpose.count == 0)
				drw_map(drw, win, 0, 0, mw, mh);
			break;
		case FocusIn:
			/* regrab focus from parent window */
			if (ev.xfocus.window != win)
				grabfocus();
			break;
		case KeyPress:
			keypress(&ev.xkey);
			break;
		case VisibilityNotify:
			if (ev.xvisibility.state != VisibilityUnobscured)
				XRaiseWindow(dpy, win);
			break;
		case ButtonPress:
			buttonpress(&ev);
			break;
		}
	}
}


static void
setup(void)
{
	int x, y, i, j;
	unsigned int du;
	XSetWindowAttributes swa;
	XIM xim;
	Window w, dw, *dws;
	XWindowAttributes wa;
	XClassHint ch = {"islide", "islide"};
#ifdef XINERAMA
	XineramaScreenInfo *info;
	Window pw;
	int a, di, n, area = 0;
#endif
	if (startvalue)
		value = startvalue;
	else
		value = maxvalue / 2;
	/* init appearance */
	for (j = 0; j < SchemeLast; j++)
		scheme[j] = drw_scm_create(drw, colors[j], 2);

	clip = XInternAtom(dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dpy, "UTF8_STRING", False);

	/* calculate menu geometry */
	bh = drw->fonts->h + 16;
	lines = MAX(lines, 0);
	mh = (lines + 1) * bh;
#ifdef XINERAMA
	i = 0;
	if (parentwin == root && (info = XineramaQueryScreens(dpy, &n))) {
		XGetInputFocus(dpy, &w, &di);
		if (mon >= 0 && mon < n)
			i = mon;
		else if (w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if (XQueryTree(dpy, (pw = w), &dw, &w, &dws, &du) && dws)
					XFree(dws);
			} while (w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if (XGetWindowAttributes(dpy, pw, &wa))
				for (j = 0; j < n; j++)
					if ((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
						area = a;
						i = j;
					}
		}
		/* no focused window is on screen, so use pointer location instead */
		if (mon < 0 && !area && XQueryPointer(dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
			for (i = 0; i < n; i++)
				if (INTERSECT(x, y, 1, 1, info[i]))
					break;

		x = info[i].x_org;
		y = info[i].y_org + (topbar ? 0 : info[i].height - mh);
		mw = info[i].width;
		XFree(info);
	} else
#endif
	{
		if (!XGetWindowAttributes(dpy, parentwin, &wa))
			die("could not get embedding window attributes: 0x%lx",
			    parentwin);
		x = 0;
		y = topbar ? 0 : wa.height - mh;
		mw = wa.width;
	}
	promptw = (prompt && *prompt) ? TEXTW(prompt) - lrpad / 4 : 0;
	inputw = MIN(inputw, mw/3);

	/* create menu window */
	swa.override_redirect = True;
	swa.background_pixel = scheme[SchemeNorm][ColBg].pixel;
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask | ButtonPressMask;
	win = XCreateWindow(dpy, parentwin, x, y, mw, mh, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);
	XSetClassHint(dpy, win, &ch);


	/* input methods */
	if ((xim = XOpenIM(dpy, NULL, NULL, NULL)) == NULL)
		die("XOpenIM failed: could not open input device");

	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);

	XMapRaised(dpy, win);
	if (embed) {
		XSelectInput(dpy, parentwin, FocusChangeMask | SubstructureNotifyMask);
		if (XQueryTree(dpy, parentwin, &dw, &w, &dws, &du) && dws) {
			for (i = 0; i < du && dws[i] != win; ++i)
				XSelectInput(dpy, dws[i], FocusChangeMask);
			XFree(dws);
		}
		grabfocus();
	}
	mcursor[CurNormal] = drw_cur_create(drw, XC_left_ptr);
	mcursor[CurMove] = drw_cur_create(drw, XC_fleur);

	drw_resize(drw, mw, mh);
	drawmenu();
}

static void
usage(void)
{
	fputs("usage: dmenu [-bfiv] [-l lines] [-p prompt] [-fn font] [-m monitor]\n"
	      "             [-nb color] [-nf color] [-sb color] [-sf color] [-w windowid]\n", stderr);
	exit(1);
}

int
main(int argc, char *argv[])
{
	XWindowAttributes wa;
	int i, fast = 0;

	for (i = 1; i < argc; i++)
		/* these options take no arguments */
		if (!strcmp(argv[i], "-v")) {      /* prints version information */
			puts("dmenu-"VERSION);
			exit(0);
		} else if (!strcmp(argv[i], "-b")) /* appears at the bottom of the screen */
			topbar = 0;
		else if (!strcmp(argv[i], "-f"))   /* grabs keyboard before reading stdin */
			fast = 1;
		else if (!strcmp(argv[i], "-i")) { /* case-insensitive item matching */
			fstrncmp = strncasecmp;
		} else if (i + 1 == argc)
			usage();
		/* these options take one argument */
		else if (!strcmp(argv[i], "-s"))   /* number of lines in vertical list */
			startvalue = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-m"))
			maxvalue = atoi(argv[++i]);
		else if (!strcmp(argv[i], "-p"))   /* adds prompt to left of input field */
			prompt = argv[++i];
		else if (!strcmp(argv[i], "-c"))   /* adds prompt to left of input field */
			command = argv[++i];
		else if (!strcmp(argv[i], "-a"))   /* adds prompt to left of input field */
			suffix = argv[++i];
		else if (!strcmp(argv[i], "-fn"))  /* font or font set */
			fonts[0] = argv[++i];
		else if (!strcmp(argv[i], "-nb"))  /* normal background color */
			colors[SchemeNorm][ColBg] = argv[++i];
		else if (!strcmp(argv[i], "-nf"))  /* normal foreground color */
			colors[SchemeNorm][ColFg] = argv[++i];
		else if (!strcmp(argv[i], "-sb"))  /* selected background color */
			colors[SchemeSel][ColBg] = argv[++i];
		else if (!strcmp(argv[i], "-sf"))  /* selected foreground color */
			colors[SchemeSel][ColFg] = argv[++i];
		else if (!strcmp(argv[i], "-w"))   /* embedding window id */
			embed = argv[++i];
		else
			usage();

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("warning: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("cannot open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	if (!embed || !(parentwin = strtol(embed, NULL, 0)))
		parentwin = root;
	if (!XGetWindowAttributes(dpy, parentwin, &wa))
		die("could not get embedding window attributes: 0x%lx",
		    parentwin);
	drw = drw_create(dpy, screen, root, wa.width, wa.height);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("no fonts could be loaded.");
	lrpad = drw->fonts->h;

#ifdef __OpenBSD__
	if (pledge("stdio rpath", NULL) == -1)
		die("pledge");
#endif

		grabkeyboard();
	
	setup();
	run();

	return 1; /* unreachable */
}
