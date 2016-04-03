// phatch_analyser.cpp
// weed plugin
// (c) G. Finch (salsaman) 2016
//

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

/*
Copyright (c) 2016, DWANGO Co., Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


  
#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h"
#include "../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

enum {
  PARAM_ANGLE,
  PARAM_LENGTH,
  PARAM_ATTENUATION,
};

#include <stdlib.h>
#include <stdio.h>

#include <chrono>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

template <typename VecT> 
int kernel(Mat& in, Mat& retimg, int palette, weed_plant_t **in_params) {
  
  int error;
  
  int const type = retimg.type();
  Size const size = retimg.size();

  float const angle = weed_get_int_value(in_params[PARAM_ANGLE],"value",&error);
  float const length = weed_get_double_value(in_params[PARAM_LENGTH],"value",&error);
  float const attenuation = weed_get_double_value(in_params[PARAM_ATTENUATION],"value",&error);

  // snp noise based on grayscale
  Mat noise(size, CV_MAKETYPE(CV_MAT_DEPTH(type), 1));
  {

    // grascaling
    Mat grayscale;
    switch (palette) {
    case WEED_PALETTE_ARGB32:
      // A was moved to end
    case WEED_PALETTE_RGBA32:
      cvtColor(in, grayscale, COLOR_RGBA2GRAY);
      break;
    case WEED_PALETTE_BGRA32:
      cvtColor(in, grayscale, COLOR_BGRA2GRAY);
      break;
    case WEED_PALETTE_RGB24:
      cvtColor(in, grayscale, COLOR_RGB2GRAY);
      break;
    case WEED_PALETTE_BGR24:
      cvtColor(in, grayscale, COLOR_BGR2GRAY);
      break;
    default:
      break;
    }
      
    
    float const norm_const = 1.0f / std::numeric_limits<uchar>::max();

    // generate snp noise
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 engine(seed);
    for (int y = 0; y < size.height; ++y) {
      uchar const* g = grayscale.ptr<uchar const>(y);
      uchar* n = noise.ptr<uchar>(y);
      for (int x = 0; x < size.width; ++x) {
        std::bernoulli_distribution rbern(g[x] * norm_const);
        n[x] = rbern(engine) ? std::numeric_limits<uchar>::max() : 0;
      }
    }
  }

  // generate pencil drawings
  Point2f const dir(std::cos(angle), std::sin(angle));
  for (int y = 0; y < size.height; ++y) {
    VecT const* src = in.ptr<VecT>(y);
    VecT* dst = retimg.ptr<VecT>(y);

    for (int x = 0; x < size.width; ++x) {
      // line integral convolution
      Point const org(x, y);  // origin
      float gray = 0.0f;
      float sum = 0.0f;
      {
        // minus
        Point const pt(static_cast<int>(x - length * dir.x),
                           static_cast<int>(y - length * dir.y));
        LineIterator it(noise, org, pt, 4);
        if (it.count > 0) {
          float rho = 1.0f;
          for (int i = 0; i < it.count; ++i, ++it, rho *= attenuation) {
            uchar const sample = *reinterpret_cast<uchar const*>(*it);
            gray += rho * sample;
            sum += rho;
          }
        }
      }
      {
        // plus
        Point const pt(static_cast<int>(x + length * dir.x),
                           static_cast<int>(y + length * dir.y));
        LineIterator it(noise, org, pt, 4);
        if (it.count > 0) {
          float rho = 1.0f;
          for (int i = 0; i < it.count; ++i, ++it, rho *= attenuation) {
            uchar const sample = *reinterpret_cast<uchar const*>(*it);
            gray += rho * sample;
            sum += rho;
          }
        }
      }
      if (sum > 0) {
        gray /= sum;
      }
      uchar const g = saturate_cast<uchar>(gray);
      switch (palette) {
      case WEED_PALETTE_ARGB32:
	dst[x] = VecT(src[x][3], g, g, g);
	break;
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGRA32:
	dst[x] = VecT(g, g, g, src[x][3]);
	break;
      default:
	dst[x] = VecT(g, g, g);
	break;
      }
    }
  }
  return 0;
}



int phatch_process (weed_plant_t *inst, weed_timecode_t tc) {
  int error;

  Mat srcMat, mixMat, destMat;

  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  uint8_t *src=(uint8_t *)weed_get_voidptr_value(in_channel,"pixel_data",&error);
  float *dst=(float *)weed_get_voidptr_value(out_channel,"pixel_data",&error);

  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int palette=weed_get_int_value(in_channel,"current_palette",&error);

  int irow=weed_get_int_value(in_channel,"rowstrides",&error);
  int orow=weed_get_int_value(out_channel,"rowstrides",&error);

  int psize=4;

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    mixMat=Mat(height,width,CV_8UC3,src,irow);
    destMat=Mat(height,width,CV_8UC3,dst,orow);
    psize=3;
    break;
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_RGBA32:
    mixMat=Mat(height,width,CV_8UC4,src,irow);
    destMat=Mat(height,width,CV_8UC4,dst,orow);
    break;
  case WEED_PALETTE_ARGB32:
    {
      int from_to[]={0,3,1,0,2,1,3,2}; // convert src argb to rgba
      srcMat=Mat(height,width,CV_8UC4,src,irow);
      mixChannels(&srcMat,1,&mixMat,1,from_to,4);
      destMat=Mat(height,width,CV_8UC4,dst,orow);
    }
    break;
  default:
    break;
  }

  if (psize==4) 
    kernel<Vec4b>(mixMat,destMat,palette,in_params);
  else
    kernel<Vec3b>(mixMat,destMat,palette,in_params);
  
  weed_free(in_params);
  
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {

    int palette_list[]={WEED_PALETTE_RGB24,WEED_PALETTE_BGR24,
      WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};
    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel",0,palette_list),NULL};

    weed_plant_t *in_params[]={weed_integer_init("angle","_Angle",0,0,360),weed_float_init("length","_Length",0.01,0.,1.),
			       weed_float_init("attenuation","A_ttenuation",0.9,0.,1.),NULL};

    
    weed_plant_t *filter_class=weed_filter_class_init("Toonz: pencil_hatching","DWANGO co.",1,0,NULL,
						      &phatch_process,NULL,
						      in_chantmpls,out_chantmpls,in_params,NULL);

    weed_set_boolean_value(in_params[PARAM_ANGLE],"wrap",WEED_TRUE);
    
    weed_set_string_value(filter_class,"extra_authors","salsaman");

    weed_set_string_value(filter_class,"copyright","DWANGO 2016, salsaman 2016");
    
    weed_set_string_value(filter_class,"license","BSD 3-clause");
    
    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);

  }
  return plugin_info;
}

