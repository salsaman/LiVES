// LiVES - mkv decoder plugin
// (c) G. Finch 2011 <salsaman@xs4all.nl,salsaman@gmail.com>

/*
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * LiVES is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LiVES; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

///////////////////////////////////////////////




/* EBML version supported */
#define EBML_VERSION 1

/* top-level master-IDs */
#define EBML_ID_HEADER             0x1A45DFA3

/* IDs in the HEADER master */
#define EBML_ID_EBMLVERSION        0x4286
#define EBML_ID_EBMLREADVERSION    0x42F7
#define EBML_ID_EBMLMAXIDLENGTH    0x42F2
#define EBML_ID_EBMLMAXSIZELENGTH  0x42F3
#define EBML_ID_DOCTYPE            0x4282
#define EBML_ID_DOCTYPEVERSION     0x4287
#define EBML_ID_DOCTYPEREADVERSION 0x4285
\
/* general EBML types */
#define EBML_ID_VOID               0xEC
#define EBML_ID_CRC32              0xBF

/*
 * Matroska element IDs, max. 32 bits
 */

/* toplevel segment */
#define MATROSKA_ID_SEGMENT    0x18538067

/* Matroska top-level master IDs */
#define MATROSKA_ID_INFO       0x1549A966
#define MATROSKA_ID_TRACKS     0x1654AE6B
#define MATROSKA_ID_CUES       0x1C53BB6B
#define MATROSKA_ID_TAGS       0x1254C367
#define MATROSKA_ID_SEEKHEAD   0x114D9B74
#define MATROSKA_ID_ATTACHMENTS 0x1941A469
#define MATROSKA_ID_CLUSTER    0x1F43B675
#define MATROSKA_ID_CHAPTERS   0x1043A770

/* IDs in the info master */
#define MATROSKA_ID_TIMECODESCALE 0x2AD7B1
#define MATROSKA_ID_DURATION   0x4489
#define MATROSKA_ID_TITLE      0x7BA9
#define MATROSKA_ID_WRITINGAPP 0x5741
#define MATROSKA_ID_MUXINGAPP  0x4D80
#define MATROSKA_ID_DATEUTC    0x4461
#define MATROSKA_ID_SEGMENTUID 0x73A4

/* ID in the tracks master */
#define MATROSKA_ID_TRACKENTRY 0xAE

/* IDs in the trackentry master */
#define MATROSKA_ID_TRACKNUMBER 0xD7
#define MATROSKA_ID_TRACKUID   0x73C5
#define MATROSKA_ID_TRACKTYPE  0x83
#define MATROSKA_ID_TRACKVIDEO     0xE0
#define MATROSKA_ID_TRACKAUDIO     0xE1
#define MATROSKA_ID_TRACKOPERATION 0xE2
#define MATROSKA_ID_TRACKCOMBINEPLANES 0xE3
#define MATROSKA_ID_TRACKPLANE         0xE4
#define MATROSKA_ID_TRACKPLANEUID      0xE5
#define MATROSKA_ID_TRACKPLANETYPE     0xE6
#define MATROSKA_ID_CODECID    0x86
#define MATROSKA_ID_CODECPRIVATE 0x63A2
#define MATROSKA_ID_CODECNAME  0x258688
#define MATROSKA_ID_CODECINFOURL 0x3B4040
#define MATROSKA_ID_CODECDOWNLOADURL 0x26B240
#define MATROSKA_ID_CODECDECODEALL 0xAA
#define MATROSKA_ID_TRACKNAME  0x536E
#define MATROSKA_ID_TRACKLANGUAGE 0x22B59C
#define MATROSKA_ID_TRACKFLAGENABLED 0xB9
#define MATROSKA_ID_TRACKFLAGDEFAULT 0x88
#define MATROSKA_ID_TRACKFLAGFORCED 0x55AA
#define MATROSKA_ID_TRACKFLAGLACING 0x9C
#define MATROSKA_ID_TRACKMINCACHE 0x6DE7
#define MATROSKA_ID_TRACKMAXCACHE 0x6DF8
#define MATROSKA_ID_TRACKDEFAULTDURATION 0x23E383
#define MATROSKA_ID_TRACKCONTENTENCODINGS 0x6D80
#define MATROSKA_ID_TRACKCONTENTENCODING 0x6240
#define MATROSKA_ID_TRACKTIMECODESCALE 0x23314F
#define MATROSKA_ID_TRACKMAXBLKADDID 0x55EE

/* IDs in the trackvideo master */
#define MATROSKA_ID_VIDEOFRAMERATE 0x2383E3
#define MATROSKA_ID_VIDEODISPLAYWIDTH 0x54B0
#define MATROSKA_ID_VIDEODISPLAYHEIGHT 0x54BA
#define MATROSKA_ID_VIDEOPIXELWIDTH 0xB0
#define MATROSKA_ID_VIDEOPIXELHEIGHT 0xBA
#define MATROSKA_ID_VIDEOPIXELCROPB 0x54AA
#define MATROSKA_ID_VIDEOPIXELCROPT 0x54BB
#define MATROSKA_ID_VIDEOPIXELCROPL 0x54CC
#define MATROSKA_ID_VIDEOPIXELCROPR 0x54DD
#define MATROSKA_ID_VIDEODISPLAYUNIT 0x54B2
#define MATROSKA_ID_VIDEOFLAGINTERLACED 0x9A
#define MATROSKA_ID_VIDEOSTEREOMODE 0x53B8
#define MATROSKA_ID_VIDEOASPECTRATIO 0x54B3
#define MATROSKA_ID_VIDEOCOLORSPACE 0x2EB524

/* IDs in the trackaudio master */
#define MATROSKA_ID_AUDIOSAMPLINGFREQ 0xB5
#define MATROSKA_ID_AUDIOOUTSAMPLINGFREQ 0x78B5

#define MATROSKA_ID_AUDIOBITDEPTH 0x6264
#define MATROSKA_ID_AUDIOCHANNELS 0x9F

/* IDs in the content encoding master */
#define MATROSKA_ID_ENCODINGORDER 0x5031
#define MATROSKA_ID_ENCODINGSCOPE 0x5032
#define MATROSKA_ID_ENCODINGTYPE 0x5033
#define MATROSKA_ID_ENCODINGCOMPRESSION 0x5034
#define MATROSKA_ID_ENCODINGCOMPALGO 0x4254
#define MATROSKA_ID_ENCODINGCOMPSETTINGS 0x4255

/* ID in the cues master */
#define MATROSKA_ID_POINTENTRY 0xBB

/* IDs in the pointentry master */
#define MATROSKA_ID_CUETIME    0xB3
#define MATROSKA_ID_CUETRACKPOSITION 0xB7

/* IDs in the cuetrackposition master */
#define MATROSKA_ID_CUETRACK   0xF7
#define MATROSKA_ID_CUECLUSTERPOSITION 0xF1
#define MATROSKA_ID_CUEBLOCKNUMBER 0x5378

