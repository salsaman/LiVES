/* videojack test plugin for Weed
   authors: Salsaman (G. Finch) <salsaman@xs4all.nl,salsaman@gmail.com>

// released under the GNU GPL 3 or higher
// see file COPYING or www.gnu.org for details

 (c) 2005, project authors
*/

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-plugin.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#include "../../../libweed/weed-plugin.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed-plugin-utils.h> // optional
#else
#include "../../../libweed/weed-plugin-utils.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////


#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <jack/jack.h>
#include <jack/video.h>
#include <jack/ringbuffer.h>

/////////////////////////////////////////////////////////////
// gdk stuff for resizing


#include <gdk/gdk.h>

inline G_GNUC_CONST int pl_gdk_rowstride_value(int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}

inline int G_GNUC_CONST pl_gdk_last_rowstride_value(int width, int nchans) {
  // from gdk pixbuf docs
  return width*(((nchans<<3)+7)>>3);
}

static void plugin_free_buffer(guchar *pixels, gpointer data) {
  return;
}


static inline GdkPixbuf *pl_gdk_pixbuf_cheat(GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample, int width, int height,
    guchar *buf) {
  // we can cheat if our buffer is correctly sized
  int channels=has_alpha?4:3;
  int rowstride=pl_gdk_rowstride_value(width*channels);
  return gdk_pixbuf_new_from_data(buf, colorspace, has_alpha, bits_per_sample, width, height, rowstride, plugin_free_buffer, NULL);
}



static GdkPixbuf *pl_data_to_pixbuf(int palette, int width, int height, int irowstride, guchar *pixel_data) {
  GdkPixbuf *pixbuf;
  int rowstride,orowstride;
  gboolean cheat=FALSE;
  gint n_channels;
  guchar *pixels,*end;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    if (irowstride==pl_gdk_rowstride_value(width*3)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, FALSE, 8, width, height, pixel_data);
      cheat=TRUE;
    } else pixbuf=gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
    n_channels=3;
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32: // TODO - change to RGBA ??
    if (irowstride==pl_gdk_rowstride_value(width*4)) {
      pixbuf=pl_gdk_pixbuf_cheat(GDK_COLORSPACE_RGB, TRUE, 8, width, height, pixel_data);
      cheat=TRUE;
    } else pixbuf=gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, width, height);
    n_channels=4;
    break;
  default:
    return NULL;
  }
  pixels=gdk_pixbuf_get_pixels(pixbuf);
  orowstride=gdk_pixbuf_get_rowstride(pixbuf);

  if (irowstride>orowstride) rowstride=orowstride;
  else rowstride=irowstride;
  end=pixels+orowstride*height;

  if (!cheat) {
    gboolean done=FALSE;
    for (; pixels<end&&!done; pixels+=orowstride) {
      if (pixels+orowstride>=end) {
        orowstride=rowstride=pl_gdk_last_rowstride_value(width,n_channels);
        done=TRUE;
      }
      weed_memcpy(pixels,pixel_data,rowstride);
      if (rowstride<orowstride) weed_memset(pixels+rowstride,0,orowstride-rowstride);
      pixel_data+=irowstride;
    }
  }
  return pixbuf;
}



static gboolean pl_pixbuf_to_channel(weed_plant_t *channel, GdkPixbuf *pixbuf) {
  // return TRUE if we can use the original pixbuf pixels

  int error;
  int rowstride=gdk_pixbuf_get_rowstride(pixbuf);
  int width=gdk_pixbuf_get_width(pixbuf);
  int height=gdk_pixbuf_get_height(pixbuf);
  int n_channels=gdk_pixbuf_get_n_channels(pixbuf);
  guchar *in_pixel_data=(guchar *)gdk_pixbuf_get_pixels(pixbuf);
  int out_rowstride=weed_get_int_value(channel,"rowstrides",&error);
  guchar *dst=weed_get_voidptr_value(channel,"pixel_data",&error);

  register int i;

  if (rowstride==pl_gdk_last_rowstride_value(width,n_channels)&&rowstride==out_rowstride) {
    weed_memcpy(dst,in_pixel_data,rowstride*height);
    return FALSE;
  }

  for (i=0; i<height; i++) {
    if (i==height-1) rowstride=pl_gdk_last_rowstride_value(width,n_channels);
    weed_memcpy(dst,in_pixel_data,rowstride);
    in_pixel_data+=rowstride;
    dst+=out_rowstride;
  }

  return FALSE;
}


