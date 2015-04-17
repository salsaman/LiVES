// colourspace.c
// LiVES
// (c) G. Finch 2004 - 2013 <salsaman@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// code for palette conversions

/*

 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA.
 *
 */

// *
// TODO -
//      - resizing of single plane (including bicubic)
//      - external plugins for palette conversion, resizing
//      - convert yuv subspace and sampling type

#include <math.h>

#include "main.h"

#ifdef USE_SWSCALE

#include <libswscale/swscale.h>

// for libweed-compat.h
#define HAVE_AVCODEC
#define HAVE_AVUTIL

#endif // USE_SWSCALE

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#include <weed/weed-host.h>

#else // HAVE_SYSTEM_WEED
#include "../libweed/weed.h"
#include "../libweed/weed-palettes.h"
#include "../libweed/weed-effects.h"
#include "../libweed/weed-host.h"
#endif

#ifdef USE_SWSCALE

#ifdef HAVE_SYSTEM_WEED_COMPAT
#include <weed/weed-compat.h>
#else
#include "../libweed/weed-compat.h"
#endif // HAVE_SYSTEM_WEED_COMPAT

#endif // USE_SWSCALE

#include "cvirtual.h"

#ifdef USE_SWSCALE
#define N_SWS_CTX 16

boolean swscale_ctx_list_inited=FALSE;

struct _swscale_ctx {
  int iwidth;
  int iheight;
  int width;
  int height;
  enum PixelFormat ipixfmt;
  enum PixelFormat opixfmt;
  int flags;
  struct SwsContext *ctx;
};

static struct _swscale_ctx swscale_ctx[N_SWS_CTX];

#endif // USE_SWSCALE

#define USE_THREADS 1


static pthread_t cthreads[MAX_FX_THREADS];

static boolean unal_inited=FALSE;

LIVES_INLINE G_GNUC_CONST int get_rowstride_value(int rowstride) {
  // from gdk-pixbuf.c
  /* Always align rows to 32-bit boundaries */
  return (rowstride + 3) & ~3;
}


LIVES_INLINE G_GNUC_CONST int get_last_rowstride_value(int width, int nchans) {
#ifdef GUI_GTK
  // from gdk pixbuf docs
  return width*(((nchans<<3)+7)>>3);
#else
  return width * nchans;
#endif

}

static void lives_free_buffer(uint8_t *pixels, livespointer data) {
  lives_free(pixels);
}

LIVES_INLINE G_GNUC_CONST uint8_t CLAMP0255(int32_t a) {
  return a>255?255:(a<0)?0:a;
}

/* precomputed tables */

// generic
static int *Y_R;
static int *Y_G;
static int *Y_B;
static int *Cb_R;
static int *Cb_G;
static int *Cb_B;
static int *Cr_R;
static int *Cr_G;
static int *Cr_B;


// clamped Y'CbCr
static int Y_Rc[256];
static int Y_Gc[256];
static int Y_Bc[256];
static int Cb_Rc[256];
static int Cb_Gc[256];
static int Cb_Bc[256];
static int Cr_Rc[256];
static int Cr_Gc[256];
static int Cr_Bc[256];


// unclamped Y'CbCr
static int Y_Ru[256];
static int Y_Gu[256];
static int Y_Bu[256];
static int Cb_Ru[256];
static int Cb_Gu[256];
static int Cb_Bu[256];
static int Cr_Ru[256];
static int Cr_Gu[256];
static int Cr_Bu[256];

// clamped BT.701
static int HY_Rc[256];
static int HY_Gc[256];
static int HY_Bc[256];
static int HCb_Rc[256];
static int HCb_Gc[256];
static int HCb_Bc[256];
static int HCr_Rc[256];
static int HCr_Gc[256];
static int HCr_Bc[256];


// unclamped BT.701
static int HY_Ru[256];
static int HY_Gu[256];
static int HY_Bu[256];
static int HCb_Ru[256];
static int HCb_Gu[256];
static int HCb_Bu[256];
static int HCr_Ru[256];
static int HCr_Gu[256];
static int HCr_Bu[256];

static boolean conv_RY_inited = FALSE;




// generic
static int *RGB_Y;
static int *R_Cr;
static int *G_Cb;
static int *G_Cr;
static int *B_Cb;


// clamped Y'CbCr
static int RGB_Yc[256];
static int R_Crc[256];
static int G_Cbc[256];
static int G_Crc[256];
static int B_Cbc[256];

// unclamped Y'CbCr
static int RGB_Yu[256];
static int R_Cru[256];
static int G_Cbu[256];
static int G_Cru[256];
static int B_Cbu[256];

// clamped BT.701
static int HRGB_Yc[256];
static int HR_Crc[256];
static int HG_Cbc[256];
static int HG_Crc[256];
static int HB_Cbc[256];

// unclamped BT.701
static int HRGB_Yu[256];
static int HR_Cru[256];
static int HG_Cbu[256];
static int HG_Cru[256];
static int HB_Cbu[256];

static int conv_YR_inited = 0;


static short min_Y,max_Y,min_UV,max_UV;


// averaging
static uint8_t *cavg;
static uint8_t cavgc[256][256];
static uint8_t cavgu[256][256];
static uint8_t cavgrgb[256][256];
static int avg_inited = 0;


// pre-post multiply alpha

static int unal[256][256];
static int al[256][256];
static int unalcy[256][256];
static int alcy[256][256];
static int unalcuv[256][256];
static int alcuv[256][256];



// clamping and subspace converters

// generic
static uint8_t *Y_to_Y;
static uint8_t *U_to_U;
static uint8_t *V_to_V;

// same subspace, clamped to unclamped
static uint8_t Yclamped_to_Yunclamped[256];
static uint8_t UVclamped_to_UVunclamped[256];


// same subspace, unclamped to clamped
static uint8_t Yunclamped_to_Yclamped[256];
static uint8_t UVunclamped_to_UVclamped[256];

static int conv_YY_inited=0;


// gamma correction

//#define TEST_GAMMA
#ifdef TEST_GAMMA

uint8_t gamma_lut[256];
double current_gamma=-1.;

/* Updates the gamma look-up-table. */
static inline void update_gamma_lut(double gamma) {
  register int i;
  double inv_gamma = (1./gamma);
  gamma_lut[0] = 0;
  for (i=1; i<256; ++i) gamma_lut[i] = CLAMP0255(myround(255.0 * pow((double)i / 255.0, inv_gamma)));
  current_gamma=gamma;
}

#endif





static void init_RGB_to_YUV_tables(void) {
  register int i;

  // Digital Y'UV proper [ITU-R BT.601-5] for digital NTSC
  // uses Kr = 0.299 and Kb = 0.114
  // offs U,V = 128

  // (I call this subspace YUV_SUBSPACE_YCBCR)

  // this is used for e.g. theora encoding, and for most video cards


  for (i = 0; i < 256; i++) {
    Y_Rc[i] = myround(0.299 * (double)i
                      * 219./255. * (1<<FP_BITS));   // Kr
    Y_Gc[i] = myround(0.587 * (double)i
                      * 219./255. * (1<<FP_BITS));   // Kb
    Y_Bc[i] = myround((0.114 * (double)i
                       // here we add the 16 which goes into all components
                       * 219./255. + 16.) * (1<<FP_BITS));

    Cb_Bc[i] = myround(-0.168736 * (double)i
                       * 224./255. * (1<<FP_BITS));
    Cb_Gc[i] = myround(-0.331264 * (double)i
                       * 224./255. * (1<<FP_BITS));
    Cb_Rc[i] = myround((0.500 * (double)i
                        * 224./255. + 128.) * (1<<FP_BITS));

    Cr_Bc[i] = myround(0.500 * (double)i
                       * 224./255. * (1<<FP_BITS));
    Cr_Gc[i] = myround(-0.418688 * (double)i
                       * 224./255. * (1<<FP_BITS));
    Cr_Rc[i] = myround((-0.081312 * (double)i
                        * 224./255. + 128.) * (1<<FP_BITS));
  }


  for (i = 0; i < 256; i++) {
    Y_Ru[i] = myround(0.299 * (double)i
                      * (1<<FP_BITS));
    Y_Gu[i] = myround(0.587 * (double)i
                      * (1<<FP_BITS));
    Y_Bu[i] = myround(0.114 * (double)i
                      * (1<<FP_BITS));


    Cb_Bu[i] = myround(-0.168736 * (double)i
                       * (1<<FP_BITS));
    Cb_Gu[i] = myround(-0.331264 * (double)i
                       * (1<<FP_BITS));
    Cb_Ru[i] = myround((0.500 * (double)i
                        + 128.) * (1<<FP_BITS));

    Cr_Bu[i] = myround(0.500 * (double)i
                       * (1<<FP_BITS));
    Cr_Gu[i] = myround(-0.418688 * (double)i
                       * (1<<FP_BITS));
    Cr_Ru[i] = myround((-0.081312 * (double)i
                        + 128.) * (1<<FP_BITS));
  }


  // Different values are used for hdtv, I call this subspace YUV_SUBSPACE_BT709

  // Kb = 0.0722
  // Kr = 0.2126



  // converting from one subspace to another is not recommended.


  for (i = 0; i < 256; i++) {
    HY_Rc[i] = myround(0.183 * (double)i
                       * (1<<FP_BITS));   // Kr
    HY_Gc[i] = myround(0.614 * (double)i
                       * (1<<FP_BITS));   // Kb
    HY_Bc[i] = myround((0.062 * (double)i
                        // here we add the 16 which goes into all components
                        + 16.) * (1<<FP_BITS));

    HCb_Bc[i] = myround(-0.101 * (double)i
                        * (1<<FP_BITS));
    HCb_Gc[i] = myround(-0.339 * (double)i
                        * (1<<FP_BITS));
    HCb_Rc[i] = myround((0.439 * (double)i
                         + 128.) * (1<<FP_BITS));

    HCr_Bc[i] = myround(0.439 * (double)i
                        * (1<<FP_BITS));
    HCr_Gc[i] = myround(-0.399 * (double)i
                        * (1<<FP_BITS));
    HCr_Rc[i] = myround((-0.040 * (double)i
                         + 128.) * (1<<FP_BITS));
  }


  for (i = 0; i < 256; i++) {
    HY_Ru[i] = myround(0.213 * (double)i
                       * (1<<FP_BITS));   // Kr
    HY_Gu[i] = myround(0.715 * (double)i
                       * (1<<FP_BITS));   // Kb
    HY_Bu[i] = myround(0.0722 * (double)i
                       * (1<<FP_BITS));

    HCb_Bu[i] = myround(-0.115 * (double)i
                        * (1<<FP_BITS));
    HCb_Gu[i] = myround(-0.4542 * (double)i
                        * (1<<FP_BITS));
    HCb_Ru[i] = myround((0.5 * (double)i
                         + 128.) * (1<<FP_BITS));

    HCr_Bu[i] = myround(0.5 * (double)i
                        * (1<<FP_BITS));
    HCr_Gu[i] = myround(-0.4542 * (double)i
                        * (1<<FP_BITS));
    HCr_Ru[i] = myround((-0.4554 * (double)i
                         + 128.) * (1<<FP_BITS));

  }

  conv_RY_inited = TRUE;
}




static void init_YUV_to_RGB_tables(void) {
  register int i;

  // These values are for what I call YUV_SUBSPACE_YCBCR

  /* clip Y values under 16 */
  for (i = 0; i < 17; i++) {
    RGB_Yc[i] = 0;
  }
  for (i = 17; i < 235; i++) {
    RGB_Yc[i] = myround(((double)i-16.)/219.*255. * (1<<FP_BITS));
  }
  /* clip Y values above 235 */
  for (i = 235; i < 256; i++) {
    RGB_Yc[i] = myround(235./219.*255. * (1<<FP_BITS));
  }

  /* clip Cb/Cr values below 16 */
  for (i = 0; i < 17; i++) {
    R_Crc[i] = 0;
    G_Crc[i] = 0;
    G_Cbc[i] = 0;
    B_Cbc[i] = 0;
  }
  for (i = 17; i < 240; i++) {
    R_Crc[i] = myround(1.402 * ((((double)i-16.)/224.*255.)-128.) * (1<<FP_BITS)); // 2*(1-Kr)
    G_Crc[i] = myround(-0.714136 * ((((double)i-16.)/224.*255.)-128.)  * (1<<FP_BITS));
    G_Cbc[i] = myround(-0.344136 * ((((double)i-16.)/224.*255.)-128.) * (1<<FP_BITS));
    B_Cbc[i] = myround(1.772 * ((((double)i-16.)/224.*255.)-128.) * (1<<FP_BITS)); // 2*(1-Kb)
  }
  /* clip Cb/Cr values above 240 */
  for (i = 240; i < 256; i++) {
    R_Crc[i] = myround(1.402 * 127. * (1<<FP_BITS)); // 2*(1-Kr)
    G_Crc[i] = myround(-0.714136 * 127.  * (1<<FP_BITS));
    G_Cbc[i] = myround(-0.344136 * 127. * (1<<FP_BITS));
    B_Cbc[i] = myround(1.772 * 127. * (1<<FP_BITS)); // 2*(1-Kb)
  }



  // unclamped Y'CbCr
  for (i = 0; i <= 255; i++) {
    RGB_Yu[i] = i * (1<<FP_BITS);
  }

  for (i = 0; i <= 255; i++) {
    R_Cru[i] = myround(1.402 * ((double)(i)-128.)) * (1<<FP_BITS); // 2*(1-Kr)
    G_Cru[i] = myround(-0.714136 * ((double)(i)-128.)) * (1<<FP_BITS); //
    G_Cbu[i] = myround(-0.344136 * ((double)(i)-128.)) * (1<<FP_BITS);
    B_Cbu[i] = myround(1.772 * ((double)(i)-128.)) * (1<<FP_BITS); // 2*(1-Kb)
  }





  // These values are for what I call YUV_SUBSPACE_BT709

  /* clip Y values under 16 */
  for (i = 0; i < 17; i++) {
    HRGB_Yc[i] = 0;
  }
  for (i = 17; i < 235; i++) {
    HRGB_Yc[i] = myround(((double)i-16.) * 1.164 * (1<<FP_BITS));
  }
  /* clip Y values above 235 */
  for (i = 235; i < 256; i++) {
    HRGB_Yc[i] = myround(235. * 1.164 * (1<<FP_BITS));
  }

  /* clip Cb/Cr values below 16 */
  for (i = 0; i < 17; i++) {
    HR_Crc[i] = 0;
    HG_Crc[i] = 0;
    HG_Cbc[i] = 0;
    HB_Cbc[i] = 0;
  }
  for (i = 17; i < 240; i++) {
    HR_Crc[i] = myround(1.793 * (((double)i-16.)-128.) * (1<<FP_BITS)); // 2*(1-Kr)
    HG_Crc[i] = myround(-0.533 * (((double)i-16.)-128.)  * (1<<FP_BITS));
    HG_Cbc[i] = myround(-0.213 * (((double)i-16.)-128.) * (1<<FP_BITS));
    HB_Cbc[i] = myround(2.112 * (((double)i-16.)-128.) * (1<<FP_BITS)); // 2*(1-Kb)
  }
  /* clip Cb/Cr values above 240 */
  for (i = 240; i < 256; i++) {
    HR_Crc[i] = myround(1.793 * 127. * (1<<FP_BITS)); // 2*(1-Kr)
    HG_Crc[i] = myround(-0.533 * 127.  * (1<<FP_BITS));
    HG_Cbc[i] = myround(-0.213 * 127. * (1<<FP_BITS));
    HB_Cbc[i] = myround(2.112 * 127. * (1<<FP_BITS)); // 2*(1-Kb)
  }

  // unclamped for bt.709

  for (i = 0; i < 255; i++) {
    HRGB_Yu[i] = i * (1<<FP_BITS);
  }


  for (i = 0; i <= 255; i++) {
    HR_Cru[i] = myround(1.57503 * ((double)(i)-128.)) * (1<<FP_BITS); // 2*(1-Kr)
    HG_Cru[i] = myround(-0.468204 * ((double)(i)-128.)) * (1<<FP_BITS); //
    HG_Cbu[i] = myround(-0.186375 * ((double)(i)-128.)) * (1<<FP_BITS);
    HB_Cbu[i] = myround(1.85524 * ((double)(i)-128.)) * (1<<FP_BITS); // 2*(1-Kb)
  }

  conv_YR_inited = 1;
}



static void init_YUV_to_YUV_tables(void) {
  register int i;

  // init clamped -> unclamped, same subspace
  for (i=0; i<17; i++) {
    Yclamped_to_Yunclamped[i]=0;
  }
  for (i=17; i<235; i++) {
    Yclamped_to_Yunclamped[i]=myround((i-16.)*255./219.);
  }
  for (i=235; i<256; i++) {
    Yclamped_to_Yunclamped[i]=255;
  }

  for (i=0; i<17; i++) {
    UVclamped_to_UVunclamped[i]=0;
  }
  for (i=17; i<240; i++) {
    UVclamped_to_UVunclamped[i]=myround((i-16.)*255./224.);
  }
  for (i=240; i<256; i++) {
    UVclamped_to_UVunclamped[i]=255;
  }


  for (i=0; i<256; i++) {
    Yunclamped_to_Yclamped[i]=myround((i/255.)*219.+16.);
    UVunclamped_to_UVclamped[i]=myround((i/255.)*224.+16.);
  }

  conv_YY_inited = 1;
}


static void init_average(void) {
  short a,b,c;
  int x,y;
  for (x=0; x<256; x++) {
    for (y=0; y<256; y++) {
      a=(short)(x-128);
      b=(short)(y-128);
      if ((c=(a+b-((a*b)>>8)+128))>240) c=240;
      cavgc[x][y]=(uint8_t)(c>16?c:16);         // this is fine because headroom==footroom==16
      if ((c=(a+b-((a*b)>>8)+128))>255) c=255;
      cavgu[x][y]=(uint8_t)(c>0?c:0);
      cavgrgb[x][y]=(x+y)/2;
    }
  }
  avg_inited=1;
}



static void init_unal(void) {
  // premult to postmult and vice-versa

  register int i,j;

  for (i=0; i<256; i++) { //alpha val
    for (j=0; j<256; j++) { // val to be converted
      unal[i][j]=(float)j*255./(float)i;
      al[i][j]=(float)j*(float)i/255.;

      // clamped versions
      unalcy[i][j]=((j-16.)*255./219.)*255./(float)i;
      alcy[i][j]=((j-16.)*255./219.)*(float)i/255.;
      unalcuv[i][j]=((j-16.)*255./224.)*255./(float)i;
      alcuv[i][j]=((j-16.)*255./224.)*(float)i/255.;

    }
  }
  unal_inited=TRUE;
}






static void set_conversion_arrays(int clamping, int subspace) {
  // set conversion arrays for RGB <-> YUV, also min/max YUV values
  // depending on clamping and subspace

  switch (subspace) {
  case WEED_YUV_SUBSPACE_YUV:
  case WEED_YUV_SUBSPACE_YCBCR:
    if (clamping==WEED_YUV_CLAMPING_CLAMPED) {
      Y_R=Y_Rc;
      Y_G=Y_Gc;
      Y_B=Y_Bc;

      Cb_R=Cb_Rc;
      Cb_G=Cb_Gc;
      Cb_B=Cb_Bc;

      Cr_R=Cr_Rc;
      Cr_G=Cr_Gc;
      Cr_B=Cr_Bc;

      RGB_Y=RGB_Yc;

      R_Cr=R_Crc;
      G_Cr=G_Crc;
      G_Cb=G_Cbc;
      B_Cb=B_Cbc;
    } else {
      Y_R=Y_Ru;
      Y_G=Y_Gu;
      Y_B=Y_Bu;

      Cb_R=Cb_Ru;
      Cb_G=Cb_Gu;
      Cb_B=Cb_Bu;

      Cr_R=Cr_Ru;
      Cr_G=Cr_Gu;
      Cr_B=Cr_Bu;

      RGB_Y=RGB_Yu;

      R_Cr=R_Cru;
      G_Cr=G_Cru;
      G_Cb=G_Cbu;
      B_Cb=B_Cbu;
    }
    break;
  case WEED_YUV_SUBSPACE_BT709:
    if (clamping==WEED_YUV_CLAMPING_CLAMPED) {
      Y_R=HY_Rc;
      Y_G=HY_Gc;
      Y_B=HY_Bc;

      Cb_R=HCb_Rc;
      Cb_G=HCb_Gc;
      Cb_B=HCb_Bc;

      Cr_R=HCr_Rc;
      Cr_G=HCr_Gc;
      Cr_B=HCr_Bc;

      RGB_Y=HRGB_Yc;

      R_Cr=HR_Crc;
      G_Cr=HG_Crc;
      G_Cb=HG_Cbc;
      B_Cb=HB_Cbc;
    } else {
      Y_R=HY_Ru;
      Y_G=HY_Gu;
      Y_B=HY_Bu;

      Cb_R=HCb_Ru;
      Cb_G=HCb_Gu;
      Cb_B=HCb_Bu;

      Cr_R=HCr_Ru;
      Cr_G=HCr_Gu;
      Cr_B=HCr_Bu;

      RGB_Y=HRGB_Yu;

      R_Cr=HR_Cru;
      G_Cr=HG_Cru;
      G_Cb=HG_Cbu;
      B_Cb=HB_Cbu;
    }

    break;
  }

  if (!avg_inited) init_average();

  if (clamping==WEED_YUV_CLAMPING_CLAMPED) {
    min_Y=min_UV=16;
    max_Y=235;
    max_UV=240;
    cavg=(uint8_t *)cavgc;
  } else {
    min_Y=min_UV=0;
    max_Y=max_UV=255;
    cavg=(uint8_t *)cavgu;
  }
}


static void get_YUV_to_YUV_conversion_arrays(int iclamping, int isubspace, int oclamping, int osubspace) {
  // get conversion arrays for YUV -> YUV depending on in/out clamping and subspace
  // currently only clamped <-> unclamped conversions are catered for, subspace conversions are not yet done

  if (!conv_YY_inited) init_YUV_to_YUV_tables();

  switch (isubspace) {
  case WEED_YUV_SUBSPACE_YUV:
    LIVES_WARN("YUV subspace not specified, assuming Y'CbCr");
  case WEED_YUV_SUBSPACE_YCBCR:
    switch (osubspace) {
    case WEED_YUV_SUBSPACE_YCBCR:
    case WEED_YUV_SUBSPACE_YUV:
      if (iclamping==WEED_YUV_CLAMPING_CLAMPED) {
        //Y'CbCr clamped -> Y'CbCr unclamped
        Y_to_Y=Yclamped_to_Yunclamped;
        U_to_U=V_to_V=UVclamped_to_UVunclamped;
      } else {
        //Y'CbCr unclamped -> Y'CbCr clamped
        Y_to_Y=Yunclamped_to_Yclamped;
        U_to_U=V_to_V=UVunclamped_to_UVclamped;
      }
      break;
      // TODO - other subspaces
    default:
      LIVES_ERROR("Invalid YUV subspace conversion");
    }
    break;
  case WEED_YUV_SUBSPACE_BT709:
    switch (osubspace) {
    case WEED_YUV_SUBSPACE_BT709:
    case WEED_YUV_SUBSPACE_YUV:
      if (iclamping==WEED_YUV_CLAMPING_CLAMPED) {
        //BT.709 clamped -> BT.709 unclamped
        Y_to_Y=Yclamped_to_Yunclamped;
        U_to_U=V_to_V=UVclamped_to_UVunclamped;
      } else {
        //BT.709 unclamped -> BT.709 clamped
        Y_to_Y=Yunclamped_to_Yclamped;
        U_to_U=V_to_V=UVunclamped_to_UVclamped;
      }
      break;
      // TODO - other subspaces
    default:
      LIVES_ERROR("Invalid YUV subspace conversion");
    }
  default:
    LIVES_ERROR("Invalid YUV subspace conversion");
    break;
  }
}



//////////////////////////
// pixel conversions

static LIVES_INLINE uint8_t avg_chroma(size_t x, size_t y) {
  // cavg == cavgc for clamped, cavgu for unclamped
  return *(cavg+(x<<8)+y);
}


static LIVES_INLINE void rgb2yuv(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t *y, uint8_t *u, uint8_t *v) {
  register short a;
  if ((a=((Y_R[r0]+Y_G[g0]+Y_B[b0])>>FP_BITS))>max_Y) a=max_Y;
  *y=a<min_Y?min_Y:a;
  if ((a=((Cr_R[r0]+Cr_G[g0]+Cr_B[b0])>>FP_BITS))>max_UV) a=max_UV;
  *u=a<min_UV?min_UV:a;
  if ((a=((Cb_R[r0]+Cb_G[g0]+Cb_B[b0])>>FP_BITS))>max_UV) a=max_UV;
  *v=a<min_UV?min_UV:a;
}

static LIVES_INLINE void rgb2uyvy(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, uyvy_macropixel *uyvy) {
  register short a;
  if ((a=((Y_R[r0]+Y_G[g0]+Y_B[b0])>>FP_BITS))>max_Y) a=max_Y;
  uyvy->y0=a<min_Y?min_Y:a;
  if ((a=((Y_R[r1]+Y_G[g1]+Y_B[b1])>>FP_BITS))>max_Y) a=max_Y;
  uyvy->y1=a<min_Y?min_Y:a;

  uyvy->u0=avg_chroma((Cr_R[r0]+Cr_G[g0]+Cr_B[b0])>>FP_BITS,
                      (Cr_R[r1]+Cr_G[g1]+Cr_B[b1])>>FP_BITS);

  uyvy->v0=avg_chroma((Cb_R[r0]+Cb_G[g0]+Cb_B[b0])>>FP_BITS,
                      (Cb_R[r1]+Cb_G[g1]+Cb_B[b1])>>FP_BITS);

}

static LIVES_INLINE void rgb2yuyv(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1, yuyv_macropixel *yuyv) {
  register short a;
  if ((a=((Y_R[r0]+Y_G[g0]+Y_B[b0])>>FP_BITS))>max_Y) a=max_Y;
  yuyv->y0=a<min_Y?min_Y:a;
  if ((a=((Y_R[r1]+Y_G[g1]+Y_B[b1])>>FP_BITS))>max_Y) a=max_Y;
  yuyv->y1=a<min_Y?min_Y:a;

  yuyv->u0=avg_chroma((Cr_R[r0]+Cr_G[g0]+Cr_B[b0])>>FP_BITS,
                      (Cr_R[r1]+Cr_G[g1]+Cr_B[b1])>>FP_BITS);

  yuyv->v0=avg_chroma((Cb_R[r0]+Cb_G[g0]+Cb_B[b0])>>FP_BITS,
                      (Cb_R[r1]+Cb_G[g1]+Cb_B[b1])>>FP_BITS);


}

static LIVES_INLINE void rgb2_411(uint8_t r0, uint8_t g0, uint8_t b0, uint8_t r1, uint8_t g1, uint8_t b1,
                                  uint8_t r2, uint8_t g2, uint8_t b2, uint8_t r3, uint8_t g3, uint8_t b3, yuv411_macropixel *yuv) {
  register int a;
  if ((a=((Y_R[r0]+Y_G[g0]+Y_B[b0])>>FP_BITS))>max_Y) a=max_Y;
  yuv->y0=a<min_Y?min_Y:a;
  if ((a=((Y_R[r1]+Y_G[g1]+Y_B[b1])>>FP_BITS))>max_Y) a=max_Y;
  yuv->y1=a<min_Y?min_Y:a;
  if ((a=((Y_R[r2]+Y_G[g2]+Y_B[b2])>>FP_BITS))>max_Y) a=max_Y;
  yuv->y2=a<min_Y?min_Y:a;
  if ((a=((Y_R[r3]+Y_G[g3]+Y_B[b3])>>FP_BITS))>max_Y) a=max_Y;
  yuv->y3=a<min_Y?min_Y:a;

  if ((a=((((Cb_R[r0]+Cb_G[g0]+Cb_B[b0])>>FP_BITS)+((Cb_R[r1]+Cb_G[g1]+Cb_B[b1])>>FP_BITS)+
           ((Cb_R[r2]+Cb_G[g2]+Cb_B[b2])>>FP_BITS)+((Cb_R[r3]+Cb_G[g3]+Cb_B[b3])>>FP_BITS))>>2))>max_UV) a=max_UV;
  yuv->v2=a<min_UV?min_UV:a;
  if ((a=((((Cr_R[r0]+Cr_G[g0]+Cr_B[b0])>>FP_BITS)+((Cr_R[r1]+Cr_G[g1]+Cr_B[b1])>>FP_BITS)+
           ((Cr_R[r2]+Cr_G[g2]+Cr_B[b2])>>FP_BITS)+((Cr_R[r3]+Cr_G[g3]+Cr_B[b3])>>FP_BITS))>>2))>max_UV) a=max_UV;
  yuv->u2=a<min_UV?min_UV:a;
}

static LIVES_INLINE void yuv2rgb(uint8_t y, uint8_t u, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
  *r = CLAMP0255((RGB_Y[y] + R_Cr[v]) >> FP_BITS);
  *g = CLAMP0255((RGB_Y[y] + G_Cb[u]+ G_Cr[v]) >> FP_BITS);
  *b = CLAMP0255((RGB_Y[y] + B_Cb[u]) >> FP_BITS);
}

static LIVES_INLINE void uyvy2rgb(uyvy_macropixel *uyvy, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                                  uint8_t *r1, uint8_t *g1, uint8_t *b1) {
  yuv2rgb(uyvy->y0,uyvy->u0,uyvy->v0,r0,g0,b0);
  yuv2rgb(uyvy->y1,uyvy->u0,uyvy->v0,r1,g1,b1);
  //if (uyvy->y0>240||uyvy->u0>240||uyvy->v0>240||uyvy->y1>240) lives_printerr("got unclamped !\n");
}


static LIVES_INLINE void yuyv2rgb(yuyv_macropixel *yuyv, uint8_t *r0, uint8_t *g0, uint8_t *b0,
                                  uint8_t *r1, uint8_t *g1, uint8_t *b1) {
  yuv2rgb(yuyv->y0,yuyv->u0,yuyv->v0,r0,g0,b0);
  yuv2rgb(yuyv->y1,yuyv->u0,yuyv->v0,r1,g1,b1);
}


