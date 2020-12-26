// cliphandler.h
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _CLIPHANDLER_H_
#define _CLIPHANDLER_H_

typedef enum {
  CLIP_DETAILS_BPP,
  CLIP_DETAILS_FPS,
  CLIP_DETAILS_PB_FPS,
  CLIP_DETAILS_WIDTH,
  CLIP_DETAILS_HEIGHT,
  CLIP_DETAILS_UNIQUE_ID,
  CLIP_DETAILS_ARATE,
  CLIP_DETAILS_PB_ARATE,
  CLIP_DETAILS_ACHANS,
  CLIP_DETAILS_ASIGNED,
  CLIP_DETAILS_AENDIAN,
  CLIP_DETAILS_ASAMPS,
  CLIP_DETAILS_FRAMES,
  CLIP_DETAILS_TITLE,
  CLIP_DETAILS_AUTHOR,
  CLIP_DETAILS_COMMENT,
  CLIP_DETAILS_PB_FRAMENO,
  CLIP_DETAILS_FILENAME,
  CLIP_DETAILS_CLIPNAME,
  CLIP_DETAILS_HEADER_VERSION,
  CLIP_DETAILS_KEYWORDS,
  CLIP_DETAILS_INTERLACE,
  CLIP_DETAILS_DECODER_NAME,
  CLIP_DETAILS_GAMMA_TYPE,
  CLIP_DETAILS_MD5SUM, // for future use
  CLIP_DETAILS_CACHE_OBJECTS, // for future use
  CLIP_DETAILS_RESERVED30,
  CLIP_DETAILS_RESERVED29,
  CLIP_DETAILS_RESERVED28,
  CLIP_DETAILS_RESERVED27,
  CLIP_DETAILS_RESERVED26,
  CLIP_DETAILS_RESERVED25,
  CLIP_DETAILS_RESERVED24,
  CLIP_DETAILS_RESERVED23,
  CLIP_DETAILS_RESERVED22,
  CLIP_DETAILS_RESERVED21,
  CLIP_DETAILS_RESERVED20,
  CLIP_DETAILS_RESERVED19,
  CLIP_DETAILS_RESERVED18,
  CLIP_DETAILS_RESERVED17,
  CLIP_DETAILS_RESERVED16,
  CLIP_DETAILS_RESERVED15,
  CLIP_DETAILS_RESERVED14,
  CLIP_DETAILS_RESERVED13,
  CLIP_DETAILS_RESERVED12,
  CLIP_DETAILS_RESERVED11,
  CLIP_DETAILS_RESERVED10,
  CLIP_DETAILS_RESERVED9,
  CLIP_DETAILS_RESERVED8,
  CLIP_DETAILS_RESERVED7,
  CLIP_DETAILS_RESERVED6,
  CLIP_DETAILS_RESERVED5,
  CLIP_DETAILS_RESERVED4,
  CLIP_DETAILS_RESERVED3,
  CLIP_DETAILS_RESERVED2,
  CLIP_DETAILS_RESERVED1,
  CLIP_DETAILS_RESERVED0
} lives_clip_details_t;

char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp);

boolean get_clip_value(int which, lives_clip_details_t, void *retval, size_t maxlen);
boolean save_clip_value(int which, lives_clip_details_t, void *val);
boolean save_clip_values(int which_file);

boolean read_file_details(const char *file_name, boolean only_check_for_audio, boolean open_image);

boolean restore_clip_binfmt(int clipno);
lives_clip_t *clip_forensic(int clipno);

int save_event_frames(void);

void remove_old_headers(int clipno);
boolean write_headers(int clipno);
boolean read_headers(int clipno, const char *dir, const char *file_name);

char *get_clip_dir(int clipno);

void permit_close(int clipno);

#endif
