// LiVES - video playback plugin header
// (c) G. Finch 2003 - 2008 <salsaman@xs4all.nl,salsaman@gmail.com>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifndef __VIDPLUGIN_H__
#define __VIDPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#ifndef NEED_LOCAL_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#else
#include "../../../../libweed/weed.h"
#include "../../../../libweed/weed-palettes.h"
#endif

typedef weed_plant_t weed_layer_t;

#include <inttypes.h>

#ifndef PRId64

#ifndef __WORDSIZE
#if defined __x86_64__
# define __WORDSIZE	64
#ifndef __WORDSIZE_COMPAT32
# define __WORDSIZE_COMPAT32	1
#endif
#else
# define __WORDSIZE	32
#endif
#endif // __WORDSIZE

#ifndef __PRI64_PREFIX
# if __WORDSIZE == 64
#  define __PRI64_PREFIX	"l"
# else
#  define __PRI64_PREFIX	"ll"
# endif
#endif

# define PRId64		__PRI64_PREFIX "d"
# define PRIu64		__PRI64_PREFIX "u"
#endif // ifndef PRI64d

typedef void (*func_ptr)(void *);

#ifndef IS_MINGW
typedef int boolean;
#endif
#undef TRUE
#undef FALSE

#define TRUE 1
#define FALSE 0

#ifndef ABS
#define ABS(a) (a > 0 ? a : -a)
#endif

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

// Warning - CPU_BITS macro evaluates only at runtime (uses sizeof)
#define CPU_BITS ((sizeof(void *)) << 3)

// all playback modules need to implement these functions, unless they are marked (optional)

/// host calls at startup
const char *module_check_init(void);
const char *version(void);
const char *get_description(void);   ///< optional
const char *get_init_rfx(int intention);   ///< optional

#ifdef __WEED_EFFECTS_H__
///< optional (but should return a weed plantptr array of paramtmpl and chantmpl, NULL terminated)
const weed_plant_t **get_play_params(weed_bootstrap_f boot);
#endif

/// plugin send list of palettes, in order of preference
const int *get_palette_list(void);

/// host sets the palette used
boolean set_palette(int palette);

/// host will call this
uint64_t get_capabilities(int palette);

/// for future use
#define LIVES_INTENTION_PLAY               	1
#define LIVES_INTENTION_STREAM          	2
#define LIVES_INTENTION_TRANSCODE    	3

//#define VPP_CAN                         (1<<0)   /// can resize the image to fit the play window
#define VPP_CAN_RESIZE                         (1<<0)   /// can resize the image to fit the play window
#define VPP_CAN_RETURN                       (1<<1)
#define VPP_LOCAL_DISPLAY                    (1<<2)
#define VPP_LINEAR_GAMMA                    (1<<3)
#define VPP_CAN_RESIZE_WINDOW          (1<<4)   /// can resize the image to fit the play window
#define VPP_CAN_LETTERBOX                  (1<<5)

/// ready the screen to play (optional)
boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv);

/// display one frame, adding effects if you like,
/// and resizing it to screen size if possible (VPP_CAN_RESIZE)
///
/// if return_data is non-NULL, you should either fill it with the effected,
/// unresized data (VPP_CAN_RETURN) or set it to NULL if you can't
///
/// hsize and vsize are width and height of the pixel data (in macropixels)
/// no extra padding (rowstrides) is allowed
/// play_params should be cast to weed_plant_t ** (if the plugin exports get_play_paramtmpls() )
/// otherwise it can be ignored
boolean render_frame(int hsize, int vsize, int64_t timecode, void **pixel_data, void **return_data,
                     void **play_params);

/// the same as render frame, but extra padding bytes are allowed, the values in bytes are set in rowstrides
/// return_data will have the same rs values as pixel_data
boolean play_frame(weed_layer_t *frame, int64_t tc, weed_layer_t *ret);

/// destroy the screen, return mouse to original posn., allow the host GUI to take over (optional)
void exit_screen(int16_t mouse_x, int16_t mouse_y);

/// this is called when module is unloaded
void module_unload(void);

////////////////////////////////////////////////////////////////////

// extra functions for fixed fps
const char *get_fps_list(int palette);
boolean set_fps(double fps);

///////////////////////////////////////////////////////////////////////////

// optional functions for yuv palettes

/// plugin send list of palette sampling types, in order of preference (optional); -1 terminates list
const int *get_yuv_palette_sampling(int palette);

/// plugin send list of palette clamping types, in order of preference (optional); -1 terminates list
const int *get_yuv_palette_clamping(int palette);

/// plugin send list of palette subspace types, in order of preference (optional); -1 terminates list
const int *get_yuv_palette_subspace(int palette);

/// host sets the palette sampling (optional)
boolean set_yuv_palette_sampling(int sampling_type);

/// host sets the palette sampling (optional)
boolean set_yuv_palette_clamping(int clamping_type);

/// host sets the palette subspace (optional)
boolean set_yuv_palette_subspace(int subspace_type);

/// newer style
boolean init_audio(int sample_rate, int nchans, int argc, char **argv);

// only float handled for now
boolean render_audio_frame_float(float **audio, int nsamps);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __VIDPLUGIN_H__
