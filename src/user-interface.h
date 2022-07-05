// user-interface.h.
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _USER_INTERFACE_H
#define _USER_INTERFACE_H

// functions which deal with the overal look and feel of the interface, rather than individual widgets

void sensitize(void);
void sensitize_rfx(void);
void desensitize(void);
void procw_desensitize(void);

boolean set_palette_colours(boolean force_reload);

void set_main_title(const char *filename, int or_untitled_number);

#endif