/* IDs in the tags master */
#define MATROSKA_ID_TAG                 0x7373
#define MATROSKA_ID_SIMPLETAG           0x67C8
#define MATROSKA_ID_TAGNAME             0x45A3
#define MATROSKA_ID_TAGSTRING           0x4487
#define MATROSKA_ID_TAGLANG             0x447A
#define MATROSKA_ID_TAGDEFAULT          0x4484
#define MATROSKA_ID_TAGDEFAULT_BUG      0x44B4
#define MATROSKA_ID_TAGTARGETS          0x63C0
#define MATROSKA_ID_TAGTARGETS_TYPE       0x63CA
#define MATROSKA_ID_TAGTARGETS_TYPEVALUE  0x68CA
#define MATROSKA_ID_TAGTARGETS_TRACKUID   0x63C5
#define MATROSKA_ID_TAGTARGETS_CHAPTERUID 0x63C4
#define MATROSKA_ID_TAGTARGETS_ATTACHUID  0x63C6

/* IDs in the seekhead master */
#define MATROSKA_ID_SEEKENTRY  0x4DBB

/* IDs in the seekpoint master */
#define MATROSKA_ID_SEEKID     0x53AB
#define MATROSKA_ID_SEEKPOSITION 0x53AC

/* IDs in the cluster master */
#define MATROSKA_ID_CLUSTERTIMECODE 0xE7
#define MATROSKA_ID_CLUSTERPOSITION 0xA7
#define MATROSKA_ID_CLUSTERPREVSIZE 0xAB
#define MATROSKA_ID_BLOCKGROUP 0xA0
#define MATROSKA_ID_SIMPLEBLOCK 0xA3

/* IDs in the blockgroup master */
#define MATROSKA_ID_BLOCK      0xA1
#define MATROSKA_ID_BLOCKDURATION 0x9B
#define MATROSKA_ID_BLOCKREFERENCE 0xFB

/* IDs in the attachments master */
#define MATROSKA_ID_ATTACHEDFILE        0x61A7
#define MATROSKA_ID_FILEDESC            0x467E
#define MATROSKA_ID_FILENAME            0x466E
#define MATROSKA_ID_FILEMIMETYPE        0x4660
#define MATROSKA_ID_FILEDATA            0x465C
#define MATROSKA_ID_FILEUID             0x46AE

/* IDs in the chapters master */
#define MATROSKA_ID_EDITIONENTRY        0x45B9
#define MATROSKA_ID_CHAPTERATOM         0xB6
#define MATROSKA_ID_CHAPTERTIMESTART    0x91
#define MATROSKA_ID_CHAPTERTIMEEND      0x92
#define MATROSKA_ID_CHAPTERDISPLAY      0x80
#define MATROSKA_ID_CHAPSTRING          0x85
#define MATROSKA_ID_CHAPLANG            0x437C
#define MATROSKA_ID_EDITIONUID          0x45BC
#define MATROSKA_ID_EDITIONFLAGHIDDEN   0x45BD
#define MATROSKA_ID_EDITIONFLAGDEFAULT  0x45DB
#define MATROSKA_ID_EDITIONFLAGORDERED  0x45DD
#define MATROSKA_ID_CHAPTERUID          0x73C4
#define MATROSKA_ID_CHAPTERFLAGHIDDEN   0x98
#define MATROSKA_ID_CHAPTERFLAGENABLED  0x4598
#define MATROSKA_ID_CHAPTERPHYSEQUIV    0x63C3

typedef enum {
  MATROSKA_TRACK_TYPE_NONE     = 0x0,
  MATROSKA_TRACK_TYPE_VIDEO    = 0x1,
  MATROSKA_TRACK_TYPE_AUDIO    = 0x2,
  MATROSKA_TRACK_TYPE_COMPLEX  = 0x3,
  MATROSKA_TRACK_TYPE_LOGO     = 0x10,
  MATROSKA_TRACK_TYPE_SUBTITLE = 0x11,
  MATROSKA_TRACK_TYPE_CONTROL  = 0x20,
} MatroskaTrackType;

typedef enum {
  MATROSKA_TRACK_ENCODING_COMP_ZLIB        = 0,
  MATROSKA_TRACK_ENCODING_COMP_BZLIB       = 1,
  MATROSKA_TRACK_ENCODING_COMP_LZO         = 2,
  MATROSKA_TRACK_ENCODING_COMP_HEADERSTRIP = 3,
} MatroskaTrackEncodingCompAlgo;

typedef enum {
  MATROSKA_VIDEO_STEREOMODE_TYPE_MONO               = 0,
  MATROSKA_VIDEO_STEREOMODE_TYPE_LEFT_RIGHT         = 1,
  MATROSKA_VIDEO_STEREOMODE_TYPE_BOTTOM_TOP         = 2,
  MATROSKA_VIDEO_STEREOMODE_TYPE_TOP_BOTTOM         = 3,
  MATROSKA_VIDEO_STEREOMODE_TYPE_CHECKERBOARD_RL    = 4,
  MATROSKA_VIDEO_STEREOMODE_TYPE_CHECKERBOARD_LR    = 5,
  MATROSKA_VIDEO_STEREOMODE_TYPE_ROW_INTERLEAVED_RL = 6,
  MATROSKA_VIDEO_STEREOMODE_TYPE_ROW_INTERLEAVED_LR = 7,
  MATROSKA_VIDEO_STEREOMODE_TYPE_COL_INTERLEAVED_RL = 8,
  MATROSKA_VIDEO_STEREOMODE_TYPE_COL_INTERLEAVED_LR = 9,
  MATROSKA_VIDEO_STEREOMODE_TYPE_ANAGLYPH_CYAN_RED  = 10,
  MATROSKA_VIDEO_STEREOMODE_TYPE_RIGHT_LEFT         = 11,
  MATROSKA_VIDEO_STEREOMODE_TYPE_ANAGLYPH_GREEN_MAG = 12,
  MATROSKA_VIDEO_STEREOMODE_TYPE_BOTH_EYES_BLOCK_LR = 13,
  MATROSKA_VIDEO_STEREOMODE_TYPE_BOTH_EYES_BLOCK_RL = 14,
} MatroskaVideoStereoModeType;

/*
 * Matroska Codec IDs, strings
 */

typedef struct CodecTags {
  char str[20];
  enum CodecID id;
} CodecTags;

typedef struct CodecMime {
  char str[32];
  enum CodecID id;
} CodecMime;

/* max. depth in the EBML tree structure */
#define EBML_MAX_DEPTH 16

#define MATROSKA_VIDEO_STEREO_MODE_COUNT  15
#define MATROSKA_VIDEO_STEREO_PLANE_COUNT  3

struct AVMetadataConv {
  const char *native;
  const char *generic;
};

#if FF_API_OLD_METADATA2
#else
typedef struct AVMetadataConv AVMetadataConv;
#endif

const AVMetadataConv ff_mkv_metadata_conv[] = {
  { "LEAD_PERFORMER", "performer" },
  { "PART_NUMBER"   , "track"  },
  { 0 }
};


/*extern const CodecTags ff_mkv_codec_tags[];
extern const CodecMime ff_mkv_mime_tags[];
extern const
extern const char * const matroska_video_stereo_mode[MATROSKA_VIDEO_STEREO_MODE_COUNT];
extern const char * const matroska_video_stereo_plane[MATROSKA_VIDEO_STEREO_PLANE_COUNT];*/

