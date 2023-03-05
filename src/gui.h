// gui.h
// (c) G. Finch 2002 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _GUI_H_
#define _GUI_H_

void  create_LiVES(void);
void show_lives(void);
void set_colours(LiVESWidgetColor *colf, LiVESWidgetColor *colb, LiVESWidgetColor *colf2,
                 LiVESWidgetColor *colb2, LiVESWidgetColor *coli, LiVESWidgetColor *colt);
void set_preview_box_colours(void);
void load_theme_images(void);
void set_interactive(boolean interactive);
char *get_menu_name(lives_clip_t *sfile, boolean add_set);
int get_sepbar_height(void);
void enable_record(void);
void toggle_record(void);
void disable_record(void);
void make_custom_submenus(void);
void fade_background(void);
void unfade_background(void);
void fullscreen_internal(void);
void block_expose(void);
void unblock_expose(void);
void frame_size_update(void);
void splash_init(void);
void splash_end(void);
void splash_msg(const char *msg, double pct);
void hide_main_gui(void);
void unhide_main_gui(void);
void resize_widgets_for_monitor(boolean get_play_times);
void reset_message_area(void);
void get_letterbox_sizes(int *pwidth, int *pheight, int *lb_width, int *lb_height, boolean player_can_upscale);

#if GTK_CHECK_VERSION(3, 0, 0)
void calibrate_sepwin_size(void);
boolean expose_pim(LiVESWidget *, lives_painter_t *, livespointer);
boolean expose_sim(LiVESWidget *, lives_painter_t *, livespointer);
boolean expose_eim(LiVESWidget *, lives_painter_t *, livespointer);
#endif

#endif