static LIVES_INLINE void yuv888_2_rgb(uint8_t *yuv, uint8_t *rgb, boolean add_alpha) {
  yuv2rgb(yuv[0],yuv[1],yuv[2],&(rgb[0]),&(rgb[1]),&(rgb[2]));
  if (add_alpha) rgb[3]=255;
}


static LIVES_INLINE void yuva8888_2_rgba(uint8_t *yuva, uint8_t *rgba, boolean del_alpha) {
  yuv2rgb(yuva[0],yuva[1],yuva[2],&(rgba[0]),&(rgba[1]),&(rgba[2]));
  if (!del_alpha) rgba[3]=yuva[3];
}

static LIVES_INLINE void yuv888_2_bgr(uint8_t *yuv, uint8_t *bgr, boolean add_alpha) {
  yuv2rgb(yuv[0],yuv[1],yuv[2],&(bgr[2]),&(bgr[1]),&(bgr[0]));
  if (add_alpha) bgr[3]=255;
}


static LIVES_INLINE void yuva8888_2_bgra(uint8_t *yuva, uint8_t *bgra, boolean del_alpha) {
  yuv2rgb(yuva[0],yuva[1],yuva[2],&(bgra[2]),&(bgra[1]),&(bgra[0]));
  if (!del_alpha) bgra[3]=yuva[3];
}

static LIVES_INLINE void yuv888_2_argb(uint8_t *yuv, uint8_t *argb) {
  argb[0]=255;
  yuv2rgb(yuv[0],yuv[1],yuv[2],&(argb[1]),&(argb[2]),&(argb[3]));
}


static LIVES_INLINE void yuva8888_2_argb(uint8_t *yuva, uint8_t *argb) {
  argb[0]=yuva[3];
  yuv2rgb(yuva[0],yuva[1],yuva[2],&(argb[1]),&(argb[2]),&(argb[3]));
}

static LIVES_INLINE void uyvy_2_yuv422(uyvy_macropixel *uyvy, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) {
  *u0=uyvy->u0;
  *y0=uyvy->y0;
  *v0=uyvy->v0;
  *y1=uyvy->y1;
}

static LIVES_INLINE void yuyv_2_yuv422(yuyv_macropixel *yuyv, uint8_t *y0, uint8_t *u0, uint8_t *v0, uint8_t *y1) {
  *y0=yuyv->y0;
  *u0=yuyv->u0;
  *y1=yuyv->y1;
  *v0=yuyv->v0;
}



/////////////////////////////////////////////////
//utilities

LIVES_INLINE boolean weed_palette_is_alpha_palette(int pal) {
  return (pal>=1024&&pal<2048)?TRUE:FALSE;
}

LIVES_INLINE boolean weed_palette_is_rgb_palette(int pal) {
  return (pal<512)?TRUE:FALSE;
}

LIVES_INLINE boolean weed_palette_is_yuv_palette(int pal) {
  return (pal>=512&&pal<1024)?TRUE:FALSE;
}

LIVES_INLINE int weed_palette_get_numplanes(int pal) {
  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24||pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||
      pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV411||
      pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888||pal==WEED_PALETTE_AFLOAT||pal==WEED_PALETTE_A8||
      pal==WEED_PALETTE_A1||pal==WEED_PALETTE_RGBFLOAT||pal==WEED_PALETTE_RGBAFLOAT) return 1;
  if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P) return 3;
  if (pal==WEED_PALETTE_YUVA4444P) return 4;
  return 0; // unknown palette
}

LIVES_INLINE boolean weed_palette_is_valid_palette(int pal) {
  if (weed_palette_get_numplanes(pal)==0) return FALSE;
  return TRUE;
}

LIVES_INLINE int weed_palette_get_bits_per_macropixel(int pal) {
  if (pal==WEED_PALETTE_A8||pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P||
      pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P) return 8;
  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_BGR24) return 24;
  if (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||
      pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888||pal==WEED_PALETTE_YUV888||pal==WEED_PALETTE_YUVA8888)
    return 32;
  if (pal==WEED_PALETTE_YUV411) return 48;
  if (pal==WEED_PALETTE_AFLOAT) return sizeof(float);
  if (pal==WEED_PALETTE_A1) return 1;
  if (pal==WEED_PALETTE_RGBFLOAT) return (3*sizeof(float));
  if (pal==WEED_PALETTE_RGBAFLOAT) return (4*sizeof(float));
  return 0; // unknown palette
}


LIVES_INLINE int weed_palette_get_pixels_per_macropixel(int pal) {
  if (pal==WEED_PALETTE_UYVY8888||pal==WEED_PALETTE_YUYV8888) return 2;
  if (pal==WEED_PALETTE_YUV411) return 4;
  return 1;
}

LIVES_INLINE boolean weed_palette_is_float_palette(int pal) {
  return (pal==WEED_PALETTE_RGBAFLOAT||pal==WEED_PALETTE_AFLOAT||pal==WEED_PALETTE_RGBFLOAT)?TRUE:FALSE;
}

LIVES_INLINE boolean weed_palette_has_alpha_channel(int pal) {
  return (pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_BGRA32||pal==WEED_PALETTE_ARGB32||
          pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_YUVA8888||pal==WEED_PALETTE_RGBAFLOAT||
          weed_palette_is_alpha_palette(pal))?TRUE:FALSE;
}

LIVES_INLINE double weed_palette_get_plane_ratio_horizontal(int pal, int plane) {
  // return ratio of plane[n] width/plane[0] width;
  if (plane==0) return 1.0;
  if (plane==1||plane==2) {
    if (pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P) return 1.0;
    if (pal==WEED_PALETTE_YUV422P||pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane==3) {
    if (pal==WEED_PALETTE_YUVA4444P) return 1.0;
  }
  return 0.0;
}

LIVES_INLINE double weed_palette_get_plane_ratio_vertical(int pal, int plane) {
  // return ratio of plane[n] height/plane[n] height
  if (plane==0) return 1.0;
  if (plane==1||plane==2) {
    if (pal==WEED_PALETTE_YUV444P||pal==WEED_PALETTE_YUVA4444P||pal==WEED_PALETTE_YUV422P) return 1.0;
    if (pal==WEED_PALETTE_YUV420P||pal==WEED_PALETTE_YVU420P) return 0.5;
  }
  if (plane==3) {
    if (pal==WEED_PALETTE_YUVA4444P) return 1.0;
  }
  return 0.0;
}

boolean weed_palette_is_lower_quality(int p1, int p2) {
  // return TRUE if p1 is lower quality than p2
  // we don't yet handle float palettes, or RGB or alpha properly

  // currently only works well for YUV palettes

  if ((weed_palette_is_alpha_palette(p1)&&!weed_palette_is_alpha_palette(p2))||
      (weed_palette_is_alpha_palette(p2)&&!weed_palette_is_alpha_palette(p1))) return TRUE; // invalid conversion

  if (weed_palette_is_rgb_palette(p1)&&weed_palette_is_rgb_palette(p2)) return FALSE;

  switch (p2) {
  case WEED_PALETTE_YUVA8888:
    if (p1!=WEED_PALETTE_YUVA8888&&p1!=WEED_PALETTE_YUVA4444P) return TRUE;
    break;
  case WEED_PALETTE_YUVA4444P:
    if (p1!=WEED_PALETTE_YUVA8888&&p1!=WEED_PALETTE_YUVA4444P) return TRUE;
    break;
  case WEED_PALETTE_YUV888:
    if (p1!=WEED_PALETTE_YUVA8888&&p1!=WEED_PALETTE_YUVA4444P&&p1!=WEED_PALETTE_YUV444P&&p1!=WEED_PALETTE_YUVA4444P)
      return TRUE;
    break;
  case WEED_PALETTE_YUV444P:
    if (p1!=WEED_PALETTE_YUVA8888&&p1!=WEED_PALETTE_YUVA4444P&&p1!=WEED_PALETTE_YUV444P&&p1!=WEED_PALETTE_YUVA4444P)
      return TRUE;
    break;

  case WEED_PALETTE_YUV422P:
  case WEED_PALETTE_UYVY8888:
  case WEED_PALETTE_YUYV8888:
    if (p1!=WEED_PALETTE_YUVA8888&&p1!=WEED_PALETTE_YUVA4444P&&p1!=WEED_PALETTE_YUV444P&&
        p1!=WEED_PALETTE_YUVA4444P&&p1!=WEED_PALETTE_YUV422P&&p1!=WEED_PALETTE_UYVY8888&&p1!=WEED_PALETTE_YUYV8888)
      return TRUE;
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    if (p1==WEED_PALETTE_YUV411) return TRUE;
    break;
  case WEED_PALETTE_A8:
    if (p1==WEED_PALETTE_A1) return TRUE;
  }
  return FALSE; // TODO
}


const char *weed_palette_get_name(int pal) {
  switch (pal) {
  case WEED_PALETTE_RGB24:
    return "RGB24";
  case WEED_PALETTE_RGBA32:
    return "RGBA32";
  case WEED_PALETTE_BGR24:
    return "BGR24";
  case WEED_PALETTE_BGRA32:
    return "BGRA32";
  case WEED_PALETTE_ARGB32:
    return "ARGB32";
  case WEED_PALETTE_RGBFLOAT:
    return "RGBFLOAT";
  case WEED_PALETTE_RGBAFLOAT:
    return "RGBAFLOAT";
  case WEED_PALETTE_YUV888:
    return "YUV888";
  case WEED_PALETTE_YUVA8888:
    return "YUVA8888";
  case WEED_PALETTE_YUV444P:
    return "YUV444P";
  case WEED_PALETTE_YUVA4444P:
    return "YUVA4444P";
  case WEED_PALETTE_YUV422P:
    return "YUV4422P";
  case WEED_PALETTE_YUV420P:
    return "YUV420P";
  case WEED_PALETTE_YVU420P:
    return "YVU420P";
  case WEED_PALETTE_YUV411:
    return "YUV411";
  case WEED_PALETTE_UYVY8888:
    return "UYVY";
  case WEED_PALETTE_YUYV8888:
    return "YUYV";
  case WEED_PALETTE_A8:
    return "8 BIT ALPHA";
  case WEED_PALETTE_A1:
    return "1 BIT ALPHA";
  case WEED_PALETTE_AFLOAT:
    return "FLOAT ALPHA";
  default:
    if (pal>=2048) return "custom";
    return "unknown";
  }
}


const char *weed_yuv_clamping_get_name(int clamping) {
  if (clamping==WEED_YUV_CLAMPING_UNCLAMPED) return (_("unclamped"));
  if (clamping==WEED_YUV_CLAMPING_CLAMPED) return (_("clamped"));
  return NULL;
}


const char *weed_yuv_subspace_get_name(int subspace) {
  if (subspace==WEED_YUV_SUBSPACE_YUV) return "Y'UV";
  if (subspace==WEED_YUV_SUBSPACE_YCBCR) return "Y'CbCr";
  if (subspace==WEED_YUV_SUBSPACE_BT709) return "BT.709";
  return NULL;
}


char *weed_palette_get_name_full(int pal, int clamped, int subspace) {
  const char *pname=weed_palette_get_name(pal);

  if (!weed_palette_is_yuv_palette(pal)) return lives_strdup(pname);
  else {
    const char *clamp=weed_yuv_clamping_get_name(clamped);
    const char *sspace=weed_yuv_subspace_get_name(subspace);
    return lives_strdup_printf("%s:%s (%s)",pname,sspace,clamp);
  }
}




/////////////////////////////////////////////////////////
// just as an example: 1.0 == RGB(A), 0.5 == compressed to 50%, etc

double weed_palette_get_compression_ratio(int pal) {
  double tbits=0.;

  int nplanes=weed_palette_get_numplanes(pal);
  int pbits;

  register int i;

  if (!weed_palette_is_valid_palette(pal)) return 0.;
  if (weed_palette_is_alpha_palette(pal)) return 0.; // invalid for alpha palettes
  for (i=0; i<nplanes; i++) {
    pbits=weed_palette_get_bits_per_macropixel(pal)/weed_palette_get_pixels_per_macropixel(pal);
    tbits+=pbits*weed_palette_get_plane_ratio_vertical(pal,i)*weed_palette_get_plane_ratio_horizontal(pal,i);
  }
  if (weed_palette_has_alpha_channel(pal)) return tbits/32.;
  return tbits/24.;
}


//////////////////////////////////////////////////////////////


boolean lives_pixbuf_is_all_black(LiVESPixbuf *pixbuf) {
  int width=lives_pixbuf_get_width(pixbuf);
  int height=lives_pixbuf_get_height(pixbuf);
  int rstride=lives_pixbuf_get_rowstride(pixbuf);
  boolean has_alpha=lives_pixbuf_get_has_alpha(pixbuf);
  const uint8_t *pdata=lives_pixbuf_get_pixels_readonly(pixbuf);

  int offs=0;  // TODO *** - if it is a QImage, offs may be 1
  int psize=has_alpha?4:3;
  register int i,j;

  width*=psize;

  for (j=0; j<height; j++) {
    for (i=offs; i<width; i+=psize) {
      if (pdata[i]>BLACK_THRESH||pdata[i+1]>BLACK_THRESH||pdata[i+2]>BLACK_THRESH) {
        return FALSE;
      }
    }
    pdata+=rstride;
  }

  return TRUE;
}


void pixel_data_planar_from_membuf(void **pixel_data, void *data, size_t size, int palette) {
  // convert contiguous memory block planes to planar data
  // size is the byte size of the Y plane (width*height in pixels)

  switch (palette) {
  case WEED_PALETTE_YUV444P:
    lives_memcpy(pixel_data[0],data,size);
    lives_memcpy(pixel_data[1],data+size,size);
    lives_memcpy(pixel_data[2],data+size*2,size);
    break;
  case WEED_PALETTE_YUVA4444P:
    lives_memcpy(pixel_data[0],data,size);
    lives_memcpy(pixel_data[1],data+size,size);
    lives_memcpy(pixel_data[2],data+size*2,size);
    lives_memcpy(pixel_data[3],data+size*2,size);
    break;
  case WEED_PALETTE_YUV422P:
    lives_memcpy(pixel_data[0],data,size);
    lives_memcpy(pixel_data[1],data+size,size/2);
    lives_memcpy(pixel_data[2],data+size*3/2,size/2);
    break;
  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    lives_memcpy(pixel_data[0],data,size);
    lives_memcpy(pixel_data[1],data+size,size/4);
    lives_memcpy(pixel_data[2],data+size*5/4,size/4);
    break;
  }
}



///////////////////////////////////////////////////////////
// frame conversions

static void convert_yuv888_to_rgb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
                                        int orowstride, uint8_t *dest, boolean add_alpha,
                                        boolean clamped, int thread_id) {
  register int x,y,i;
  size_t offs=3;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv888_to_rgb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv888_to_rgb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) offs=4;
  orowstride-=offs*hsize;
  irowstride-=hsize*3;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuv888_2_rgb(src,dest,add_alpha);
      src+=3;
      dest+=offs;
    }
    dest+=orowstride;
    src+=irowstride;
  }
}


void *convert_yuv888_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv888_to_rgb_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                              ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->out_alpha,
                              ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}





static void convert_yuva8888_to_rgba_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, boolean del_alpha, boolean clamped,
    int thread_id) {
  register int x,y,i;

  size_t offs=4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=!del_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuva8888_to_rgba_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuva8888_to_rgba_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (del_alpha) offs=3;
  orowstride-=offs*hsize;
  irowstride-=hsize*4;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuva8888_2_rgba(src,dest,del_alpha);
      src+=4;
      dest+=offs;
    }
    dest+=orowstride;
    src+=irowstride;
  }
}


void *convert_yuva8888_to_rgba_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuva8888_to_rgba_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                                 ccparams->orowstrides[0],(uint8_t *)ccparams->dest,!ccparams->out_alpha,
                                 ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}





static void convert_yuv888_to_bgr_frame(uint8_t *src, int hsize, int vsize, int irowstride,
                                        int orowstride, uint8_t *dest, boolean add_alpha, boolean clamped,
                                        int thread_id) {
  register int x,y,i;
  size_t offs=3;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv888_to_bgr_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv888_to_bgr_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) offs=4;
  orowstride-=offs*hsize;
  irowstride-=hsize*3;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuv888_2_bgr(src,dest,add_alpha);
      src+=3;
      dest+=offs;
    }
    dest+=orowstride;
    src+=irowstride;
  }

}



void *convert_yuv888_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv888_to_bgr_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                              ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->out_alpha,
                              ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}




static void convert_yuva8888_to_bgra_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, boolean del_alpha, boolean clamped,
    int thread_id) {
  register int x,y,i;

  size_t offs=4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=!del_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuva8888_to_bgra_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuva8888_to_bgra_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (del_alpha) offs=3;
  orowstride-=offs*hsize;
  irowstride-=4*hsize;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuva8888_2_bgra(src,dest,del_alpha);
      src+=4;
      dest+=offs;
    }
    dest+=orowstride;
    src+=irowstride;
  }
}



void *convert_yuva8888_to_bgra_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuva8888_to_bgra_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                                 ccparams->orowstrides[0],(uint8_t *)ccparams->dest,!ccparams->out_alpha,
                                 ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}



static void convert_yuv888_to_argb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest,
    boolean clamped, int thread_id) {
  register int x,y,i;
  size_t offs=4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv888_to_argb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv888_to_argb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  orowstride-=offs*hsize;
  irowstride-=hsize*3;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuv888_2_argb(src,dest);
      src+=3;
      dest+=4;
    }
    dest+=orowstride;
    src+=irowstride;
  }
}


void *convert_yuv888_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv888_to_argb_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                               ccparams->orowstrides[0],(uint8_t *)ccparams->dest,
                               ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}





static void convert_yuva8888_to_argb_frame(uint8_t *src, int hsize, int vsize, int irowstride,
    int orowstride, uint8_t *dest, boolean clamped,
    int thread_id) {
  register int x,y,i;

  size_t offs=4;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    uint8_t *end=src+vsize*irowstride;
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuva8888_to_rgba_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuva8888_to_rgba_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  orowstride-=offs*hsize;
  irowstride-=hsize*4;

  for (y=0; y<vsize; y++) {
    for (x=0; x<hsize; x++) {
      yuva8888_2_argb(src,dest);
      src+=4;
      dest+=4;
    }
    dest+=orowstride;
    src+=irowstride;
  }
}


void *convert_yuva8888_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuva8888_to_argb_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                                 ccparams->orowstrides[0],(uint8_t *)ccparams->dest,
                                 ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}



static void convert_yuv420p_to_rgb_frame(uint8_t **src, int width, int height, int orowstride,
    uint8_t *dest, boolean add_alpha, boolean is_422, int sample_type,
    boolean clamped) {
  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int widthx,hwidth=width>>1;
  int opsize=3,opsize2;
  uint8_t y,u,v;

  if (add_alpha) opsize=4;

  widthx=width*opsize;
  opsize2=opsize*2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height; i++) {
    y=*(s_y++);
    u=s_u[0];
    v=s_v[0];

    yuv2rgb(y,u,v,&dest[0],&dest[1],&dest[2]);

    if (add_alpha) dest[3]=dest[opsize+3]=255;

    y=*(s_y++);
    dest+=opsize;

    yuv2rgb(y,u,v,&dest[0],&dest[1],&dest[2]);
    dest-=opsize;

    for (j=opsize2; j<widthx; j+=opsize2) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width

      // implements jpeg style subsampling : TODO - mpeg and dvpal style

      y=*(s_y++);
      u=s_u[(j/opsize)/2];
      v=s_v[(j/opsize)/2];

      yuv2rgb(y,u,v,&dest[j],&dest[j+1],&dest[j+2]);

      dest[j-opsize]=cavgrgb[dest[j-opsize]][dest[j]];
      dest[j-opsize+1]=cavgrgb[dest[j-opsize+1]][dest[j+1]];
      dest[j-opsize+2]=cavgrgb[dest[j-opsize+2]][dest[j+2]];

      y=*(s_y++);
      yuv2rgb(y,u,v,&dest[j+opsize],&dest[j+opsize+1],&dest[j+opsize+2]);

      if (add_alpha) dest[j+3]=dest[j+7]=255;

      if (!is_422&&!chroma&&i>0) {
        // pass 2
        // average two src rows
        dest[j-widthx]=cavgrgb[dest[j-widthx]][dest[j]];
        dest[j+1-widthx]=cavgrgb[dest[j+1-widthx]][dest[j+1]];
        dest[j+2-widthx]=cavgrgb[dest[j+2-widthx]][dest[j+2]];
        dest[j-opsize-widthx]=cavgrgb[dest[j-opsize-widthx]][dest[j-opsize]];
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
      }
    }
    if (!is_422&&chroma) {
      if (i>0) {
        dest[j-opsize-widthx]=cavgrgb[dest[j-opsize-widthx]][dest[j-opsize]];
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
      }
      s_u+=hwidth;
      s_v+=hwidth;
    }
    chroma=!chroma;
    dest+=orowstride;
  }

}


static void convert_yuv420p_to_bgr_frame(uint8_t **src, int width, int height, int orowstride,
    uint8_t *dest, boolean add_alpha, boolean is_422, int sample_type,
    boolean clamped) {
  // TODO - handle dvpal in sampling type
  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int widthx,hwidth=width>>1;
  int opsize=3,opsize2;
  uint8_t y,u,v;

  if (add_alpha) opsize=4;

  widthx=width*opsize;
  opsize2=opsize*2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height; i++) {
    y=*(s_y++);
    u=s_u[0];
    v=s_v[0];

    yuv2rgb(y,u,v,&dest[2],&dest[1],&dest[0]);

    if (add_alpha) dest[3]=dest[opsize+3]=255;

    y=*(s_y++);
    dest+=opsize;

    yuv2rgb(y,u,v,&dest[2],&dest[1],&dest[0]);
    dest-=opsize;

    for (j=opsize2; j<widthx; j+=opsize2) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width

      // implements jpeg style subsampling : TODO - mpeg and dvpal style

      y=*(s_y++);
      u=s_u[(j/opsize)/2];
      v=s_v[(j/opsize)/2];

      yuv2rgb(y,u,v,&dest[j+2],&dest[j+1],&dest[j]);

      dest[j-opsize]=cavgrgb[dest[j-opsize]][dest[j]];
      dest[j-opsize+1]=cavgrgb[dest[j-opsize+1]][dest[j+1]];
      dest[j-opsize+2]=cavgrgb[dest[j-opsize+2]][dest[j+2]];

      y=*(s_y++);
      yuv2rgb(y,u,v,&dest[j+opsize+2],&dest[j+opsize+1],&dest[j+opsize]);

      if (add_alpha) dest[j+3]=dest[j+7]=255;

      if (!is_422&&!chroma&&i>0) {
        // pass 2
        // average two src rows
        dest[j-widthx]=cavgrgb[dest[j-widthx]][dest[j]];
        dest[j+1-widthx]=cavgrgb[dest[j+1-widthx]][dest[j+1]];
        dest[j+2-widthx]=cavgrgb[dest[j+2-widthx]][dest[j+2]];
        dest[j-opsize-widthx]=cavgrgb[dest[j-opsize-widthx]][dest[j-opsize]];
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
      }
    }
    if (!is_422&&chroma) {
      if (i>0) {
        // TODO
        dest[j-opsize-widthx]=cavgrgb[dest[j-opsize-widthx]][dest[j-opsize]];
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
      }
      s_u+=hwidth;
      s_v+=hwidth;
    }
    chroma=!chroma;
    dest+=orowstride;
  }
}


static void convert_yuv420p_to_argb_frame(uint8_t **src, int width, int height, int orowstride,
    uint8_t *dest, boolean is_422, int sample_type,
    boolean clamped) {
  // TODO - handle dvpal in sampling type
  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int widthx,hwidth=width>>1;
  int opsize=4,opsize2;
  uint8_t y,u,v;

  widthx=width*opsize;
  opsize2=opsize*2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height; i++) {
    y=*(s_y++);
    u=s_u[0];
    v=s_v[0];

    yuv2rgb(y,u,v,&dest[1],&dest[2],&dest[3]);

    dest[0]=dest[4]=255;

    y=*(s_y++);
    dest+=opsize;

    yuv2rgb(y,u,v,&dest[0],&dest[1],&dest[2]);
    dest-=opsize;

    for (j=opsize2; j<widthx; j+=opsize2) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width

      // implements jpeg style subsampling : TODO - mpeg and dvpal style

      y=*(s_y++);
      u=s_u[(j/opsize)/2];
      v=s_v[(j/opsize)/2];

      yuv2rgb(y,u,v,&dest[j+1],&dest[j+2],&dest[j+3]);

      dest[j-opsize+1]=cavgrgb[dest[j-opsize+1]][dest[j+1]];
      dest[j-opsize+2]=cavgrgb[dest[j-opsize+2]][dest[j+2]];
      dest[j-opsize+3]=cavgrgb[dest[j-opsize+3]][dest[j+3]];

      y=*(s_y++);
      yuv2rgb(y,u,v,&dest[j+opsize+1],&dest[j+opsize+2],&dest[j+opsize+3]);

      dest[j]=dest[j+4]=255;

      if (!is_422&&!chroma&&i>0) {
        // pass 2
        // average two src rows
        dest[j+1-widthx]=cavgrgb[dest[j+1-widthx]][dest[j+1]];
        dest[j+2-widthx]=cavgrgb[dest[j+2-widthx]][dest[j+2]];
        dest[j+3-widthx]=cavgrgb[dest[j+3-widthx]][dest[j+3]];
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
        dest[j-opsize+3-widthx]=cavgrgb[dest[j-opsize+3-widthx]][dest[j-opsize+3]];
      }
    }
    if (!is_422&&chroma) {
      if (i>0) {
        // TODO
        dest[j-opsize+1-widthx]=cavgrgb[dest[j-opsize+1-widthx]][dest[j-opsize+1]];
        dest[j-opsize+2-widthx]=cavgrgb[dest[j-opsize+2-widthx]][dest[j-opsize+2]];
        dest[j-opsize+3-widthx]=cavgrgb[dest[j-opsize+3-widthx]][dest[j-opsize+3]];
      }
      s_u+=hwidth;
      s_v+=hwidth;
    }
    chroma=!chroma;
    dest+=orowstride;
  }
}


static void convert_rgb_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      uyvy_macropixel *u, boolean has_alpha, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=3,ipsize2;
  uint8_t *end;
  register int i;

  int x=3,y=4,z=5;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  end=rgbdata+(rowstride*vsize)-5;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }

        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_rgb_to_uyvy_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_rgb_to_uyvy_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize=4;
  }

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2uyvy(rgbdata[i],rgbdata[i+1],rgbdata[i+2],rgbdata[i+x],rgbdata[i+y],rgbdata[i+z],u++);
    }
  }
}


void *convert_rgb_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_rgb_to_uyvy_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (uyvy_macropixel *)ccparams->dest,ccparams->in_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      yuyv_macropixel *u, boolean has_alpha, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=3,ipsize2;
  uint8_t *end=rgbdata+(rowstride*vsize)-5;
  register int i;

  int x=3,y=4,z=5;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_rgb_to_yuyv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_rgb_to_yuyv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<prefs->nfx_threads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize=4;
  }

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 YUYV bytes
      rgb2yuyv(rgbdata[i],rgbdata[i+1],rgbdata[i+2],rgbdata[i+x],rgbdata[i+y],rgbdata[i+z],u++);
    }
  }
}


void *convert_rgb_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_rgb_to_yuyv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (yuyv_macropixel *)ccparams->dest,ccparams->in_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_bgr_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      uyvy_macropixel *u, boolean has_alpha, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=3,ipsize2;
  uint8_t *end=rgbdata+(rowstride*vsize)-5;
  register int i;

  int x=3,y=4,z=5;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    boolean useme=FALSE;
    int dheight;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_bgr_to_uyvy_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_bgr_to_uyvy_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize=4;
  }

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2uyvy(rgbdata[i+2],rgbdata[i+1],rgbdata[i],rgbdata[i+z],rgbdata[i+y],rgbdata[i+x],u++);
    }
  }
}


void *convert_bgr_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_bgr_to_uyvy_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (uyvy_macropixel *)ccparams->dest,ccparams->in_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}





static void convert_bgr_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      yuyv_macropixel *u, boolean has_alpha, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=3,ipsize2;

  uint8_t *end=rgbdata+(rowstride*vsize)-5;
  register int i;

  int x=3,y=4,z=5;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    boolean useme=FALSE;
    int dheight;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_bgr_to_yuyv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_bgr_to_yuyv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    ipsize=4;
  }

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2yuyv(rgbdata[i+2],rgbdata[i+1],rgbdata[i],rgbdata[i+z],rgbdata[i+y],rgbdata[i+x],u++);
    }
  }
}


void *convert_bgr_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_bgr_to_yuyv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (yuyv_macropixel *)ccparams->dest,ccparams->in_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}




static void convert_argb_to_uyvy_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                       uyvy_macropixel *u, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=4,ipsize2;
  uint8_t *end;
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  end=rgbdata+(rowstride*vsize)-5;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }

        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_argb_to_uyvy_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_argb_to_uyvy_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2uyvy(rgbdata[i+1],rgbdata[i+2],rgbdata[i+3],rgbdata[i+5],rgbdata[i+6],rgbdata[i+7],u++);
    }
  }
}


void *convert_argb_to_uyvy_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_argb_to_uyvy_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                             (uyvy_macropixel *)ccparams->dest,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}




static void convert_argb_to_yuyv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                       yuyv_macropixel *u, boolean clamped, int thread_id) {
  // for odd sized widths, cut the rightmost pixel
  int hs3,ipsize=4,ipsize2;
  uint8_t *end;
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  end=rgbdata+(rowstride*vsize)-5;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*(hsize>>1);

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }

        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_argb_to_yuyv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_argb_to_yuyv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  ipsize2=ipsize*2;
  hs3=((hsize>>1)*ipsize2)-(ipsize2-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2yuyv(rgbdata[i+1],rgbdata[i+2],rgbdata[i+3],rgbdata[i+5],rgbdata[i+6],rgbdata[i+7],u++);
    }
  }
}


void *convert_argb_to_yuyv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_argb_to_yuyv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                             (yuyv_macropixel *)ccparams->dest,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}