const CodecTags ff_mkv_codec_tags[]= {
  {"A_AAC"            , CODEC_ID_AAC},
  {"A_AC3"            , CODEC_ID_AC3},
  {"A_DTS"            , CODEC_ID_DTS},
  {"A_EAC3"           , CODEC_ID_EAC3},
  {"A_FLAC"           , CODEC_ID_FLAC},
  {"A_MLP"            , CODEC_ID_MLP},
  {"A_MPEG/L2"        , CODEC_ID_MP2},
  {"A_MPEG/L1"        , CODEC_ID_MP2},
  {"A_MPEG/L3"        , CODEC_ID_MP3},
  {"A_PCM/FLOAT/IEEE" , CODEC_ID_PCM_F32LE},
  {"A_PCM/FLOAT/IEEE" , CODEC_ID_PCM_F64LE},
  {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S16BE},
  {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S24BE},
  {"A_PCM/INT/BIG"    , CODEC_ID_PCM_S32BE},
  {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S16LE},
  {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S24LE},
  {"A_PCM/INT/LIT"    , CODEC_ID_PCM_S32LE},
  {"A_PCM/INT/LIT"    , CODEC_ID_PCM_U8},
  {"A_QUICKTIME/QDM2" , CODEC_ID_QDM2},
  {"A_REAL/14_4"      , CODEC_ID_RA_144},
  {"A_REAL/28_8"      , CODEC_ID_RA_288},
  {"A_REAL/ATRC"      , CODEC_ID_ATRAC3},
  {"A_REAL/COOK"      , CODEC_ID_COOK},
  {"A_REAL/SIPR"      , CODEC_ID_SIPR},
  {"A_TRUEHD"         , CODEC_ID_TRUEHD},
  {"A_TTA1"           , CODEC_ID_TTA},
  {"A_VORBIS"         , CODEC_ID_VORBIS},
  {"A_WAVPACK4"       , CODEC_ID_WAVPACK},

  {"S_TEXT/UTF8"      , CODEC_ID_TEXT},
  {"S_TEXT/UTF8"      , CODEC_ID_SRT},
  {"S_TEXT/ASCII"     , CODEC_ID_TEXT},
  {"S_TEXT/ASS"       , CODEC_ID_SSA},
  {"S_TEXT/SSA"       , CODEC_ID_SSA},
  {"S_ASS"            , CODEC_ID_SSA},
  {"S_SSA"            , CODEC_ID_SSA},
  {"S_VOBSUB"         , CODEC_ID_DVD_SUBTITLE},
  {"S_HDMV/PGS"       , CODEC_ID_HDMV_PGS_SUBTITLE},

  {"V_DIRAC"          , CODEC_ID_DIRAC},
  {"V_MJPEG"          , CODEC_ID_MJPEG},
  {"V_MPEG1"          , CODEC_ID_MPEG1VIDEO},
  {"V_MPEG2"          , CODEC_ID_MPEG2VIDEO},
  {"V_MPEG4/ISO/ASP"  , CODEC_ID_MPEG4},
  {"V_MPEG4/ISO/AP"   , CODEC_ID_MPEG4},
  {"V_MPEG4/ISO/SP"   , CODEC_ID_MPEG4},
  {"V_MPEG4/ISO/AVC"  , CODEC_ID_H264},
  {"V_MPEG4/MS/V3"    , CODEC_ID_MSMPEG4V3},
  {"V_REAL/RV10"      , CODEC_ID_RV10},
  {"V_REAL/RV20"      , CODEC_ID_RV20},
  {"V_REAL/RV30"      , CODEC_ID_RV30},
  {"V_REAL/RV40"      , CODEC_ID_RV40},
  {"V_SNOW"           , CODEC_ID_SNOW},
  {"V_THEORA"         , CODEC_ID_THEORA},
  {"V_UNCOMPRESSED"   , CODEC_ID_RAWVIDEO},
  {"V_VP8"            , CODEC_ID_VP8},

  {""                 , CODEC_ID_NONE}
};

const CodecMime ff_mkv_mime_tags[] = {
  {"text/plain"                 , CODEC_ID_TEXT},
  {"image/gif"                  , CODEC_ID_GIF},
  {"image/jpeg"                 , CODEC_ID_MJPEG},
  {"image/png"                  , CODEC_ID_PNG},
  {"image/tiff"                 , CODEC_ID_TIFF},
  {"application/x-truetype-font", CODEC_ID_TTF},
  {"application/x-font"         , CODEC_ID_TTF},

  {""                           , CODEC_ID_NONE}
};


const char *const matroska_video_stereo_mode[MATROSKA_VIDEO_STEREO_MODE_COUNT] = {
  "mono",
  "left_right",
  "bottom_top",
  "top_bottom",
  "checkerboard_rl",
  "checkerboard_lr"
  "row_interleaved_rl",
  "row_interleaved_lr",
  "col_interleaved_rl",
  "col_interleaved_lr",
  "anaglyph_cyan_red",
  "right_left",
  "anaglyph_green_magenta",
  "block_lr",
  "block_rl",
};

const char *const matroska_video_stereo_plane[MATROSKA_VIDEO_STEREO_PLANE_COUNT] = {
  "left",
  "right",
  "background",
};


typedef enum {
  EBML_NONE,
  EBML_UINT,
  EBML_FLOAT,
  EBML_STR,
  EBML_UTF8,
  EBML_BIN,
  EBML_NEST,
  EBML_PASS,
  EBML_STOP,
  EBML_TYPE_COUNT
} EbmlType;

typedef const struct EbmlSyntax {
  uint32_t id;
  EbmlType type;
  int list_elem_size;
  int data_offset;
  union {
    uint64_t    u;
    double      f;
    const char *s;
    const struct EbmlSyntax *n;
  } def;
} EbmlSyntax;

typedef struct {
  int nb_elem;
  void *elem;
} EbmlList;

typedef struct {
  int      size;
  uint8_t *data;
  int64_t  pos;
} EbmlBin;

typedef struct {
  uint64_t version;
  uint64_t max_size;
  uint64_t id_length;
  char    *doctype;
  uint64_t doctype_version;
} Ebml;

typedef struct {
  uint64_t algo;
  EbmlBin  settings;
} MatroskaTrackCompression;

typedef struct {
  uint64_t scope;
  uint64_t type;
  MatroskaTrackCompression compression;
} MatroskaTrackEncoding;

typedef struct {
  double   frame_rate;
  uint64_t display_width;
  uint64_t display_height;
  uint64_t pixel_width;
  uint64_t pixel_height;
  EbmlBin color_space;
  uint64_t stereo_mode;
  uint64_t flag_interlaced;
} MatroskaTrackVideo;

typedef struct {
  double   samplerate;
  double   out_samplerate;
  uint64_t bitdepth;
  uint64_t channels;

  /* real audio header (extracted from extradata) */
  int      coded_framesize;
  int      sub_packet_h;
  int      frame_size;
  int      sub_packet_size;
  int      sub_packet_cnt;
  int      pkt_cnt;
  uint64_t buf_timecode;
  uint8_t *buf;
} MatroskaTrackAudio;

typedef struct {
  uint64_t uid;
  uint64_t type;
} MatroskaTrackPlane;

typedef struct {
  EbmlList combine_planes;
} MatroskaTrackOperation;

typedef struct {
  uint64_t num;
  uint64_t uid;
  uint64_t type;
  char    *name;
  char    *codec_id;
  EbmlBin  codec_priv;
  char    *language;
  double time_scale;
  uint64_t default_duration;
  uint64_t flag_default;
  uint64_t flag_forced;
  MatroskaTrackVideo video;
  MatroskaTrackAudio audio;
  MatroskaTrackOperation operation;
  EbmlList encodings;

  AVStream *stream;
  int64_t end_timecode;
  int ms_compat;
} MatroskaTrack;