///////////////////////////////////////////////////////

static int instances;

typedef struct {
  jack_port_t *input_port;
  jack_ringbuffer_t *rb;
  jack_client_t *client;
#define SMOOTH
#ifdef SMOOTH
  unsigned char *bgbuf;
#endif
} sdata;



int server_process(jack_nframes_t nframes, void *arg) {
  // this is called by jack when a frame is received
  sdata *sd=(sdata *)arg;
  unsigned int width=jack_video_get_width(sd->client,sd->input_port);
  unsigned int height=jack_video_get_height(sd->client,sd->input_port);
  unsigned int frame_size;
  uint8_t *in = jack_port_get_buffer(sd->input_port, nframes);

  frame_size = width * height * 4;

  if (frame_size==0||in==NULL||sd->rb==NULL) return 0;

  // enough space for one more frame
  if (jack_ringbuffer_write_space(sd->rb) >= frame_size) {
    jack_ringbuffer_write(sd->rb, (void *) in, frame_size);
  } else {
    //fprintf (stderr, "drop frame\n");
  }

  return 0;
}




/* declare our init function */
int vjack_rcv_init(weed_plant_t *inst) {
  weed_plant_t *out_channel;
  int error;
  unsigned int out_frame_size;
  const char *assigned_client_name;
  char *server_name,*conffile;
  jack_options_t options = JackServerName;
  jack_status_t status;
  weed_plant_t **in_params;
  sdata *sd;
  char *client_name="Weed-receiver";
  unsigned int out_height,out_width;
  //char com[512];
  double jack_sample_rate;

  sd=weed_malloc(sizeof(sdata));
  if (sd==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  server_name=weed_get_string_value(in_params[0],"value",&error);
  conffile=weed_get_string_value(in_params[1],"value",&error);
  weed_free(in_params);

  instances++;

  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  out_height=weed_get_int_value(out_channel,"height",&error);
  out_width=weed_get_int_value(out_channel,"width",&error);

  out_frame_size=out_width*out_height*4;

#ifdef SMOOTH
  sd->bgbuf=weed_malloc(out_frame_size);
  if (sd->bgbuf==NULL) {
    weed_free(sd);
    return WEED_ERROR_MEMORY_ALLOCATION;
  }
  weed_memset(sd->bgbuf,0,out_frame_size);
#endif

  // use user defined rc file
  //  system("/bin/mv -f ~/.jackdrc ~/.jackdrc._bak>/dev/null 2>&1");
  //snprintf(com,512,"/bin/ln -s %s ~/.jackdrc>/dev/null 2>&1",conffile);
  //system(com);

  sd->client = jack_client_open(client_name, options, &status, server_name);
  weed_free(server_name);
  weed_free(conffile);

  //  system("/bin/mv -f ~/.jackdrc._bak ~/.jackdrc>/dev/null 2>&1");

  if (sd->client==NULL) {
    fprintf(stderr, "jack_client_open() failed, "
            "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf(stderr, "Unable to connect to JACK server\n");
    }
#ifdef SMOOTH
    weed_free(sd->bgbuf);
#endif
    weed_free(sd);
    return WEED_ERROR_INIT_ERROR;
  }
  if (status & JackServerStarted) {
    fprintf(stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    assigned_client_name = jack_get_client_name(sd->client);
    fprintf(stderr, "unique name `%s' assigned\n", assigned_client_name);
  }

  fprintf(stderr,"engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate(sd->client));



  sd->input_port = jack_port_register(sd->client,
                                      "video_in",
                                      JACK_DEFAULT_VIDEO_TYPE,
                                      JackPortIsInput,
                                      0);

  if (sd->input_port==NULL) {
    fprintf(stderr, "no more JACK ports available\n");
#ifdef SMOOTH
    weed_free(sd->bgbuf);
#endif
    weed_free(sd);
    return WEED_ERROR_INIT_ERROR;
  }

  // set process callback and start

  jack_set_process_callback(sd->client, server_process, sd);

  if (jack_activate(sd->client)) {
    fprintf(stderr, "cannot activate client");
#ifdef SMOOTH
    weed_free(sd->bgbuf);
#endif
    weed_free(sd);
    return WEED_ERROR_INIT_ERROR;
  }

  sd->rb=NULL;

  //jack_on_shutdown (client, jack_shutdown, 0);
  weed_set_voidptr_value(inst,"plugin_internal",sd);

  jack_sample_rate=jack_get_sample_rate(sd->client);

  weed_set_double_value(inst,"target_fps",jack_sample_rate); // set reasonable value
  return WEED_NO_ERROR;
}


int vjack_rcv_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  // TODO check for server shutdown
  int error;
  unsigned int frame_size;
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);
  int out_width=weed_get_int_value(out_channel,"width",&error);
  int out_height=weed_get_int_value(out_channel,"height",&error);
  int wrote=0;
  sdata *sd=(sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  unsigned int in_width=jack_video_get_width(sd->client,sd->input_port);
  unsigned int in_height=jack_video_get_height(sd->client,sd->input_port);

  char *tmpbuff=NULL;

  GdkPixbuf *in_pixbuf,*out_pixbuf;

  int up_interp=GDK_INTERP_HYPER;
  int down_interp=GDK_INTERP_BILINEAR;

  frame_size = in_width*in_height*4;

  if (frame_size==0) return WEED_NO_ERROR;  // not connected to an output

  // communication structure between process and display thread
  if (sd->rb==NULL) sd->rb = jack_ringbuffer_create(2*in_width*in_height*4);

  if (in_width==out_width&&in_height==out_height) {
    while (jack_ringbuffer_read_space(sd->rb) >= frame_size) {
      jack_ringbuffer_read(sd->rb, (char *)dst, frame_size);
      wrote=1;
    }

#ifdef SMOOTH
    if (!wrote) weed_memcpy(dst,sd->bgbuf,frame_size);
    else weed_memcpy(sd->bgbuf,dst,frame_size);
#endif

    return WEED_NO_ERROR;
  }

  // resize needed

  while (jack_ringbuffer_read_space(sd->rb) >= frame_size) {
    if (tmpbuff==NULL) tmpbuff = weed_malloc(frame_size);
    jack_ringbuffer_read(sd->rb, tmpbuff, frame_size);
    wrote=1;
  }

  frame_size = out_width*out_height*4;

  if (!wrote) {
    weed_memcpy(dst,sd->bgbuf,frame_size);
    return WEED_NO_ERROR;
  }

  in_pixbuf=pl_data_to_pixbuf(WEED_PALETTE_RGBA32, in_width, in_height, in_width*4, (guchar *)tmpbuff);

  if (out_width>in_width||out_height>in_height) {
    out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,out_width,out_height,up_interp);
  } else {
    out_pixbuf=gdk_pixbuf_scale_simple(in_pixbuf,out_width,out_height,down_interp);
  }

  g_object_unref(in_pixbuf);

  weed_free(tmpbuff);

  pl_pixbuf_to_channel(out_channel,out_pixbuf);

  g_object_unref(out_pixbuf);

#ifdef SMOOTH
  weed_memcpy(sd->bgbuf,dst,frame_size);
#endif

  return WEED_NO_ERROR;
}




int vjack_rcv_deinit(weed_plant_t *inst) {
  int error;
  sdata *sd=(sdata *)weed_get_voidptr_value(inst,"plugin_internal",&error);

  jack_deactivate(sd->client);
  jack_client_close(sd->client);
  if (--instances<0) instances=0;

#ifdef SMOOTH
  weed_free(sd->bgbuf);
#endif

  if (sd->rb!=NULL) jack_ringbuffer_free(sd->rb);

  weed_free(sd);

  return WEED_NO_ERROR; // success
}





weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_RGBA32,WEED_PALETTE_END};

    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,palette_list),NULL};

    weed_plant_t *in_params[]= {weed_text_init("servername","_Server name","default"),weed_text_init("conffile","_Config file","~/.jackdrc.vjack"),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("vjack_rcv","martin/salsaman",1,0,&vjack_rcv_init,&vjack_rcv_process,&vjack_rcv_deinit,
                               NULL,out_chantmpls,in_params,NULL);

    weed_plant_t *gui=weed_parameter_template_get_gui(in_params[0]);
    weed_set_int_value(gui,"maxchars",32);

    gui=weed_parameter_template_get_gui(in_params[1]);
    weed_set_int_value(gui,"maxchars",128);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}