static void convert_rgb_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                     uint8_t *u, boolean in_has_alpha, boolean out_has_alpha,
                                     boolean clamped, int thread_id) {
  int ipsize=3,opsize=3;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;
  uint8_t in_alpha=255;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*hsize*opsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=in_has_alpha;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_rgb_to_yuv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_rgb_to_yuv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (in_has_alpha) ipsize=4;

  if (out_has_alpha) opsize=4;

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (in_has_alpha) in_alpha=rgbdata[i+3];
      if (out_has_alpha) u[3]=in_alpha;
      rgb2yuv(rgbdata[i],rgbdata[i+1],rgbdata[i+2],&(u[0]),&(u[1]),&(u[2]));
      u+=opsize;
    }
  }
}

void *convert_rgb_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_rgb_to_yuv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                           (uint8_t *)ccparams->dest,ccparams->in_alpha,ccparams->out_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_rgb_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      uint8_t **yuvp, boolean in_has_alpha, boolean out_has_alpha, boolean clamped,
                                      int thread_id) {
  int ipsize=3;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;
  uint8_t in_alpha=255,*a=NULL;

  uint8_t *y,*u,*v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  y=yuvp[0];
  u=yuvp[1];
  v=yuvp[2];
  if (out_has_alpha) a=yuvp[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;

        ccparams[i].destp[0]=y+dheight*i*hsize;
        ccparams[i].destp[1]=u+dheight*i*hsize;
        ccparams[i].destp[2]=v+dheight*i*hsize;
        if (out_has_alpha) ccparams[i].destp[3]=a+dheight*i*hsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=in_has_alpha;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_rgb_to_yuvp_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_rgb_to_yuvp_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (in_has_alpha) ipsize=4;

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (in_has_alpha) in_alpha=rgbdata[i+3];
      if (out_has_alpha) *(a++)=in_alpha;
      rgb2yuv(rgbdata[i],rgbdata[i+1],rgbdata[i+2],y,u,v);
      y++;
      u++;
      v++;
    }
  }
}


void *convert_rgb_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_rgb_to_yuvp_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (uint8_t **)ccparams->destp,ccparams->in_alpha,ccparams->out_alpha,ccparams->out_clamped,
                            ccparams->thread_id);
  return NULL;
}



static void convert_bgr_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                     uint8_t *u, boolean in_has_alpha, boolean out_has_alpha,
                                     boolean clamped, int thread_id) {
  int ipsize=3,opsize=3;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;
  uint8_t in_alpha=255;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;
        ccparams[i].dest=u+dheight*i*hsize*opsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=in_has_alpha;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_bgr_to_yuv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_bgr_to_yuv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (in_has_alpha) ipsize=4;

  if (out_has_alpha) opsize=4;

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (in_has_alpha) in_alpha=rgbdata[i+3];
      if (out_has_alpha) u[3]=in_alpha;
      rgb2yuv(rgbdata[i+2],rgbdata[i+1],rgbdata[i],&(u[0]),&(u[1]),&(u[2]));
      u+=opsize;
    }
  }
}

void *convert_bgr_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_bgr_to_yuv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                           (uint8_t *)ccparams->dest,ccparams->in_alpha,ccparams->out_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}



static void convert_bgr_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      uint8_t **yuvp, boolean in_has_alpha, boolean out_has_alpha, boolean clamped,
                                      int thread_id) {

  // TESTED !

  int ipsize=3;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;
  uint8_t in_alpha=255,*a=NULL;

  uint8_t *y,*u,*v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  y=yuvp[0];
  u=yuvp[1];
  v=yuvp[2];
  if (out_has_alpha) a=yuvp[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;

        ccparams[i].destp[0]=y+dheight*i*hsize;
        ccparams[i].destp[1]=u+dheight*i*hsize;
        ccparams[i].destp[2]=v+dheight*i*hsize;
        if (out_has_alpha) ccparams[i].destp[3]=a+dheight*i*hsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].in_alpha=in_has_alpha;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_bgr_to_yuvp_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_bgr_to_yuvp_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (in_has_alpha) ipsize=4;

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (in_has_alpha) in_alpha=rgbdata[i+3];
      if (out_has_alpha) *(a++)=in_alpha;
      rgb2yuv(rgbdata[i+2],rgbdata[i+1],rgbdata[i],&(y[0]),&(u[0]),&(v[0]));
      y++;
      u++;
      v++;
    }
  }
}


void *convert_bgr_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_bgr_to_yuvp_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (uint8_t **)ccparams->destp,ccparams->in_alpha,ccparams->out_alpha,ccparams->out_clamped,
                            ccparams->thread_id);
  return NULL;
}




static void convert_argb_to_yuv_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                      uint8_t *u, boolean out_has_alpha,
                                      boolean clamped, int thread_id) {
  int ipsize=4,opsize=3;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;

        ccparams[i].dest=u+dheight*i*hsize*opsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_rgb_to_yuv_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_rgb_to_yuv_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (out_has_alpha) opsize=4;

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (out_has_alpha) u[3]=rgbdata[i];
      rgb2yuv(rgbdata[i+1],rgbdata[i+2],rgbdata[i+3],&(u[0]),&(u[1]),&(u[2]));
      u+=opsize;
    }
  }
}

void *convert_argb_to_yuv_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_argb_to_yuv_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            (uint8_t *)ccparams->dest,ccparams->out_alpha,ccparams->out_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_argb_to_yuvp_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                       uint8_t **yuvp, boolean out_has_alpha, boolean clamped,
                                       int thread_id) {
  int ipsize=4;
  int iwidth;
  uint8_t *end=rgbdata+(rowstride*vsize);
  register int i;
  uint8_t *a=NULL;
  uint8_t *y,*u,*v;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  y=yuvp[0];
  u=yuvp[1];
  v=yuvp[2];
  if (out_has_alpha) a=yuvp[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)vsize/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((rgbdata+dheight*i*rowstride)<end) {

        ccparams[i].src=rgbdata+dheight*i*rowstride;
        ccparams[i].hsize=hsize;

        ccparams[i].destp[0]=y+dheight*i*hsize;
        ccparams[i].destp[1]=u+dheight*i*hsize;
        ccparams[i].destp[2]=v+dheight*i*hsize;
        if (out_has_alpha) ccparams[i].destp[3]=a+dheight*i*hsize;

        if (dheight*(i+1)>(vsize-4)) {
          dheight=vsize-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=rowstride;
        ccparams[i].out_alpha=out_has_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_argb_to_yuvp_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_argb_to_yuvp_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }



  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  hsize=(hsize>>1)<<1;
  iwidth=hsize*ipsize;

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<iwidth; i+=ipsize) {
      if (out_has_alpha) *(a++)=rgbdata[i];
      rgb2yuv(rgbdata[i+1],rgbdata[i+2],rgbdata[i+3],y,u,v);
      y++;
      u++;
      v++;
    }
  }
}


void *convert_argb_to_yuvp_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_argb_to_yuvp_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                             (uint8_t **)ccparams->destp,ccparams->out_alpha,ccparams->out_clamped,
                             ccparams->thread_id);
  return NULL;
}






static void convert_rgb_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        uint8_t **dest, boolean is_422, boolean has_alpha, int samtype, boolean clamped) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  int hs3;

  uint8_t *y,*Cb,*Cr;
  uyvy_macropixel u;
  register int i,j;
  boolean chroma_row=TRUE;

  int ipsize=3,ipsize2;
  size_t hhsize;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) ipsize=4;

  // ensure width and height are both divisible by two
  hsize=(hsize>>1)<<1;
  vsize=(vsize>>1)<<1;

  y=dest[0];
  Cb=dest[1];
  Cr=dest[2];

  hhsize=hsize>>1;
  ipsize2=ipsize*2;
  hs3=(hsize*ipsize)-(ipsize2-1);

  for (i=0; i<vsize; i++) {
    for (j=0; j<hs3; j+=ipsize2) {
      // mpeg style, Cb and Cr are co-located
      // convert 6 RGBRGB bytes to 4 UYVY bytes

      // TODO: for mpeg use rgb2yuv and write alternate u and v

      rgb2uyvy(rgbdata[j],rgbdata[j+1],rgbdata[j+2],rgbdata[j+ipsize],rgbdata[j+ipsize+1],rgbdata[j+ipsize+2],&u);

      *(y++)=u.y0;
      *(y++)=u.y1;
      *(Cb++)=u.u0;
      *(Cr++)=u.v0;

      if (!is_422&&chroma_row&&i>0) {
        // average two rows
        Cb[-1-hhsize]=avg_chroma(Cb[-1],Cb[-1-hhsize]);
        Cr[-1-hhsize]=avg_chroma(Cr[-1],Cr[-1-hhsize]);
      }

    }
    if (!is_422) {
      if (chroma_row) {
        Cb-=hhsize;
        Cr-=hhsize;
      }
      chroma_row=!chroma_row;
    }
    rgbdata+=rowstride;
  }
}



static void convert_argb_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
    uint8_t **dest, boolean is_422, int samtype, boolean clamped) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  int hs3;

  uint8_t *y,*Cb,*Cr;
  uyvy_macropixel u;
  register int i,j;
  boolean chroma_row=TRUE;

  int ipsize=4,ipsize2;
  size_t hhsize;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  // ensure width and height are both divisible by two
  hsize=(hsize>>1)<<1;
  vsize=(vsize>>1)<<1;

  y=dest[0];
  Cb=dest[1];
  Cr=dest[2];

  hhsize=hsize>>1;
  ipsize2=ipsize*2;
  hs3=(hsize*ipsize)-(ipsize2-1);

  for (i=0; i<vsize; i++) {
    for (j=0; j<hs3; j+=ipsize2) {
      // mpeg style, Cb and Cr are co-located
      // convert 6 RGBRGB bytes to 4 UYVY bytes

      // TODO: for mpeg use rgb2yuv and write alternate u and v

      rgb2uyvy(rgbdata[j+1],rgbdata[j+2],rgbdata[j+3],rgbdata[j+1+ipsize],rgbdata[j+2+ipsize+1],rgbdata[j+3+ipsize+2],&u);

      *(y++)=u.y0;
      *(y++)=u.y1;
      *(Cb++)=u.u0;
      *(Cr++)=u.v0;

      if (!is_422&&chroma_row&&i>0) {
        // average two rows
        Cb[-1-hhsize]=avg_chroma(Cb[-1],Cb[-1-hhsize]);
        Cr[-1-hhsize]=avg_chroma(Cr[-1],Cr[-1-hhsize]);
      }

    }
    if (!is_422) {
      if (chroma_row) {
        Cb-=hhsize;
        Cr-=hhsize;
      }
      chroma_row=!chroma_row;
    }
    rgbdata+=rowstride;
  }
}



static void convert_bgr_to_yuv420_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        uint8_t **dest, boolean is_422, boolean has_alpha, int samtype, boolean clamped) {
  // for odd sized widths, cut the rightmost pixel
  // TODO - handle different out sampling types
  int hs3;

  uint8_t *y,*Cb,*Cr;
  uyvy_macropixel u;
  register int i,j;
  int chroma_row=TRUE;
  int ipsize=3,ipsize2;
  size_t hhsize;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) ipsize=4;

  // ensure width and height are both divisible by two
  hsize=(hsize>>1)<<1;
  vsize=(vsize>>1)<<1;

  y=dest[0];
  Cb=dest[1];
  Cr=dest[2];

  ipsize2=ipsize*2;
  hhsize=hsize>>1;
  hs3=(hsize*ipsize)-(ipsize2-1);
  for (i=0; i<vsize; i++) {
    for (j=0; j<hs3; j+=ipsize2) {
      // convert 6 RGBRGB bytes to 4 UYVY bytes
      rgb2uyvy(rgbdata[j+2],rgbdata[j+1],rgbdata[j],rgbdata[j+ipsize+2],rgbdata[j+ipsize+1],rgbdata[j+ipsize],&u);

      *(y++)=u.y0;
      *(y++)=u.y1;
      *(Cb++)=u.u0;
      *(Cr++)=u.v0;

      if (!is_422&&chroma_row&&i>0) {
        // average two rows
        Cb[-1-hhsize]=avg_chroma(Cb[-1],Cb[-1-hhsize]);
        Cr[-1-hhsize]=avg_chroma(Cr[-1],Cr[-1-hhsize]);
      }
    }
    if (!is_422) {
      if (chroma_row) {
        Cb-=hhsize;
        Cr-=hhsize;
      }
      chroma_row=!chroma_row;
    }
    rgbdata+=rowstride;
  }
}


static void convert_yuv422p_to_uyvy_frame(uint8_t **src, int width, int height, uint8_t *dest) {
  // TODO - handle different in sampling types
  uint8_t *src_y=src[0];
  uint8_t *src_u=src[1];
  uint8_t *src_v=src[2];
  uint8_t *end=src_y+width*height;

  while (src_y<end) {
    *(dest++)=*(src_u++);
    *(dest++)=*(src_y++);
    *(dest++)=*(src_v++);
    *(dest++)=*(src_y++);
  }
}


static void convert_yuv422p_to_yuyv_frame(uint8_t **src, int width, int height, uint8_t *dest) {
  // TODO - handle different in sampling types

  uint8_t *src_y=src[0];
  uint8_t *src_u=src[1];
  uint8_t *src_v=src[2];
  uint8_t *end=src_y+width*height;

  while (src_y<end) {
    *(dest++)=*(src_u++);
    *(dest++)=*(src_y++);
    *(dest++)=*(src_v++);
    *(dest++)=*(src_y++);
  }
}


static void convert_rgb_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        yuv411_macropixel *u, boolean has_alpha, boolean clamped) {
  // for odd sized widths, cut the rightmost one, two or three pixels. Widths should be divisible by 4.
  // TODO - handle different out sampling types
  int hs3=(int)(hsize>>2)*12,ipstep=12;

  uint8_t *end;
  register int i;

  int x=3,y=4,z=5,a=6,b=7,c=8,d=9,e=10,f=11;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    a+=2;
    b+=2;
    c+=2;
    d+=3;
    e+=3;
    f+=3;
    hs3=(int)(hsize>>2)*16;
    ipstep=16;
  }
  end=rgbdata+(rowstride*vsize)+1-ipstep;
  hs3-=(ipstep-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i],rgbdata[i+1],rgbdata[i+2],rgbdata[i+x],rgbdata[i+y],rgbdata[i+z],rgbdata[i+a],rgbdata[i+b],rgbdata[i+c],rgbdata[i+d],
               rgbdata[i+e],rgbdata[i+f],u++);
    }
  }
}


static void convert_bgr_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
                                        yuv411_macropixel *u, boolean has_alpha, boolean clamped) {
  // for odd sized widths, cut the rightmost one, two or three pixels
  // TODO - handle different out sampling types
  int hs3=(int)(hsize>>2)*12,ipstep=12;

  uint8_t *end;
  register int i;

  int x=3,y=4,z=5,a=6,b=7,c=8,d=9,e=10,f=11;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) {
    z++;
    y++;
    x++;
    a+=2;
    b+=2;
    c+=2;
    d+=3;
    e+=3;
    f+=3;
    hs3=(int)(hsize>>2)*16;
    ipstep=16;
  }
  end=rgbdata+(rowstride*vsize)+1-ipstep;
  hs3-=(ipstep-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i+2],rgbdata[i+1],rgbdata[i],rgbdata[i+z],rgbdata[i+y],rgbdata[i+x],rgbdata[i+c],rgbdata[i+b],rgbdata[i+a],rgbdata[i+f],
               rgbdata[i+e],rgbdata[i+d],u++);
    }
  }
}


static void convert_argb_to_yuv411_frame(uint8_t *rgbdata, int hsize, int vsize, int rowstride,
    yuv411_macropixel *u, boolean clamped) {
  // for odd sized widths, cut the rightmost one, two or three pixels. Widths should be divisible by 4.
  // TODO - handle different out sampling types
  int hs3=(int)(hsize>>2)*12,ipstep=12;

  uint8_t *end;
  register int i;

  if (LIVES_UNLIKELY(!conv_RY_inited)) init_RGB_to_YUV_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  hs3=(int)(hsize>>2)*16;
  ipstep=16;

  end=rgbdata+(rowstride*vsize)+1-ipstep;
  hs3-=(ipstep-1);

  for (; rgbdata<end; rgbdata+=rowstride) {
    for (i=0; i<hs3; i+=ipstep) {
      // convert 12 RGBRGBRGBRGB bytes to 6 UYYVYY bytes
      rgb2_411(rgbdata[i+1],rgbdata[i+2],rgbdata[i+3],rgbdata[i+5],rgbdata[i+6],rgbdata[i+7],rgbdata[i+9],rgbdata[i+10],rgbdata[i+11],
               rgbdata[i+13],rgbdata[i+14],rgbdata[i+15],u++);
    }
  }
}

static void convert_uyvy_to_rgb_frame(uyvy_macropixel *src, int width, int height, int orowstride,
                                      uint8_t *dest, boolean add_alpha, boolean clamped, int thread_id) {
  register int i,j;
  int psize=6;
  int a=3,b=4,c=5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_uyvy_to_rgb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_uyvy_to_rgb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    psize=8;
    a=4;
    b=5;
    c=6;
  }

  orowstride-=width*psize;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      uyvy2rgb(src,&dest[0],&dest[1],&dest[2],&dest[a],&dest[b],&dest[c]);
      if (add_alpha) dest[3]=dest[7]=255;
      dest+=psize;
      src++;
    }
    dest+=orowstride;
  }
}


void *convert_uyvy_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_uyvy_to_rgb_frame((uyvy_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest,ccparams->out_alpha,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_uyvy_to_bgr_frame(uyvy_macropixel *src, int width, int height, int orowstride,
                                      uint8_t *dest, boolean add_alpha, boolean clamped, int thread_id) {
  register int i,j;
  int psize=6;

  int a=3,b=4,c=5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_uyvy_to_bgr_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_uyvy_to_bgr_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    psize=8;
    a=4;
    b=5;
    c=6;
  }

  orowstride-=width*psize;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      uyvy2rgb(src,&dest[2],&dest[1],&dest[0],&dest[c],&dest[b],&dest[a]);
      if (add_alpha) dest[3]=dest[7]=255;
      dest+=psize;
      src++;
    }
    dest+=orowstride;
  }
}


void *convert_uyvy_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_uyvy_to_bgr_frame((uyvy_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest,ccparams->out_alpha,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_uyvy_to_argb_frame(uyvy_macropixel *src, int width, int height, int orowstride,
                                       uint8_t *dest, boolean clamped, int thread_id) {
  register int i,j;
  int psize=8;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_uyvy_to_argb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_uyvy_to_argb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);


  orowstride-=width*psize;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      uyvy2rgb(src,&dest[1],&dest[2],&dest[3],&dest[5],&dest[6],&dest[7]);
      dest[0]=dest[4]=255;
      dest+=psize;
      src++;
    }
    dest+=orowstride;
  }
}


void *convert_uyvy_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_uyvy_to_argb_frame((uyvy_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                             (uint8_t *)ccparams->dest,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_rgb_frame(yuyv_macropixel *src, int width, int height, int orowstride,
                                      uint8_t *dest, boolean add_alpha, boolean clamped, int thread_id) {
  register int i,j;
  int psize=6;
  int a=3,b=4,c=5;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuyv_to_rgb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuyv_to_rgb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    psize=8;
    a=4;
    b=5;
    c=6;
  }

  orowstride-=width*psize;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      yuyv2rgb(src,&dest[0],&dest[1],&dest[2],&dest[a],&dest[b],&dest[c]);
      if (add_alpha) dest[3]=dest[7]=255;
      dest+=psize;
      src++;
    }
    dest+=orowstride;
  }
}


void *convert_yuyv_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuyv_to_rgb_frame((yuyv_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest,ccparams->out_alpha,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_bgr_frame(yuyv_macropixel *src, int width, int height, int orowstride,
                                      uint8_t *dest, boolean add_alpha, boolean clamped, int thread_id) {
  register int x;
  int size=width*height,psize=6;
  int a=3,b=4,c=5;
  register int i;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].out_alpha=add_alpha;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuyv_to_bgr_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuyv_to_bgr_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    psize=8;
    a=5;
    b=6;
    c=7;
  }

  for (x=0; x<size; x++) {
    yuyv2rgb(src,&dest[2],&dest[1],&dest[0],&dest[c],&dest[b],&dest[a]);
    if (add_alpha) dest[3]=dest[7]=255;
    dest+=psize;
    src++;
  }
}


void *convert_yuyv_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuyv_to_bgr_frame((yuyv_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                            (uint8_t *)ccparams->dest,ccparams->out_alpha,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_yuyv_to_argb_frame(yuyv_macropixel *src, int width, int height, int orowstride,
                                       uint8_t *dest, boolean clamped, int thread_id) {
  register int i,j;
  int psize=8;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((dheight*i)<height) {

        ccparams[i].src=src+dheight*i*width;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuyv_to_argb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuyv_to_argb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);


  orowstride-=width*psize;
  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      yuyv2rgb(src,&dest[1],&dest[2],&dest[3],&dest[5],&dest[6],&dest[7]);
      dest[0]=dest[4]=255;
      dest+=psize;
      src++;
    }
    dest+=orowstride;
  }
}


void *convert_yuyv_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuyv_to_argb_frame((yuyv_macropixel *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                             (uint8_t *)ccparams->dest,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_yuv420_to_uyvy_frame(uint8_t **src, int width, int height, uyvy_macropixel *dest) {
  register int i=0,j;
  uint8_t *y,*u,*v,*end;
  int hwidth=width>>1;
  boolean chroma=TRUE;

  // TODO - handle different in sampling types
  if (!avg_inited) init_average();

  y=src[0];
  u=src[1];
  v=src[2];

  end=y+width*height;

  while (y<end) {
    for (j=0; j<hwidth; j++) {
      dest->u0=u[0];
      dest->y0=y[0];
      dest->v0=v[0];
      dest->y1=y[1];

      if (chroma&&i>0) {
        dest[-hwidth].u0=avg_chroma(dest[-hwidth].u0,u[0]);
        dest[-hwidth].v0=avg_chroma(dest[-hwidth].v0,v[0]);
      }

      dest++;
      y+=2;
      u++;
      v++;
    }
    if (chroma) {
      u-=hwidth;
      v-=hwidth;
    }
    chroma=!chroma;
    i++;
  }
}


static void convert_yuv420_to_yuyv_frame(uint8_t **src, int width, int height, yuyv_macropixel *dest) {
  register int i=0,j;
  uint8_t *y,*u,*v,*end;
  int hwidth=width>>1;
  boolean chroma=TRUE;

  // TODO - handle different in sampling types
  if (!avg_inited) init_average();

  y=src[0];
  u=src[1];
  v=src[2];

  end=y+width*height;

  while (y<end) {
    for (j=0; j<hwidth; j++) {
      dest->y0=y[0];
      dest->u0=u[0];
      dest->y1=y[1];
      dest->v0=v[0];

      if (chroma&&i>0) {
        dest[-hwidth].u0=avg_chroma(dest[-hwidth].u0,u[0]);
        dest[-hwidth].v0=avg_chroma(dest[-hwidth].v0,v[0]);
      }

      dest++;
      y+=2;
      u++;
      v++;
    }
    if (chroma) {
      u-=hwidth;
      v-=hwidth;
    }
    chroma=!chroma;
    i++;
  }
}


static void convert_yuv_planar_to_rgb_frame(uint8_t **src, int width, int height, int orowstride, uint8_t *dest,
    boolean in_alpha, boolean out_alpha, boolean clamped, int thread_id) {
  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];
  uint8_t *a=NULL;

  uint8_t *end=y+width*height;

  size_t opstep=3,rowstride;
  register int i,j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (in_alpha) a=src[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((y+dheight*i*width)<end) {

        ccparams[i].hsize=width;

        ccparams[i].srcp[0]=y+dheight*i*width;
        ccparams[i].srcp[1]=u+dheight*i*width;
        ccparams[i].srcp[2]=v+dheight*i*width;
        if (in_alpha) ccparams[i].srcp[3]=a+dheight*i*width;

        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_alpha=in_alpha;
        ccparams[i].out_alpha=out_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv_planar_to_rgb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv_planar_to_rgb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (out_alpha) opstep=4;

  rowstride=orowstride-width*opstep;

  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      yuv2rgb(*(y++),*(u++),*(v++),&dest[0],&dest[1],&dest[2]);
      if (out_alpha) {
        if (in_alpha) {
          dest[3]=*(a++);
        } else dest[3]=255;
      }
      dest+=opstep;
    }
    dest+=rowstride;
  }
}

void *convert_yuv_planar_to_rgb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv_planar_to_rgb_frame((uint8_t **)ccparams->srcp,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                                  (uint8_t *)ccparams->dest,ccparams->in_alpha,ccparams->out_alpha,
                                  ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}


static void convert_yuv_planar_to_bgr_frame(uint8_t **src, int width, int height, int orowstride, uint8_t *dest,
    boolean in_alpha, boolean out_alpha, boolean clamped, int thread_id) {
  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];
  uint8_t *a=NULL;

  uint8_t *end=y+width*height;

  size_t opstep=4,rowstride;
  register int i,j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (in_alpha) a=src[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((y+dheight*i*width)<end) {

        ccparams[i].hsize=width;

        ccparams[i].srcp[0]=y+dheight*i*width;
        ccparams[i].srcp[1]=u+dheight*i*width;
        ccparams[i].srcp[2]=v+dheight*i*width;
        if (in_alpha) ccparams[i].srcp[3]=a+dheight*i*width;

        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_alpha=in_alpha;
        ccparams[i].out_alpha=out_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv_planar_to_bgr_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv_planar_to_bgr_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  rowstride=orowstride-width*opstep;

  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      yuv2rgb(*(y++),*(u++),*(v++),&dest[2],&dest[1],&dest[0]);
      if (out_alpha) {
        if (in_alpha) {
          dest[3]=*(a++);
        } else dest[3]=255;
      }
      dest+=opstep;
    }
    dest+=rowstride;
  }
}


void *convert_yuv_planar_to_bgr_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv_planar_to_bgr_frame((uint8_t **)ccparams->srcp,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                                  (uint8_t *)ccparams->dest,ccparams->in_alpha,ccparams->out_alpha,
                                  ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}



static void convert_yuv_planar_to_argb_frame(uint8_t **src, int width, int height, int orowstride, uint8_t *dest,
    boolean in_alpha, boolean clamped, int thread_id) {
  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];
  uint8_t *a=NULL;

  uint8_t *end=y+width*height;

  size_t opstep=4,rowstride;
  register int i,j;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  if (in_alpha) a=src[3];

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((y+dheight*i*width)<end) {

        ccparams[i].hsize=width;

        ccparams[i].srcp[0]=y+dheight*i*width;
        ccparams[i].srcp[1]=u+dheight*i*width;
        ccparams[i].srcp[2]=v+dheight*i*width;
        if (in_alpha) ccparams[i].srcp[3]=a+dheight*i*width;

        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].in_alpha=in_alpha;
        ccparams[i].out_clamped=clamped;
        ccparams[i].thread_id=i;

        if (useme) convert_yuv_planar_to_argb_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_yuv_planar_to_argb_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  rowstride=orowstride-width*opstep;

  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {
      yuv2rgb(*(y++),*(u++),*(v++),&dest[1],&dest[2],&dest[3]);
      if (in_alpha) {
        dest[0]=*(a++);
      } else dest[0]=255;
      dest+=opstep;
    }
    dest+=rowstride;
  }
}


void *convert_yuv_planar_to_argb_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_yuv_planar_to_argb_frame((uint8_t **)ccparams->srcp,ccparams->hsize,ccparams->vsize,ccparams->orowstrides[0],
                                   (uint8_t *)ccparams->dest,ccparams->in_alpha,ccparams->in_clamped,ccparams->thread_id);
  return NULL;
}




static void convert_yuv_planar_to_uyvy_frame(uint8_t **src, int width, int height, uyvy_macropixel *uyvy, boolean clamped) {
  register int x;
  int size=(width*height)>>1;

  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (x=0; x<size; x++) {
    // subsample two u pixels
    uyvy->u0=avg_chroma(u[0],u[1]);
    u+=2;
    uyvy->y0=*(y++);
    // subsample 2 v pixels
    uyvy->v0=avg_chroma(v[0],v[1]);
    v+=2;
    uyvy->y1=*(y++);
    uyvy++;
  }
}


static void convert_yuv_planar_to_yuyv_frame(uint8_t **src, int width, int height, yuyv_macropixel *yuyv, boolean clamped) {
  register int x;
  int hsize=(width*height)>>1;

  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (x=0; x<hsize; x++) {
    yuyv->y0=*(y++);
    yuyv->u0=avg_chroma(u[0],u[1]);
    u+=2;
    yuyv->y1=*(y++);
    yuyv->v0=avg_chroma(v[0],v[1]);
    v+=2;
    yuyv++;
  }
}


static void convert_combineplanes_frame(uint8_t **src, int width, int height, uint8_t *dest, boolean in_alpha,
                                        boolean out_alpha) {
  // turn 3 or 4 planes into packed pixels, src and dest can have alpha

  // e.g yuv444(4)p to yuv888(8)

  int size=width*height;

  uint8_t *y=src[0];
  uint8_t *u=src[1];
  uint8_t *v=src[2];
  uint8_t *a=NULL;

  register int x;

  if (in_alpha) a=src[3];

  for (x=0; x<size; x++) {
    *(dest++)=*(y++);
    *(dest++)=*(u++);
    *(dest++)=*(v++);
    if (out_alpha) {
      if (in_alpha) *(dest++)=*(a++);
      else *(dest++)=255;
    }
  }
}


static void convert_yuvap_to_yuvp_frame(uint8_t **src, int width, int height, uint8_t **dest) {
  size_t size=width*height;

  uint8_t *ys=src[0];
  uint8_t *us=src[1];
  uint8_t *vs=src[2];

  uint8_t *yd=dest[0];
  uint8_t *ud=dest[1];
  uint8_t *vd=dest[2];

  if (yd!=ys) lives_memcpy(yd,ys,size);
  if (ud!=us) lives_memcpy(ud,us,size);
  if (vd!=vs) lives_memcpy(vd,vs,size);

}


static void convert_yuvp_to_yuvap_frame(uint8_t **src, int width, int height, uint8_t **dest) {
  convert_yuvap_to_yuvp_frame(src,width,height,dest);
  memset(dest[3],255,width*height);
}


