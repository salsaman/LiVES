// farneback_analyser.cpp
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// Computes a dense optical flow using the Gunnar Farneback’s algorithm.

/*
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
  following conditions are met:

    Redistributions of source code must retain the above copyright notice, this list of conditions
    and the following disclaimer.
    Redistributions in binary form must reproduce the above copyright notice, this list of conditions
    and the following disclaimer in the documentation and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
  INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
  WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// calculates array flow, s.t.:
// prevImg(y,x) = nextImg( y + flow(y,x)[0], x + flow(y,x)[1] )

// input is nextImg : this is cached in memory to provide prevImg for the next call
// output is two channels of type ALPHA FLOAT (the flow(y,x)[0] in the first, and the flow(y,x)[1] in the second)

///////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package

//////////////////////////////////////////////////////////////////

#define NEED_PALETTE_CONVERSIONS

#ifndef NEED_LOCAL_WEED_PLUGIN
#include <weed/weed-plugin.h>
#ifndef NEED_LOCAL_WEED_UTILS
#include <weed/weed-utils.h> // optional
#else
#include "../../libweed/weed-utils.h" // optional
#endif
#include <weed/weed-plugin-utils.h>
#else
#include "../../libweed/weed-plugin.h"
#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional
#endif

#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/video/tracking.hpp"

using namespace cv;

typedef struct {
  int inited;
  Mat *prevgrey;
} _sdata;


static void unclamp_frame(uint8_t *data, int width, int row, int height) {
  row -= width;
  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      data[i * row + j] = y_clamped_to_unclamped(data[i * row + j]);
    }
  }
}


static uint8_t *copy_frame(const uint8_t *src, int width, int row, int height) {
  uint8_t *dst = (uint8_t *)weed_calloc(height, row);
  if (!dst) return NULL;
  weed_memcpy(dst, src, row * height);
  return dst;
}


static weed_error_t farneback_init(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_malloc(sizeof(_sdata));
  if (!sdata) return WEED_ERROR_MEMORY_ALLOCATION;

  sdata->inited = WEED_FALSE;
  weed_set_voidptr_value(inst, "plugin_internal", sdata);

  return WEED_SUCCESS;
}


static weed_error_t farneback_deinit(weed_plant_t *inst) {
  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  if (sdata) {
    if (sdata->inited) delete sdata->prevgrey;
    weed_free(sdata);
    weed_set_voidptr_value(inst, "plugin_internal", NULL);
  }

  return WEED_SUCCESS;
}


static weed_error_t farneback_process(weed_plant_t *inst, weed_timecode_t tc) {
  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t **out_channels = weed_get_out_channels(inst, NULL);

  uint8_t *src = (uint8_t *)weed_channel_get_pixel_data(in_channel);

  float *dst1 = (float *)weed_channel_get_pixel_data(out_channels[0]);
  float *dst2 = (float *)weed_channel_get_pixel_data(out_channels[1]);

  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int palette = weed_channel_get_palette(in_channel);

  int irow = weed_channel_get_stride(in_channel);
  int orow1 = weed_channel_get_stride(out_channels[0]);
  int orow2 = weed_channel_get_stride(out_channels[1]);

  register int i, j;

  Mat *cvgrey;
  Mat cvprevgrey, cvflow, srcMat, mixMat, ucMat;

  float *fptr;

  _sdata *sdata = (_sdata *)weed_get_voidptr_value(inst, "plugin_internal", NULL);

  weed_free(out_channels);

  // convert image to greyscale
  cvgrey = new Mat;

  switch (palette) {
  case WEED_PALETTE_RGB24:
    srcMat = Mat(height, width, CV_8UC3, src, irow);
    cvtColor(srcMat, *cvgrey, COLOR_RGB2GRAY);
    // This used to work at one point.
    // the only thing I can see is that the plugin complains about not finding _ZN2cv6String10deallocateEv
    // which seems odd since we are doing only matrix transforms and nothing with strings.
    break;
  case WEED_PALETTE_BGR24:
    srcMat = Mat(height, width, CV_8UC3, src, irow);
    cvtColor(srcMat, *cvgrey, COLOR_BGR2GRAY);
    break;
  case WEED_PALETTE_RGBA32:
    srcMat = Mat(height, width, CV_8UC4, src, irow);
    cvtColor(srcMat, *cvgrey, COLOR_RGB2GRAY);
    break;
  case WEED_PALETTE_BGRA32:
    srcMat = Mat(height, width, CV_8UC4, src, irow);
    cvtColor(srcMat, *cvgrey, COLOR_BGR2GRAY);
    break;
  case WEED_PALETTE_ARGB32: {
    int from_to[] = {0, 3, 1, 0, 2, 1, 3, 2}; // convert argb to rgba
    srcMat = Mat(height, width, CV_8UC4, src, irow);
    mixMat = Mat(height, width, CV_8UC4);
    mixChannels(&srcMat, 1, &mixMat, 1, from_to, 4);
    cvtColor(mixMat, *cvgrey, COLOR_RGB2GRAY);
  }
  break;
  case WEED_PALETTE_YUVA4444P:
  case WEED_PALETTE_YUV444P:
  case WEED_PALETTE_YUV422P:
  case WEED_PALETTE_YUV420P:
  case WEED_PALETTE_YVU420P:
    if (weed_channel_get_yuv_clamping(in_channel) == WEED_YUV_CLAMPING_CLAMPED) {
      srcMat = Mat(height, width, CV_8U, src, irow);
      ucMat = Mat(256, 1, CV_8U, YCL_YUCL);
      LUT(srcMat, ucMat, *cvgrey);
    } else {
      srcMat = Mat(height, width, CV_8U, src, irow);
      srcMat.copyTo(*cvgrey);
    }
    break;
  default:
    break;
  }

  if (sdata->inited == WEED_FALSE) {
    sdata->prevgrey = cvgrey;
    sdata->inited = WEED_TRUE;
    return WEED_SUCCESS;
  }

  /*
    Parameters:

        prevImg – First 8-bit single-channel input image.

        nextImg – Second input image of the same size and the same type as prevImg .

        flow – Computed flow image that has the same size as prevImg and type CV_32FC2 .

        pyrScale – Parameter specifying the image scale (<1) to build pyramids for each image.

    pyrScale=0.5 means a classical pyramid, where each next layer is twice smaller than the previous one.

        levels – Number of pyramid layers including the initial image. levels=1 means that no extra layers are
    created and only the original images are used.

        winsize – Averaging window size. Larger values increase the algorithm robustness to image noise and give
    more chances for fast motion detection, but yield more blurred motion field.

        iterations – Number of iterations the algorithm does at each pyramid level.

        polyN – Size of the pixel neighborhood used to find polynomial expansion in each pixel.
    Larger values mean that the image will be approximated with smoother surfaces,
    yielding more robust algorithm and more blurred motion field. Typically, polyN =5 or 7.

        polySigma – Standard deviation of the Gaussian that is used to smooth derivatives used as a basis for the
    polynomial expansion. For polyN=5 , you can set polySigma=1.1 .
    For polyN=7 , a good value would be polySigma=1.5 .

        flags –

        Operation flags that can be a combination of the following:
            OPTFLOW_USE_INITIAL_FLOW Use the input flow as an initial flow approximation.
            OPTFLOW_FARNEBACK_GAUSSIAN Use the Gaussian winsize times winsize filter instead of a
      box filter of the same size for optical flow estimation. Usually, this option gives
      z more accurate flow than with a box filter, at the cost of lower speed. Normally, winsize for a
      Gaussian window should be set to a larger value to achieve the same level of robustness.

      The function finds an optical flow for each prevImg pixel using the [Farneback2003] algorithm so that

      prevImg (y,x) = nextImg ( y + flow (y,x)[1], x + flow (y,x)[0])

      since we are mainly interested in nextImg, we store: -flow
      e.g.

      nextImg (y,x) = prevImg ( y - flow (y,x)[1], x - flow (y,x)[0])

  */

  calcOpticalFlowFarneback(*sdata->prevgrey, *cvgrey, cvflow, 0.5, 3, 15, 3, 5, 1.2, 0);

  delete sdata->prevgrey;
  sdata->prevgrey = cvgrey;

  // copy cvflow to float outputs

  // TODO: construct cvflow from dest
  width = cvflow.size().width;
  height = cvflow.size().height;

  irow = (cvflow.step[0] >> 3) - width;
  orow1 = (orow1 >> 2) - width;
  orow2 = (orow2 >> 2) - width;

  fptr = (float *)cvflow.data;

  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++) {
      *dst1++ = -*fptr++;
      *dst2++ = -*fptr++;
    }
    fptr += irow;
    dst1 += orow1;
    dst2 += orow2;
  }

  return WEED_SUCCESS;
}