typedef struct {
  uint64_t uid;
  char *filename;
  char *mime;
  EbmlBin bin;

  AVStream *stream;
} MatroskaAttachement;

typedef struct {
  uint64_t start;
  uint64_t end;
  uint64_t uid;
  char    *title;

  AVChapter *chapter;
} MatroskaChapter;

typedef struct {
  uint64_t track;
  uint64_t pos;
} MatroskaIndexPos;

typedef struct {
  uint64_t time;
  EbmlList pos;
} MatroskaIndex;

typedef struct {
  char *name;
  char *string;
  char *lang;
  uint64_t def;
  EbmlList sub;
} MatroskaTag;

typedef struct {
  char    *type;
  uint64_t typevalue;
  uint64_t trackuid;
  uint64_t chapteruid;
  uint64_t attachuid;
} MatroskaTagTarget;

typedef struct {
  MatroskaTagTarget target;
  EbmlList tag;
} MatroskaTags;

typedef struct {
  uint64_t id;
  uint64_t pos;
} MatroskaSeekhead;

typedef struct {
  uint64_t start;
  uint64_t length;
} MatroskaLevel;

typedef struct {
  AVFormatContext *ctx;

  /* EBML stuff */
  int num_levels;
  MatroskaLevel levels[EBML_MAX_DEPTH];
  int level_up;
  uint32_t current_id;

  uint64_t time_scale;
  double   duration;
  char    *title;
  EbmlList tracks;
  EbmlList attachments;
  EbmlList chapters;
  EbmlList index;
  EbmlList tags;
  EbmlList seekhead;

  /* byte position of the segment inside the stream */
  int64_t segment_start;

  /* the packet queue */
  AVPacket **packets;
  int num_packets;
  AVPacket *prev_pkt;

  int done;

  /* What to skip before effectively reading a packet. */
  int skip_to_keyframe;
  uint64_t skip_to_timecode;
  int cues_parsing_deferred;
} MatroskaDemuxContext;

typedef struct {
  uint64_t duration;
  int64_t  reference;
  uint64_t non_simple;
  EbmlBin  bin;
} MatroskaBlock;

typedef struct {
  uint64_t timecode;
  EbmlList blocks;
} MatroskaCluster;

static EbmlSyntax ebml_header[] = {
  { EBML_ID_EBMLREADVERSION,        EBML_UINT, 0, offsetof(Ebml,version), {.u=EBML_VERSION} },
  { EBML_ID_EBMLMAXSIZELENGTH,      EBML_UINT, 0, offsetof(Ebml,max_size), {.u=8} },
  { EBML_ID_EBMLMAXIDLENGTH,        EBML_UINT, 0, offsetof(Ebml,id_length), {.u=4} },
  { EBML_ID_DOCTYPE,                EBML_STR,  0, offsetof(Ebml,doctype), {.s="(none)"} },
  { EBML_ID_DOCTYPEREADVERSION,     EBML_UINT, 0, offsetof(Ebml,doctype_version), {.u=1} },
  { EBML_ID_EBMLVERSION,            EBML_NONE },
  { EBML_ID_DOCTYPEVERSION,         EBML_NONE },
  { 0 }
};

static EbmlSyntax ebml_syntax[] = {
  { EBML_ID_HEADER,                 EBML_NEST, 0, 0, {.n=ebml_header} },
  { 0 }
};