static void convert_yuvp_to_yuv420_frame(uint8_t **src, int width, int height, uint8_t **dest, boolean clamped) {
  // halve the chroma samples vertically and horizontally, with sub-sampling

  // convert 444p to 420p

  // TODO - handle different output sampling types

  // y-plane should be copied before entering here

  register int i,j;
  uint8_t *d_u,*d_v,*s_u=src[1],*s_v=src[2];
  register short x_u,x_v;
  boolean chroma=FALSE;

  int hwidth=width>>1;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (dest[0]!=src[0]) lives_memcpy(dest[0],src[0],width*height);

  d_u=dest[1];
  d_v=dest[2];

  for (i=0; i<height; i++) {
    for (j=0; j<hwidth; j++) {

      if (!chroma) {
        // pass 1, copy row
        // average two dest pixels
        d_u[j]=avg_chroma(s_u[j*2],s_u[j*2+1]);
        d_v[j]=avg_chroma(s_v[j*2],s_v[j*2+1]);
      } else {
        // pass 2
        // average two dest pixels
        x_u=avg_chroma(s_u[j*2],s_u[j*2+1]);
        x_v=avg_chroma(s_v[j*2],s_v[j*2+1]);
        // average two dest rows
        d_u[j]=avg_chroma(d_u[j],x_u);
        d_v[j]=avg_chroma(d_v[j],x_v);
      }
    }
    if (chroma) {
      d_u+=hwidth;
      d_v+=hwidth;
    }
    chroma=!chroma;
    s_u+=width;
    s_v+=width;
  }
}


static void convert_yuvp_to_yuv411_frame(uint8_t **src, int width, int height, yuv411_macropixel *yuv, boolean clamped) {
  // quarter the chroma samples horizontally, with sub-sampling

  // convert 444p to 411 packed
  // TODO - handle different output sampling types

  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  register short x_u,x_v;

  int widtha=(width>>1)<<1; // cut rightmost odd bytes
  int cbytes=width-widtha;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height; i++) {
    for (j=0; j<widtha; j+=4) {
      // average four dest pixels
      yuv->u2=avg_chroma(s_u[0],s_u[1]);
      x_u=avg_chroma(s_u[2],s_u[3]);
      yuv->u2=avg_chroma(yuv->u2,x_u);

      s_u+=4;

      yuv->y0=*(s_y++);
      yuv->y1=*(s_y++);

      yuv->v2=avg_chroma(s_v[0],s_v[1]);
      x_v=avg_chroma(s_v[2],s_v[3]);
      yuv->v2=avg_chroma(yuv->v2,x_v);

      s_v+=4;

      yuv->y2=*(s_y++);
      yuv->y3=*(s_y++);
    }
    s_u+=cbytes;
    s_v+=cbytes;
  }
}


static void convert_uyvy_to_yuvp_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t **dest, boolean add_alpha) {
  // TODO - avg_chroma

  int size=width*height;
  register int x;

  uint8_t *y=dest[0];
  uint8_t *u=dest[1];
  uint8_t *v=dest[2];

  for (x=0; x<size; x++) {
    *(u++)=uyvy->u0;
    *(u++)=uyvy->u0;
    *(y++)=uyvy->y0;
    *(v++)=uyvy->v0;
    *(v++)=uyvy->v0;
    *(y++)=uyvy->y1;
    uyvy++;
  }

  if (add_alpha) memset(dest[3],255,size*2);
}


static void convert_yuyv_to_yuvp_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t **dest, boolean add_alpha) {
  // TODO - subsampling

  int size=width*height;
  register int x;

  uint8_t *y=dest[0];
  uint8_t *u=dest[1];
  uint8_t *v=dest[2];

  for (x=0; x<size; x++) {
    *(y++)=yuyv->y0;
    *(u++)=yuyv->u0;
    *(u++)=yuyv->u0;
    *(y++)=yuyv->y1;
    *(v++)=yuyv->v0;
    *(v++)=yuyv->v0;
    yuyv++;
  }

  if (add_alpha) memset(dest[3],255,size*2);
}


static void convert_uyvy_to_yuv888_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t *yuv, boolean add_alpha) {
  int size=width*height;
  register int x;

  // double chroma horizontally, no subsampling
  // no subsampling : TODO

  for (x=0; x<size; x++) {
    *(yuv++)=uyvy->y0;
    *(yuv++)=uyvy->u0;
    *(yuv++)=uyvy->v0;
    if (add_alpha) *(yuv++)=255;
    *(yuv++)=uyvy->y1;
    *(yuv++)=uyvy->u0;
    *(yuv++)=uyvy->v0;
    if (add_alpha) *(yuv++)=255;
    uyvy++;
  }
}


static void convert_yuyv_to_yuv888_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t *yuv, boolean add_alpha) {
  int size=width*height;
  register int x;

  // no subsampling : TODO

  for (x=0; x<size; x++) {
    *(yuv++)=yuyv->y0;
    *(yuv++)=yuyv->u0;
    *(yuv++)=yuyv->v0;
    if (add_alpha) *(yuv++)=255;
    *(yuv++)=yuyv->y1;
    *(yuv++)=yuyv->u0;
    *(yuv++)=yuyv->v0;
    if (add_alpha) *(yuv++)=255;
    yuyv++;
  }
}


static void convert_uyvy_to_yuv420_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t **yuv, boolean clamped) {
  // subsample vertically

  // TODO - handle different sampling types

  register int j;

  uint8_t *y=yuv[0];
  uint8_t *u=yuv[1];
  uint8_t *v=yuv[2];

  boolean chroma=TRUE;

  uint8_t *end=y+width*height*2;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (y<end) {
    for (j=0; j<width; j++) {
      if (chroma) *(u++)=uyvy->u0;
      else {
        *u=avg_chroma(*u,uyvy->u0);
        u++;
      }
      *(y++)=uyvy->y0;
      if (chroma) *(v++)=uyvy->v0;
      else {
        *v=avg_chroma(*v,uyvy->v0);
        v++;
      }
      *(y++)=uyvy->y1;
      uyvy++;
    }
    if (chroma) {
      u-=width;
      v-=width;
    }
    chroma=!chroma;
  }
}


static void convert_yuyv_to_yuv420_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t **yuv, boolean clamped) {
  // subsample vertically

  // TODO - handle different sampling types

  register int j;

  uint8_t *y=yuv[0];
  uint8_t *u=yuv[1];
  uint8_t *v=yuv[2];

  boolean chroma=TRUE;

  uint8_t *end=y+width*height*2;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (y<end) {
    for (j=0; j<width; j++) {
      *(y++)=yuyv->y0;
      if (chroma) *(u++)=yuyv->u0;
      else {
        *u=avg_chroma(*u,yuyv->u0);
        u++;
      }
      *(y++)=yuyv->y1;
      if (chroma) *(v++)=yuyv->v0;
      else {
        *v=avg_chroma(*v,yuyv->v0);
        v++;
      }
      yuyv++;
    }
    if (chroma) {
      u-=width;
      v-=width;
    }
    chroma=!chroma;
  }
}


static void convert_uyvy_to_yuv411_frame(uyvy_macropixel *uyvy, int width, int height, yuv411_macropixel *yuv,
    boolean clamped) {
  // subsample chroma horizontally

  uyvy_macropixel *end=uyvy+width*height;
  register int x;

  int widtha=(width<<1)>>1;
  size_t cbytes=width-widtha;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (; uyvy<end; uyvy+=cbytes) {
    for (x=0; x<widtha; x+=2) {
      yuv->u2=avg_chroma(uyvy[0].u0,uyvy[1].u0);

      yuv->y0=uyvy[0].y0;
      yuv->y1=uyvy[0].y1;

      yuv->v2=avg_chroma(uyvy[0].v0,uyvy[1].v0);

      yuv->y2=uyvy[1].y0;
      yuv->y3=uyvy[1].y1;

      uyvy+=2;
      yuv++;
    }
  }
}


static void convert_yuyv_to_yuv411_frame(yuyv_macropixel *yuyv, int width, int height, yuv411_macropixel *yuv,
    boolean clamped) {
  // subsample chroma horizontally

  // TODO - handle different sampling types

  yuyv_macropixel *end=yuyv+width*height;
  register int x;

  int widtha=(width<<1)>>1;
  size_t cybtes=width-widtha;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (; yuyv<end; yuyv+=cybtes) {
    for (x=0; x<widtha; x+=2) {
      yuv->u2=avg_chroma(yuyv[0].u0,yuyv[1].u0);

      yuv->y0=yuyv[0].y0;
      yuv->y1=yuyv[0].y1;

      yuv->v2=avg_chroma(yuyv[0].v0,yuyv[1].v0);

      yuv->y2=yuyv[1].y0;
      yuv->y3=yuyv[1].y1;

      yuyv+=2;
      yuv++;
    }
  }
}

static void convert_yuv888_to_yuv420_frame(uint8_t *yuv8, int width, int height, int irowstride,
    uint8_t **yuv4, boolean src_alpha, boolean clamped) {
  // subsample vertically and horizontally

  //

  // yuv888(8) packed to 420p

  // TODO - handle different sampling types

  // TESTED !

  register int j;
  register short x_u,x_v;

  uint8_t *d_y,*d_u,*d_v,*end;

  boolean chroma=TRUE;

  size_t hwidth=width>>1,ipsize=3,ipsize2;
  int widthx;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (src_alpha) ipsize=4;

  d_y=yuv4[0];
  d_u=yuv4[1];
  d_v=yuv4[2];

  end=d_y+width*height;
  ipsize2=ipsize*2;
  widthx=width*ipsize;

  while (d_y<end) {
    for (j=0; j<widthx; j+=ipsize2) {
      *(d_y++)=yuv8[j];
      *(d_y++)=yuv8[j+ipsize];
      if (chroma) {
        *(d_u++)=avg_chroma(yuv8[j+1],yuv8[j+1+ipsize]);
        *(d_v++)=avg_chroma(yuv8[j+2],yuv8[j+2+ipsize]);
      } else {
        x_u=avg_chroma(yuv8[j+1],yuv8[j+1+ipsize]);
        *d_u=avg_chroma(*d_u,x_u);
        d_u++;
        x_v=avg_chroma(yuv8[j+2],yuv8[j+2+ipsize]);
        *d_v=avg_chroma(*d_v,x_v);
        d_v++;
      }
    }
    if (chroma) {
      d_u-=hwidth;
      d_v-=hwidth;
    }
    chroma=!chroma;
    yuv8+=irowstride;
  }
}


static void convert_uyvy_to_yuv422_frame(uyvy_macropixel *uyvy, int width, int height, uint8_t **yuv) {
  int size=width*height; // y is twice this, u and v are equal

  uint8_t *y=yuv[0];
  uint8_t *u=yuv[1];
  uint8_t *v=yuv[2];

  register int x;

  for (x=0; x<size; x++) {
    uyvy_2_yuv422(uyvy,y,u,v,y+1);
    y+=2;
    u++;
    v++;
  }
}

static void convert_yuyv_to_yuv422_frame(yuyv_macropixel *yuyv, int width, int height, uint8_t **yuv) {
  int size=width*height; // y is twice this, u and v are equal

  uint8_t *y=yuv[0];
  uint8_t *u=yuv[1];
  uint8_t *v=yuv[2];

  register int x;

  for (x=0; x<size; x++) {
    yuyv_2_yuv422(yuyv,y,u,v,y+1);
    y+=2;
    u++;
    v++;
  }
}


static void convert_yuv888_to_yuv422_frame(uint8_t *yuv8, int width, int height, int irowstride,
    uint8_t **yuv4, boolean has_alpha, boolean clamped) {

  // 888(8) packed to 422p

  // TODO - handle different sampling types

  int size=width*height; // y is equal this, u and v are half, chroma subsampled horizontally

  uint8_t *y=yuv4[0];
  uint8_t *u=yuv4[1];
  uint8_t *v=yuv4[2];

  register int x,i,j;

  int offs=0;
  size_t ipsize;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs=1;

  ipsize=(3+offs)<<1;

  if ((irowstride<<1)==width*ipsize) {
    for (x=0; x<size; x+=2) {
      *(y++)=yuv8[0];
      *(y++)=yuv8[3+offs];
      *(u++)=avg_chroma(yuv8[1],yuv8[4+offs]);
      *(v++)=avg_chroma(yuv8[2],yuv8[5+offs]);
      yuv8+=ipsize;
    }
  } else {
    width>>=1;
    irowstride-=width*ipsize;
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        *(y++)=yuv8[0];
        *(y++)=yuv8[3+offs];
        *(u++)=avg_chroma(yuv8[1],yuv8[4+offs]);
        *(v++)=avg_chroma(yuv8[2],yuv8[5+offs]);
        yuv8+=ipsize;
      }
      yuv8+=irowstride;
    }
  }

}


static void convert_yuv888_to_uyvy_frame(uint8_t *yuv, int width, int height, int irowstride,
    uyvy_macropixel *uyvy, boolean has_alpha, boolean clamped) {
  int size=width*height;

  register int x,i,j;

  int offs=0;
  size_t ipsize;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs=1;

  ipsize=(3+offs)<<1;

  if ((irowstride<<1) == width*ipsize) {
    for (x=0; x<size; x+=2) {
      uyvy->u0=avg_chroma(yuv[1],yuv[4+offs]);
      uyvy->y0=yuv[0];
      uyvy->v0=avg_chroma(yuv[2],yuv[5+offs]);
      uyvy->y1=yuv[3+offs];
      yuv+=ipsize;
      uyvy++;
    }
  } else {
    width>>=1;
    irowstride-=width*ipsize;
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        uyvy->u0=avg_chroma(yuv[1],yuv[4+offs]);
        uyvy->y0=yuv[0];
        uyvy->v0=avg_chroma(yuv[2],yuv[5+offs]);
        uyvy->y1=yuv[3+offs];
        yuv+=ipsize;
        uyvy++;
      }
      yuv+=irowstride;
    }
  }


}


static void convert_yuv888_to_yuyv_frame(uint8_t *yuv, int width, int height, int irowstride,
    yuyv_macropixel *yuyv, boolean has_alpha, boolean clamped) {
  int size=width*height;

  register int x,i,j;

  int offs=0;
  size_t ipsize;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (has_alpha) offs=1;

  ipsize=(3+offs)<<1;

  if (irowstride<<1 == width*ipsize) {
    for (x=0; x<size; x+=2) {
      yuyv->y0=yuv[0];
      yuyv->u0=avg_chroma(yuv[1],yuv[4+offs]);
      yuyv->y1=yuv[3+offs];
      yuyv->v0=avg_chroma(yuv[2],yuv[5+offs]);
      yuv+=ipsize;
      yuyv++;
    }
  } else {
    width>>=1;
    irowstride-=width*ipsize;
    for (i=0; i<height; i++) {
      for (j=0; j<width; j++) {
        yuyv->y0=yuv[0];
        yuyv->u0=avg_chroma(yuv[1],yuv[4+offs]);
        yuyv->y1=yuv[3+offs];
        yuyv->v0=avg_chroma(yuv[2],yuv[5+offs]);
        yuv+=ipsize;
        yuyv++;
      }
      yuv+=irowstride;
    }
  }
}

static void convert_yuv888_to_yuv411_frame(uint8_t *yuv8, int width, int height, int irowstride,
    yuv411_macropixel *yuv411, boolean has_alpha) {
  // yuv 888(8) packed to yuv411. Chroma pixels are averaged.

  // TODO - handle different sampling types

  uint8_t *end=yuv8+width*height;
  register int x;
  size_t ipsize=3;
  int widtha=(width>>1)<<1; // cut rightmost odd bytes
  int cbytes=width-widtha;

  if (has_alpha) ipsize=4;

  irowstride-=widtha*ipsize;

  for (; yuv8<end; yuv8+=cbytes) {
    for (x=0; x<widtha; x+=4) { // process 4 input pixels for one output macropixel
      yuv411->u2=(yuv8[1]+yuv8[ipsize+1]+yuv8[2*ipsize+1]+yuv8[3*ipsize+1])>>2;
      yuv411->y0=yuv8[0];
      yuv411->y1=yuv8[ipsize];
      yuv411->v2=(yuv8[2]+yuv8[ipsize+2]+yuv8[2*ipsize+2]+yuv8[3*ipsize+2])>>2;
      yuv411->y2=yuv8[ipsize*2];
      yuv411->y3=yuv8[ipsize*3];

      yuv411++;
      yuv8+=ipsize*4;
    }
    yuv8+=irowstride;
  }
}


static void convert_yuv411_to_rgb_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
                                        uint8_t *dest, boolean add_alpha, boolean clamped) {
  uyvy_macropixel uyvy;
  int m=3,n=4,o=5;
  uint8_t u,v,h_u,h_v,q_u,q_v,y0,y1;
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  size_t psize=3,psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    m=4;
    n=5;
    o=6;
    psize=4;
  }

  orowstride-=width*4*psize;
  psize2=psize<<1;

  while (yuv411<end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3]=dest[7]=255;

    uyvy.y0=yuv411[0].y0;
    uyvy.y1=yuv411[0].y1;
    uyvy.u0=yuv411[0].u2;
    uyvy.v0=yuv411[0].v2;
    uyvy2rgb(&uyvy,&dest[0],&(dest[1]),&dest[2],&dest[m],&dest[n],&dest[o]);
    dest+=psize2;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;
      y1=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j-1].u2);
      q_v=avg_chroma(h_v,yuv411[j-1].v2);

      // average again to get 1/8, 3/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[0],&dest[1],&dest[2]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[m],&dest[n],&dest[o]);

      dest+=psize2;

      // set first 2 RGB pixels of this block

      y0=yuv411[j].y0;
      y1=yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j].u2);
      q_v=avg_chroma(h_v,yuv411[j].v2);

      // average again to get 5/8, 7/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[0],&dest[1],&dest[2]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[m],&dest[n],&dest[o]);

      if (add_alpha) dest[3]=dest[7]=255;
      dest+=psize2;

    }
    // write last 2 pixels

    if (add_alpha) dest[3]=dest[7]=255;

    uyvy.y0=yuv411[j-1].y2;
    uyvy.y1=yuv411[j-1].y3;
    uyvy.u0=yuv411[j-1].u2;
    uyvy.v0=yuv411[j-1].v2;
    uyvy2rgb(&uyvy,&dest[0],&(dest[1]),&dest[2],&dest[m],&dest[n],&dest[o]);

    dest+=psize2+orowstride;
    yuv411+=width;
  }
}


static void convert_yuv411_to_bgr_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
                                        uint8_t *dest, boolean add_alpha, boolean clamped) {
  uyvy_macropixel uyvy;
  int m=3,n=4,o=5;
  uint8_t u,v,h_u,h_v,q_u,q_v,y0,y1;
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  size_t psize=3,psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) {
    m=4;
    n=5;
    o=6;
    psize=4;
  }

  orowstride-=width*4*psize;

  psize2=psize<<1;

  while (yuv411<end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3]=dest[7]=255;

    uyvy.y0=yuv411[0].y0;
    uyvy.y1=yuv411[0].y1;
    uyvy.u0=yuv411[0].u2;
    uyvy.v0=yuv411[0].v2;
    uyvy2rgb(&uyvy,&dest[0],&(dest[1]),&dest[2],&dest[o],&dest[n],&dest[m]);
    dest+=psize2;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;
      y1=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j-1].u2);
      q_v=avg_chroma(h_v,yuv411[j-1].v2);

      // average again to get 1/8, 3/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[0],&dest[1],&dest[2]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[o],&dest[n],&dest[m]);

      dest+=psize2;

      // set first 2 RGB pixels of this block

      y0=yuv411[j].y0;
      y1=yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j].u2);
      q_v=avg_chroma(h_v,yuv411[j].v2);

      // average again to get 5/8, 7/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[0],&dest[1],&dest[2]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[o],&dest[n],&dest[m]);

      if (add_alpha) dest[3]=dest[7]=255;
      dest+=psize2;

    }
    // write last 2 pixels

    if (add_alpha) dest[3]=dest[7]=255;

    uyvy.y0=yuv411[j-1].y2;
    uyvy.y1=yuv411[j-1].y3;
    uyvy.u0=yuv411[j-1].u2;
    uyvy.v0=yuv411[j-1].v2;
    uyvy2rgb(&uyvy,&dest[0],&(dest[1]),&dest[2],&dest[m],&dest[n],&dest[o]);

    dest+=psize2+orowstride;
    yuv411+=width;
  }
}


static void convert_yuv411_to_argb_frame(yuv411_macropixel *yuv411, int width, int height, int orowstride,
    uint8_t *dest, boolean clamped) {
  uyvy_macropixel uyvy;
  uint8_t u,v,h_u,h_v,q_u,q_v,y0,y1;
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  size_t psize=4,psize2;

  if (LIVES_UNLIKELY(!conv_YR_inited)) init_YUV_to_RGB_tables();

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  orowstride-=width*4*psize;
  psize2=psize<<1;

  while (yuv411<end) {
    // write 2 ARGB pixels
    dest[0]=dest[4]=255;

    uyvy.y0=yuv411[0].y0;
    uyvy.y1=yuv411[0].y1;
    uyvy.u0=yuv411[0].u2;
    uyvy.v0=yuv411[0].v2;
    uyvy2rgb(&uyvy,&dest[1],&(dest[2]),&dest[3],&dest[5],&dest[6],&dest[7]);
    dest+=psize2;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 4 argb pixels

      // average first 2 ARGB pixels of this block and last 2 ARGB pixels of previous block

      y0=yuv411[j-1].y2;
      y1=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j-1].u2);
      q_v=avg_chroma(h_v,yuv411[j-1].v2);

      // average again to get 1/8, 3/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[1],&dest[2],&dest[3]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[5],&dest[6],&dest[7]);

      dest+=psize2;

      // set first 2 ARGB pixels of this block

      y0=yuv411[j].y0;
      y1=yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j].u2);
      q_v=avg_chroma(h_v,yuv411[j].v2);

      // average again to get 5/8, 7/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      yuv2rgb(y0,u,v,&dest[1],&dest[2],&dest[3]);

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      yuv2rgb(y1,u,v,&dest[5],&dest[6],&dest[7]);

      dest[0]=dest[4]=255;
      dest+=psize2;

    }
    // write last 2 pixels

    dest[0]=dest[4]=255;

    uyvy.y0=yuv411[j-1].y2;
    uyvy.y1=yuv411[j-1].y3;
    uyvy.u0=yuv411[j-1].u2;
    uyvy.v0=yuv411[j-1].v2;
    uyvy2rgb(&uyvy,&dest[1],&(dest[2]),&dest[3],&dest[5],&dest[6],&dest[7]);

    dest+=psize2+orowstride;
    yuv411+=width;
  }
}


static void convert_yuv411_to_yuv888_frame(yuv411_macropixel *yuv411, int width, int height,
    uint8_t *dest, boolean add_alpha, boolean clamped) {
  size_t psize=3;
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t u,v,h_u,h_v,q_u,q_v,y0,y1;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) psize=4;

  while (yuv411<end) {
    // write 2 RGB pixels
    if (add_alpha) dest[3]=dest[7]=255;

    // write first 2 pixels
    dest[0]=yuv411[0].y0;
    dest[1]=yuv411[0].u2;
    dest[2]=yuv411[0].v2;
    dest+=psize;

    dest[0]=yuv411[0].y1;
    dest[1]=yuv411[0].u2;
    dest[2]=yuv411[0].v2;
    dest+=psize;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;
      y1=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j-1].u2);
      q_v=avg_chroma(h_v,yuv411[j-1].v2);

      // average again to get 1/8, 3/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      dest[0]=y0;
      dest[1]=u;
      dest[2]=v;
      if (add_alpha) dest[3]=255;

      dest+=psize;

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      dest[0]=y1;
      dest[1]=u;
      dest[2]=v;
      if (add_alpha) dest[3]=255;

      dest+=psize;

      // set first 2 RGB pixels of this block

      y0=yuv411[j].y0;
      y1=yuv411[j].y1;

      // avg to get 3/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j].u2);
      q_v=avg_chroma(h_v,yuv411[j].v2);

      // average again to get 5/8, 7/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      dest[0]=y0;
      dest[1]=u;
      dest[2]=v;

      if (add_alpha) dest[3]=255;
      dest+=psize;

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      dest[0]=y1;
      dest[1]=u;
      dest[2]=v;

      if (add_alpha) dest[3]=255;
      dest+=psize;

    }
    // write last 2 pixels

    if (add_alpha) dest[3]=dest[7]=255;

    dest[0]=yuv411[j-1].y2;
    dest[1]=yuv411[j-1].u2;
    dest[2]=yuv411[j-1].v2;
    dest+=psize;

    dest[0]=yuv411[j-1].y3;
    dest[1]=yuv411[j-1].u2;
    dest[2]=yuv411[j-1].v2;

    dest+=psize;
    yuv411+=width;
  }

}


static void convert_yuv411_to_yuvp_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest,
    boolean add_alpha, boolean clamped) {
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t u,v,h_u,h_v,q_u,q_v,y0;

  uint8_t *d_y=dest[0];
  uint8_t *d_u=dest[1];
  uint8_t *d_v=dest[2];
  uint8_t *d_a=dest[3];

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411<end) {
    // write first 2 pixels
    *(d_y++)=yuv411[0].y0;
    *(d_u++)=yuv411[0].u2;
    *(d_v++)=yuv411[0].v2;
    if (add_alpha) *(d_a++)=255;

    *(d_y++)=yuv411[0].y0;
    *(d_u++)=yuv411[0].u2;
    *(d_v++)=yuv411[0].v2;
    if (add_alpha) *(d_a++)=255;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 4 rgb(a) pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j-1].u2);
      q_v=avg_chroma(h_v,yuv411[j-1].v2);

      // average again to get 1/8, 3/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      *(d_y++)=y0;
      *(d_u++)=u;
      *(d_v++)=v;
      if (add_alpha) *(d_a++)=255;

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      *(d_y++)=y0;
      *(d_u++)=u;
      *(d_v++)=v;
      if (add_alpha) *(d_a++)=255;

      // set first 2 RGB pixels of this block

      y0=yuv411[j].y0;

      // avg to get 3/4, 1/2

      q_u=avg_chroma(h_u,yuv411[j].u2);
      q_v=avg_chroma(h_v,yuv411[j].v2);

      // average again to get 5/8, 7/8

      u=avg_chroma(q_u,yuv411[j-1].u2);
      v=avg_chroma(q_v,yuv411[j-1].v2);

      *(d_y++)=y0;
      *(d_u++)=u;
      *(d_v++)=v;
      if (add_alpha) *(d_a++)=255;

      u=avg_chroma(q_u,yuv411[j].u2);
      v=avg_chroma(q_v,yuv411[j].v2);

      *(d_y++)=y0;
      *(d_u++)=u;
      *(d_v++)=v;
      if (add_alpha) *(d_a++)=255;

    }
    // write last 2 pixels
    *(d_y++)=yuv411[j-1].y2;
    *(d_u++)=yuv411[j-1].u2;
    *(d_v++)=yuv411[j-1].v2;
    if (add_alpha) *(d_a++)=255;

    *(d_y++)=yuv411[j-1].y3;
    *(d_u++)=yuv411[j-1].u2;
    *(d_v++)=yuv411[j-1].v2;
    if (add_alpha) *(d_a++)=255;

    yuv411+=width;
  }

}


static void convert_yuv411_to_uyvy_frame(yuv411_macropixel *yuv411, int width, int height,
    uyvy_macropixel *uyvy, boolean clamped) {
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t u,v,h_u,h_v,y0;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411<end) {
    // write first uyvy pixel
    uyvy->u0=yuv411->u2;
    uyvy->y0=yuv411->y0;
    uyvy->v0=yuv411->v2;
    uyvy->y1=yuv411->y1;

    uyvy++;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 2 uyvy macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u=avg_chroma(h_u,yuv411[j-1].u2);
      v=avg_chroma(h_v,yuv411[j-1].v2);

      uyvy->u0=u;
      uyvy->y0=y0;
      uyvy->v0=v;
      uyvy->y1=y0;

      uyvy++;

      // average last pixel again to get 3/4

      u=avg_chroma(h_u,yuv411[j].u2);
      v=avg_chroma(h_v,yuv411[j].v2);

      // set first uyvy macropixel of this block

      y0=yuv411[j].y0;

      uyvy->u0=u;
      uyvy->y0=y0;
      uyvy->v0=v;
      uyvy->y1=y0;

      uyvy++;
    }
    // write last uyvy macro pixel
    uyvy->u0=yuv411[j-1].u2;
    uyvy->y0=yuv411[j-1].y2;
    uyvy->v0=yuv411[j-1].v2;
    uyvy->y1=yuv411[j-1].y3;

    uyvy++;

    yuv411+=width;
  }

}


static void convert_yuv411_to_yuyv_frame(yuv411_macropixel *yuv411, int width, int height, yuyv_macropixel *yuyv,
    boolean clamped) {
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t u,v,h_u,h_v,y0;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411<end) {
    // write first yuyv pixel
    yuyv->y0=yuv411->y0;
    yuyv->u0=yuv411->u2;
    yuyv->y1=yuv411->y1;
    yuyv->v0=yuv411->v2;

    yuyv++;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      y0=yuv411[j-1].y2;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u=avg_chroma(h_u,yuv411[j-1].u2);
      v=avg_chroma(h_v,yuv411[j-1].v2);

      yuyv->y0=y0;
      yuyv->u0=u;
      yuyv->y1=y0;
      yuyv->v0=v;

      yuyv++;

      // average last pixel again to get 3/4

      u=avg_chroma(h_u,yuv411[j].u2);
      v=avg_chroma(h_v,yuv411[j].v2);

      // set first yuyv macropixel of this block

      y0=yuv411[j].y0;

      yuyv->y0=y0;
      yuyv->u0=u;
      yuyv->y1=y0;
      yuyv->v0=v;

      yuyv++;
    }
    // write last yuyv macro pixel
    yuyv->y0=yuv411[j-1].y2;
    yuyv->u0=yuv411[j-1].u2;
    yuyv->y1=yuv411[j-1].y3;
    yuyv->v0=yuv411[j-1].v2;

    yuyv++;

    yuv411+=width;
  }

}


