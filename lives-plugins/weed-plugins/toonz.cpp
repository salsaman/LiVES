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

//////////////////////////////////////////////////////////////////

static int package_version = 1; // version of this package
/////////////////////////////////////////////////////////////

#define NEED_PALETTE_UTILS

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

#include <iostream>
#include <array>
#include <random>

#define TWO_PI (M_PI * 2.)

enum {
  PARAMa_ANGLE,
  PARAMa_LENGTH,
  PARAMa_ATTENUATION,
}; // pencil_hatching

enum {
  PARAMb_GAMMA,
  PARAMb_EXPOSURE,
  PARAMb_GAIN,
  PARAMb_RADIUS,
  PARAMb_ATTENUATION,
  PARAMb_NUMBER,
  PARAMb_ANGLE,
}; // light_glare

enum {
  PARAMc_TIME,
  PARAMc_TIME_LIMIT,
  PARAMc_ALPHA,
  PARAMc_GAIN,
  PARAMc_BIAS,
  PARAMc_AMP0,
  PARAMc_AMP1,
  PARAMc_AMP2,
  PARAMc_AMP3,
  PARAMc_AMP4,
}; // coherent noise

enum {
  PARAMd_GAMMA,
  PARAMd_EXPOSURE,
  PARAMd_GAIN,
  PARAMd_RADIUS,
  PARAMd_LEVEL,
}; // light_bloom

enum {
  PARAMe_DISTANCE,
  PARAMe_THETA,
  PARAMe_RADIUS,
  PARAMe_COLOR,
}; // paraffin

enum {
  FILTER_LIGHT_GLARE,
  FILTER_LIGHT_BLOOM,
  FILTER_PHATCH,
  FILTER_PARAFFIN,
};

#include <stdlib.h>
#include <stdio.h>

#include <chrono>
#include <memory>

#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"

using namespace cv;

#define DEBUG_PRINT(a) std::cerr << a << std::endl;

//////////////////////////////////////////////////////////

