// LiVES - vjack playback engine
// (c) G. Finch 2006 - 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#include "videoplugin.h"

#include <stdio.h>

/////////////////////////////////////////////////////////////////

static char plugin_version[64]="LiVES videojack output client 1.1";
static char fps_list[256];
static int palette_list[2];
static int mypalette;

//////////////////////////////////////////////////////////////////

#include <jack/jack.h>
#include <jack/video.h>
#include <jack/ringbuffer.h>

static jack_ringbuffer_t *rb;
static size_t frame_size;
static jack_port_t *output_port;
static jack_client_t *client;

int server_process(jack_nframes_t nframes, void *arg) {
  // jack calls this
  uint8_t *in;

  if (rb==NULL) return 0;

  in = jack_port_get_buffer(output_port, nframes);
  while (jack_ringbuffer_read_space(rb) >= frame_size) {
    jack_ringbuffer_read(rb, (void *) in, frame_size);
  }
  return 0;
}

//////////////////////////////////////////////
const char *get_fps_list(int palette) {
  double fps=(double)jack_get_sample_rate(client);
  snprintf(fps_list,256,"%.8f",fps);
  return fps_list;
}

const char *module_check_init(void) {
  const char *client_name="LiVES";
  jack_options_t options = JackNullOption;
  jack_status_t status;
  const char *server_name=NULL; // set to "video0" if running with audio on "default"

  // connect to vjack
  client = jack_client_open(client_name, options, &status, server_name);
  if (client == NULL) {
    fprintf(stderr, "jack_client_open() failed, "
            "status = 0x%2.0x\n", status);
    if (status & JackServerFailed) {
      fprintf(stderr, "vjack_output: Unable to connect to JACK server\n");
    }
    return "unable to connect";
  }
  if (status & JackServerStarted) {
    fprintf(stderr, "JACK server started\n");
  }
  if (status & JackNameNotUnique) {
    client_name = jack_get_client_name(client);
    fprintf(stderr, "unique name `%s' assigned\n", client_name);
  }

  fprintf(stderr,"engine sample rate: %" PRIu32 "\n",
          jack_get_sample_rate(client));

  output_port = jack_port_register(client,
                                   "video_out",
                                   JACK_DEFAULT_VIDEO_TYPE,
                                   JackPortIsOutput,
                                   0);

  if (output_port==NULL) {
    fprintf(stderr, "vjack_output: no more JACK ports available\n");
    return "no jack ports available";
  }

  rb = NULL;

  // set process callback and start
  jack_set_process_callback(client, server_process, NULL);

  // and start the processing
  if (jack_activate(client)) {
    fprintf(stderr, "vjack_output: cannot activate client\n");
    return "cannot activate client";
  }

  return NULL;
}

const char *version(void) {
  return plugin_version;
}

const char *get_description(void) {
  return "The vjack_output plugin allows sending frames to videojack.\nThis is an experimental plugin\n";
}

uint64_t get_capabilities(int palette) {
  return 0;
}

int *get_palette_list(void) {
  palette_list[0]=WEED_PALETTE_RGBA32;
  palette_list[1]=WEED_PALETTE_END;
  return palette_list;
}

boolean set_palette(int palette) {
  if (palette==WEED_PALETTE_RGBA32) {
    mypalette=palette;
    return TRUE;
  }
  // invalid palette
  return FALSE;
}

boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv) {
  jack_video_set_width_and_height(client,output_port,width,height);
  frame_size=width*height*4;
  rb = jack_ringbuffer_create(16 * frame_size);
  return TRUE;
}

boolean render_frame(int hsize, int vsize, int64_t tc, void **pixel_data, void **return_data) {
  // hsize and vsize are in pixels (n-byte)

  jack_ringbuffer_write(rb, pixel_data[0], frame_size);
  return TRUE;
}

void exit_screen(int16_t mouse_x, int16_t mouse_y) {
  if (rb!=NULL) jack_ringbuffer_free(rb);
  rb=NULL;
}

void module_unload(void) {
  if (jack_deactivate(client)) {
    fprintf(stderr, "vjack_output error: cannot deactivate client\n");
  }

  // disconnect from vjack
  jack_client_close(client);
  fprintf(stderr, "vjack_output: jack port unregistered\n");
}
