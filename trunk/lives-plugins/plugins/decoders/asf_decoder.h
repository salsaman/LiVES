// codec types - it would be very nice if the ffmpeg devs would put this in a header...

typedef struct AVCodecTag {
  int id;
  unsigned int tag;
} AVCodecTag;

const AVCodecTag codec_bmp_tags[] = {
  { CODEC_ID_H264,         MKTAG('H', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('h', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('X', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('x', '2', '6', '4') },
  { CODEC_ID_H264,         MKTAG('a', 'v', 'c', '1') },
  { CODEC_ID_H264,         MKTAG('V', 'S', 'S', 'H') },
  { CODEC_ID_H263,         MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('X', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('T', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('L', '2', '6', '3') },
  { CODEC_ID_H263,         MKTAG('V', 'X', '1', 'K') },
  { CODEC_ID_H263,         MKTAG('Z', 'y', 'G', 'o') },
  { CODEC_ID_H263P,        MKTAG('H', '2', '6', '3') },
  { CODEC_ID_H263I,        MKTAG('I', '2', '6', '3') }, /* intel h263 */
  { CODEC_ID_H261,         MKTAG('H', '2', '6', '1') },
  { CODEC_ID_H263P,        MKTAG('U', '2', '6', '3') },
  { CODEC_ID_H263P,        MKTAG('v', 'i', 'v', '1') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', '5', '0') },
  { CODEC_ID_MPEG4,        MKTAG('X', 'V', 'I', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'P', '4', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'S', '2') },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  0 ) }, /* some broken avi use this */
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'V', '1') },
  { CODEC_ID_MPEG4,        MKTAG('B', 'L', 'Z', '0') },
  { CODEC_ID_MPEG4,        MKTAG('m', 'p', '4', 'v') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('W', 'V', '1', 'F') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'E', 'D', 'G') },
  { CODEC_ID_MPEG4,        MKTAG('R', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('3', 'I', 'V', '2') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'F', 'D', 'S') },
  { CODEC_ID_MPEG4,        MKTAG('F', 'V', 'F', 'W') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'C', 'O', 'D') },
  { CODEC_ID_MPEG4,        MKTAG('M', 'V', 'X', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('P', 'M', '4', 'V') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'M', 'P', '4') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'X', 'G', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'I', 'D', 'M') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'T', '3') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('H', 'D', 'X', '4') }, /* flipped video */
  { CODEC_ID_MPEG4,        MKTAG('D', 'M', 'K', '2') },
  { CODEC_ID_MPEG4,        MKTAG('D', 'I', 'G', 'I') },
  { CODEC_ID_MPEG4,        MKTAG('I', 'N', 'M', 'C') },
  { CODEC_ID_MPEG4,        MKTAG('E', 'P', 'H', 'V') }, /* Ephv MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('E', 'M', '4', 'A') },
  { CODEC_ID_MPEG4,        MKTAG('M', '4', 'C', 'C') }, /* Divio MPEG-4 */
  { CODEC_ID_MPEG4,        MKTAG('S', 'N', '4', '0') },
  { CODEC_ID_MPEG4,        MKTAG('V', 'S', 'P', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('U', 'L', 'D', 'X') },
  { CODEC_ID_MPEG4,        MKTAG('G', 'E', 'O', 'V') },
  { CODEC_ID_MPEG4,        MKTAG('S', 'I', 'P', 'P') }, /* Samsung SHR-6040 */
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '3') }, /* default signature when using MSMPEG4 */
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', '4', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('M', 'P', 'G', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '5') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '6') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'I', 'V', '4') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('D', 'V', 'X', '3') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('A', 'P', '4', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '1') },
  { CODEC_ID_MSMPEG4V3,    MKTAG('C', 'O', 'L', '0') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('M', 'P', '4', '2') },
  { CODEC_ID_MSMPEG4V2,    MKTAG('D', 'I', 'V', '2') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', 'G', '4') },
  { CODEC_ID_MSMPEG4V1,    MKTAG('M', 'P', '4', '1') },
  { CODEC_ID_WMV1,         MKTAG('W', 'M', 'V', '1') },
  { CODEC_ID_WMV2,         MKTAG('W', 'M', 'V', '2') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', 'd') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 's', 'l') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '2', '5') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', '5', '0') },
  { CODEC_ID_DVVIDEO,      MKTAG('c', 'd', 'v', 'c') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('C', 'D', 'V', 'H') }, /* Canopus DV */
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', ' ') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'c', 's') },
  { CODEC_ID_DVVIDEO,      MKTAG('d', 'v', 'h', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '1') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('m', 'p', 'g', '2') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'P', 'E', 'G') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('P', 'I', 'M', '1') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('P', 'I', 'M', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG('V', 'C', 'R', '2') },
  { CODEC_ID_MPEG1VIDEO,   MKTAG( 1 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG( 2 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG4,        MKTAG( 4 ,  0 ,  0 ,  16) },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('D', 'V', 'R', ' ') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('M', 'M', 'E', 'S') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('L', 'M', 'P', '2') }, /* Lead MPEG2 in avi */
  { CODEC_ID_MPEG2VIDEO,   MKTAG('s', 'l', 'i', 'f') },
  { CODEC_ID_MPEG2VIDEO,   MKTAG('E', 'M', '2', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('d', 'm', 'b', '1') },
  { CODEC_ID_MJPEG,        MKTAG('m', 'j', 'p', 'a') },
  { CODEC_ID_LJPEG,        MKTAG('L', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('J', 'P', 'G', 'L') }, /* Pegasus lossless JPEG */
  { CODEC_ID_JPEGLS,       MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - encoder */
  { CODEC_ID_MJPEG,        MKTAG('M', 'J', 'L', 'S') }, /* JPEG-LS custom FOURCC for avi - decoder */
  { CODEC_ID_MJPEG,        MKTAG('j', 'p', 'e', 'g') },
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'P', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'R', 'n') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'C', 'D', 'V') },
  { CODEC_ID_MJPEG,        MKTAG('Q', 'I', 'V', 'G') },
  { CODEC_ID_MJPEG,        MKTAG('S', 'L', 'M', 'J') }, /* SL M-JPEG */
  { CODEC_ID_MJPEG,        MKTAG('C', 'J', 'P', 'G') }, /* Creative Webcam JPEG */
  { CODEC_ID_MJPEG,        MKTAG('I', 'J', 'L', 'V') }, /* Intel JPEG Library Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('M', 'V', 'J', 'P') }, /* Midvid JPEG Video Codec */
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '1') },
  { CODEC_ID_MJPEG,        MKTAG('A', 'V', 'I', '2') },
  { CODEC_ID_MJPEG,        MKTAG('M', 'T', 'S', 'J') },
  { CODEC_ID_MJPEG,        MKTAG('Z', 'J', 'P', 'G') }, /* Paradigm Matrix M-JPEG Codec */
  { CODEC_ID_HUFFYUV,      MKTAG('H', 'F', 'Y', 'U') },
  { CODEC_ID_FFVHUFF,      MKTAG('F', 'F', 'V', 'H') },
  { CODEC_ID_CYUV,         MKTAG('C', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG( 0 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG( 3 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', '4', '2', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'Y', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'U', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'N', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('u', 'y', 'v', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'V', 'u', '1') },
  { CODEC_ID_RAWVIDEO,     MKTAG('2', 'v', 'u', 'y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('P', '4', '2', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', '1', '2') },
  { CODEC_ID_RAWVIDEO,     MKTAG('U', 'Y', 'V', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'Y', 'U', 'Y') },
  { CODEC_ID_RAWVIDEO,     MKTAG('I', 'Y', 'U', 'V') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', '8', '0', '0') },
  { CODEC_ID_RAWVIDEO,     MKTAG('H', 'D', 'Y', 'C') },
  { CODEC_ID_RAWVIDEO,     MKTAG('Y', 'V', 'U', '9') },
  { CODEC_ID_RAWVIDEO,     MKTAG('V', 'D', 'T', 'Z') }, /* SoftLab-NSK VideoTizer */
  { CODEC_ID_FRWU,         MKTAG('F', 'R', 'W', 'U') },
  { CODEC_ID_R210,         MKTAG('r', '2', '1', '0') },
  { CODEC_ID_V210,         MKTAG('v', '2', '1', '0') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '1') },
  { CODEC_ID_INDEO3,       MKTAG('I', 'V', '3', '2') },
  { CODEC_ID_INDEO4,       MKTAG('I', 'V', '4', '1') },
  { CODEC_ID_INDEO5,       MKTAG('I', 'V', '5', '0') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '1') },
  { CODEC_ID_VP3,          MKTAG('V', 'P', '3', '0') },
  { CODEC_ID_VP5,          MKTAG('V', 'P', '5', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '0') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '1') },
  { CODEC_ID_VP6,          MKTAG('V', 'P', '6', '2') },
  { CODEC_ID_VP6F,         MKTAG('V', 'P', '6', 'F') },
  { CODEC_ID_VP6F,         MKTAG('F', 'L', 'V', '4') },
  { CODEC_ID_ASV1,         MKTAG('A', 'S', 'V', '1') },
  { CODEC_ID_ASV2,         MKTAG('A', 'S', 'V', '2') },
  { CODEC_ID_VCR1,         MKTAG('V', 'C', 'R', '1') },
  { CODEC_ID_FFV1,         MKTAG('F', 'F', 'V', '1') },
  { CODEC_ID_XAN_WC4,      MKTAG('X', 'x', 'a', 'n') },
  { CODEC_ID_MIMIC,        MKTAG('L', 'M', '2', '0') },
  { CODEC_ID_MSRLE,        MKTAG('m', 'r', 'l', 'e') },
  { CODEC_ID_MSRLE,        MKTAG( 1 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSRLE,        MKTAG( 2 ,  0 ,  0 ,  0 ) },
  { CODEC_ID_MSVIDEO1,     MKTAG('M', 'S', 'V', 'C') },
  { CODEC_ID_MSVIDEO1,     MKTAG('m', 's', 'v', 'c') },
  { CODEC_ID_MSVIDEO1,     MKTAG('C', 'R', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('c', 'r', 'a', 'm') },
  { CODEC_ID_MSVIDEO1,     MKTAG('W', 'H', 'A', 'M') },
  { CODEC_ID_MSVIDEO1,     MKTAG('w', 'h', 'a', 'm') },
  { CODEC_ID_CINEPAK,      MKTAG('c', 'v', 'i', 'd') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('D', 'U', 'C', 'K') },
  { CODEC_ID_TRUEMOTION1,  MKTAG('P', 'V', 'E', 'Z') },
  { CODEC_ID_MSZH,         MKTAG('M', 'S', 'Z', 'H') },
  { CODEC_ID_ZLIB,         MKTAG('Z', 'L', 'I', 'B') },
  { CODEC_ID_SNOW,         MKTAG('S', 'N', 'O', 'W') },
  { CODEC_ID_4XM,          MKTAG('4', 'X', 'M', 'V') },
  { CODEC_ID_FLV1,         MKTAG('F', 'L', 'V', '1') },
  { CODEC_ID_FLASHSV,      MKTAG('F', 'S', 'V', '1') },
  { CODEC_ID_SVQ1,         MKTAG('s', 'v', 'q', '1') },
  { CODEC_ID_TSCC,         MKTAG('t', 's', 'c', 'c') },
  { CODEC_ID_ULTI,         MKTAG('U', 'L', 'T', 'I') },
  { CODEC_ID_VIXL,         MKTAG('V', 'I', 'X', 'L') },
  { CODEC_ID_QPEG,         MKTAG('Q', 'P', 'E', 'G') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '0') },
  { CODEC_ID_QPEG,         MKTAG('Q', '1', '.', '1') },
  { CODEC_ID_WMV3,         MKTAG('W', 'M', 'V', '3') },
  { CODEC_ID_VC1,          MKTAG('W', 'V', 'C', '1') },
  { CODEC_ID_VC1,          MKTAG('W', 'M', 'V', 'A') },
  { CODEC_ID_LOCO,         MKTAG('L', 'O', 'C', 'O') },
  { CODEC_ID_WNV1,         MKTAG('W', 'N', 'V', '1') },
  { CODEC_ID_AASC,         MKTAG('A', 'A', 'S', 'C') },
  { CODEC_ID_INDEO2,       MKTAG('R', 'T', '2', '1') },
  { CODEC_ID_FRAPS,        MKTAG('F', 'P', 'S', '1') },
  { CODEC_ID_THEORA,       MKTAG('t', 'h', 'e', 'o') },
  { CODEC_ID_TRUEMOTION2,  MKTAG('T', 'M', '2', '0') },
  { CODEC_ID_CSCD,         MKTAG('C', 'S', 'C', 'D') },
  { CODEC_ID_ZMBV,         MKTAG('Z', 'M', 'B', 'V') },
  { CODEC_ID_KMVC,         MKTAG('K', 'M', 'V', 'C') },
  { CODEC_ID_CAVS,         MKTAG('C', 'A', 'V', 'S') },
  { CODEC_ID_JPEG2000,     MKTAG('M', 'J', '2', 'C') },
  { CODEC_ID_VMNC,         MKTAG('V', 'M', 'n', 'c') },
  { CODEC_ID_TARGA,        MKTAG('t', 'g', 'a', ' ') },
  { CODEC_ID_PNG,          MKTAG('M', 'P', 'N', 'G') },
  { CODEC_ID_PNG,          MKTAG('P', 'N', 'G', '1') },
  { CODEC_ID_CLJR,         MKTAG('c', 'l', 'j', 'r') },
  { CODEC_ID_DIRAC,        MKTAG('d', 'r', 'a', 'c') },
  { CODEC_ID_RPZA,         MKTAG('a', 'z', 'p', 'r') },
  { CODEC_ID_RPZA,         MKTAG('R', 'P', 'Z', 'A') },
  { CODEC_ID_RPZA,         MKTAG('r', 'p', 'z', 'a') },
  { CODEC_ID_SP5X,         MKTAG('S', 'P', '5', '4') },
  { CODEC_ID_AURA,         MKTAG('A', 'U', 'R', 'A') },
  { CODEC_ID_AURA2,        MKTAG('A', 'U', 'R', '2') },
  { CODEC_ID_DPX,          MKTAG('d', 'p', 'x', ' ') },
  { CODEC_ID_KGV1,         MKTAG('K', 'G', 'V', '1') },
  { CODEC_ID_NONE,         0 }
};


typedef uint8_t lives_asf_guid[16];

const lives_asf_guid lives_asf_header = {
  0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xA6, 0xD9, 0x00, 0xAA, 0x00, 0x62, 0xCE, 0x6C
};

const lives_asf_guid lives_asf_file_header = {
  0xA1, 0xDC, 0xAB, 0x8C, 0x47, 0xA9, 0xCF, 0x11, 0x8E, 0xE4, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const lives_asf_guid lives_asf_stream_header = {
  0x91, 0x07, 0xDC, 0xB7, 0xB7, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const lives_asf_guid lives_asf_ext_stream_header = {
  0xCB, 0xA5, 0xE6, 0x14, 0x72, 0xC6, 0x32, 0x43, 0x83, 0x99, 0xA9, 0x69, 0x52, 0x06, 0x5B, 0x5A
};

const lives_asf_guid lives_asf_audio_stream = {
  0x40, 0x9E, 0x69, 0xF8, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};

/*
  const lives_asf_guid lives_asf_audio_conceal_none = {
  // 0x40, 0xa4, 0xf1, 0x49, 0x4ece, 0x11d0, 0xa3, 0xac, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6
  // New value lifted from avifile
  0x00, 0x57, 0xfb, 0x20, 0x55, 0x5B, 0xCF, 0x11, 0xa8, 0xfd, 0x00, 0x80, 0x5f, 0x5c, 0x44, 0x2b
  };

  const lives_asf_guid lives_asf_audio_conceal_spread = {
  0x50, 0xCD, 0xC3, 0xBF, 0x8F, 0x61, 0xCF, 0x11, 0x8B, 0xB2, 0x00, 0xAA, 0x00, 0xB4, 0xE2, 0x20
  };
*/
const lives_asf_guid lives_asf_video_stream = {
  0xC0, 0xEF, 0x19, 0xBC, 0x4D, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
};
/*
  const lives_asf_guid lives_asf_video_conceal_none = {
  0x00, 0x57, 0xFB, 0x20, 0x55, 0x5B, 0xCF, 0x11, 0xA8, 0xFD, 0x00, 0x80, 0x5F, 0x5C, 0x44, 0x2B
  };

*/

const lives_asf_guid lives_asf_command_stream = {
  0xC0, 0xCF, 0xDA, 0x59, 0xE6, 0x59, 0xD0, 0x11, 0xA3, 0xAC, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};

const lives_asf_guid lives_asf_comment_header = {
  0x33, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

const lives_asf_guid lives_asf_codec_comment_header = {
  0x40, 0x52, 0xD1, 0x86, 0x1D, 0x31, 0xD0, 0x11, 0xA3, 0xA4, 0x00, 0xA0, 0xC9, 0x03, 0x48, 0xF6
};
const lives_asf_guid lives_asf_codec_comment1_header = {
  0x41, 0x52, 0xd1, 0x86, 0x1D, 0x31, 0xD0, 0x11, 0xa3, 0xa4, 0x00, 0xa0, 0xc9, 0x03, 0x48, 0xf6
};

const lives_asf_guid lives_asf_data_header = {
  0x36, 0x26, 0xb2, 0x75, 0x8E, 0x66, 0xCF, 0x11, 0xa6, 0xd9, 0x00, 0xaa, 0x00, 0x62, 0xce, 0x6c
};

const lives_asf_guid lives_asf_head1_guid = {
  0xb5, 0x03, 0xbf, 0x5f, 0x2E, 0xA9, 0xCF, 0x11, 0x8e, 0xe3, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

const lives_asf_guid lives_asf_head2_guid = {
  0x11, 0xd2, 0xd3, 0xab, 0xBA, 0xA9, 0xCF, 0x11, 0x8e, 0xe6, 0x00, 0xc0, 0x0c, 0x20, 0x53, 0x65
};

const lives_asf_guid lives_asf_extended_content_header = {
  0x40, 0xA4, 0xD0, 0xD2, 0x07, 0xE3, 0xD2, 0x11, 0x97, 0xF0, 0x00, 0xA0, 0xC9, 0x5E, 0xA8, 0x50
};

const lives_asf_guid lives_asf_simple_index_header = {
  0x90, 0x08, 0x00, 0x33, 0xB1, 0xE5, 0xCF, 0x11, 0x89, 0xF4, 0x00, 0xA0, 0xC9, 0x03, 0x49, 0xCB
};

const lives_asf_guid lives_asf_ext_stream_embed_stream_header = {
  0xe2, 0x65, 0xfb, 0x3a, 0xEF, 0x47, 0xF2, 0x40, 0xac, 0x2c, 0x70, 0xa9, 0x0d, 0x71, 0xd3, 0x43
};

const lives_asf_guid lives_asf_ext_stream_audio_stream = {
  0x9d, 0x8c, 0x17, 0x31, 0xE1, 0x03, 0x28, 0x45, 0xb5, 0x82, 0x3d, 0xf9, 0xdb, 0x22, 0xf5, 0x03
};

const lives_asf_guid lives_asf_metadata_header = {
  0xea, 0xcb, 0xf8, 0xc5, 0xaf, 0x5b, 0x77, 0x48, 0x84, 0x67, 0xaa, 0x8c, 0x44, 0xfa, 0x4c, 0xca
};

const lives_asf_guid lives_asf_marker_header = {
  0x01, 0xCD, 0x87, 0xF4, 0x51, 0xA9, 0xCF, 0x11, 0x8E, 0xE6, 0x00, 0xC0, 0x0C, 0x20, 0x53, 0x65
};

const lives_asf_guid lives_asf_my_guid = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const lives_asf_guid lives_asf_language_guid = {
  0xa9, 0x46, 0x43, 0x7c, 0xe0, 0xef, 0xfc, 0x4b, 0xb2, 0x29, 0x39, 0x3e, 0xde, 0x41, 0x5c, 0x85
};

const lives_asf_guid lives_asf_content_encryption = {
  0xfb, 0xb3, 0x11, 0x22, 0x23, 0xbd, 0xd2, 0x11, 0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e
};

const lives_asf_guid lives_asf_ext_content_encryption = {
  0x14, 0xe6, 0x8a, 0x29, 0x22, 0x26, 0x17, 0x4c, 0xb9, 0x35, 0xda, 0xe0, 0x7e, 0xe9, 0x28, 0x9c
};

const lives_asf_guid lives_asf_digital_signature = {
  0xfc, 0xb3, 0x11, 0x22, 0x23, 0xbd, 0xd2, 0x11, 0xb4, 0xb7, 0x00, 0xa0, 0xc9, 0x55, 0xfc, 0x6e
};


static const lives_asf_guid index_guid = {
  0x90, 0x08, 0x00, 0x33, 0xb1, 0xe5, 0xcf, 0x11, 0x89, 0xf4, 0x00, 0xa0, 0xc9, 0x03, 0x49, 0xcb
};

static const lives_asf_guid stream_bitrate_guid = { /* (http://get.to/sdp) */
  0xce, 0x75, 0xf8, 0x7b, 0x8d, 0x46, 0xd1, 0x11, 0x8d, 0x82, 0x00, 0x60, 0x97, 0xc9, 0xa2, 0xb2
};





/* List of official tags at http://msdn.microsoft.com/en-us/library/dd743066(VS.85).aspx */
/*const AVMetadataConv lives_asf_metadata_conv[] = {
  { "WM/AlbumArtist"     , "album_artist"},
  { "WM/AlbumTitle"      , "album"       },
  { "Author"             , "artist"      },
  { "Description"        , "comment"     },
  { "WM/Composer"        , "composer"    },
  { "WM/EncodedBy"       , "encoded_by"  },
  { "WM/EncodingSettings", "encoder"     },
  { "WM/Genre"           , "genre"       },
  { "WM/Language"        , "language"    },
  { "WM/OriginalFilename", "filename"    },
  { "WM/PartOfSet"       , "disc"        },
  { "WM/Publisher"       , "publisher"   },
  { "WM/Tool"            , "encoder"     },
  { "WM/TrackNumber"     , "track"       },
  { "WM/Track"           , "track"       },
  //  { "Year"               , "date"        }, TODO: conversion year<->date
  { 0 }
  };*/



#define ASF_PROBE_SIZE 16


#define PACKET_SIZE 3200

typedef struct {
  int num;
  unsigned char seq;
  /* use for reading */
  AVPacket pkt;
  int frag_offset;
  int timestamp;
  int64_t duration;

  int ds_span;                /* descrambling  */
  int ds_packet_size;
  int ds_chunk_size;

  int64_t packet_pos;

  uint16_t stream_language_index;

} ASFStream;


typedef struct {
  lives_asf_guid guid;                  ///< generated by client computer
  uint64_t file_size;         /**< in bytes
			       *   invalid if broadcasting */
  uint64_t create_time;       /**< time of creation, in 100-nanosecond units since 1.1.1601
			       *   invalid if broadcasting */
  uint64_t play_time;         /**< play time, in 100-nanosecond units
			       * invalid if broadcasting */
  uint64_t send_time;         /**< time to send file, in 100-nanosecond units
			       *   invalid if broadcasting (could be ignored) */
  uint32_t preroll;           /**< timestamp of the first packet, in milliseconds
			       *   if nonzero - subtract from time */
  uint32_t ignore;            ///< preroll is 64bit - but let's just ignore it
  uint32_t flags;             /**< 0x01 - broadcast
			       *   0x02 - seekable
			       *   rest is reserved should be 0 */
  uint32_t min_pktsize;       /**< size of a data packet
			       *   invalid if broadcasting */
  uint32_t max_pktsize;       /**< shall be the same as for min_pktsize
			       *   invalid if broadcasting */
  uint32_t max_bitrate;       /**< bandwidth of stream in bps
			       *   should be the sum of bitrates of the
			       *   individual media streams */
} ASFMainHeader;


typedef struct {
  uint32_t packet_number;
  uint16_t packet_count;
} ASFIndex;


typedef struct {
  uint32_t seqno;
  int is_streamed;
  int asfid2avid[128];                 ///< conversion table from asf ID 2 AVStream ID
  ASFStream streams[128];              ///< it's max number and it's not that big
  uint32_t stream_bitrates[128];       ///< max number of streams, bitrate for each (for streaming)
  char stream_languages[128][6];       ///< max number of streams, language for each (RFC1766, e.g. en-US)
  /* non streamed additonnal info */
  uint64_t nb_packets;                 ///< how many packets are there in the file, invalid if broadcasting
  int64_t duration;                    ///< in 100ns units
  /* packet filling */
  unsigned char multi_payloads_present;
  int packet_size_left;
  int packet_timestamp_start;
  int packet_timestamp_end;
  unsigned int packet_nb_payloads;
  int packet_nb_frames;
  uint8_t packet_buf[PACKET_SIZE];
  /* only for reading */
  uint64_t data_offset;                ///< beginning of the first data packet
  uint64_t data_object_offset;         ///< data object offset (excl. GUID & size)
  uint64_t data_object_size;           ///< size of the data object
  int index_read;

  ASFMainHeader hdr;

  int packet_flags;
  int packet_property;
  int packet_timestamp;
  int packet_segsizetype;
  int packet_segments;
  int packet_seq;
  int packet_replic_size;
  int packet_key_frame;
  int packet_padsize;
  unsigned int packet_frag_offset;
  unsigned int packet_frag_size;
  int64_t packet_frag_timestamp;
  int packet_multi_size;
  int packet_obj_size;
  int packet_time_delta;
  int packet_time_start;
  int64_t packet_pos;

  int stream_index;


  int64_t last_indexed_pts;
  ASFIndex* index_ptr;
  uint32_t nb_index_count;
  uint32_t nb_index_memory_alloc;
  uint16_t maximum_packet;

  ASFStream* asf_st;                   ///< currently decoded stream
} ASFContext;


#define FRAME_HEADER_SIZE 17

typedef struct _index_entry index_entry;

struct _index_entry {
  index_entry *next; ///< ptr to next entry
  uint32_t dts; ///< dts of keyframe
  uint64_t offs;  ///< offset in file to asf packet header
  uint8_t frag; ///< fragment number (counting only video fragments)
};


typedef struct {
  int fd; ///< file handle 208 477373 22415 108fat32
  int64_t input_position; /// current or next input postion
  int64_t data_start; ///< offset of data start in file
  int64_t hdr_start;  ///< file offset of current asf packet
  int64_t start_dts;  ///< first video dts
  int64_t frame_dts;
  boolean have_start_dts;
  boolean black_fill;
  size_t def_packet_size;
  off_t filesize;
  ASFContext *asf;
  AVFormatContext *s;
  AVCodecContext *ctx;
  AVStream *st;
  ASFStream *asf_st;
  AVFrame *picture;
  AVPacket avpkt;
  int64_t last_frame; ///< last frame decoded
  index_entry *idx;  ///< linked list of index (keyframes)
  index_entry *kframe; ///< current keyframe
  int fragnum; ///< current fragment number
} lives_asf_priv_t;


#define GET_UTF16(val, GET_16BIT, ERROR)	\
  val = GET_16BIT;				\
  {						\
    unsigned int hi = val - 0xD800;		\
    if (hi < 0x800) {				\
      val = GET_16BIT - 0xDC00;			\
      if (val > 0x3FFU || hi > 0x3FFU)		\
	ERROR					\
	  val += (hi<<10) + 0x10000;		\
    }						\
  }						\


#define PUT_UTF8(val, tmp, PUT_BYTE)			\
  {							\
    int bytes, shift;					\
    uint32_t in = val;					\
    if (in < 0x80) {					\
      tmp = in;						\
      PUT_BYTE						\
        } else {					\
      bytes = (av_log2(in) + 4) / 5;			\
      shift = (bytes - 1) * 6;				\
      tmp = (256 - (256 >> bytes)) | (in >> shift);	\
      PUT_BYTE						\
	while (shift >= 6) {				\
	  shift -= 6;					\
	  tmp = 0x80 | ((in >> shift) & 0x3f);		\
	  PUT_BYTE					\
            }						\
    }							\
  }


#define DO_2BITS(bits, var, defval)					\
  switch (bits & 3)							\
    {									\
      int dummy;							\
    case 3: dummy=read(priv->fd,buffer,4); var = get_le32int(buffer); priv->input_position+=4; rsize+=4; break; \
    case 2: dummy=read (priv->fd,buffer,2); var = get_le16int(buffer); priv->input_position+=2; rsize+=2; break; \
    case 1: dummy=read (priv->fd,buffer,1); var = *buffer; priv->input_position++; rsize++; break; \
    default: var = defval; break;					\
    }



index_entry *index_upto(const lives_clip_data_t *, int pts);

