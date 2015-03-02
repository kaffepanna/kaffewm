#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct client;




static Display *display;
static Window root;
static int screen;

static int width, height;

/* events */
static void map_request(XEvent *e);
static void keypress(XEvent *e);
static void destroy(XEvent *e);
void configurerequest(XEvent *e);
/* Callbacks */
static void run_terminal(void);


static void (*events[LASTEvent])(XEvent *e) = {
	[KeyPress] = keypress,
	[MapRequest] = map_request,
	[DestroyNotify] = destroy,
	[ConfigureRequest] = configurerequest
};

struct key {
	unsigned int mod;
	KeySym keysym;
	void (*callback)(void);
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

static struct key keys[] = {
	{Mod1Mask, XK_Return, run_terminal},
};

static struct column column = { 0, 0 };

static void keypress(XEvent *e)
{
	int i;
	XKeyEvent ke = e->xkey;
	int keysyms_per_keycode_return;
	KeySym *keysym = XGetKeyboardMapping(display,
					     ke.keycode,
	                                     1,
                                             &keysyms_per_keycode_return);
	for (i = 0; i < sizeof(keys)/sizeof(struct key); i++)
	{
		if (keys[i].keysym == keysym[0] && keys[i].mod == ke.state)
		{
			keys[i].callback();
			break;
		}
	}
}

void tile()
{
	int y = 0;
	struct client *tmp;
	if (!column.nclients)
		return;
	for (tmp=column.client_list; tmp; tmp=tmp->next)
	{
		XMoveResizeWindow(display, tmp->window, 0,y, width, height/column.nclients);
		y += height/column.nclients;
	}

}

static void destroy(XEvent *e)
{
	struct client *tmp;
	XDestroyWindowEvent *ev = &e->xdestroywindow;
	for (tmp=column.client_list; tmp; tmp = tmp->next)
	{
		if (tmp->window == ev->window)
		{
			if (tmp->prev) {
				tmp->prev->next = tmp->next;
				if (tmp->next)
					tmp->next->prev = tmp->prev;
			}
			else {
				column.client_list = tmp->next;
				if( tmp->next)
					tmp->next->prev = 0;
			}
			column.nclients--;
			free(tmp);
			break;
		}
	}
	tile();
}

static void map_request(XEvent *e)
{
	struct client *c, *tmp;
	XMapRequestEvent *ev = &e->xmaprequest;

	c = (struct client*)malloc(sizeof(struct client));

	if (column.client_list == 0)
	{
		c->next = 0;
		c->prev = 0;
		c->window = ev->window;
		column.client_list = c;
		column.nclients = 1;
	}
	else
	{
		for (tmp=column.client_list; tmp->next; tmp=tmp->next);
		c->next = 0;
		c->prev = tmp;
		c->window = ev->window;
		tmp->next = c;
		column.nclients++;
	}

	XMapWindow(display, ev->window);
	tile();
}

void configurerequest(XEvent *e) {
	// Paste from DWM, thx again \o/
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
static void run_terminal(void)
{
	int ret;
	if (fork() == 0)
	{
		ret = system("xterm");
		exit(0);
	}
}

int main(int argc, char **Argv)
{
	if (!(display = XOpenDisplay(NULL)))
	{
		return 1;
	}
	screen = DefaultScreen(display);
	root= RootWindow(display, screen);

	width  = XDisplayWidth(display, screen);
	height = XDisplayHeight(display, screen);

	int i;
	for (i=0; i < sizeof(keys)/sizeof(struct key); i++)
	{
		KeyCode code;
		if ((code = XKeysymToKeycode(display, keys[i].keysym)))
		{
			XGrabKey(display, code, keys[i].mod, root, True, GrabModeAsync,
			         GrabModeAsync);
		}
	}
	XSelectInput(display,root,SubstructureNotifyMask|SubstructureRedirectMask);
	for (;;)
	{
		XEvent ev;
		while (!XNextEvent(display, &ev))
		{
			if(events[ev.type])
				events[ev.type](&ev);
		}
	}
	return 0;
}