namespace tnzu {
template <typename T>
struct opencv_type_traits;

template <>
struct opencv_type_traits<float> {
  static int const value = CV_32FC1;
};

template <>
struct opencv_type_traits<cv::Vec2f> {
  static int const value = CV_32FC2;
};

template <>
struct opencv_type_traits<cv::Vec3f> {
  static int const value = CV_32FC3;
};

template <>
struct opencv_type_traits<cv::Vec4f> {
  static int const value = CV_32FC4;
};

template <>
struct opencv_type_traits<double> {
  static int const value = CV_64FC1;
};

template <>
struct opencv_type_traits<cv::Vec2d> {
  static int const value = CV_64FC2;
};

template <>
struct opencv_type_traits<cv::Vec3d> {
  static int const value = CV_64FC3;
};

template <>
struct opencv_type_traits<cv::Vec4d> {
  static int const value = CV_64FC4;
};


// snp (salt and pepper) noise
template <typename VecT>
cv::Mat make_snp_noise(cv::Size const size, float const low, float const high) {
  cv::Mat retval = cv::Mat::zeros(size, tnzu::opencv_type_traits<VecT>::value);
  cv::randu(retval, low, high);
  return retval;
}


// Perlin noise
template <typename VecT, std::size_t Sz>
cv::Mat make_perlin_noise(cv::Size const size,
                          std::array<float, Sz> const &amp) {
  cv::Mat retval = cv::Mat::zeros(size, tnzu::opencv_type_traits<VecT>::value);

  for (std::size_t i = 0; i < Sz; ++i) {
    float const range = amp[i];
    cv::Size const octave_size(2 << i, 2 << i);

    cv::Mat field = tnzu::make_snp_noise<VecT>(octave_size, -range, range);

    float const scale = std::max(float(size.width) / octave_size.width,
                                 float(size.height) / octave_size.height);
    cv::resize(field, field, size, scale, scale, cv::INTER_CUBIC);
    retval += field;
  }

  return retval;
}



void generate_bloom(cv::Mat &img, int level, int radius) {
  std::vector<cv::Mat> dst(level + 1);

  cv::Size const ksize(radius * 2 + 1, radius * 2 + 1);

  cv::Mat tmp;
  int i;
  cv::Size size = img.size();
  for (i = 0; i <= level;) {
    if (i) {
      cv::resize(img, tmp, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
      img = tmp;
      size = img.size();
    }

    cv::GaussianBlur(img, dst[i], ksize, 0.0);

    ++i;

    if ((size.width <= 1) || (size.height <= 1)) {
      break;
    }
  }

  for (--i; i > 0; --i) {
    cv::resize(dst[i], tmp, dst[i - 1].size());
    dst[i - 1] += tmp;
  }
  img = dst[0];
}


template <typename T, typename S>
inline T normalize_cast(S const value) {
  return cv::saturate_cast<T>(value * std::numeric_limits<T>::max());
}


// convert sRGB color space to power space
template <typename T>
inline T to_linear_color_space(T nonlinear_color, T exposure, T gamma) {
  return -std::log(T(1) - std::pow(nonlinear_color, gamma)) / exposure;
}

// convert power space to sRGB color space
template <typename T>
inline T to_nonlinear_color_space(T linear_color, T exposure, T gamma) {
  return std::pow(T(1) - std::exp(-exposure * linear_color), T(1) / gamma);
}

template <std::size_t BitDepth, typename T = float>
class linear_color_space_converter {
public:
  using this_type = linear_color_space_converter<BitDepth, T>;
  static std::size_t const Size = 1 << BitDepth;

public:
  inline linear_color_space_converter(T exposure, T gamma)
    : table_(new T[Size]) {
    T const scale = T(1) / Size;
    for (int i = 0; i < (int)Size; i++) {
      table_[i] =
        tnzu::to_linear_color_space((i + T(0.5)) * scale, exposure, gamma);
    }
  }

  inline T operator[](int value) const {
    return table_[value];
  }

private:
  std::unique_ptr<T[]> table_;
};

}

///////////////////////////////////////////////////////////////////////////////////

template <typename VecT>
int phatch_kernel(Mat &in, Mat &retimg, int palette, weed_plant_t **in_params) {
  int const type = retimg.type();
  Size const size = retimg.size();

  float const angle = (float)weed_param_get_value_int(in_params[PARAMa_ANGLE]);
  float const length = (float)weed_param_get_value_double(in_params[PARAMa_LENGTH]) * size.height;
  float const attenuation = (float)weed_param_get_value_double(in_params[PARAMa_ATTENUATION]);

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
      uchar const *g = grayscale.ptr<uchar const>(y);
      uchar *n = noise.ptr<uchar>(y);
      for (int x = 0; x < size.width; ++x) {
        std::bernoulli_distribution rbern(g[x] * norm_const);
        n[x] = rbern(engine) ? std::numeric_limits<uchar>::max() : 0;
      }
    }
  }

  // generate pencil drawings
  Point2f const dir(std::cos(angle), std::sin(angle));
  for (int y = 0; y < size.height; ++y) {
    VecT const *src = in.ptr<VecT>(y);
    VecT *dst = retimg.ptr<VecT>(y);

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
            uchar const sample = *reinterpret_cast<uchar const *>(*it);
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
            uchar const sample = *reinterpret_cast<uchar const *>(*it);
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
        // RGB24 or BGR24
        dst[x] = VecT(g, g, g);
        break;
      }
    }
  }
  return 0;
}


