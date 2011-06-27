// LiVES - decoder plugin header
// (c) G. Finch 2008 <salsaman@xs4all.nl>
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


#ifndef __DECPLUGIN_H__
#define __DECPLUGIN_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <inttypes.h>
#include <sys/types.h>

// palettes, etc.
#ifdef HAVE_SYSTEM_WEED
#include "weed/weed-palettes.h"
#else
#include "../../../libweed/weed-palettes.h"
#endif

#ifdef IS_DARWIN
#ifndef lseek64
#define lseek64 lseek
#endif
#ifndef off64_t
#define off64_t off_t
#endif
#endif


  typedef int boolean;
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0

typedef enum {
  LIVES_INTERLACE_NONE=0,
  LIVES_INTERLACE_BOTTOM_FIRST=1,
  LIVES_INTERLACE_TOP_FIRST=2
} lives_interlace_t;

  /// good
#define LIVES_SEEK_FAST (1<<0)

  /// not so good
#define LIVES_SEEK_NEEDS_CALCULATION (1<<1)
#define LIVES_SEEK_QUALITY_LOSS (1<<2)


typedef struct {
  char *URI; ///< the URI of this cdata

  int nclips; ///< number of clips (titles) in container
  char container_name[512]; ///< name of container, e.g. "ogg" or NULL

  /// plugin should init this to 0 if URI changes
  int current_clip; ///< current clip number in container (starts at 0, MUST be <= nclips) [rw host]

  // video data
  int width;
  int height;
  int64_t nframes;
  lives_interlace_t interlace;

  /// x and y offsets of picture within frame
  /// for primary pixel plane
  int offs_x;
  int offs_y;
  int frame_width;  ///< frame is the surrounding part, including any black border (>=width)
  int frame_height;

  float par; ///< pixel aspect ratio (sample width / sample height)

  float fps;

  int *palettes; ///< list of palettes which the format supports, terminated with WEED_PALETTE_END 

  /// plugin should init this to palettes[0] if URI changes
  int current_palette;  ///< current palette [rw host]; must be contained in palettes
  
  int YUV_sampling;
  int YUV_clamping;
  int YUV_subspace;
  char video_name[512]; ///< name of video codec, e.g. "theora" or NULL

  /* audio data */
  int arate;
  int achans;
  int asamps;
  boolean asigned;
  boolean ainterleaf;
  char audio_name[512]; ///< name of audio codec, e.g. "vorbis" or NULL

  int seek_flag; ///< bitmap of seek properties

  void *priv; ///< private data for demuxer/decoder

} lives_clip_data_t;



// std functions
const char *version(void);


/// pass in NULL clip_data for the first call, subsequent calls (if the URI, current_clip or current_palette changes) 
/// should reuse the previous value. If URI or current_clip are invalid, clip_data will be freed and NULL returned.
///
/// plugin may or may not check current_palette to see if it is valid

// should be threadsafe, and clip_data should be freed with clip_data_free() when no longer required

lives_clip_data_t *get_clip_data(const char *URI, lives_clip_data_t *clip_data);

/// frame starts at 0
boolean get_frame(const lives_clip_data_t *cdata, int64_t frame, void **pixel_data);

/// free clip data - this should be called for each instance before unloading the module
void clip_data_free(lives_clip_data_t *);




// opt fns
const char *module_check_init(void);

int64_t rip_audio (const lives_clip_data_t *, const char *fname, int64_t stframe, int64_t nframes, unsigned char **abuff);
void rip_audio_cleanup(const lives_clip_data_t *);

void module_unload(void);





#define MK_FOURCC(a, b, c, d) ((a<<24)|(b<<16)|(c<<8)|d)






  // bitstream functions from vlc



  typedef struct bs_s
  {
    uint8_t *p_start;
    uint8_t *p;
    uint8_t *p_end;
    
    ssize_t  i_left;    /**< i_count number of available bits */
  } bs_t;
  
  static inline void bs_init( bs_t *s, const void *p_data, size_t i_data )
  {
    s->p_start = (void *)p_data;
    s->p       = s->p_start;
    s->p_end   = s->p_start + i_data;
    s->i_left  = 8;
  }
  
  static inline int bs_pos( const bs_t *s )
  {
    return( 8 * ( s->p - s->p_start ) + 8 - s->i_left );
  }
  
  static inline int bs_eof( const bs_t *s )
  {
    return( s->p >= s->p_end ? 1: 0 );
  }
  
  static inline uint32_t bs_read( bs_t *s, int i_count )
  {
    static const uint32_t i_mask[33] =
      {  0x00,
	 0x01,      0x03,      0x07,      0x0f,
	 0x1f,      0x3f,      0x7f,      0xff,
	 0x1ff,     0x3ff,     0x7ff,     0xfff,
	 0x1fff,    0x3fff,    0x7fff,    0xffff,
	 0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
	 0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
	 0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
	 0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    int      i_shr;
    uint32_t i_result = 0;
    
    while( i_count > 0 )
      {
        if( s->p >= s->p_end )
	  {
            break;
	  }
	
        if( ( i_shr = s->i_left - i_count ) >= 0 )
	  {
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count;
            if( s->i_left == 0 )
	      {
                s->p++;
                s->i_left = 8;
	      }
            return( i_result );
	  }
        else
	  {
            /* less in the buffer than requested */
	    i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
	    i_count  -= s->i_left;
	    s->p++;
	    s->i_left = 8;
	  }
      }
    
    return( i_result );
  }
  
  static inline uint32_t bs_read1( bs_t *s )
  {
    if( s->p < s->p_end )
      {
        unsigned int i_result;
	
        s->i_left--;
        i_result = ( *s->p >> s->i_left )&0x01;
        if( s->i_left == 0 )
	  {
            s->p++;
            s->i_left = 8;
	  }
        return i_result;
      }
    
    return 0;
  }
  
  static inline uint32_t bs_show( bs_t *s, int i_count )
  {
    bs_t     s_tmp = *s;
    return bs_read( &s_tmp, i_count );
  }
  
  static inline void bs_skip( bs_t *s, ssize_t i_count )
  {
    s->i_left -= i_count;
    
    if( s->i_left <= 0 )
      {
        const int i_bytes = ( -s->i_left + 8 ) / 8;
	
        s->p += i_bytes;
        s->i_left += 8 * i_bytes;
      }
  }
  
  static inline void bs_write( bs_t *s, int i_count, uint32_t i_bits )
  {
    while( i_count > 0 )
      {
        if( s->p >= s->p_end )
	  {
            break;
	  }
	
        i_count--;
	
        if( ( i_bits >> i_count )&0x01 )
	  {
            *s->p |= 1 << ( s->i_left - 1 );
	  }
        else
	  {
            *s->p &= ~( 1 << ( s->i_left - 1 ) );
	  }
        s->i_left--;
        if( s->i_left == 0 )
	  {
            s->p++;
            s->i_left = 8;
	  }
      }
  }
  
  static inline void bs_align( bs_t *s )
  {
    if( s->i_left != 8 )
      {
        s->i_left = 8;
        s->p++;
      }
  }
  
  static inline void bs_align_0( bs_t *s )
  {
    if( s->i_left != 8 )
      {
        bs_write( s, s->i_left, 0 );
      }
  }
  
  static inline void bs_align_1( bs_t *s )
  {
    while( s->i_left != 8 )
      {
        bs_write( s, 1, 1 );
      }
  }
  

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __DECPLUGIN_H__