static void convert_yuv411_to_yuv422_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest,
    boolean clamped) {
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t h_u,h_v;

  uint8_t *d_y=dest[0];
  uint8_t *d_u=dest[1];
  uint8_t *d_v=dest[2];

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411<end) {
    // write first 2 y and 1 uv pixel
    *(d_y++)=yuv411->y0;
    *(d_y++)=yuv411->y1;
    *(d_u++)=yuv411->u2;
    *(d_v++)=yuv411->v2;

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      *(d_y++)=yuv411[j-1].y2;
      *(d_y++)=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      *(d_u++)=avg_chroma(h_u,yuv411[j-1].u2);
      *(d_v++)=avg_chroma(h_v,yuv411[j-1].v2);

      // average first pixel to get 3/4

      *(d_y++)=yuv411[j].y0;
      *(d_y++)=yuv411[j].y1;

      *(d_u++)=avg_chroma(h_u,yuv411[j].u2);
      *(d_v++)=avg_chroma(h_v,yuv411[j].v2);

    }
    // write last pixels
    *(d_y++)=yuv411[j-1].y2;
    *(d_y++)=yuv411[j-1].y3;
    *(d_u++)=yuv411[j-1].u2;
    *(d_v++)=yuv411[j-1].v2;

    yuv411+=width;
  }

}


static void convert_yuv411_to_yuv420_frame(yuv411_macropixel *yuv411, int width, int height, uint8_t **dest,
    boolean is_yvu, boolean clamped) {
  register int j;
  yuv411_macropixel *end=yuv411+width*height;
  uint8_t h_u,h_v,u,v;

  uint8_t *d_y=dest[0];
  uint8_t *d_u;
  uint8_t *d_v;

  boolean chroma=FALSE;

  size_t width2=width<<1;

  if (!is_yvu) {
    d_u=dest[1];
    d_v=dest[2];
  } else {
    d_u=dest[2];
    d_v=dest[1];
  }

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  while (yuv411<end) {
    // write first 2 y and 1 uv pixel
    *(d_y++)=yuv411->y0;
    *(d_y++)=yuv411->y1;

    u=yuv411->u2;
    v=yuv411->v2;

    if (!chroma) {
      *(d_u++)=u;
      *(d_v++)=v;
    } else {
      *d_u=avg_chroma(*d_u,u);
      *d_v=avg_chroma(*d_v,v);
    }

    for (j=1; j<width; j++) {
      // convert 6 yuv411 bytes to 2 yuyv macro pixels

      // average first 2 RGB pixels of this block and last 2 RGB pixels of previous block

      *(d_y++)=yuv411[j-1].y2;
      *(d_y++)=yuv411[j-1].y3;

      h_u=avg_chroma(yuv411[j-1].u2,yuv411[j].u2);
      h_v=avg_chroma(yuv411[j-1].v2,yuv411[j].v2);

      // now we have 1/2, 1/2

      // average last pixel again to get 1/4

      u=avg_chroma(h_u,yuv411[j-1].u2);
      v=avg_chroma(h_v,yuv411[j-1].v2);

      if (!chroma) {
        *(d_u++)=u;
        *(d_v++)=v;
      } else {
        *d_u=avg_chroma(*d_u,u);
        *d_v=avg_chroma(*d_v,v);
      }

      // average first pixel to get 3/4

      *(d_y++)=yuv411[j].y0;
      *(d_y++)=yuv411[j].y1;

      u=avg_chroma(h_u,yuv411[j].u2);
      v=avg_chroma(h_v,yuv411[j].v2);

      if (!chroma) {
        *(d_u++)=u;
        *(d_v++)=v;
      } else {
        *d_u=avg_chroma(*d_u,u);
        *d_v=avg_chroma(*d_v,v);
      }

    }

    // write last pixels
    *(d_y++)=yuv411[j-1].y2;
    *(d_y++)=yuv411[j-1].y3;

    u=yuv411[j-1].u2;
    v=yuv411[j-1].v2;

    if (!chroma) {
      *(d_u++)=u;
      *(d_v++)=v;

      d_u-=width2;
      d_v-=width2;

    } else {
      *d_u=avg_chroma(*d_u,u);
      *d_v=avg_chroma(*d_v,v);
    }

    chroma=!chroma;
    yuv411+=width;
  }

}


static void convert_yuv420_to_yuv411_frame(uint8_t **src, int hsize, int vsize, yuv411_macropixel *dest,
    boolean is_422, boolean clamped) {
  // TODO -handle various sampling types

  register int i=0,j;
  uint8_t *y,*u,*v,*end;
  boolean chroma=TRUE;

  size_t qwidth,hwidth;

  // TODO - handle different in sampling types
  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  y=src[0];
  u=src[1];
  v=src[2];

  end=y+hsize*vsize;

  hwidth=hsize>>1;
  qwidth=hwidth>>1;

  while (y<end) {
    for (j=0; j<qwidth; j++) {
      dest->u2=avg_chroma(u[0],u[1]);
      dest->y0=y[0];
      dest->y1=y[1];
      dest->v2=avg_chroma(v[0],v[1]);
      dest->y2=y[2];
      dest->y3=y[3];

      if (!is_422&&chroma&&i>0) {
        dest[-qwidth].u2=avg_chroma(dest[-qwidth].u2,dest->u2);
        dest[-qwidth].v2=avg_chroma(dest[-qwidth].v2,dest->v2);
      }
      dest++;
      y+=4;
      u+=2;
      v+=2;
    }
    chroma=!chroma;
    if (!chroma&&!is_422) {
      u-=hwidth;
      v-=hwidth;
    }
    i++;
  }
}



static void convert_splitplanes_frame(uint8_t *src, int width, int height, int irowstride,
                                      uint8_t **dest, boolean src_alpha, boolean dest_alpha) {

  // convert 888(8) packed to 444(4)P planar
  size_t size=width*height;
  int ipsize=3;

  uint8_t *y=dest[0];
  uint8_t *u=dest[1];
  uint8_t *v=dest[2];
  uint8_t *a=dest[3];

  uint8_t *end;

  register int i,j;

  if (src_alpha) ipsize=4;
  if (irowstride==ipsize*width) {

    for (end=src+size*ipsize; src<end;) {
      *(y++)=*(src++);
      *(u++)=*(src++);
      *(v++)=*(src++);
      if (dest_alpha) {
        if (src_alpha) *(a++)=*(src++);
        else *(a++)=255;
      }
    }
  } else {
    width*=ipsize;
    irowstride-=width;
    for (i=0; i<height; i++) {
      for (j=0; j<width; j+=ipsize) {
        *(y++)=*(src++);
        *(u++)=*(src++);
        *(v++)=*(src++);
        if (dest_alpha) {
          if (src_alpha) *(a++)=*(src++);
          else *(a++)=255;
        }
      }

      src+=irowstride;
    }
  }

}



/////////////////////////////////////////////////////////////////
// RGB palette conversions

static void convert_swap3_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                uint8_t *dest, int thread_id) {
  // swap 3 byte palette
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*3)&&(orowstride==irowstride)) {
    // quick version
#ifdef ENABLE_OIL
    oil_rgb2bgr(dest,src,width*height);
#else
    for (; src<end; src+=3) {
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
    }
#endif
  } else {
    int width3=width*3;
    orowstride-=width3;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width3; i+=3) {
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap3_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                      ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}



static void convert_swap4_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                uint8_t *dest, int thread_id) {
  // swap 4 byte palette
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if ((height-dheight*i)<dheight) dheight=height-(dheight*i);
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap4_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap4_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  if ((irowstride==width*4)&&(orowstride==irowstride)) {
    // quick version
    for (; src<end; src+=4) {
      *(dest++)=src[3]; // alpha
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
    }
  } else {
    int width4=width*4;
    orowstride-=width4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        *(dest++)=src[i+3]; // alpha
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap4_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap4_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                      ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_swap3addpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                       uint8_t *dest, int thread_id) {
  // swap 3 bytes, add post alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3addpost_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3addpost_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*3)&&(orowstride==width*4)) {
    // quick version
    for (; src<end; src+=3) {
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
      *(dest++)=255; // alpha
    }
  } else {
    int width3=width*3;
    orowstride-=width*4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width3; i+=3) {
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
        *(dest++)=255; // alpha
      }
      dest+=orowstride;
    }
  }
}



void *convert_swap3addpost_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3addpost_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                             ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}



static void convert_swap3addpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, int thread_id) {
  // swap 3 bytes, add pre alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3addpre_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3addpre_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*3)&&(orowstride==width*4)) {
    // quick version
    for (; src<end; src+=3) {
      *(dest++)=255; // alpha
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
    }
  } else {
    int width3=width*3;
    orowstride-=width*4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width3; i+=3) {
        *(dest++)=255; // alpha
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap3addpre_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3addpre_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_swap3postalpha_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
    uint8_t *dest, int thread_id) {
  // swap 3 bytes, leave alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3postalpha_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3postalpha_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  if ((irowstride==width*4)&&(orowstride==irowstride)) {
    // quick version
    for (; src<end; src+=4) {
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
      *(dest++)=src[3]; // alpha
    }
  } else {
    int width4=width*4;
    orowstride-=width4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
        *(dest++)=src[i+3]; // alpha
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap3postalpha_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3postalpha_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                               ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_addpost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                  uint8_t *dest, int thread_id) {
  // add post alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_addpost_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_addpost_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*3)&&(orowstride==width*4)) {
    // quick version
#ifdef ENABLE_OIL
    oil_rgb2rgba(dest,src,width*height);
#else
    for (; src<end; src+=3) {
      *(dest++)=src[0]; // r
      *(dest++)=src[1]; // g
      *(dest++)=src[2]; // b
      *(dest++)=255; // alpha
    }
#endif
  } else {
    int width3=width*3;
    orowstride-=width*4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width3; i+=3) {
        *(dest++)=src[i]; // r
        *(dest++)=src[i+1]; // g
        *(dest++)=src[i+2]; // b
        *(dest++)=255; // alpha
      }
      dest+=orowstride;
    }
  }
}



void *convert_addpost_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_addpost_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                        ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_addpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                 uint8_t *dest, int thread_id) {
  // add pre alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_addpre_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_addpre_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  if ((irowstride==width*3)&&(orowstride==width*4)) {
    // quick version
    for (; src<end; src+=3) {
      *(dest++)=255; // alpha
      *(dest++)=src[0]; // r
      *(dest++)=src[1]; // g
      *(dest++)=src[2]; // b
    }
  } else {
    int width3=width*3;
    orowstride-=width*4;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width3; i+=3) {
        *(dest++)=255; // alpha
        *(dest++)=src[i]; // r
        *(dest++)=src[i+1]; // g
        *(dest++)=src[i+2]; // b
      }
      dest+=orowstride;
    }
  }
}


void *convert_addpre_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_addpre_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                       ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}

static void convert_swap3delpost_frame(uint8_t *src,int width,int height, int irowstride, int orowstride,
                                       uint8_t *dest, int thread_id) {
  // swap 3 bytes, delete post alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3delpost_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3delpost_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*4)&&(orowstride==width*3)) {
    // quick version
    for (; src<end; src+=4) {
      *(dest++)=src[2]; // red
      *(dest++)=src[1]; // green
      *(dest++)=src[0]; // blue
    }
  } else {
    int width4=width*4;
    orowstride-=width*3;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        *(dest++)=src[i+2]; // red
        *(dest++)=src[i+1]; // green
        *(dest++)=src[i]; // blue
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap3delpost_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3delpost_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                             ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_delpost_frame(uint8_t *src,int width,int height, int irowstride, int orowstride,
                                  uint8_t *dest, int thread_id) {
  // delete post alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_delpost_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_delpost_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  if ((irowstride==width*4)&&(orowstride==width*3)) {
    // quick version
    for (; src<end; src+=4) {
      lives_memcpy(dest,src,3);
      dest+=3;
    }
  } else {
    int width4=width*4;
    orowstride-=width*3;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        lives_memcpy(dest,&src[i],3);
        dest+=3;
      }
      dest+=orowstride;
    }
  }
}


void *convert_delpost_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_delpost_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                        ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}



static void convert_delpre_frame(uint8_t *src,int width,int height, int irowstride, int orowstride,
                                 uint8_t *dest, int thread_id) {
  // delete pre alpha
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_delpre_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_delpre_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  src++;

  if ((irowstride==width*4)&&(orowstride==width*3)) {
    // quick version
    for (; src<end; src+=4) {
      lives_memcpy(dest,src,3);
      dest+=3;
    }
  } else {
    int width4=width*4;
    orowstride-=width*3;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        lives_memcpy(dest,&src[i],3);
        dest+=3;
      }
      dest+=orowstride;
    }
  }
}



void *convert_delpre_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_delpre_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                       ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_swap3delpre_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, int thread_id) {
  // delete pre alpha, swap last 3
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;
        ccparams[i].thread_id=i;

        if (useme) convert_swap3delpre_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swap3delpre_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*4)&&(orowstride==width*3)) {
    // quick version
    for (; src<end; src+=4) {
      *(dest++)=src[3]; // red
      *(dest++)=src[2]; // green
      *(dest++)=src[1]; // blue
    }
  } else {
    int width4=width*4;
    orowstride-=width*3;
    for (; src<end; src+=irowstride) {
      for (i=0; i<width4; i+=4) {
        *(dest++)=src[i+3]; // red
        *(dest++)=src[i+2]; // green
        *(dest++)=src[i+1]; // blue
      }
      dest+=orowstride;
    }
  }
}


void *convert_swap3delpre_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swap3delpre_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}



static void convert_swapprepost_frame(uint8_t *src, int width, int height, int irowstride, int orowstride,
                                      uint8_t *dest, boolean alpha_first, int thread_id) {
  // swap first and last bytes in a 4 byte palette
  uint8_t *end=src+height*irowstride;
  register int i;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*irowstride)<end) {

        ccparams[i].src=src+dheight*i*irowstride;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*orowstride;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].irowstrides[0]=irowstride;
        ccparams[i].orowstrides[0]=orowstride;

        ccparams[i].alpha_first=alpha_first;

        ccparams[i].thread_id=i;

        if (useme) convert_swapprepost_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swapprepost_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }

  if ((irowstride==width*4)&&(orowstride==irowstride)) {
    // quick version
    if (alpha_first) {
      for (; src<end; src+=4) {
        *(dest++)=src[1];
        *(dest++)=src[2];
        *(dest++)=src[3];
        *(dest++)=src[0];
      }
    } else {
      for (; src<end; src+=4) {
        *(dest++)=src[3];
        *(dest++)=src[0];
        *(dest++)=src[1];
        *(dest++)=src[2];
      }
    }
  } else {
    int width4=width*4;
    orowstride-=width4;
    for (; src<end; src+=irowstride) {
      if (alpha_first) {
        for (i=0; i<width4; i+=4) {
          *(dest++)=src[i+1];
          *(dest++)=src[i+2];
          *(dest++)=src[i+3];
          *(dest++)=src[i];
        }
      } else {
        for (i=0; i<width4; i+=4) {
          *(dest++)=src[i+3];
          *(dest++)=src[i];
          *(dest++)=src[i+1];
          *(dest++)=src[i+2];
        }
      }
      dest+=orowstride;
    }
  }
}


void *convert_swapprepost_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swapprepost_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,ccparams->irowstrides[0],
                            ccparams->orowstrides[0],(uint8_t *)ccparams->dest,ccparams->alpha_first,ccparams->thread_id);
  return NULL;
}

//////////////////////////
// genric YUV

static void convert_swab_frame(uint8_t *src, int width, int height, uint8_t *dest, int thread_id) {
  register int i;
  int width4=width*4;
  uint8_t *end=src+height*width4;

  if (thread_id==-1&&prefs->nfx_threads>1) {
    int nthreads=0;
    int dheight;
    boolean useme=FALSE;
    lives_cc_params *ccparams=(lives_cc_params *)lives_malloc(prefs->nfx_threads*sizeof(lives_cc_params));

    dheight=CEIL((double)height/(double)prefs->nfx_threads,4);
    for (i=0; i<prefs->nfx_threads; i++) {
      if ((src+dheight*i*width4)<end) {

        ccparams[i].src=src+dheight*i*width4;
        ccparams[i].hsize=width;
        ccparams[i].dest=dest+dheight*i*width4;

        if (dheight*(i+1)>(height-4)) {
          dheight=height-(dheight*i);
          useme=TRUE;
        }
        if (dheight<4) break;

        ccparams[i].vsize=dheight;

        ccparams[i].thread_id=i;

        if (useme) convert_swab_frame_thread(&ccparams[i]);
        else {
          pthread_create(&cthreads[i],NULL,convert_swab_frame_thread,&ccparams[i]);
          nthreads++;
        }
      }
    }

    for (i=0; i<nthreads; i++) {
      pthread_join(cthreads[i],NULL);
    }
    lives_free(ccparams);
    return;
  }


  for (; src<end; src+=width4) {
    for (i=0; i<width4; i+=4) {
      swab(&src[i],&dest[i],4);
    }
    dest+=width4;
  }
}


void *convert_swab_frame_thread(void *data) {
  lives_cc_params *ccparams=(lives_cc_params *)data;
  convert_swab_frame((uint8_t *)ccparams->src,ccparams->hsize,ccparams->vsize,
                     (uint8_t *)ccparams->dest,ccparams->thread_id);
  return NULL;
}


static void convert_halve_chroma(uint8_t **src, int width, int height, uint8_t **dest, boolean clamped) {
  // width and height here are width and height of src *chroma* planes, in bytes

  // halve the chroma samples vertically, with sub-sampling, e.g. 422p to 420p

  // TODO : handle different sampling methods in and out

  register int i,j;
  uint8_t *d_u=dest[1],*d_v=dest[2],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height; i++) {
    for (j=0; j<width; j++) {

      if (!chroma) {
        // pass 1, copy row
        lives_memcpy(d_u,s_u,width);
        lives_memcpy(d_v,s_v,width);
      } else {
        // pass 2
        // average two dest rows
        d_u[j]=avg_chroma(d_u[j],s_u[j]);
        d_v[j]=avg_chroma(d_v[j],s_v[j]);
      }
    }
    if (chroma) {
      d_u+=width;
      d_v+=width;
    }
    chroma=!chroma;
    s_u+=width;
    s_v+=width;
  }
}


static void convert_double_chroma(uint8_t **src, int width, int height, uint8_t **dest, boolean clamped) {
  // width and height here are width and height of src *chroma* planes, in bytes

  // double two chroma planes vertically, with interpolation: eg: 420p to 422p

  // TODO - handle different sampling methods in and out

  register int i,j;
  uint8_t *d_u=dest[1],*d_v=dest[2],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int height2=height<<1;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  for (i=0; i<height2; i++) {
    for (j=0; j<width; j++) {

      lives_memcpy(d_u,s_u,width);
      lives_memcpy(d_v,s_v,width);

      if (!chroma&&i>0) {
        // pass 2
        // average two src rows
        d_u[j-width]=avg_chroma(d_u[j-width],s_u[j]);
        d_v[j-width]=avg_chroma(d_v[j-width],s_v[j]);
      }
    }
    if (chroma) {
      s_u+=width;
      s_v+=width;
    }
    chroma=!chroma;
    d_u+=width;
    d_v+=width;
  }
}


static void convert_quad_chroma(uint8_t **src, int width, int height, uint8_t **dest, boolean add_alpha, boolean clamped) {
  // width and height here are width and height of dest chroma planes, in bytes

  // double the chroma samples vertically and horizontally, with interpolation, eg. 420p to 444p

  // output to planes

  //TODO: handle mpeg and dvpal input

  // TESTED !

  register int i,j;
  uint8_t *d_u=dest[1],*d_v=dest[2],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int height2;
  int width2;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  height>>=1;
  width>>=1;

  height2=height<<1;
  width2=width<<1;

  // for this algorithm, we assume chroma samples are aligned like mpeg

  for (i=0; i<height2; i++) {
    d_u[0]=d_u[1]=s_u[0];
    d_v[0]=d_v[1]=s_v[0];
    for (j=2; j<width2; j+=2) {
      d_u[j+1]=d_u[j]=s_u[(j>>1)];
      d_v[j+1]=d_v[j]=s_v[(j>>1)];

      d_u[j-1]=avg_chroma(d_u[j-1],d_u[j]);
      d_v[j-1]=avg_chroma(d_v[j-1],d_v[j]);

      if (!chroma&&i>0) {
        // pass 2
        // average two src rows (e.g 2 with 1, 4 with 3, ... etc) for odd dst rows
        // thus dst row 1 becomes average of src chroma rows 0 and 1, etc.)
        d_u[j-width2]=avg_chroma(d_u[j-width2],d_u[j]);
        d_v[j-width2]=avg_chroma(d_v[j-width2],d_v[j]);
        d_u[j-1-width2]=avg_chroma(d_u[j-1-width2],d_u[j-1]);
        d_v[j-1-width2]=avg_chroma(d_v[j-1-width2],d_v[j-1]);
      }

    }
    if (!chroma&&i>0) {
      d_u[j-1-width2]=avg_chroma(d_u[j-1-width2],d_u[j-1]);
      d_v[j-1-width2]=avg_chroma(d_v[j-1-width2],d_v[j-1]);
    }
    if (chroma) {
      s_u+=width;
      s_v+=width;
    }
    chroma=!chroma;
    d_u+=width2;
    d_v+=width2;
  }

  if (add_alpha) memset(dest+((width*height)<<3),255,((width*height)<<3));

}


static void convert_quad_chroma_packed(uint8_t **src, int width, int height, uint8_t *dest, boolean add_alpha,
                                       boolean clamped) {
  // width and height here are width and height of dest chroma planes, in bytes
  // stretch (double) the chroma samples vertically and horizontally, with interpolation

  // ouput to packed pixels

  // e.g: 420p to 888(8)

  //TODO: handle mpeg and dvpal input

  // TESTED !

  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  boolean chroma=FALSE;
  int widthx,hwidth=width>>1;
  int opsize=3,opsize2;

  //int count;
  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) opsize=4;

  widthx=width*opsize;
  opsize2=opsize*2;

  for (i=0; i<height; i++) {
    dest[0]=*(s_y++);
    dest[1]=dest[opsize+1]=s_u[0];
    dest[2]=dest[opsize+2]=s_v[0];
    if (add_alpha) dest[3]=dest[opsize+3]=255;
    dest[opsize]=*(s_y++);

    //count=10;

    for (j=opsize2; j<widthx; j+=opsize2) {
      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width

      // implements jpeg style subsampling : TODO - mpeg and dvpal style

      dest[j]=*(s_y++);
      dest[j+opsize]=*(s_y++);
      if (add_alpha) dest[j+3]=dest[j+7]=255;
      dest[j+1]=dest[j+opsize+1]=s_u[(j/opsize)/2];
      dest[j+2]=dest[j+opsize+2]=s_v[(j/opsize)/2];

      dest[j-opsize+1]=avg_chroma(dest[j-opsize+1],dest[j+1]);
      dest[j-opsize+2]=avg_chroma(dest[j-opsize+2],dest[j+2]);
      if (!chroma&&i>0) {
        // pass 2
        // average two src rows
        dest[j+1-widthx]=avg_chroma(dest[j+1-widthx],dest[j+1]);
        dest[j+2-widthx]=avg_chroma(dest[j+2-widthx],dest[j+2]);
        dest[j-opsize+1-widthx]=avg_chroma(dest[j-opsize+1-widthx],dest[j-opsize+1]);
        dest[j-opsize+2-widthx]=avg_chroma(dest[j-opsize+2-widthx],dest[j-opsize+2]);
      }

    }
    if (!chroma&&i>0) {
      dest[j-opsize+1-widthx]=avg_chroma(dest[j-opsize+1-widthx],dest[j-opsize+1]);
      dest[j-opsize+2-widthx]=avg_chroma(dest[j-opsize+2-widthx],dest[j-opsize+2]);
    }
    if (chroma) {
      s_u+=hwidth;
      s_v+=hwidth;
    }
    chroma=!chroma;
    dest+=widthx;
  }
}

static void convert_double_chroma_packed(uint8_t **src, int width, int height, uint8_t *dest, boolean add_alpha,
    boolean clamped) {
  // width and height here are width and height of dest chroma planes, in bytes
  // double the chroma samples horizontally, with interpolation

  // output to packed pixels

  // e.g 422p to 888(8)

  //TODO: handle non-dvntsc in

  register int i,j;
  uint8_t *s_y=src[0],*s_u=src[1],*s_v=src[2];
  int widthx;
  int opsize=3,opsize2;

  set_conversion_arrays(clamped?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,WEED_YUV_SUBSPACE_YCBCR);

  if (add_alpha) opsize=4;

  widthx=width*opsize;
  opsize2=opsize*2;

  for (i=0; i<height; i++) {
    dest[0]=*(s_y++);
    dest[1]=dest[opsize+1]=s_u[0];
    dest[2]=dest[opsize+2]=s_v[0];
    if (add_alpha) dest[3]=dest[opsize+3]=255;
    dest[opsize]=*(s_y++);
    for (j=opsize2; j<widthx; j+=opsize2) {
      // dvntsc style - chroma is aligned with luma

      // process two pixels at a time, and we average the first colour pixel with the last from the previous 2
      // we know we can do this because Y must be even width
      dest[j]=*(s_y++);
      dest[j+opsize]=*(s_y++);
      if (add_alpha) dest[j+opsize-1]=dest[j+opsize2-1]=255;

      dest[j+1]=dest[j+opsize+1]=s_u[(j/opsize)>>1];
      dest[j+2]=dest[j+opsize+2]=s_v[(j/opsize)>>1];

      dest[j-opsize+1]=avg_chroma(dest[j-opsize+1],dest[j+1]);
      dest[j-opsize+2]=avg_chroma(dest[j-opsize+2],dest[j+2]);
    }
    s_u+=width;
    s_v+=width;
    dest+=widthx;
  }
}



static void switch_yuv_sampling(weed_plant_t *layer) {
  int error;
  int sampling=weed_get_int_value(layer,"YUV_sampling",&error);
  int clamping=weed_get_int_value(layer,"YUV_clamping",&error);
  int subspace=weed_get_int_value(layer,"YUV_subspace",&error);
  int palette=weed_get_int_value(layer,"current_palette",&error);
  int width=(weed_get_int_value(layer,"width",&error)>>1);
  int height=(weed_get_int_value(layer,"height",&error)>>1);
  unsigned char **pixel_data,*dst;

  register int i,j,k;

  if (palette!=WEED_PALETTE_YUV420P) return;

  pixel_data=(unsigned char **)weed_get_voidptr_array(layer,"pixel_data",&error);

  set_conversion_arrays(clamping,subspace);

  if (sampling==WEED_YUV_SAMPLING_MPEG) {
    // jpeg is located centrally between Y, mpeg(2) and some flv are located on the left Y
    // so here we just set dst[0]=avg(src[0],src[1]), dst[1]=avg(src[1],src[2]), etc.
    // the last value is repeated once

    width--;
    for (k=1; k<3; k++) {
      dst=pixel_data[k];
      for (j=0; j<height; j++) {
        for (i=0; i<width; i++) dst[i]=avg_chroma(dst[i],dst[i+1]);
        dst[i]=dst[i-1];
        dst+=width+1;
      }
    }
    weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_JPEG);
  } else if (sampling==WEED_YUV_SAMPLING_JPEG) {
    for (k=1; k<3; k++) {
      dst=pixel_data[k];
      for (j=0; j<height; j++) {
        for (i=width-1; i>0; i--) dst[i]=avg_chroma(dst[i],dst[i-1]);
        dst[0]=dst[1];
        dst+=width;
      }
    }
    weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_MPEG);
  }
  lives_free(pixel_data);
}



static void switch_yuv_clamping_and_subspace(weed_plant_t *layer, int oclamping, int osubspace) {
  // currently subspace conversions are not performed - TODO
  // we assume subspace Y'CbCr
  int error;
  int iclamping=weed_get_int_value(layer,"YUV_clamping",&error);
  int isubspace=weed_get_int_value(layer,"YUV_subspace",&error);

  int palette=weed_get_int_value(layer,"current_palette",&error);
  int width=weed_get_int_value(layer,"width",&error);
  int height=weed_get_int_value(layer,"height",&error);

  void **pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  uint8_t *src,*src1,*src2,*end;

  get_YUV_to_YUV_conversion_arrays(iclamping,isubspace,oclamping,osubspace);

  switch (palette) {
  case WEED_PALETTE_YUVA8888:
    src=(uint8_t *)pixel_data[0];
    end=src+width*height*4;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=U_to_U[*src];
      src++;
      *src=V_to_V[*src];
      src+=2;
    }
    break;
  case WEED_PALETTE_YUV888:
    src=(uint8_t *)pixel_data[0];
    end=src+width*height*3;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=U_to_U[*src];
      src++;
      *src=V_to_V[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUVA4444P:
  case WEED_PALETTE_YUV444P:
    src=(uint8_t *)pixel_data[0];
    src1=(uint8_t *)pixel_data[1];
    src2=(uint8_t *)pixel_data[2];
    end=src+width*height;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src1=U_to_U[*src1];
      src1++;
      *src2=V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_UYVY:
    src=(uint8_t *)pixel_data[0];
    end=src+width*height*4;
    while (src<end) {
      *src=U_to_U[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=V_to_V[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUYV:
    src=(uint8_t *)pixel_data[0];
    end=src+width*height*4;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=U_to_U[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=V_to_V[*src];
      src++;
    }
    break;
  case WEED_PALETTE_YUV422P:
    src=(uint8_t *)pixel_data[0];
    src1=(uint8_t *)pixel_data[1];
    src2=(uint8_t *)pixel_data[2];
    end=src+width*height;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src1=U_to_U[*src1];
      src1++;
      *src2=V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YVU420P:
    src=(uint8_t *)pixel_data[0];
    src1=(uint8_t *)pixel_data[2];
    src2=(uint8_t *)pixel_data[1];
    end=src+width*height;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src1=U_to_U[*src1];
      src1++;
      *src2=V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YUV420P:
    src=(uint8_t *)pixel_data[0];
    src1=(uint8_t *)pixel_data[1];
    src2=(uint8_t *)pixel_data[2];
    end=src+width*height;
    while (src<end) {
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src1=U_to_U[*src1];
      src1++;
      *src2=V_to_V[*src2];
      src2++;
    }
    break;
  case WEED_PALETTE_YUV411:
    src=(uint8_t *)pixel_data[0];
    end=src+width*height*6;
    while (src<end) {
      *src=U_to_U[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=V_to_V[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
      *src=Y_to_Y[*src];
      src++;
    }
    break;
  }
  weed_set_int_value(layer,"YUV_clamping",oclamping);
  lives_free(pixel_data);

}



////////////////////////////////////////////////////////////////////////////////////////


void create_empty_pixel_data(weed_plant_t *layer, boolean black_fill, boolean may_contig) {
  // width, height are src size in (macro) pixels
  // warning, width and height may be adjusted

  // if black_fill is set, fill with opaque black in whatever palette

  // may_contig should normally be set to TRUE, except for special uses during palette conversion

  int error;
  int palette=weed_get_int_value(layer,"current_palette",&error);
  int width=weed_get_int_value(layer,"width",&error);
  int height=weed_get_int_value(layer,"height",&error);
  int rowstride,*rowstrides;

  int y_black=16;
  int clamping=WEED_YUV_CLAMPING_CLAMPED;

  uint8_t *pixel_data;
  uint8_t *memblock;
  uint8_t **pd_array;

  unsigned char black[6]= {0,0,0,255,255,255};
  float blackf[4]= {0.,0.,0.,1.};

  unsigned char *ptr;

  size_t framesize;

  register int i,j;

  weed_set_voidptr_value(layer,"pixel_data", NULL);
  if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous"))
    weed_leaf_delete(layer,"host_pixel_data_contiguous");

  if (black_fill) {
    if (weed_plant_has_leaf(layer,"YUV_clamping")) clamping=weed_get_int_value(layer,"YUV_clamping",&error);
    if (clamping!=WEED_YUV_CLAMPING_CLAMPED) y_black=0;
  }

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*3,mainw->rowstride_alignment);
#else
    rowstride=width*3;
#endif
    framesize=CEIL(rowstride*height,32)+64;

    pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_YUV888:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*3,mainw->rowstride_alignment);
#else
    rowstride=width*3;
#endif
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      black[0]=y_black;
      black[1]=black[2]=128;
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,3);
          ptr+=3;
        }
      }
    }
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_UYVY8888:
    framesize=CEIL(width*height*4,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      black[1]=black[3]=y_black;
      black[0]=black[2]=128;
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    rowstride=width*4;
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_YUYV8888:
    framesize=CEIL(width*height*4,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      black[0]=black[2]=y_black;
      black[1]=black[3]=128;
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    rowstride=width*4;
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_RGBA32:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*4,mainw->rowstride_alignment);
#else
    rowstride=width*4;
#endif
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_BGRA32:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*4,mainw->rowstride_alignment);
#else
    rowstride=width*4;