template <typename VecT>
int lglare_kernel(Mat &in, Mat &retimg, int palette, weed_plant_t **in_params) {
  using value_type = typename VecT::value_type;

  int psize;

  Size const size = retimg.size();

  // init parameters
  float const gamma = (float)weed_param_get_value_double(in_params[PARAMb_GAMMA]);
  float const exposure = (float)weed_param_get_value_double(in_params[PARAMb_EXPOSURE]);
  float const gain = (float)weed_param_get_value_double(in_params[PARAMb_GAIN]);

  float const radius = (float)weed_param_get_value_double(in_params[PARAMb_RADIUS]) * size.height;
  float const attenuation = (float)weed_param_get_value_double(in_params[PARAMb_ATTENUATION]);

  int const number = weed_param_get_value_int(in_params[PARAMb_NUMBER]);
  int const angle = weed_param_get_value_int(in_params[PARAMb_ANGLE]);

  Mat src(size, CV_32FC3);

  psize = pixel_size(palette);

  Size const local_size = in.size();
  Mat local(src, Rect(0, 0, local_size.width, local_size.height)); // not sure that this is right...

  {
    tnzu::linear_color_space_converter<sizeof(value_type) * 8> converter(exposure, gamma);

    for (int y = 0; y < local_size.height; ++y) {
      VecT const *s = in.ptr<VecT const>(y);
      cv::Vec3f *d = local.ptr<cv::Vec3f>(y);
      for (int x = 0; x < local_size.width; ++x) {
        d[x] = cv::Vec3f(converter[s[x][0]], converter[s[x][1]],
                         converter[s[x][2]]);
      }
    }
  }

  // generate glare kernel

  int const fsize = radius * 2 + 1;
  cv::Mat kernel = cv::Mat::zeros(cv::Size(fsize, fsize), CV_32F);
  if (radius > 0) {
    float energy = 0.0f;
    cv::Point2f const center(fsize * 0.5f, fsize * 0.5f);
    for (int i = 0; i < number; i++) {
      float const theta = angle + i * float(2 * M_PI) / number;
      float const dx = radius * std::cos(theta);
      float const dy = radius * std::sin(theta);

      cv::LineIterator it(kernel, center, center + cv::Point2f(dx, dy));
      float a = 1.0f;
      for (int i = 0; i < it.count; ++i, ++it, a *= attenuation) {
        *reinterpret_cast<float *>(*it) = a;
        energy += a;
      }
    }
    if (energy > 0.0f) {
      kernel *= gain / energy;
    }
  } else {
    kernel = cv::Scalar(1);
  }

  // generate glare

  cv::filter2D(src, src, -1, kernel);

  for (int y = 0; y < size.height; ++y) {
    cv::Vec3f const *s = src.ptr<cv::Vec3f>(y);
    VecT *d = retimg.ptr<VecT>(y);
    for (int x = 0; x < size.width; ++x) {
      if (psize == 4) {
        Vec4f const sbgra(
          tnzu::to_nonlinear_color_space(s[x][0], exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][1], exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][2], exposure, gamma), 1.0f);

        for (int c = 0; c < 4; ++c) {
          d[x][c] = tnzu::normalize_cast<uchar>(sbgra[c]);
        }
      } else {
        Vec3f const sbgr(
          tnzu::to_nonlinear_color_space(s[x][0], exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][1], exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][2], exposure, gamma));

        for (int c = 0; c < 3; ++c) {
          d[x][c] = tnzu::normalize_cast<uchar>(sbgr[c]);
        }
      }
    }
  }

  return 0;
}


template <typename VecT>
int lbloom_kernel(Mat &in, Mat &retimg, int palette, weed_plant_t **in_params) {
  using value_type = typename VecT::value_type;
  int psize;

  Size const size = retimg.size();

  // init parameters
  float const gamma = (float)weed_param_get_value_double(in_params[PARAMd_GAMMA]);
  float const exposure = (float)weed_param_get_value_double(in_params[PARAMd_EXPOSURE]);
  float const gain = (float)weed_param_get_value_double(in_params[PARAMd_GAIN]);

  int const radius = weed_param_get_value_int(in_params[PARAMd_RADIUS]);
  int const level = weed_param_get_value_int(in_params[PARAMd_LEVEL]);

  Mat src(size, CV_32FC3);
  psize = pixel_size(palette);

  // transform color space
  {
    tnzu::linear_color_space_converter<sizeof(value_type) * 8> converter(
      exposure, gamma);

    Size const local_size = in.size();
    Mat local(src, Rect(0, 0, local_size.width, local_size.height)); // not sure that this is right...
    //cv::Size const local_size = args.size(PORT_INPUT);
    //cv::Mat local(src, args.rect(PORT_INPUT));

    for (int y = 0; y < local_size.height; ++y) {
      VecT const *s = in.ptr<VecT>(y);
      cv::Vec3f *d = local.ptr<cv::Vec3f>(y);
      for (int x = 0; x < local_size.width; ++x) {
        d[x] = cv::Vec3f(converter[s[x][0]], converter[s[x][1]],
                         converter[s[x][2]]);
      }
    }
  }

  // generate bloom
  tnzu::generate_bloom(src, level, radius);

  // transform color space
  float const scale = gain;

  for (int y = 0; y < size.height; ++y) {
    cv::Vec3f const *s = src.ptr<cv::Vec3f>(y);
    VecT *d = retimg.ptr<VecT>(y);
    for (int x = 0; x < size.width; ++x) {
      if (psize == 4) {
        Vec4f const sbgra(
          tnzu::to_nonlinear_color_space(s[x][0] * scale, exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][1] * scale, exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][2] * scale, exposure, gamma), 1.0f);

        for (int c = 0; c < 4; ++c) {
          d[x][c] = tnzu::normalize_cast<uchar>(sbgra[c]);
        }
      } else {
        Vec3f const sbgr(
          tnzu::to_nonlinear_color_space(s[x][0] * scale, exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][1] * scale, exposure, gamma),
          tnzu::to_nonlinear_color_space(s[x][2] * scale, exposure, gamma));

        for (int c = 0; c < 3; ++c) {
          d[x][c] = tnzu::normalize_cast<uchar>(sbgr[c]);
        }
      }
    }
  }
  return 0;
}


