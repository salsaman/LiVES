// cvirtual.h
// LiVES
// (c) G. Finch 2008 - 2013 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions for handling "virtual" clips (CLIP_TYPE_FILE)

#ifndef HAS_LIVES_CVIRTUAL_H
#define HAS_LIVES_CVIRTUAL_H


void create_frame_index(int fileno, boolean init, int start_offset, int nframes);
boolean save_frame_index(int fileno);
boolean load_frame_index(int fileno) WARN_UNUSED;
boolean check_clip_integrity(int fileno, const lives_clip_data_t *cdata);

boolean virtual_to_images(int sfileno, int sframe, int eframe, boolean update_progress, LiVESPixbuf **pbr) WARN_UNUSED;
void delete_frames_from_virtual(int sfileno, int start, int end);
void insert_images_in_virtual(int sfileno, int where, int frames, int *frame_index, int start);
void del_frame_index(lives_clip_t *sfile);
void reverse_frame_index(int sfileno);
void clean_images_from_virtual(lives_clip_t *sfile, int oldframes);
int *frame_index_copy(int *findex, int nframes, int offset);
boolean check_if_non_virtual(int fileno, int start, int end);

void restore_frame_index_back(int sfileno);

boolean is_virtual_frame(int sfileno, int frame);

int count_virtual_frames(int *findex, int start, int end);


#endif
