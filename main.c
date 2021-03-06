#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xcms.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define WINDOW_MARGIN 6

/* Datastructures */
struct client;
struct key {
	unsigned int mod;
	KeySym keysym;
	void (*callback) (void);
};

struct client {
	struct client *prev;
	struct client *next;
	Window window;
};

struct column {
	unsigned int nclients;
	struct client *client_list;
};

/* Prototypes */
static void configurerequest(XEvent * e);
static void destroy(XEvent * e);
static void keypress(XEvent * e);
static void map_request(XEvent * e);
/*
static void focus_up(XEvent * e);
static void focus_down(XEvent * e); */
static void run_terminal(void);
static void tile(void);
static void enternotify(XEvent * e);
/* Globals */
static struct column column = { 0, 0 };

static Display *display;
static Window root;
static int screen;
static int width, height;
static struct client *focus_client;

static struct key keys[] = {
	{ Mod1Mask,	XK_Return,	run_terminal}
/*	{ Mod1Mask,	XK_j,		focus_up},
	{ Mod1Mask,	XK_k,		focus_down},*/
};

static void (*events[LASTEvent]) (XEvent * e) = {
	[KeyPress] = keypress,
	[MapRequest] = map_request,
	[DestroyNotify] = destroy,
	[ConfigureRequest] = configurerequest,
	[EnterNotify] = enternotify
};

void configurerequest(XEvent * e)
{
	XConfigureRequestEvent *ev = &e->xconfigurerequest;
	XWindowChanges wc;
	wc.x = ev->x;
	wc.y = ev->y;
	wc.width = ev->width;
	wc.height = ev->height;
	wc.border_width = ev->border_width;
	wc.sibling = ev->above;
	wc.stack_mode = ev->detail;
	XConfigureWindow(display, ev->window, ev->value_mask, &wc);
}

static void destroy(XEvent * e)
{
	struct client *tmp;
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	for (tmp = column.client_list; tmp; tmp = tmp->next) {
		if (tmp->window == ev->window) {
			if (tmp->prev) {
				tmp->prev->next = tmp->next;
				if (tmp->next)
					tmp->next->prev = tmp->prev;
			} else {
				column.client_list = tmp->next;
				if (tmp->next)
					tmp->next->prev = 0;
			}
			column.nclients--;
			free(tmp);
			break;
		}
	}
	tile();
}

static void enternotify(XEvent * e)
{
	struct client *tmp;
	XCrossingEvent *ev = &e->xcrossing;
	printf("Focus in event\n");
	if (column.nclients == 0)
		goto setfocus;
	for (tmp = column.client_list; tmp; tmp = tmp->next) {
		if (tmp->window == ev->window) {
			focus_client = tmp;
			return;
		}
	}
setfocus:
	XSetInputFocus(display, tmp->window, RevertToPointerRoot, 0);
}

static void keypress(XEvent * e)
{
	int i;
	XKeyEvent ke = e->xkey;
	int keysyms_per_keycode_return;
	KeySym *keysym = XGetKeyboardMapping(display,
					     ke.keycode,
					     1,
					     &keysyms_per_keycode_return);
	for (i = 0; i < sizeof(keys) / sizeof(struct key); i++) {
		if (keys[i].keysym == keysym[0] && keys[i].mod == ke.state) {
			keys[i].callback();
			break;
		}
	}
}

static void map_request(XEvent * e)
{
	struct client *c, *tmp;
	Window border_window;
	XWindowAttributes attributes;

	XMapRequestEvent *ev = &e->xmaprequest;
	XGetWindowAttributes(display, ev->window, &attributes);

	c = (struct client *)malloc(sizeof(struct client));

	if (column.client_list == 0) {
		c->next = 0;
		c->prev = 0;
		c->window = ev->window;
		column.client_list = c;
		column.nclients = 1;
	} else {
		for (tmp = column.client_list; tmp->next; tmp = tmp->next) ;
		c->next = 0;
		c->prev = tmp;
		c->window = ev->window;
		tmp->next = c;
		column.nclients++;
	}
	XMapWindow(display, c->window);

	focus_client = c;
	tile();
}

static void run_terminal(void)
{
	int ret;
	if (fork() == 0) {
		ret = system("xterm");
		exit(ret);
	}
}

static void tile(void)
{
	int y = 0;
	struct client *tmp;
	if (!column.nclients)
		return;
	for (tmp = column.client_list; tmp; tmp = tmp->next) {
		XMoveResizeWindow(display, tmp->window, WINDOW_MARGIN,
				  y + WINDOW_MARGIN, width - 2*WINDOW_MARGIN,
				  height / column.nclients - 2*WINDOW_MARGIN);
		y += height / column.nclients;
		if (tmp == focus_client) {
			XSetInputFocus(display, tmp->window, RevertToPointerRoot, 0);
		}
	}

}

int main(int argc, char **Argv)
{
	if (!(display = XOpenDisplay(NULL))) {
		return 1;
	}
	screen = DefaultScreen(display);
	root = RootWindow(display, screen);

	width = XDisplayWidth(display, screen);
	height = XDisplayHeight(display, screen);

	int i;
	for (i = 0; i < sizeof(keys) / sizeof(struct key); i++) {
		KeyCode code;
		if ((code = XKeysymToKeycode(display, keys[i].keysym))) {
			XGrabKey(display, code, keys[i].mod, root, True,
				 GrabModeAsync, GrabModeAsync);
		}
	}
	XSelectInput(display, root,
		     SubstructureNotifyMask | SubstructureRedirectMask);
	printf("Screen (%d, %d)\n", width, height);
	for (;;) {
		XEvent ev;
		while (!XNextEvent(display, &ev)) {
			if (events[ev.type])
				events[ev.type] (&ev);
		}
	}
	return 0;
}
