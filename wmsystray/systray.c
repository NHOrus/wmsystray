/*****************************************************************************
 systray.c

 Initialize and clean up the systray area; handle X protocal junk relating to
 systray communication.

 This source code is copyright (C) 2004 Matthew Reppert. Redistribution and
 use may occur under the terms of the GNU General Public License v2.
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <xembed.h>

#include "list.h"
#include "systray.h"
#include "trace.h"
#include "ui.h"


static Atom systray_atom = None;
static Atom opcode_atom = None;
static Atom message_atom = None;
struct list_head systray_list;
struct list_head *current_item = &systray_list;
int systray_item_count = 0, systray_current_item_no = 0;

int handle_dock_request (Window embed_wind);


/*
 * init_systray
 *
 * Initializes the system tray area and registers it with X.
 */
int init_systray() {
	char atom_name[22] = "_NET_SYSTEM_TRAY_S0";
	XEvent ev;

	INIT_LIST_HEAD (&systray_list);

	/*
	 * "On startup, the system tray must acquire a manager selection
	 *  called _NET_SYSTEM_TRAY_Sn, replacing n with the screen number
	 *  the tray wants to use." - freedesktop System Tray protocol
	 */
	snprintf(atom_name, 21, "_NET_SYSTEM_TRAY_S%d", DefaultScreen(main_disp));
	systray_atom = XInternAtom (main_disp, atom_name, False);
	XSetSelectionOwner (main_disp, systray_atom, sel_wind, CurrentTime);
	if (XGetSelectionOwner (main_disp, systray_atom) != sel_wind)
		return -1;

	opcode_atom = XInternAtom (main_disp, "_NET_SYSTEM_TRAY_OPCODE", False);
	message_atom = XInternAtom (main_disp, "_NET_SYSTEM_TRAY_MESSAGE_DATA",
				    False);

	/*
	 * Selection managers are required to broadcast their existence when
	 * they become selection managers.
	 */
	ev.type = ClientMessage;
	ev.xclient.message_type = XInternAtom (main_disp, "MANAGER", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = systray_atom; 
	ev.xclient.data.l[2] = sel_wind;
	ev.xclient.data.l[3] = 0;
	ev.xclient.data.l[4] = 0;
	XSendEvent (main_disp, DefaultRootWindow(main_disp), False,
		    StructureNotifyMask, &ev);

	return 0;
}



/*
 * cleanup_systray
 *
 * Deregisters us as the systray, informs all docked clients of our departure,
 * and cleans up any lingering resources.
 */
void cleanup_systray() {
	struct list_head *n;
	struct systray_item *item;

	if (systray_atom != None) 
		XSetSelectionOwner (main_disp, systray_atom, None, CurrentTime);

	list_for_each (n, &systray_list) {
		item = list_entry (n, struct systray_item, systray_list);
		xembed_unembed_window (main_disp, item->window_id);
	}
}



/*
 * event_is_systray_event
 *
 * Returns non-zero if an X event was sent from a systray client.
 */
int event_is_systray_event(XEvent *ev) {
	int ret = 0;

	/*
	 * "Tray icons can send "opcodes" to the system tray. These are X client
	 *  messages, sent with NoEventMask, a message_type of
	 *  _NET_SYSTEM_TRAY_OPCODE, and format 32." - fd.o System Tray protocol
	 */
	if (ev->xclient.type == ClientMessage &&
	    ev->xclient.message_type == opcode_atom &&
	    ev->xclient.format == 32)
	{
		ret = 1;
	}

	return ret;
}



/*
 * systray_property_update
 *
 * Handle a program changing its _XEMBED_INFO properties. Returns zero if
 * _XEMBED_INFO properties changed, nonzero if they didn't.
 */
int systray_property_update (struct systray_item *item) {
	struct xembed_info info;
	int flags_changed;

	TRACE((stderr, "ENTERING: systray_property_update\n"));

	xembed_get_info (main_disp, item->window_id, &info);
	flags_changed = info.flags ^ item->info.flags;
	if (flags_changed == 0)
		return -1;

	item->info.flags = info.flags;
	if (flags_changed & XEMBED_MAPPED) {
		if (info.flags & XEMBED_MAPPED) {
			TRACE2((stderr, "\tMapping %x on xembed info change\n",
				item->window_id));

			XMapRaised (main_disp, item->window_id);
			xembed_window_activate (main_disp, item->window_id);
		} else {
			TRACE2((stderr,"\tUnmapping %x on xembed info change\n",
				item->window_id));

			XUnmapWindow (main_disp, item->window_id);
			xembed_window_deactivate (main_disp, item->window_id);
			xembed_focus_out (main_disp, item->window_id);
		}
	}

	TRACE((stderr, "LEAVING: systray_property_update\n"));
	return 0;
}



/*
 * find_systray_item
 *
 * Given a window id, find the associated systray_item; if there is none,
 * return NULL.
 */
struct systray_item *find_systray_item (Window id) {
	struct list_head *n;
	struct systray_item *item;

	item = NULL;

	list_for_each (n, &systray_list) {
		item = list_entry (n, struct systray_item, systray_list);
		if (item->window_id == id)
			return item;
	}

	return NULL;
}



/*
 * handle_systray_event
 *
 * Deal with a systray event. (We assume that the given event has already been
 * verified to be a systray event, e.g. using event_is_systray_event.)
 *
 * The first data field is a timestap (e.g. CurrentTime); the second data
 * field is an integer indicating the opcode (SYSTEM_TRAY_XXX in systray.h).
 */
int handle_systray_event(XEvent *ev) {
	TRACE((stderr, "ENTERING: handle_systray_event\n"));

	TRACE((stderr, "Systray event to %x\n", ev->xclient.window));
	switch (ev->xclient.data.l[1]) {
	case SYSTEM_TRAY_REQUEST_DOCK:
		/*
		 * xclient.data.l2 contains the icon tray window ID.
		 */
		TRACE2((stderr, "DOCK REQUEST from window %x\n",
				 ev->xclient.data.l[2]));
		handle_dock_request (ev->xclient.data.l[2]);
		break;

	case SYSTEM_TRAY_BEGIN_MESSAGE:
		/*
		 * "xclient.data.l[2] contains the timeout in thousandths of
		 *  a second or zero for infinite timeout, ...l[3] contains
		 *  the lengeth of the message string in bytes, not including
		 *  any nul bytes, and ...l[4] contains an ID number for the
		 *  message."
		 */

		break;

	case SYSTEM_TRAY_CANCEL_MESSAGE:
		break;
	}

	TRACE((stderr, "LEAVING: handle_systray_event\n"));
	return 0;	
}



/*
 * systray_item_at_coords
 *
 * Return the item at the given point.
 */
struct systray_item *systray_item_at_coords (int x, int y) {
	int item;
	struct systray_item *ret;
	struct list_head *n;

	x -= 8;
	y -= 4;
	item = (y / 24) * 2 + x / 24;

	n = systray_list.next;
	while (item > 0) {
		n = n->next;
		item--;

		if (n == &systray_list)
			break;
	}

	if (n == &systray_list)
		ret = NULL;
	else
		ret = list_entry (n, struct systray_item, systray_list);

	return ret;
}



/*
 * repaint_systray
 *
 * Repaint our systray area.
 */
void repaint_systray() {
	struct systray_item *item;
	struct list_head *n;
	int x, y;
	int i = 0;

	TRACE((stderr, "ENTERING: repaint_systray\n"));

	draw_ui_elements();
	list_for_each (n, &systray_list) {
		item = list_entry (n, struct systray_item, systray_list);

		x = 8 + ((i % 2) * 24);
		y = 4 + ((i / 2) * 24);
		XMoveResizeWindow (main_disp, item->window_id, x, y, 24, 24);
		i++;

		if (i == 4)
			break;
	}

	XSync (main_disp, False);
	TRACE((stderr, "LEAVING: repaint_systray\n"));
}



/*
 * print_geometry
 *
 * Get a Window's geometry and print it.
 */
void print_geometry (Window embed_wind) {
	int x, y, width, height, border_width, depth;
	Window parent;

	XGetGeometry (main_disp, embed_wind, &parent, &x, &y,
			&width, &height, &border_width, &depth);
	TRACE((stderr, "\tEmbedded %x has parent %x (%x)\n",
			embed_wind, parent, main_wind));
	TRACE((stderr, "\t%dx%d+%d+%d, border %d, depth %d\n",
			width, height, x, y, border_width, depth));
}



/*
 * handle_dock_request
 *
 * Handle System Tray Protocol dock requests from windows.
 */
int handle_dock_request (Window embed_wind) {
	struct systray_item *item;
	struct xembed_info info;
	int new_x, new_y, status;
	long version;

	TRACE((stderr, "ENTERING: handle_dock_request\n"));
	new_x = new_y = 0;
	version = XEMBED_VERSION;

	/*
	 * XXX - temporarily reject if we've got four
	 */
	if (systray_item_count == 4) {
		XReparentWindow (main_disp, embed_wind,
				DefaultRootWindow(main_disp), 0, 0);
		TRACE((stderr, "REJECTED!\n"));
		return 1;
	}

	XSelectInput (main_disp, embed_wind, StructureNotifyMask |
						PropertyChangeMask);

	XWithdrawWindow (main_disp, embed_wind, 0);
	XReparentWindow (main_disp, embed_wind, draw_wind, 0, 0);
	XSync (main_disp, False);


	/*
	 * Insert the new item into our list of embedded systray clients.
	 */
	item = malloc (sizeof (struct systray_item));
	INIT_LIST_HEAD (& item->systray_list);
	memcpy (&item->info, &info, sizeof(info));
	item->window_id = embed_wind;
	list_add_tail (&item->systray_list, &systray_list);

	/*SelectInput was here*/
	status = xembed_get_info (main_disp, embed_wind, &info);
	XSync (main_disp, False);

	if (info.version < XEMBED_VERSION)
		version = info.version;

	TRACE2((stderr, "Using protocol version %ld\n", version));
	status = xembed_embedded_notify (main_disp, embed_wind, draw_wind,
					 version);

	/*
	 * XXX Unconditional mapping works for now, it's probably bad. Anyway,
	 * we need to track ProcessNotify events with the next changeset,
	 * probably.
	 *
	 * Actually, thinking about this coming home, it may be appropriate for
	 * a sys tray to have windows always thinking they're mappend and have
	 * focus.
	 *
	 * Also, unconditional mapping probably works because it's MapRaised,
	 * and thus embedded windows always get events even though we're not
	 * forwarding.
	 */
	if (info.flags & XEMBED_MAPPED) {
		TRACE((stderr, "Mapping\n"));
		XMapRaised (main_disp, embed_wind);
		/*
		status = xembed_window_activate (main_disp, embed_wind);
		status = xembed_focus_in (main_disp, embed_wind, 1);
		*/
	}

	/*
	 * Sync states with the embedded window.
	 */
	systray_item_count++;
	/*
	print_geometry (embed_wind);
	status = xembed_modality_off (main_disp, embed_wind);
	XSync (main_disp, False);
	*/

	repaint_systray();
	TRACE((stderr, "LEAVING: handle_dock_request\n"));
	return 0;
}