template <typename VecT>
int paraffin_kernel(Mat &in, Mat &retimg, int palette, weed_plant_t **in_params) {
  using value_type = typename VecT::value_type;

  int psize = 4;

  Size const size = retimg.size();

  //
  // Params
  //
  // geometry
  float const d = (float)weed_param_get_value_double(in_params[PARAMe_DISTANCE]) * size.height;
  float const a = (float)weed_param_get_value_int(in_params[PARAMe_THETA]) / 360. * TWO_PI;

  int const s = (int)((float)weed_param_get_value_double(in_params[PARAMe_RADIUS]) * size.height * 0.5) * 2 + 1;

  double *cvals = weed_param_get_array_double(in_params[PARAMe_COLOR], NULL);

  float r, g = cvals[1], b;

  if (palette == WEED_PALETTE_BGR24 || palette == WEED_PALETTE_BGRA32) {
    r = cvals[2];
    b = cvals[0];
  } else {
    r = cvals[0];
    b = cvals[2];
  }

  weed_free(cvals);

  // define paraffin shadow
  cv::Mat shadow(size, CV_32FC3, cv::Scalar(1, 1, 1));

  // draw parafffin
  {
    std::array<cv::Point, 4> pts;

    cv::Point2f const o(size.width * 0.5f, size.height * 0.5f);
    float const l = std::sqrt(o.x * o.x + o.y * o.y) + 1;
    float const s = std::sin(a);
    float const c = std::cos(a);

    pts[0] = cv::Point(static_cast<int>(o.x + l * c + (d + l) * s),
                       static_cast<int>(o.y - l * s + (d + l) * c));
    pts[1] = cv::Point(static_cast<int>(o.x + l * c + (d - l) * s),
                       static_cast<int>(o.y - l * s + (d - l) * c));
    pts[2] = cv::Point(static_cast<int>(o.x - l * c + (d - l) * s),
                       static_cast<int>(o.y + l * s + (d - l) * c));
    pts[3] = cv::Point(static_cast<int>(o.x - l * c + (d + l) * s),
                       static_cast<int>(o.y + l * s + (d + l) * c));

    cv::fillConvexPoly(shadow, pts.data(), static_cast<int>(pts.size()),
                       cv::Scalar(b, g, r));
  }

  // blur bar
  cv::GaussianBlur(shadow, shadow, cv::Size(s, s), 0.0);

  // init color table
  tnzu::linear_color_space_converter<sizeof(value_type) * 8> converter(1.0f,
      2.2f);

  // add incident light on linear color space

  for (int y = 0; y < size.height; y++) {
    cv::Vec3f const *s = shadow.ptr<cv::Vec3f>(y);
    VecT *d = retimg.ptr<VecT>(y);
    for (int x = 0; x < size.width; x++) {
      if (psize == 4) {
        cv::Vec4f const sbgra(tnzu::to_nonlinear_color_space(
                                converter[d[x][0]] * s[x][0], 1.0f, 2.2f),
                              tnzu::to_nonlinear_color_space(
                                converter[d[x][1]] * s[x][1], 1.0f, 2.2f),
                              tnzu::to_nonlinear_color_space(
                                converter[d[x][2]] * s[x][2], 1.0f, 2.2f),
                              1.0f);
        for (int c = 0; c < 4; ++c) {
          d[x][c] = tnzu::normalize_cast<value_type>(sbgra[c]);
        }
      } else {
        cv::Vec3f const sbgr(tnzu::to_nonlinear_color_space(
                               converter[d[x][0]] * s[x][0], 1.0f, 2.2f),
                             tnzu::to_nonlinear_color_space(
                               converter[d[x][1]] * s[x][1], 1.0f, 2.2f),
                             tnzu::to_nonlinear_color_space(
                               converter[d[x][2]] * s[x][2], 1.0f, 2.2f));
        for (int c = 0; c < 3; ++c) {
          d[x][c] = tnzu::normalize_cast<value_type>(sbgr[c]);
        }
      }
    }
  }
  return 0;
}


