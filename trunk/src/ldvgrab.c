// ldvgrab.c
// LiVES
// (c) G. Finch 2006 - 2015 <salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// portions of this file (c) Dan Dennedy (dan@dennedy.org)

#include <sys/poll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <errno.h>

#include <sys/time.h>
#include <sys/resource.h>

#define RX_CHANNEL 63
#define RAW_BUF_SIZE 4096

unsigned char g_rx_packet[RAW_BUF_SIZE]; /* the received packet data */
int g_rx_length; /* the size of a received packet */
int g_alldone = 0; /* flag to indicate when to quit */
int g_rx_channel = RX_CHANNEL;


//////////////////////////////////////////////////////////////////////////

#include "main.h"
#include "support.h"
#include "ldvinterface.h"
#include "ldvcallbacks.h"

int raw_iso_handler(raw1394handle_t handle, int channel, size_t length, quadlet_t *data) {
  if (length < RAW_BUF_SIZE && channel == g_rx_channel) {
    g_rx_length = length;
    lives_memcpy(g_rx_packet, data, length);
  }
  return 0;
}

/* libraw1394 executes this when there is a bus reset. We'll just keep it
   simple and quit */
int reset_handler(raw1394handle_t handle, unsigned int generation) {
  raw1394_update_generation(handle, generation);
  on_camquit_clicked(NULL,dvgrabw->cam);
  return 0;
}


raw1394handle_t open_raw1394(void) {
  int numcards;
  struct raw1394_portinfo pinf[16];
  raw1394handle_t handle;
  struct pollfd raw1394_poll;

  if (!(handle = raw1394_new_handle())) {
    d_print(_("raw1394 - couldn't get handle"));
    do_error_dialog(_("\nThe ieee1394 driver is not loaded or /dev/raw1394 does not exist.\n"));
    return NULL;
  }

  if ((numcards = raw1394_get_port_info(handle, pinf, 16)) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't get card info.\n"));
    return NULL;
  }

  /* port 0 is the first host adapter card */
  if (raw1394_set_port(handle, 0) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't set port.\n"));
    return NULL;
  }

  /* tell libraw1394 the names of our callback functions */
  //raw1394_set_iso_handler(handle, g_rx_channel, raw_iso_handler);
  raw1394_set_bus_reset_handler(handle, reset_handler);

  /* poll for leftover events */
  raw1394_poll.fd = raw1394_get_fd(handle);
  raw1394_poll.events = POLLIN;

  while (1) {
    if (poll(&raw1394_poll, 1, 10) < 1) break;
    raw1394_loop_iterate(handle);
  }

  /* Starting iso receive */
  /*  if (raw1394_start_iso_rcv(handle, g_rx_channel) < 0) {
    do_error_dialog(_("\nraw1394 - couldn't start iso receive.\n"));
    return NULL;
    }*/
  return handle;
}


void close_raw1394(raw1394handle_t handle) {
  //raw1394_stop_iso_rcv(handle, g_rx_channel);
  raw1394_destroy_handle(handle);
}


void camdest(s_cam *cam) {
  raw1394_destroy_handle(cam->handle);
  lives_free(cam);
}

s_cam *camready(void) {
  rom1394_directory rom_dir;

  struct raw1394_portinfo pinf[16];

  s_cam *cam=(s_cam *)lives_malloc(sizeof(s_cam));

  char *msg;

  int n_ports;

  register int i,j;

  cam->device=-1;

#ifdef RAW1394_V_0_8
  cam->handle = raw1394_get_handle();
#else
  cam->handle = raw1394_new_handle();
#endif

  if (!cam->handle) {
    if (!errno) {
      do_error_dialog(_("\nraw1394 device not compatible!\n"));
    } else {
      d_print(_("Couldn't get 1394 handle"));
      do_error_dialog(_("\nIs ieee1394, driver, and raw1394 loaded?\n"));
    }
    return NULL;
  }

  if ((n_ports = raw1394_get_port_info(cam->handle, pinf, 16)) < 0) {
    msg=lives_strdup_printf(_("raw1394 - failed to get port info: %s.\n"), lives_strerror(errno));
    d_print(msg);
    lives_free(msg);
    raw1394_destroy_handle(cam->handle);
    return NULL;
  }



  for (j = 0; j < n_ports && cam->device == -1; j++) {

    if (raw1394_set_port(cam->handle, j) < 0) {
      msg=lives_strdup_printf(_("\nraw1394 - couldn't set port %d !\n"),j);
      d_print(msg);
      lives_free(msg);
      continue;
    }

    for (i=0; i < raw1394_get_nodecount(cam->handle); ++i) {

      if (rom1394_get_directory(cam->handle, i, &rom_dir) < 0) {
        msg=lives_strdup_printf(_("error reading config rom directory for node %d\n"), i);
        d_print(msg);
        lives_free(msg);
        continue;
      }

      if ((rom1394_get_node_type(&rom_dir) == ROM1394_NODE_TYPE_AVC) &&
          avc1394_check_subunit_type(cam->handle, i, AVC1394_SUBUNIT_TYPE_VCR)) {
        cam->device = i;
        break;
      }
    }
  }

  if (0&&cam->device == -1) {
    do_error_dialog(
      _("\nLiVES could not find any firewire camera.\nPlease make sure your camera is switched on,\nand check that you have read/write permissions for the camera device\n(generally /dev/raw1394*).\n"));
    raw1394_destroy_handle(cam->handle);
    return NULL;
  }

  return cam;
}






