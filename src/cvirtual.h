// cvirtual.h
// LiVES
// (c) G. Finch 2008 - 2009 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details


// functions for handling "virtual" clips (CLIP_TYPE_FILE)


void create_frame_index(gint fileno, gboolean init, gint start_offset, gint nframes);
gboolean save_frame_index(gint fileno);
gboolean load_frame_index(gint fileno) WARN_UNUSED;
gboolean check_clip_integrity(file *sfile, const lives_clip_data_t *cdata);

gboolean virtual_to_images(gint sfileno, gint sframe, gint eframe, gboolean update_progress) WARN_UNUSED;
void delete_frames_from_virtual (gint sfileno, gint start, gint end);
void insert_images_in_virtual (gint sfileno, gint where, gint frames);
void del_frame_index(file *sfile);
void clean_images_from_virtual (file *sfile, gint oldframes);
int *frame_index_copy(int *findex, gint nframes);
gboolean check_if_non_virtual(gint fileno, gint start, gint end);

void restore_frame_index_back (gint sfileno);

gboolean is_virtual_frame(int sfileno, int frame);

gint count_virtual_frames(int *findex, int start, int end);