static weed_error_t common_process(weed_plant_t *inst, weed_timecode_t tc, int filter_type) {
  Mat srcMat, mixMat, destMat;

  weed_plant_t *in_channel = weed_get_in_channel(inst, 0);
  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);

  weed_plant_t **in_params = weed_get_in_params(inst, NULL);

  uint8_t *src = (uint8_t *)weed_channel_get_pixel_data(in_channel);
  uint8_t *dst = (uint8_t *)weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(in_channel);
  int height = weed_channel_get_height(in_channel);
  int palette = weed_channel_get_palette(in_channel);

  int irow = weed_channel_get_stride(in_channel);
  int orow = weed_channel_get_stride(out_channel);

  int psize = pixel_size(palette);

  switch (palette) {
  case WEED_PALETTE_RGB24:
  case WEED_PALETTE_BGR24:
    srcMat = mixMat = Mat(height, width, CV_8UC3, src, irow);
    destMat = Mat(height, width, CV_8UC3, dst, orow);
    break;
  case WEED_PALETTE_BGRA32:
  case WEED_PALETTE_RGBA32:
    srcMat = mixMat = Mat(height, width, CV_8UC4, src, irow);
    destMat = Mat(height, width, CV_8UC4, dst, orow);
    break;
  case WEED_PALETTE_ARGB32: {
    int from_to[] = {0, 3, 1, 0, 2, 1, 3, 2}; // convert src argb to rgba
    srcMat = Mat(height, width, CV_8UC4, src, irow);
    mixChannels(&srcMat, 1, &mixMat, 1, from_to, 4);
    destMat = Mat(height, width, CV_8UC4, dst, orow);
  }
  break;
  default:
    break;
  }

  switch (filter_type) {
  case FILTER_LIGHT_BLOOM:
    if (psize == 4)
      lbloom_kernel<Vec4b>(mixMat, destMat, palette, in_params);
    else
      lbloom_kernel<Vec3b>(mixMat, destMat, palette, in_params);
    break;
  case FILTER_LIGHT_GLARE:
    if (psize == 4)
      lglare_kernel<Vec4b>(mixMat, destMat, palette, in_params);
    else
      lglare_kernel<Vec3b>(mixMat, destMat, palette, in_params);
    break;
  case FILTER_PHATCH:
    phatch_kernel<Vec4b>(mixMat, destMat, palette, in_params);
    break;
  case FILTER_PARAFFIN:
    srcMat.copyTo(destMat);
    if (psize == 4)
      paraffin_kernel<Vec4b>(mixMat, destMat, palette, in_params);
    else
      paraffin_kernel<Vec3b>(mixMat, destMat, palette, in_params);
    break;
  default:
    break;
  }

  weed_free(in_params);

  return WEED_SUCCESS;
}


//////////////////////////////////////////

static weed_error_t lbloom_process(weed_plant_t *inst, weed_timecode_t tc) {
  return common_process(inst, tc, FILTER_LIGHT_BLOOM);
}

static weed_error_t lglare_process(weed_plant_t *inst, weed_timecode_t tc) {
  return common_process(inst, tc, FILTER_LIGHT_GLARE);
}

static weed_error_t phatch_process(weed_plant_t *inst, weed_timecode_t tc) {
  return common_process(inst, tc, FILTER_PHATCH);
}

static weed_error_t paraffin_process(weed_plant_t *inst, weed_timecode_t tc) {
  return common_process(inst, tc, FILTER_PARAFFIN);
}


///////////////////////////////////////

