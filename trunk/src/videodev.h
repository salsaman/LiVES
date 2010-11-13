// LiVES - videodev input
// (c) G. Finch 2010 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_UNICAP

#include <unicap/unicap.h>

typedef struct {
  unicap_handle_t handle;
  unicap_data_buffer_t buffer;
  int current_palette;
} lives_vdev_t;


#define MAX_DEVICES 1024
#define MAX_FORMATS 1024

gboolean weed_layer_set_from_lvdev (weed_plant_t *layer, file *sfile);
void on_openvdev_activate (GtkMenuItem *, gpointer);


#endif