WEED_SETUP_START(200, 200) {
  int ipalette_list[] = {WEED_PALETTE_RGB24, WEED_PALETTE_BGR24, WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32,
                         WEED_PALETTE_YUVA4444P, WEED_PALETTE_YUV444P, WEED_PALETTE_YUV422P,
                         WEED_PALETTE_YUV420P, WEED_PALETTE_YVU420P, WEED_PALETTE_END
                        };

  // define a vector output
  int opalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel",
                                  WEED_CHANNEL_REINIT_ON_SIZE_CHANGE |
                                  WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE |
                                  WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE), NULL
                                 };

  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("X values", 0),
                                   weed_channel_template_init("Y values", 0), NULL
                                  };

  weed_plant_t *filter_class = weed_filter_class_init("farneback_analyser", "salsaman", 1,
                               WEED_FILTER_PALETTES_MAY_VARY | WEED_FILTER_IS_CONVERTER, NULL,
                               farneback_init, farneback_process, farneback_deinit,
                               in_chantmpls, out_chantmpls, NULL, NULL);

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_set_int_array(in_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(ipalette_list) / 4, ipalette_list);
  weed_set_int_array(out_chantmpls[0], WEED_LEAF_PALETTE_LIST, sizeof(opalette_list) / 4, opalette_list);
  weed_set_int_array(out_chantmpls[1], WEED_LEAF_PALETTE_LIST, sizeof(opalette_list) / 4, opalette_list);

  weed_set_int_value(in_chantmpls[0], WEED_LEAF_YUV_CLAMPING, WEED_YUV_CLAMPING_UNCLAMPED);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;