static EbmlSyntax matroska_info[] = {
  { MATROSKA_ID_TIMECODESCALE,      EBML_UINT,  0, offsetof(MatroskaDemuxContext,time_scale), {.u=1000000} },
  { MATROSKA_ID_DURATION,           EBML_FLOAT, 0, offsetof(MatroskaDemuxContext,duration) },
  { MATROSKA_ID_TITLE,              EBML_UTF8,  0, offsetof(MatroskaDemuxContext,title) },
  { MATROSKA_ID_WRITINGAPP,         EBML_NONE },
  { MATROSKA_ID_MUXINGAPP,          EBML_NONE },
  { MATROSKA_ID_DATEUTC,            EBML_NONE },
  { MATROSKA_ID_SEGMENTUID,         EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_track_video[] = {
  { MATROSKA_ID_VIDEOFRAMERATE,     EBML_FLOAT,0, offsetof(MatroskaTrackVideo,frame_rate) },
  { MATROSKA_ID_VIDEODISPLAYWIDTH,  EBML_UINT, 0, offsetof(MatroskaTrackVideo,display_width) },
  { MATROSKA_ID_VIDEODISPLAYHEIGHT, EBML_UINT, 0, offsetof(MatroskaTrackVideo,display_height) },
  { MATROSKA_ID_VIDEOPIXELWIDTH,    EBML_UINT, 0, offsetof(MatroskaTrackVideo,pixel_width) },
  { MATROSKA_ID_VIDEOPIXELHEIGHT,   EBML_UINT, 0, offsetof(MatroskaTrackVideo,pixel_height) },
  { MATROSKA_ID_VIDEOCOLORSPACE,    EBML_BIN,  0, offsetof(MatroskaTrackVideo,color_space) },
  { MATROSKA_ID_VIDEOSTEREOMODE,    EBML_UINT, 0, offsetof(MatroskaTrackVideo,stereo_mode) },
  { MATROSKA_ID_VIDEOFLAGINTERLACED,EBML_UINT, 0, offsetof(MatroskaTrackVideo,flag_interlaced), {.u=0}  },
  { MATROSKA_ID_VIDEOPIXELCROPB,    EBML_NONE },
  { MATROSKA_ID_VIDEOPIXELCROPT,    EBML_NONE },
  { MATROSKA_ID_VIDEOPIXELCROPL,    EBML_NONE },
  { MATROSKA_ID_VIDEOPIXELCROPR,    EBML_NONE },
  { MATROSKA_ID_VIDEODISPLAYUNIT,   EBML_NONE },
  { MATROSKA_ID_VIDEOASPECTRATIO,   EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_track_audio[] = {
  { MATROSKA_ID_AUDIOSAMPLINGFREQ,  EBML_FLOAT,0, offsetof(MatroskaTrackAudio,samplerate), {.f=8000.0} },
  { MATROSKA_ID_AUDIOOUTSAMPLINGFREQ,EBML_FLOAT,0,offsetof(MatroskaTrackAudio,out_samplerate) },
  { MATROSKA_ID_AUDIOBITDEPTH,      EBML_UINT, 0, offsetof(MatroskaTrackAudio,bitdepth) },
  { MATROSKA_ID_AUDIOCHANNELS,      EBML_UINT, 0, offsetof(MatroskaTrackAudio,channels), {.u=1} },
  { 0 }
};

static EbmlSyntax matroska_track_encoding_compression[] = {
  { MATROSKA_ID_ENCODINGCOMPALGO,   EBML_UINT, 0, offsetof(MatroskaTrackCompression,algo), {.u=0} },
  { MATROSKA_ID_ENCODINGCOMPSETTINGS,EBML_BIN, 0, offsetof(MatroskaTrackCompression,settings) },
  { 0 }
};

static EbmlSyntax matroska_track_encoding[] = {
  { MATROSKA_ID_ENCODINGSCOPE,      EBML_UINT, 0, offsetof(MatroskaTrackEncoding,scope), {.u=1} },
  { MATROSKA_ID_ENCODINGTYPE,       EBML_UINT, 0, offsetof(MatroskaTrackEncoding,type), {.u=0} },
  { MATROSKA_ID_ENCODINGCOMPRESSION,EBML_NEST, 0, offsetof(MatroskaTrackEncoding,compression), {.n=matroska_track_encoding_compression} },
  { MATROSKA_ID_ENCODINGORDER,      EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_track_encodings[] = {
  { MATROSKA_ID_TRACKCONTENTENCODING, EBML_NEST, sizeof(MatroskaTrackEncoding), offsetof(MatroskaTrack,encodings), {.n=matroska_track_encoding} },
  { 0 }
};

static EbmlSyntax matroska_track_plane[] = {
  { MATROSKA_ID_TRACKPLANEUID,  EBML_UINT, 0, offsetof(MatroskaTrackPlane,uid) },
  { MATROSKA_ID_TRACKPLANETYPE, EBML_UINT, 0, offsetof(MatroskaTrackPlane,type) },
  { 0 }
};

static EbmlSyntax matroska_track_combine_planes[] = {
  { MATROSKA_ID_TRACKPLANE, EBML_NEST, sizeof(MatroskaTrackPlane), offsetof(MatroskaTrackOperation,combine_planes), {.n=matroska_track_plane} },
  { 0 }
};

static EbmlSyntax matroska_track_operation[] = {
  { MATROSKA_ID_TRACKCOMBINEPLANES, EBML_NEST, 0, 0, {.n=matroska_track_combine_planes} },
  { 0 }
};

static EbmlSyntax matroska_track[] = {
  { MATROSKA_ID_TRACKNUMBER,          EBML_UINT, 0, offsetof(MatroskaTrack,num) },
  { MATROSKA_ID_TRACKNAME,            EBML_UTF8, 0, offsetof(MatroskaTrack,name) },
  { MATROSKA_ID_TRACKUID,             EBML_UINT, 0, offsetof(MatroskaTrack,uid) },
  { MATROSKA_ID_TRACKTYPE,            EBML_UINT, 0, offsetof(MatroskaTrack,type) },
  { MATROSKA_ID_CODECID,              EBML_STR,  0, offsetof(MatroskaTrack,codec_id) },
  { MATROSKA_ID_CODECPRIVATE,         EBML_BIN,  0, offsetof(MatroskaTrack,codec_priv) },
  { MATROSKA_ID_TRACKLANGUAGE,        EBML_UTF8, 0, offsetof(MatroskaTrack,language), {.s="eng"} },
  { MATROSKA_ID_TRACKDEFAULTDURATION, EBML_UINT, 0, offsetof(MatroskaTrack,default_duration) },
  { MATROSKA_ID_TRACKTIMECODESCALE,   EBML_FLOAT,0, offsetof(MatroskaTrack,time_scale), {.f=1.0} },
  { MATROSKA_ID_TRACKFLAGDEFAULT,     EBML_UINT, 0, offsetof(MatroskaTrack,flag_default), {.u=1} },
  { MATROSKA_ID_TRACKFLAGFORCED,      EBML_UINT, 0, offsetof(MatroskaTrack,flag_forced), {.u=0} },
  { MATROSKA_ID_TRACKVIDEO,           EBML_NEST, 0, offsetof(MatroskaTrack,video), {.n=matroska_track_video} },
  { MATROSKA_ID_TRACKAUDIO,           EBML_NEST, 0, offsetof(MatroskaTrack,audio), {.n=matroska_track_audio} },
  { MATROSKA_ID_TRACKOPERATION,       EBML_NEST, 0, offsetof(MatroskaTrack,operation), {.n=matroska_track_operation} },
  { MATROSKA_ID_TRACKCONTENTENCODINGS,EBML_NEST, 0, 0, {.n=matroska_track_encodings} },
  { MATROSKA_ID_TRACKFLAGENABLED,     EBML_NONE },
  { MATROSKA_ID_TRACKFLAGLACING,      EBML_NONE },
  { MATROSKA_ID_CODECNAME,            EBML_NONE },
  { MATROSKA_ID_CODECDECODEALL,       EBML_NONE },
  { MATROSKA_ID_CODECINFOURL,         EBML_NONE },
  { MATROSKA_ID_CODECDOWNLOADURL,     EBML_NONE },
  { MATROSKA_ID_TRACKMINCACHE,        EBML_NONE },
  { MATROSKA_ID_TRACKMAXCACHE,        EBML_NONE },
  { MATROSKA_ID_TRACKMAXBLKADDID,     EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_tracks[] = {
  { MATROSKA_ID_TRACKENTRY,         EBML_NEST, sizeof(MatroskaTrack), offsetof(MatroskaDemuxContext,tracks), {.n=matroska_track} },
  { 0 }
};

static EbmlSyntax matroska_attachment[] = {
  { MATROSKA_ID_FILEUID,            EBML_UINT, 0, offsetof(MatroskaAttachement,uid) },
  { MATROSKA_ID_FILENAME,           EBML_UTF8, 0, offsetof(MatroskaAttachement,filename) },
  { MATROSKA_ID_FILEMIMETYPE,       EBML_STR,  0, offsetof(MatroskaAttachement,mime) },
  { MATROSKA_ID_FILEDATA,           EBML_BIN,  0, offsetof(MatroskaAttachement,bin) },
  { MATROSKA_ID_FILEDESC,           EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_attachments[] = {
  { MATROSKA_ID_ATTACHEDFILE,       EBML_NEST, sizeof(MatroskaAttachement), offsetof(MatroskaDemuxContext,attachments), {.n=matroska_attachment} },
  { 0 }
};

static EbmlSyntax matroska_chapter_display[] = {
  { MATROSKA_ID_CHAPSTRING,         EBML_UTF8, 0, offsetof(MatroskaChapter,title) },
  { MATROSKA_ID_CHAPLANG,           EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_chapter_entry[] = {
  { MATROSKA_ID_CHAPTERTIMESTART,   EBML_UINT, 0, offsetof(MatroskaChapter,start), {.u=AV_NOPTS_VALUE} },
  { MATROSKA_ID_CHAPTERTIMEEND,     EBML_UINT, 0, offsetof(MatroskaChapter,end), {.u=AV_NOPTS_VALUE} },
  { MATROSKA_ID_CHAPTERUID,         EBML_UINT, 0, offsetof(MatroskaChapter,uid) },
  { MATROSKA_ID_CHAPTERDISPLAY,     EBML_NEST, 0, 0, {.n=matroska_chapter_display} },
  { MATROSKA_ID_CHAPTERFLAGHIDDEN,  EBML_NONE },
  { MATROSKA_ID_CHAPTERFLAGENABLED, EBML_NONE },
  { MATROSKA_ID_CHAPTERPHYSEQUIV,   EBML_NONE },
  { MATROSKA_ID_CHAPTERATOM,        EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_chapter[] = {
  { MATROSKA_ID_CHAPTERATOM,        EBML_NEST, sizeof(MatroskaChapter), offsetof(MatroskaDemuxContext,chapters), {.n=matroska_chapter_entry} },
  { MATROSKA_ID_EDITIONUID,         EBML_NONE },
  { MATROSKA_ID_EDITIONFLAGHIDDEN,  EBML_NONE },
  { MATROSKA_ID_EDITIONFLAGDEFAULT, EBML_NONE },
  { MATROSKA_ID_EDITIONFLAGORDERED, EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_chapters[] = {
  { MATROSKA_ID_EDITIONENTRY,       EBML_NEST, 0, 0, {.n=matroska_chapter} },
  { 0 }
};

static EbmlSyntax matroska_index_pos[] = {
  { MATROSKA_ID_CUETRACK,           EBML_UINT, 0, offsetof(MatroskaIndexPos,track) },
  { MATROSKA_ID_CUECLUSTERPOSITION, EBML_UINT, 0, offsetof(MatroskaIndexPos,pos)   },
  { MATROSKA_ID_CUEBLOCKNUMBER,     EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_index_entry[] = {
  { MATROSKA_ID_CUETIME,            EBML_UINT, 0, offsetof(MatroskaIndex,time) },
  { MATROSKA_ID_CUETRACKPOSITION,   EBML_NEST, sizeof(MatroskaIndexPos), offsetof(MatroskaIndex,pos), {.n=matroska_index_pos} },
  { 0 }
};

static EbmlSyntax matroska_index[] = {
  { MATROSKA_ID_POINTENTRY,         EBML_NEST, sizeof(MatroskaIndex), offsetof(MatroskaDemuxContext,index), {.n=matroska_index_entry} },
  { 0 }
};

static EbmlSyntax matroska_simpletag[] = {
  { MATROSKA_ID_TAGNAME,            EBML_UTF8, 0, offsetof(MatroskaTag,name) },
  { MATROSKA_ID_TAGSTRING,          EBML_UTF8, 0, offsetof(MatroskaTag,string) },
  { MATROSKA_ID_TAGLANG,            EBML_STR,  0, offsetof(MatroskaTag,lang), {.s="und"} },
  { MATROSKA_ID_TAGDEFAULT,         EBML_UINT, 0, offsetof(MatroskaTag,def) },
  { MATROSKA_ID_TAGDEFAULT_BUG,     EBML_UINT, 0, offsetof(MatroskaTag,def) },
  { MATROSKA_ID_SIMPLETAG,          EBML_NEST, sizeof(MatroskaTag), offsetof(MatroskaTag,sub), {.n=matroska_simpletag} },
  { 0 }
};

static EbmlSyntax matroska_tagtargets[] = {
  { MATROSKA_ID_TAGTARGETS_TYPE,      EBML_STR,  0, offsetof(MatroskaTagTarget,type) },
  { MATROSKA_ID_TAGTARGETS_TYPEVALUE, EBML_UINT, 0, offsetof(MatroskaTagTarget,typevalue), {.u=50} },
  { MATROSKA_ID_TAGTARGETS_TRACKUID,  EBML_UINT, 0, offsetof(MatroskaTagTarget,trackuid) },
  { MATROSKA_ID_TAGTARGETS_CHAPTERUID,EBML_UINT, 0, offsetof(MatroskaTagTarget,chapteruid) },
  { MATROSKA_ID_TAGTARGETS_ATTACHUID, EBML_UINT, 0, offsetof(MatroskaTagTarget,attachuid) },
  { 0 }
};

static EbmlSyntax matroska_tag[] = {
  { MATROSKA_ID_SIMPLETAG,          EBML_NEST, sizeof(MatroskaTag), offsetof(MatroskaTags,tag), {.n=matroska_simpletag} },
  { MATROSKA_ID_TAGTARGETS,         EBML_NEST, 0, offsetof(MatroskaTags,target), {.n=matroska_tagtargets} },
  { 0 }
};

static EbmlSyntax matroska_tags[] = {
  { MATROSKA_ID_TAG,                EBML_NEST, sizeof(MatroskaTags), offsetof(MatroskaDemuxContext,tags), {.n=matroska_tag} },
  { 0 }
};

static EbmlSyntax matroska_seekhead_entry[] = {
  { MATROSKA_ID_SEEKID,             EBML_UINT, 0, offsetof(MatroskaSeekhead,id) },
  { MATROSKA_ID_SEEKPOSITION,       EBML_UINT, 0, offsetof(MatroskaSeekhead,pos), {.u=-1} },
  { 0 }
};

static EbmlSyntax matroska_seekhead[] = {
  { MATROSKA_ID_SEEKENTRY,          EBML_NEST, sizeof(MatroskaSeekhead), offsetof(MatroskaDemuxContext,seekhead), {.n=matroska_seekhead_entry} },
  { 0 }
};

static EbmlSyntax matroska_segment[] = {
  { MATROSKA_ID_INFO,           EBML_NEST, 0, 0, {.n=matroska_info       } },
  { MATROSKA_ID_TRACKS,         EBML_NEST, 0, 0, {.n=matroska_tracks     } },
  { MATROSKA_ID_ATTACHMENTS,    EBML_NEST, 0, 0, {.n=matroska_attachments} },
  { MATROSKA_ID_CHAPTERS,       EBML_NEST, 0, 0, {.n=matroska_chapters   } },
  { MATROSKA_ID_CUES,           EBML_NEST, 0, 0, {.n=matroska_index      } },
  { MATROSKA_ID_TAGS,           EBML_NEST, 0, 0, {.n=matroska_tags       } },
  { MATROSKA_ID_SEEKHEAD,       EBML_NEST, 0, 0, {.n=matroska_seekhead   } },
  { MATROSKA_ID_CLUSTER,        EBML_STOP },
  { 0 }
};

static EbmlSyntax matroska_segments[] = {
  { MATROSKA_ID_SEGMENT,        EBML_NEST, 0, 0, {.n=matroska_segment    } },
  { 0 }
};

static EbmlSyntax matroska_blockgroup[] = {
  { MATROSKA_ID_BLOCK,          EBML_BIN,  0, offsetof(MatroskaBlock,bin) },
  { MATROSKA_ID_SIMPLEBLOCK,    EBML_BIN,  0, offsetof(MatroskaBlock,bin) },
  { MATROSKA_ID_BLOCKDURATION,  EBML_UINT, 0, offsetof(MatroskaBlock,duration) },
  { MATROSKA_ID_BLOCKREFERENCE, EBML_UINT, 0, offsetof(MatroskaBlock,reference) },
  { 1,                          EBML_UINT, 0, offsetof(MatroskaBlock,non_simple), {.u=1} },
  { 0 }
};

static EbmlSyntax matroska_cluster[] = {
  { MATROSKA_ID_CLUSTERTIMECODE,EBML_UINT,0, offsetof(MatroskaCluster,timecode) },
  { MATROSKA_ID_BLOCKGROUP,     EBML_NEST, sizeof(MatroskaBlock), offsetof(MatroskaCluster,blocks), {.n=matroska_blockgroup} },
  { MATROSKA_ID_SIMPLEBLOCK,    EBML_PASS, sizeof(MatroskaBlock), offsetof(MatroskaCluster,blocks), {.n=matroska_blockgroup} },
  { MATROSKA_ID_CLUSTERPOSITION,EBML_NONE },
  { MATROSKA_ID_CLUSTERPREVSIZE,EBML_NONE },
  { 0 }
};

static EbmlSyntax matroska_clusters[] = {
  { MATROSKA_ID_CLUSTER,        EBML_NEST, 0, 0, {.n=matroska_cluster} },
  { MATROSKA_ID_INFO,           EBML_NONE },
  { MATROSKA_ID_CUES,           EBML_NONE },
  { MATROSKA_ID_TAGS,           EBML_NONE },
  { MATROSKA_ID_SEEKHEAD,       EBML_NONE },
  { 0 }
};



const AVCodecTag codec_movvideo_tags[] = {
  /*  { CODEC_ID_, MKTAG('I', 'V', '5', '0') }, *//* Indeo 5.0 */

  { CODEC_ID_RAWVIDEO, MKTAG('r', 'a', 'w', ' ') }, /* Uncompressed RGB */
  { CODEC_ID_RAWVIDEO, MKTAG('y', 'u', 'v', '2') }, /* Uncompressed YUV422 */
  { CODEC_ID_RAWVIDEO, MKTAG('A', 'V', 'U', 'I') }, /* YUV with alpha-channel (AVID Uncompressed) */
  { CODEC_ID_RAWVIDEO, MKTAG('2', 'v', 'u', 'y') }, /* UNCOMPRESSED 8BIT 4:2:2 */
  { CODEC_ID_RAWVIDEO, MKTAG('y', 'u', 'v', 's') }, /* same as 2vuy but byte swapped */

  { CODEC_ID_RAWVIDEO, MKTAG('L', '5', '5', '5') },
  { CODEC_ID_RAWVIDEO, MKTAG('L', '5', '6', '5') },
  { CODEC_ID_RAWVIDEO, MKTAG('B', '5', '6', '5') },
  { CODEC_ID_RAWVIDEO, MKTAG('2', '4', 'B', 'G') },
  { CODEC_ID_RAWVIDEO, MKTAG('B', 'G', 'R', 'A') },
  { CODEC_ID_RAWVIDEO, MKTAG('R', 'G', 'B', 'A') },
  { CODEC_ID_RAWVIDEO, MKTAG('A', 'B', 'G', 'R') },
  { CODEC_ID_RAWVIDEO, MKTAG('b', '1', '6', 'g') },
  { CODEC_ID_RAWVIDEO, MKTAG('b', '4', '8', 'r') },
  { CODEC_ID_RAWVIDEO, MKTAG('D', 'V', 'O', 'O') }, /* Digital Voodoo SD 8 Bit */

  { CODEC_ID_R10K,   MKTAG('R', '1', '0', 'k') }, /* UNCOMPRESSED 10BIT RGB */
  { CODEC_ID_R10K,   MKTAG('R', '1', '0', 'g') }, /* UNCOMPRESSED 10BIT RGB */
  { CODEC_ID_R210,   MKTAG('r', '2', '1', '0') }, /* UNCOMPRESSED 10BIT RGB */
  { CODEC_ID_V210,   MKTAG('v', '2', '1', '0') }, /* UNCOMPRESSED 10BIT 4:2:2 */

  { CODEC_ID_MJPEG,  MKTAG('j', 'p', 'e', 'g') }, /* PhotoJPEG */
  { CODEC_ID_MJPEG,  MKTAG('m', 'j', 'p', 'a') }, /* Motion-JPEG (format A) */
  { CODEC_ID_MJPEG,  MKTAG('A', 'V', 'D', 'J') }, /* MJPEG with alpha-channel (AVID JFIF meridien compressed) */
  /*  { CODEC_ID_MJPEG,  MKTAG('A', 'V', 'R', 'n') }, *//* MJPEG with alpha-channel (AVID ABVB/Truevision NuVista) */
  { CODEC_ID_MJPEG,  MKTAG('d', 'm', 'b', '1') }, /* Motion JPEG OpenDML */
  { CODEC_ID_MJPEGB, MKTAG('m', 'j', 'p', 'b') }, /* Motion-JPEG (format B) */

  { CODEC_ID_SVQ1, MKTAG('S', 'V', 'Q', '1') }, /* Sorenson Video v1 */
  { CODEC_ID_SVQ1, MKTAG('s', 'v', 'q', '1') }, /* Sorenson Video v1 */
  { CODEC_ID_SVQ1, MKTAG('s', 'v', 'q', 'i') }, /* Sorenson Video v1 (from QT specs)*/
  { CODEC_ID_SVQ3, MKTAG('S', 'V', 'Q', '3') }, /* Sorenson Video v3 */

  { CODEC_ID_MPEG4, MKTAG('m', 'p', '4', 'v') },
  { CODEC_ID_MPEG4, MKTAG('D', 'I', 'V', 'X') }, /* OpenDiVX *//* sample files at http://heroinewarrior.com/xmovie.php3 use this tag */
  { CODEC_ID_MPEG4, MKTAG('X', 'V', 'I', 'D') },
  { CODEC_ID_MPEG4, MKTAG('3', 'I', 'V', '2') }, /* experimental: 3IVX files before ivx D4 4.5.1 */

  { CODEC_ID_H263, MKTAG('h', '2', '6', '3') }, /* H263 */
  { CODEC_ID_H263, MKTAG('s', '2', '6', '3') }, /* H263 ?? works */

  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'c', 'p') }, /* DV PAL */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'c', ' ') }, /* DV NTSC */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'p', 'p') }, /* DVCPRO PAL produced by FCP */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', '5', 'p') }, /* DVCPRO50 PAL produced by FCP */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', '5', 'n') }, /* DVCPRO50 NTSC produced by FCP */
  { CODEC_ID_DVVIDEO, MKTAG('A', 'V', 'd', 'v') }, /* AVID DV */
  { CODEC_ID_DVVIDEO, MKTAG('A', 'V', 'd', '1') }, /* AVID DV100 */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', 'q') }, /* DVCPRO HD 720p50 */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', 'p') }, /* DVCPRO HD 720p60 */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '2') },
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '4') },
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '5') }, /* DVCPRO HD 50i produced by FCP */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '6') }, /* DVCPRO HD 60i produced by FCP */
  { CODEC_ID_DVVIDEO, MKTAG('d', 'v', 'h', '3') }, /* DVCPRO HD 30p produced by FCP */

  { CODEC_ID_VP3,     MKTAG('V', 'P', '3', '1') }, /* On2 VP3 */
  { CODEC_ID_RPZA,    MKTAG('r', 'p', 'z', 'a') }, /* Apple Video (RPZA) */
  { CODEC_ID_CINEPAK, MKTAG('c', 'v', 'i', 'd') }, /* Cinepak */
  { CODEC_ID_8BPS,    MKTAG('8', 'B', 'P', 'S') }, /* Planar RGB (8BPS) */
  { CODEC_ID_SMC,     MKTAG('s', 'm', 'c', ' ') }, /* Apple Graphics (SMC) */
  { CODEC_ID_QTRLE,   MKTAG('r', 'l', 'e', ' ') }, /* Apple Animation (RLE) */
  { CODEC_ID_MSRLE,   MKTAG('W', 'R', 'L', 'E') },
  { CODEC_ID_QDRAW,   MKTAG('q', 'd', 'r', 'w') }, /* QuickDraw */

  { CODEC_ID_RAWVIDEO, MKTAG('W', 'R', 'A', 'W') },

  { CODEC_ID_H264, MKTAG('a', 'v', 'c', '1') }, /* AVC-1/H.264 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', 'p') }, /* AVC-Intra  50M 720p24/30/60 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', 'q') }, /* AVC-Intra  50M 720p25/50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', '2') }, /* AVC-Intra  50M 1080p25/50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', '3') }, /* AVC-Intra  50M 1080p24/30/60 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', '5') }, /* AVC-Intra  50M 1080i50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '5', '6') }, /* AVC-Intra  50M 1080i60 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', 'p') }, /* AVC-Intra 100M 720p24/30/60 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', 'q') }, /* AVC-Intra 100M 720p25/50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', '2') }, /* AVC-Intra 100M 1080p25/50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', '3') }, /* AVC-Intra 100M 1080p24/30/60 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', '5') }, /* AVC-Intra 100M 1080i50 */
  { CODEC_ID_H264, MKTAG('a', 'i', '1', '6') }, /* AVC-Intra 100M 1080i60 */

  { CODEC_ID_MPEG1VIDEO, MKTAG('m', '1', 'v', '1') }, /* Apple MPEG-1 Camcorder */
  { CODEC_ID_MPEG1VIDEO, MKTAG('m', 'p', 'e', 'g') }, /* MPEG */
  { CODEC_ID_MPEG1VIDEO, MKTAG('m', '1', 'v', ' ') },
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', '2', 'v', '1') }, /* Apple MPEG-2 Camcorder */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '1') }, /* MPEG2 HDV 720p30 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '2') }, /* MPEG2 HDV 1080i60 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '3') }, /* MPEG2 HDV 1080i50 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '5') }, /* MPEG2 HDV 720p25 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '6') }, /* MPEG2 HDV 1080p24 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '7') }, /* MPEG2 HDV 1080p25 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('h', 'd', 'v', '8') }, /* MPEG2 HDV 1080p30 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '5', 'n') }, /* MPEG2 IMX NTSC 525/60 50mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '5', 'p') }, /* MPEG2 IMX PAL 625/50 50mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '4', 'n') }, /* MPEG2 IMX NTSC 525/60 40mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '4', 'p') }, /* MPEG2 IMX PAL 625/50 40mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '3', 'n') }, /* MPEG2 IMX NTSC 525/60 30mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('m', 'x', '3', 'p') }, /* MPEG2 IMX PAL 625/50 30mb/s produced by FCP */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', '4') }, /* XDCAM HD422 720p24 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', '5') }, /* XDCAM HD422 720p25 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', '9') }, /* XDCAM HD422 720p60 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'a') }, /* XDCAM HD422 720p50 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'b') }, /* XDCAM HD422 1080i60 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'c') }, /* XDCAM HD422 1080i50 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'd') }, /* XDCAM HD422 1080p24 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'e') }, /* XDCAM HD422 1080p25 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', '5', 'f') }, /* XDCAM HD422 1080p30 CBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '1') }, /* XDCAM EX 720p30 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '2') }, /* XDCAM HD 1080i60 */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '3') }, /* XDCAM HD 1080i50 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '4') }, /* XDCAM EX 720p24 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '5') }, /* XDCAM EX 720p25 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '6') }, /* XDCAM HD 1080p24 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '7') }, /* XDCAM HD 1080p25 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '8') }, /* XDCAM HD 1080p30 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', '9') }, /* XDCAM EX 720p60 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'a') }, /* XDCAM EX 720p50 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'b') }, /* XDCAM EX 1080i60 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'c') }, /* XDCAM EX 1080i50 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'd') }, /* XDCAM EX 1080p24 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'e') }, /* XDCAM EX 1080p25 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('x', 'd', 'v', 'f') }, /* XDCAM EX 1080p30 VBR */
  { CODEC_ID_MPEG2VIDEO, MKTAG('A', 'V', 'm', 'p') }, /* AVID IMX PAL */

  { CODEC_ID_JPEG2000, MKTAG('m', 'j', 'p', '2') }, /* JPEG 2000 produced by FCP */

  { CODEC_ID_TARGA, MKTAG('t', 'g', 'a', ' ') }, /* Truevision Targa */
  { CODEC_ID_TIFF,  MKTAG('t', 'i', 'f', 'f') }, /* TIFF embedded in MOV */
  { CODEC_ID_GIF,   MKTAG('g', 'i', 'f', ' ') }, /* embedded gif files as frames (usually one "click to play movie" frame) */
  { CODEC_ID_PNG,   MKTAG('p', 'n', 'g', ' ') },

  { CODEC_ID_VC1, MKTAG('v', 'c', '-', '1') }, /* SMPTE RP 2025 */
  { CODEC_ID_CAVS, MKTAG('a', 'v', 's', '2') },

  { CODEC_ID_DIRAC, MKTAG('d', 'r', 'a', 'c') },
  { CODEC_ID_DNXHD, MKTAG('A', 'V', 'd', 'n') }, /* AVID DNxHD */
  //  { CODEC_ID_FLV1,  MKTAG('H', '2', '6', '3') }, /* Flash Media Server */
  { CODEC_ID_MSMPEG4V3, MKTAG('3', 'I', 'V', 'D') }, /* 3ivx DivX Doctor */
  { CODEC_ID_RAWVIDEO, MKTAG('A', 'V', '1', 'x') }, /* AVID 1:1x */
  { CODEC_ID_RAWVIDEO, MKTAG('A', 'V', 'u', 'p') },
  { CODEC_ID_SGI,   MKTAG('s', 'g', 'i', ' ') }, /* SGI  */
  { CODEC_ID_DPX,   MKTAG('d', 'p', 'x', ' ') }, /* DPX */

  { CODEC_ID_PRORES, MKTAG('a', 'p', 'c', 'h') }, /* Apple ProRes 422 High Quality */
  { CODEC_ID_PRORES, MKTAG('a', 'p', 'c', 'n') }, /* Apple ProRes 422 Standard Definition */
  { CODEC_ID_PRORES, MKTAG('a', 'p', 'c', 's') }, /* Apple ProRes 422 LT */
  { CODEC_ID_PRORES, MKTAG('a', 'p', 'c', 'o') }, /* Apple ProRes 422 Proxy */
  { CODEC_ID_PRORES, MKTAG('a', 'p', '4', 'h') }, /* Apple ProRes 4444 */

  { CODEC_ID_NONE, 0 },
};

