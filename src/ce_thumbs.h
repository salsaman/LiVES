// ce_thumbs.h
// LiVES
// (c) G. Finch 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef HAS_LIVES_CE_THUMBS_H
#define HAS_LIVES_CE_THUMBS_H


void start_ce_thumb_mode(void);
void end_ce_thumb_mode(void);

void ce_thumbs_set_interactive(boolean interactive);

void ce_thumbs_set_keych(int key, boolean on);
void ce_thumbs_set_mode_combo(int key, int mode);

void ce_thumbs_add_param_box(int key, boolean remove);

void ce_thumbs_set_key_check_state(void);

void ce_thumbs_register_rfx_change(int key, int mode);
void ce_thumbs_apply_rfx_changes(void);
void ce_thumbs_update_params(int key, int i, LiVESList *list);

void ce_thumbs_update_visual_params(int key);
void ce_thumbs_check_for_rte(lives_rfx_t *ce_rfx, lives_rfx_t *rte_rfx, int key);

void ce_thumbs_reset_combos(void);
void ce_thumbs_reset_combo(int key);

void ce_thumbs_set_clip_area(void);
void ce_thumbs_set_fx_area(int area);

void ce_thumbs_update_current_clip(void);
void ce_thumbs_highlight_current_clip(void);

void ce_thumbs_liberate_clip_area(boolean liberate);
void ce_thumbs_liberate_clip_area_register(boolean liberate);
void ce_thumbs_apply_liberation(void);

#endif // HAS_LIVES_CE_THUMBS_H
