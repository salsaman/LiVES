// cvirtual.h
// LiVES
// (c) G. Finch 2008 - 2018 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

// functions for handling "virtual" clips (CLIP_TYPE_FILE)

#ifndef HAS_LIVES_CVIRTUAL_H
#define HAS_LIVES_CVIRTUAL_H

#define FRAME_INDEX_FNAME "file_index"

boolean create_frame_index(int clipno, boolean init, frames_t start_offset, frames_t nframes);
boolean save_frame_index(int clipno);
frames_t load_frame_index(int clipno) WARN_UNUSED;
boolean check_clip_integrity(int clipno, const lives_clip_data_t *cdata, frames_t maxframe);
lives_img_type_t resolve_img_type(lives_clip_t *);
boolean repair_frame_index(int clipno, frames_t offs);
void repair_findex_cb(LiVESMenuItem *, livespointer offsp);

frames_t virtual_to_images(int sclipno, frames_t sframe, frames_t eframe, boolean update_progress, LiVESPixbuf **pbr);
void delete_frames_from_virtual(int sclipno, frames_t start, frames_t end);
void insert_images_in_virtual(int sclipno, frames_t where, frames_t frames, frames_t *frame_index, frames_t start);
void del_frame_index(int sclipno);
void reverse_frame_index(int sclipno);

boolean realize_all_frames(int clipno, const char *msg, boolean enough, frames_t start, frames_t end);

/*
   @brief remove rendered (real) frames from region oldsframe -> oldframes, when they are virtual in current frame_index
*/
void clean_images_from_virtual(lives_clip_t *, frames_t oldsframe, frames_t oldframes);
int *frame_index_copy(frames_t *findex, frames_t nframes, frames_t offset);

//frames_t first_virtual_frame(int clipno, frames_t start, frames_t end);
boolean check_if_non_virtual(int clipno, frames_t start, frames_t end);

void restore_frame_index_back(int sclipno);

boolean is_virtual_frame(int sclipno, frames_t frame);

frames_t count_virtual_frames(frames_t *findex, frames_t start, frames_t end);

void insert_blank_frames(int sclipno, frames_t nframes, frames_t after, int palette);

#define get_indexed_frame(clip, frame) ((IS_PHYSICAL_CLIP((clip)) && (frame) > 0 && (frame)\
					 <= mainw->files[(clip)]->frames) \
					? (mainw->files[(clip)]->frame_index \
					   ? (mainw->files[(clip)]->frame_index[((frame) - 1)] < 0 ? -(frame) : \
					      mainw->files[(clip)]->frame_index[((frame) - 1)]) : -(frame)) : 1)

frames_t *trim_frame_index(int clipno, frames_t *ref_frames, frames_t start, frames_t len);

#define get_alt_indexed_frame(clip, frame)				\
  ((frame) <= mainw->files[(clip)]->alt_frames				\
   ? get_indexed_frame((clip), (mainw->files[(clip)]->alt_frame_index[((frame) - 1)] + 1)) \
   : get_indexed_frame((clip), (frame)))

#endif