///////////////////////////////////////////////




// TODO - this is a lazy implementation - for speed we should use bi-directional skip-lists

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  int32_t dts; ///< dts of keyframe
  uint64_t offs;  ///< offset in file
};


typedef struct {
  index_entry *idxhh;  ///< head of head list
  index_entry *idxht; ///< tail of head list

  int nclients;
  lives_clip_data_t **clients;
  pthread_mutex_t mutex;
} index_container_t;


typedef struct {
  int fd;
  boolean inited;
  boolean has_video;
  boolean has_audio;
  int vididx;
  AVStream *vidst;
  int64_t input_position;
  int64_t data_start;
  off_t filesize;
  MatroskaDemuxContext matroska;
  AVFormatContext *s;
  AVCodec *codec;
  AVCodecContext *ctx;
  AVFrame *picture;
  AVPacket avpkt;
  int64_t last_frame; ///< last frame displayed
  index_container_t *idxc;
  boolean expect_eof;
} lives_mkv_priv_t;


#define ERR_NOMEM         1
#define ERR_INVALID_DATA  2
#define ERR_MAX_DEPTH     3
#define ERR_EOF           4


static int ebml_parse_elem(const lives_clip_data_t *cdata,  EbmlSyntax *syntax, void *data);

static boolean matroska_read_packet(const lives_clip_data_t *cdata, AVPacket *pkt);

static index_entry *matroska_read_seek(const lives_clip_data_t *cdata, uint32_t timestamp);

static int matroska_read_close(const lives_clip_data_t *cdata);

static void matroska_clear_queue(MatroskaDemuxContext *matroska);

static index_entry *lives_add_idx(const lives_clip_data_t *cdata, uint64_t offset, uint32_t pts);