#endif
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_ARGB32:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*4,mainw->rowstride_alignment);
#else
    rowstride=width*4;
#endif
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      black[0]=255;
      black[3]=0;
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_YUVA8888:
#ifdef USE_SWSCALE
    rowstride=CEIL(width*4,mainw->rowstride_alignment);
#else
    rowstride=width*4;
#endif
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) {
      pixel_data=(uint8_t *)lives_calloc(framesize>>4,16);
      if (pixel_data==NULL) return;
    } else {
      black[0]=y_black;
      black[1]=black[2]=128;
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,4);
          ptr+=4;
        }
      }
    }
    weed_set_int_value(layer,"rowstrides",rowstride);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    width=(width>>1)<<1;
    weed_set_int_value(layer,"width",width);
    height=(height>>1)<<1;
    weed_set_int_value(layer,"height",height);
    rowstrides=(int *)lives_malloc(sizint*3);
    rowstrides[0]=width;
    rowstrides[1]=rowstrides[2]=(width>>1);
    weed_set_int_array(layer,"rowstrides",3,rowstrides);
    lives_free(rowstrides);
    pd_array=(uint8_t **)lives_malloc(3*sizeof(uint8_t *));

    framesize=CEIL(width*height,32);


    if (!may_contig) {
      weed_leaf_delete(layer,"host_pixel_data_contiguous");
      if (!black_fill) {
        pd_array[0]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        pd_array[1]=(uint8_t *)lives_calloc(framesize>>4,4);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        pd_array[2]=(uint8_t *)lives_calloc((framesize>>4)+8,4);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
      } else {
        pd_array[0]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=(uint8_t *)lives_try_malloc(framesize>>2);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[1],128,width*height/4);
        pd_array[2]=(uint8_t *)lives_try_malloc((framesize>>2)+64);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[2],128,width*height/4);
      }

    } else {
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);

      if (!black_fill) {
        memblock=(uint8_t *)lives_calloc(((framesize*3)>>3)+8,4);
        if (memblock==NULL) return;
        pd_array[0]=(uint8_t *)memblock;
        pd_array[1]=(uint8_t *)(memblock+framesize);
        pd_array[2]=(uint8_t *)(memblock+((framesize*5)>>2));
      } else {
        memblock=(uint8_t *)lives_try_malloc(framesize*3/2);
        if (memblock==NULL) return;
        pd_array[0]=(uint8_t *)memblock;
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=(uint8_t *)(memblock+framesize);
        memset(pd_array[1],128,width*height/2);
        pd_array[2]=(uint8_t *)(memblock+((framesize*5)>>2));
      }

    }

    weed_set_voidptr_array(layer,"pixel_data",3,(void **)pd_array);
    lives_free(pd_array);
    break;

  case WEED_PALETTE_YUV422P:
    width=(width>>1)<<1;
    weed_set_int_value(layer,"width",width);
    rowstrides=(int *)lives_malloc(sizint*3);
    rowstrides[0]=width;
    rowstrides[1]=rowstrides[2]=width>>1;
    weed_set_int_array(layer,"rowstrides",3,rowstrides);
    lives_free(rowstrides);
    pd_array=(uint8_t **)lives_malloc(3*sizeof(uint8_t *));
    framesize=CEIL(width*height,32);

    if (!may_contig) {
      weed_leaf_delete(layer,"host_pixel_data_contiguous");

      if (!black_fill) {
        pd_array[0]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        pd_array[1]=(uint8_t *)lives_calloc(framesize>>3,4);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        pd_array[2]=(uint8_t *)lives_calloc((framesize>>3)+8,4);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
      } else {
        pd_array[0]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=(uint8_t *)lives_try_malloc(framesize>>1);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[1],128,width*height/2);
        pd_array[2]=(uint8_t *)lives_try_malloc((framesize>>1)+64);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[2],128,width*height/2);
      }


    } else {
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);

      if (!black_fill) {
        memblock=(uint8_t *)lives_calloc((framesize>>1)+8,4);
        if (memblock==NULL) return;
        pd_array[0]=(uint8_t *)memblock;
        pd_array[1]=(uint8_t *)(memblock+framesize);
        pd_array[2]=(uint8_t *)(memblock+((3*framesize)>>1));
      } else {
        memblock=(uint8_t *)lives_try_malloc((framesize*2)+64);
        if (memblock==NULL) return;
        pd_array[0]=(uint8_t *)memblock;
        pd_array[1]=(uint8_t *)(memblock+framesize);
        pd_array[2]=(uint8_t *)(memblock+((3*framesize)>>1));
        memset(pd_array[0],y_black,width*height);
        memset(pd_array[1],128,width*height);
      }

    }
    weed_set_voidptr_array(layer,"pixel_data",3,(void **)pd_array);
    lives_free(pd_array);
    break;


  case WEED_PALETTE_YUV444P:
    rowstrides=(int *)lives_malloc(sizint*3);
    rowstrides[0]=rowstrides[1]=rowstrides[2]=width;
    weed_set_int_array(layer,"rowstrides",3,rowstrides);
    lives_free(rowstrides);
    pd_array=(uint8_t **)lives_malloc(3*sizeof(uint8_t *));
    framesize=CEIL(width*height,32);

    if (!may_contig) {
      weed_leaf_delete(layer,"host_pixel_data_contiguous");

      if (!black_fill) {
        pd_array[0]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        pd_array[1]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        pd_array[2]=(uint8_t *)lives_calloc((framesize>>4)+4,16);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
      } else {
        pd_array[0]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[1],128,width*height);
        pd_array[2]=(uint8_t *)lives_try_malloc(framesize+64);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[2],128,width*height);
      }


    } else {
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);
      if (!black_fill) {
        memblock=(uint8_t *)lives_calloc(((framesize*3)>>4)+4,16);
        if (memblock==NULL) return;
        pd_array[0]=memblock;
        pd_array[1]=memblock+framesize;
        pd_array[2]=memblock+framesize*2;

      } else {
        memblock=(uint8_t *)lives_try_malloc(framesize*3+64);
        if (memblock==NULL) return;
        pd_array[0]=memblock;
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=memblock+framesize;
        pd_array[2]=memblock+framesize*2;
        memset(pd_array[1],128,width*height*2);
      }
    }
    weed_set_voidptr_array(layer,"pixel_data",3,(void **)pd_array);
    lives_free(pd_array);
    break;


  case WEED_PALETTE_YUVA4444P:
    rowstrides=(int *)lives_malloc(sizint*4);
    rowstrides[0]=rowstrides[1]=rowstrides[2]=rowstrides[3]=width;
    weed_set_int_array(layer,"rowstrides",4,rowstrides);
    lives_free(rowstrides);
    pd_array=(uint8_t **)lives_malloc(4*sizeof(uint8_t *));
    framesize=CEIL(width*height,32);

    if (!may_contig) {
      weed_leaf_delete(layer,"host_pixel_data_contiguous");

      if (!black_fill) {
        pd_array[0]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        pd_array[1]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        pd_array[2]=(uint8_t *)lives_calloc(framesize>>4,16);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        pd_array[3]=(uint8_t *)lives_calloc((framesize>>4)+4,16);
        if (pd_array[3]==NULL) {
          lives_free(pd_array[2]);
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
      } else {
        pd_array[0]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[0]==NULL) {
          lives_free(pd_array);
          return;
        }
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[1]==NULL) {
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[1],128,width*height);
        pd_array[2]=(uint8_t *)lives_try_malloc(framesize);
        if (pd_array[2]==NULL) {
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[2],128,width*height);
        pd_array[3]=(uint8_t *)lives_try_malloc(framesize+64);
        if (pd_array[3]==NULL) {
          lives_free(pd_array[2]);
          lives_free(pd_array[1]);
          lives_free(pd_array[0]);
          lives_free(pd_array);
          return;
        }
        memset(pd_array[3],255,width*height);
      }


    } else {
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);

      if (!black_fill) {
        memblock=(uint8_t *)lives_calloc(framesize+16,4);
        if (memblock==NULL) return;
        pd_array[0]=memblock;
        pd_array[1]=memblock+framesize;
        pd_array[2]=memblock+framesize*2;
        pd_array[3]=memblock+framesize*3;
      } else {
        memblock=(uint8_t *)lives_try_malloc(framesize*4+64);
        if (memblock==NULL) return;
        pd_array[0]=memblock;
        memset(pd_array[0],y_black,width*height);
        pd_array[1]=memblock+framesize;
        pd_array[2]=memblock+framesize*2;

        memset(pd_array[1],128,width*height*2);

        pd_array[3]=memblock+framesize*3;

        memset(pd_array[3],255,width*height);
      }
    }
    weed_set_voidptr_array(layer,"pixel_data",4,(void **)pd_array);
    lives_free(pd_array);
    break;


  case WEED_PALETTE_YUV411:
    weed_set_int_value(layer,"width",width);
    rowstride=width*6; // a macro-pixel is 6 bytes, and contains 4 real pixels
    if (!black_fill) pixel_data=(uint8_t *)lives_calloc(width*height*3+32,2);
    else {
      black[0]=black[3]=128;
      black[1]=black[2]=black[4]=black[5]=y_black;
      pixel_data=(uint8_t *)lives_calloc(width*height*3+32,2);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,black,6);
          ptr+=6;
        }
      }
    }
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_RGBFLOAT:
    rowstride=width*3*sizeof(float);
    pixel_data=(uint8_t *)lives_calloc(width*height*3+(64/sizeof(float)),sizeof(float));
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_RGBAFLOAT:
    rowstride=width*4*sizeof(float);
    if (!black_fill) pixel_data=(uint8_t *)lives_calloc(width*height*4+(64/sizeof(float)),sizeof(float));
    else {
      size_t sizf=4*sizeof(float);
      pixel_data=(uint8_t *)lives_try_malloc(width*height*sizf+64);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,blackf,sizf);
          ptr+=sizf;
        }
      }
    }
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_AFLOAT:
    rowstride=width*sizeof(float);
    if (!black_fill) pixel_data=(uint8_t *)lives_calloc(width*height+(64/sizeof(float)),sizeof(float));
    else {
      size_t sizf=sizeof(float);
      pixel_data=(uint8_t *)lives_try_malloc(width*height*sizf+64);
      if (pixel_data==NULL) return;
      ptr=pixel_data;
      blackf[0]=1.;
      for (i=0; i<height; i++) {
        for (j=0; j<width; j++) {
          lives_memcpy(ptr,blackf,sizf);
          ptr+=sizf;
        }
      }
    }
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_A8:
    rowstride=width;
    framesize=CEIL(width*height,32)+64;
    if (!black_fill) pixel_data=(uint8_t *)lives_calloc(framesize,1);
    else {
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      memset(pixel_data,255,width*height);
    }
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  case WEED_PALETTE_A1:
    rowstride=(width+7)>>3;
    framesize=CEIL(rowstride*height,32)+64;
    if (!black_fill) pixel_data=(uint8_t *)lives_calloc(framesize,1);
    else {
      pixel_data=(uint8_t *)lives_try_malloc(framesize);
      if (pixel_data==NULL) return;
      memset(pixel_data,255,rowstride*height);
    }
    if (pixel_data==NULL) return;
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",rowstride);
    break;

  default:
    lives_printerr("Warning: asked to create empty pixel_data for palette %d !\n",palette);
  }
}





void alpha_unpremult(weed_plant_t *layer, boolean un) {
  // un-premultply alpha - this only occurs when going from palette with alpha to one without
  // if un is FALSE we go the other way, and do a premultiplication
  int error;
  int aoffs,coffs,psize,psizel,widthx;
  int alpha;
  int flags=0;
  int width=weed_get_int_value(layer,"width",&error);
  int height=weed_get_int_value(layer,"height",&error);
  int rowstride=weed_get_int_value(layer,"rowstrides",&error);
  int pal=weed_get_int_value(layer,"current_palette",&error);

  int *rows;

  unsigned char *ptr;
  unsigned char **ptrp;

  boolean clamped;

  register int i,j,p;

  if (!unal_inited) init_unal();

  if (weed_plant_has_leaf(layer,"YUV_clamping"))
    clamped=(weed_get_int_value(layer,"YUV_clamping",&error)==WEED_YUV_CLAMPING_CLAMPED);
  else clamped=TRUE;

  switch (pal) {
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    clamped=FALSE;
  case WEED_PALETTE_YUVA8888:
    widthx=width*4;
    psize=4;
    psizel=3;
    coffs=0;
    aoffs=3;
    break;
  case WEED_PALETTE_ARGB32:
    widthx=width*4;
    psize=4;
    psizel=4;
    coffs=1;
    aoffs=0;
    clamped=FALSE;
    break;
  case WEED_PALETTE_YUVA4444P:
    // special case - planar with alpha
    ptrp=(unsigned char **)weed_get_voidptr_array(layer,"pixel_data",&error);
    rows=weed_get_int_array(layer,"rowstrides",&error);

    if (!clamped) {
      if (un) {
        for (i=0; i<height; i++) {
          for (j=0; j<width; j++) {
            alpha=ptrp[3][j];
            for (p=0; p<3; p++) {
              ptrp[p][j]=unal[alpha][ptrp[p][j]];
            }
          }
          for (p=0; p<4; p++) {
            ptrp[p]+=rows[p];
          }
        }
      } else {
        for (i=0; i<height; i++) {
          for (j=0; j<width; j++) {
            alpha=ptrp[3][j];
            for (p=0; p<3; p++) {
              ptrp[p][j]=al[alpha][ptrp[p][j]];
            }
          }
          for (p=0; p<4; p++) {
            ptrp[p]+=rows[p];
          }
        }
      }
    } else {
      if (un) {
        for (i=0; i<height; i++) {
          for (j=0; j<width; j++) {
            alpha=ptrp[3][j];
            ptrp[0][j]=unalcy[alpha][ptrp[0][j]];
            ptrp[1][j]=unalcuv[alpha][ptrp[0][j]];
            ptrp[2][j]=unalcuv[alpha][ptrp[0][j]];
          }
          for (p=0; p<4; p++) {
            ptrp[p]+=rows[p];
          }
        }
      } else {
        for (i=0; i<height; i++) {
          for (j=0; j<width; j++) {
            alpha=ptrp[3][j];
            ptrp[0][j]=alcy[alpha][ptrp[0][j]];
            ptrp[1][j]=alcuv[alpha][ptrp[0][j]];
            ptrp[2][j]=alcuv[alpha][ptrp[0][j]];
          }
          for (p=0; p<4; p++) {
            ptrp[p]+=rows[p];
          }
        }
      }
    }
    return;
  default:
    return;
  }

  ptr=(unsigned char *)weed_get_voidptr_value(layer,"pixel_data",&error);

  if (!clamped) {
    if (un) {
      for (i=0; i<height; i++) {
        for (j=0; j<widthx; j+=psize) {
          alpha=ptr[j+aoffs];
          for (p=coffs; p<psizel; p++) {
            ptr[j+p]=unal[alpha][ptr[j+p]];
          }
        }
        ptr+=rowstride;
      }
    } else {
      for (i=0; i<height; i++) {
        for (j=0; j<widthx; j+=psize) {
          alpha=ptr[j+aoffs];
          for (p=coffs; p<psizel; p++) {
            ptr[j+p]=al[alpha][ptr[j+p]];
          }
        }
        ptr+=rowstride;
      }
    }
  } else {
    // unclamped YUVA8888 (packed)
    if (un) {
      for (i=0; i<height; i++) {
        for (j=0; j<widthx; j+=psize) {
          alpha=ptr[j+3];
          ptr[j]=unalcy[alpha][ptr[j]];
          ptr[j+1]=unalcuv[alpha][ptr[j]];
          ptr[j+2]=unalcuv[alpha][ptr[j]];
        }
        ptr+=rowstride;
      }
    } else {
      for (i=0; i<height; i++) {
        for (j=0; j<widthx; j+=psize) {
          alpha=ptr[j+3];
          ptr[j]=alcy[alpha][ptr[j]];
          ptr[j+1]=alcuv[alpha][ptr[j]];
          ptr[j+2]=alcuv[alpha][ptr[j]];
        }
        ptr+=rowstride;
      }
    }
  }

  if (weed_plant_has_leaf(layer,"flags"))
    flags=weed_get_int_value(layer,"flags",&error);

  if (!un) flags|=WEED_CHANNEL_ALPHA_PREMULT;
  else if (flags&WEED_CHANNEL_ALPHA_PREMULT) flags^=WEED_CHANNEL_ALPHA_PREMULT;

  if (flags==0) weed_leaf_delete(layer,"flags");
  else weed_set_int_value(layer,"flags",flags);
}