int cnoise_compute(Mat &retimg, weed_plant_t **in_params, double sec) {
  try {
    cv::Size const size = retimg.size();

    int const time = weed_param_get_value_int(in_params[PARAMc_TIME]);
    int const time_limit = weed_param_get_value_int(in_params[PARAMc_TIME_LIMIT]);
    float const alpha = (float)weed_param_get_value_double(in_params[PARAMc_ALPHA]);
    float const gain = (float)weed_param_get_value_double(in_params[PARAMc_GAIN]);
    float const bias = (float)weed_param_get_value_double(in_params[PARAMc_BIAS]);

    std::array<float, 5> const amp = {
      (float)weed_param_get_value_double(in_params[PARAMc_AMP0]),
      (float)weed_param_get_value_double(in_params[PARAMc_AMP1]),
      (float)weed_param_get_value_double(in_params[PARAMc_AMP2]),
      (float)weed_param_get_value_double(in_params[PARAMc_AMP3]),
      (float)weed_param_get_value_double(in_params[PARAMc_AMP4])
    };

    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    cv::theRNG().state = seed;

    int ntimes =
      (time ? time - 1 : std::max(1, (int)(sec / 25.))) % (time_limit * 2);
    if (ntimes >= time_limit) {
      ntimes = time_limit * 2 - ntimes;
    }
    ++ntimes;

    // generate time-Coherent perlin noise
    cv::Mat field = tnzu::make_perlin_noise<float>(size, amp);
    for (int t = 0; t <= ntimes; ++t) {
      cv::Mat next = tnzu::make_perlin_noise<float>(size, amp);
      field *= alpha;
      field += next * (1 - alpha);
    }
    field *= gain / 5;
    field += bias;

    for (int y = 0; y < size.height; ++y) {
      float *dst = retimg.ptr<float>(y);
      float const *src = field.ptr<float>(y);
      for (int x = 0; x < size.width; ++x) {
        dst[x] = src[x];
      }
    }

    return 0;
  } catch (cv::Exception const &e) {
    DEBUG_PRINT(e.what());
    return 1;
  }
}


static weed_error_t cnoise_process(weed_plant_t *inst, weed_timecode_t tc) {
  Mat destMat;

  weed_plant_t *out_channel = weed_get_out_channel(inst, 0);
  weed_plant_t **in_params = weed_get_in_params(inst, NULL);

  float *dst = (float *)weed_channel_get_pixel_data(out_channel);

  int width = weed_channel_get_width(out_channel);
  int height = weed_channel_get_height(out_channel);

  int orow = weed_channel_get_stride(out_channel);

  destMat = Mat(height, width, CV_32FC1, dst, orow);

  cnoise_compute(destMat, in_params, (double)tc / (double)WEED_TICKS_PER_SECOND);

  weed_free(in_params);

  return WEED_SUCCESS;
}


///////////////////////////////

