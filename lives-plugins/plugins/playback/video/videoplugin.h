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

#define VIDP_PLUGIN_VERSION_MAJOR 2
#define VIDP_PLUGIN_VERSION_MINOR 0

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

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

// Warning - CPU_BITS macro evaluates only at runtime (uses sizeof)
#define CPU_BITS ((sizeof(void *)) << 3)

#ifndef NEED_LOCAL_WEED
#include <weed/weed-plugin.h>
#include <weed/weed.h>
#include <weed/weed-effects.h>
#include <weed/weed-utils.h>
#else
#include "../../../../libweed/weed-plugin.h"
#include "../../../../libweed/weed.h"
#include "../../../../libweed/weed-effects.h"
#include "../../../../libweed/weed-utils.h"
#endif

#include "../../../weed-plugins/weed-plugin-utils.c"

typedef weed_plant_t weed_layer_t;

// intentcaps
typedef weed_plant_t lives_capacity_t;

typedef int32_t lives_intention;

#define LIVES_CAPACITY_AUDIO_RATE "audio_rate"			// int value
#define LIVES_CAPACITY_AUDIO_CHANS "audio_channels"		// int value

typedef struct {
  lives_intention intent;
  lives_capacity_t *capacities; ///< type specific capabilities
} lives_intentcap_t;

#define lives_capacity_exists(caps, key) (caps ? (weed_plant_has_leaf(caps, key) == WEED_TRUE ? TRUE : FALSE) \
					  : FALSE)

#define lives_capacity_is_readonly(caps, key) (caps ? ((weed_leaf_get_flags(caps, key) & WEED_FLAG_IMMUTABLE) \
						      ? TRUE : FALSE) : FALSE)

#define lives_capacity_get_int(caps, key) (caps ? weed_get_int_value(caps, key, 0) : 0)
#define lives_capacity_get_string(caps, key) (caps ? weed_get_string_value(caps, key, 0) : 0)

/////
#define PLUGIN_TYPE_PLAYER		260 // "player"
#define PLUGIN_PKGTYPE_DYNAMIC     	128 // dynamic library

#define LIVES_INTENTION_PLAY		0x00000200
#define LIVES_INTENTION_STREAM		0x00000201
#define LIVES_INTENTION_TRANSCODE	0x00000202

typedef struct {
  uint64_t uid; // fixed enumeration
  uint64_t type;  ///< e.g. "decoder"
  uint64_t pkgtype;  ///< e.g. dynamic
  char script_lang[32];  ///< for scripted types only, the script interpreter, e.g. "perl", "python3"
  int api_version_major; ///< version of interface API
  int api_version_minor;
  char name[32];  ///< e.g. "mkv_decoder"
  int pl_version_major; ///< version of plugin
  int pl_version_minor;
  lives_intentcap_t *intentcaps;  /// array of intentcaps (NULL terminated)
  void *unused; // padding
} lives_plugin_id_t;

const lives_plugin_id_t *get_plugin_id(void);

static lives_plugin_id_t plugin_id;

static inline lives_plugin_id_t *_make_plugin_id(const char *name, int vmaj, int vmin) {
  static int inited = 0;
  if (!inited) {
    inited = 1;
    plugin_id.uid = PLUGIN_UID;
    plugin_id.type = PLUGIN_TYPE_PLAYER;
    plugin_id.pkgtype = PLUGIN_PKGTYPE_DYNAMIC;
    *plugin_id.script_lang = 0;
    plugin_id.api_version_major = VIDP_PLUGIN_VERSION_MAJOR;
    plugin_id.api_version_major = VIDP_PLUGIN_VERSION_MINOR;
    snprintf(plugin_id.name, 32, "%s", name);
    plugin_id.pl_version_major = vmaj;
    plugin_id.pl_version_minor = vmin;
#ifndef PLUGIN_INTENTCAPS
#define PLUGIN_INTENTCAPS NULL
#endif
    plugin_id.intentcaps = PLUGIN_INTENTCAPS;
  }
  return &plugin_id;
}

// all playback modules need to implement these functions, unless they are marked (optional)

/// host calls at startup
const char *module_check_init(void);
const char *get_description(void);   ///< optional

const char *get_init_rfx(lives_intentcap_t *icaps);   ///< optional

///< optional (but should return a weed plantptr array of paramtmpl and chantmpl, NULL terminated)
const weed_plant_t **get_play_params(weed_bootstrap_f boot);

/// plugin send list of palettes, in order of preference
const int *get_palette_list(void);

/// host sets the palette used
boolean set_palette(int palette);

/// host will call this
uint64_t get_capabilities(int palette);

#define VPP_CAN_RESIZE	       		(1<<0)   ///< can resize the image to fit the play window / letterbox
#define VPP_CAN_RETURN	       		(1<<1)   ///< can return pixel_data after playing
#define VPP_LOCAL_DISPLAY      		(1<<2)   ///< displays to the local monitor
#define VPP_LINEAR_GAMMA       		(1<<3)   ///< input RGB data should be in linear gamma (not v. useful)
#define VPP_CAN_RESIZE_WINDOW          	(1<<4)   ///< can resize the play window on the fly (without init_screen / exit_screen)
#define VPP_CAN_LETTERBOX     		(1<<5)   ///< player can center at xoffset, yoffset (values set in frame in play_frame)
#define VPP_CAN_CHANGE_PALETTE 		(1<<6)   ///< host can switch palette overriding settings
// bit combinations: 0 & 5: can resize and letterbox; 5 without 0: cannot resize image, but it can offset the top left pixel

/// ready the screen to play (optional)
boolean init_screen(int width, int height, boolean fullscreen, uint64_t window_id, int argc, char **argv);

/// display one frame, adding effects if you like,
/// and resizing it to screen size if possible (VPP_CAN_RESIZE) or to letterbox size
///
/// if return_data is non-NULL, you should either fill it with the effected,
/// unresized data (VPP_CAN_RETURN) or set it to NULL if you can't
///
/// hsize and vsize are width and height of the pixel data (in macropixels)
/// no extra padding (rowstrides) is allowed
/// play_params should be cast to weed_plant_t ** (if the plugin exports get_play_paramtmpls() )
/// otherwise it can be ignored **** (deprecated) *****
boolean render_frame(int hsize, int vsize, int64_t timecode, void **pixel_data, void **return_data,
                     void **play_params);

/// updated version of render_frame: input is a weed_layer and timecode, if ret is non NULL, return pixel_data in ret
/// any player params are now in parameters for the layer, which acts like a filter channel
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

#ifdef USE_LIBWEED
weed_plant_t *weed_setup(weed_bootstrap_f);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __VIDPLUGIN_H__