//////////////////////////////////////////////////////////////////////////////////////////////

void camplay(s_cam *cam) {
  avc1394_vcr_play(cam->handle, cam->device);
}

void camstop(s_cam *cam) {
  g_alldone=1;
  avc1394_vcr_stop(cam->handle, cam->device);
}

void camrew(s_cam *cam) {
  avc1394_vcr_rewind(cam->handle, cam->device);
}

void camff(s_cam *cam) {
  avc1394_vcr_forward(cam->handle, cam->device);
}

void campause(s_cam *cam) {
  avc1394_vcr_pause(cam->handle, cam->device);
}

void cameject(s_cam *cam) {
  avc1394_vcr_eject(cam->handle, cam->device);
}



////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


char *find_free_camfile(int format) {
  char *filename=lives_strdup(lives_entry_get_text(LIVES_ENTRY(dvgrabw->filent)));
  char *fname,*tmp=NULL,*tmp2,*tmp3;

  register int i;

  if (format==CAM_FORMAT_HDV) {
    for (i=1; i<10000; i++) {
      fname=lives_strdup_printf("%s%04d.mpg",filename,i);
      if (!lives_file_test((tmp=lives_build_filename((tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                                (tmp3=lives_filename_from_utf8(fname,-1,NULL,NULL,NULL)),NULL)),
                           LIVES_FILE_TEST_EXISTS)) break;
      lives_free(tmp);
      lives_free(tmp2);
      lives_free(tmp3);
      tmp=NULL;
    }
  } else {
    for (i=1; i<1000; i++) {
      fname=lives_strdup_printf("%s%03d.dv",filename,i);
      if (!lives_file_test((tmp=lives_build_filename((tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                                (tmp3=lives_filename_from_utf8(fname,-1,NULL,NULL,NULL)),NULL)),
                           LIVES_FILE_TEST_EXISTS)) break;
      lives_free(tmp);
      lives_free(tmp2);
      lives_free(tmp3);
      tmp=NULL;
    }
  }
  if (tmp!=NULL) lives_free(tmp);
  lives_free(filename);

  return fname;
}


boolean rec(s_cam *cam) {
  // returns filename of file being written

  char *tmp2,*tmp3,*com;
  char *splits;

  if (cam->pgid!=0) return FALSE;

  if (lives_toggle_button_get_active(LIVES_TOGGLE_BUTTON(dvgrabw->split))) splits=lives_strdup("-autosplit ");
  else splits=lives_strdup("");

  if (cam->format==CAM_FORMAT_DV) {
    // dv format
#ifndef IS_MINGW
    com=lives_strdup_printf("dvgrab -format raw %s\"%s/%s\" >/dev/null 2>&1 &",splits,
                            (tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                            (tmp3=lives_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
#else
    com=lives_strdup_printf("dvgrab.exe -format raw %s\"%s/%s\" >NUL 2>&1 &",splits,
                            (tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                            (tmp3=lives_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
#endif
    cam->pgid=lives_fork(com);
    lives_free(com);
    lives_free(tmp2);
    lives_free(tmp3);
    lives_free(splits);
    return TRUE;
  }

  // hdv format
#ifndef IS_MINGW
  com=lives_strdup_printf("dvgrab -format mpeg2 %s\"%s/%s\" >/dev/null 2>&1 &",splits,
                          (tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                          (tmp3=lives_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
#else
  com=lives_strdup_printf("dvgrab.exe -format mpeg2 %s\"%s/%s\" >NUL 2>&1 &",splits,
                          (tmp2=lives_filename_from_utf8(dvgrabw->dirname,-1,NULL,NULL,NULL)),
                          (tmp3=lives_filename_from_utf8(dvgrabw->filename,-1,NULL,NULL,NULL)));
#endif

  cam->pgid=lives_fork(com);

  lives_free(com);
  lives_free(tmp2);
  lives_free(tmp3);
  lives_free(splits);

  return TRUE;
}



void on_open_fw_activate(LiVESMenuItem *menuitem, livespointer user_data) {
  int type=LIVES_POINTER_TO_INT(user_data); // type 0==dv, type 1==hdv
  s_cam *cam;

  if (type==CAM_FORMAT_DV&&!capable->has_dvgrab) {
    do_dvgrab_error();
    return;
  }

  cam=camready();
  if (cam==NULL) return;

  /*  if (type==CAM_FORMAT_HDV) {
    cam->rec_handle=open_raw1394();
    if (cam->rec_handle==NULL) return;
  }
  else*/
  cam->rec_handle=NULL;

  if (mainw->multitrack!=NULL) {
    if (mainw->multitrack->idlefunc>0) {
      lives_source_remove(mainw->multitrack->idlefunc);
      mainw->multitrack->idlefunc=0;
    }
    mt_desensitise(mainw->multitrack);
  }

  dvgrabw = create_camwindow(cam,type);
  lives_widget_show_all(dvgrabw->dialog);
  dvgrabw->cursor=NULL;
  cam->format=type;
  cam->grabbed_clips=FALSE;
  cam->pgid=0;
  dvgrabw->cam=cam;
}