boolean convert_layer_palette_full(weed_plant_t *layer, int outpl, int osamtype, boolean oclamping, int osubspace) {
  // here we will not handle RGB float palettes (wait for libabl for that...)
  // but we will convert to/from the 5 other RGB palettes and 10 YUV palettes
  // giving a total of 15*14=210 conversions
  //
  // number coded so far 180
  // ~85% complete

  // some conversions to and from ARGB and to and from yuv411 are missing

  // NOTE - if converting to YUV411, we cut pixels so (RGB) width is divisible by 4
  //        if converting to other YUV, we cut pixels so (RGB) width is divisible by 2
  //        if converting to YUV420 or YVU420, we cut pixels so height is divisible by 2

  //        currently chroma is assumed centred between luma
  // osamtype, subspace and clamping currently only work for default values


  //       TODO: allow plugin candidates/delegates

  // - original palette "pixel_data" is free()d

  // returns FALSE if the palette conversion fails

  // layer must not be NULL


  // rowstrides value MUST be checked for input palettes RGB24, BGR24 and YUV888



  uint8_t *gusrc=NULL,**gusrc_array=NULL,*gudest,**gudest_array,*tmp;
  int width,height,orowstride,irowstride;
  int error,inpl,flags=0;
  int isamtype,isubspace;
  boolean iclamped,contig=FALSE;


  inpl=weed_get_int_value(layer,"current_palette",&error);

  if (weed_plant_has_leaf(layer,"YUV_sampling")) isamtype=weed_get_int_value(layer,"YUV_sampling",&error);
  else isamtype=WEED_YUV_SAMPLING_DEFAULT;

  if (weed_plant_has_leaf(layer,"YUV_clamping"))
    iclamped=(weed_get_int_value(layer,"YUV_clamping",&error)==WEED_YUV_CLAMPING_CLAMPED);
  else iclamped=TRUE;

  if (weed_plant_has_leaf(layer,"YUV_subspace")) isubspace=weed_get_int_value(layer,"YUV_subspace",&error);
  else isubspace=WEED_YUV_SUBSPACE_YUV;

  //#define DEBUG_PCONV
#ifdef DEBUG_PCONV
  char *tmp2,*tmp3;
  g_print("converting palette %s(%s - %d) to %s(%s - %d)\n",weed_palette_get_name(inpl),
          (tmp2=lives_strdup(weed_yuv_clamping_get_name(!iclamped))),!iclamped,weed_palette_get_name(outpl),
          (tmp3=lives_strdup(weed_yuv_clamping_get_name(!oclamping))),!oclamping);
  lives_free(tmp2);
  lives_free(tmp3);
#endif

  if (weed_palette_is_yuv_palette(inpl)&&weed_palette_is_yuv_palette(outpl)&&(iclamped!=oclamping||isubspace!=osubspace)) {
    if (isubspace==osubspace) {
#ifdef DEBUG_PCONV
      lives_printerr("converting clamping %d to %d\n",!iclamped,!oclamping);
#endif
      switch_yuv_clamping_and_subspace(layer,oclamping?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED,osubspace);
      iclamped=oclamping;
    } else {
      // convert first to RGB(A)
      if (weed_palette_has_alpha_channel(inpl)) {
        convert_layer_palette_full(layer, WEED_PALETTE_RGBA32, WEED_YUV_SAMPLING_DEFAULT, FALSE, WEED_YUV_SUBSPACE_YUV);
      } else {
        convert_layer_palette_full(layer, WEED_PALETTE_RGB24, WEED_YUV_SAMPLING_DEFAULT, FALSE, WEED_YUV_SUBSPACE_YUV);
      }
      inpl=weed_get_int_value(layer,"current_palette",&error);
      isubspace=osubspace;
      isamtype=osamtype;
      iclamped=oclamping;
#ifdef DEBUG_PCONV
      g_print("subspace conversion via palette %s\n",weed_palette_get_name(inpl));
#endif
    }
  }


  if (inpl==outpl) {
#ifdef DEBUG_PCONV
    lives_printerr("not converting palette\n");
#endif
    if (!weed_palette_is_yuv_palette(inpl)||(isamtype==osamtype&&
        (isubspace==osubspace||(osubspace!=WEED_YUV_SUBSPACE_BT709)))) return TRUE;
    if (inpl==WEED_PALETTE_YUV420P&&((isamtype==WEED_YUV_SAMPLING_JPEG&&osamtype==WEED_YUV_SAMPLING_MPEG)||
                                     (isamtype==WEED_YUV_SAMPLING_MPEG&&osamtype==WEED_YUV_SAMPLING_JPEG))) {
      switch_yuv_sampling(layer);
    } else {
      char *tmp2=lives_strdup_printf("Switch sampling types (%d %d) or subspace(%d %d): (%d) conversion not yet written !\n",
                                     isamtype,osamtype,isubspace,osubspace,inpl);
      LIVES_DEBUG(tmp2);
      lives_free(tmp2);
      return TRUE;
    }
  }

  if (weed_plant_has_leaf(layer,"flags"))
    flags=weed_get_int_value(layer,"flags",&error);

  if (prefs->alpha_post) {
    if ((flags&WEED_CHANNEL_ALPHA_PREMULT) &&
        (weed_palette_has_alpha_channel(inpl)&&!(weed_palette_has_alpha_channel(outpl)))) {
      // if we have pre-multiplied alpha, remove it when removing alpha channel
      alpha_unpremult(layer,TRUE);
    }
  } else {
    if (!weed_palette_has_alpha_channel(inpl)&&weed_palette_has_alpha_channel(outpl)) {
      flags|=WEED_CHANNEL_ALPHA_PREMULT;
      weed_set_int_value(layer,"flags",flags);
    }
  }

  if (weed_palette_has_alpha_channel(inpl)&&!(weed_palette_has_alpha_channel(outpl))&&(flags&WEED_CHANNEL_ALPHA_PREMULT)) {
    flags^=WEED_CHANNEL_ALPHA_PREMULT;
    if (flags==0) weed_leaf_delete(layer,"flags");
    else weed_set_int_value(layer,"flags",flags);
  }

  if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") &&
      weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
    contig=TRUE;

  width=weed_get_int_value(layer,"width",&error);
  height=weed_get_int_value(layer,"height",&error);

  switch (inpl) {
  case WEED_PALETTE_BGR24:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3addpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_addpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3addpre_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv_frame(gusrc,width,height,irowstride,gudest,FALSE,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv_frame(gusrc,width,height,irowstride,gudest,FALSE,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,
                                  FALSE,WEED_YUV_SAMPLING_DEFAULT,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,FALSE,osamtype,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,FALSE,FALSE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,FALSE,TRUE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,FALSE,oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_RGBA32:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3delpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_delpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3postalpha_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swapprepost_frame(gusrc,width,height,irowstride,orowstride,gudest,FALSE,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv_frame(gusrc,width,height,irowstride,gudest,TRUE,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv_frame(gusrc,width,height,irowstride,gudest,TRUE,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,TRUE,WEED_YUV_SAMPLING_DEFAULT,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,TRUE,osamtype,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,TRUE,FALSE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,TRUE,TRUE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,TRUE,oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_RGB24:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_addpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3addpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_addpre_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv_frame(gusrc,width,height,irowstride,gudest,FALSE,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv_frame(gusrc,width,height,irowstride,gudest,FALSE,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,FALSE,osamtype,oclamping);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,
                                  FALSE,WEED_YUV_SAMPLING_DEFAULT,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,FALSE,FALSE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_rgb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,FALSE,TRUE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_rgb_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,FALSE,oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_BGRA32:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_delpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3delpost_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3postalpha_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap4_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv_frame(gusrc,width,height,irowstride,gudest,TRUE,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv_frame(gusrc,width,height,irowstride,gudest,TRUE,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,TRUE,
                                  WEED_YUV_SAMPLING_DEFAULT,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YVU420P:
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,TRUE,osamtype,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,TRUE,FALSE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_bgr_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,TRUE,TRUE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_bgr_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,TRUE,oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_ARGB32:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap3delpre_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_delpre_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swapprepost_frame(gusrc,width,height,irowstride,orowstride,gudest,TRUE,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swap4_frame(gusrc,width,height,irowstride,orowstride,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_argb_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_argb_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_argb_to_yuv_frame(gusrc,width,height,irowstride,gudest,FALSE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_argb_to_yuv_frame(gusrc,width,height,irowstride,gudest,TRUE,oclamping,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_argb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,FALSE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_argb_to_yuvp_frame(gusrc,width,height,irowstride,gudest_array,TRUE,oclamping,-USE_THREADS);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_argb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,WEED_YUV_SAMPLING_DEFAULT,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_argb_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,osamtype,oclamping);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_argb_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,oclamping);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_YUV444P:
    gusrc_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_halve_chroma(gusrc_array,width,height,gudest_array,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,FALSE,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,FALSE,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_argb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_uyvy_frame(gusrc_array,width,height,(uyvy_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_yuyv_frame(gusrc_array,width,height,(yuyv_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_combineplanes_frame(gusrc_array,width,height,gudest,FALSE,FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_combineplanes_frame(gusrc_array,width,height,gudest,FALSE,TRUE);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuvp_to_yuvap_frame(gusrc_array,width,height,gudest_array);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuvp_to_yuv420_frame(gusrc_array,width,height,gudest_array,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuvp_to_yuv411_frame(gusrc_array,width,height,(yuv411_macropixel *)gudest,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      if (gusrc_array!=NULL) lives_free(gusrc_array);
      return FALSE;
    }
    if (gusrc_array!=NULL) {
      if (gusrc_array[0]!=NULL) lives_free(gusrc_array[0]);
      if (!contig) {
        lives_free(gusrc_array[1]);
        lives_free(gusrc_array[2]);
      }
      lives_free(gusrc_array);
    }
    break;
  case WEED_PALETTE_YUVA4444P:
    gusrc_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_halve_chroma(gusrc_array,width,height,gudest_array,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,TRUE,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,TRUE,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_argb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_uyvy_frame(gusrc_array,width,height,(uyvy_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv_planar_to_yuyv_frame(gusrc_array,width,height,(yuyv_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_combineplanes_frame(gusrc_array,width,height,gudest,TRUE,FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_combineplanes_frame(gusrc_array,width,height,gudest,TRUE,TRUE);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuvap_to_yuvp_frame(gusrc_array,width,height,gudest_array);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuvp_to_yuv420_frame(gusrc_array,width,height,gudest_array,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuvp_to_yuv411_frame(gusrc_array,width,height,(yuv411_macropixel *)gudest,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      if (gusrc_array!=NULL) lives_free(gusrc_array);
      return FALSE;
    }
    if (gusrc_array!=NULL) {
      if (gusrc_array[0]!=NULL) lives_free(gusrc_array[0]);
      if (!contig) {
        lives_free(gusrc_array[1]);
        lives_free(gusrc_array[2]);
        lives_free(gusrc_array[3]);
      }
      lives_free(gusrc_array);
    }
    break;
  case WEED_PALETTE_UYVY8888:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swab_frame(gusrc,width,height,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_uyvy_to_yuv422_frame((uyvy_macropixel *)gusrc,width,height,gudest_array);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_uyvy_to_rgb_frame((uyvy_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_uyvy_to_rgb_frame((uyvy_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_uyvy_to_bgr_frame((uyvy_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_uyvy_to_bgr_frame((uyvy_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_uyvy_to_argb_frame((uyvy_macropixel *)gusrc,width,height,orowstride,gudest,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_uyvy_to_yuvp_frame((uyvy_macropixel *)gusrc,width,height,gudest_array,FALSE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_uyvy_to_yuvp_frame((uyvy_macropixel *)gusrc,width,height,gudest_array,TRUE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_uyvy_to_yuv888_frame((uyvy_macropixel *)gusrc,width,height,gudest,FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_uyvy_to_yuv888_frame((uyvy_macropixel *)gusrc,width,height,gudest,TRUE);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_uyvy_to_yuv420_frame((uyvy_macropixel *)gusrc,width,height,gudest_array,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_uyvy_to_yuv411_frame((uyvy_macropixel *)gusrc,width,height,(yuv411_macropixel *)gudest,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_YUYV8888:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_swab_frame(gusrc,width,height,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuyv_to_yuv422_frame((yuyv_macropixel *)gusrc,width,height,gudest_array);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_yuyv_to_rgb_frame((yuyv_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_yuyv_to_rgb_frame((yuyv_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_yuyv_to_bgr_frame((yuyv_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_yuyv_to_bgr_frame((yuyv_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      convert_yuyv_to_argb_frame((yuyv_macropixel *)gusrc,width,height,orowstride,gudest,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuyv_to_yuvp_frame((yuyv_macropixel *)gusrc,width,height,gudest_array,FALSE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuyv_to_yuvp_frame((yuyv_macropixel *)gusrc,width,height,gudest_array,TRUE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuyv_to_yuv888_frame((yuyv_macropixel *)gusrc,width,height,gudest,FALSE);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuyv_to_yuv888_frame((yuyv_macropixel *)gusrc,width,height,gudest,TRUE);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuyv_to_yuv420_frame((yuyv_macropixel *)gusrc,width,height,gudest_array,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuyv_to_yuv411_frame((yuyv_macropixel *)gusrc,width,height,(yuv411_macropixel *)gudest,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_YUV888:
    // need to check rowstrides (may have been resized)
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_addpost_frame(gusrc,width,height,irowstride,width*4,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_splitplanes_frame(gusrc,width,height,irowstride,gudest_array,FALSE,FALSE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_splitplanes_frame(gusrc,width,height,irowstride,gudest_array,FALSE,TRUE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_rgb_frame(gusrc,width,height,irowstride,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_rgb_frame(gusrc,width,height,irowstride,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_bgr_frame(gusrc,width,height,irowstride,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_bgr_frame(gusrc,width,height,irowstride,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_argb_frame(gusrc,width,height,irowstride,orowstride,gudest,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_YVU420P:
      // convert to YUV420P, then fall through
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv888_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,FALSE,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv888_to_yuv422_frame(gusrc,width,height,irowstride,gudest_array,FALSE,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,FALSE);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_YUVA8888:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    irowstride=weed_get_int_value(layer,"rowstrides",&error);
    switch (outpl) {
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_delpost_frame(gusrc,width,height,irowstride,width*3,gudest,-USE_THREADS);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_splitplanes_frame(gusrc,width,height,irowstride,gudest_array,TRUE,TRUE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_splitplanes_frame(gusrc,width,height,irowstride,gudest_array,TRUE,FALSE);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuva8888_to_rgba_frame(gusrc,width,height,irowstride,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuva8888_to_rgba_frame(gusrc,width,height,irowstride,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuva8888_to_bgra_frame(gusrc,width,height,irowstride,orowstride,gudest,TRUE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuva8888_to_bgra_frame(gusrc,width,height,irowstride,orowstride,gudest,FALSE,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuva8888_to_argb_frame(gusrc,width,height,irowstride,orowstride,gudest,iclamped,-USE_THREADS);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv888_to_yuv420_frame(gusrc,width,height,irowstride,gudest_array,TRUE,iclamped);
      lives_free(gudest_array);
      weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
      //weed_set_int_value(layer,"YUV_sampling",osamtype);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv888_to_yuv422_frame(gusrc,width,height,irowstride,gudest_array,TRUE,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_uyvy_frame(gusrc,width,height,irowstride,(uyvy_macropixel *)gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_yuyv_frame(gusrc,width,height,irowstride,(yuyv_macropixel *)gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv888_to_yuv411_frame(gusrc,width,height,irowstride,(yuv411_macropixel *)gudest,TRUE);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  case WEED_PALETTE_YVU420P:
    // swap u and v planes, then fall through to YUV420P
    gusrc_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    tmp=gusrc_array[1];
    gusrc_array[1]=gusrc_array[2];
    gusrc_array[2]=tmp;
    weed_set_voidptr_array(layer,"pixel_data",3,(void **)gusrc_array);
    lives_free(gusrc_array);
  case WEED_PALETTE_YUV420P:
    gusrc_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,FALSE,isamtype,iclamped);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,FALSE,isamtype,iclamped);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,FALSE,FALSE,isamtype,iclamped);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,TRUE,FALSE,isamtype,iclamped);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_argb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,isamtype,iclamped);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420_to_uyvy_frame(gusrc_array,width,height,(uyvy_macropixel *)gudest);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420_to_yuyv_frame(gusrc_array,width,height,(yuyv_macropixel *)gudest);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_double_chroma(gusrc_array,width>>1,height>>1,gudest_array,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_quad_chroma(gusrc_array,width,height,gudest_array,FALSE,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",4,(void **)gudest_array);
      convert_quad_chroma(gusrc_array,width,height,gudest_array,TRUE,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YVU420P:
      if (inpl==WEED_PALETTE_YUV420P) {
        tmp=gusrc_array[1];
        gusrc_array[1]=gusrc_array[2];
        gusrc_array[2]=tmp;
        weed_set_voidptr_array(layer,"pixel_data",3,(void **)gusrc_array);
        lives_free(gusrc_array);
      }
      // fall through
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer,"current_palette",outpl);
      gusrc_array=NULL;
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_quad_chroma_packed(gusrc_array,width,height,gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_quad_chroma_packed(gusrc_array,width,height,gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420_to_yuv411_frame(gusrc_array,width,height,(yuv411_macropixel *)gudest,FALSE,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      if (gusrc_array!=NULL) lives_free(gusrc_array);
      return FALSE;
    }
    if (gusrc_array!=NULL) {
      if (gusrc_array[0]!=NULL) lives_free(gusrc_array[0]);
      if (!contig) {
        lives_free(gusrc_array[1]);
        lives_free(gusrc_array[2]);
      }
      lives_free(gusrc_array);

    }
    break;
  case WEED_PALETTE_YUV422P:
    gusrc_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,FALSE,TRUE,isamtype,iclamped);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_rgb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,TRUE,isamtype,iclamped);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,FALSE,TRUE,isamtype,iclamped);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_bgr_frame(gusrc_array,width,height,orowstride,gudest,TRUE,TRUE,isamtype,iclamped);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420p_to_argb_frame(gusrc_array,width,height,orowstride,gudest,TRUE,isamtype,iclamped);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv422p_to_uyvy_frame(gusrc_array,width,height,gudest);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv422p_to_yuyv_frame(gusrc_array,width,height,gudest);
      break;
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_halve_chroma(gusrc_array,width>>1,height>>1,gudest_array,iclamped);
      lives_free(gudest_array);
      gusrc_array[0]=NULL;
      weed_set_int_value(layer,"YUV_sampling",isamtype);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",3,(void **)gudest_array);
      convert_double_chroma(gusrc_array,width>>1,height>>1,gudest_array,iclamped);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,FALSE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      lives_free(gudest_array[0]);
      gudest_array[0]=gusrc_array[0];
      weed_set_voidptr_array(layer,"pixel_data",4,(void **)gudest_array);
      convert_double_chroma(gusrc_array,width>>1,height>>1,gudest_array,iclamped);
      memset(gudest_array[3],255,width*height);
      gusrc_array[0]=NULL;
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_double_chroma_packed(gusrc_array,width,height,gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_double_chroma_packed(gusrc_array,width,height,gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_YUV411:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width>>2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv420_to_yuv411_frame(gusrc_array,width,height,(yuv411_macropixel *)gudest,TRUE,iclamped);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      if (gusrc_array!=NULL) lives_free(gusrc_array);
      return FALSE;
    }
    if (gusrc_array!=NULL) {
      if (gusrc_array[0]!=NULL) lives_free(gusrc_array[0]);
      if (!contig) {
        lives_free(gusrc_array[1]);
        lives_free(gusrc_array[2]);
      }
      lives_free(gusrc_array);
    }
    break;
  case WEED_PALETTE_YUV411:
    gusrc=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
    switch (outpl) {
    case WEED_PALETTE_RGB24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_rgb_frame((yuv411_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_RGBA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_rgb_frame((yuv411_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_BGR24:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_bgr_frame((yuv411_macropixel *)gusrc,width,height,orowstride,gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_BGRA32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_bgr_frame((yuv411_macropixel *)gusrc,width,height,orowstride,gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_ARGB32:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      orowstride=weed_get_int_value(layer,"rowstrides",&error);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_argb_frame((yuv411_macropixel *)gusrc,width,height,orowstride,gudest,iclamped);
      break;
    case WEED_PALETTE_YUV888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_yuv888_frame((yuv411_macropixel *)gusrc,width,height,gudest,FALSE,iclamped);
      break;
    case WEED_PALETTE_YUVA8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_yuv888_frame((yuv411_macropixel *)gusrc,width,height,gudest,TRUE,iclamped);
      break;
    case WEED_PALETTE_YUV444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv411_to_yuvp_frame((yuv411_macropixel *)gusrc,width,height,gudest_array,FALSE,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUVA4444P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv411_to_yuvp_frame((yuv411_macropixel *)gusrc,width,height,gudest_array,TRUE,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_UYVY8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_uyvy_frame((yuv411_macropixel *)gusrc,width,height,(uyvy_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUYV8888:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<1);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);
      convert_yuv411_to_yuyv_frame((yuv411_macropixel *)gusrc,width,height,(yuyv_macropixel *)gudest,iclamped);
      break;
    case WEED_PALETTE_YUV422P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv411_to_yuv422_frame((yuv411_macropixel *)gusrc,width,height,gudest_array,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YUV420P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv411_to_yuv420_frame((yuv411_macropixel *)gusrc,width,height,gudest_array,FALSE,iclamped);
      lives_free(gudest_array);
      break;
    case WEED_PALETTE_YVU420P:
      weed_set_int_value(layer,"current_palette",outpl);
      weed_set_int_value(layer,"width",width<<2);
      create_empty_pixel_data(layer,FALSE,TRUE);
      gudest_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
      convert_yuv411_to_yuv420_frame((yuv411_macropixel *)gusrc,width,height,gudest_array,TRUE,iclamped);
      lives_free(gudest_array);
      break;
    default:
      lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                     weed_palette_get_name(outpl));
      return FALSE;
    }
    break;
  default:
    lives_printerr("Invalid palette conversion: %s to %s not written yet !!\n",weed_palette_get_name(inpl),
                   weed_palette_get_name(outpl));
    return FALSE;
  }
  if (gusrc!=NULL) lives_free(gusrc);


  if (weed_palette_is_rgb_palette(outpl)) {
    weed_leaf_delete(layer,"YUV_clamping");
    weed_leaf_delete(layer,"YUV_subspace");
    weed_leaf_delete(layer,"YUV_sampling");
  } else {
    weed_set_int_value(layer,"YUV_clamping",oclamping?WEED_YUV_CLAMPING_CLAMPED:WEED_YUV_CLAMPING_UNCLAMPED);
    if (weed_palette_is_rgb_palette(inpl)) {
      // TODO - bt709
      weed_set_int_value(layer,"YUV_subspace",WEED_YUV_SUBSPACE_YCBCR);
    }
    if (!weed_plant_has_leaf(layer,"YUV_sampling")) weed_set_int_value(layer,"YUV_sampling",WEED_YUV_SAMPLING_DEFAULT);
  }

  if (weed_palette_is_rgb_palette(inpl)&&weed_palette_is_yuv_palette(outpl)) {
    width=((weed_get_int_value(layer,"width",&error)*weed_palette_get_pixels_per_macropixel(outpl))>>1)<<1;
    weed_set_int_value(layer,"width",width/weed_palette_get_pixels_per_macropixel(outpl));
  }

  if ((outpl==WEED_PALETTE_YVU420P&&inpl!=WEED_PALETTE_YVU420P&&inpl!=WEED_PALETTE_YUV420P)) {
    // swap u and v planes
    uint8_t **pd_array=(uint8_t **)weed_get_voidptr_array(layer,"pixel_data",&error);
    uint8_t *tmp=pd_array[1];
    pd_array[1]=pd_array[2];
    pd_array[2]=tmp;
    weed_set_voidptr_array(layer,"pixel_data",3,(void **)pd_array);
    lives_free(pd_array);
  }
  return TRUE;
}


boolean convert_layer_palette(weed_plant_t *layer, int outpl, int op_clamping) {
  return convert_layer_palette_full(layer,outpl,WEED_YUV_SAMPLING_DEFAULT,
                                    op_clamping==WEED_YUV_CLAMPING_CLAMPED,WEED_YUV_SUBSPACE_YCBCR);
}

/////////////////////////////////////////////////////////////////////////////////////


LiVESPixbuf *lives_pixbuf_new_blank(int width, int height, int palette) {
  LiVESPixbuf *pixbuf;
  int rowstride;
  uint8_t *pixels;
  size_t size;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    pixbuf=lives_pixbuf_new(FALSE, width, height);
    rowstride=lives_pixbuf_get_rowstride(pixbuf);
    pixels=lives_pixbuf_get_pixels(pixbuf);
    size=rowstride*(height-1)+get_last_rowstride_value(width,3);
    memset(pixels,0,size);
    break;
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
    pixbuf=lives_pixbuf_new(TRUE, width, height);
    rowstride=lives_pixbuf_get_rowstride(pixbuf);
    pixels=lives_pixbuf_get_pixels(pixbuf);
    size=rowstride*(height-1)+get_last_rowstride_value(width,4);
    memset(pixels,0,size);
    break;
  case WEED_PALETTE_ARGB32:
    pixbuf=lives_pixbuf_new(TRUE, width, height);
    rowstride=lives_pixbuf_get_rowstride(pixbuf);
    pixels=lives_pixbuf_get_pixels(pixbuf);
    size=rowstride*(height-1)+get_last_rowstride_value(width,4);
    memset(pixels,0,size);
    break;
  default:
    return NULL;
  }
  return pixbuf;
}



static LIVES_INLINE LiVESPixbuf *lives_pixbuf_cheat(boolean has_alpha,
    int width, int height, uint8_t *buf) {
  // we can cheat if our buffer is correctly sized
  LiVESPixbuf *pixbuf;
  int channels=has_alpha?4:3;
  int rowstride=get_rowstride_value(width*channels);
  pixbuf=lives_pixbuf_new_from_data(buf, has_alpha, width, height, rowstride,
                                    (LiVESPixbufDestroyNotify)lives_free_buffer, NULL);
  threaded_dialog_spin();
  return pixbuf;
}


LiVESPixbuf *layer_to_pixbuf(weed_plant_t *layer) {
  // create a weed layer from a pixbuf
  // layer "pixel_data" is then either shared with with the pixbuf pixels, or set to NULL

  int error;
  LiVESPixbuf *pixbuf;
  int palette;
  int width;
  int height;
  int irowstride;
  int rowstride,orowstride;
  uint8_t *pixel_data,*pixels,*end,*orig_pixel_data;
  boolean cheat=FALSE,done;
  int n_channels;

  if (layer==NULL) return NULL;

  palette=weed_get_int_value(layer,"current_palette",&error);
  width=weed_get_int_value(layer,"width",&error);
  height=weed_get_int_value(layer,"height",&error);
  irowstride=weed_get_int_value(layer,"rowstrides",&error);
  orig_pixel_data=pixel_data=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);

  do {
    done=TRUE;
    switch (palette) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_YUV888:
#ifndef GUI_QT
      if (irowstride==get_rowstride_value(width*3)) {
        pixbuf=lives_pixbuf_cheat(FALSE, width, height, pixel_data);
        cheat=TRUE;
      } else
#endif
        pixbuf=lives_pixbuf_new(FALSE, width, height);
      n_channels=3;
      break;
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGRA32:
#ifdef USE_SWSCALE
    case WEED_PALETTE_ARGB32:
#else
#ifdef GUI_QT
    case WEED_PALETTE_ARGB32:
#endif
#endif
    case WEED_PALETTE_YUVA8888:
#ifndef GUI_QT
      if (irowstride==get_rowstride_value(width*4)) {
        pixbuf=lives_pixbuf_cheat(TRUE, width, height, pixel_data);
        cheat=TRUE;
      } else
#endif
        pixbuf=lives_pixbuf_new(TRUE, width, height);
      n_channels=4;
      break;
    default:
      if (weed_palette_has_alpha_channel(palette)) {
        if (!convert_layer_palette(layer,WEED_PALETTE_RGBA32,0)) return NULL;
        palette=WEED_PALETTE_RGBA32;
      } else {
        if (!convert_layer_palette(layer,WEED_PALETTE_RGB24,0)) return NULL;
        palette=WEED_PALETTE_RGB24;
      }
      done=FALSE;
    }
  } while (!done);

  if (!cheat) {
    boolean done=FALSE;
    pixels=lives_pixbuf_get_pixels(pixbuf);
    orowstride=lives_pixbuf_get_rowstride(pixbuf);

    if (irowstride>orowstride) rowstride=orowstride;
    else rowstride=irowstride;
    end=pixels+orowstride*height;

    for (; pixels<end&&!done; pixels+=orowstride) {
      if (pixels+orowstride>=end) {
        orowstride=rowstride=get_last_rowstride_value(width,n_channels);
        done=TRUE;
      }
      lives_memcpy(pixels,pixel_data,rowstride);
      if (rowstride<orowstride) memset(pixels+rowstride,0,orowstride-rowstride);
      pixel_data+=irowstride;
    }
    if (!weed_plant_has_leaf(layer,"host_orig_pdata") || weed_get_boolean_value(layer,"host_orig_pdata",&error)==WEED_FALSE) {
      // host_orig_pdata is set if this is an alpha channel we "stole" from another layer
      lives_free(orig_pixel_data);
    }
    pixel_data=NULL;

    // indicates that the pixel data was copied, not shared
    weed_set_voidptr_value(layer,"pixel_data",NULL);
  }

#ifdef TEST_GAMMA
  register int j,k;

  if (current_gamma!=SCREEN_GAMMA*.6) update_gamma_lut(SCREEN_GAMMA*.6);

  width = lives_pixbuf_get_width(pixbuf);
  height = lives_pixbuf_get_height(pixbuf);

  pixels = lives_pixbuf_get_pixels(pixbuf);
  orowstride = lives_pixbuf_get_rowstride(pixbuf);
  end=pixels+height*orowstride;
  done=FALSE;

  for (; pixels<end&&!done; pixels+=orowstride) {
    if (pixels+orowstride>=end) {
      orowstride=get_last_rowstride_value(width,n_channels);
      done=TRUE;
    }
    for (j=0; j<width*3; j+=3) {
      for (k=0; k<3; k++) pixels[j+k]=gamma_lut[pixels[j+k]];
    }
  }
#endif

  return pixbuf;
}


LIVES_INLINE boolean weed_palette_is_resizable(int pal, int clamped, boolean in_out) {
  // in_out is TRUE for input, FALSE for output

  // in future we may also have resize candidates/delegates for other palettes
  // we will need to check for these

  if (pal==WEED_PALETTE_YUV888&&clamped==WEED_YUV_CLAMPING_UNCLAMPED) pal=WEED_PALETTE_RGB24;
  if (pal==WEED_PALETTE_YUVA8888&&clamped==WEED_YUV_CLAMPING_UNCLAMPED) pal=WEED_PALETTE_RGBA32;

#ifdef USE_SWSCALE
  if (in_out&&sws_isSupportedInput(weed_palette_to_avi_pix_fmt(pal,&clamped))) return TRUE;
  else if (sws_isSupportedOutput(weed_palette_to_avi_pix_fmt(pal,&clamped))) return TRUE;
#endif
  if (pal==WEED_PALETTE_RGB24||pal==WEED_PALETTE_RGBA32||pal==WEED_PALETTE_ARGB32||pal==WEED_PALETTE_BGR24||
      pal==WEED_PALETTE_BGRA32) return TRUE;
  return FALSE;
}


void lives_pixbuf_set_opaque(LiVESPixbuf *pixbuf) {
  unsigned char *pdata=lives_pixbuf_get_pixels(pixbuf);
  int row=lives_pixbuf_get_rowstride(pixbuf);
  int height=lives_pixbuf_get_height(pixbuf);
  int offs;
#ifdef GUI_GTK
  offs=3;
#endif

#ifdef GUI_QT
  offs=0;
#endif

  register int i,j;
  for (i=0; i<height; i++) {
    for (j=offs; j<row; j+=4) {
      pdata[j]=255;
    }
    pdata+=row;
  }
}



void compact_rowstrides(weed_plant_t *layer) {
  // remove any extra padding after the image data
  int error;
  int *rowstrides=weed_get_int_array(layer,"rowstrides",&error);
  int pal=weed_get_int_value(layer,"current_palette",&error);
  int width=weed_get_int_value(layer,"width",&error);
  int height=weed_get_int_value(layer,"height",&error);
  int xheight;
  int crow=width*weed_palette_get_bits_per_macropixel(pal)/8;
  int cxrow;
  int nplanes=weed_palette_get_numplanes(pal);
  register int i,j;

  size_t framesize=0;

  void **pixel_data,**new_pixel_data,*npixel_data;

  boolean needs_change=FALSE;

  pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  for (i=0; i<nplanes; i++) {
    cxrow=crow*weed_palette_get_plane_ratio_horizontal(pal,i);
    xheight=height*weed_palette_get_plane_ratio_vertical(pal,i);
    framesize+=CEIL(cxrow*xheight,32);
    if (cxrow!=rowstrides[i]) {
      // nth plane has extra padding
      needs_change=TRUE;
    }
  }

  if (!needs_change) {
    lives_free(pixel_data);
    lives_free(rowstrides);
    return;
  }

  npixel_data=lives_try_malloc(framesize);
  if (npixel_data==NULL) {
    lives_free(pixel_data);
    lives_free(rowstrides);
    return;
  }

  new_pixel_data=(void **)lives_malloc(nplanes*sizeof(void *));

  for (i=0; i<nplanes; i++) {
    cxrow=crow*weed_palette_get_plane_ratio_horizontal(pal,i);
    xheight=height*weed_palette_get_plane_ratio_vertical(pal,i);

    new_pixel_data[i]=npixel_data;

    for (j=0; j<xheight; j++) {
      lives_memcpy(new_pixel_data[i]+j*cxrow,pixel_data[i]+j*rowstrides[i],cxrow);
    }

    framesize=CEIL(cxrow*xheight,32);
    npixel_data+=framesize;

    rowstrides[i]=cxrow;

  }

  weed_layer_pixel_data_free(layer);

  if (nplanes>1)
    weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);

  weed_set_voidptr_array(layer,"pixel_data",nplanes,new_pixel_data);
  weed_set_int_array(layer,"rowstrides",nplanes,rowstrides);
  lives_free(pixel_data);
  lives_free(new_pixel_data);
  lives_free(rowstrides);
}


#ifdef USE_SWSCALE

static struct SwsContext *swscale_find_context(int iwidth, int iheight,
    int width, int height,
    enum PixelFormat ipixfmt, enum PixelFormat opixfmt, int flags) {
  register int i;
  struct _swscale_ctx tmpctx;

  if (!swscale_ctx_list_inited) {
    for (i=0; i<N_SWS_CTX; i++) {
      memset(&swscale_ctx[i],0,sizeof(struct _swscale_ctx));
      swscale_ctx_list_inited=TRUE;
    }
  }

  for (i=0; i<N_SWS_CTX; i++) {
    if (swscale_ctx[i].ctx==NULL) return NULL;

    if (swscale_ctx[i].iwidth==iwidth &&
        swscale_ctx[i].iheight==iheight &&
        swscale_ctx[i].width==width &&
        swscale_ctx[i].height==height &&
        swscale_ctx[i].ipixfmt==ipixfmt &&
        swscale_ctx[i].opixfmt==opixfmt &&
        swscale_ctx[i].flags==flags) {
      // move to front
      if (i>0) {
        lives_memcpy(&tmpctx,&swscale_ctx[i],sizeof(struct _swscale_ctx));
        for (; i>0; i--) {
          lives_memcpy(&swscale_ctx[i],&swscale_ctx[i-1],sizeof(struct _swscale_ctx));
        }
        lives_memcpy(&swscale_ctx[0],&tmpctx,sizeof(struct _swscale_ctx));
      }
      return swscale_ctx[0].ctx;
    }

  }

  return NULL;
}


static void swscale_add_context(int iwidth, int iheight, int width, int height, enum PixelFormat ipixfmt, enum PixelFormat opixfmt,
                                int flags, struct SwsContext *ctx) {
  // add at head of list
  register int i;

  for (i=N_SWS_CTX-1; i>0; i--) {
    // make space by shifting up
    if (swscale_ctx[i-1].ctx==NULL) continue;

    // free last in list
    if (i==N_SWS_CTX-1)
      sws_freeContext(swscale_ctx[i].ctx);

    // shift up
    lives_memcpy(&swscale_ctx[i],&swscale_ctx[i-1],sizeof(struct _swscale_ctx));
  }

  // add at posn 0
  swscale_ctx[0].iwidth=iwidth;
  swscale_ctx[0].iheight=iheight;
  swscale_ctx[0].width=width;
  swscale_ctx[0].height=height;
  swscale_ctx[0].ipixfmt=ipixfmt;
  swscale_ctx[0].opixfmt=opixfmt;
  swscale_ctx[0].flags=flags;
  swscale_ctx[0].ctx=ctx;
}


void sws_free_context(void) {
  register int i;
  if (!swscale_ctx_list_inited) return;

  for (i=0; i<N_SWS_CTX; i++) {
    if (swscale_ctx[i].ctx!=NULL) sws_freeContext(swscale_ctx[i].ctx);
  }
}

#endif

boolean resize_layer(weed_plant_t *layer, int width, int height, LiVESInterpType interp, int opal_hint, int oclamp_hint) {
  // resize a layer; width is in macropixels

  // TODO ** - see if there is a resize plugin candidate/delegate which supports this palette :
  // this allows e.g libabl or a hardware rescaler

  // opal_hint and oclamp_hint may be set to hint the desired output palette and clamping
  // this is simply to ensure more efficient resizing
  // - setting opal_hint to WEED_PALETTE_END will attempt to minimise palette changes

  // "current_palette" should be checked on return

  // don't forget also - layer "width" is in *macro-pixels* so it may not come back exactly as expected

  // return FALSE if we were unable to resize

  LiVESPixbuf *pixbuf=NULL;
  LiVESPixbuf *new_pixbuf=NULL;

  boolean keep_in_pixel_data=FALSE;
  boolean is_contiguous=FALSE;
  boolean retval=TRUE;

  int error;
  int palette=weed_get_int_value(layer,"current_palette",&error);

  // original width and height (in macropixels)
  int iwidth=weed_get_int_value(layer,"width",&error);
  int iheight=weed_get_int_value(layer,"height",&error);
  int iclamped=WEED_YUV_CLAMPING_UNCLAMPED;


  if (iwidth==width&&iheight==height) return TRUE; // no resize needed

  if (width<=0||height<=0) {
    char *msg=lives_strdup_printf("unable to scale layer to %d x %d for palette %d\n",width,height,palette);
    LIVES_DEBUG(msg);
    lives_free(msg);
    return FALSE;
  }

  // if in palette is a YUV palette which we cannot scale, convert to YUV888 (unclamped) or YUVA8888 (unclamped)
  // we can always scale these
  if (weed_palette_is_yuv_palette(palette)) {
    iclamped=weed_get_int_value(layer,"YUV_clamping",&error);
    if (!weed_palette_is_resizable(palette, iclamped, TRUE)) {
      iwidth*=weed_palette_get_pixels_per_macropixel(palette); // orig width is in macropixels
      width*=weed_palette_get_pixels_per_macropixel(palette); // desired width is in macropixels
      if (opal_hint==WEED_PALETTE_END||weed_palette_is_yuv_palette(opal_hint)) {
        // we should always convert to unclamped values before resizing
        int oclamping=WEED_YUV_CLAMPING_UNCLAMPED;

        if (weed_palette_has_alpha_channel(palette)) {
          convert_layer_palette(layer,WEED_PALETTE_YUVA8888,oclamping);
        } else {
          convert_layer_palette(layer,WEED_PALETTE_YUV888,oclamping);
        }
        iclamped=oclamping;
      } else {
        if (weed_palette_has_alpha_channel(palette)) {
          convert_layer_palette(layer,WEED_PALETTE_RGBA32,0);
        } else {
          convert_layer_palette(layer,WEED_PALETTE_RGB24,0);
        }
      }
      palette=weed_get_int_value(layer,"current_palette",&error);
    }
  }

  // check if we can also convert to the output palette
#ifdef USE_SWSCALE
  // only swscale can convert and resize together
  if (opal_hint==WEED_PALETTE_END||!weed_palette_is_resizable(opal_hint,oclamp_hint,FALSE)) {
#endif
    opal_hint=palette;
    oclamp_hint=iclamped;
#ifdef USE_SWSCALE
  }
#endif

  if (weed_plant_has_leaf(layer,"host_orig_pdata")&&weed_get_boolean_value(layer,"host_orig_pdata",&error)==WEED_TRUE) {
    // host_orig_pdata is set if this is an alpha channel we "stole" from another layer
    keep_in_pixel_data=TRUE;
  }

  if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") &&
      weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
    is_contiguous=TRUE;

  // if we cannot scale YUV888 (unclamped) or YUVA8888 (unclamped) directly, pretend they are RGB24 and RGBA32
  if (palette==WEED_PALETTE_YUV888&&iclamped==WEED_YUV_CLAMPING_UNCLAMPED
      &&!weed_palette_is_resizable(palette, iclamped, TRUE)) opal_hint=palette=WEED_PALETTE_RGB24;
  if (palette==WEED_PALETTE_YUVA8888&&iclamped==WEED_YUV_CLAMPING_UNCLAMPED
      &&!weed_palette_is_resizable(palette, iclamped, TRUE)) opal_hint=palette=WEED_PALETTE_RGBA32;

  // check if we can convert to the target palette/clamping
  if (!weed_palette_is_resizable(opal_hint, oclamp_hint, FALSE)) {
    opal_hint=palette;
    oclamp_hint=iclamped;
  }


#ifdef USE_SWSCALE
  if (iwidth>1&&iheight>1&&weed_palette_is_resizable(palette, iclamped, TRUE)&&
      weed_palette_is_resizable(opal_hint, oclamp_hint, FALSE)) {
    struct SwsContext *swscale;

    enum PixelFormat ipixfmt,opixfmt;

    void **pd_array;
    void **in_pixel_data,**out_pixel_data;

    boolean store_ctx=FALSE;

    int *irowstrides,*orowstrides;
    int *ir_array,*or_array;

    int flags;

    register int i;

    av_log_set_level(AV_LOG_FATAL);

    mainw->rowstride_alignment_hint=16;

    if (interp==LIVES_INTERP_BEST) flags=SWS_BICUBIC;
    if (interp==LIVES_INTERP_NORMAL) flags=SWS_BILINEAR;
    if (interp==LIVES_INTERP_FAST) flags=SWS_FAST_BILINEAR;

    ipixfmt=weed_palette_to_avi_pix_fmt(palette,&iclamped);
    opixfmt=weed_palette_to_avi_pix_fmt(opal_hint,&oclamp_hint);

    in_pixel_data=(void **)lives_malloc0(4*sizeof(void *));
    irowstrides=(int *)lives_malloc0(4*sizint);

    pd_array=weed_get_voidptr_array(layer,"pixel_data",&error);
    ir_array=weed_get_int_array(layer,"rowstrides",&error);

    for (i=0; i<weed_palette_get_numplanes(palette); i++) {
      in_pixel_data[i]=pd_array[i];
      irowstrides[i]=ir_array[i];
    }

    lives_free(pd_array);
    lives_free(ir_array);

    if (palette!=opal_hint) {
      width*=weed_palette_get_pixels_per_macropixel(palette); // desired width is in macropixels
      width/=weed_palette_get_pixels_per_macropixel(opal_hint); // desired width is in macropixels
      weed_set_int_value(layer,"current_palette",opal_hint);
    }

    if (weed_palette_is_yuv_palette(opal_hint))
      weed_set_int_value(layer,"YUV_clamping",oclamp_hint);

    weed_set_int_value(layer,"width",width);
    weed_set_int_value(layer,"height",height);

    create_empty_pixel_data(layer,FALSE,TRUE);

    out_pixel_data=(void **)lives_malloc0(4*sizeof(void *));
    orowstrides=(int *)lives_malloc0(4*sizint);

    pd_array=weed_get_voidptr_array(layer,"pixel_data",&error);
    or_array=weed_get_int_array(layer,"rowstrides",&error);

    for (i=0; i<weed_palette_get_numplanes(opal_hint); i++) {
      out_pixel_data[i]=pd_array[i];
      orowstrides[i]=or_array[i];
    }

    lives_free(pd_array);
    lives_free(or_array);

    width*=weed_palette_get_pixels_per_macropixel(opal_hint);

    iwidth*=weed_palette_get_pixels_per_macropixel(palette); // input width is in macropixels

    if ((swscale=swscale_find_context(iwidth,iheight,width,height,ipixfmt,opixfmt,flags))==NULL) {
      swscale = sws_getContext(iwidth, iheight, ipixfmt, width, height, opixfmt, flags, NULL, NULL, NULL);
      store_ctx=TRUE;
    }

    if (swscale==NULL) {
      LIVES_DEBUG("swscale is NULL !!");
    } else {
      sws_scale(swscale, (const uint8_t * const *)in_pixel_data, irowstrides, 0, iheight,
                (uint8_t * const *)out_pixel_data, orowstrides);
      if (store_ctx) swscale_add_context(iwidth,iheight,width,height,ipixfmt,opixfmt,flags,swscale);
    }

    if (!keep_in_pixel_data) {
      for (i=0; i<weed_palette_get_numplanes(palette); i++) {
        lives_free(in_pixel_data[i]);
        if (is_contiguous) break;
      }
    } else weed_leaf_delete(layer,"host_orig_pdata");

    lives_free(out_pixel_data);

    lives_free(orowstrides);
    lives_free(irowstrides);

    return TRUE;
  }
#endif

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:

  case WEED_PALETTE_YUV888:
  case WEED_PALETTE_YUVA8888:

    // create a new pixbuf
    pixbuf=layer_to_pixbuf(layer);

    threaded_dialog_spin();
    new_pixbuf=lives_pixbuf_scale_simple(pixbuf,width,height,interp);
    threaded_dialog_spin();
    if (new_pixbuf!=NULL) {
      weed_set_int_value(layer,"width",lives_pixbuf_get_width(new_pixbuf));
      weed_set_int_value(layer,"height",lives_pixbuf_get_height(new_pixbuf));
      weed_set_int_value(layer,"rowstrides",lives_pixbuf_get_rowstride(new_pixbuf));
    }

    // might be threaded here so we need a mutex to stop other thread resetting free_fn
    pthread_mutex_lock(&mainw->free_fn_mutex);

    if (weed_get_voidptr_value(layer,"pixel_data",&error)!=NULL && keep_in_pixel_data) {
      // we created a shared layer/pixbuf, and we need to preserve the data
      mainw->do_not_free=(livespointer)lives_pixbuf_get_pixels_readonly(pixbuf);
      mainw->free_fn=_lives_free_with_check;
    }

    lives_object_unref(pixbuf);
    mainw->do_not_free=NULL;
    mainw->free_fn=_lives_free_normal;

    pthread_mutex_unlock(&mainw->free_fn_mutex);

    break;
  default:
    lives_printerr("Warning: resizing unknown palette %d\n",palette);
    break_me();
    retval=FALSE;
  }

  if (new_pixbuf==NULL||(width!=weed_get_int_value(layer,"width",&error)||
                         height!=weed_get_int_value(layer,"height",&error)))  {
    lives_printerr("unable to scale layer to %d x %d for palette %d\n",width,height,palette);
    retval=FALSE;
  } else {
    if (weed_plant_has_leaf(layer,"host_orig_pdata"))
      weed_leaf_delete(layer,"host_orig_pdata");
  }

  // might be threaded here so we need a mutex to stop other thread resetting free_fn
  pthread_mutex_lock(&mainw->free_fn_mutex);

  if (new_pixbuf!=NULL) {
    if (pixbuf_to_layer(layer,new_pixbuf)) {
      // TODO - for QImage, cast to parent class and use parent destructor
      mainw->do_not_free=(livespointer)lives_pixbuf_get_pixels_readonly(new_pixbuf);
      mainw->free_fn=_lives_free_with_check;
    }
    lives_object_unref(new_pixbuf);
    mainw->do_not_free=NULL;
    mainw->free_fn=_lives_free_normal;
  }

  pthread_mutex_unlock(&mainw->free_fn_mutex);

  return retval;
}


void letterbox_layer(weed_plant_t *layer, int width, int height, int nwidth, int nheight) {
  // stretch or shrink layer to width/height, then overlay it in a black rectangle size nwidth/nheight
  // width, nwidth should be in macropixels

  int error;
  int offs_x=0,offs_y=0;
  int pal,nplanes;
  LiVESInterpType interp;

  int *rowstrides,*irowstrides;

  void **pixel_data;
  void **new_pixel_data;

  uint8_t *dst,*src;

  register int i;

  if (nwidth*nheight==0) return;

  interp=get_interp_value(prefs->pb_quality);

  pal=weed_get_int_value(layer,"current_palette",&error);
  nwidth*=weed_palette_get_pixels_per_macropixel(pal); // convert from macropixels to pixels

  resize_layer(layer,width,height,interp,WEED_PALETTE_END,0); // resize can change current_palette

  // get current pixel_data
  pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
  if (pixel_data==NULL) return;

  pal=weed_get_int_value(layer,"current_palette",&error);
  nwidth/=weed_palette_get_pixels_per_macropixel(pal); // set back to macropixels in new palette

  width=weed_get_int_value(layer,"width",&error);
  height=weed_get_int_value(layer,"height",&error);
  irowstrides=weed_get_int_array(layer,"rowstrides",&error);

  // create new pixel_data - all black
  weed_set_int_value(layer,"width",nwidth);
  weed_set_int_value(layer,"height",nheight);
  create_empty_pixel_data(layer,TRUE,TRUE);
  new_pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);

  nwidth=weed_get_int_value(layer,"width",&error);
  nheight=weed_get_int_value(layer,"height",&error);

  if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous") &&
      weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
    nplanes=1;

  if (nwidth<width||nheight<height) {
    for (i=0; i<nplanes; i++) free(new_pixel_data[i]);
    lives_free(new_pixel_data);
    weed_set_voidptr_array(layer,"pixel_data",nplanes,pixel_data);
    lives_free(pixel_data);
    lives_free(irowstrides);
    weed_set_int_value(layer,"width",width);
    weed_set_int_value(layer,"height",height);
    return;
  }

  offs_x=(nwidth-width+1)>>1;
  offs_y=(nheight-height+1)>>1;

  rowstrides=weed_get_int_array(layer,"rowstrides",&error);

  switch (pal) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
  case WEED_PALETTE_YUV888:
    width*=3;
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*3);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_RGBA32:
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_ARGB32:
  case WEED_PALETTE_YUVA8888:
  case WEED_PALETTE_UYVY:
  case WEED_PALETTE_YUYV:
    width*=4;
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*4);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_YUV411:
    width*=6;
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*6);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_YUV444P:
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    dst=(uint8_t *)(new_pixel_data[1]+offs_y*rowstrides[1]+offs_x);
    src=(uint8_t *)pixel_data[1];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[1];
      src+=irowstrides[1];
    }
    dst=(uint8_t *)(new_pixel_data[2]+offs_y*rowstrides[2]+offs_x);
    src=(uint8_t *)pixel_data[2];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[2];
      src+=irowstrides[2];
    }
    break;

  case WEED_PALETTE_YUVA4444P:
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    dst=(uint8_t *)(new_pixel_data[1]+offs_y*rowstrides[1]+offs_x);
    src=(uint8_t *)pixel_data[1];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[1];
      src+=irowstrides[1];
    }
    dst=(uint8_t *)(new_pixel_data[2]+offs_y*rowstrides[2]+offs_x);
    src=(uint8_t *)pixel_data[2];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[2];
      src+=irowstrides[2];
    }
    dst=(uint8_t *)(new_pixel_data[3]+offs_y*rowstrides[3]+offs_x);
    src=(uint8_t *)pixel_data[3];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[3];
      src+=irowstrides[3];
    }
    break;

  case WEED_PALETTE_YUV422P:
    width*=4;
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    height>>=1;
    offs_x>>=1;
    dst=(uint8_t *)(new_pixel_data[1]+offs_y*rowstrides[1]+offs_x);
    src=(uint8_t *)pixel_data[1];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[1];
      src+=irowstrides[1];
    }
    dst=(uint8_t *)(new_pixel_data[2]+offs_y*rowstrides[2]+offs_x);
    src=(uint8_t *)pixel_data[2];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[2];
      src+=irowstrides[2];
    }
    break;

  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    height>>=1;
    offs_x>>=1;
    width>>=1;
    offs_y>>=1;
    dst=(uint8_t *)(new_pixel_data[1]+offs_y*rowstrides[1]+offs_x);
    src=(uint8_t *)pixel_data[1];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[1];
      src+=irowstrides[1];
    }
    dst=(uint8_t *)(new_pixel_data[2]+offs_y*rowstrides[2]+offs_x);
    src=(uint8_t *)pixel_data[2];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[2];
      src+=irowstrides[2];
    }
    break;

  case WEED_PALETTE_RGBFLOAT:
    width*=3*sizeof(float);
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*3*sizeof(float));
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_RGBAFLOAT:
    width*=4*sizeof(float);
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*4*sizeof(float));
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_AFLOAT:
    width*=sizeof(float);
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x*sizeof(float));
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

  case WEED_PALETTE_A8:
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+offs_x);
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;

    // assume offs_x and width is a multiple of 8
  case WEED_PALETTE_A1:
    width>>=3;
    dst=(uint8_t *)(new_pixel_data[0]+offs_y*rowstrides[0]+(offs_x>>3));
    src=(uint8_t *)pixel_data[0];
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,width);
      dst+=rowstrides[0];
      src+=irowstrides[0];
    }
    break;
  }

  for (i=0; i<nplanes; i++) if (pixel_data[i]!=NULL) lives_free(pixel_data[i]);

  lives_free(pixel_data);
  lives_free(new_pixel_data);
  lives_free(irowstrides);
  lives_free(rowstrides);

}


boolean pixbuf_to_layer(weed_plant_t *layer, LiVESPixbuf *pixbuf) {
  // turn a (Gdk)Pixbuf into a Weed layer

  // a "layer" is CHANNEL type plant which is not created from a plugin CHANNEL_TEMPLATE.
  // When we pass this to a plugin, we need to adjust it depending
  // on the plugin's CHANNEL_TEMPLATE to which we will assign it

  // e.g.: memory may need aligning afterwards for particular plugins which set channel template flags:
  // memory may need aligning, layer palette may need changing, layer may need resizing

  // return TRUE if we can use the original pixbuf pixels; in this case the pixbuf pixels should not be free()d !
  // see code example.


  /* code example:

  boolean needs_unlock=FALSE;

  if (pixbuf!=NULL) {
    if (pixbuf_to_layer(layer,pixbuf)) {
      mainw->do_not_free=(livespointer)lives_pixbuf_get_pixels_readonly(pixbuf);
      pthread_mutex_lock(&mainw->free_fn_mutex);
      needs_unlock=TRUE;
      mainw->free_fn=_lives_free_with_check;
    }
    lives_object_unref(pixbuf);
    mainw->do_not_free=NULL;
    mainw->free_fn=_lives_free_normal;
    if (needs_unlock) pthread_mutex_unlock(&mainw->free_fn_mutex);

  }


  */

  int rowstride;
  int width;
  int height;
  int nchannels;
  void *pixel_data;
  void *in_pixel_data;

  size_t framesize;

  if (!LIVES_IS_PIXBUF(pixbuf)) {
    weed_set_int_value(layer,"width",0);
    weed_set_int_value(layer,"height",0);
    weed_set_int_value(layer,"rowstrides",0);
    weed_set_voidptr_value(layer,"pixel_data",NULL);
    return FALSE;
  }

  rowstride=lives_pixbuf_get_rowstride(pixbuf);
  width=lives_pixbuf_get_width(pixbuf);
  height=lives_pixbuf_get_height(pixbuf);
  nchannels=lives_pixbuf_get_n_channels(pixbuf);

  weed_set_int_value(layer,"width",width);
  weed_set_int_value(layer,"height",height);
  weed_set_int_value(layer,"rowstrides",rowstride);

  if (!weed_plant_has_leaf(layer,"current_palette")) {
#ifdef GUI_GTK
    if (nchannels==4) weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGBA32);
    else weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGB24);

#endif
#ifdef GUI_QT
    // TODO - need to check this, it may be endian dependent
    if (nchannels==4) {
      int flags=0,error;
      weed_set_int_value(layer,"current_palette",WEED_PALETTE_ARGB32);
      if (weed_plant_has_leaf(layer,"flags")) flags=weed_get_int_value(layer,"flags",&error);
      flags|=WEED_CHANNEL_ALPHA_PREMULT;
      weed_set_int_value(layer,"flags",flags);
    } else weed_set_int_value(layer,"current_palette",WEED_PALETTE_RGB24);
#endif
  }

#ifndef GUI_QT
  if (rowstride==get_last_rowstride_value(width,nchannels)) {
    in_pixel_data=(void *)lives_pixbuf_get_pixels(pixbuf);
    weed_set_voidptr_value(layer,"pixel_data",in_pixel_data);
    return TRUE;
  }
#endif

  framesize=CEIL(rowstride*height,32);

  pixel_data=lives_calloc(framesize>>4,16);

  if (pixel_data!=NULL) {
    in_pixel_data=(void *)lives_pixbuf_get_pixels_readonly(pixbuf);
    lives_memcpy(pixel_data,in_pixel_data,rowstride*(height-1));
    // this part is needed because layers always have a memory size height*rowstride, whereas gdkpixbuf can have
    // a shorter last row
    lives_memcpy(pixel_data+rowstride*(height-1),in_pixel_data+rowstride*(height-1),get_last_rowstride_value(width,nchannels));
  }

  weed_set_voidptr_value(layer,"pixel_data",pixel_data);
  return FALSE;
}







//////////////////////////////////////
// TODO - move into layers.c

#ifdef GUI_GTK


LIVES_INLINE int get_weed_palette_for_lives_painter(void) {
  // TODO - should move to weed-compat.h
  return (capable->byte_order==LIVES_BIG_ENDIAN)?WEED_PALETTE_ARGB32:WEED_PALETTE_BGRA32;
}


lives_painter_t *layer_to_lives_painter(weed_plant_t *layer) {
  // convert a weed layer to lives_painter

  // "width","rowstrides" and "current_palette" of layer may all change

  int irowstride,orowstride;
  int width,widthx;
  int height,pal;
  int error;

  register int i;

  uint8_t *src,*dst,*orig_pixel_data,*pixel_data;

  lives_painter_surface_t *surf;
  lives_painter_t *cairo;
  lives_painter_format_t cform;


  width=weed_get_int_value(layer,"width",&error);

  pal=weed_get_int_value(layer,"current_palette",&error);
  if (pal==WEED_PALETTE_A8) {
    cform=LIVES_PAINTER_FORMAT_A8;
    widthx=width;
  } else if (pal==WEED_PALETTE_A1) {
    cform=LIVES_PAINTER_FORMAT_A1;
    widthx=width>>3;
  } else {
    if (capable->byte_order==LIVES_BIG_ENDIAN) {
      convert_layer_palette(layer,WEED_PALETTE_ARGB32,0);
    } else {
      convert_layer_palette(layer,WEED_PALETTE_BGRA32,0);
    }
    cform=LIVES_PAINTER_FORMAT_ARGB32;
    widthx=width<<2;
  }

  height=weed_get_int_value(layer,"height",&error);

  irowstride=weed_get_int_value(layer,"rowstrides",&error);

  orowstride=lives_painter_format_stride_for_width(cform,width);

  orig_pixel_data=src=(uint8_t *)weed_get_voidptr_value(layer,"pixel_data",&error);

  if (irowstride==orowstride) {
    pixel_data=src;
  } else {
    dst=pixel_data=(uint8_t *)lives_try_malloc(height*orowstride);
    if (pixel_data==NULL) return NULL;
    for (i=0; i<height; i++) {
      lives_memcpy(dst,src,widthx);
      memset(dst+widthx,0,widthx-orowstride);
      dst+=orowstride;
      src+=irowstride;
    }
    lives_free(orig_pixel_data);
    weed_set_voidptr_value(layer,"pixel_data",pixel_data);
    weed_set_int_value(layer,"rowstrides",orowstride);
  }

  if (cform==LIVES_PAINTER_FORMAT_ARGB32 && weed_palette_has_alpha_channel(pal)) {
    int flags=0;
    if (weed_plant_has_leaf(layer,"flags")) flags=weed_get_int_value(layer,"flags",&error);
    if (!(flags&WEED_CHANNEL_ALPHA_PREMULT)) {
      // if we have post-multiplied alpha, pre multiply
      alpha_unpremult(layer,FALSE);
      flags|=WEED_CHANNEL_ALPHA_PREMULT;
      weed_set_int_value(layer,"flags",flags);
    }
  }

  surf=lives_painter_image_surface_create_for_data(pixel_data,
       cform,
       width, height,
       orowstride);

  if (surf==NULL) return NULL;

  cairo=lives_painter_create(surf); // surf is refcounted
  lives_painter_surface_destroy(surf); // reduce refcount, so it is destroyed with the cairo

  return cairo;
}



boolean lives_painter_to_layer(lives_painter_t *cr, weed_plant_t *layer) {
  // updates a weed_layer from a cr

  // TODO *** - keep the surface around using lives_painter_surface_reference() and destroy it when the "pixel_data" is freed or changed

  void *pixel_data,*src;

  int width,height,rowstride;

  lives_painter_surface_t *surface=lives_painter_get_target(cr);
  lives_painter_format_t  cform;

  // flush to ensure all writing to the image was done
  lives_painter_surface_flush(surface);

  src = lives_painter_image_surface_get_data(surface);
  width = lives_painter_image_surface_get_width(surface);
  height = lives_painter_image_surface_get_height(surface);
  rowstride = lives_painter_image_surface_get_stride(surface);

  if (weed_plant_has_leaf(layer,"pixel_data")) {
    int error;
    pixel_data=weed_get_voidptr_value(layer,"pixel_data",&error);
    if (pixel_data!=NULL&&pixel_data!=src) lives_free(pixel_data);
  }

  pixel_data=lives_try_malloc(CEIL(height*rowstride,32));

  weed_set_voidptr_value(layer,"pixel_data",pixel_data);

  if (pixel_data==NULL) return FALSE;

  lives_memcpy(pixel_data,src,height*rowstride);

  weed_set_int_value(layer,"rowstrides",rowstride);
  weed_set_int_value(layer,"width",width);
  weed_set_int_value(layer,"height",height);

  cform = lives_painter_image_surface_get_format(surface);

  switch (cform) {
  case LIVES_PAINTER_FORMAT_ARGB32:
    if (capable->byte_order==LIVES_BIG_ENDIAN) {
      weed_set_int_value(layer,"current_palette",WEED_PALETTE_ARGB32);
    } else {
      weed_set_int_value(layer,"current_palette",WEED_PALETTE_BGRA32);
    }

    if (prefs->alpha_post) {
      // un-premultiply the alpha
      alpha_unpremult(layer,TRUE);
    } else {
      int flags=0,error;
      if (weed_plant_has_leaf(layer,"flags"))
        flags=weed_get_int_value(layer,"flags",&error);

      flags|=WEED_CHANNEL_ALPHA_PREMULT;
      weed_set_int_value(layer,"flags",flags);
    }
    break;

  case LIVES_PAINTER_FORMAT_A8:
    weed_set_int_value(layer,"current_palette",WEED_PALETTE_A8);
    break;

  case LIVES_PAINTER_FORMAT_A1:
    weed_set_int_value(layer,"current_palette",WEED_PALETTE_A1);
    break;

  default:
    break;
  }

  return TRUE;
}

#endif

weed_plant_t *weed_layer_new(int width, int height, int *rowstrides, int current_palette) {
  weed_plant_t *layer=weed_plant_new(WEED_PLANT_CHANNEL);

  weed_set_int_value(layer,"width",width);
  weed_set_int_value(layer,"height",height);

  if (current_palette!=WEED_PALETTE_END) {
    weed_set_int_value(layer,"current_palette",current_palette);
    if (rowstrides!=NULL) weed_set_int_array(layer,"rowstrides",weed_palette_get_numplanes(current_palette),rowstrides);
  }
  return layer;
}


weed_plant_t *weed_layer_copy(weed_plant_t *dlayer, weed_plant_t *slayer) {
  // copy source slayer to dest dlayer
  // for a newly created layer, this is a deep copy, since the pixel_data array is also copied

  // for an existing dlayer, we copy pixel_data by reference

  // if dlayer is NULL, we return a new plant, otherwise we return dlayer

  int pd_elements,error;
  void **pd_array,**pixel_data;
  void *npixel_data;
  int height,width,palette,flags;
  int *rowstrides;
  size_t size,totsize=0;
  boolean deep=FALSE,contig;

  register int i;

  weed_plant_t *layer;

  if (dlayer==NULL) {
    layer=weed_plant_new(WEED_PLANT_CHANNEL);
    deep=TRUE;
  } else layer=dlayer;

  // now copy relevant leaves
  flags=weed_get_int_value(slayer,"flags",&error);
  height=weed_get_int_value(slayer,"height",&error);
  width=weed_get_int_value(slayer,"width",&error);
  palette=weed_get_int_value(slayer,"current_palette",&error);
  pd_elements=weed_leaf_num_elements(slayer,"pixel_data");
  pixel_data=weed_get_voidptr_array(slayer,"pixel_data",&error);
  rowstrides=weed_get_int_array(slayer,"rowstrides",&error);
  contig=weed_get_boolean_value(slayer,"host_pixel_data_contiguous",&error);

  weed_set_boolean_value(layer,"host_pixel_data_contiguous",contig);
  weed_set_voidptr_value(layer,"pixel_data",NULL);

  if (deep) {
    pd_array=(void **)lives_malloc(pd_elements*sizeof(void *));

    for (i=0; i<pd_elements; i++) {
      size=(size_t)((double)height*weed_palette_get_plane_ratio_vertical(palette,i)*(double)rowstrides[i]);
      totsize+=CEIL(size,32);
    }

    npixel_data=lives_try_malloc(totsize);
    if (npixel_data==NULL) return layer;

    for (i=0; i<pd_elements; i++) {
      size=(size_t)((double)height*weed_palette_get_plane_ratio_vertical(palette,i)*(double)rowstrides[i]);
      pd_array[i]=npixel_data;
      lives_memcpy(pd_array[i],pixel_data[i],size);
      npixel_data+=CEIL(size,32);
    }
    if (pd_elements>1)
      weed_set_boolean_value(layer,"host_pixel_data_contiguous",WEED_TRUE);
    else if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous"))
      weed_leaf_delete(layer,"host_pixel_data_contiguous");
  } else pd_array=pixel_data;

  weed_set_voidptr_array(layer,"pixel_data",pd_elements,pd_array);
  weed_set_int_value(layer,"flags",flags);
  weed_set_int_value(layer,"height",height);
  weed_set_int_value(layer,"width",width);
  weed_set_int_value(layer,"current_palette",palette);
  weed_set_int_array(layer,"rowstrides",pd_elements,rowstrides);

  if (weed_plant_has_leaf(slayer,"YUV_clamping"))
    weed_set_int_value(layer,"YUV_clamping",weed_get_int_value(slayer,"YUV_clamping",&error));
  if (weed_plant_has_leaf(slayer,"YUV_subspace"))
    weed_set_int_value(layer,"YUV_subspace",weed_get_int_value(slayer,"YUV_subspace",&error));
  if (weed_plant_has_leaf(slayer,"YUV_sampling"))
    weed_set_int_value(layer,"YUV_sampling",weed_get_int_value(slayer,"YUV_sampling",&error));

  if (weed_plant_has_leaf(slayer,"pixel_aspect_ratio"))
    weed_set_double_value(layer,"pixel_aspect_ratio",weed_get_int_value(slayer,"pixel_aspect_ratio",&error));

  if (pd_array!=pixel_data) lives_free(pd_array);
  lives_free(pixel_data);
  lives_free(rowstrides);

  return layer;
}



void weed_layer_pixel_data_free(weed_plant_t *layer) {
  void **pixel_data;

  int error;
  int pd_elements;

  register int i;

  if (layer==NULL) return;

  if (weed_plant_has_leaf(layer,"host_orig_pdata")&&weed_get_boolean_value(layer,"host_orig_pdata",&error)==WEED_TRUE)
    return;

  if (weed_plant_has_leaf(layer,"pixel_data")) {
    pd_elements=weed_leaf_num_elements(layer,"pixel_data");
    if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous")&&
        weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE) pd_elements=1;
    if (pd_elements>0) {
      pixel_data=weed_get_voidptr_array(layer,"pixel_data",&error);
      if (pixel_data!=NULL) {

        if (weed_plant_has_leaf(layer,"host_pixel_data_contiguous")) {
          if (weed_get_boolean_value(layer,"host_pixel_data_contiguous",&error)==WEED_TRUE)
            pd_elements=1;
          weed_leaf_delete(layer,"host_pixel_data_contiguous");
        }

        for (i=0; i<pd_elements; i++) {
          if (pixel_data[i]!=NULL) lives_free(pixel_data[i]);
        }

        lives_free(pixel_data);
        weed_set_voidptr_value(layer,"pixel_data",NULL);
      }
    }
  }
}

void weed_layer_free(weed_plant_t *layer) {

  if (layer==NULL) return;
  weed_layer_pixel_data_free(layer);
  weed_plant_free(layer);
}


int weed_layer_get_palette(weed_plant_t *layer) {
  int error;
  int pal=weed_get_int_value(layer,"current_palette",&error);
  if (error==WEED_NO_ERROR) return pal;
  return WEED_PALETTE_END;
}



void insert_blank_frames(int sfileno, int nframes, int after) {
  // insert blank frames in clip (only valid just after clip is opened)

  // this is ugly, it should be moved to another file

  lives_clip_t *sfile=mainw->files[sfileno];
  LiVESPixbuf *blankp;
  LiVESError *error=NULL;
  char oname[PATH_MAX];
  char nname[PATH_MAX];
  char *com;

  register int i;

  blankp=lives_pixbuf_new_blank(sfile->hsize,sfile->vsize,WEED_PALETTE_RGB24);

  for (i=1; i<=sfile->frames; i++) {
    lives_snprintf(oname,PATH_MAX,"%s/%s/%08d.%s",prefs->tmpdir,sfile->handle,i,get_image_ext_for_type(sfile->img_type));
    if (lives_file_test(oname,LIVES_FILE_TEST_EXISTS)) {
      lives_snprintf(nname,PATH_MAX,"%s/%s/%08d.%s",prefs->tmpdir,sfile->handle,i+nframes,get_image_ext_for_type(sfile->img_type));
      mainw->com_failed=FALSE;
#ifndef IS_MINGW
      com=lives_strdup_printf("/bin/mv \"%s\" \"%s\"",
                              oname,nname);
#else
      com=lives_strdup_printf("mv.exe \"%s\" \"%s\"",
                              oname,nname);

#endif
      lives_system(com,FALSE);
      lives_free(com);

      if (mainw->com_failed) {
        return;
      }
    }
  }

  for (i=after; i<after+nframes; i++) {
    lives_snprintf(oname,PATH_MAX,"%s/%s/%08d.%s",prefs->tmpdir,sfile->handle,i+1,get_image_ext_for_type(sfile->img_type));
    lives_pixbuf_save(blankp, oname, sfile->img_type, 100-prefs->ocp, TRUE, &error);
    if (error!=NULL) {
      lives_error_free(error);
      break;
    }
  }


  insert_images_in_virtual(sfileno,after,nframes,NULL,0);

  sfile->frames+=nframes;

  lives_object_unref(blankp);
}