WEED_SETUP_START(200, 200) {
  int palette_list[] = ALL_RGB_PALETTES;
  int palette_list4[] = {WEED_PALETTE_RGBA32, WEED_PALETTE_BGRA32, WEED_PALETTE_ARGB32, WEED_PALETTE_END};

  int opalette_list[] = {WEED_PALETTE_AFLOAT, WEED_PALETTE_END};

  weed_plant_t *in_chantmpls[] = {weed_channel_template_init("in channel", 0), NULL};
  weed_plant_t *out_chantmpls[] = {weed_channel_template_init("out channel", 0), NULL};

  weed_plant_t *in_paramsa[] = {weed_integer_init("angle", "_Angle", 0, 0, 360), weed_float_init("length", "_Length", 0.01, 0., 1.),
                                weed_float_init("attenuation", "A_ttenuation", 0.9, 0., 1.), NULL
                               };

  weed_plant_t *in_paramsb[] = {weed_float_init("gamma", "_Gamma", 2.2, 0.1, 5.0),
                                weed_float_init("exposure", "_Exposure", 1.0, 0.125, 8.),
                                weed_float_init("gain", "Ga_in", 1., 0.1, 10.0), weed_float_init("radius", "_Radius", .1, 0.01, 1.),
                                weed_float_init("attenuation", "A_ttenuation", .9, 0.001, .999), weed_integer_init("number", "_Number", 6, 2, 10),
                                weed_integer_init("angle", "_Angle", 15, 0, 180), NULL
                               };

  weed_plant_t *in_paramsc[] = {weed_integer_init("time", "_Time", 0, 0, 1500), weed_integer_init("time_limit", "Time _Limit", 8, 2, 250),
                                weed_float_init("alpha", "_Alpha", .8, 0., 1.), weed_float_init("gain", "_Gain", 1., 0., 1.),
                                weed_float_init("bias", "_Bias", .5, 0., 1.),
                                weed_float_init("amp0", "Amp _0", 1., 0., 1.),
                                weed_float_init("amp1", "Amp _1", .8, 0., 1.),
                                weed_float_init("amp2", "Amp _2", .6, 0., 1.),
                                weed_float_init("amp3", "Amp _3", .4, 0., 1.),
                                weed_float_init("amp4", "Amp _4", .2, 0., 1.),
                                NULL
                               };

  weed_plant_t *in_paramsd[] = {weed_float_init("gamma", "_Gamma", 2.2, 0.1, 5.0),
                                weed_float_init("exposure", "_Exposure", 1.0, 0.125, 8.),
                                weed_float_init("gain", "Ga_in", 1., 0.1, 10.0), weed_integer_init("radius", "_Radius", 6, 1, 32),
                                weed_integer_init("level", "_Level", 4, 0, 10), NULL
                               };

  weed_plant_t *in_paramse[] = {weed_float_init("distance", "_Distance", -1., -1.5, 1.5),
                                weed_integer_init("theta", "_Theta", 40, -180, 180),
                                weed_float_init("radius", "_Radius", .1, 0., 1.),
                                weed_colRGBd_init("color", "_Color", 0., 0., 0.),
                                NULL
                               };

  weed_plant_t *filter_class, *gui;

  // coherent noise
  filter_class = weed_filter_class_init("Toonz: Coherent Noise", "DWANGO co.", 1, 0, opalette_list,
                                        NULL, cnoise_process, NULL, NULL, out_chantmpls, in_paramsc, NULL);

  weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
  weed_set_string_value(filter_class, "url", "http://dwango.co.jp");
  weed_set_string_value(filter_class, "copyright", "DWANGO 2016, salsaman 2016");
  weed_set_string_value(filter_class, "license", "BSD 3-clause");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  // light bloom
  filter_class = weed_filter_class_init("Toonz: Light Bloom", "DWANGO co.", 1, 0, palette_list,
                                        NULL, lbloom_process, NULL, in_chantmpls, out_chantmpls, in_paramsd, NULL);

  weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
  weed_set_string_value(filter_class, "url", "http://dwango.co.jp");
  weed_set_string_value(filter_class, "copyright", "DWANGO 2016, salsaman 2016");
  weed_set_string_value(filter_class, "license", "BSD 3-clause");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  // light glare
  filter_class = weed_filter_class_init("Toonz: Light Glare", "DWANGO co.", 1, 0, palette_list,
                                        NULL, lglare_process, NULL, in_chantmpls, out_chantmpls, in_paramsb, NULL);

  weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
  weed_set_string_value(filter_class, "url", "http://dwango.co.jp");
  weed_set_string_value(filter_class, "copyright", "DWANGO 2016, salsaman 2016");
  weed_set_string_value(filter_class, "license", "BSD 3-clause");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  // paraffin
  filter_class = weed_filter_class_init("Toonz: Paraffin", "DWANGO co.", 1, 0, palette_list,
                                        NULL, paraffin_process, NULL, in_chantmpls, out_chantmpls, in_paramse, NULL);

  weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
  weed_set_string_value(filter_class, "url", "http://dwango.co.jp");
  weed_set_string_value(filter_class, "copyright", "DWANGO 2016, salsaman 2016");
  weed_set_string_value(filter_class, "license", "BSD 3-clause");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  // pencil hatching
  filter_class = weed_filter_class_init("Toonz: Pencil Hatching", "DWANGO co.", 1, 0, palette_list4,
                                        NULL, phatch_process, NULL, in_chantmpls, out_chantmpls, in_paramsa, NULL);

  gui = weed_paramtmpl_get_gui(in_paramsa[PARAMa_ANGLE]);
  weed_set_boolean_value(gui, WEED_LEAF_WRAP, WEED_TRUE);

  weed_set_string_value(filter_class, WEED_LEAF_EXTRA_AUTHORS, "salsaman");
  weed_set_string_value(filter_class, "url", "http://dwango.co.jp");
  weed_set_string_value(filter_class, "copyright", "DWANGO 2016, salsaman 2016");
  weed_set_string_value(filter_class, "license", "BSD 3-clause");

  weed_plugin_info_add_filter_class(plugin_info, filter_class);

  weed_plugin_set_package_version(plugin_info, package_version);
}
WEED_SETUP_END;

