// LiVES - video playback plugin header
// (c) G. Finch 2003 - 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef __VIDPLUGIN_H__
#define __VIDPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <inttypes.h>

#include "../../../../libweed/weed-palettes.h"

typedef int boolean;
#undef TRUE
#undef FALSE

#define TRUE 1
#define FALSE 0

#define CPU_BITS ((sizeof(void *))<<3)

// all playback modules need to implement these functions, unless they are marked (optional)

// host calls at startup
const char *module_check_init (void);
const char *version (void);
const char *get_description (void);  // optional
const char *get_rfx (void);  // optional

// plugin send list of palettes, in order of preference
int *get_palette_list(void);

// host sets the palette used
boolean set_palette (int palette);

// host will call this 
uint64_t get_capabilities (int palette);

#define VPP_CAN_RESIZE    1<<0
#define VPP_CAN_RETURN    1<<1
#define VPP_LOCAL_DISPLAY 1<<2


// ready the screen to play (optional)
boolean init_screen (int width, int height, boolean fullscreen, uint32_t window_id, int argc, char **argv);

// display one frame, adding effects if you like,
// and resizing it to screen size if possible (VPP_CAN_RESIZE)
// if return_data is non-NULL, you should either fill it with the effected,
// unresized data (VPP_CAN_RETURN) or set it back to NULL if you can't
// hsize and vsize are width and height of the pixel data (in macropixels)
boolean render_frame (int hsize, int vsize, int64_t timecode, void **pixel_data, void **return_data);

// destroy the screen, return mouse to original posn., allow the host GUI to take over (optional)
void exit_screen (int16_t mouse_x, int16_t mouse_y);

// this is called when module is unloaded
void module_unload (void);

////////////////////////////////////////////////////////////////////

// extra functions for fixed fps
const char *get_fps_list (int palette);
boolean set_fps (double fps);

///////////////////////////////////////////////////////////////////////////
// mandatory function for display plugins (VPP_LOCAL_DISPLAY)

// This is a host function - a pointer to it is passed in send_keycodes()
typedef boolean (*keyfunc)(boolean down, uint16_t unicode, uint16_t keymod);

#define MOD_CONTROL_MASK 1<<2
#define MOD_ALT_MASK 1<<3
#define MOD_NEEDS_TRANSLATION 1<<15

// host calls this fn to send keycodes, plugin should call key function with a unicode keycode and modifier. If no more keys, return FALSE.
boolean send_keycodes (keyfunc);

//////////////////////////////////////////////////////////////////////////
// optional functions for yuv palettes

// plugin send list of palette sampling types, in order of preference (optional); -1 terminates list
int *get_yuv_palette_sampling (int palette);

// plugin send list of palette clamping types, in order of preference (optional); -1 terminates list
int *get_yuv_palette_clamping (int palette);

// plugin send list of palette subspace types, in order of preference (optional); -1 terminates list
int *get_yuv_palette_subspace (int palette);

// host sets the palette sampling (optional)
boolean set_yuv_palette_sampling (int sampling_type);

// host sets the palette sampling (optional)
boolean set_yuv_palette_clamping (int clamping_type);

// host sets the palette subspace (optional)
boolean set_yuv_palette_subspace (int subspace_type);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __VIDPLUGIN_H__
