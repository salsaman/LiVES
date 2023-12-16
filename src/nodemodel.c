// nodemodel.c
// LiVES
// (c) G. Finch 2023 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// node modelling - this is used to model the effect chain before it is appplied
// in order to optimise the operational sequence and to compare hypothetical alternatives

// in the current implementation, the focus is on optimising the sequence of palette convesrsion
// and resizes
// as layers pass through the effects chain from sources to display sink

#include "main.h"
#include "nodemodel.h"
#include "effects-weed.h"
#include "effects.h"
#include "cvirtual.h"
#include "diagnostics.h"

static volatile int nplans = 0;
pthread_mutex_t nplans_mutex = PTHREAD_MUTEX_INITIALIZER;

static int allpals[] = {_ALL_24BIT_PALETTES, _ALL_32BIT_PALETTES, WEED_PALETTE_END};
static int n_allpals = 0;

#define ANN_ERR_THRESH 0.05
#define ANN_GEN_LIMIT 50

glob_timedata_t *glob_timing = NULL;

static double ztime;

// still to do:

// elsewhere:
// include provision  for deinterlace, subtitles
// measure process_time
// create model deltas, bypass nodes


LIVES_GLOBAL_INLINE double get_cycle_avg_time(double *dets) {
  double ret = 0.;
  if (glob_timing) {
    pthread_mutex_lock(&glob_timing->upd_mutex);
    ret = glob_timing->avg_duration;
    if (dets) {
      dets[0] = glob_timing->curr_cpuload;
      dets[1] = glob_timing->last_cyc_duration;
      dets[2] = glob_timing->tgt_duration;
    }
    pthread_mutex_unlock(&glob_timing->upd_mutex);
  }
  return ret;
}


static inst_node_t *desc_and_add_steps(inst_node_t *, exec_plan_t *);
static inst_node_t *desc_and_align(inst_node_t *, lives_nodemodel_t *);
static inst_node_t *desc_and_reinit(inst_node_t *n);

static lives_result_t get_op_order(int out_width, int out_height, int in_width, int in_height,
                                   int flags, int outpl, int inpl,
                                   int out_gamma_type, int in_gamma_type, int *op_order);

static void reset_model(lives_nodemodel_t *nodemodel) {
  for (LiVESList *list = nodemodel->nodes; list; list = list->next) {
    inst_node_t *n = (inst_node_t *)list->data;
    if (n) {
      int i;
      for (i = 0; i < n->n_inputs; i++) n->inputs[i]->flags &= ~ NODEFLAG_PROCESSED;
      for (i = 0; i < n->n_outputs; i++) n->outputs[i]->flags &= ~ NODEFLAG_PROCESSED;
      n->flags &= ~ NODEFLAG_PROCESSED;
    }
  }
}


LIVES_LOCAL_INLINE boolean skip_ctmpl(weed_filter_t *filter, weed_chantmpl_t *ctmpl) {
  // returns TRUE if chentmpl is audio, is disabled. or is alpha only channel
  return weed_chantmpl_is_audio(ctmpl) || weed_chantmpl_is_disabled(ctmpl)
    || !has_non_alpha_palette(ctmpl, filter);
}


static weed_chantmpl_t *get_nth_chantmpl(inst_node_t *n, int cn, int *counts, int direction) {
  // return the nth in or out chantmpl for filter.
  // ignoring audio chans, disabled chans, alpha chans
  // for some filters we have repeatable chantmpls, if counts is supplied, then a value > 1
  // indicates the number of copies of that chantmpl
  //
  weed_filter_t *filter = (weed_filter_t *)n->model_for;
  weed_chantmpl_t **ctmpls, *ctmpl = NULL;
  int nctmpls, i;

  if (direction == LIVES_INPUT) ctmpls = weed_filter_get_in_chantmpls(filter, &nctmpls);
  else ctmpls = weed_filter_get_out_chantmpls(filter, &nctmpls);
  if (!ctmpls) return NULL;

  for (i = 0; i < nctmpls; i++) {
    ctmpl = ctmpls[i];
    if (skip_ctmpl(filter, ctmpl)) continue;
    if (counts) cn -= counts[i];
    else cn--;
    if (cn < 0) break;
  }
  lives_free(ctmpls);
  return ctmpl;
}


static int count_ctmpls(weed_filter_t *filter, int direction) {
  // return the nth in or out chantmpl for filter.
  // ignoring audio chans, disabled chans, alpha chans
  // for some filters we have repeatable chantmpls, if counts is supplied, then a value > 1
  // indicates the number of copies of that chantmpl
  //
  int nctmpls, tot = 0;
  weed_chantmpl_t **ctmpls = direction == LIVES_INPUT ? weed_filter_get_in_chantmpls(filter, &nctmpls)
    : weed_filter_get_out_chantmpls(filter, &nctmpls);
  if (ctmpls) {
    while (nctmpls) if (!skip_ctmpl(filter, ctmpls[--nctmpls])) tot++;
    lives_free(ctmpls);
  }
  return tot;
}


static boolean does_pconv_gamma(int outpl, int inpl) {
  if (can_inline_gamma(outpl, inpl)) return TRUE;
  if (weed_palette_is_rgb(outpl) && weed_palette_is_yuv(inpl)) return TRUE;
  return FALSE;
}


static void get_resize_ops(int outpl, int inpl, int *ops, int upscale) {
  int oclamp;
  int interpal = outpl;

  ops[OP_RESIZE] = ops[OP_PCONV] = ops[OP_GAMMA] = FALSE;

  if (get_resizable(&interpal, NULL, &oclamp, &inpl, NULL, upscale) == LIVES_RESULT_SUCCESS) {
    ops[OP_PCONV] = TRUE;
    if (interpal != outpl && does_pconv_gamma(outpl, interpal))
      ops[OP_GAMMA] = TRUE;
  }
  if (weed_palette_is_rgb(outpl) && weed_palette_is_rgb(inpl)) ops[OP_GAMMA] = TRUE;
}


#define GCONV_RGB_RGB			(1ull << 0)
#define GCONV_RGB_YUV			(1ull << 1)
#define GCONV_YUV_RGB			(1ull << 2)
#define GCONV_RESIZE			(1ull << 3)
#define GCONV_LETTERBOX			(1ull << 4)

#define OPORD_LETTERBOX			(1 << 16)
#define OPORD_EXPLAIN			(1 << 17)

static lives_result_t get_op_order(int out_width, int out_height, int in_width, int in_height,
                                   int flags, int outpl, int inpl,
                                   int out_gamma_type, int in_gamma_type, int *op_order) {
  // we can define order 1, 2, 3
  // if multiple ops have same number, they are done simultaneously
  // if an op is not needed, order remains at 0

  // each of resize / palconv / gamma can be needed or not
  // resize can do resize OR resize + palconv OR ressize + gamma OR resize + palconv + gamma
  // palconv can do palconv or palconv + gamma

  // also, we can NEED resize / palconv / gamma
  uint64_t changemap = 0;
  boolean resize_ops[N_OP_TYPES], ops_needed[N_OP_TYPES];

  boolean pconv_does_gamma = FALSE;
  boolean is_upscale = FALSE, lbox = FALSE, explain = FALSE;
  boolean in_yuv = FALSE, out_yuv = FALSE;

  int out_size = out_width * out_height, in_size = in_width * in_height;

  if (flags & OPORD_LETTERBOX) lbox = TRUE;
  if (flags & OPORD_EXPLAIN) explain = TRUE;

  for (int i = 0; i < N_OP_TYPES; i++) op_order[i] = 0;

  ops_needed[OP_GAMMA] = ops_needed[OP_RESIZE] = ops_needed[OP_PCONV]
    = ops_needed[OP_LETTERBOX] = FALSE;

  if (weed_palette_is_yuv(outpl)) out_yuv = TRUE;
  if (weed_palette_is_yuv(inpl)) in_yuv = TRUE;

  if (in_width != out_width || in_height != out_height) {
    if (explain)
      d_print_debug("Needs resizing as we are going from %d X %d to %d X %d\n",
                    out_width, out_height, in_width, in_height);
    ops_needed[OP_RESIZE] = TRUE;
    changemap |= GCONV_RESIZE;
    if (in_size > out_size) is_upscale = TRUE;
  }

  if (lbox) {
    if (explain)
      d_print_debug("Needs letteboxing\n");
    ops_needed[OP_LETTERBOX] = TRUE;
    changemap |= GCONV_LETTERBOX;
  }

  if (out_gamma_type != WEED_GAMMA_UNKNOWN && in_gamma_type == WEED_GAMMA_UNKNOWN)
    in_gamma_type = get_tgt_gamma(inpl, outpl);

  if (out_gamma_type != in_gamma_type && (!out_yuv && !in_yuv)) {
    if (explain)
      d_print_debug("Needs gamma conversion because we are going from out_gamma %s to in_gamma %s and "
                    "both palettes are RGB\n", weed_gamma_get_name(out_gamma_type), weed_gamma_get_name(in_gamma_type));
    changemap |= GCONV_RGB_RGB;
    ops_needed[OP_GAMMA] = TRUE;
  } else if (out_yuv && !in_yuv && in_gamma_type != WEED_GAMMA_UNKNOWN
             && in_gamma_type != out_gamma_type) {
    if (explain)
      d_print_debug("Needs gamma conversion because we are going from YUV (%s) to RGB (%s) and "
                    "we need gamma_type %s\n", weed_palette_get_name(outpl), weed_palette_get_name(inpl),
                    weed_gamma_get_name(in_gamma_type));
    changemap |= GCONV_YUV_RGB;
    ops_needed[OP_GAMMA] = TRUE;
  } else if (in_yuv && !out_yuv && out_gamma_type != WEED_GAMMA_UNKNOWN
             && out_gamma_type != in_gamma_type) {
    if (explain)
      d_print_debug("Needs automatic gamma conversion because we are going from RGB (%s) to YUV (%s) and "
                    "output has gamma_type %s\n", weed_palette_get_name(outpl), weed_palette_get_name(inpl),
                    weed_gamma_get_name(out_gamma_type));
    changemap |= GCONV_RGB_YUV;
    ops_needed[OP_GAMMA] = TRUE;
  }

  if (outpl != inpl) {
    if (explain)
      d_print_debug("Needs palette conversion as we are going from %s to %s\n",
                    weed_palette_get_name(outpl), weed_palette_get_name(inpl));
    ops_needed[OP_PCONV] = TRUE;
  }

  // handle trivial situations first
  if (!ops_needed[OP_RESIZE]) {
    if (!ops_needed[OP_PCONV]) {
      if (!ops_needed[OP_GAMMA]) {
        op_order[OP_RESIZE] = op_order[OP_PCONV] = op_order[OP_GAMMA] = 0;
        if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 1;
        return LIVES_RESULT_SUCCESS;
      }
      // only gamma
      op_order[OP_GAMMA] = 1;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
      return LIVES_RESULT_SUCCESS;
    }
    // no resize; pconv needed
    if (!ops_needed[OP_GAMMA]) {
      // no gamma
      op_order[OP_PCONV] = 1;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
      return LIVES_RESULT_SUCCESS;
    }
    // pconv / gamma
    // pconv may do both
    pconv_does_gamma = does_pconv_gamma(outpl, inpl);
    if (pconv_does_gamma) {
      if (explain)
        d_print_debug("Palette conversion and gamma change may be combined in one operation\n");
      op_order[OP_PCONV] = op_order[OP_GAMMA] = 1;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
      return LIVES_RESULT_SUCCESS;
    }
    if (explain)
      d_print_debug("Palette conversion and gamma change cannot be combined\n");
    if (in_yuv) {
      // gamma then pconv
      if (explain)
        d_print_debug("Since we are converting to YUV, gamma change must be done first\n");
      op_order[OP_GAMMA] = 1;
      op_order[OP_PCONV] = 2;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
      return LIVES_RESULT_SUCCESS;
    }
    // pconv then gamma
    op_order[OP_PCONV] = 1;
    op_order[OP_GAMMA] = 2;
    if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
    return LIVES_RESULT_SUCCESS;
  }

  // resize needed
  get_resize_ops(outpl, inpl, resize_ops, is_upscale);
  if (ops_needed[OP_PCONV]) {
    if (resize_ops[OP_PCONV]) {
      if (ops_needed[OP_GAMMA]) {
        if (!resize_ops[OP_GAMMA]) {
          // resize does palconv but not gamma
          if (in_yuv || (is_upscale && !out_yuv)) {
            // do gamma b4 palconv
            if (explain)
              d_print_debug("Since we are upscaling, gamma change will be applied first\n");
            op_order[OP_GAMMA] = 1;
            op_order[OP_RESIZE] = op_order[OP_PCONV] = 2;
            if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
            return LIVES_RESULT_SUCCESS;
          }
          // do pal_conv b4 gamma
          op_order[OP_RESIZE] = op_order[OP_PCONV] = 1;
          op_order[OP_GAMMA] = 2;
          if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
          return LIVES_RESULT_SUCCESS;
        }
        // resize does all
        if (explain)
          d_print_debug("Resize, Palette conversion and gamma change will be combined in one operation\n");
        op_order[OP_RESIZE] = op_order[OP_PCONV] = op_order[OP_GAMMA] = 1;
        if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
        return LIVES_RESULT_SUCCESS;
      }
      // resize does palconv, no gamma needed
      op_order[OP_RESIZE] = op_order[OP_PCONV] = 1;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
      return LIVES_RESULT_SUCCESS;
    }

    // resize does not do palconv, palconv needed

    if (ops_needed[OP_GAMMA]) {
      // gamma needed
      // palconv may do gamma
      if (explain)
        d_print_debug("Resize and palette conversion cannot be done combined\n");
      pconv_does_gamma = does_pconv_gamma(outpl, inpl);
      if (explain) {
        if (pconv_does_gamma)
          d_print_debug("Palette conversion and gamma change cannot be done combined\n");
        else
          d_print_debug("Palette conversion and gamma change wil be combined\n");
      }
      if (is_upscale) {
        // upscale - do resize last if we can
        // do palconv / gamma before resize
        if (explain)
          d_print_debug("Since we are upscaling, resize will be done last\n");
        if (pconv_does_gamma) {
          op_order[OP_GAMMA] = op_order[OP_PCONV] = 1;
          op_order[OP_RESIZE] = 2;
          if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
          return LIVES_RESULT_SUCCESS;
        }
        // palconv dont do gamma
        if (!out_yuv) {
          // gamma, palconv, resize
          op_order[OP_GAMMA] = 1;
          op_order[OP_PCONV] = 2;
          op_order[OP_RESIZE] = 3;
          if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 4;
          return LIVES_RESULT_SUCCESS;
        }
        // palconv, gamma, resize
        op_order[OP_PCONV] = 1;
        op_order[OP_GAMMA] = 2;
        op_order[OP_RESIZE] = 3;
        if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
        return LIVES_RESULT_SUCCESS;
      }
      // resize does not do palconv, palconv needed
      // mo upscale, do resize first

      if (pconv_does_gamma) {
        if (explain) {
          d_print_debug("Palette conversion and gamma change wil be combined\n");
          op_order[OP_RESIZE] = 1;
          op_order[OP_PCONV] = 2;
          op_order[OP_GAMMA] = 2;
          if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
          return LIVES_RESULT_SUCCESS;
        }
        if (in_yuv) {
          // gamma before palconv
          op_order[OP_RESIZE] = 1;
          op_order[OP_GAMMA] = 2;
          op_order[OP_PCONV] = 3;
          if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 4;
          return LIVES_RESULT_SUCCESS;
        }
        // palconv before gamma
        op_order[OP_RESIZE] = 1;
        op_order[OP_PCONV] = 2;
        op_order[OP_GAMMA] = 3;
        if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 4;
        return LIVES_RESULT_SUCCESS;
      }
    }
    // no gamma needed
    op_order[OP_RESIZE] = 1;
    op_order[OP_PCONV] = 2;
    if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
    return LIVES_RESULT_SUCCESS;
  }

  // resize, no palconv, maybe gamma
  if (ops_needed[OP_GAMMA]) {
    if (resize_ops[OP_GAMMA]) {
      // resize does gamma
      if (explain) {
        if (pconv_does_gamma)
          d_print_debug("Resize and gamma change wil be combined\n");
        op_order[OP_RESIZE] = 1;
        op_order[OP_GAMMA] = 1;
        if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
        return LIVES_RESULT_SUCCESS;
      }
    }
    // gamma needed, reszie does pconv, but no pconv needed
    if (is_upscale) {
      if (explain)
        d_print_debug("Since we are upscaling, gamma change  will be done first\n");
      op_order[OP_GAMMA] = 1;
      op_order[OP_RESIZE] = 2;
      if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
      return LIVES_RESULT_SUCCESS;
    }
    op_order[OP_RESIZE] = 1;
    op_order[OP_GAMMA] = 2;
    if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 3;
    return LIVES_RESULT_SUCCESS;
  }

  // no gamma, no pcomv
  // no gamma needed, only resize
  op_order[OP_RESIZE] = 1;
  if (ops_needed[OP_LETTERBOX]) op_order[OP_LETTERBOX] = 2;
  return LIVES_RESULT_SUCCESS;
}


// all operations that have associated costs may be handled here
// depending on the in and out palettes we may be abel to combine two or three of rsize, palconv, gammconv
// this will reduce the overall time compared to performing the operations in sequence
// we also have a number of pool threads available, if we know what operations are occuring in paralell,
// we can model distrubuting the threads approximately over simultaneous operations
// and discount some time cost
// this can affect resize, palcovn, gamma conv and proc tcosts (and dinterlace)

// here we calulculate tcost, qloss_sG, qloss_sR
static double get_resize_cost(int cost_type, int out_width, int out_height, int in_width, int in_height,
                              int outpl, int inpl) {
  // TODO - we really need to know clamping -
  // but we cannot know tht until we actually pull frames from clip_srcs
  // - if we can resize, but not directly, and outpl is yuv, and it is clamped
  // we need to convert to unclamped, to allow masquerading
  // - this affects tcost only, but we have a clamped -> unclamped op

  int in_size, out_size;
  // if downscaling we add a potential cost, if upscaling we add a real one
  switch (cost_type) {
  case COST_TYPE_QLOSS_S:
    out_size = out_width * out_height;
    in_size = in_width * in_height;
    if (in_size < out_size) return out_size / in_size;
    return in_size / out_size;

  case COST_TYPE_TIME: {
    double tcost = 1.;
    /* if (glob_timing) { */
    /*   volatile float *cpuload; */
    /*   ann_testdata_t realdata; */

    /*   double outsize = (double)lives_frame_calc_bytesize(out_width, out_height, outpl, FALSE, NULL); */
    /*   double insize = (double)lives_frame_calc_bytesize(in_width, in_height, inpl, FALSE, NULL); */

    /*   realdata.inputs = LIVES_CALLOC_SIZEOF(double, glob_timing->ann->lcount[0]); */

    /*   realdata.inputs[TIMING_ANN_OUTSIZE] = outsize / 1000000.; */
    /*   realdata.inputs[TIMING_ANN_INSIZE] = insize / 1000000.; */

    /*   cpuload = get_core_loadvar(0); */
    /*   realdata.inputs[TIMING_ANN_CPULOAD] = ((double) * cpuload) / 50.; */
    /*   realdata.inputs[TIMING_ANN_PBQ_BASE + prefs->pb_quality] = 100.; */
    /*   if (outpl != inpl) { */
    /*     double pval = 1.; */
    /*     if ((weed_palette_is_rgb(outpl) && weed_palette_is_yuv(inpl)) */
    /*         || (weed_palette_is_yuv(outpl) && weed_palette_is_rgb(inpl))) pval = 5; */
    /*     realdata.inputs[TIMING_ANN_OUT_PAL_BASE + get_enum_palette(outpl)] = pval; */
    /*     realdata.inputs[TIMING_ANN_IN_PAL_BASE + get_enum_palette(inpl)] = pval; */
    /*   } */
    /*   pthread_mutex_lock(&glob_timing->ann_mutex); */
    /*   tcost = lives_ann_predict_result(glob_timing->ann, &realdata) / 1000.; */
    /*   pthread_mutex_unlock(&glob_timing->ann_mutex); */
    /*   lives_free(realdata.inputs); */
    /* } */
    return tcost;
  }
  default: break;
  }
  return 0.;
}


static double get_layer_copy_cost(int cost_type, int width, int height, int pal) {
  if (cost_type != COST_TYPE_TIME) return 0.;
  if (!glob_timing->bytes_per_sec) return 0.;
  size_t bytes = lives_frame_calc_bytesize(width, height, pal, FALSE, NULL);
  return bytes / glob_timing->bytes_per_sec;
}


static double get_gamma_cost(int cost_type, int width, int height, int pal, int out_gamma_type, int in_gamma_type,
                             boolean ghost) {
  // find the time cst for converting gamma - calc size * psize
  // the mpy by const
  // - may be paralellisable
  //
  // there is a small qloss associated, which varies with pb_quality
  // there is also a ghost negative qloss for using linear gamma if a plugin prefers this

  if (cost_type == COST_TYPE_QLOSS_P) {
    double q = 1.;
    if (in_gamma_type != out_gamma_type) {
      q *= .99;
      // converting from gamma != linear, unknown to different !linear, unknown
      // we have to go via linear, so that would be two conversions
      if (out_gamma_type != WEED_GAMMA_LINEAR && out_gamma_type != WEED_GAMMA_UNKNOWN
          && in_gamma_type != WEED_GAMMA_LINEAR && in_gamma_type != WEED_GAMMA_UNKNOWN)
        q *= .99;
    }
    if (ghost && in_gamma_type == WEED_GAMMA_LINEAR) q += .1;
    return (1. - q);
  }
  if (cost_type == COST_TYPE_TIME) {
    if (glob_timing->gbytes_per_sec) {
      size_t bytes = lives_frame_calc_bytesize(width, height, pal, FALSE, NULL);
      return bytes / glob_timing->gbytes_per_sec;
    } else {
      if (glob_timing->bytes_per_sec) {
        size_t bytes = lives_frame_calc_bytesize(width, height, pal, FALSE, NULL);
        return bytes / glob_timing->bytes_per_sec;
      }
    }
  }
  return 0.;
}


static double get_misc_cost(int cost_type, int flags, int pal, int width, int height) {
  // calc for misc costs like deinterlac

  return 0.;
}


// estimate COST_TYPE_QLOSS_P for a palette conversion
// - rather than returning the cost per se, we return (1. - q_in / q_uot)
// i.e if there is no quality loss we return 1.0, for a 5% loss we return 0.95
// this makes it easier to multiply losses. We try to maximise this value
// and when calulating combined_cost we use factor * (1.0 - qloss_p)
// if the (out) gamma_type is WEED_GAMMA_LINEAR, then this means that the filter prefers that,
// and if calulating with ghost == TRUE, we actually get a bonus (considered as NEGATIVE quality loss)
//
static double get_qloss_p(int outpl, int inpl, int *inpals) {
  double q = 1., cost = 0.;

  // subspace and sampling are  ignored for now
  int in_clamping, out_clamping, in_subspace, out_subspace, in_sampling, out_sampling;

  out_clamping = in_clamping = WEED_YUV_CLAMPING_UNCLAMPED;
  out_sampling = in_sampling = WEED_YUV_SAMPLING_DEFAULT;
  out_subspace = in_subspace = WEED_YUV_SUBSPACE_YUV;

  if (inpl != outpl)  {
    switch (outpl) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_BGRA32:
    case WEED_PALETTE_ARGB32:
      switch (inpl) {
      case WEED_PALETTE_RGB24:
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGR24:
      case WEED_PALETTE_BGRA32:
      case WEED_PALETTE_ARGB32:
        break;
      case WEED_PALETTE_YUV888:
      case WEED_PALETTE_YUV444P:
      case WEED_PALETTE_YUVA8888:
      case WEED_PALETTE_YUVA4444P:
        q *= .95;
        if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
        break;
      case WEED_PALETTE_YUV422P:
      case WEED_PALETTE_UYVY:
      case WEED_PALETTE_YUYV:
        q *= .9;
        if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
        break;
      case WEED_PALETTE_YUV420P:
      case WEED_PALETTE_YVU420P:
        q *= .85;
        if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
        break;
      case WEED_PALETTE_YUV411:
        q *= .8;
        if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
        break;
      default:
        if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
        break;
      }
    case WEED_PALETTE_YUV888:
    case WEED_PALETTE_YUV444P:
    case WEED_PALETTE_YUVA8888:
    case WEED_PALETTE_YUVA4444P:
      switch (inpl) {
      case WEED_PALETTE_YUV888:
      case WEED_PALETTE_YUV444P:
      case WEED_PALETTE_YUVA8888:
      case WEED_PALETTE_YUVA4444P:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        break;
      case WEED_PALETTE_RGB24:
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGR24:
      case WEED_PALETTE_BGRA32:
      case WEED_PALETTE_ARGB32:
        q *= .95;
        break;
      case WEED_PALETTE_YUV422P:
      case WEED_PALETTE_UYVY:
      case WEED_PALETTE_YUYV:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .95;
        break;
      case WEED_PALETTE_YUV420P:
      case WEED_PALETTE_YVU420P:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .9;
        break;
      case WEED_PALETTE_YUV411:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .85;
        break;
      default: break;
      }
    case WEED_PALETTE_YUV422P:
    case WEED_PALETTE_UYVY:
    case WEED_PALETTE_YUYV:
      switch (inpl) {
      case WEED_PALETTE_YUV888:
      case WEED_PALETTE_YUV444P:
      case WEED_PALETTE_YUVA8888:
      case WEED_PALETTE_YUVA4444P:
      case WEED_PALETTE_YUV422P:
      case WEED_PALETTE_UYVY:
      case WEED_PALETTE_YUYV:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        break;
      case WEED_PALETTE_RGB24:
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGR24:
      case WEED_PALETTE_BGRA32:
      case WEED_PALETTE_ARGB32:
        q *= .95;
        break;
      case WEED_PALETTE_YUV420P:
      case WEED_PALETTE_YVU420P:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .95;
        break;
      case WEED_PALETTE_YUV411:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .9;
        break;
      default: break;
      }
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      switch (inpl) {
      case WEED_PALETTE_YUV888:
      case WEED_PALETTE_YUV444P:
      case WEED_PALETTE_YUVA8888:
      case WEED_PALETTE_YUVA4444P:
      case WEED_PALETTE_YUV422P:
      case WEED_PALETTE_UYVY:
      case WEED_PALETTE_YUYV:
      case WEED_PALETTE_YUV420P:
      case WEED_PALETTE_YVU420P:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        break;
      case WEED_PALETTE_RGB24:
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGR24:
      case WEED_PALETTE_BGRA32:
      case WEED_PALETTE_ARGB32:
        q *= .95;
        break;
      case WEED_PALETTE_YUV411:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        q *= .95;
        break;
      default: break;
      }
    case WEED_PALETTE_YUV411:
      switch (inpl) {
      case WEED_PALETTE_YUV888:
      case WEED_PALETTE_YUV444P:
      case WEED_PALETTE_YUVA8888:
      case WEED_PALETTE_YUVA4444P:
      case WEED_PALETTE_YUV422P:
      case WEED_PALETTE_UYVY:
      case WEED_PALETTE_YUYV:
      case WEED_PALETTE_YUV420P:
      case WEED_PALETTE_YVU420P:
        if (out_clamping != in_clamping) q *= .99;
        if (out_sampling != in_sampling) q *= .99;
        if (out_subspace != in_subspace) q *= .96;
        break;
      case WEED_PALETTE_RGB24:
      case WEED_PALETTE_RGBA32:
      case WEED_PALETTE_BGR24:
      case WEED_PALETTE_BGRA32:
      case WEED_PALETTE_ARGB32:
        q *= .95;
        break;
      default: break;
      }
    default: break;
    }
    cost = 1. - q;
    if (prefs->pb_quality == PB_QUALITY_HIGH) cost *= .8;
    else if (prefs->pb_quality == PB_QUALITY_LOW) cost *= 1.2;
  }
  return cost;
}


double get_pconv_cost(int cost_type, int width, int height, int outpl, int inpl, int *inpals) {
  // find the time cst for converting gamma - calc size * psize
  // the mpy by const
  // - may be paralellisable
  //
  // there is a small qloss associated, which varies with pb_quality
  // there is also a ghost negative qloss for using linear gamma if a plugin prefers this
  // --
  if (cost_type == COST_TYPE_QLOSS_P)
    return get_qloss_p(outpl, inpl, inpals);
  if (cost_type == COST_TYPE_TIME) {
    double tcost = 0.01;
    /* if (glob_timing) { */
    /*   volatile float *cpuload; */
    /*   ann_testdata_t realdata; */
    /*   cpuload = get_core_loadvar(0); */

    /* realdata.inputs = LIVES_CALLOC_SIZEOF(double, glob_timing->ann->lcount[0]); */

    /* realdata.inputs[TIMING_ANN_CPULOAD] = ((double) * cpuload) / 50.; */
    /* realdata.inputs[TIMING_ANN_PBQ_BASE + prefs->pb_quality] = 100.; */
    /* if (outpl != inpl) { */
    /*   double pval = 1.; */
    /*   if ((weed_palette_is_rgb(outpl) && weed_palette_is_yuv(inpl)) */
    /*       || (weed_palette_is_yuv(outpl) && weed_palette_is_rgb(inpl))) pval = 5; */
    /*   realdata.inputs[TIMING_ANN_OUT_PAL_BASE + get_enum_palette(outpl)] = pval; */
    /*   realdata.inputs[TIMING_ANN_IN_PAL_BASE + get_enum_palette(inpl)] = pval; */
    /* } */
    /* pthread_mutex_lock(&glob_timing->ann_mutex); */
    /* tcost = lives_ann_predict_result(glob_timing->ann, &realdata) / 1000.; */
    /* pthread_mutex_unlock(&glob_timing->ann_mutex); */
    /* lives_free(realdata.inputs); */
    //}
    return tcost;
  }
  return 0.;
}


static double get_proc_cost(int cost_type, weed_filter_t *filter, int width, int height, int pal) {
  // get processing cost for applying an instance. The only cost with non-zero valueis tcost
  double est = 0.;
  if (cost_type == COST_TYPE_TIME) {

    // return estimate which will be fn(size, pal)
  }
  return est;
}


// we go from out (output) to in (input)
static double get_conversion_cost(int cost_type, int out_width, int out_height, int in_width, int in_height,
                                  boolean lbox, int outpl, int inpl, int *inpals, int out_gamma_type,
                                  int in_gamma_type, boolean ghost) {
  // get cost for resize, pal_conv, gamma_change + misc_costs
  // qloss costs for size_changes (QLOSS_S) are already baked into the model so we do not calculate those
  // for QLOSS_P, we havlbe conversion cosst + possible ghost cost
  // for time cost, some operations can be combined - resize, pal_conv, gamma_conversion
  // in addiition we may have misc costs (e.g for deinterlacing)
  // so, valid cost_types are COST_TYPE_TIME and COST_TYPE_QLOSS_P
  double cost = 0.;

  int op_order[N_OP_TYPES];
  int flags = 0;
  if (lbox) flags |= OPORD_LETTERBOX;

  get_op_order(out_width, out_height, in_width,  in_height, flags, outpl, inpl,
               out_gamma_type, in_gamma_type, op_order);

  if (!op_order[OP_RESIZE] && !op_order[OP_PCONV] && !op_order[OP_GAMMA] && !op_order[OP_LETTERBOX]) return 0.;

  // for COST_TYPE_QLOSS_P it can be more convenient to call calc_pal_conv_costs directly
  if (cost_type == COST_TYPE_QLOSS_P) {
    double q = 1.;
    // call this whether or not we convert gamma, if in_gamma_type is linear, we get a bonus ghost cost
    // we can actually end up with a negative cost !
    q *= get_gamma_cost(cost_type, 0., 0., WEED_PALETTE_NONE, out_gamma_type, in_gamma_type, ghost);
    if (op_order[OP_PCONV])
      q *= get_pconv_cost(cost_type, 0., 0., outpl, inpl, inpals);
    return q;
  }

  if (cost_type != COST_TYPE_TIME) return 0.;

  if (lbox && glob_timing->bytes_per_sec) cost += (in_width * in_height) / glob_timing->bytes_per_sec;

  if (op_order[OP_RESIZE] == 1) {
    // 1 - -
    if (op_order[OP_PCONV] == 1) {
      // 1 1 -
      if (op_order[OP_GAMMA] == 1) {
        // all 3 ops - so only resize cost : R
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // resize + pconv : R
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize + palconv / gamma : R G
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl)
	  + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (!op_order[OP_PCONV]) {
      // 1 0 -
      if (op_order[OP_GAMMA] == 1) {
        // resize + gamma : R
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // resize only : R
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize / gamma : R G
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl)
	  + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_PCONV] == 2) {
      // 1 2 -
      if (op_order[OP_GAMMA] == 1) {
        // resize + gamma / pconv: R P
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (!op_order[OP_GAMMA]) {
        // resize / pconv : R P
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize / palconv + gamma : R P
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 3) {
        // resize / pconv / gamma : R P G
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals)
	  + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_PCONV] == 3) {
      // 1 3 2
      if (op_order[OP_GAMMA] == 2) {
        // resize / gamma / pconv : R G P
        return cost + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, outpl, out_gamma_type, in_gamma_type, ghost)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
    }
  }

  // pconv first
  if (op_order[OP_PCONV] == 1) {
    // - 1 -
    if (!op_order[OP_RESIZE]) {
      // 0 1 -
      if (!op_order[OP_GAMMA]) {
        // pconv only : P
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 1) {
        // pconv + gamma : P
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 2) {
        // pconv / gamma : P G
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 2) {
      if (op_order[OP_GAMMA] == 1) {
        // pconv + gamma / resize : P R
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // pconv / resize : P R
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // pconv / resize + gamma : P R
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (op_order[OP_GAMMA] == 3) {
        // pconv / resize / gamma : P R G
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl)
	  + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 3) {
      if (op_order[OP_GAMMA] == 2) {
        // pconv / gamma / resize : P G R
        return cost + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, inpl, out_gamma_type, in_gamma_type, ghost)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
    }
  }

  // gamma first
  if (op_order[OP_GAMMA] == 1) {
    // - - 1
    if (!op_order[OP_RESIZE]) {
      if (!op_order[OP_PCONV]) {
        // gamma only
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
      if (op_order[OP_PCONV] == 2) {
        // gamma / pconv : G P
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
	  + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (!op_order[OP_PCONV]) {
        // gamma only : G
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 2) {
      if (op_order[OP_PCONV] == 2) {
        // gamma / resize + pconv : G R
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl);
      }
      if (op_order[OP_PCONV] == 3) {
        // gamma / resize / pconv : G R P
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
	  + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
    }
    if (op_order[OP_RESIZE] == 3) {
      if (op_order[OP_PCONV] == 2) {
        // gamma / pconv / resize : G P R
        return cost + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
	  + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
	  + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
    }
  }
  return 0.;
}


static exec_plan_substep_t *make_substep(int op_idx, double st_time,
					 int out_width, int out_height, int out_pal) {
  volatile float const *cpuload;
  LIVES_CALLOC_TYPE(exec_plan_substep_t, substep, 1);
  substep->op_idx = op_idx;
  substep->width = out_width;
  substep->height = out_height;
  substep->pal = out_pal;
  substep->start = st_time;
  substep->pb_quality = prefs->pb_quality;
  cpuload = get_core_loadvar(0);
  substep->cpuload = (float) * cpuload;
  glob_timing->cpu_nsamples++;
  //glob_timing->av_cpuload += substep->cpuload;
  return substep;
}


// CONVERT STEP // (need LOAD STEP, LAYER COPY STEP, APPLY INST step)

static boolean do_opt_actions(plan_step_t *step, boolean check) {
  int track = step->track;
  lives_layer_t *layer = step->plan->layers[track];
  int clipno;

  if (!layer) return FALSE;

  clipno = weed_get_int_value(layer, WEED_LEAF_CLIP, NULL);

  lives_clip_t *sfile = RETURN_VALID_CLIP(clipno);
  if (!sfile) {
    return FALSE;
  }

  /* if (step->st_type == STEP_TYPE_LOAD && !(step->flags & STEP_FLAG_RUN_AS_LOAD)) { */
  /*   // should be done in first conversion step after loading */
  /*   ticks_t tc; */
  /*   if (check) return TRUE; */
  /*   tc = (lives_layer_get_frame(layer) - 1.) / sfile->fps * TICKS_PER_SECOND_DBL; */
  /*   if (sfile->deinterlace) deinterlace_frame(layer, tc); */
  /* } */

  // render subtitles from file

  // should be done after final conversion step for sink display
  if (step->st_type == STEP_TYPE_APPLY_INST) {
    if (!step->target) {
      if (sfile->subt && sfile->subt->tfile >= 0 && prefs->show_subtitles) {
        if (check) {
          //weed_layer_unref(layer);
          return TRUE;
        }
        frames_t frame = lives_layer_get_frame(layer);
        double xtime = (double)(frame - 1) / sfile->fps;
        render_subs_from_file(sfile, xtime, layer);
      }
    }
  }
  //weed_layer_unref(layer);
  return FALSE;
}


static lives_filter_error_t pconv_substep(plan_step_t *step) {
  ____FUNC_ENTRY____(pconv_substep, "i", "v");
  lives_filter_error_t retval = FILTER_ERROR_MISSING_LAYER;
  int track = step->track;
  lives_layer_t *layer = step->plan->layers[track];

  if (layer) {
    GET_PROC_THREAD_SELF(self);
    exec_plan_substep_t *sub;
    int tgt_gamma = step->fin_gamma;
    int osampling = step->fin_sampling;
    int osubspace = step->fin_subspace;
    int oclamping = step->fin_sampling;
    int opalette = step->fin_pal;
    weed_layer_ref(layer);
    int inpalette = weed_layer_get_palette(layer);

    double xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "pconv_start", xtime);

    retval = FILTER_SUCCESS;
    sub = make_substep(OP_PCONV, xtime, weed_layer_get_width(layer),
                       weed_layer_get_height(layer), weed_layer_get_palette(layer));
    step->substeps = lives_list_append(step->substeps, (void *)sub);

    if (inpalette != opalette) {
      if (!convert_layer_palette_full(layer, opalette, oclamping,
                                      osampling, osubspace, tgt_gamma)) {
        char *msg = lives_strdup_printf("Invalid palette conversion %d to %d\n", inpalette, opalette);
        retval = FILTER_ERROR_INVALID_PALETTE_CONVERSION;
        BREAK_ME("invpal");
        lives_proc_thread_error(self, (int)retval, LPT_ERR_MAJOR, "%s", msg);
        lives_free(msg);
        weed_layer_unref(layer);
        lives_proc_thread_cancel(self);
      }
    }
    xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "pconv_end", xtime);

    sub->end = xtime;

    weed_layer_unref(layer);
  }

  ____FUNC_EXIT_VAL____("i", retval);
  return retval;
}


static lives_filter_error_t gamma_substep(plan_step_t *step) {
  ____FUNC_ENTRY____(gamma_substep, "i", "v");
  lives_filter_error_t retval = FILTER_ERROR_MISSING_LAYER;
  int track = step->track;
  lives_layer_t *layer = step->plan->layers[track];
  if (layer) {
    GET_PROC_THREAD_SELF(self);
    int tgt_gamma = step->fin_gamma;
    int l_gamma = weed_layer_get_gamma(layer);

    double xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "gconv_start", xtime);
    weed_layer_ref(layer);

    retval = FILTER_SUCCESS;

    if (l_gamma != tgt_gamma && tgt_gamma != WEED_GAMMA_UNKNOWN)
      gamma_convert_layer(tgt_gamma, layer);

    xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "gconv_end", xtime);

    weed_layer_unref(layer);
  }
  ____FUNC_EXIT_VAL____("i", retval);
  return retval;
}


static lives_filter_error_t res_substep(plan_step_t *step) {
  ____FUNC_ENTRY____(res_substep, "i", "v");
  lives_filter_error_t retval = FILTER_ERROR_MISSING_LAYER;

  int track = step->track;
  lives_layer_t *layer = step->plan->layers[track];
  if (layer) {
    GET_PROC_THREAD_SELF(self);
    exec_plan_substep_t *sub;
    //double lb_time = 0.;
    int interp = GET_SELF_VALUE(int, "interp");
    int oclamping = step->fin_sampling;
    int opalette = step->fin_pal;
    int xwidth = step->fin_iwidth;
    int xheight = step->fin_iheight;
    int width = step->fin_width;
    int height = step->fin_height;
    boolean resized = FALSE;

    double xtime = lives_get_session_time();

    weed_layer_ref(layer);

    if (!xwidth) xwidth = width;
    if (!xheight) xheight = height;

    SET_SELF_VALUE(double, "res_start", xtime);

    sub = make_substep(OP_RESIZE, xtime, weed_layer_get_width(layer),
                       weed_layer_get_height(layer), weed_layer_get_palette(layer));
    step->substeps = lives_list_append(step->substeps, (void *)sub);

    retval = FILTER_SUCCESS;

    resized = resize_layer(layer, xwidth, xheight, interp, opalette, oclamping);

    if (!resized) {
      retval = FILTER_ERROR_UNABLE_TO_RESIZE;
      fprintf(stderr, "failed %d X %d pal %d to %d X %d %d\n", weed_layer_get_width(layer),
              weed_layer_get_height(layer), weed_layer_get_palette(layer),
              width, height, opalette);
      lives_proc_thread_error(self, (int)retval, LPT_ERR_MAJOR, "%s", "Unable to resize");
    }

    xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "res_end", xtime);

    sub->end = xtime;

    if (sub->start > sub->end) sub->end = sub->start;

    weed_layer_unref(layer);
  }
  ____FUNC_EXIT_VAL____("i", retval);
  return retval;
}


static lives_filter_error_t lbox_substep(plan_step_t *step) {
  ____FUNC_ENTRY____(lbox_substep, "i", "v");
  // letterbox substep
  lives_filter_error_t retval = FILTER_ERROR_MISSING_LAYER;
  int track = step->track;
  lives_layer_t *layer = step->plan->layers[track];

  if (layer) {
    GET_PROC_THREAD_SELF(self);
    exec_plan_substep_t *sub;
    double lb_time = 0.;

    int interp = GET_SELF_VALUE(int, "interp");
    int oclamping = step->fin_sampling;
    int opalette = step->fin_pal;
    int xwidth = step->fin_iwidth;
    int xheight = step->fin_iheight;
    int width = step->fin_width;
    int height = step->fin_height;
    boolean ret;

    double xtime = lives_get_session_time();

    weed_layer_ref(layer);

    retval = FILTER_SUCCESS;

    SET_SELF_VALUE(double, "res_start", xtime);
    sub = make_substep(OP_LETTERBOX, xtime, weed_layer_get_width(layer),
                       weed_layer_get_height(layer), weed_layer_get_palette(layer));
    step->substeps = lives_list_append(step->substeps, (void *)sub);

    ret = letterbox_layer(layer, width, height, xwidth, xheight, interp, opalette, oclamping);

    if (!ret) {
      retval = FILTER_ERROR_UNABLE_TO_RESIZE;
      d_print_debug("failed %d X %d pal %d to %d X %d (%d X %d) %d\n", weed_layer_get_width(layer),
                    weed_layer_get_height(layer), weed_layer_get_palette(layer),
                    width, height, xwidth, xheight, opalette);
      lives_proc_thread_error(self, (int)retval, LPT_ERR_MAJOR, "%s", "Unable to resize");
    }

    xtime = lives_get_session_time();
    SET_SELF_VALUE(double, "res_end", xtime);

    xtime -= lb_time;
    sub->end = xtime;

    if (sub->start > sub->end) sub->end = sub->start;

    sub->copy_time = lb_time;

    weed_layer_unref(layer);
  }
  ____FUNC_EXIT_VAL____("i", retval);
  return retval;
}


/////////////////////////////
static lives_filter_error_t run_apply_inst_step(plan_step_t *step, weed_instance_t *inst) {
  double xtime;
  exec_plan_t *plan = step->plan;
  lives_filter_error_t filter_error;

  xtime = lives_get_session_time();
  step->real_st = xtime;

  if (step->fin_gamma == WEED_GAMMA_LINEAR) {
    // if we have RGBA type in / out params, and instance runs with linear gamma
    // then we scale the param values according to the gamma correction
    gamma_conv_params(WEED_GAMMA_LINEAR, inst, TRUE);
    gamma_conv_params(WEED_GAMMA_LINEAR, inst, FALSE);
  }

  /* if (step->flags & STEP_FLAG_RUN_AS_LOAD) { */
  /*   lives_clip_t *sfile; */
  /*   int clipno = plan->model->clip_index[step->track]; */

  /*   lives_layer_t *layer; */
  /*   layer = plan->layers[step->track]; */
    
  /*   if (!layer) layer = plan->layers[step->track] = lives_layer_new_for_frame(clipno, 1); */

  /*   if (prefs->dev_show_timing) */
  /*     d_print_debug("my clipno is %d\n", clipno); */

  /*   sfile = RETURN_VALID_CLIP(clipno); */
  /*   if (!sfile) return FILTER_ERROR_INVALID_LAYER; */

  /*   // frame will be loaded from whatever clip_src */
  /*   step->state = STEP_STATE_RUNNING; */
  /*   pull_frame_threaded(layer, sfile->hsize, sfile->vsize); */
  /*   return 0; */
  /* } */

  filter_error = act_on_instance(inst, step->target_idx, plan->layers,
                                 plan->model->opwidth, plan->model->opheight);

  if (filter_error == FILTER_ERROR_NEEDS_REINIT) {
    d_print_debug("NEEDS reinit\n");
    weed_reinit_effect(inst, TRUE);
    mainw->refresh_model = TRUE;
    filter_error = act_on_instance(inst, step->target_idx, plan->layers,
                                   plan->model->opwidth, plan->model->opheight);
    if (filter_error != FILTER_SUCCESS)
      filter_error = FILTER_ERROR_INVALID_FILTER;
  }

  weed_instance_unref(inst);
  return filter_error;
}


static boolean ann_roll(void) {
  GET_PROC_THREAD_SELF(self);
  /* if (glob_timing->ann_data_in) { */
  /*   for (LiVESList *list = glob_timing->ann_data_in; list; list = listnext) { */
  /* 	listnext = lixt->next; */
  /* 	ann_data = (ann_data_t *)list->data; */
  /* 	if (ann_data->status == ANN_DATA_PREDICT) { */
  /* 	  pthread_mutex_lock(&ann->data->ann_mutex); */
  /* 	  ann_data->status = ANN_DATA_BUSY; */
  /* 	  ann_data->res = lives_ann_predict_result(ann_data->ann, ann_data->data); */
  /* 	  ann_data->status = ANN_DATA_READY; */
  /* 	  pthread_mutex_unlock(&ann->data->ann_mutex); */
  /* 	} */
  /* 	if (ann_data->status == ANN_DATA_TRAIN) { */
  /* 	  //etc */
  /* 	} */
  /*   } */
  /* } */
  int genstorun = 0;
  double loveliness;
  while (!lives_proc_thread_should_cancel(self)) {
    pthread_mutex_lock(&glob_timing->ann_mutex);
    genstorun = glob_timing->ann->genstorun;
    pthread_mutex_unlock(&glob_timing->ann_mutex);
    if (!genstorun) {
      lives_proc_thread_wait(self, ONE_MILLION);
    } else {
      while (genstorun) {
        uint64_t wait_time;
        if (lives_proc_thread_get_pause_requested(self)) break;
        if (lives_proc_thread_should_cancel(self)) break;
        loveliness = THREADVAR(loveliness);
        if (!pthread_mutex_trylock(&glob_timing->ann_mutex)) {
          genstorun--;
          glob_timing->ann->genstorun--;
          lives_ann_evolve(glob_timing->ann);
          pthread_mutex_unlock(&glob_timing->ann_mutex);
        }
        wait_time = (DEF_LOVELINESS - loveliness) * 10. * ONE_MILLION;
        lives_proc_thread_wait(self, wait_time);
      }
      genstorun = 0;
    }
    if (lives_proc_thread_get_cancel_requested(self)) {
      lives_proc_thread_cancel(self);
    }
    if (lives_proc_thread_get_pause_requested(self))
      lives_proc_thread_pause(self);
  }
  return TRUE;
}


static int check_step_condition(exec_plan_t *plan, plan_step_t *step, boolean cancelled, int error, boolean paused) {
  double xtime;
  lives_proc_thread_t lpt = step->proc_thread;
  if (lpt) {
    if (error || cancelled)
      if (!lives_proc_thread_had_error(lpt))
        if (!lives_proc_thread_get_cancel_requested(lpt))
          lives_proc_thread_request_cancel(lpt, FALSE);

    if (paused) {
      if (!lives_proc_thread_get_pause_requested(lpt))
        lives_proc_thread_request_pause(lpt);
      if (lives_proc_thread_is_paused(lpt)) {
        double xtime = lives_get_session_time();
        step->paused_time -= xtime;
        step->state = STEP_STATE_PAUSED;
        return 99;
      }
    }

    if (plan->state == STEP_STATE_RESUMING) {
      if (!lives_proc_thread_get_resume_requested(lpt))
        lives_proc_thread_request_resume(lpt);

      if (lives_proc_thread_is_paused(lpt)) {
        return 3;
        /* can_resume = FALSE; */
        /* complete = FALSE; */
      }

      // unpaused

      if (step->real_st) {
        double xtime = lives_get_session_time();
        step->paused_time += xtime;
        step->state = STEP_STATE_RUNNING;
      } else step->state = STEP_STATE_WAITING;
    }

    if (lives_proc_thread_is_paused(lpt)) {
      step->state = STEP_STATE_PAUSED;
      return 1;
    }

    if (!lives_proc_thread_is_done(lpt, FALSE)) return 1;
  }

  xtime = lives_get_session_time();
  step->tdata->real_end = xtime;
  step->tdata->real_duration = 1000. * (step->tdata->real_end
                                        - step->tdata->real_start - step->tdata->paused_time);
  plan->tdata->sequential_time += step->tdata->real_duration;

  if (lpt) {
    if (lives_proc_thread_had_error(lpt)) {
      step->errmsg = lives_proc_thread_get_errmsg(lpt);
      step->state = STEP_STATE_ERROR;
    } else if (lives_proc_thread_was_cancelled(lpt)) {
      step->state = STEP_STATE_CANCELLED;
    } else {
      step->state = STEP_STATE_FINISHED;
    }
  } else step->state = STEP_STATE_ERROR;

  return 0;
}


static lives_proc_thread_t ann_proc = NULL;


void ann_roll_cancel(void) {
  if (!ann_proc) return;
  if (lives_proc_thread_ref(ann_proc) > 1) {
    lives_proc_thread_request_cancel(ann_proc, TRUE);
    lives_proc_thread_unref(ann_proc);
    ann_proc = NULL;
  }
}


static void ann_roll_launch(void) {
  if (ann_proc) return;
  ann_proc =  lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                       ann_roll, WEED_SEED_BOOLEAN, "", NULL);
  lives_proc_thread_auto_nullify(ann_proc, TRUE);
  lives_proc_thread_set_pauseable(ann_proc, TRUE);
  lives_proc_thread_set_cancellable(ann_proc);
  //mainw->debug_ptr = ann_proc;
  lives_proc_thread_queue(ann_proc, 0);
}


static void glob_timing_init(void) {
  if (glob_timing) return;
  int lcounts[] = TIMING_ANN_LCOUNTS;
  glob_timing = LIVES_CALLOC_SIZEOF(glob_timedata_t, 1);
  glob_timing->ann = lives_ann_create(TIMING_ANN_NLAYERS, lcounts);
  pthread_mutex_init(&glob_timing->upd_mutex, NULL);
  pthread_mutex_init(&glob_timing->ann_mutex, NULL);
  // the predictor is VERY sensitive to inital conditions
  // but with these values it can usually train itself in under 50 generations
  lives_ann_init_seed(glob_timing->ann, .001);
  lives_ann_set_variance(glob_timing->ann, 1.0, 1.0, 0.9999, 2);
  glob_timing->cpuloadvar = get_core_loadvar(0);
  //ann_roll_launch();
}

#define ANN_MAX_DPOINTS 200


static ann_testdata_t *tst_data_copy(ann_testdata_t *tstdata, int nins) {
  LIVES_CALLOC_TYPE(ann_testdata_t, trndata, 1);
  lives_memcpy(trndata, tstdata, sizeof(ann_testdata_t));
  trndata->inputs = LIVES_CALLOC_SIZEOF(double, nins);
  lives_memcpy(trndata->inputs, tstdata->inputs, nins * sizdbl);
  return trndata;
}


static void extract_timedata(exec_plan_t *plan) {
  // go though plan steps and scavenge timer data
  for (LiVESList *list = plan->steps; list; list = list->next) {
    plan_step_t *step = (plan_step_t *)list->data;
    if (!step) continue;
    if (step->state == STEP_STATE_FINISHED) {
      switch (step->st_type) {
      case STEP_TYPE_CONVERT:
        // what we are interested in here are the sub-step timings
        // resize and palcovn - we will use this to make training data for the ANN
        for (LiVESList *sublist = step->substeps; sublist; sublist = sublist->next) {
          ann_testdata_t *tstdata;
          exec_plan_substep_t *substep = (exec_plan_substep_t *)sublist->data;
          float cpuload = substep->cpuload;


          switch (substep->op_idx) {
          case OP_PCONV:
          case OP_RESIZE: {
            double outsize = (double)lives_frame_calc_bytesize(substep->width, substep->height, substep->pal, FALSE, NULL);
            double insize = (double)lives_frame_calc_bytesize(step->fin_iwidth, step->fin_iheight,
							      step->fin_pal, FALSE, NULL);
            tstdata  = LIVES_CALLOC_SIZEOF(ann_testdata_t, 1);
            tstdata->inputs = LIVES_CALLOC_SIZEOF(double, glob_timing->ann->lcount[0]);

            tstdata->inputs[TIMING_ANN_OUTSIZE] = outsize / 1000000.;
            tstdata->inputs[TIMING_ANN_INSIZE] = insize / 1000000.;

            tstdata->inputs[TIMING_ANN_CPULOAD] = substep->cpuload / 50.;
            tstdata->inputs[TIMING_ANN_PBQ_BASE + substep->pb_quality] = 100.;
            if (substep->pal != step->fin_pal) {
              double pval = 1.;
              if ((weed_palette_is_rgb(substep->pal) && weed_palette_is_yuv(step->fin_pal))
                  || (weed_palette_is_yuv(substep->pal) && weed_palette_is_rgb(step->fin_pal))) pval = 5;
              tstdata->inputs[TIMING_ANN_OUT_PAL_BASE + get_enum_palette(substep->pal)] = pval;
              tstdata->inputs[TIMING_ANN_IN_PAL_BASE + get_enum_palette(step->fin_pal)] = pval;
            }
            if (insize == outsize && substep->pal == step->fin_pal)
              tstdata->res = 0.;
            else tstdata->res = (substep->end - substep->start - substep->paused) * 1000.;

            int max = 1;
            //if (plan->model->flags & NODEMODEL_NEW) max = ANN_MAX_DPOINTS / (1 + plan->iteration);
            for (int rpt = 0; rpt < max; rpt++) {
              ann_testdata_t *trndata = tstdata;
              if (rpt < max - 1) trndata = tst_data_copy(tstdata, glob_timing->ann->lcount[0]);
              glob_timing->ann->tstdata = lives_list_prepend(glob_timing->ann->tstdata,
							     (void *)trndata);
              if (!glob_timing->ann->last_data)
                glob_timing->ann->last_data = glob_timing->ann->tstdata;
              if (glob_timing->ann->ndatapoints < ANN_MAX_DPOINTS)
                glob_timing->ann->ndatapoints++;
              else {
                LiVESList *old_last = glob_timing->ann->last_data;
                glob_timing->ann->last_data = old_last->prev;
                old_last->prev->next = NULL;
                old_last->prev = NULL;
                trndata = (ann_testdata_t *)old_last->data;
                if (trndata) {
                  if (trndata->inputs) lives_free(trndata->inputs);
                  lives_free(trndata);
                }
                lives_list_free(old_last);
              }
            }
          }
	    break;
          default: break;
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*
}

static double inc_running_steps(plan_step_t *step) {
  exec_plan_t *plan = step->plan;
  double xtime = lives_get_session_time();
  plan->nsteps_running++;
  if (plan->nsteps_running == 1) {
    plan->tdata->waiting_time += xtime;
    if (plan->tdata->actual_start)  plan->tdata->active_pl_time -= xtime;
  } else plan->tdata->concurrent_time -= xtime;
  step->tdata->real_start = xtime;
  return xtime;
}


static double dec_running_steps(plan_step_t *step) {
  exec_plan_t *plan = step->plan;
  double xtime = lives_get_session_time();
  if (!plan->nsteps_running) return xtime;
  plan->nsteps_running--;
  if (!plan->nsteps_running) {
    plan->tdata->waiting_time -= xtime;
    if (plan->tdata->actual_start)
      plan->tdata->active_pl_time += xtime;
  } else if (plan->nsteps_running >= 1)
    plan->tdata->concurrent_time += xtime;
 
  step->tdata->real_end = xtime;
  step->tdata->real_duration = 1000. * (step->tdata->real_end - step->tdata->real_start
                                        - step->tdata->paused_time);
  plan->tdata->sequential_time += step->tdata->real_duration;
  return xtime;
}


#define SET_PLAN_STATE(xstate) _DW0(plan->state = PLAN_STATE_##xstate;)

static void run_plan(exec_plan_t *plan) {
  // cycle through the plan steps
  // skip over any flagged as running, finished, error or ignore
  GET_PROC_THREAD_SELF(self);
  pthread_mutex_t *pause_mutex;
  plan_step_t *step;
  lives_layer_t *layer;
  lives_proc_thread_t lpt;
  double xtime;
  double errval;
  boolean complete = FALSE;
  boolean cancelled = FALSE;
  boolean paused = FALSE;
  boolean got_act_st = FALSE;
  int error = 0, res;
  int lstatus, i, state;
  int nloops = 0;

  if (!plan) return;

  if (nplans > 1) lives_abort("multiple plans cannot be active simultaneously");
  
  ____FUNC_ENTRY____(run_plan, NULL, "v");

  //bbsummary();

  MSGMODE_ON(DEBUG);

  //if (!ann_proc) ann_roll_launch();
  if (plan->iteration == 1) {
    glob_timing->tot_duration = glob_timing->avg_duration = 0.;
    if (ann_proc && (plan->model->flags & NODEMODEL_NEW))
      lives_proc_thread_set_loveliness(ann_proc, DEF_LOVELINESS);
  }

  if (lives_proc_thread_get_cancel_requested(self)) {
    lives_proc_thread_cancel(self);
  }

  plan->tdata->trun_time = lives_get_session_time();
  plan->tdata->queued_time = plan->tdata->trun_time - plan->tdata->exec_time;

  pause_mutex = &THREADVAR(pause_mutex);
  pthread_mutex_lock(pause_mutex);

  if (!plan->tdata->trigger_time) {
    SET_PLAN_STATE(WAITING);

    // if cycle has not yet been triggered, we are going to sleep until it is
    // however, if cancel is requested, when calling the pause_function,
    // this thread will immediately cancelled
    // this is OK since we set a callback for this

    // wait for plan cycle to be triggered
    _lives_proc_thread_pause(self, TRUE);
    plan->tdata->trigger_time = lives_get_session_time();
  }
  pthread_mutex_unlock(pause_mutex);

  plan->tdata->start_wait = plan->tdata->trigger_time - plan->tdata->trun_time;

  d_print_debug("plan triggered @ %.2f msec\n", plan->tdata->trigger_time * 1000.);

  if (lives_proc_thread_get_cancel_requested(self)) {
    MSGMODE_OFF(DEBUG);
    ____FUNC_EXIT____;
    lives_proc_thread_cancel(self);
  }

  SET_PLAN_STATE(RUNNING);

  plan->tdata->real_start = lives_get_session_time();
  plan->tdata->preload_time -= plan->tdata->real_start;

  plan->tdata->tgt_time = 1. / abs(mainw->files[mainw->playing_file]->pb_fps);

  plan->tdata->waiting_time = -plan->tdata->real_start;

  if (!glob_timing->cpuloadvar)
    glob_timing->cpuloadvar = get_core_loadvar(0);

  if (glob_timing->cpuloadvar) {
    glob_timing->curr_cpuload = (float)(*glob_timing->cpuloadvar);
    glob_timing->active = TRUE;
  }
  do {
    int step_count = 0;
    boolean can_resume = TRUE;
    complete = TRUE;

    if (nloops++ & 0x40) {
      nloops = 0;
      if (!glob_timing->cpuloadvar)
        glob_timing->cpuloadvar = get_core_loadvar(0);
      if (glob_timing->cpuloadvar) {
        glob_timing->curr_cpuload = (float)(*glob_timing->cpuloadvar);
        glob_timing->active = TRUE;
      }
    }

    for (LiVESList *steps = plan->steps; steps; steps = steps->next) {
      step_count++;

      if (!plan->layers) {
        d_print_debug("no layers for plan\n");
        error = 1;
      }

      if (error && !plan->nsteps_running) break;

      if (!cancelled && lives_proc_thread_get_cancel_requested(self))
        cancelled = TRUE;

      if (cancelled && !plan->nsteps_running) break;

      if (!paused && lives_proc_thread_get_pause_requested(self))
        paused = TRUE;

      if (paused) {
        if (lives_proc_thread_get_resume_requested(self)) {
          paused = FALSE;
          if (!plan->nsteps_running) SET_PLAN_STATE(RUNNING);
          else SET_PLAN_STATE(RESUMING);
        } else if (!plan->nsteps_running) break;
      }

      if (!got_act_st && plan->tdata->actual_start) {
        got_act_st = TRUE;
        plan->tdata->preload_time += lives_get_session_time();
        if (plan->nsteps_running) plan->tdata->active_pl_time += lives_get_session_time();
      }

      step = (plan_step_t *)steps->data;

      if (!step) continue;

      state = step->state;
      step->count = step_count;

      if (state == STEP_STATE_ERROR) {
        if (!error) d_print_debug("step encountered an ERROR st of loop:- %s",
				  step->errmsg ? step->errmsg : "unspecified error");
        error = 2;
      }

      if (state == STEP_STATE_ERROR || state == STEP_STATE_FINISHED
          || state == STEP_STATE_SKIPPED || state == STEP_STATE_IGNORE
          || state == STEP_STATE_CANCELLED) continue;

      // need to check if layer is in state LOADED and
      if (state != STEP_STATE_RUNNING && !(plan->state == STEP_STATE_RESUMING
                                           && step->state == STEP_STATE_PAUSED)) {
        if (cancelled || paused || error) {
          if (cancelled) step->state = STEP_STATE_CANCELLED;
          if (paused) step->state = STEP_STATE_PAUSED;
          if (error) step->state = STEP_STATE_SKIPPED;
          continue;
        }

        step->state = STEP_STATE_WAITING;

        complete = FALSE;

        // ensure dependencie are fullfilled
        if (step->st_type == STEP_TYPE_LOAD) {
          frames64_t frame;
          // ensure the layer exists and is in state PREPARED
          if (!plan->layers) {
            d_print_debug("no layers for plan2\n");
            error = 3;
            continue;
          }

          layer = plan->layers[step->track];

          if (!layer && cancelled) {
            if (step->state == STEP_STATE_RUNNING) dec_running_steps(step);
            step->state = STEP_STATE_CANCELLED;
            continue;
          }

          if (layer) {
            if (!weed_layer_check_valid(layer)) {
              error = 4;
              if (step->state == STEP_STATE_RUNNING) dec_running_steps(step);
              step->state = STEP_STATE_ERROR;
              continue;
            }

            if (!(step->flags & STEP_FLAG_NO_READY_STAT)) {
              // OK
              lock_layer_status(layer);
              if (_lives_layer_get_status(layer) == LAYER_STATUS_LOADED)
                _lives_layer_set_status(layer, LAYER_STATUS_READY);
              unlock_layer_status(layer);
            }

            frame = lives_layer_get_frame(layer);
            plan->frame_idx[step->track] = frame;
            plan->template->frame_idx[step->track] = plan->frame_idx[step->track];

            lstatus = lives_layer_get_status(layer);
            if (lstatus == LAYER_STATUS_READY || lstatus == LAYER_STATUS_LOADED) {
              d_print_debug("\nstep %d LOAD skipped, frame appeared on track %d as if by magic !\n",
                            step_count, step->track);

              if (step->state == STEP_STATE_RUNNING)
                dec_running_steps(step);

              step->state = STEP_STATE_SKIPPED;
              continue;
            }
            if (lstatus != LAYER_STATUS_PREPARED) continue;

            if (!(step->flags & STEP_FLAG_TAGGED_LAYER)) {
              weed_set_boolean_value(layer, LIVES_LEAF_PLAN_CONTROL, TRUE);
              step->flags |= STEP_FLAG_TAGGED_LAYER;
              d_print_debug("TAGGED layer\n");
            }

            /* if (frame == plan->template->frame_idx[step->track]) { */
            /*   plan->layers[step->track] = get_cached_frame(step->track); */
            /*   // now skip set all deps as done up to point where frame was cached */
            /*   // status should be READY */
            /*   continue; */
            /* } */
          } else {
            // !layer
            frame = plan->frame_idx[step->track];
            if (!frame) continue;

            /* if (frame == plan->template->frame_idx[step->track]) { */
            /*   plan->layers[step->track] = get_cached_frame(step->track); */
            /*   // now skip set all deps as done up to point where frame was cached */
            /*   // status should be READY */
            /*   continue; */
            /* } */

            plan->template->frame_idx[step->track] = frame;

            layer = plan->layers[step->track]
	      = lives_layer_new_for_frame(plan->model->clip_index[step->track],
					  plan->frame_idx[step->track]);

            lives_layer_set_srcgrp(layer, mainw->track_sources[step->track]);
            lives_layer_set_track(layer, step->track);
            weed_set_boolean_value(layer, LIVES_LEAF_PLAN_CONTROL, TRUE);
            step->flags |= STEP_FLAG_TAGGED_LAYER;
            lives_layer_set_status(layer, LAYER_STATUS_PREPARED);
            // LOAD prep
          }
          plan->template->frame_idx[step->track] = frame;
        } else {
          // check if all dependency steps are done
          for (i = 0; i < step->ndeps; i++) {
            plan_step_t *xstep = step->deps[i];

            int state = xstep->state;
            if (state == STEP_STATE_ERROR || state == STEP_STATE_CANCELLED
                || state == STEP_STATE_PAUSED) {
              if (state != STEP_STATE_PAUSED) step->state = STEP_STATE_SKIPPED;
              continue;
            }

            if (!(state == STEP_STATE_FINISHED || state == STEP_STATE_SKIPPED
                  || state == STEP_STATE_IGNORE))  break;
          }
          if (i < step->ndeps) continue;
        }

        if (error || cancelled) {
          step->state = STEP_STATE_SKIPPED;
          continue;
        }

        xtime = lives_get_session_time();

        switch (step->st_type) {
        case STEP_TYPE_LOAD: {
          lives_clip_t *sfile;
          int clipno = plan->model->clip_index[step->track];
          sfile = RETURN_VALID_CLIP(clipno);
          if (sfile) layer = plan->layers[step->track];
          else layer = NULL;

          if (!layer) {
            g_print("layer invalid, sfile is %p\n", sfile);
            if (step->state == STEP_STATE_RUNNING)
              dec_running_steps(step);
            step->state = STEP_STATE_ERROR;
            error = 5;
            break;
         }
          if (!weed_layer_check_valid(layer)) {
            g_printerr("layer on track %d not valid\n", step->track);
            error = 6;
            if (step->state == STEP_STATE_RUNNING)
              dec_running_steps(step);
            step->state = STEP_STATE_ERROR;
            break;
          }

          xtime = inc_running_steps(step);
          d_print_debug("\nstep %d; RUN LOAD - track %d, clip %d, frame %ld, @ %.2f msec\n", step_count,
                        step->track, step->target_idx, plan->frame_idx[step->track], xtime * 1000.);
          step->state = STEP_STATE_RUNNING;
          pull_frame_threaded(layer, sfile->hsize, sfile->vsize);
        }
	  break;
        case STEP_TYPE_CONVERT: {
          layer = plan->layers[step->track];
          if (!layer) continue;
          if (!weed_layer_check_valid(layer)) {
            error = 7;
            g_printerr("layer on track %d not valid during conversion\n", step->track);
            if (step->state == STEP_STATE_RUNNING)
              dec_running_steps(step);
            step->state = STEP_STATE_ERROR;
            break;
          }
          step->ini_pal = weed_layer_get_palette(layer);
          step->ini_width = weed_layer_get_width(layer);
          step->ini_height = weed_layer_get_height(layer);
          step->ini_gamma = weed_layer_get_gamma(layer);
          d_print_debug("\nstep %d: RUN CONVERT (track %d) @ %.2f msec: "
                        "from pal %s, %d X %d, gamma %s ---> pal %s, %d X %d gamma %s\n", step_count,
                        step->track, 1000. * xtime, weed_palette_get_name(step->ini_pal), step->ini_width, step->ini_height,
                        weed_gamma_get_name(step->ini_gamma), weed_palette_get_name(step->fin_pal),
                        step->fin_width, step->fin_height, weed_gamma_get_name(step->fin_gamma));

          // figure out the sequence of operations needed, construct a prochthread chain,
          // then queue it
          //double est_dur;
          int op_order[N_OP_TYPES];
          int out_width = weed_layer_get_width(layer), out_height = weed_layer_get_height(layer);
          int in_width = step->fin_width, in_height = step->fin_height;
          int in_iwidth = step->fin_iwidth, in_iheight = step->fin_iheight;
          int outpl = weed_layer_get_palette(layer), inpl = step->fin_pal;
          int out_gamma_type = weed_layer_get_gamma(layer), in_gamma_type = step->fin_gamma;
          int flags = OPORD_EXPLAIN;

          if (in_iwidth < in_width || in_iheight < in_height)
            flags |= OPORD_LETTERBOX;

          // if this is a pre-conversion for a clip, we won't have a target pal or gamma
          // we have to get these from the srcgroup

          if (inpl == WEED_PALETTE_NONE) {
            lives_clipsrc_group_t *srcgrp = mainw->track_sources[step->track];
            int clipno = plan->model->clip_index[step->track];
            if (!srcgrp) srcgrp = get_primary_srcgrp(clipno);
            inpl = srcgrp->apparent_pal;
            in_gamma_type = srcgrp->apparent_gamma;
          }

          get_op_order(out_width, out_height, in_iwidth, in_iheight, flags, outpl, inpl,
                       out_gamma_type, in_gamma_type, op_order);

          if (in_iwidth < in_width || in_iheight < in_height)
            d_print_debug("Needs letterbox, as we have outer size %d X %d and inner size %d X %d\n",
                          in_width, in_height, in_iwidth, in_iheight);

          if (!op_order[OP_RESIZE] && !op_order[OP_PCONV] && !op_order[OP_GAMMA] && !op_order[OP_LETTERBOX]) {
            //if (prefs->dev_show_timing)
            d_print_debug("No conversion needed - skipping step\n");
            step->state = STEP_STATE_SKIPPED;

            dec_running_steps(step);

            if (weed_layer_check_valid(layer)) {
              if (!(step->flags & STEP_FLAG_NO_READY_STAT)) {
                d_print_debug("set layer %d stat to ready\n", step->track);
                lives_layer_set_status(layer, LAYER_STATUS_READY);
              } else {
                d_print_debug("set layer %d stat to loaded\n", step->track);
                lives_layer_set_status(layer, LAYER_STATUS_LOADED);
              }
            }
            break;
          }

          lives_layer_set_status(layer, LAYER_STATUS_CONVERTING);

          /* est_dur = get_conversion_cost(COST_TYPE_TIME, out_width, out_height, */
          /*                               in_iwidth, in_iheight, letterbox, */
          /*                               outpl, inpl, NULL, */
          /*                               out_gamma_type, in_gamma_type, FALSE); */

          //d_print_debug("online estimate is %.4f msec\n", est_dur * 1000.);

          d_print_debug("ORDER of ops is: (res %d, pconv %d, gamma %d, lb %d)\n",
                        op_order[OP_RESIZE], op_order[OP_PCONV], op_order[OP_GAMMA], op_order[OP_LETTERBOX]);
          if (op_order[OP_RESIZE] == 1) {
            // RESIZE IS 1
            d_print_debug("Resize");
            // check combined
            if (op_order[OP_PCONV] == 1)
              d_print_debug(" + palconv");
            if (op_order[OP_GAMMA] == 1)
              d_print_debug(" + gamma");
            if (op_order[OP_LETTERBOX] == 1)
              d_print_debug(" + letterbox");

            lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                           res_substep, WEED_SEED_INT, "v", step);
          } else if (op_order[OP_PCONV] == 1) {
            // pconv is 1
            d_print_debug("Palconv");
            if (op_order[OP_GAMMA] == 1)
              d_print_debug(" + gamma");
            lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                           pconv_substep, WEED_SEED_INT, "v", step);
          } else if (op_order[OP_LETTERBOX] == 1) {
            d_print_debug("Letterbox");
            lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                           lbox_substep, WEED_SEED_INT, "v", step);
          } else {
            d_print_debug("Gamma");
            lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                           gamma_substep, WEED_SEED_INT, "v", step);
          }
          //
          if (op_order[OP_RESIZE] == 2) {
            d_print_debug(", resize");
            if (op_order[OP_PCONV] == 2)
              d_print_debug(" + palconv");
            if (op_order[OP_GAMMA] == 2)
              d_print_debug(" + gamma");
            lives_proc_thread_chain(lpt, res_substep, WEED_SEED_INT, "v", step, NULL);
          } else if (op_order[OP_PCONV] == 2) {
            d_print_debug(", palconv");
            if (op_order[OP_GAMMA] == 2)
              d_print_debug(" + gamma");
            lives_proc_thread_chain(lpt, pconv_substep, WEED_SEED_INT, "v", step, NULL);
          } else if (op_order[OP_GAMMA] == 2) {
            d_print_debug(", gamma");
            lives_proc_thread_chain(lpt, gamma_substep, WEED_SEED_INT, "v", step, NULL);
          } else if (op_order[OP_LETTERBOX] == 2) {
            d_print_debug(", letterbox");
            lpt = lives_proc_thread_chain(lpt, lbox_substep, WEED_SEED_INT, "v", step, NULL);
          }

          if (op_order[OP_RESIZE] == 3) {
            d_print_debug(", resize");
            if (op_order[OP_PCONV] == 3)
              d_print_debug(" + palconv");
            if (op_order[OP_GAMMA] == 3)
              d_print_debug(" + gamma");
            lives_proc_thread_chain(lpt, res_substep, WEED_SEED_INT, "v", step, NULL);
          } else if (op_order[OP_PCONV] == 3) {
            if (op_order[OP_GAMMA] == 3) {
              d_print_debug(" + gamma");
              lives_proc_thread_chain(lpt, pconv_substep, WEED_SEED_INT, "v", step, NULL);
            }
          } else if (op_order[OP_GAMMA] == 3) {
            d_print_debug(", gamma");
            lives_proc_thread_chain(lpt, gamma_substep, WEED_SEED_INT, "v", step, NULL);
          } else if (op_order[OP_LETTERBOX] == 3) {
            d_print_debug(", letterbox");
            lpt = lives_proc_thread_chain(lpt, lbox_substep, WEED_SEED_INT, "v", step, NULL);
          }

          if (op_order[OP_LETTERBOX] == 4) {
            d_print_debug(", letterbox");
            lpt = lives_proc_thread_chain(lpt, lbox_substep, WEED_SEED_INT, "v", step, NULL);
          }

          d_print_debug("\n");

          SET_LPT_VALUE(lpt, int, "interp", get_interp_value(prefs->pb_quality, TRUE));

          step->proc_thread = lpt;
          step->state = STEP_STATE_RUNNING;

          lives_proc_thread_set_cancellable(lpt);

          // queue lpt and have it remove proc_thread from layer after
          lives_layer_async_auto(layer, lpt);

          inc_running_steps(step);
        }
	  break;

        case STEP_TYPE_APPLY_INST: {
          weed_instance_t *inst;
          if (!step->target) {
            d_print_debug("\nstep %d: Output to sink ready @ %.2f msec\n", step_count, xtime * 1000.);
            step->state = STEP_STATE_FINISHED;
            //dec_running_steps(step);
            break;
          }

          inst = rte_keymode_get_instance(step->target_idx + 1, rte_key_getmode(step->target_idx + 1));
          if (!(step->flags & STEP_FLAG_RUN_AS_LOAD)) {
            d_print_debug("\nstep %d: RUN APPLY_INST (%s) @ %.2f msec, pd i %p\n",
                          step_count, weed_filter_get_name((weed_filter_t *)step->target),
                          xtime * 1000., weed_layer_get_pixel_data(plan->layers[0]));

            if (weed_get_boolean_value(inst, LIVES_LEAF_SOFT_DEINIT, NULL)) {
              d_print_debug("\nstep %d APPLY INST  skipped, soft deinit !\n",
                            step->count, step->track);
              step->state = STEP_STATE_SKIPPED;
              break;
            }

            step->proc_thread =
              lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                       run_apply_inst_step, WEED_SEED_INT, "vv", step, inst);
          } else {
            //run_apply_inst_step(step, NULL);
            step->st_type = STEP_TYPE_LOAD;
          }
          step->state = STEP_STATE_RUNNING;
          inc_running_steps(step);
        }
	  break;
        default: break;
        }
      }

      if (state == STEP_STATE_RUNNING || (plan->state == STEP_STATE_RESUMING
                                          && step->state == STEP_STATE_PAUSED)) {
        complete = FALSE;
        // check if step has completed
        switch (step->st_type) {
        case STEP_TYPE_LOAD: {
          // finishes when state is LOADED or READY
          layer = plan->layers[step->track];
          if (!layer) {
            step->state = STEP_STATE_ERROR;
            error = 8;

            xtime = dec_running_steps(step);
            d_print_debug("LOAD (track %d) done @ %.2f msec, duration %.2f\n", step->track, xtime * 1000.,
                          step->tdata->real_duration);
            d_print_debug("layer disappeared !\n");

            break;
          }
          if (!weed_layer_check_valid(layer)) {
            error = 9;
            step->state = STEP_STATE_ERROR;

            xtime = dec_running_steps(step);

            d_print_debug("LOAD (track %d) done @ %.2f msec, duration %.2f\n", step->track, xtime * 1000.,
                          step->tdata->real_duration);
            d_print_debug("layer invalid !\n");

            break;
          }
          if (step->proc_thread) {
            double xtime;
            exec_plan_substep_t *substep = NULL;
            if (!lives_proc_thread_is_done(step->proc_thread, FALSE)) break;
            xtime = lives_get_session_time();
            lives_proc_thread_join(step->proc_thread);
            d_print_debug("deinterlace completed @ \n", xtime * 1000.);
            for (LiVESList *list = step->substeps; list; list = list->next) {
              substep = (exec_plan_substep_t *)list->data;
              substep->end = xtime;
              if (substep->op_idx == OP_DEINTERLACE) break;
              substep = NULL;
            }
            if (substep) {
              double dur = xtime - substep->start;
              d_print_debug("Done in %.2f msec\n", dur * 1000.);
            }
            lives_layer_set_status(layer, LAYER_STATUS_LOADED);
          } else {
            lpt = lives_layer_get_proc_thread(layer);
            lstatus = lives_layer_get_status(layer);
            if (lstatus == LAYER_STATUS_LOADING || lstatus == LAYER_STATUS_CONVERTING) {
              if (lpt) {
                if (error || cancelled) {
                  if (!lives_proc_thread_get_cancel_requested(lpt)) {
                    weed_layer_set_invalid(layer, TRUE);
                    lives_proc_thread_request_cancel(lpt, FALSE);
                    if (lives_proc_thread_was_cancelled(lpt)) {
                      weed_layer_set_invalid(layer, TRUE);
                      step->state = STEP_STATE_CANCELLED;
                    }

                    dec_running_steps(step);
                    break;
                  }
                  if (paused) {
                    if (!lives_proc_thread_get_pause_requested(lpt))
                      lives_proc_thread_request_pause(lpt);
                    if (lives_proc_thread_is_paused(lpt)) {
                      xtime = lives_get_session_time();
                      step->state = STEP_STATE_PAUSED;
                      xtime = lives_get_session_time();
                      step->tdata->paused_time -= xtime;
                    }
                    break;
                  }
                  if (lives_proc_thread_was_cancelled(lpt)) {
                    step->state = STEP_STATE_CANCELLED;
                    weed_layer_set_invalid(layer, TRUE);

                    xtime = dec_running_steps(step);
                    d_print_debug("LOAD (track %d) done @ %.2f msec, duration %.2f\n", step->track, xtime * 1000.,
                                  step->tdata->real_duration);
                    d_print_debug("plan cancelled !\n");
                    break;
                  }
                  if (plan->state == STEP_STATE_RESUMING) {
                    if (!lives_proc_thread_get_resume_requested(lpt))
                      lives_proc_thread_request_resume(lpt);

                    if (lives_proc_thread_is_paused(lpt)) {
                      can_resume = FALSE;
                      break;
                    }
                    xtime = lives_get_session_time();
                    step->tdata->paused_time += xtime;
                    step->state = STEP_STATE_RUNNING;
                  }
                  if (lives_proc_thread_is_paused(lpt)) {
                    xtime = lives_get_session_time() ;
                    step->tdata->paused_time += xtime;
                    step->state = STEP_STATE_PAUSED;
                    break;
                  }
                }
              }
            }
          }
          lstatus = lives_layer_get_status(layer);
          if (lstatus == LAYER_STATUS_LOADED || lstatus == LAYER_STATUS_READY) {
            xtime = lives_get_session_time();

            if (!weed_layer_get_width(layer) || !weed_layer_get_height(layer)) BREAK_ME("size 0 layer");

            d_print_debug("LOAD (track %d) done @ %.2f msec, duration %.2f\n", step->track, xtime * 1000.,
                          step->tdata->real_duration);

            if (weed_get_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, NULL)) {
              exec_plan_substep_t *sub = make_substep(OP_DEINTERLACE, xtime, weed_layer_get_width(layer),
                                                      weed_layer_get_height(layer), weed_layer_get_palette(layer));
              step->substeps = lives_list_append(step->substeps, (void *)sub);
              d_print_debug("Layer needs deinterlacing\n");
              lives_layer_set_status(layer, LAYER_STATUS_CONVERTING);
              weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, FALSE);
              step->proc_thread = lives_proc_thread_create(LIVES_THRDATTR_NONE, deinterlace_frame, -1, "vI", layer,
							   mainw->currticks);
            } else {
              if (!(step->flags & STEP_FLAG_NO_READY_STAT))
                lives_layer_set_status(layer, LAYER_STATUS_READY);
              else lives_layer_set_status(layer, LAYER_STATUS_LOADED);
              if (step->flags & STEP_FLAG_RUN_AS_LOAD) {
                weed_instance_t *inst =
                  rte_keymode_get_instance(step->target_idx + 1, rte_key_getmode(step->target_idx + 1));
                int layer_gamma = weed_layer_get_gamma(layer);
                if (layer_gamma == WEED_GAMMA_LINEAR) {
                  // if we scaled params, scale them back so they are displayed correctly in interfaces
                  gamma_conv_params(WEED_GAMMA_SRGB, inst, TRUE);
                  gamma_conv_params(WEED_GAMMA_SRGB, inst, FALSE);
                }
              }
              step->state = STEP_STATE_FINISHED;
              dec_running_steps(step);
            }
            break;
          }
        }
	  break;

        case STEP_TYPE_CONVERT: {
          double gstart;
          layer = plan->layers[step->track];
          if (!layer) {
            dec_running_steps(step);
            d_print_debug("CONVERT (track %d) done @ %.2f msec, duration %.2f\n", step->track,
                          step->tdata->real_end * 1000., step->tdata->real_duration);
            d_print_debug("Something happened to layer !\n");

            step->state = STEP_STATE_ERROR;
            error = 10;
            break;
          }
          if (!weed_layer_check_valid(layer)) {
            if (!cancelled && !error) {
              dec_running_steps(step);
              d_print_debug("CONVERT (track %d) done @ %.2f msec, duration %.2f\n", step->track,
                            step->tdata->real_end * 1000., step->tdata->real_duration);
              g_printerr("layer on track %d became invalid during conversion\n", step->track);
              error = 11;
              step->state = STEP_STATE_ERROR;
              break;
            }
          }
          res = check_step_condition(plan, step, cancelled, error, paused);
          if (!res) {
            if (step->state == STEP_STATE_ERROR) {
              d_print_debug("step convert encountered an ERROR: %s\n",
                            step->errmsg ? step->errmsg : "unspecified error");
              if (layer) weed_layer_set_invalid(layer, TRUE);
              error = 12;
            } else if (step->state == STEP_STATE_CANCELLED) {
              if (layer) weed_layer_set_invalid(layer, TRUE);
            } else {
              if (weed_layer_check_valid(layer)) {
                if (!(step->flags & STEP_FLAG_NO_READY_STAT))
                  lives_layer_set_status(layer, LAYER_STATUS_READY);
                else lives_layer_set_status(layer, LAYER_STATUS_LOADED);
              } else {
                d_print_debug("Layer %d is invalid !!!!\n", step->track);
                step->state = STEP_STATE_ERROR;
              }

              lpt = step->proc_thread;
              lives_proc_thread_join_int(lpt);

              gstart = lives_proc_thread_get_double_value(lpt, "gconv_start");
              if (gstart) {
                size_t frmsize = lives_frame_calc_bytesize(step->fin_width, step->fin_height,
							   step->fin_pal, FALSE, NULL);
                double gend = lives_proc_thread_get_double_value(lpt, "gconv_end");
                glob_timing->gbytes_per_sec = frmsize / (gend - gstart);
              }

              weed_leaf_delete(layer, LIVES_LEAF_PROC_THREAD);

              step->proc_thread = NULL;
              lives_proc_thread_unref(lpt);
              dec_running_steps(step);

              d_print_debug("CONVERT (track %d) done @ %.2f msec, duration %.2f\n", step->track,
                            step->tdata->real_end * 1000., step->tdata->real_duration);
              if (step->tdata->est_duration)
                d_print_debug("EST DURATION %f vs real duration %f\n", step->tdata->est_duration  * 1000.,
                              step->tdata->real_duration);
            }
          } else {
            complete = FALSE;
            if (res & 2) can_resume = FALSE;
          }
        }
	  break;
        case STEP_TYPE_APPLY_INST:
          res = check_step_condition(plan, step, cancelled, error, paused);
          if (!res) {
            if (step->state == STEP_STATE_ERROR) {
              d_print_debug("step appl inst encountered an ERROR: %s\n",
                            step->errmsg ? step->errmsg : "unspecified error");
              error = 13;
            } else if (step->state == STEP_STATE_CANCELLED) {
              d_print_debug("step cancelled\n");
            } else {
              lives_filter_error_t filter_error;

              // filter_errors include:
              /* FILTER_ERROR_INVALID_PLUGIN; */
              /* FILTER_ERROR_INVALID_FILTER; */
              /* FILTER_ERROR_BUSY; */
              /* FILTER_ERROR_NEEDS_REINIT; */

              // thse should have been checked for already:
              /* FILTER_ERROR_INVALID_INSTANCE - NULL instance */
              /* FILTER_ERROR_IS_AUDIO */

              filter_error = lives_proc_thread_join_int(step->proc_thread);

              // grab all the juicy timing data from lpt and store it in the summary
              lives_proc_thread_unref(step->proc_thread);

              if (filter_error == FILTER_ERROR_INVALID_INSTANCE || filter_error == FILTER_ERROR_IS_AUDIO
                  || filter_error == FILTER_ERROR_BUSY || filter_error == FILTER_INFO_BYPASSED) {
                step->state = STEP_STATE_SKIPPED;
              }
            }
            dec_running_steps(step);
            d_print_debug("APPL inst done @ %.2f msec, duration %.2f msec\n", 1000. * step->tdata->real_end,
                          step->tdata->real_duration);
          } else {
            if ((cancelled || error) && step->proc_thread
                && !lives_proc_thread_should_cancel(step->proc_thread))
              lives_proc_thread_request_cancel(step->proc_thread, TRUE);
            complete = FALSE;
            if (res & 2) can_resume = FALSE;
          }
          break;
        default: break;
        }
      }

      if (plan->state == PLAN_STATE_RESUMING && can_resume) {
        xtime = lives_get_session_time();
        SET_PLAN_STATE(RUNNING);
        plan->tdata->paused_time += xtime;
      }

      if (complete && paused) {
        xtime = lives_get_session_time();
        plan->tdata->paused_time -= xtime;
        SET_PLAN_STATE(PAUSED);
        lives_proc_thread_pause(self);
        SET_PLAN_STATE(RESUMING);
        complete = FALSE;
        paused = FALSE;
      }
    }
    pthread_yield();
    _lives_microsleep(10);
  } while (!complete);

  xtime = lives_get_session_time();
  plan->tdata->real_end = xtime;
  plan->tdata->waiting_time += xtime;

  for (int i = 0; i < plan->model->ntracks; i++) {
    if (plan->layers) {
      layer = plan->layers[i];
      //weed_layer_ref(layer);
      if (layer) {
        if (plan->state == PLAN_STATE_CANCELLED || plan->state == PLAN_STATE_ERROR)
          weed_layer_set_invalid(layer, TRUE);
        else {
          lock_layer_status(layer);
          if (_lives_layer_get_status(layer) != LAYER_STATUS_READY) {
            _weed_layer_set_invalid(layer, TRUE);
          }
          unlock_layer_status(layer);
        }
        if (weed_plant_has_leaf(layer, LIVES_LEAF_PLAN_CONTROL))
          weed_leaf_delete(layer, LIVES_LEAF_PLAN_CONTROL);
      }
    }
  }

  if (cancelled) {
    d_print_debug("Cancel plan requested\n");
    d_print_debug("Cancelling plan @ %.2f\n", xtime * 1000.);
  } else if (error) {
    SET_PLAN_STATE(ERROR);
    g_print("PLAN error %d\n", error);
    lives_proc_thread_error(self, error, LPT_ERR_MAJOR, "plan error %d", error);
  } else {
    if (lives_proc_thread_get_cancel_requested(self)) {
      d_print_debug("Cancel requested, ignoring as we are done anyway !\n");
    }
  }

  if (plan->state == PLAN_STATE_RUNNING) {
    int gtorun = 0;
    //extract_timedata(plan);
    /* d_print_debug("train nnet\n"); */
    /* if (plan->iteration < 10) { */
    /*   gtorun = plan->iteration; */
    /* } else { */
    /*   if (glob_timing->ann_gens < 200) gtorun = 200 - glob_timing->ann_gens; */
    /*   else { */
    /*     if (!glob_timing->ann->genstorun) { */
    /*       if (glob_timing->ann->last_res > ANN_ERR_THRESH) */
    /*         gtorun = 5; */
    /*       else  gtorun = 1; */
    /*     } */
    /*   } */
    /* } */
    /* if (gtorun) glob_timing->ann->genstorun += gtorun; */

    /* do { */
    /*   // train nnet. With very little data it trains easily */
    /*   // but when we have varied data this is more difficult */
    /*   // and it maybe overtrained */
    /*   nns++; */
    /*   errval = lives_ann_evolve(glob_timing->ann); */
    /*   // want high varaince at first then reduce it */
    /*   if ((mainw->nodemodel->flags & NODEMODEL_TRAINED)) { */
    /*     if (glob_timing->ann->no_change_count > 1000) { */
    /*       glob_timing->ann->nvary = 1; */
    /*       glob_timing->ann->damp = 0.9999; */
    /*     } */
    /*   } */
    /*   // when we get a new model, we do not want to train the estimator too much, else it can get overtrained */
    /*   // on intitial values; however we want to gradually start increasing this until we get reasonable estimates */
    /*   // then we can rebuild the nodemodel and plan with more accurate values */
    /*   //  */
    /* } while (nns <=  */
    /* 	     && nns < ANN_GEN_LIMIT / 5); */

    /* if (nns == ANN_GEN_LIMIT) { */
    /*   lives_ann_set_variance(glob_timing->ann, glob_timing->ann->maxr, glob_timing->ann->maxrb, */
    /* 			     0.9999, 1); */
    /* } */
    /* if (errval <= ANN_ERR_THRESH) */
    /*   glob_timing->ann->flags |= ANN_TRAINED; */

    errval = sqrt(glob_timing->ann->last_res);
    d_print_debug("ann error is %f msec after %d generations\n", errval, glob_timing->ann->generations);

    pthread_mutex_lock(&glob_timing->upd_mutex);
    // update: real_duration, tot_duration and avg_duration

    if (plan->tdata->actual_start)
      plan->tdata->real_duration = plan->tdata->real_end - plan->tdata->actual_start
	- plan->tdata->paused_time;
    else
      plan->tdata->real_duration = plan->tdata->real_end - plan->tdata->real_start
	- plan->tdata->paused_time;

    glob_timing->tot_duration += plan->tdata->real_duration;
    glob_timing->last_cyc_duration = plan->tdata->real_duration;

    glob_timing->tgt_duration = plan->tdata->tgt_time;

    glob_timing->avg_duration = glob_timing->tot_duration / (double)plan->iteration;

    glob_timing->active = FALSE;
    pthread_mutex_unlock(&glob_timing->upd_mutex);

    d_print_debug("PLAN DONE, finished cycle in %.4f msec, target was < %.4f (%+.4f), average is %.4f\n"
                  "sequential time %.4f (%.2f %%), concurrent time = %.4f (%.2f %%)\n"
                  "preload time = %.4f, preload active time = %.4f (%.2f %%)\n"
                  "queued time = %.4f (%.2f %%), start wait = %.4f (%.2f %%), paused for %.4f, "
                  "waiting time = %.4f (%.2f %%)\n",
                  1000. * plan->tdata->real_duration, plan->tdata->tgt_time * 1000.,
                  1000. * (plan->tdata->real_duration - plan->tdata->tgt_time),
                  1000. * glob_timing->tot_duration / (double)plan->iteration,
                  plan->tdata->sequential_time, plan->tdata->sequential_time / plan->tdata->real_duration / 10.,
                  1000. * plan->tdata->concurrent_time, plan->tdata->concurrent_time / plan->tdata->real_duration * 100.,
                  1000. * plan->tdata->preload_time, 1000. * plan->tdata->active_pl_time,
                  plan->tdata->active_pl_time / plan->tdata->preload_time * 100.,
                  1000. * plan->tdata->queued_time, plan->tdata->queued_time / plan->tdata->real_duration * 100.,
                  1000. * plan->tdata->start_wait, plan->tdata->start_wait / plan->tdata->real_duration * 100.,
                  1000. * plan->tdata->paused_time, 1000. * plan->tdata->waiting_time,
                  plan->tdata->waiting_time / plan->tdata->real_duration * 100.);

    char *bps = NULL, *gbps = NULL;

    if (glob_timing->bytes_per_sec) bps = lives_format_storage_space_string((uint64_t)glob_timing->bytes_per_sec);
    if (glob_timing->gbytes_per_sec) gbps = lives_format_storage_space_string((uint64_t)glob_timing->gbytes_per_sec);

    if (bps) d_print_debug(", memcpy speed is measured as %s per second", bps);
    if (gbps) d_print_debug(", gamma convert byterate is measured as %s per second", gbps);
    d_print_debug("\n\n\n");
    if (bps) lives_free(bps);
    if (gbps) lives_free(gbps);

    /* if (lives_proc_thread_is_paused(ann_proc)) lives_proc_thread_request_resume(ann_proc); */

    /* if (!(glob_timing->ann->flags & ANN_TRAINED)) { */
    /*   // needs about 200 generations of testing */
    /*   if (glob_timing->ann_gens > 200 || glob_timing->ann->no_change_count >= 50 */
    /*       || errval <= ANN_ERR_THRESH) { */
    /*     if (!lives_proc_thread_is_paused(ann_proc)) lives_proc_thread_request_pause(ann_proc); */
    /* 	  mainw->refresh_model = TRUE; */
    /*       glob_timing->ann->flags |= ANN_TRAINED; */
    /*       lives_proc_thread_set_loveliness(ann_proc, DEF_LOVELINESS / 2.); */
    /*   } */
    /* } */
    SET_PLAN_STATE(COMPLETE);
  } else {
    pthread_mutex_lock(&glob_timing->upd_mutex);
    glob_timing->active = FALSE;
    pthread_mutex_unlock(&glob_timing->upd_mutex);
  }
  MSGMODE_OFF(DEBUG);

  ____FUNC_EXIT____;
  if (plan->state == PLAN_STATE_CANCELLED) lives_proc_thread_cancel(self);
  nplans--; 
}


void plan_cycle_trigger(exec_plan_t *plan) {
  if (plan->tdata->trigger_time) return;

  plan->tdata->trigger_time = lives_get_session_time();

  // wait for queued plan to be picked up by a worker thread
  lives_millisleep_while_true(plan->state == PLAN_STATE_QUEUED);

  if (plan->state == PLAN_STATE_WAITING) {
    // now in the waiting state wait until it either pauses or gets cancelled
    lives_microsleep_while_false(lives_proc_thread_is_paused(mainw->plan_runner_proc)
                                 || lives_proc_thread_was_cancelled(mainw->plan_runner_proc));
    if (lives_proc_thread_is_paused(mainw->plan_runner_proc))
      // once paused, we send a resume request which will wake it
      lives_proc_thread_request_resume(mainw->plan_runner_proc);
  }
}


// copy layer:
// bef conv check can we non inplace A ?
// conv A + apply A + conv A -> B cf copy lyer + conv p ->b
// - if we cannot non inplace A, then B instead of conv -> B has to use conv ->A
//   then cna w enon inplace B ?  A gets conv A + apply B   so apply B has to b < coppy cost
// and B, conv A,   so is conv A cost < copy cos
//  - do conv instead can B now use pal

// copy step
// acost = cop cost
// bcost = cop cost + conv p -> B

// can we non inpl a ?
///   can we non inpl B and both use same pal ?
// a cos = 0
// b cost -> conv a cost

// cannot no inpl B, or b cannot use a pal
// b cost -> a conv + a apply + a -> b conv

// cannot inpl a, but can inpl b and b can us a pal
// a cast -> b apply cost

/* if (can _non_inpl(a)) { */
/*   if (has_same_pal(a, b) && can_non_inpl(b)) { */
/*     // both get layer, do not free til both done */
/*     // b inst gets dep on a conv */
/*     // note tha a and b share layer */
/*     // only free when both are done */
/*   } */
/*   else { */
/*     xbcost = conv(a) + apply(a) + conv(a, b); */
/*     if (cbcost < copy_cost + conv(p, a) + b slack) { */
/* 	// make b conv dep on a inst */
/* 	// do not free channel after appl a */
/* 	// b takes inp from a in chan */
/*     } */
/*   } */
/* } */
/* else if (can_non_inpl(b) && has_same_pal(a, b)) { */
/*   xacost = appl_b_cost; */
/*   if (xacos < copy_csot + slack) { */
/*     if (only_deb_for_b_inst is bconv) { */
/* 	// make b inst dep on a conve */
/* 	// make a inst dep on b inst instead of a conv */
/* 	// do not free in chan in b */
/*     } */
/*   } */
/* } */

// so need to be able to say, do not free in chan
// and do  not free in chan until steps complete
// and take pixdata fro in chan X


static boolean runner_cancelled_cb(void *lptp, void *planp) {
  exec_plan_t *plan = (exec_plan_t *)planp;
  if (plan) {
    double xtime = lives_get_session_time();
    d_print_debug("plan cancelled @ %.2f msec\n", xtime * 1000.);
    if (plan->state == PLAN_STATE_WAITING || plan->state == PLAN_STATE_QUEUED)
      d_print_debug("(plan cancelled before running)\n");
    nplans--;
    SET_PLAN_STATE(CANCELLED);
  }
  return FALSE;
}


lives_proc_thread_t execute_plan(exec_plan_t *plan, boolean async) {
  // execute steps in plan. If asynch is TRUE, then this is done in a proc_thread whichis returned
  // otherwise it runs synch and NULL is returned
  lives_proc_thread_t lpt = NULL;
  if (async) {
    if (plan->state != PLAN_STATE_INERT) return mainw->plan_runner_proc;
    SET_PLAN_STATE(QUEUED);

    mainw->plan_runner_proc = lpt
      = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED, run_plan, -1, "v", plan);
    lives_proc_thread_add_hook(lpt, CANCELLED_HOOK, 0, runner_cancelled_cb, (void *)plan);

    lives_proc_thread_set_cancellable(lpt);
    lives_proc_thread_set_pauseable(lpt, TRUE);
    plan->tdata->exec_time = lives_get_session_time();
    nplans++;
    lives_proc_thread_queue(lpt, LIVES_THRDATTR_PRIORITY);
  } else run_plan(plan);
  return lpt;
}


static plan_step_t *plan_step_copy(exec_plan_t *dplan, exec_plan_t *splan, plan_step_t *sstep) {
  plan_step_t *dstep = (plan_step_t *)lives_calloc(1, sizeof(plan_step_t));
  lives_memcpy(dstep, sstep, sizeof(plan_step_t));
  dstep->tdata = LIVES_CALLOC_SIZEOF(timedata_t, 1);
  lives_memcpy(dstep->tdata, sstep->tdata, sizeof(timedata_t));
  dstep->deps = NULL;
  dstep->substeps = NULL;
  dstep->plan = dplan;

  // copy deps - go through orig list - get the dep, then go through list again - find idx of dep
  // iter through copy list to same idx, and set dest dep
  if (sstep->ndeps) {
    dstep->deps = (plan_step_t **)lives_calloc(sstep->ndeps, sizeof(plan_step_t *));
    for (int i = 0; i < sstep->ndeps; i++) {
      int j = 0;
      for (LiVESList *xsstep = splan->steps; xsstep; xsstep = xsstep->next) {
        if (xsstep->data == sstep->deps[i]) {
          LiVESList *xdstep = dplan->steps;
          for (int k = 0 ; k < j; k++) xdstep = xdstep->next;
          dstep->deps[i] = (plan_step_t *)xdstep->data;
          break;
        }
        j++;
      }
    }
  }
  return dstep;
}


exec_plan_t *create_plan_cycle(exec_plan_t *template, lives_layer_t **layers) {
  exec_plan_t *cycle = NULL;
  if (template && layers) {
    cycle = (exec_plan_t *)lives_calloc(1, sizeof(exec_plan_t));
    lives_memcpy(cycle, template, sizeof(exec_plan_t));
    cycle->state = PLAN_STATE_INERT;
    cycle->template = template;
    cycle->iteration = ++template->iteration;
    cycle->layers = layers;
    cycle->tdata = LIVES_CALLOC_SIZEOF(timedata_t, 1);
    lives_memcpy(cycle->tdata, template->tdata, sizeof(timedata_t));
    cycle->frame_idx = (frames64_t *)lives_calloc(cycle->model->ntracks,
						  sizeof(frames64_t));
    cycle->steps = NULL;

    for (LiVESList *list = template->steps; list; list = list->next) {
      plan_step_t *step = (plan_step_t *)list->data,
	*xstep = plan_step_copy(cycle, template, step);
      cycle->steps = lives_list_append(cycle->steps, (void *)xstep);
    }
  }
  return cycle;
}


static plan_step_t *alloc_step(exec_plan_t *plan, int st_type, int ndeps, plan_step_t **deps) {
  LIVES_CALLOC_TYPE(plan_step_t,  step, 1);
  step->st_type = st_type;
  step->deps = deps;
  step->ndeps = ndeps;
  step->plan = plan;
  step->tdata = LIVES_CALLOC_SIZEOF(timedata_t, 1);

  for (int i = 0; i < ndeps; i++) {
    plan_step_t *prev_step = deps[i];
    for (int j = 0; j < N_RES_TYPES; j++) {
      if (j == RES_TYPE_THRD) step->start_res[j] = 1;
      else {
        step->start_res[j] += prev_step->end_res[j];
        step->end_res[j] = step->start_res[j];
      }
    }
  }
  return step;
}


static plan_step_t *create_step(exec_plan_t *plan, int st_type, inst_node_t *n, int idx,
                                plan_step_t **deps, int ndeps) {
  // create a step for an action and prepend to the plan
  // we also attach resource costs to the steps, at the start and at the end
  // later we will assing start and end times to the steps and then we can simulate running the plan and keep
  // track of the amount of each resource needed. We can chack to ensure these quantites remain within
  // defined rlimits. We can also find the max for each resource type and reserve thast in advance
  // we can subsititue a bblock resource for an amount of meory resource
  // however it is not easy to detemine whne that bblock resource is returned to the pool
  // blocks can be subsituted in place of layer copy, load and convert
  // for convert we can maintain the bblock if we have a spare bblock (except som pconv do inplace, then we do not
  // need a spare). If not enough spares then we add a -1 bblock cost and a mem cost
  // For appply inst, this is tricky. We can check all dependancy steps and find out how many bblocks in.
  // then we need to determine if the step feeds to an inplace channel or not. For non inlace, if they have a bblock
  // we can only assign a bblock out if there are spares
  // so when we add a convert or copy step as a dep for apply inst,
  // we know the input id, hten we need to consult in_tracks
  // to see which channel it maps to, then we can flag the dep as inplace or not. Then we find n inplace / n non inplace
  // for non inplace we can assign a bblock if we have a spare. For each step we need to gla if it usea a bllcok resource
  // so when we create a dependecny, the resource costs are carried over, and then we can add to them
  // then we sum all deps
  plan_step_t *step = NULL;
  input_node_t *in = NULL;
  output_node_t *out = NULL;
  inst_node_t *p;
  boolean add_step;
  int step_number = 0;
  int pal;

  if (idx >= 0) {
    if (st_type == STEP_TYPE_COPY_IN_LAYER)
      in = n->inputs[idx];
    else {
      if (st_type == STEP_TYPE_COPY_OUT_LAYER
          || st_type == STEP_TYPE_CONVERT)
        out = n->outputs[idx];
    }
  }

  do {
    add_step = FALSE;

    if (prefs->dev_show_timing)
      d_print_debug("add step type %d\n", st_type);
    step_number++;

    switch (st_type) {
    case STEP_TYPE_LOAD: {
      step = alloc_step(plan, st_type, ndeps, deps);
      step->tdata->est_start = -1;
      step->tdata->est_duration = -1;
      step->tdata->deadline = 0;

      // all we really need is the track number
      step->track = idx;
      step->target_idx = n->model_idx;

      // can substitute a bblock cost
      step->start_res[RES_TYPE_THRD] += 1;
      if (step->fin_pal)
        step->end_res[RES_TYPE_MEM] =
          lives_frame_calc_bytesize(step->fin_width, step->fin_height, step->fin_pal, FALSE, NULL);
      step->end_res[RES_TYPE_BBLOCK] = 1;
    }
      break;

    case STEP_TYPE_CONVERT: {
      weed_filter_t *filter = NULL;
      int *pal_list;
      int ipal, opal;
      boolean inplace = FALSE;
      boolean letterbox = FALSE;

      // for srcs we want to ad a "pre" convert to srcgroup
      if (n->model_type == NODE_MODELS_CLIP && step_number == 1) {
        // convert to the track_source srcgroup
        lives_clipsrc_group_t *srcgrp;
        lives_clip_t *sfile;
        step = alloc_step(plan, st_type, ndeps, deps);

        for (int i = 0; i < step->ndeps; i++)
          step->deps[i]->flags |= STEP_FLAG_NO_READY_STAT;

        step->track = out->track;

        step->target_idx = n->model_idx;
        srcgrp = get_srcgrp(step->target_idx, step->track, SRC_PURPOSE_ANY);

        sfile = RETURN_VALID_CLIP(step->target_idx);

        step->fin_width = step->fin_iwidth = sfile->hsize;
        step->fin_height = step->fin_iheight = sfile->vsize;
        step->fin_pal = srcgrp->apparent_pal;
        step->fin_gamma = srcgrp->apparent_gamma;

        deps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
        ndeps = 1;
        deps[0] = step;
        add_step = TRUE;
        break;
      }

      in = out->node->inputs[out->iidx];

      if (in->npals) {
        ipal = in->pals[in->optimal_pal];
        pal_list = in->pals;
      } else {
        ipal = out->node->pals[out->node->optimal_pal];
        pal_list = out->node->pals;
      }
      if (out->npals) opal = out->pals[out->optimal_pal];
      else opal = n->pals[n->optimal_pal];

      if (out->width == in->width && out->height == in->height && ipal == opal
          && n->gamma_type == out->node->gamma_type) {
        lives_free(deps);
        break;
      }

      if (step) plan->steps = lives_list_prepend(plan->steps, (void *)step);

      step = alloc_step(plan, st_type, ndeps, deps);
      step->track = out->track;

      for (int i = 0; i < step->ndeps; i++)
        step->deps[i]->flags |= STEP_FLAG_NO_READY_STAT;

      step->tdata->est_start = n->ready_ticks;

      if (n->model_type == NODE_MODELS_FILTER ||
          n->model_type == NODE_MODELS_GENERATOR) {
        filter = (weed_filter_t *)n->model_for;
        if (filter) {
          step->tdata->est_start += get_proc_cost(COST_TYPE_TIME, filter,
                                                  n->width, n->height, n->optimal_pal);
        }
      }

      if (in->inner_width < in->width || in->inner_height < in->height)
        letterbox = TRUE;

      step->tdata->est_duration = get_conversion_cost(COST_TYPE_TIME, out->width, out->height,
						      in->inner_width, in->inner_height, letterbox,
						      opal, ipal, pal_list,
						      n->gamma_type, out->node->gamma_type, FALSE);

      step->tdata->deadline = out->node->ready_ticks;

      step->ini_width = out->width;
      step->ini_height = out->height;
      step->ini_pal = opal;
      step->ini_gamma = n->gamma_type;

      step->fin_width = in->width;
      step->fin_height = in->height;
      step->fin_iwidth = in->inner_width;
      step->fin_iheight = in->inner_height;
      step->fin_pal = ipal;
      step->fin_gamma = out->node->gamma_type;

      if (!step->fin_iwidth) step->fin_iwidth = step->fin_width;
      if (!step->fin_iheight) step->fin_iheight = step->fin_height;

      if (in->width == out->width && in->height == out->height
          && pconv_can_inplace(opal, ipal)) inplace = TRUE;

      if (!inplace || !step->start_res[RES_TYPE_MEM]) {
        if (!step->start_res[RES_TYPE_MEM])
          step->start_res[RES_TYPE_MEM] =
            lives_frame_calc_bytesize(out->width, out->height, opal, FALSE, NULL);

        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(step->fin_width, step->fin_height, step->fin_pal, FALSE, NULL);
        step->end_res[RES_TYPE_MEM] = step->start_res[RES_TYPE_MEM]
	  - lives_frame_calc_bytesize(out->width, out->height, opal, FALSE, NULL);
        step->start_res[RES_TYPE_BBLOCK]++;
        step->end_res[RES_TYPE_BBLOCK] = 1;
      }
    }
      break;

    case STEP_TYPE_COPY_OUT_LAYER:
    case STEP_TYPE_COPY_IN_LAYER: {
      if (st_type == STEP_TYPE_COPY_IN_LAYER) {
        input_node_t *orig = n->inputs[in->origin];
        p = orig->node;
        out = p->outputs[orig->oidx];
        if (orig->npals) pal = orig->pals[orig->optimal_pal];
        else  pal = n->pals[n->optimal_pal];
        step = alloc_step(plan, st_type, ndeps, deps);
        step->schan = in->origin;
        step->dchan = idx;
        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(orig->width, orig->height, pal, FALSE, NULL);
        step->tdata->est_duration = get_layer_copy_cost(COST_TYPE_TIME, orig->width, orig->height, pal);
      } else {
        output_node_t *orig = n->outputs[out->origin];
        if (orig->npals) pal = orig->pals[orig->optimal_pal];
        else pal = n->pals[n->optimal_pal];
        step = alloc_step(plan, st_type, ndeps, deps);
        step->schan = out->origin;
        step->dchan = idx;
        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(orig->width, orig->height, pal, FALSE, NULL);
        step->tdata->est_duration = get_layer_copy_cost(COST_TYPE_TIME, orig->width, orig->height, pal);
      }

      step->end_res[RES_TYPE_MEM] = step->start_res[RES_TYPE_MEM];

      step->tdata->est_start = -1;
      step->tdata->deadline = n->ready_ticks;
      step->dchan = idx;

      step->start_res[RES_TYPE_BBLOCK]++;
      step->end_res[RES_TYPE_BBLOCK] = step->start_res[RES_TYPE_BBLOCK];
    }
      break;

    case STEP_TYPE_APPLY_INST: {
      step = alloc_step(plan, st_type, ndeps, deps);

      step->tdata->est_start = n->ready_ticks;
      step->target_idx = idx;

      if (n->model_type == NODE_MODELS_FILTER ||
          n->model_type == NODE_MODELS_GENERATOR) {
        size_t memused = 0;
        weed_filter_t *filter = (weed_filter_t *)n->model_for;
        step->target = filter;
        step->tdata->est_duration = get_proc_cost(COST_TYPE_TIME, filter, n->width, n->height, n->optimal_pal);

        step->tdata->deadline = -1;

        for (int i = 0; i < n->n_outputs; i++) {
          // if we had bblocks in inputs, if an out is inplace and input is bblock
          // then we keep it. If not inplace, we can add a bblock if we have spares
          out = n->outputs[i];
          if (out->flags & NODEFLAGS_IO_SKIP) continue;
          if (out->npals) pal = out->optimal_pal;
          else pal = n->optimal_pal;
          memused += lives_frame_calc_bytesize(out->width, out->height, pal, FALSE, NULL);
        }

        step->start_res[RES_TYPE_MEM] += memused;
        step->end_res[RES_TYPE_MEM] = memused;
        step->start_res[RES_TYPE_BBLOCK] += n->n_outputs;
        step->end_res[RES_TYPE_BBLOCK] = n->n_outputs;

        if (n->model_type == NODE_MODELS_GENERATOR) {
          step->flags |= STEP_FLAG_RUN_AS_LOAD;
          step->track = out->track;
        }
      } else {
        // this must be an output sink
        size_t memused = 0;
        step->tdata->est_duration = 0;
        in = n->inputs[0];
        pal = n->optimal_pal;
        if (pal)
          memused = lives_frame_calc_bytesize(in->width, in->height, pal, FALSE, NULL);
        step->start_res[RES_TYPE_THRD] = step->end_res[RES_TYPE_THRD] = 0;
        step->end_res[RES_TYPE_MEM] = memused;
        step->end_res[RES_TYPE_BBLOCK] = 1;
      }
    }
      break;
    default: break;
    }
  } while (add_step);
  return step;
}


exec_plan_t *create_plan_from_model(lives_nodemodel_t *nodemodel) {
  // since a nodemodel can be difficult to parse we will create a plan - a "flattebed version"
  // of it in temploral order
  //
  // create a plan from the nodemodel
  // the shedule will list all the actions for the nodemodel in temporal oreder
  // the format will be st_time (est) | p | flags | duration (est) | deadline | action | data |
  // in_tr[] | in_ch | out_tr | out_ch{}
  // final_pal[4] | sizes[4] |  gamma | opts
  //
  // opts can include SCHED_OPT_DEINTERLACE
  //
  // pal will be {idx, clamp, samp, subspace),
  // sizes will be {width in pixels, height, inwidth pixels, inheight}
  // if inheght < height or inwidth < widht, this indicates letterboxing
  // p is an array of actions which must be completed before the step can be run
  // flags will include STEP_FLAG_COMPLETE, STEP_FLAG_BYPASS
  // if bypass is set the step should be marked as complete as soon as the prerequsites are complted
  // if a sstep has duration 0, this means it cannot be estimate, the duration should be measured
  // the value added as an offeset to all other start and deadline times
  //
  // when running an action we can check current cycle time, and if curtime + dur < dealine
  // we can de prioritse thi
  // if curtime + dur > deadline, we can try to up prioritise

  //
  //
  // e.g
  // 0 | dur | ded | load + conv |  -    | track | -     | -     | fin pal | fin_w | fin_h | fin gamma | deinterlace?
  // 0 | dur | ded | convert 	 |  -    | track | -     | -     | fin pal | fin_w | fin h | fin_gamma | -
  // t | dur | ded | apply inst  | inst  |
  // t | dur | ded | copy layer  |  -    |   -   | didx  | sidx  |
  //
  // for load + conv - wait for non NULL layer[track], if clip < 0, load blank frame
  // for other steps wait for deps to complete:
  // convert always depends on either apply_inst (or copy_layer - TODO)
  // copy_layer can depend on either (apply_inst - TODO)  or convert
  // apply_inst can depend on a combo of any previous steps

  inst_node_t *retn = NULL;
  exec_plan_t *plan = (exec_plan_t *)lives_calloc(1, sizeof(exec_plan_t));

  plan->uid = gen_unique_id();
  plan->model = nodemodel;
  plan->state = PLAN_STATE_TEMPLATE;
  plan->frame_idx = (frames64_t *)lives_calloc(nodemodel->ntracks, sizeof(frames64_t));
  plan->tdata = LIVES_CALLOC_SIZEOF(timedata_t, 1);

  reset_model(nodemodel);

  do {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      // since we ran only the first part of the process, finding the costs
      // but not the second, finding the palette sequence, we need to clear the Processed flag
      // else we would be unbale to reacluate costs again.
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      retn = desc_and_add_steps(n, plan);
    }
  } while (retn);

  reset_model(nodemodel);

  plan->steps = lives_list_reverse(plan->steps);
  ///  if (prefs->dev_show_timing)
  //display_plan(plan);

  if (nodemodel->flags & NODEMODEL_NEW) {
    pthread_mutex_lock(&glob_timing->ann_mutex);
    glob_timing->ann->flags &= ~ANN_TRAINED;
    glob_timing->ann_gens = 0;
    lives_ann_init_seed(glob_timing->ann, .001);
    lives_ann_set_variance(glob_timing->ann, 1.0, 1.0, 0.9999, 2);
    glob_timing->ann->nvary = glob_timing->ann->nnodes;
    glob_timing->ann->last_res = 0.;
    glob_timing->ann->no_change_count = 0;
    pthread_mutex_unlock(&glob_timing->ann_mutex);
  }
  return plan;
}


static void plan_step_free(plan_step_t *step) {
  if (step) {
    if (step->tdata) lives_free(step->tdata);
    if (step->errmsg) lives_free(step->errmsg);
    if (step->deps) lives_free(step->deps);
    if (step->substeps) lives_list_free_all(&step->substeps);
    lives_free(step);
  }
}


void exec_plan_free(exec_plan_t *plan) {
  if (plan) {
    if (plan->frame_idx) lives_free(plan->frame_idx);
    if (plan->tdata) lives_free(plan->tdata);
    if (plan->steps) {
      for (LiVESList *list = plan->steps; list; list = list->next) {
        if (list->data) plan_step_free((plan_step_t *)list->data);
      }
      lives_list_free(plan->steps);
    }
    lives_free(plan);
  }
}


void display_plan(exec_plan_t *plan) {
  int stepno = 1;
  if (!plan) return;

  lives_printerr("\n\nDISPLAYING PLAN 0X%016lX created from nodemodel %p\n", plan->uid, plan->model);
  if (plan->state == PLAN_STATE_TEMPLATE)
    lives_printerr("This is a template. It has been used to create %ld cycles\n", plan->iteration);
  else {
    lives_printerr("This is a plan cycle, iteration %ld\n", plan->iteration);
    switch (plan->state) {
    case PLAN_STATE_INERT:
      lives_printerr("Plan cycle is inactive");
      break;
    case PLAN_STATE_WAITING:
      lives_printerr("Plan cycle is waiting for trigger");
      break;
    case PLAN_STATE_RUNNING:
      lives_printerr("Plan cycle is active, running");
      break;
    case PLAN_STATE_COMPLETE:
      lives_printerr("Plan cycle is complete");
      break;
    default:
      lives_printerr("Plan cycle encountered an error");
      break;
    }
  }
  lives_printerr("\nStep sequence:\nBEGIN:\n");

  for (LiVESList *list = plan->steps; list; list = list->next) {
    plan_step_t *step = (plan_step_t *)list->data;
    char *memstr;
    lives_printerr("\nStep %d\n", stepno++);
    switch (step->st_type) {
    case STEP_TYPE_LOAD:
      lives_printerr("LOAD ");
      lives_printerr("into Track %d - source: clip number %d: size %d X %d, pal %s (gamma %s)\n",
                     step ->track, step->target_idx, step->fin_width, step->fin_height,
                     weed_palette_get_name(step->fin_pal), weed_gamma_get_name(step->fin_gamma));
      break;
    case STEP_TYPE_CONVERT:
      lives_printerr("CONVERT ");
      lives_printerr("layer on Track %d: from %d X %d, pal %s gamma %s to  size %d X %d (%d X %d), "
                     "new pal %s, new gamma %s\n",
                     step->track, step->ini_width, step->ini_height, weed_palette_get_name(step->ini_pal),
                     weed_gamma_get_name(step->ini_gamma),
                     step->fin_width, step->fin_height, step->fin_iwidth, step->fin_iheight,
                     weed_palette_get_name(step->fin_pal), weed_gamma_get_name(step->fin_gamma));
      break;
    case STEP_TYPE_APPLY_INST:
      lives_printerr("APPLY INSTANCE ");
      lives_printerr("filter: %s\n", weed_filter_get_name((weed_filter_t *)step->target));
      break;
    default:
      lives_printerr("copy layer\n");
      break;
    }
    switch (step->state) {
    case STEP_STATE_INACTIVE:
      lives_printerr("Step status is inactive");
      break;
    case STEP_STATE_WAITING:
      lives_printerr("Step status is wating");
      break;
    case STEP_STATE_RUNNING:
      lives_printerr("Step status is active, running");
      break;
    case STEP_STATE_SKIPPED:
      lives_printerr("Step was skipped");
      break;
    case STEP_STATE_CANCELLED:
      lives_printerr("Step was cancelled");
      break;
    case STEP_STATE_IGNORE:
      lives_printerr("Ignored step");
      break;
    case STEP_STATE_PAUSED:
      lives_printerr("Step is paused");
      break;
    case STEP_STATE_RESUMING:
      lives_printerr("Step is resuming after a pause");
      break;
    case STEP_STATE_FINISHED:
      lives_printerr("Step status is complete");
      break;
    case STEP_STATE_ERROR:
      lives_printerr("Step  encountered an error");
      break;
    default:
      lives_printerr("Unecognized step state !");
      break;
    }
    lives_printerr("\n");
    lives_printerr("Resources:\n");
    memstr = lives_format_storage_space_string(step->start_res[RES_TYPE_MEM]);
    lives_printerr("\tOn entry: %ld threads, %s OR %ld bigblocks\n",
                   step->start_res[RES_TYPE_THRD], memstr, step->start_res[RES_TYPE_BBLOCK]);
    lives_free(memstr);
    memstr = lives_format_storage_space_string(step->end_res[RES_TYPE_MEM]);
    lives_printerr("\tOn exit: %ld threads, %s OR %ld bigblocks\n",
                   step->end_res[RES_TYPE_THRD], memstr, step->end_res[RES_TYPE_BBLOCK]);
    lives_free(memstr);
    lives_printerr("Timing estimates: start time %.4f msec, duration %.4f msec, deadline %.4f msec\n",
                   step->tdata->est_start / TICKS_PER_SECOND_DBL * 1000.,
                   step->tdata->est_duration * 1000.,
                   step->tdata->deadline / TICKS_PER_SECOND_DBL * 1000.);
    lives_printerr("Dependencies: ");
    if (step->st_type == STEP_TYPE_LOAD) lives_printerr("Wait for LAYER_STATUS_PREPARED\n"
							"Skip on: LAYER_STATUS_LOADED, LAYER_STATUS_READY\n");
    else {
      if (!step->ndeps) lives_printerr("None");
      else {
        for (int i = 0; i < step->ndeps; i++) {
          int j = 1;
          for (LiVESList *xlist = plan->steps; xlist; xlist = xlist->next) {
            plan_step_t *xstep = (plan_step_t *)xlist->data;
            if (xstep == step->deps[i]) {
              lives_printerr("%sstep %d", j > 1 ? ", " : "",  j);
              break;
            }
            j++;
	    // *INDENT-OFF*
	  }}}}
    // *INDENT-ON*
    lives_printerr("\n\n");
  }
  lives_printerr("END\n");
  //print_diagnostics(DIAG_ALL);
}


static void align_with_node(lives_nodemodel_t *nodemodel, inst_node_t *n) {
  // here we can align instances, clip_srcs, playback plugins
  // the MODEL will  be abstract, dealing with filters adn channel_templates
  // , here we will create or update filter instances, and we have actual channels rather than chantmpls
  // the nodes model FILTERS, so we need some way to find corresponding INSTANCES...
  if (n) {
    input_node_t *in;
    int pal, cpal;

    switch (n->model_type) {
    case NODE_MODELS_CLIP: {
      // for clip sources, set the clip_srcs with the correct palettes internally
      // - find the track for the node
      // - finde the srcgrp from mainw->track_sources
      // - set apparent_palette, apparent_gamma for the srcgrp to match node
      // --- the value is taken from n->optimal_pal
      int track = n->outputs[0]->track;
      int clip = nodemodel->clip_index[track];
      lives_clipsrc_group_t *srcgrp = mainw->track_sources[track];
      if (!srcgrp) srcgrp = get_primary_srcgrp(clip);

      for (int ni = 0; ni < n->n_clip_srcs; ni++) {
        lives_clip_src_t *mysrc;
        in = n->inputs[ni];
        mysrc = find_src_by_class_uid(srcgrp, in->src_uid);
        switch (mysrc->src_type) {
        case LIVES_SRC_TYPE_DECODER: {
          lives_decoder_t *dplug = (lives_decoder_t *)mysrc->actor;
          if (dplug) {
            int cpal = dplug->cdata->current_palette;
            int tpal = in->optimal_pal;
            if (dplug->dpsys->set_palette && tpal != cpal) {
              int pal = best_palette_match(dplug->cdata->palettes, -1, tpal);
              if (pal != cpal) {
                pthread_mutex_lock(&dplug->mutex);
                dplug->cdata->current_palette = pal;
                if (!(*dplug->dpsys->set_palette)(dplug->cdata)) {
                  dplug->cdata->current_palette = cpal;
                  (*dplug->dpsys->set_palette)(dplug->cdata);
                } else if (dplug->cdata->rec_rowstrides) {
                  lives_free(dplug->cdata->rec_rowstrides);
                  dplug->cdata->rec_rowstrides = NULL;
                  cpal = pal;
                }
		pthread_mutex_unlock(&dplug->mutex);
		// *INDENT-OFF*
	      }}}}
	  break;
	default: break;
	}}}
      // *INDENT-ON*
      break;
    case NODE_MODELS_GENERATOR:
    case NODE_MODELS_FILTER: {
      // set channel palettes and sizes, get actual size after channel limitations are applied.
      // check if weed to reinit
      weed_filter_t *filter = (weed_filter_t *)n->model_for;
      weed_instance_t *inst;
      weed_channel_t **in_channels, **out_channels;
      boolean is_first = TRUE;
      int nins, nouts, i = 0, k, cwidth, cheight;

      if (n->model_type == NODE_MODELS_FILTER)
	inst = rte_keymode_get_instance(n->model_idx + 1, rte_key_getmode(n->model_idx + 1));
      else {
	lives_clip_t *sfile = RETURN_VALID_CLIP(n->model_idx);
	inst = get_primary_inst(sfile);
	weed_instance_ref(inst);
      }
      in_channels = weed_instance_get_in_channels(inst, &nins);
      out_channels = weed_instance_get_out_channels(inst, &nouts);

      n->needs_reinit = FALSE;

      for (k = 0; k < nins; k++) {
	weed_channel_t *channel = in_channels[k];
	weed_chantmpl_t *chantmpl = weed_channel_get_template(channel);

	if (weed_channel_is_alpha(channel)) continue;
	if (weed_channel_is_disabled(channel)) continue;
	if (weed_chantmpl_is_audio(chantmpl)) continue;

	if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL))
	  continue;

	in = n->inputs[i++];

	if (in->npals) {
	  pal = in->pals[in->optimal_pal];
	  cpal = in->cpal = weed_channel_get_palette(channel);
	} else {
	  pal = n->pals[n->optimal_pal];
	  cpal = n->cpal = weed_channel_get_palette(channel);
	}

	if (in->flags & NODEFLAGS_IO_SKIP) continue;

	weed_channel_set_palette_yuv(channel, pal, WEED_YUV_CLAMPING_UNCLAMPED,
				     WEED_YUV_SAMPLING_DEFAULT, WEED_YUV_SUBSPACE_YUV);

	cwidth = weed_channel_get_pixel_width(channel);
	cheight = weed_channel_get_height(channel);
	set_channel_size(filter, channel, &in->width, &in->height);

	if (in->inner_width && in->inner_height
	    && (in->inner_width < in->width || in->inner_height < in->height)) {
	  int lbvals[4] = {(in->width - in->inner_width) >> 1,
			   (in->height - in->inner_height) >> 1,
			   in->inner_width, in->inner_height
	  };
	  weed_set_int_array(channel, WEED_LEAF_INNER_SIZE, 4, lbvals);
	}

	if (in->width != cwidth || in->height != cheight)
	  if (in->flags & NODESRC_REINIT_SIZE) n->needs_reinit = TRUE;
	if (in->width * pixel_size(in->optimal_pal) != cwidth * pixel_size(cpal))
	  if (in->flags & NODESRC_REINIT_RS) n->needs_reinit = TRUE;
	if (pal != cpal)
	  if (in->flags & NODESRC_REINIT_PAL) n->needs_reinit = TRUE;
      }

      i = 0;
      for (k = 0; k < nouts; k++) {
	// output channel setup
	output_node_t *out;
	input_node_t *in;
	weed_channel_t *channel = out_channels[k];
	int ipal;
	if (weed_channel_is_alpha(channel) ||
	    weed_channel_is_disabled(channel)) continue;

	out = n->outputs[i++];

	if (out->flags & NODEFLAGS_IO_SKIP) continue;

	if (n->model_type == NODE_MODELS_GENERATOR && is_first) {
	  // this would have been set during ascending phase of size setting
	  // now we can set in sfile
	  lives_clip_t *sfile = RETURN_VALID_CLIP(n->model_idx);
	  sfile->hsize = out->width;
	  sfile->vsize = out->height;
	}

	is_first = FALSE;

	cpal = weed_channel_get_palette(channel);

	if (out->npals) pal = out->pals[out->optimal_pal];
	else pal = n->pals[n->optimal_pal];

	in = out->node->inputs[out->iidx];

	if (in->npals) ipal = in->pals[in->optimal_pal];
	else ipal = out->node->pals[out->node->optimal_pal];

	if (out->npals) {
	  for (int z = 0; z < out->npals; z++) {
	    if (out->pals[z] == ipal) {
	      pal = ipal;
	      break;
	    }
	  }
	} else {
	  for (int z = 0; z < n->npals; z++) {
	    d_print_debug("CF %d and %d\n", n->pals[z], ipal);
	    if (n->pals[z] == ipal) {
	      pal = ipal;
	      break;
	    }
	  }
	}

	weed_channel_set_palette(channel, pal);

	cwidth = weed_channel_get_width(channel);
	cheight = weed_channel_get_height(channel);
	set_channel_size(filter, channel, &out->width, &out->height);

	if (!out->npals) n->cpal = pal;
	else out->cpal = pal;

	if (out->width != cwidth || out->height != cheight)
	  if (out->flags & NODESRC_REINIT_SIZE) n->needs_reinit = TRUE;
	if (out->width * pixel_size(pal) != cwidth * pixel_size(cpal))
	  if (out->flags & NODESRC_REINIT_RS) n->needs_reinit = TRUE;
	if (pal != cpal)
	  if (out->flags & NODESRC_REINIT_PAL) n->needs_reinit = TRUE;
      }

      if (!n->model_inst) n->model_inst = inst;
      else weed_instance_unref(inst);

      lives_freep((void **)&in_channels);
      lives_freep((void **)&out_channels);
      //update_widget_vis(NULL, i, key_modes[i]);
    }
      break;

    case NODE_MODELS_OUTPUT: {
      _vid_playback_plugin *vpp = (_vid_playback_plugin *)n->model_for;

      n->cpal = vpp->palette;
      pal = n->pals[n->optimal_pal];

      if (n->cpal != pal && (vpp->capabilities & VPP_CAN_CHANGE_PALETTE)) {
	if ((*vpp->set_palette)(pal)) n->cpal = pal;

	// TODO
	/* if (weed_palette_is_yuv(n->cpal) && vpp->get_yuv_palette_clamping) { */
	/*   int *yuv_clamping_types = (*vpp->get_yuv_palette_clamping)(n->cpal); */
	/*   int lclamping = n->YUV_clamping; */
	/*   for (int i = 0; yuv_clamping_types[i] != -1; i++) { */
	/*     if (yuv_clamping_types[i] == lclamping) { */
	/*       if ((*vpp->set_yuv_palette_clamping)(lclamping)) */
	/* 	vpp->YUV_clamping = lclamping; */
	/*     } */
	/*   } */
	/* } */
      }
    }
      break;
    default: break;
    }
  }
}


void align_with_model(lives_nodemodel_t *nodemodel) {
  // after creating and optimising the model, we now align the real objects with the nodemodel map
  LiVESList *list;
  node_chain_t *nchain;
  inst_node_t *n, *retn = NULL;
  boolean *used;

  //MSGMODE_ON(DEBUG);

  d_print_debug("%s @ %s\n", "align start", lives_format_timing_string(lives_get_session_time() - ztime));

  // used: // some tracks may be in clip_index, but they never actually connect to anything else
  // in this case we can set the clip_index val to -1, and this will umap the clip_src in the track_sources
  used = (boolean *)lives_calloc(nodemodel->ntracks, sizeof(boolean));
  for (int i = 0; i < nodemodel->ntracks; i++) used[i] = FALSE;

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    used[nchain->track] = TRUE;
  }

  for (int i = 0; i < nodemodel->ntracks; i++)
    if (!used[i]) nodemodel->clip_index[i] = -1;

  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      lives_clipsrc_group_t *srcgrp;
      lives_clip_t *sfile;
      full_pal_t pally;
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      sfile = RETURN_VALID_CLIP(n->model_idx);
      srcgrp = mainw->track_sources[nchain->track];
      if (!srcgrp) {
	// will need to check for this to mapped later, and we will neeed to set apparent_pal
	// layer has no clipsrc, so we need to find all connected inputs and mark as ignore
	for (int no = 0; no < n->n_outputs; no++) {
	  output_node_t *out = n->outputs[no];
	  inst_node_t *nxt = out->node;
	  input_node_t *in = nxt->inputs[out->iidx];
	  if (nxt->model_type == NODE_MODELS_FILTER) {
	    weed_instance_t *inst = rte_keymode_get_instance(n->model_idx + 1, rte_key_getmode(n->model_idx + 1));
	    if (inst) {
	      weed_channel_t *channel = get_enabled_channel(inst, out->iidx, LIVES_INPUT);
	      weed_chantmpl_t *chantmpl = weed_channel_get_template(channel);
	      if (weed_plant_has_leaf(chantmpl, WEED_LEAF_MAX_REPEATS) || (weed_chantmpl_is_optional(chantmpl)))
		weed_set_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, WEED_TRUE);
	      else weed_set_boolean_value(channel, WEED_LEAF_DISABLED, WEED_TRUE);
	    }
	  }
	  in->flags |= NODEFLAG_IO_IGNORE;
	}
	continue;
      }

      pally.pal = n->pals[n->optimal_pal];
      if (weed_palette_is_yuv(n->optimal_pal)) {
	pally.clamping = WEED_YUV_CLAMPING_UNCLAMPED;
	pally.sampling = WEED_YUV_SAMPLING_DEFAULT;
	pally.subspace = WEED_YUV_SUBSPACE_YUV;
      }

      srcgrp_set_apparent(sfile, srcgrp, &pally, n->gamma_type);
      retn = desc_and_align(n, nodemodel);
    }
  } while (retn);

  reset_model(nodemodel);

  do {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      retn = desc_and_reinit(n);
    }
  } while (retn);

  reset_model(nodemodel);

  d_print_debug("%s @ %s\n", "align end", lives_format_timing_string(lives_get_session_time() - ztime));
  //MSGMODE_OFF(DEBUG);
}


#define HIGH_DISPX_MAX 2.

static double ord_diff(double * bands, int nvals) {
  double xmin = 0., lmin, tot = 0.;
  while (1) {
    lmin = xmin;
    for (int i = 0; i < nvals; i++) {
      if (bands[i] > lmin && (xmin == 0. || bands[i] < xmin))
	xmin = bands[i];
    }
    if (xmin == lmin) break;
    tot += xmin - lmin;
  }
  return tot;
}


static void calc_node_sizes(lives_nodemodel_t *nodemodel, inst_node_t *n) {
  // CALCULATE CHANNEL SIZES (initial valuess)

  // the sizes set here are just initial values to bootstrap the nodemodel

  // in the final implementation, sizes will be a thing that gets optimised, alongside palettes
  // and gamma types

  // MODEL_TYPES

  // for NODE_MODELS_CLIP, we use clip hsize / vsize (since we have to start off with at least SOME sizes defined)
  // this is what we use to calculate sizes at the next node, until we get to sink node then we must resize
  // for NODE_MODELS_SRC, we do not set the sizes
  // SRC can produce frames at any desired size, CLIP may have multip[e internal clip_srcs, hence
  // no real size, so we just resize to whatever the connected input requirss
  // for INSTANCES which are also sources (generators) we also assume (for now) that they can produce any necessary size

  // this leaves non source INSTANCES, OUTPUTS, and INTERNAL sinks

  // since source sizes can be easily adjusted, we start from the sink / output and work backwards

  // calculating the input size to the sink is of extreme importance since it has a huge breaking on overal costs
  // it also depends on quality level
  // for low q and mid q, the input may be smaller than the node size (opwidth, opheight)
  //

  // since we have FIVE contributors to costs - palette conversion, resizes, and gamma conversion,
  // processing time, and layer copy times
  //
  //
  // and all of these are dependant on the palette an input frame size (assumed)
  // then by measuring the actual times we can estimate the change
  //
  // (there is also a ghost cost for switching palette type / gamma type) but this is a constant value
  //
  // when optimising, we will beign by looking for bottlenecks (large cost deltas, and try to reducre these first. We can
  // try reducing frame size, swithcing to a differnet palette, swithcing gamm conversion on or off
  //
  // and find the effect on combined cost at the sink
  //
  // - initially it was envisioned that optimisation would only tesst alternat palettes, but it is apparent we can change
  // the sizes to get better results, e.g if we have an instance which becomes very slow for larger frame sizes
  // e. O(n^2) where n is frame size, then it could be worth downscaling the frames before applyin it and upsclaing later
  // - there is a tcost for donw sscaling and tcost and qloss for upscaling, but the trade offs maybe worth it
  // particulary towards the end of the chain whre qloss has a lesser effect
  // we could also spend some "slack" on resiiing, and discount some or all of the additional tcost
  //

  // we begin with sources and get the layer size as exits from the source output(s)
  // for the next node, we want to find the input width and height
  //
  // some sources have variable out sizes, in this case we leave width and height as 0.
  // Then ascending we replace 0. sizes from the connected input node,
  // output node sizes from the inputs, starting from the sink

  //
  // if svary is set, then - if it is a converter, we use prev size for all inputs and
  // set output to a preset size
  //
  //   - if not a converter,  all inputs just use prev size, however we stil calulate opwidth, opheight as if
  // svary were not set. This is the size we want for the output, curently with svary, if not a converter
  // ins and outs must be paired, so we will set output 0 to the size, and hence we must resize 1st input to this size
  // if letterboxing

  // we want to set 1st input and 1st output to specific size
  // depending on quality level
  //
  // if not letterboxing:
  // For highq we expand one axis so ar. is equal to op and both axes >= op.
  // for med, we stretch one axis to get the ar, then if either axis > op we scale down so it fits
  // for low q, we shrink on axis to it takes on ar of op, then if larger than op shrink it
  // nodemodel optimisation may adjust all of this
  // at the sink - if image size < op size - if using ext plugin and it can resize,
  // we jusst send as is, else we will upscale / downscale to op size
  //
  // for multiple inputs, in highq we use max(inputs, op)
  // for med, use min(max input, opsize)
  // for high max(inputs, op)
  //
  // if letterboxing, we want to set the aspect ratio to be equal to the output size
  //
  // in high q, if either image axis < op size, we resize it so that one axis is equal to output
  // and the other is equal or larger; at the sink we will reduce the overflowing axis to op size, then letterbox
  // if both axes are >= op we leave same size
  // for multiple inputs, we find max width and max height and resize all to that size
  // for med and low we want to find one layer to be the "primary", and  find out which axis will letterbox
  // then all other layers will be sized so that the letterbox axis
  // is equal to the prime layer, the other axis will either shrink to fit or stay the same.
  // the prime layer will usually be whichever has ar. closest to op
  // for med q this is exapnded so other lays can fit inside it
  // for low q other layers will be shrunk to fit inside
  //
  // layer is out from generator, and no lb gens is set, it will be strched or shrunk op a.r. and become prime layer
  //
  // at sink,we will strech or expand to fit in sink
  // if sink is a pb plugin and can letterbox, we do not add blank bars. If sink can resize

  input_node_t *in;
  output_node_t *out;

  double rmaxw = 0., rmaxh = 0., rminw = 0., rminh = 0;
  double xmaxw = 0., xmaxh = 0., xwidth, xheight;

  int opwidth = nodemodel->opwidth;
  int opheight = nodemodel->opheight;
  int width, height;

  double op_ar = (double)opwidth / (double)opheight;

  boolean letterbox = FALSE;
  boolean svary = FALSE;
  boolean has_non_lb_layers = FALSE;

  int ni, no, nins = n->n_inputs, nouts = n->n_outputs;

  //MSGMODE_ON(DEBUG);

  d_print_debug("CALC in / out sizes\n");

  if (n->flags & NODESRC_ANY_SIZE) svary = TRUE;
  if ((mainw->multitrack && prefs->letterbox_mt) || (prefs->letterbox && !mainw->multitrack)
      || (LIVES_IS_RENDERING && prefs->enc_letterbox))
    if (!svary) letterbox = TRUE;

  // get max and min widths and heights for all input layers
  for (ni = 0; ni < nins; ni++) {
    /* if (prefs->dev_show_timing) */
    /* 	d_print_debug("set input %d\n", ni); */


    in = n->inputs[ni];
    if (in->flags & NODEFLAGS_IO_SKIP) continue;
    d_print_debug("chksz1 %d %d %d %d\n", ni, letterbox, in->node->model_type, prefs->no_lb_gens);

    if (letterbox && in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens) {
      has_non_lb_layers = TRUE;
      continue;
    }

    d_print_debug("chksz2\n");
    // get size from prev output
    out = in->node->outputs[in->oidx];
    if (!out || !out->width || !out->height) continue;
    d_print_debug("chksz3\n");

    if (in->flags & NODEFLAG_IO_FIXED_SIZE) {
      // if the input is flagged as FIXED_SIZE, then we use whateve size is currently set
      // the only thing we want to check is, whether letterboxing is enabled,
      // if so we scale out size to fit in output size, reather than ssterecthgin or shirnking to exactly
      // fixed size
      if (letterbox) {
	width = out->width;
	height = out->height;
	calc_maxspect(in->width, in->height, &width, & height);
	if (width <= in->width && height <= in->height) {
	  in->inner_width = width;
	  in->inner_height = height;
	}
      }
      // FIXED_SIZE done
      break;
    }
    d_print_debug("chksz4\n");

    if (NODE_IS_SINK(n)) {
      boolean can_resize = FALSE, can_letterbox = FALSE;
      if (n->model_type == NODE_MODELS_OUTPUT) {
	// for ink outputs, we may have fixed size inputss or these maybe vairiabl
	// (generally we only have one input per sink, though for example it could take a picture layer
	// and a subtitle layer)

	// if the plugin the node represents can resize we can jusst ue the size as output from prev node
	// if not theen w3 have to use the op (display) sizesome plugins can also do letterboxing
	// in this ase, if it cannot resize, if the output from pre v node < displayu size, we can
	// sstretch or shrink output size, so 2 edges just touch and letterbox to display size.
	// if the out size is

	_vid_playback_plugin *vpp = (_vid_playback_plugin *)n->model_for;
	if (vpp && (vpp->capabilities & VPP_LOCAL_DISPLAY)) {
	  if (vpp->capabilities & VPP_CAN_RESIZE) can_resize = TRUE;
	  if (vpp->capabilities & VPP_CAN_LETTERBOX) can_letterbox = TRUE;
	}
      }
      if (!can_resize) {
	// output cannot resize - this is basically the same as having fixed size == ouptut size
	in->width = opwidth;
	in->height = opheight;
	if (letterbox) {
	  width = out->width;
	  height = out->height;
	  calc_maxspect(in->width, in->height, &width, &height);
	  in->inner_width = width;
	  in->inner_height = height;
	  if (can_letterbox) {
	    in->width = in->inner_width;
	    in->height = in->inner_height;
	  }
	}
      } else {
	// output CAN resize, if not letterboxing we can
	// jusst use out_size
	if (!letterbox || can_letterbox) {
	  in->inner_width = in->width = out->width;
	  in->inner_height = in->height = out->height;
	} else {
	  // can resize, but cannot letterbox, and banindg to get ar of output
	  width = in->inner_width = out->width;
	  height = in->inner_height = out->height;
	  calc_minspect(opwidth, opheight, &width, &height);
	  in->width = width;
	  in->height = height;
	}
      }
      break;
    }

    in->width = in->inner_width = out->width;
    in->height = in->inner_height = out->height;

    if (prefs->dev_show_timing)
      d_print_debug("set in sizes %d X %d\n", in->width, in->height);

    if (nins == 1) {
      opwidth = in->width;
      rmaxw = rminw = xmaxw = (double)opwidth;
      opheight = in->height;
      rmaxh = rminh = xmaxh = (double)opheight;
      break;
    }

    if (!in->width || !in->height) continue;

    // if we have mixed letterboxing / non letterboxing layers, we note this
    // then we calculate size excluding these layers
    // then the size of these layers is minspect (bounding box) to a.r of display

    if (rminw == 0. || (double)in->width < rminw) rminw = (double)in->width;
    if ((double)in->width > rmaxw) rmaxw = (double)in->width;

    if (rminh == 0. || (double)in->height < rminh) rminh = (double)in->height;
    if ((double)in->height > rmaxh) rmaxh = (double)in->height;
  }

  d_print_debug("maxw is %f, maxh is %f, rminw ois %f rimnh is %f\n",
		rmaxw, rmaxh, rminw, rminh);

  if (nins > 1 && rmaxw > 0. && rmaxh > 0.) {
    // we can have cases where all inputs are from srcs with var. size
    // if so then we are going to set node and output sizes to 0
    // and fix this ascending - depending on quality

    // otherwise we have rmaxw, rmaxh, rminw, rminh
    // if not letterboxing, we want to make a rectangle with a.r equal to output
    // size doesn't matter yet

    if (!letterbox) {
      xwidth = rmaxw;
      xheight = rmaxw / op_ar;
    } else {
      // for letterboxing: all layers keep their original a.r
      // we found rmaxw, rmaxh, we expand layers to fit inside this
      // we have two options - find the widest layer, then shrink / expand all other layers vertically
      // to fit inside this - the widest layer now defines the new bounding box and all other layers fit inside
      // or find tallest layer, set all to its width, find new tallest
      // thus we may have 2 new bounding boxes, for each of these,
      // this new bounding box must then (eventually) fit inside output where it may gain additional banding

      // ideally we pick the option which needs minimal banding internally, and min banding externally
      // note first, all layers will have either same height or same width. The size in the other dimension will vary
      // we can take the (one or more layers) with max value in the unbounded dimension and call this the outer layer
      // Some quantity of other layers will have unbounded dimension < than this. These are inner layers
      // These layers will have banding in the unbounded dimension, which we can call internal banding
      // In addition, the outer layer will gain banding to fit in output a.r (or it may gain inner banding at another fx)
      // One consideration is the spread of sizes in the unbounded dimenaion. If we have layers close together
      // it looks better than having them all spread out.
      // So the factors we have to consider: spread of inner banding, total inner banding, amount of outer banding
      // example - sorted inner banding 1, 5, 6  - we can calulate spread as 6 - 5 + 5 - 1 == 5, total == 12
      // or better, as fractions - if with is 10 then we have spread == 0.5, total == 1.2
      // and we can calculate extrnal banding as
      // MIN(opwidth / bbwidth * opheiht / bbhheight, bbheight / opheight * opwidth / bbwidth))
      // now we can apply weigthings to these 3 values and find min. This tells us wther to use max width or max heihgt
      // we then scale all layers accordingly to find input / output sizes. Then finally we scale calulated size up / down
      // to fit in screen size, or leave as is depending on quality. (e.g for high we scale up to fit unbounded in
      // MAX(screen or narrowest scaled layer) in mid, MIN(output, max unbounded), in low MIN(output, min unbounded)

      const double we = 1.0, wi = 1.0, ws = 1.0;

      double spreadh, spreadv, totibh = 0., totibv = 0., extbh, extbv;
      double *bandh = lives_calloc(nins, sizdbl);
      double *bandv = lives_calloc(nins, sizdbl);
      double xmaxw = rmaxw, xmaxh = rmaxh;
      double score_h, score_v;

      // suffix h is for horizontal banding, ie, y axis unbounded
      // suffix v is for vertical banding, ie, x axis unbounded

      xmaxw = rmaxw;
      xmaxh = rmaxh;


      for (ni = 0; ni < nins; ni++) {
	// calculate expanded bounding box
	// find widest and tallest, note opposite directions, we will have rmaxw X xmaxh, xmaxw X rmaxh
	in = n->inputs[ni];
	if (in->flags & NODEFLAGS_IO_SKIP) continue;
	if (letterbox && in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens) continue;
	if (!in->width || !in->height) continue;

	if (in->width == rmaxw && in->height < xmaxh) xmaxh = in->height;
	if (in->height == rmaxh && in->width < xmaxw) xmaxw = in->width;
      }
      // pass 2, set heights to xmaxh and update rmaxw, etc
      for (ni = 0; ni < nins; ni++) {
	// calculate expanded bounding box
	// find widest and tallest, note opposite directions, we will have rmaxw X xmaxh, xmaxw X rmaxh
	in = n->inputs[ni];
	if (in->flags & NODEFLAGS_IO_SKIP) continue;
	if (letterbox && in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens) continue;
	if (!in->width || !in->height) continue;

	if (in->width * xmaxh / in->height > rmaxw) rmaxw = in->width * xmaxh / in->height;
	if (in->height * xmaxw / in->width > rmaxh) rmaxh = in->height * xmaxw / in->width;
      }

      // our bbox is now either rmaxw X xmaxh OR xmaxw x rmaxh

      // find extern banding

      // check first: unbounded width -> h banding
      width = (int)(xmaxw + .5);
      height = (int)(rmaxh + .5);

      // fit in output rectangle
      calc_maxspect(opwidth, opheight, &width, &height);
      extbh = 1. - (width * height) / (opwidth * opheight);

      // check second: unbounded height - v banding
      width = (int)(rmaxw + .5);
      height = (int)(xmaxh + .5);

      // fit in output rectangle
      calc_maxspect(opwidth, opheight, &width, &height);
      extbv = 1. - (width * height) / (opwidth * opheight);

      // find total internal banding
      for (ni = 0; ni < nins; ni++) {
	in = n->inputs[ni];
	if (in->flags & NODEFLAGS_IO_SKIP) continue;
	if (letterbox && in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens) continue;
	if (!in->width || !in->height) continue;

	xwidth = in->width * (double)rmaxh / (double)in->height;
	xheight = in->height * (double)rmaxw / (double)in->width;

	totibv += (xmaxw - xwidth) / xmaxw;
	totibh += (xmaxh - xheight) / xmaxh;

	bandh[ni] = (xmaxw - xwidth) / xmaxw;
	bandv[ni] = (xmaxh - xheight) / xmaxh;
      }

      // calculate spread for each option
      spreadv = ord_diff(bandv, nins);
      spreadh = ord_diff(bandh, nins);
      lives_free(bandh); lives_free(bandv);

      // now have 3 vals for each
      score_h = extbh * we + totibh * wi + spreadh * ws;
      score_v = extbv * we + totibv * wi + spreadv * ws;

      xwidth = xmaxw;
      xheight = xmaxh;

      if (score_h > score_v) xheight = rmaxh; // vertical banding preferred, size is xmaxw X rmaxh -
      else xwidth = rmaxw; // rmaxw X xmaxh

      d_print_debug("score_h == %f, score_v == %f\n", score_h, score_v);
    }

    // now have our bounding box xwidth X xheight, we will expand or shrink this according to quality level

    // for high q we expand to contain MAX(largest layer, out size)
    // for mid we expand to contain MIN(largest layer, out size)
    // for low we expand / shrink to contain MIN(mallest layer, out size)

    width = (int)(xwidth + .5);
    height = (int)(xheight + .5);

    d_print_debug("size before q adjustemnt: %d X %d\n", width, height);

    switch (prefs->pb_quality) {
    case PB_QUALITY_HIGH:
      calc_minspect(rmaxw, rmaxh, &width, &height);
      width = xwidth;
      height = xheight;
      if (xwidth > opwidth || xheight > opheight)
	calc_minspect(opwidth, opheight, &width, &height);
      else
	calc_maxspect(opwidth, opheight, &width, &height);
      break;
    case PB_QUALITY_MED:
      calc_minspect(rmaxw, rmaxh, &width, &height);
      if (xwidth > opwidth || xheight > opheight) {
	width = xwidth;
	height = xheight;
	calc_minspect(opwidth, opheight, &width, &height);
      }
      break;
    case PB_QUALITY_LOW:
      calc_minspect(rminw, rminh, &width, &height);
      if (xwidth > opwidth || xheight > opheight) {
	width = xwidth;
	height = xheight;
	calc_maxspect(opwidth, opheight, &width, &height);
      }
      break;
    default: break;
    }

    d_print_debug("size after q adjustemnt: %d X %d\n", width, height);

    d_print_debug("2maxw is %f, maxh is %f, xmaxw is %f, xmaxh is %f, rminw ois %d rimnh is %d\n",
		  rmaxw, rmaxh, xmaxw, xmaxh, opwidth, opheight);

    // if we are letterboxing and have layers that do not leetterbox, (e.g generators with pref no_lb_gens)
    // then we will take the new bounding box, create another box around this with a.r of output
    // - this now becomes the bounding box size. Layers which do not letterbox will then fill this
    // other layer will letterbox inside this. So all layers now get width, height of outer box
    // but keep the same inner_size. Layers which do not letterbox get inner size == outer size

    // opwidth, opheight will be et to the 'screen' size
    // width, height are the inner bounding box size
    // these will be identical unless we have a generaotr which fills the whole screen
    // in which cae opwidht, opheight get a.r of screen

    if (has_non_lb_layers) {
      int xopwidth = opwidth, xopheight = opheight;
      d_print_debug("some layers do not letterbox, adjust size  %d X %d to ar %d X %d\n", width, height, opwidth, opheight);
      // find min box with ar, opwidth, opheight which contains w, h
      calc_minspect(width, height, &opwidth, &opheight);
      d_print_debug("some layers do not letterbox, adjusted size to  %d X %d\n", opwidth, opheight);
      if (prefs->pb_quality == PB_QUALITY_HIGH) {
	if (opwidth < xopwidth || opheight < xopheight) {
	  opwidth = xopwidth;
	  opheight = xopheight;
	}
      } else if (opwidth > xopwidth || opheight > xopheight)
	if (opwidth < xopwidth || opheight < xopheight) {
	  opwidth = xopwidth;
	  opheight = xopheight;
	}
    }  else {
      width = opwidth;
      height = opheight;
    }

    opwidth = (opwidth >> 2) << 2;
    opheight = (opheight >> 2) << 2;

    d_print_debug("Final bounding box size is %d X %d. Some layers may lb inside this\n", opwidth, opheight);

    for (ni = 0; ni < n->n_inputs; ni++) {
      in = n->inputs[ni];

      if (in->flags & NODEFLAGS_IO_SKIP) continue;

      xwidth = in->width;
      xheight = in->height;

      in->inner_width = in->width = opwidth;
      in->inner_height = in->height = opheight;

      if (!xwidth || !xheight) continue;

      if (letterbox) {
	if (!(in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens)
	    && !svary && rmaxw && rmaxh) {
	  if (opwidth * xheight / xwidth > opheight) {
	    in->inner_height = opheight;
	    in->inner_width = opheight * xwidth / xheight;
	  } else {
	    in->inner_width = opwidth;
	    in->inner_height = opwidth * xheight / xwidth;
	  }

	  in->inner_width = (in->inner_width >> 2) << 2;
	  in->inner_height = (in->inner_height >> 2) << 2;
	}
      }

      d_print_debug("input chan %d got sizes %d X %d (%d X %d)\n",
		    ni, in->width, in->height, in->inner_width, in->inner_height);
      d_print_debug("3maxw is %f, maxh is %f, rminw ois %d rminh is %d\n",
		    rmaxw, rmaxh, opwidth, opheight);
    }
  }

  if (rmaxw > 0 && rmaxh > 0) {
    if (!svary) {
      for (no = 0; no < nouts; no++) {
	out = n->outputs[no];

	if (out->flags & NODEFLAGS_IO_SKIP) continue;

	out->width = opwidth;
	out->height = opheight;

	if (n->model_type == NODE_MODELS_GENERATOR
	    || n->model_type == NODE_MODELS_FILTER) {
	  weed_filter_t *filter = (weed_filter_t *)n->model_for;
	  weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, no, NULL, LIVES_OUTPUT);
	  validate_channel_sizes(filter, chantmpl, &out->width, &out->height);
	}

	d_print_debug("output chan %d got sizes %d X %d\n",
		      no, out->width, out->height);
      }
    }
  }
  //MSGMODE_OFF(DEBUG);
}


static int cost_summation[N_COST_TYPES];
static boolean sumty = FALSE;

// nodetpye can heve the follwoing values:
/* NODE_MODELS_CLIP		*/
/* NODE_MODELS_FILTER		*/
/* NODE_MODELS_GENERATOR       	*/
/* NODE_MODELS_OUTPUT		*/
/* NODE_MODELS_SRC		*/
/* NODE_MODELS_INTERNAL		*/

static lives_result_t prepend_node(lives_nodemodel_t *nodemodel, inst_node_t *n, int track, inst_node_t *target) {
  // create a forward link from n to target
  // first check out_tracks in n, ensure track is in the list - this will tell us which output to use
  // - there may only be 1 output for any track
  // then check in_tracks in target, this tells which inputs to use, there has to 1 or more with matching track
  // out with track number will be connected to matching input(s)
  // then for each input we set the details, the second and subsquent get the clone flag set
  // this indicates the output layer will be duplicated

  // we match an input from target to an output from n, using track numbers
  // we fill the the input values for the target and the output details for n

  input_node_t *in;
  output_node_t *out;
  int *pals;
  int op_idx = -1, main_idx = -1, ip_idx = -1, i, npals;

  if (prefs->dev_show_timing)
    d_print_debug("prepend node %p to %p for track %d\n", n, target, track);

  // locate output in n to connect from

  for (i = 0; i < n->n_outputs; i++) {
    if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    if (n->outputs[i]->track == track) {
      if (prefs->dev_show_timing)
	d_print_debug("found output idx %d\n", i);
      // found an output for this track
      if (op_idx == -1) main_idx = i;
      op_idx = i;
      // if it is unconnected, we can  use it
      if (!n->outputs[i]->node) break;
      // otherwise, this is going to be a clone of an earlier output
    }
  }

  // we did not find an output for this track
  if (i == n->n_outputs) return LIVES_RESULT_ERROR;

  // locate input in target to connect to

  for (i = 0; i < target->n_inputs; i++) {
    if (target->inputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    if (prefs->dev_show_timing)
      d_print_debug("CF in track %d and wanted track %d\n", target->inputs[i]->track, track);
    if (target->inputs[i]->track == track) {
      if (prefs->dev_show_timing)
	d_print_debug("found, ip idx is %d with node %p\n", i, target->inputs[i]->node);
      // found an input for the track
      // this must be  unconnected, because if we have clone inputs
      // they are never connected to a node, but are simply copies of another parent input
      if (target->inputs[i]->node) return LIVES_RESULT_ERROR;
      ip_idx = i;
      break;
    }
  }

  // failed to find an input
  if (ip_idx == -1) return LIVES_RESULT_ERROR;

  if (prefs->dev_show_timing)
    d_print_debug("ADD output node %p as %d for %p, connecting to in %d\n", target, op_idx, n, ip_idx);
  out = n->outputs[op_idx];
  out->node = target;
  out->iidx = ip_idx;

  if (main_idx != -1 && main_idx != op_idx) {
    d_print_debug("MAIN IDX is %d and op_idx == %d\n", main_idx, op_idx);
    out->origin = main_idx;
    out->flags |= NODEFLAG_IO_CLONE;
    //
  } else {
    // if n is a filter, set the details for the src output
    // for other source types (layer, blank), the outputs will produce whatever the input requires
    switch (n->model_type) {
    case NODE_MODELS_SRC:
      if (!n->npals) {
	out->flags |= NODESRC_ANY_SIZE;
	out->flags |= NODESRC_ANY_PALETTE;
	n->npals = n_allpals;
	n->pals = allpals;
      }
      break;

    case NODE_MODELS_CLIP: {
      // for clip_srcs
      for (i = 0; i < n->n_clip_srcs; i++) {
	n->inputs[i]->best_src_pal = (int *)lives_calloc(N_COST_TYPES * n_allpals, sizint);
	n->inputs[i]->min_cost = (double *)lives_calloc(N_COST_TYPES * n_allpals, sizdbl);
      }
      out->flags |= NODESRC_ANY_PALETTE;

      if (!out->width || !out->height) {
	lives_clip_t *sfile = (lives_clip_t *)n->model_for;
	if (sfile) {
	  out->width = sfile->hsize;
	  out->height = sfile->hsize;
	  out->flags |= NODEFLAG_IO_FIXED_SIZE;
	}
      }
      n->npals = n_allpals;
      n->pals = allpals;

      // quick optimisation - if n is a src, set node gamma to match target gamma
      // when we align with model, we will set apparent_gamma for the primary src_group
      // this will avoid the need for a separate gamma conversion following the conversion to apparent_pallete
      n->gamma_type = target->gamma_type;
    }
      break;

    case NODE_MODELS_GENERATOR:
    case NODE_MODELS_FILTER: {
      weed_filter_t *filter = (weed_filter_t *)n->model_for;
      weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, op_idx, NULL, LIVES_OUTPUT);
      int *pals, npals;
      boolean pvary = weed_filter_palettes_vary(filter);
      int sflags = weed_chantmpl_get_flags(chantmpl);

      out->maxwidth = weed_get_int_value(chantmpl, WEED_LEAF_MAXWIDTH, NULL);
      out->maxheight = weed_get_int_value(chantmpl, WEED_LEAF_MAXHEIGHT, NULL);
      out->minwidth = weed_get_int_value(chantmpl, WEED_LEAF_MINWIDTH, NULL);
      out->minheight = weed_get_int_value(chantmpl, WEED_LEAF_MINHEIGHT, NULL);

      if (sflags & WEED_CHANNEL_CAN_DO_INPLACE) {
	weed_chantmpl_t *ichantmpl = get_nth_chantmpl(n, op_idx, NULL, LIVES_INPUT);
	if (ichantmpl) {
	  n->inputs[op_idx]->flags |= NODESRC_INPLACE;
	}
      }

      pals = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);

      if (pvary) {
	out->pals = pals;
	out->npals = npals;
      } else {
	n->pals = pals;
	n->npals = npals;
      }

      // get current palette, width and height from channel

      if (sflags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
	out->flags |= NODESRC_REINIT_SIZE;
      if (sflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
	out->flags |= NODESRC_REINIT_PAL;
      if (sflags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
	out->flags |= NODESRC_REINIT_RS;
    }
      break;
      // OUTPUT, INTERNAL should have no out chans, and in any case they dont get
      // prepended to anything !
    default: break;
    }
  }

  // input node in target

  in = target->inputs[ip_idx] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));

  in->node = n;
  in->oidx = op_idx;

  npals = target->npals;
  if (in->npals) npals = in->npals;

  //in->min_cost = (double *)lives_calloc(N_COST_TYPES * npals, sizdbl);

  switch (target->model_type) {
  case NODE_MODELS_FILTER: {
    weed_filter_t *filter = (weed_filter_t *)target->model_for;
    weed_chantmpl_t *chantmpl = get_nth_chantmpl(target, ip_idx, NULL, LIVES_INPUT);
    int sflags = weed_chantmpl_get_flags(chantmpl);
    boolean svary = weed_filter_channel_sizes_vary(filter);
    boolean pvary = weed_filter_palettes_vary(filter);

    weed_instance_t *inst = target->model_inst;
    if (inst) {
      weed_channel_t *channel = get_enabled_channel(inst, ip_idx, LIVES_INPUT);
      if (channel) {
	in->cpal = weed_channel_get_palette(channel);
      }
    }

    if (sflags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
      in->flags |= NODESRC_REINIT_SIZE;
    if (sflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
      in->flags |= NODESRC_REINIT_PAL;
    if (sflags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
      in->flags |= NODESRC_REINIT_RS;

    pals = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);
    target->free_pals = TRUE;

    if (pvary) {
      in->pals = pals;
      in->npals = npals;
    } else {
      target->pals = pals;
      target->npals = npals;
    }

    if (svary) in->flags |= NODESRC_ANY_SIZE;

    for (i = ip_idx + 1; i < target->n_inputs; i++) {
      // mark any clone inputs
      if (target->inputs[i]->track == track) {
	target->inputs[i]->flags |= NODEFLAG_IO_CLONE;
	target->inputs[i]->origin = ip_idx;
      }
    }
  }
    break;

  case NODE_MODELS_OUTPUT: {
    _vid_playback_plugin *vpp = (_vid_playback_plugin *)target->model_for;
    int *xplist;
    int npals = 1;
    int cpal = vpp->palette;
    if (vpp->capabilities & VPP_CAN_CHANGE_PALETTE) {
      int *pal_list = (vpp->get_palette_list)();
      for (npals = 0; pal_list[npals] != WEED_PALETTE_END; npals++);
      xplist = (int *)lives_calloc(npals + 1, sizint);
      for (i = 0; i < npals; i++) xplist[i] = pal_list[i];
    } else {
      xplist = (int *)lives_calloc(2, sizint);
      xplist[0] = cpal;
    }
    xplist[npals] = WEED_PALETTE_END;

    target->cpal = cpal;
    target->npals = npals;
    target->pals = xplist;
    target->free_pals = TRUE;

    if (!(vpp->capabilities & VPP_CAN_RESIZE))
      in->flags |= NODEFLAG_IO_FIXED_SIZE;
    in->inner_width = in->width = nodemodel->opwidth;
    in->inner_height = in->height = nodemodel->opheight;
  }
    break;
  case NODE_MODELS_INTERNAL:
    if (!target->n_outputs) {
      int *xplist = (int *)lives_calloc(3, sizint);
      xplist[0] = WEED_PALETTE_RGB24;
      xplist[1] = WEED_PALETTE_RGBA32;
      xplist[2] = WEED_PALETTE_END;
      target->cpal = WEED_PALETTE_RGB24;
      target->npals = 2;
      target->pals = xplist;
      target->free_pals = TRUE;
      in->flags |= NODEFLAG_IO_FIXED_SIZE;
      in->inner_width = in->width = nodemodel->opwidth;
      in->inner_height = in->height = nodemodel->opheight;
      d_print_debug("set sink sizes %d X %d\n", in->width, in->height);
    }
    break;
    // target cannot be a SRC or CLIP, these have no inputs, so we cannot prepend anything to them
  default: break;
  }

  return LIVES_RESULT_SUCCESS;
}


/* static lives_result_t append_node(inst_node_t *n, int track, inst_node_t * target) { */
/*   return prepend_node(target, track, n); */
/* } */


// get node_chain for track - during construction this will retunr the lastest, otherwise the earliest
static node_chain_t *get_node_chain(lives_nodemodel_t *nodemodel, int track) {
  for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    if (nchain->track == track) return nchain;
  }
  return NULL;
}


static int find_output_track(lives_nodemodel_t *nodemodel) {
  // create an int array of size ntracks
  // the we step throughnode_chains, find the track, and set teh follwoing
  // - if we find a TERMINATED chain, and array val is 0, set it to 2
  // - if we find an UNTERMINATED node_chain, set it to 1
  //   also note min_track for unterminated chains
  // --
  // then check if we have a non NULL layer betwwen 0 and min_track, where the
  // array value iss 0, if so we return that as the track, otherwise we return min_track
  // thus we return the idx of the first non_NULL layer which is not in any chain, or the trackof the first
  // unterminated node_chain, whicheer of these is the lower
  //
  // if all of the above fail then we return the default -1, and a blank frame src will be created

  int *vals = (int *)lives_calloc(nodemodel->ntracks, sizint);
  int min_track = nodemodel->ntracks, track;

  for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    track = nchain->track;
    if (track < 0 || track >= nodemodel->ntracks) continue;
    if (!(nchain->terminated)) {
      vals[track] = 1;
      if (min_track == -1 || track < min_track) {
	min_track = track;
      }
    } else if (vals[track] == 0) vals[track] = 2;
  }
  for (track = 0; track < min_track; track++)
    if (!vals[track] && nodemodel->clip_index[track] >= 0) {
      min_track = track;
      break;
    }
  lives_free(vals);
  if (min_track == nodemodel->ntracks) return -1;
  return min_track;
}


// prepend a new node_chain for src node to the nodemodel
// if we have an existing unterminated node_Chain for the track, then this is an ERROR
// and we return NULL, otherwise we retunr a pointer to the newly prepended
static node_chain_t *add_src_node(lives_nodemodel_t *nodemodel, inst_node_t *n, int track) {
  // check first to ensure we don't have an active uncommited node_chain
  node_chain_t *nchain = get_node_chain(nodemodel, track);
  if (nchain && !(nchain->terminated)) return NULL;

  nchain = (node_chain_t *)lives_calloc(1, sizeof(node_chain_t));
  nchain->track = track;
  nchain->first_node = nchain->last_node = n;
  nodemodel->node_chains = lives_list_prepend(nodemodel->node_chains, (void *)nchain);
  return nchain;
}


static node_chain_t *fork_output(lives_nodemodel_t *nodemodel, inst_node_t *n, inst_node_t *dest, int track) {
  // - find node for in_track track,
  // - add another output for the track, flagged with !chCLONE
  // - add the node as first_node in anew node_chain, checking to ensure there i no exisitng unterminated chain
  // - prepend the new node_chain in the nodemodel
  // - extend the node_chain last_node to the current node and connect the forked output to th e input
  //
  // when we arrive at source node, we will copy the output layer, adding an extra layer_copy time cost
  // keep track of the layer cahce memory needed if we exceed the threshold we must warn
  node_chain_t *newchain = NULL;
  for (int i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    if (n->inputs[i]->track == track) {
      output_node_t *out;
      inst_node_t *p = n->inputs[i]->node;
      int orig = -1;
      // this will be a clone, check first to find the original output it should be cloned from
      for (int j = 0; j < p->n_outputs; j++) {
	if (p->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
	if (p->outputs[j]->track == track) {
	  orig = j;
	  break;
	}
      }
      // thisis an error, we could not locate an original output in the source node
      // with the correct track number, this is wrong as we should be cloning an exisitng track
      if (orig == -1) return NULL;

      // work out memory space we need - either we will use up a bigblock or we will allocate memory for the clone
      // we can reduce this when we process and link to the outptut

      p->outputs = (output_node_t **)lives_recalloc(p->outputs, p->n_outputs + 1,
						    p->n_outputs, sizeof(output_node_t *));
      out = p->outputs[p->n_outputs] = (output_node_t *)lives_calloc(1, sizeof(output_node_t));

      out->flags = NODEFLAG_IO_CLONE;
      out->origin = orig;
      out->track = track;

      // we will release the resvd bblock or cached mem at the end of the node processing this

      p->n_outputs++;

      // returns NULL on error
      newchain = add_src_node(nodemodel, p, track);
      return newchain;
    }
  }
  // this should never happen - it means that the last_node for a node_chain has no input
  // for the track it purportedly follows
  return NULL;
}


// check node_inputs if we find one not connected to a node then we need to find a source
// we go through nodsrcs, looking for the most recent source for the track
// if we dont find anything, then we check if there is a corresponding layer, if so we create
// a layer source node for the layer and prepend it
// if there is no layer, then this becomes a blank frame source
// if we do find a current source in the list, then we check if is flagged as connected, if not we can simply
// prepend it, otherwise we need to go to the prior node for the track and add an extra output,
// this then becomes a new unconnected sourc, which we can then prepend and possibly mark as connected

// then we check the out_tracks
// if we have an input without an output we set the connected flag for the current source for the track
// if we have an out track without and input then we add this node as a src for the track
// - if there is an existing unconnected src then this in an error

static int check_node_connections(lives_nodemodel_t *nodemodel, inst_node_t *n) {
  // here we are going to merge a new node into an existing graph
  // begin by checking the inputs, ensuring thes are all connected to a new
  // or exisitng unterminated node chain
  //
  int i, j;

  for (i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    if (!n->inputs[i]->node) {
      int track = n->inputs[i]->track;
      node_chain_t *in_chain = get_node_chain(nodemodel, track);
      // if we have no src, return to caller so it can add a src for the track
      d_print_debug("searching for node_chain for track %d\n", track);

      if (!in_chain) {
	d_print_debug("not found, add a source for track %d\n", track);
	return track;
      }
      d_print_debug("chain located ");
      if (in_chain->terminated) {
	d_print_debug("but is terminated !\n");
	// if we do have a node_chain for the track, but it is terminated
	// then we are going to back up one node and a clone output
	// this means that the track sources will fork, one output will go to
	// the terminator node, while the other output will be a layer copy
	// creating a new node_chain for this track,
	// which will be an input to this node
	in_chain = fork_output(nodemodel, in_chain->last_node, n, track);

	if (!in_chain) {
	  // this means something has gone wrong, either we found an unterminated node_chain
	  // which should have prepend before the teminated one
	  // if this is the cae it means eithe the node_chains are out of order, or
	  // a chain was terminated, whilst another unerminated node_chain existed
	  // - it should not be possible to add a new node_chain for a track when
	  // there is still an exisitn unterminated one - we check for this when adding and the code should
	  // check for this as well before creating a new node_chain
	  // OR, we failed to find an input in this node for 'track'
	  // or we failed to find an existtng output
	  lives_abort("No in_chain found for track after froking output");
	}
      }

      if (prefs->dev_show_timing)
	d_print_debug("\n");

      // here we found an unterminated chain - either from a source just added, or a forekd output
      // or else from the normal output from another instance
      prepend_node(nodemodel, in_chain->last_node, track, n);
      in_chain->last_node = n;

      for (j = 0; j < n->n_outputs; j++) {
	if (n->outputs[j]->flags & NODEFLAGS_IO_SKIP) continue;
	if (n->outputs[j]->track == track) break;
      }
      if (j == n->n_outputs) in_chain->terminated = TRUE;
    }
  }

  // once all inputs are connected, we may be able to calulate the input size for node
  // if we have only a single input, we  do not need to do any resizing, unless the frame is realtively large
  // e.g coming from a source
  // if we have mutliple inputs then we use a heuristic approach,
  // in some cases we will have sources wutrh no fixed frame size, in this casse we wait for the next pass
  // ascending, which
  //calc_node_sizes(nodemodel, n);

  // after connecting all inputs, we check outputs
  // if any out_track lacks a corresponding in track:
  //
  // - if the track has an unconnected node_chain, this is an error
  // - otherwise this node becomes a src for the track, abd we begin a new node_chain here
  //
  // for out tracks tha DO have a corresponding input, the node chian simply contues to the noxt node
  // (if the output forks, then we would have already created new node_chains when adding the forked output)
  // (if we have multiple outs for a track, and these were NOT added as clones, then this is a
  // programming erro, tracks should not appear multiple times in the original out_tracks)
  for (j = 0; j < n->n_outputs; j++) {
    if (n->outputs[j]->flags & NODEFLAGS_IO_SKIP) continue;
    for (i = 0; i < n->n_inputs; i++) {
      if (n->inputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
      if (n->outputs[j]->track == n->inputs[i]->track) break;
    }
    if (i == n->n_inputs) {
      // output but no input, add src
      node_chain_t *in_chain = get_node_chain(nodemodel, n->outputs[j]->track);
      // check to make sure there is no unterminated node_chain for this track
      // we cannot have > 1 unterminated chain simultaneously on same track
      // if there is a layer for the track, and no node_chain,
      // this is allowed, it simply means that layer is unused
      if (in_chain && !in_chain->terminated) {
	// TODO - throw error
	n->outputs[j]->flags |= NODEFLAG_IO_IGNORE;
      }
      //
      else add_src_node(nodemodel, n, n->outputs[j]->track);
    }
  }
  return -1;
}


static int node_idx = 0;

// create a new inst_node, create teh inputs and outputs
//

// nodetype can heve the follwoing values:
/* NODE_MODELS_CLIP		*/
/* NODE_MODELS_FILTER		*/
/* NODE_MODELS_OUTPUT		*/
/* NODE_MODELS_SRC		*/
/* NODE_MODELS_INTERNAL		*/
static inst_node_t *create_node(lives_nodemodel_t *nodemodel, int model_type, void *model_for,
				int nins, int *in_tracks, int nouts, int *out_tracks) {
  inst_node_t *n = (inst_node_t *)lives_calloc(1, sizeof(inst_node_t));
  int i;

  if (!n) return NULL;

  n->model = nodemodel;
  n->idx = node_idx++;
  n->model_type = model_type;
  n->model_for = model_for;

  // default unless overrridden
  // - can be overridden in 2 ways - if a source produces frames in another gamma_type
  // (e.g linear, bt709), we can opt to maintain that for the following node(s)
  // - some filters prefer linear gamma, so optionally we can convert to / from that
  // - some sources may prefer a different gamma_type from the standard (linear or bt709)
  //   (e.g -future possibility- output to HDTV, or currently when transcoding in bt709)
  // -> in these cases, the source will define the "apparent_gamma" for the clip_srcgroup
  // bound to the track. When an ouput node from a source is connected, we can choose to
  // maintain current gamma or to convert it. If the gamma is bt709, and we convert to yuv,
  // this becomes yuv_supspace_bt709, and we should use different conversion constants

  n->gamma_type = WEED_GAMMA_SRGB;

  if (prefs->dev_show_timing) {
    char *nname = get_node_name(n);
    d_print_debug("created node %p, type %s with %d ins aand %d outs\n", n, nname, nins, nouts);
    lives_free(nname);
  }

  // allocate input and output subnodes, but do not set the details yet
  // for efficiency we only do this when a connection is made

  if (n->model_type == NODE_MODELS_CLIP) {
    // for clip models, we create an input for each lip_src, bur do not count thes in n->n_inputs
    // instead we create teh inputs with an extra NULL input at the end
    n->n_clip_srcs = nins;
  } else n->n_inputs = nins;

  if (nins) {
    n->inputs = (input_node_t **)lives_calloc(nins, sizeof(input_node_t *));
    for (i = 0; i < nins; i++) {
      n->inputs[i] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));
      if (in_tracks) n->inputs[i]->track = in_tracks[i];
    }
  }

  if (nouts) {
    n->n_outputs = nouts;
    n->outputs = (output_node_t **)lives_calloc(nouts, sizeof(output_node_t *));
    for (i = 0; i < n->n_outputs; i++) {
      n->outputs[i] = (output_node_t *)lives_calloc(1, sizeof(output_node_t));
      if (out_tracks) n->outputs[i]->track = out_tracks[i];
    }
  }

  // now set some type specific things
  // - for clip, add the clip_srcs
  // - for instance, set any flags which affect the node
  switch (n->model_type) {
  case NODE_MODELS_INTERN:
    for (i = 0; i < nins; i++) {
      input_node_t *in = n->inputs[i];
      in->width = nodemodel->opwidth;
      in->height = nodemodel->opheight;
      in->flags |= NODEFLAG_IO_FIXED_SIZE;
    }
    break;
  case NODE_MODELS_OUTPUT: {
    _vid_playback_plugin *vpp = NULL;
    if (n->model_type == NODE_MODELS_OUTPUT)
      vpp = (_vid_playback_plugin *)n->model_for;
    for (i = 0; i < nins; i++) {
      // node srcs are currently hardcoded, this will change
      input_node_t *in = n->inputs[i];
      in->width = nodemodel->opwidth;
      in->height = nodemodel->opheight;
    }
    if (vpp && (vpp->capabilities & VPP_LINEAR_GAMMA)) {
      n->flags |= NODESRC_LINEAR_GAMMA;
      n->gamma_type = WEED_GAMMA_LINEAR;
    }
    //if (prefs->hdtv) n->gamma_type = WEED_GAMMA_BT709;
  }
    break;

  case NODE_MODELS_GENERATOR: {
    weed_filter_t *filter = (weed_filter_t *)n->model_for;
    if (weed_filter_prefers_linear_gamma(filter)) {
      n->flags |= NODESRC_LINEAR_GAMMA;
      n->gamma_type = WEED_GAMMA_LINEAR;
    }
  }
    break;

  case NODE_MODELS_FILTER: {
    weed_filter_t *filter = (weed_filter_t *)n->model_for;
    if (weed_filter_prefers_linear_gamma(filter)) {
      n->flags |= NODESRC_LINEAR_GAMMA;
      n->gamma_type = WEED_GAMMA_LINEAR;
    }

    if (weed_filter_prefers_premult_alpha(filter))
      n->flags |= NODESRC_ALPHA_PREMULT;

    if (weed_filter_channel_sizes_vary(filter)) n->flags |= NODESRC_ANY_SIZE;
    if (weed_filter_palettes_vary(filter)) n->flags |= NODESRC_ANY_PALETTE;
    if (weed_filter_is_converter(filter)) n->flags |= NODESRC_IS_CONVERTER;
  }
    break;

    // SRC, OUTPUT, INTERNAL are handled when connected
  default: break;
  }

  nodemodel->nodes = lives_list_prepend(nodemodel->nodes, (void *)n);
  return n;
}


// TODO - check if we need to free pals

static void free_inp_nodes(int ninps, input_node_t **inps) {
  for (int i = 0; i < ninps; i++) {
    if (inps[i]) {
      if (inps[i]->best_src_pal) lives_free(inps[i]->best_src_pal);
      if (inps[i]->min_cost) lives_free(inps[i]->min_cost);
      if (inps[i]->cdeltas) lives_list_free_all(&inps[i]->cdeltas);
      if (inps[i]->true_cdeltas) lives_list_free_all(&inps[i]->true_cdeltas);
      if (inps[i]->free_pals && inps[i]->pals) lives_free(inps[i]->pals);
      lives_free(inps[i]);
    }
  }
}

static void free_outp_nodes(int nouts, output_node_t **outs) {
  for (int i = 0; i < nouts; i++) {
    if (outs[i]) {
      if (outs[i]->free_pals && outs[i]->pals) lives_free(outs[i]->pals);
      if (outs[i]->cdeltas) lives_list_free_all(&outs[i]->cdeltas);
      if (outs[i]->true_cdeltas) lives_list_free_all(&outs[i]->true_cdeltas);
      lives_free(outs[i]);
    }
  }
}


static void free_node(inst_node_t *n) {
  if (n) {
    if (n->model_type == NODE_MODELS_FILTER) {
      if (n->n_inputs || n->n_clip_srcs) {
	free_inp_nodes(n->n_inputs ? n->n_inputs : n->n_clip_srcs, n->inputs);
	lives_free(n->inputs);
      }
      if (n->n_outputs) {
	free_outp_nodes(n->n_outputs, n->outputs);
	lives_free(n->outputs);
      }
    }

    if (n->free_pals && n->pals) lives_free(n->pals);

    if (n->in_count) lives_free(n->in_count);
    if (n->out_count) lives_free(n->out_count);
    lives_free(n);
  }
}


static boolean pal_permitted(inst_node_t *n, int pal) {
  // check if palette is allowed according to node restriction flagbits

  // TODO - for no_reinit / locked we do need current cpal
  // but this may not be set when building the model
  //
  /* if (pal != n->cpal */
  /*     && ((n->flags & NODEFLAG_LOCKED) */
  /* 	  || ((n->flags & NODEFLAG_NO_REINIT) */
  /* 	      && ((n->flags & NODESRC_REINIT_PAL) */
  /* 		  || ((n->flags & NODESRC_REINIT_RS) */
  /* 		      && weed_palette_get_bytes_per_pixel(n->cpal) */
  /* 		      != weed_palette_get_bytes_per_pixel(pal)))))) */
  /*   return FALSE; */

  if ((n->flags & NODEFLAG_ONLY_RGB) && !weed_palette_is_rgb(pal)) return FALSE;
  if ((n->flags & NODEFLAG_ONLY_YUV) && !weed_palette_is_yuv(pal)) return FALSE;
  if ((n->flags & NODEFLAG_PRESERVE_ALPHA) && !weed_palette_has_alpha(pal)) return FALSE;
  if ((n->flags & NODEFLAG_ONLY_PACKED) && weed_palette_get_nplanes(pal) > 1) return FALSE;
  if ((n->flags & NODEFLAG_ONLY_PLANAR) && weed_palette_get_nplanes(pal) == 1) return FALSE;
  return TRUE;
}

static boolean input_pal_permitted(inst_node_t *n, int idx, int pal) {
  // check if palette is allowed according to node restriction flagbits
  input_node_t *in;
  if (!n || idx < 0 || idx >= n->n_inputs) return FALSE;
  in = n->inputs[idx];
  if (in->flags & NODEFLAGS_IO_SKIP) return FALSE;
  if (!in->npals) return pal_permitted(n, pal);
  /* if (pal != in->cpal */
  /*     && ((in->flags & NODEFLAG_LOCKED) */
  /* 	  || ((n->lags & NODEFLAG_NO_REINIT) */
  /* 	      && ((n->flags & NODESRC_REINIT_PAL) */
  /* 		  || ((in->flags & NODESRC_REINIT_RS) */
  /* 		      && weed_palette_get_bytes_per_pixel(in->cpal) */
  /* 		      != weed_palette_get_bytes_per_pixel(pal)))))) */
  /* return FALSE; */

  if ((in->flags & NODEFLAG_ONLY_RGB) && !weed_palette_is_rgb(pal)) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_YUV) && !weed_palette_is_yuv(pal)) return FALSE;
  if ((in->flags & NODEFLAG_PRESERVE_ALPHA) && !weed_palette_has_alpha(pal)) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_PACKED) && weed_palette_get_nplanes(pal) > 1) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_PLANAR) && weed_palette_get_nplanes(pal) == 1) return FALSE;
  return TRUE;
}

static boolean output_pal_permitted(inst_node_t *n, int idx, int pal) {
  // check if palette is allowed according to node restriction flagbits
  output_node_t *out;
  if (!n || idx < 0 || idx >= n->n_outputs) return FALSE;
  out = n->outputs[idx];
  if (out->flags & NODEFLAGS_IO_SKIP) return FALSE;
  if (!out->npals) return pal_permitted(n, pal);
  /* if (pal != out->cpal */
  /*     && ((out->flags & NODEFLAG_LOCKED) */
  /* 	  || ((n->lags & NODEFLAG_NO_REINIT) */
  /* 	      && ((out->flags & NODESRC_REINIT_PAL) */
  /* 		  || ((out->flags & NODESRC_REINIT_RS) */
  /* 		      && weed_palette_get_bytes_per_pixel(out->cpal) */
  /* 		      != weed_palette_get_bytes_per_pixel(pal)))))) */
  /* return FALSE; */

  if ((out->flags & NODEFLAG_ONLY_RGB) && !weed_palette_is_rgb(pal)) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_YUV) && !weed_palette_is_yuv(pal)) return FALSE;
  if ((out->flags & NODEFLAG_PRESERVE_ALPHA) && !weed_palette_has_alpha(pal)) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_PACKED) && weed_palette_get_nplanes(pal) > 1) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_PLANAR) && weed_palette_get_nplanes(pal) == 1) return FALSE;
  return TRUE;
}


static LiVESList *_add_cdeltas(LiVESList * cdeltas, int out_pal, int in_pal,
			       double * costs,  int sort_by, boolean replace,
			       int out_gamma_type, int in_gamma_type) {
  // update cdeltas for an input
  // if replace is set we find an old entry to remove and re-add
  // sort_by is the cost type to order the cdeltas

  LiVESList *list = NULL;
  cost_delta_t *cdelta = NULL;

  if (cdeltas) list = cdeltas;

  if (replace) {
    for (; list; list = list->next) {
      cdelta = (cost_delta_t *)list->data;
      if (cdelta->out_pal == out_pal && cdelta->in_pal == in_pal
	  && cdelta->out_gamma_type == out_gamma_type
	  && cdelta->in_gamma_type == in_gamma_type) break;
    }
    if (list) {
      // remove data so we can re-add it sorted
      if (list != cdeltas) list->prev->next = list->next;
      else cdeltas = list->next;
      if (list->next) list->next->prev = list->prev;
      list->next = list->prev = NULL;
      cdelta = (cost_delta_t *)list->data;
      list->data = NULL;
      lives_list_free_1(list);
    }
  }

  if (!cdelta) cdelta = (cost_delta_t *)lives_calloc(1, sizeof(cost_delta_t));
  cdelta->out_pal = out_pal;
  cdelta->in_pal = in_pal;

  lives_memcpy(cdelta->deltas, costs, N_COST_TYPES * sizdbl);

  cdelta->out_gamma_type = out_gamma_type;
  cdelta->in_gamma_type = in_gamma_type;

  cdeltas = lives_list_prepend(cdeltas, (void *)cdelta);

  return cdeltas;
}


static void calc_costs_for_source(lives_nodemodel_t *nodemodel, inst_node_t *n, int track, double * factors,
				  boolean ghost) {
  // for sources, the costs depend on the model_type;
  // the costs are the same for each output
  //
  // if we have NODE_MODELS_SRC, output is NODESRC_ANY_PALETTE so the only cost is
  // proc_time to produce the frame
  //

  // there is no time cost to switch palettes, however this may affect proc_time
  // and in addition we will add a qloss_p ghost cost to represent visual changes,
  // the cost being equivalent to the cost converting from cpal to pal, or 0.5 * cost for pal to cpal
  // whichever is the greater

  // for NODE_MODELS_CLIP we have a set of all possible palettes and various clip_srcs
  // we want to set the "apparent_palette" - for the srcgrp,
  // all clip_srcs in the srcgrp will first convert to this
  // then to the following palette. This is equivalent to the node palette.
  // we want to find this apparent palette by minimiseing the cost to convert clip_srcs to it combined with the
  // cost from a. pal. to next input pal.

  // if track_sources are not set up, then we use the primary_srcgroup, if it is then we can link each track to a
  // specific srcgrp within a lives_clip_t

  // for the region betweemn start and end we can count the number of frames for each clip_src
  // then this acts like a type of fractional input

  //  --- for example we may have img src - palette RGBA32
  // -- decoder src - palette yuv420p
  // if we have 20% decoded frames and 80% undecodec then we calculate costs for RGBA32 and for YUV420P and

  // multiply the former by .2 and the later by .8 to give the calulated cost
  // we will calculate this for all possible palettes. These cost are se tin cdeltas for the output, with
  // out_pal == WEED_PALETTE_NULL, and in_pal taking the palette value
  //
  // Then when ascending we know best pal at the input, and we select the apparent pal by combining
  // cdeltas from the ouput with the conversion costs to the input.
  lives_clip_t *sfile;
  lives_clipsrc_group_t *srcgrp;
  double *costs;
  int i, j, k, ni, no;

  /* if (n->model_type == NODE_MODELS_GENERATOR) */
  /*   costs = (double *)lives_calloc(N_COST_TYPES, sizdbl); */
  /* if (ghost && cpal != WEED_PALETTE_NONE) { */
  /*   int npals, *pals, inpals, *ipals; */
  /*   for (no = 0; no < n->n_outputs; no++) { */
  /*     output_node_t *out = n->outputs[no]; */
  /*     if (out->npals) { */
  /* 	npals = out->npals; */
  /* 	pals = out->pals; */
  /*     } */
  /*     else { */
  /* 	npals = n->npals; */
  /* 	pals = n->pals; */
  /*     } */
  /*     for (j = 0; j < npals; j++) { */
  /* 	// we only have a ghost cost from switching source output palettes */
  /* 	// (and gamma - TODO) */
  /* 	double gcost; */
  /* 	if (npals[j] != cpal) { */
  /* 	  double delta_costf = get_pconv_cost(COST_TYPE_QLOSS_P, owidth, oheight, cpal, */
  /* 					      npals[j], allpals); */
  /* 	  double delta_costb = get_pconv_cost(COST_TYPE_QLOSS_P, owidth, oheight, npals[j], */
  /* 					      cpal, xpals) * .5; */
  /* 	  if (delta_costf > delta_costb) gcost = delta_costf; */
  /* 	  else gcost = delta_costb; */
  /* 	} */
  /* 	costs[COST_TYPE_QLOSS_P] = gcost; */
  /* 	costs[COST_TYPE_COMBINED] = factors[COST_TYPE_QLOSS_P] * gcost; */
  /* 	out->cdeltas = _add_cdeltas(out->cdeltas, WEED_PALETTE_NONE, j, costs, */
  /* 					   COST_TYPE_COMBINED, FALSE, n->gamma_type, n->gamma_type); */

  /*     } */
  /*   } */
  /* } */

  if (n->model_type != NODE_MODELS_CLIP) return;

  costs = (double *)lives_calloc(N_COST_TYPES, sizdbl);

  // in the model, clip sources are represented by non enumerated inputs
  // we will step through each of these and calulate its costs.
  // Then multiply the values by the relative frequency
  // and sum all of these for all clip_srcs
  // we will do this for every possible palette
  // then later when finding best palettes we will have the best_in_pal for the input it connects to
  // normally there is no cost for not converting a palette, so we would usually set out_pal == in_pal
  // however in this case we must take into account the cost to convert to the apparent_palette and
  // total this with the cost to the input palette

  sfile = (lives_clip_t *)n->model_for;

  srcgrp = mainw->track_sources[track];
  if (!srcgrp) {
    int clipno = nodemodel->clip_index[track];
    srcgrp = get_primary_srcgrp(clipno);
  }

  for (j = 0; j < n_allpals; j++) {
    // step through list of ALL palettes
    double ccost = 0;
    for (ni = 0; ni < n->n_clip_srcs; ni++) {
      double gcost = 0.;
      input_node_t *in = n->inputs[ni];
      int cpal = WEED_PALETTE_NONE;
      int owidth = in->width;
      int oheight = in->height;
      if (srcgrp) cpal = srcgrp->apparent_pal;
      if (!owidth || !oheight) {
	owidth = sfile->hsize;
	oheight = sfile->vsize;
      }

      if (srcgrp && ghost && cpal != WEED_PALETTE_NONE && allpals[j] != cpal) {
	// TODO - we should also store cpal for each clipsrc, and then multiply
	int xpals[2] = {cpal, WEED_PALETTE_END};
	double delta_costf = get_pconv_cost(COST_TYPE_QLOSS_P, owidth, oheight, cpal, allpals[j], allpals);
	double delta_costb = get_pconv_cost(COST_TYPE_QLOSS_P, owidth, oheight, allpals[j], cpal, xpals) * .5;
	if (delta_costf > delta_costb) gcost = delta_costf;
	else gcost = delta_costb;
      }
      // combo with all out_pals for clipsrc
      // generally we only have a single palette choice for clip_srcs
      for (i = 0; i < in->npals; i++) {
	// find costs to convert from width, height, ipal gamma_type to
	// owidth, oheight, opal, ogamma_type
	for (k = 0; k < N_REAL_COST_TYPES; k++) {
	  double cost;
	  if (k == COST_TYPE_COMBINED) continue;
	  // we dont know the out_size, so we assume the sfile size
	  // or in gamma type
	  cost = get_conversion_cost(k, owidth, oheight, owidth, oheight, FALSE,
				     in->pals[i], allpals[j], allpals, n->gamma_type, n->gamma_type, ghost);
	  if (k == COST_TYPE_QLOSS_P) {
	    // we will only have cpal if we have fully defined track_sources
	    // i.e. if map_sources_to_tracks was called with the current clip_index
	    cost += gcost - cost * gcost;
	    // TODO - do same for clipsrc cpal ->
	  }

	  if (!i || cost < in->min_cost[j * N_REAL_COST_TYPES + k]) {
	    in->min_cost[j * N_REAL_COST_TYPES + k] = cost;
	    in->best_src_pal[j * N_REAL_COST_TYPES + k] = i;
	  }
	  ccost += cost * factors[k];
	}
	if (!i || ccost < in->min_cost[j * N_REAL_COST_TYPES + COST_TYPE_COMBINED]) {
	  in->min_cost[j * N_REAL_COST_TYPES + COST_TYPE_COMBINED] = ccost;
	  in->best_src_pal[j * N_REAL_COST_TYPES + COST_TYPE_COMBINED] = i;
	}
      }
      for (k = 0; k < N_REAL_COST_TYPES; k++) {
	costs[k] += in->min_cost[j * N_REAL_COST_TYPES + k] * in->f_ratio;
      }
    }
    // we now have costs for each cost type for this palette j
    for (no = 0; no < n->n_outputs; no++) {
      output_node_t *out = n->outputs[no];
      if (ghost)
	out->cdeltas = _add_cdeltas(out->cdeltas, WEED_PALETTE_NONE, j, costs, COST_TYPE_COMBINED, FALSE,
				    n->gamma_type, n->gamma_type);
      else
	out->true_cdeltas = _add_cdeltas(out->true_cdeltas, WEED_PALETTE_NONE, j, costs, COST_TYPE_COMBINED, FALSE,
					 n->gamma_type, n->gamma_type);
    }
  }
  lives_free(costs);
}


static cost_delta_t *find_cdelta(LiVESList * cdeltas, int out_pal, int in_pal, int out_gamma_type, int in_gamma_type) {
  cost_delta_t *cdelta = NULL;
  for (LiVESList *list = cdeltas; list; list = list->next) {
    cdelta = (cost_delta_t *)list->data;
    if ((out_pal == WEED_PALETTE_ANY || cdelta->out_pal == out_pal)
	&& (in_pal == WEED_PALETTE_ANY || cdelta->in_pal == in_pal)) {
      // TODO - check gammas
      break;
    }
  }
  return cdelta;
}


#define _FLG_GHOST_COSTS	1
#define _FLG_ORD_DESC		2

#define _FLG_ENA_GAMMA		16

static void _calc_costs_for_input(lives_nodemodel_t *nodemodel, inst_node_t *n, int idx,
				  int *pal_list, int j, boolean flags,
				  double * glob_costs, boolean * glob_mask, double * factors) {
  // calc costs for in converting to pal_list[j]
  weed_filter_t *filter = NULL, *prev_filter = NULL;
  LiVESList *ocdeltas;
  input_node_t *in = n->inputs[idx];
  inst_node_t *p = in->node;
  output_node_t *out = p->outputs[in->oidx];
  double *costs, *srccosts = NULL;
  int npals, *pals;
  boolean clone_added = FALSE;
  int owidth, oheight, iwidth, iheight;
  int out_gamma_type = WEED_GAMMA_UNKNOWN, in_gamma_type = WEED_GAMMA_UNKNOWN;

  boolean letterbox = FALSE, ghost = FALSE;

  iwidth = in->inner_width;
  iheight = in->inner_height;

  if (iwidth < in->width || iheight < in->height)
    letterbox = TRUE;

  if (n->model_type == NODE_MODELS_FILTER)
    filter = (weed_filter_t *)n->model_for;
  if (p->model_type == NODE_MODELS_FILTER)
    prev_filter = (weed_filter_t *)p->model_for;

  // if p is an input, we dont really have palettes, we have composit clip srcs

  if (flags & _FLG_GHOST_COSTS) ghost = TRUE;

  if (ghost) ocdeltas = out->cdeltas;
  else ocdeltas = out->true_cdeltas;

  if (!p->n_inputs && !ocdeltas)
    calc_costs_for_source(nodemodel, p, in->track, factors, ghost);

  //if (flags & _FLG_ENA_GAMMA) enable_gamma = TRUE;
  if (out->npals) {
    npals = out->npals;
    pals = out->pals;
  } else {
    npals = p->npals;
    pals = p->pals;
  }

  // test with gamma conversions applied; when optimising we will test the effects of disabling this
  in_gamma_type = n->gamma_type;
  out_gamma_type = p->gamma_type;

  owidth = out->width;
  oheight = out->height;

  costs = (double *)lives_calloc(N_COST_TYPES, sizdbl);

  for (int i = 0; i < npals; i++) {
    //int out_gamma_type = in_gamma_type;
    double ccost = 0.;
    int igamma_type = in_gamma_type;

    // apply node restrictions
    if ((!out->npals && !pal_permitted(p, pals[i]))
	|| (out->npals && !output_pal_permitted(p, in->oidx, pals[i]))) continue;

    if (out_gamma_type == WEED_GAMMA_LINEAR && !weed_palette_is_rgb(pal_list[j])
	&& !weed_palette_is_rgb(pals[i]) && in_gamma_type != WEED_GAMMA_LINEAR)
      igamma_type = WEED_GAMMA_LINEAR;

    if (!p->n_inputs && p->model_type != NODE_MODELS_GENERATOR) {
      cost_delta_t *xcdelta;
      if (ghost)
	xcdelta = find_cdelta(out->cdeltas, WEED_PALETTE_ANY, pals[i], WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
      else
	xcdelta = find_cdelta(out->true_cdeltas, WEED_PALETTE_ANY, pals[i], WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
      if (xcdelta) srccosts = xcdelta->deltas;
    }

    // find cost_delta. This is computed according to
    // cost_type, in_pal, out_pal, out_pal_list, in size and out size

    for (int k = 0; k < N_REAL_COST_TYPES; k++) {
      double delta_cost = 0.;
      if (glob_mask[k]) costs[k] = glob_costs[k];
      else {
	if (k == COST_TYPE_COMBINED) continue;

	delta_cost += get_conversion_cost(k, owidth, oheight, iwidth, iheight,
					  letterbox, pals[i], pal_list[j], pal_list,
					  out_gamma_type, igamma_type, ghost);

	// if we are adding ghost costs, include processing time for prev node ussing our pal
	// current node using in pal. These are not actual costs as the proc_time just adjsuts the
	// start time, but we consider them when optimising
	// if the connected output or input has its own palette list,
	// then we cannot determine the palette dependent processing times

	if (ghost && k == COST_TYPE_TIME) {
	  if (prev_filter && !(p->flags & NODESRC_ANY_SIZE) && !out->npals)
	    delta_cost += get_proc_cost(COST_TYPE_TIME, prev_filter, owidth, oheight, pals[i]);
	  if (filter && !(n->flags & NODESRC_ANY_SIZE) && !in->npals)
	    delta_cost += get_proc_cost(COST_TYPE_TIME, filter, in->width, in->height, pal_list[j]);
	}

	// TODO: misc_costs

	if (srccosts) {
	  // if we are pulling from a clip src, we triangluate from clip_srcs to apparent_pal to in_pal
	  // and we will join both cost
	  if (k == COST_TYPE_QLOSS_P)
	    delta_cost += srccosts[k] - delta_cost * srccosts[k];
	  else delta_cost += srccosts[k];
	}
	costs[k] = delta_cost;
      }
      ccost += factors[k] * costs[k];
    }

    // if we have clones of this input, then we get an extra time cost, a layer_copy cost
    // this is added, then all costs are copied to the clones
    for (int z = idx + 1; z < n->n_inputs; z++) {
      input_node_t *xin = n->inputs[z];
      if ((xin->flags & NODEFLAG_IO_CLONE) && xin->origin == idx) {
	// if we have clone inputs, these add an extra tcost for layer_copy
	// initially we assume all copies are done in parallel
	// - when we have made the plan and done resource checking, we may find not enough
	// threads and these may be moved to sequential
	// for clone inputs, the only cost that matters is tcost, this is the conversion cost
	// for the origin plus a layer copy cost
	// if we have multiple clones, it may be incorrects to assume that we copy in parallel
	if (!clone_added) {
	  double xtra = get_layer_copy_cost(COST_TYPE_TIME, in->width, in->height, pal_list[j]);
	  costs[COST_TYPE_TIME] += xtra;
	  ccost += factors[COST_TYPE_TIME] * xtra;
	  clone_added = TRUE;
	}
	if (ghost)
	  xin->cdeltas = _add_cdeltas(xin->cdeltas, j, i, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
				      igamma_type);
	else
	  xin->true_cdeltas = _add_cdeltas(xin->true_cdeltas, j, i, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
					   igamma_type);

      }
    }

    // combined cost here is just an intial estimate, since we don't yet have absolute / max tcost
    costs[COST_TYPE_COMBINED] = ccost;
    // add to cdeltas for input, sorted by increasing ccost

    if (ghost)
      in->cdeltas = _add_cdeltas(in->cdeltas, i, j, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
				 igamma_type);
    else
      in->true_cdeltas = _add_cdeltas(in->true_cdeltas, i, j, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
				      igamma_type);
    //d_print_debug("ADD2 cdeltas for %d to %d\n", i, j);
  }

  lives_free(costs);
}


static boolean adjust_apparent_pal(inst_node_t *n) {
  int optimal_pal = 0;
  if (!n || n->n_inputs || !n->n_outputs || !n->n_clip_srcs) return FALSE;
  output_node_t *out = n->outputs[0];
  input_node_t *in = out->node->inputs[out->iidx];
  if (in->npals) n->optimal_pal = in->optimal_pal;
  else n->optimal_pal = out->node->optimal_pal;
  if (optimal_pal != n->optimal_pal) {
    n->optimal_pal = optimal_pal;
    if (weed_palette_is_yuv(n->optimal_pal)) {
      if (n->gamma_type == WEED_GAMMA_UNKNOWN || n->gamma_type == WEED_GAMMA_LINEAR
	  || n->gamma_type == WEED_GAMMA_MONITOR) n->gamma_type = WEED_GAMMA_SRGB;
    }
    return TRUE;
  }
  return FALSE;
}


static void compute_all_costs(lives_nodemodel_t *nodemodel, inst_node_t *n, int ord_ctype, double * factors, int flags) {
  // given a node n, we compute costs for all in / out pairs and add these to in->cdeltas for each input
  // the entries will be ordered by ascending cost of the specifies type
  // setting ORD_DESC will reverse the cdeltas list ordering
  // GHOST_COSTS will add "fake" costs which are transitory costs only applied when changin palette
  //  such costs are ignored when computing "true" costs
  // setting ENA_GAMMA will force linear gamma if the node prefers it
  //   (invalid if palette is yuv, prev palette was also yuv, and prev gamma type was not linear)

  // intially we may find a cdelta for an input with no inpal, and no outpal,
  // - in this case it will hold QLOSS_S calculated in previous phases.
  // We need to thus find an existing cdalta with QLOSS_S set and hold this in a global cdelta for the input
  // then using a kind of "mask" with just QLOSS_S set, copy the global masked values to each othe cdelta

  input_node_t *in;

  double glob_costs[N_COST_TYPES];
  boolean glob_mask[N_COST_TYPES];

  int ni, j, k;
  int npals, *pal_list;

  // costs for srcs are calulated at the following node
  if (!n->inputs) return;

  for (ni = 0; ni < n->n_inputs; ni++) {
    // we will go through each input separately, in case any has a private palette list,
    // then after that for the node itself, which will take care of the majority of inputs, which
    // use the instance palette
    in = n->inputs[ni];

    // skip CLONES here - they DO contribute but only when calulating tmax
    // and we calulate this when we check the origin input
    if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;

    // palettes for input can be global or specific
    npals = in->npals;
    if (npals) pal_list = in->pals;
    else {
      npals = n->npals;
      pal_list = n->pals;
    }

    for (k = 0; k < N_COST_TYPES; k++) glob_mask[k] = FALSE;

    if (in->cdeltas) {
      // qloss for size changes is pre-calulated and we set it in the single cdelta node
      // we take the first cdelta node, and we want to copy this cost to all other cdeltas
      // we set it to true in glob_mask, the value is then copied rather than being reset to zeor
      cost_delta_t *cdelta = (cost_delta_t *)in->cdeltas->data;
      for (k = 0; k < N_COST_TYPES; k++) glob_costs[k] = cdelta->deltas[k];
      glob_mask[COST_TYPE_QLOSS_S] = TRUE;
    }

    // iterate over all out pals, either for the node or for a specific input
    // cross reference this against the in pals available from the previous node output
    for (j = 0; j < npals; j++) {
      // apply node restrictions
      if (in && !input_pal_permitted(n, ni, pal_list[j])) continue;
      if (!in && !pal_permitted(n, pal_list[j])) continue;

      // go through all inputs and calculate costs for each in_pal / out_pal pair
      // storing this in cdeltas for the input_node
      // we find the output_node which the input connects to, and if it has npals > 0
      // we use the palette list from the output, otherwise we use the list from the node
      // we optimise independently for each output, if this leads to conflicting palettes in prv node
      // then that will be resolved once we are able to calulate combined cost at the sink

      // make cdeltas for each in / out pair

      // calc for in - with pal pal_list[j] - over all in pals, - if glob_mask[i] is TRUE set val to glob_costs[k]
      // factors are used to compute combined_cost
      _calc_costs_for_input(nodemodel, n, ni, pal_list, j, flags, glob_costs, glob_mask, factors);
      // *INDENT-OFF*
    }}
  // *INDENT-ON*
}


static void free_tuple(cost_tuple_t *tup) {
  if (tup) {
    if (tup->palconv) lives_free(tup->palconv);
    lives_free(tup);
  }
}

static cost_tuple_t *copy_tuple(cost_tuple_t *tup, int nins) {
  cost_tuple_t *newtup = NULL;
  if (tup) {
    newtup = (cost_tuple_t *)lives_calloc(1, sizeof(cost_tuple_t));
    lives_memcpy(newtup, tup, sizeof(cost_tuple_t));
    newtup->palconv = (conversion_t *)lives_calloc(nins, sizeof(conversion_t));
    lives_memcpy(newtup->palconv, tup->palconv, nins * sizeof(conversion_t));
  }
  return newtup;
}


#define SUM_MAX		1
#define SUM_MPY		2
#define SUM_LN_ADD	3

// we can also use this to combine any pair of costs
// when summing combined costs, we cannot just use C_tot = C_a + C_delta
// since C_a == f0 . tcost_a + f1 . qloss_a
// and C_delta == f0 . tcost_delta + f1 . qloss_delta

// adding we would get f0 . (tcost_a + tcost_delta) + f1 . (qloss_a + qloss_delta)
// this is wrong as we need qloss_a * qloss_delta
// so C_delta == f0. t_delta + f1 . q_delta
// unless we know eithr tcos or qloss, we cannot find the total
// thus whwn storing costs, we also need to store the componets costs
static double *total_costs(inst_node_t *n, int nvals, double **abs_costs, double **costs,
			   double * factors, double * out_costs) {
  // here we sum costs for a set of inputs, to give the value for a node (set in out_costs)
  // abs_costs are the value from the prveious node
  // for tcost, we take the max value, for qloss we multiply
  // then for combined we multiply by the factors and add
  double max[N_REAL_COST_TYPES];

  if (!sumty) {
    // set summation methods and def values for cost types
    sumty = TRUE;
    cost_summation[COST_TYPE_TIME] = SUM_MAX;
    cost_summation[COST_TYPE_QLOSS_P] = SUM_MPY;
    cost_summation[COST_TYPE_QLOSS_S] = SUM_LN_ADD;
  }

  for (int k = 0; k < N_REAL_COST_TYPES; k++) {
    out_costs[k] = 0.;
    if (k == COST_TYPE_COMBINED) continue;
    for (int ni = 0; ni < nvals; ni++) {
      input_node_t *in = n->inputs[ni];
      if (in->flags & NODEFLAGS_IO_SKIP) continue;

      if (k != COST_TYPE_TIME && in->flags & NODEFLAG_IO_CLONE) continue;

      if (cost_summation[k] & SUM_MAX) {
	double cost = abs_costs[ni][k] + costs[ni][k];
	if (!ni || cost > max[k]) {
	  out_costs[k] = max[k] = cost;
	}
      }
      if (cost_summation[k] == SUM_LN_ADD)
	out_costs[k] += abs_costs[ni][k] + log(abs_costs[ni][k]) + costs[ni][k];

      if (cost_summation[k] & SUM_MPY) {
	// want here; 1 - (1 - a) * (1 - b) == 1 - (1 - b - a + ab) == a + b - ab
	// eg a = .1, b = .15,  1 - .9 * 85 == .9 + .8 - .9 * .8
	out_costs[k] += abs_costs[ni][k] - out_costs[k] * abs_costs[ni][k];
	out_costs[k] += costs[ni][k] - out_costs[k] * costs[ni][k];
      }
    }
  }

  for (int k = 0; k < N_REAL_COST_TYPES; k++) {
    if (k == COST_TYPE_COMBINED) continue;
    out_costs[COST_TYPE_COMBINED] += factors[k] * out_costs[k];
  }

  return out_costs;
}


static cost_tuple_t *make_tuple(inst_node_t *n, conversion_t *conv, double * factors) {
  // input is an inst_node, an array (one per input) of conversions
  // (each with out pal and in pal)
  // for each input we fin cdelta with matching palette out/in and use the costs to create an array
  // the node total is derived and stored in the cost tuple (in future also matching gamma)

  cost_tuple_t *tup = (cost_tuple_t *)lives_calloc(1, sizeof(cost_tuple_t));
  double **costs, **oabs_costs;
  cost_delta_t *cdelta;
  if (!n->n_inputs) return NULL;

  costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
  oabs_costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));

  //d_print_debug("goint to calloc %d X %ld blocks\n", n->n_inputs, sizeof(conversion_t));
  tup->palconv = (conversion_t *)lives_calloc(n->n_inputs, sizeof(conversion_t));

  tup->node = n;
  lives_memcpy(tup->palconv, conv, n->n_inputs * sizeof(conversion_t));

  for (int ni = 0; ni < n->n_inputs; ni++) {
    int ipal, opal;
    input_node_t *in = n->inputs[ni];
    inst_node_t *p;
    if (in->flags & NODEFLAGS_IO_SKIP) continue;

    opal = conv[ni].out_pal;
    ipal = conv[ni].in_pal;
    //cdelta = find_cdelta(in->cdeltas, opal, ipal, conv->out_gamma, conv->in_gamma);
    cdelta = find_cdelta(in->cdeltas, opal, ipal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
    if (!cdelta) {
      lives_abort("No cost deltas found for nodemodel input");
    }
    costs[ni] = cdelta->deltas;
    p = in->node;
    oabs_costs[ni] = p->abs_cost;
    //d_print_debug("in make tuple, add %d %d %d\n", ni, opal, ipal);
  }

  total_costs(n, n->n_inputs, oabs_costs, costs, factors, tup->tot_cost);
  lives_free(oabs_costs); lives_free(costs);
  return tup;
}


static cost_tuple_t **backtrack(inst_node_t *n, int ni, double * factors) {
  // function is called recursively - first time with ni == 0
  // then from within the function we increment ni and call it again recursively
  // after going through all inputs, we produce an ouptut tuple and return one level
  // at each level we go through all palettes and descend
  // thus eventually we will test all combinations
  // The output tuple is formed by totalling the costs from each input, with the result being stored in the tuple itself
  // Thus we can at a later point, find the tuple with least overall cost for any cost type. The in_pal is usually
  // restricted (being the best out_pal from the first output), and searching for a tuple with matching in_pal
  // and cost_type thus indicates the best out_pal to be received for each input.
  //
  // when this is called initially we do not yet have absolute costs for each node, so we fisrts minimise the deltas
  // on the second pass, the deltas have been used to assign absolute costs starting from sources
  // Thus we can add or multiply the deltas by the absolute costs and obtain more accurate values.
  // In theory we should repeat this cycle several time, each time updating absolute costs, finding new detlas
  // updating absolutes and so on until there are no changes to the out / in palettes.
  // However there are issues with this - we could end up never finding an optimal solution, but simply flipping between
  // soulitions, and in addition there will be an optimisation stage where we may consider combinations other than the
  // minimal for a particular node, hence this is done once only.

  static cost_tuple_t **best_tuples;
  static conversion_t *conv;
  static double mincost[N_REAL_COST_TYPES];
  static int k;

  conversion_t *myconv;
  inst_node_t *p;
  input_node_t *in, *xin;
  output_node_t *out;
  int npals, *pals, onpals, *opals, i, xni;

  if (!n->n_inputs) return NULL;

  if (!ni) {
    // reset first time this is called
    conv = (conversion_t *)lives_calloc(n->n_inputs, sizeof(conversion_t));
    best_tuples = (cost_tuple_t **)lives_calloc(N_REAL_COST_TYPES, sizeof(cost_tuple_t *));
    for (i = 0; i < N_REAL_COST_TYPES; i++) mincost[i] = -1.;
    k = 0;
  }

  if (ni >= n->n_inputs) {
    // we set values for all inputs, now we produce the tuple from the set of combinations
    cost_tuple_t *tup = make_tuple(n, conv, factors);

    if (mincost[k] == -1. || tup->tot_cost[k] < mincost[k]) {
      mincost[k] = tup->tot_cost[k];
      if (best_tuples[k]) free_tuple(best_tuples[k]);
      best_tuples[k] = copy_tuple(tup, n->n_inputs);
    }

    free_tuple(tup);
    return best_tuples;
  }

  // this is done for each input n->inputs[ni]

  myconv = &conv[ni];

  in = n->inputs[ni];
  p = in->node;
  out = p->outputs[in->oidx];

  myconv->out_width = out->width;
  myconv->out_height = out->height;

  myconv->in_width = in->width;
  myconv->in_height = in->height;

  myconv->lb_width = in->inner_width;
  myconv->lb_height = in->inner_height;

  myconv->out_gamma = p->gamma_type;
  myconv->in_gamma = n->gamma_type;

  for (xni = ni + 1; xni < n->n_inputs; xni++) {
    xin = n->inputs[xni];
    if (!(xin->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE))) break;
  }

  while (k < N_REAL_COST_TYPES) {
    if (out->npals) {
      // if out palettes can vary, the palette is usually set by the host
      // or by the filter, however we can still calculate it
      onpals = out->npals;
      opals = out->pals;
    } else {
      if (p->best_pal_up[k] == WEED_PALETTE_ANY ||
	  p->best_pal_up[k] == WEED_PALETTE_NONE) {
	onpals = p->npals;
	opals = p->pals;
      } else {
	onpals = 1;
	opals = &p->pals[p->best_pal_up[k]];
      }
    }

    if (!onpals) {
      BREAK_ME("no output pals");
    }

    if (in->npals) {
      npals = in->npals;
      pals = in->pals;
    } else {
      npals = 1;
      pals = &n->pals[in->best_in_pal[k]];
    }

    for (int j = 0; j < npals; j++) {
      int pstart = 0, pend = onpals;

      myconv->in_pal = pals[j];

      // test first, if we have an out / in palette in common
      // this will always be the min cost
      for (int i = 0; i < onpals; i++) {
	if (opals[i] == pals[j]) {
	  pstart = i;
	  pend = i + 1;
	  break;
	}
      }

      for (int i = pstart; i < pend; i++) {
	myconv->out_pal = opals[i];
	if (myconv->in_pal == WEED_PALETTE_NONE || myconv->out_pal == WEED_PALETTE_NONE) {
	  g_print("in pal for %d  %d  %d is %d, out pal i %d\n", ni, i, j, myconv->in_pal, myconv->out_pal);
	  lives_abort("Bad tuple palette");
	}
	backtrack(n, xni, factors);
	// TODO - test with in_gamma / out_gamma combos
      }
    }
    if (ni) return NULL;
    k++;
  }

  lives_free(conv);
  return best_tuples;
}


LIVES_LOCAL_INLINE cost_tuple_t **best_tuples(inst_node_t *n, double * factors) {return backtrack(n, 0, factors);}


static void ascend_tree(inst_node_t *n, int oper, double * factors, int flags) {
  // - factors only fo oper 0, flags - ghost for oper 3
  // for each input, we iterate over cost_types. Since for the current node we already have
  // best_pal[cost_type], we can thus look up the prev node in pal with min cost,
  // and we set best_pal[cost_type] for each input node, ascedning the branches depth first recursively

  // oper can be: 	0 - map lc palettes
  //			1 - accumulate_slack
  // 			2  set_missing_sizes
  //			3 -  consolidate_size_costs

  static int depth = 0;
  static boolean flag_nodisplay = FALSE;
  output_node_t *out;
  input_node_t *in;
  inst_node_t *p;
  double maxqls = 0.;
  double minslack = 0.;
  boolean svary = FALSE;
  boolean ghost = FALSE;
  int width = 0, height = 0;
  int ni, no, k, pal;

  if (!depth && !NODE_IS_SINK(n)) flag_nodisplay = TRUE;

  depth++;

  //d_print_debug("trace to src %d %d\n", depth, n->n_inputs);

  if (oper == 2 && n->flags & NODESRC_ANY_SIZE) svary = TRUE;

  if (flags & _FLG_GHOST_COSTS) ghost = TRUE;

  // in case a node has multiple outputs, we can only optimise for one of these (unless the output has its
  // own palette_list)
  // we will iterate over all inputs
  // we will find the output idx for the prev node feeding to the input
  // if the output has its own palette_list, then we find best_pal for the output
  // if the output does not, then we need to optimise the global palette, to find which of the outputs we
  // should use/ For now we just use the first output to reach here

  // processs outputs
  if (n->n_outputs) {
    boolean is_first = TRUE;
    if (oper == 1 || oper == 3 || oper == 0) {
      for (no = 0; no < n->n_outputs; no++) {
	out = n->outputs[no];
	if (out->flags & NODEFLAGS_IO_SKIP) continue;

	// make sure outputs have been processed
	// for 0 - min pals, we dont need this as only out[0] counts anyway
	// but we check for pvary
	// for 2, set missing sizes, again only out[0] counts
	// but we check for svary
	if (!(out->flags & NODEFLAG_PROCESSED)) return;
      }
    }

    for (no = 0; no < n->n_outputs; no++) {
      out = n->outputs[no];
      out->flags &= ~NODEFLAG_PROCESSED;
      if (flag_nodisplay && !(out->flags & NODEFLAG_OUTPUT_NODISPLAY))
	flag_nodisplay = FALSE;

      // this is input from lower node
      in = out->node->inputs[out->iidx];
      switch (oper) {
      case -1:
      case 0:
	// set optimal pal for node or for output
	// data here is cost_type
	for (k = 0; k < N_REAL_COST_TYPES; k++) {
	  if (in->npals) pal = in->best_out_pal[k];
	  else pal = out->node->best_pal_up[k];
	  out->best_out_pal[k] = pal;
	  if (is_first) n->best_pal_up[k] = pal;
	}
	out->optimal_pal = out->best_out_pal[COST_TYPE_COMBINED];
	if (is_first) {
	  n->optimal_pal = n->best_pal_up[COST_TYPE_COMBINED];
	  if (weed_palette_is_yuv(n->optimal_pal)) {
	    if (n->gamma_type == WEED_GAMMA_UNKNOWN || n->gamma_type == WEED_GAMMA_LINEAR
		|| n->gamma_type == WEED_GAMMA_MONITOR) n->gamma_type = WEED_GAMMA_SRGB;
	  }
	}
	break;

      case 1:
	// find minslack
	if (is_first || n->outputs[no]->tot_slack < minslack)
	  minslack = n->outputs[no]->tot_slack;
	break;

      case 2:
	// foreach output, check if we have a size set, if not we set its size
	// to the size of the input node it connects to
	// we then push this size up inputs
	if (!out->width || !out->height) {
	  if (!in->width) in->width = n->model->opwidth;
	  if (!in->height) in->height = n->model->opheight;
	  out->width = in->width;
	  out->height = in->height;
	  if (n->model_type == NODE_MODELS_GENERATOR
	      || n->model_type == NODE_MODELS_FILTER) {
	    weed_filter_t *filter = (weed_filter_t *)n->model_for;
	    weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, no, NULL, LIVES_OUTPUT);
	    validate_channel_sizes(filter, chantmpl, &out->width, &out->height);
	  }
	  if (!width) {
	    width = out->width;
	    height = out->height;
	  }
#if 0
	  // if we can freely choose out sizes, we chould check if there is a preferred
	  // aspect ratio, and use this
	  if (NODE_IS_SOURCE(p) && !fixed_size
	      && (!(is_gen && prefs->no_lb_gens))) {
	    if (weed_plant_has_leaf(chantmpl, LIVES_LEAF_ASPECT_RATIO)) {
	      // handle preffer ARs
	      int nars;
	      mindelta = -1.;
	      best_ar = 1.;

	      double *ars = weed_get_double_array_counted(emo, &nars);
	      for (i = 0; i < nars; i++) {
		double diag = sqrt(ars[i] * ars[i] + ars[i + 1] * ars[i + 1]);
		if (mindelta < 0. || abs(dig - sdiag) < mindelta) {
		  mindelta = abs(dlg - sdiag);
		  best_ar = ars[i] / ars[i + 1];
		}
	      }
	      lives_free(ars);
	      // having found closest ar to in / out size,
	      // we now need to adjust this for display size
	      //again...
	    }
	  }
#endif
	}
	break;

      case 3:
	if (out->cdeltas) {
	  // find max upscale cost from all outputs
	  // since we only conrinu up after all outputs have been reached, the carried cost is stored
	  // temporarily in the outputs. Now we retrieve that to find max.
	  // set value in MAXQLS
	  cost_delta_t *cdelta = (cost_delta_t *)out->cdeltas->data;
	  double qloss_s = cdelta->deltas[COST_TYPE_QLOSS_S];
	  if (qloss_s > maxqls) maxqls = qloss_s;
	  // we no longer need cdeltas in output
	  lives_list_free_all(&out->cdeltas);
	}
	break;
      default: break;
      }
      if (oper != 0 || !(out->flags & NODEFLAG_OUTPUT_NODISPLAY))
	is_first = FALSE;
    }
  }

  // process inputs

  if (oper > 0) {
    if (n->n_inputs) {
      for (ni = 0; ni < n->n_inputs; ni++) {
	in = n->inputs[ni];

	// again, we ignore cloned inputs, they don't affect the previous palette
	if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;

	p = in->node;
	out = p->outputs[in->oidx];
	switch (oper) {
	case 1: {
	  // find slack and add to minslack
	  cost_delta_t *cdelta;
	  double slack;
	  int out_pal, in_pal;
	  if (in->npals) in_pal = in->pals[in->optimal_pal];
	  else in_pal = n->pals[n->optimal_pal];
	  if (out->npals) out_pal = out->pals[out->optimal_pal];
	  else out_pal = p->pals[p->optimal_pal];
	  cdelta = find_cdelta(in->true_cdeltas, out_pal, in_pal, p->gamma_type, n->gamma_type);
	  slack = (double)(n->ready_ticks - p->ready_ticks) / TICKS_PER_SECOND_DBL
	    - cdelta->deltas[COST_TYPE_TIME];
	  out->slack = slack;
	  out->tot_slack = slack + minslack;
	  // when optimising, we can "spend" slack to increase tcost, without increasing combined
	  // cost at the sink
	}
	  break;

	case 2:
	  if (!in->width || !in->height) {
	    if (n->n_outputs) {
	      if (svary && ni < n->n_outputs) {
		in->width = n->outputs[ni]->width;
		in->height = n->outputs[ni]->height;
	      } else {
		in->width = width;
		in->height = height;
	      }
	      if (n->model_type == NODE_MODELS_GENERATOR
		  || n->model_type == NODE_MODELS_FILTER) {
		weed_filter_t *filter = (weed_filter_t *)n->model_for;
		weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, ni, NULL, LIVES_INPUT);
		validate_channel_sizes(filter, chantmpl, &in->width, &in->height);
	      }
	    }
	  }
	  break;

	case 3: {
	  double xmaxqls = maxqls;
	  int in_size, out_size;
	  in = n->inputs[ni];
	  // again, we ignore cloned inputs, they don't affect the previous palette
	  if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
	  p = in->node;
	  out = p->outputs[in->oidx];
	  // check for downscales / upscales
	  out_size = out->width * out->height;
	  in_size = in->width * in->height;
	  if (out_size > in_size) {
	    // calc cost for downscale, store it temporarily in input
	    double qloss_s = get_resize_cost(COST_TYPE_QLOSS_S, out->width, out->height, in->width, in->height,
					     WEED_PALETTE_NONE, WEED_PALETTE_NONE);
	    if (!ghost && qloss_s > xmaxqls) qloss_s = xmaxqls;
	    if (qloss_s) {
	      double *costs = (double *)lives_calloc(N_COST_TYPES, sizdbl);
	      costs[COST_TYPE_QLOSS_S] = qloss_s;
	      in->cdeltas = _add_cdeltas(in->cdeltas, WEED_PALETTE_NONE, WEED_PALETTE_NONE,
					 costs, COST_TYPE_QLOSS_S, FALSE, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
	      lives_free(costs);
	      xmaxqls -= qloss_s;
	    }
	  } else if (out_size < in_size) {
	    // upscale - add cost to maxqls
	    xmaxqls += get_resize_cost(COST_TYPE_QLOSS_S, out->width, out->height, in->width, in->height,
				       WEED_PALETTE_NONE, WEED_PALETTE_NONE);
	  }
	  if (xmaxqls) {
	    // store maxqls in out cdelta
	    // so we can find max for all outputs
	    double *costs = (double *)lives_calloc(N_COST_TYPES, sizdbl);
	    costs[COST_TYPE_QLOSS_S] = xmaxqls;
	    out->cdeltas = _add_cdeltas(out->cdeltas, WEED_PALETTE_NONE, WEED_PALETTE_NONE,
					costs, COST_TYPE_QLOSS_S,
					FALSE, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
	    lives_free(costs);
	  }
	}
	  break;
	default: break;
	}
	if (flag_nodisplay) out->flags |= NODEFLAG_OUTPUT_NODISPLAY;
	out->flags |= NODEFLAG_PROCESSED;
	ascend_tree(p, oper, factors, flags);
      } // for loop -inputs
    } // has inputs
  } // oper > 0
  else {
    // oper == 0
    // finding best pals part 2
    //
    // having decided which is the best palette for this node
    // now we look for best out_palette coming in to the inputs
    // whilst we could simply pick the lowest cost pal for each input
    // this may not give the lowest total cost at the node
    // for example, the tcost for the node is defined by max absolute time for all inputs
    // plus the product of qlosses, both scaled. Thus if qloss is lower for one combo
    // but increase time cose, it makes no difference to the time cost if tmax stays the same.
    //
    // instead what we do is create tuples for all combos of in / out pals for all inputs
    // then pick the tuple which gives lowest overall cost. Then we use the tuple to
    // set the input palettes prev values in best_out_pal[k] for all inputs

    // Actually we do not check ALL combos - for most inputs we already know the in palette
    // if it has no npals, this is just n->best_pal_up[k]
    // if it has npals we match it with corresponding output
    // - unless it is a converter, and we have svary, or there is no matching out
    // then we use any palette in the input pal_list

    if (!n->n_inputs) {
      // for clips, we take the node pal and set best_src_pals as best out_pals
      // for other src types. setting out pal was enough
      if (n->model_type == NODE_MODELS_CLIP) {
	// when ascending we would have set best_pal_up for the node according to
	// leasst total cost to the input
	// now we want to find for each input (clip_src), which is the best palette for the selected
	// one
	for (ni = 0; ni < n->n_clip_srcs; ni++) {
	  in = n->inputs[ni];
	  if (!in) break;
	  for (int k = 0; k < N_REAL_COST_TYPES; k++) {
	    int tpal = n->best_pal_up[k];
	    in->best_in_pal[k] = tpal;
	    in->best_out_pal[k] = in->best_src_pal[tpal * N_REAL_COST_TYPES + k];
	  }
	  // this is the palette we will use for the clip_src
	  in->optimal_pal = in->best_out_pal[COST_TYPE_COMBINED];
	  n->optimal_pal = n->best_pal_up[COST_TYPE_COMBINED];
	  if (weed_palette_is_yuv(n->optimal_pal)) {
	    if (n->gamma_type == WEED_GAMMA_UNKNOWN || n->gamma_type == WEED_GAMMA_LINEAR
		|| n->gamma_type == WEED_GAMMA_MONITOR) n->gamma_type = WEED_GAMMA_SRGB;
	  }
	}
      }
    } else {
      // has inputs
      boolean is_converter = FALSE;
      if (n->flags & NODESRC_IS_CONVERTER) is_converter = TRUE;

      for (k = 0; k < N_REAL_COST_TYPES; k++) {
	for (ni = 0; ni < n->n_inputs; ni++) {
	  in = n->inputs[ni];

	  // again, we ignore cloned inputs, they don't affect the previous palette
	  if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;

	  in->best_in_pal[k] = WEED_PALETTE_NONE;
	  if (svary && !is_converter && ni < n->n_outputs)
	    in->best_in_pal[k] = n->outputs[ni]->best_out_pal[k];
	  else if (!in->npals) in->best_in_pal[k] = n->best_pal_up[k];

	  //p = in->node;
	  /* if (p->flags & NODEFLAG_LOCKED) { */
	  /*   out = p->outputs[in->oidx]; */
	  /*   if (out->npals) in->best_out_pal[k] = out->cpal; */
	  /*   else in->best_out_pal[k] = p->cpal; */
	  /* } */
	  //else
	  in->best_out_pal[k] = WEED_PALETTE_NONE;
	}

	// find a tuple with min cost for each type
	// given the limitations of best_in_pal[k], best_out_pal[k] already set

	// for the given cost type, we want to find the best combination of in / out palettes
	// for nodes with non variable palettes, all inputs have the same in palette
	// this is out n->best_pal_up set from the node below
	// for other types, one or more inputs can have its own palette list
	// then for each node inputting, the same rule occurs, if it doe not have varaible out palettes
	// then whichever node connects first can set its output palette; the vast majority of node are expected to have
	// just one output, but we can optimise later
	// if an output has variable palettes then we can optmise separately for each output
      }

      cost_tuple_t **tups = best_tuples(n, factors);
      for (k = 0; k < N_REAL_COST_TYPES; k++) {
	//d_print_debug("got best tup %p for ct %d\n", tup, k);
	cost_tuple_t *tup = tups[k];
	for (ni = 0; ni < n->n_inputs; ni++) {
	  in = n->inputs[ni];
	  if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
	  if (in->best_in_pal[k] == WEED_PALETTE_NONE)
	    in->best_in_pal[k] = tup->palconv[ni].in_pal;
	  if (in->best_out_pal[k] == WEED_PALETTE_NONE)
	    in->best_out_pal[k] = tup->palconv[ni].out_pal;
	  p = in->node;
	  if (p->best_pal_up[k] == WEED_PALETTE_NONE) {
	    p->best_pal_up[k] = tup->palconv[ni].out_pal;
	    if (k == COST_TYPE_COMBINED) {
	      p->optimal_pal = p->best_pal_up[k];
	    }
	  }
	  if (k == COST_TYPE_COMBINED && in->npals) in->optimal_pal = in->best_in_pal[k];
	}
	free_tuple(tup);
      }
      if (tups) lives_free(tups);
      // endif - has inputs
    }
    for (ni = 0; ni < n->n_inputs; ni++) {
      in = n->inputs[ni];
      // again, we ignore cloned inputs, they don't affect the previous palette
      if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
      p = in->node;
      out = p->outputs[in->oidx];
      if (flag_nodisplay) out->flags |= NODEFLAG_OUTPUT_NODISPLAY;
      out->flags |= NODEFLAG_PROCESSED;
      ascend_tree(p, oper, factors, flags);
    }
    // endif - oper == 0
  }

  depth--;
}


static void map_least_cost_palettes(inst_node_t *n, double * factors) {
  // n will always be a sink node with no outputs
  // - begin by examining the min_cost for each palette, / cost_type
  // the minimum for each cost_type is found, and the palette associated
  // becomes best_pal[cost_type]

  // however if a node has multiple inputs, then we may not know the min costs overall
  // if pick a palette, we can find min costs for each input, we can be sure for qloss this will give min
  // but for tcost we push back the input node, but we do not yet know the ready time for the node,
  // if it is a source it will get pushed back to 0 time, and if the next node up has multiple outputs
  // we may also have slack
  // however, without setting palettes, we cannot calulate absolute time. Furthermore, each iput can have a preferred
  // out palette depending on which palettes are available at the previous nodes.
  //
  // so really we should pick the out palette which gives lowest total costs, for qloss we can find this
  // for tcost we can pick the out palette with lowest max, later we will add slack and we can optimise this
  // so we will go through all out pals, and all in pals, using backtrack, and make tuples
  // then for each cost, pick a tuple with lowest cost value

  // now we know which sink palettes are best,
  // we trace the input_nodes, find the palette which contributed to this
  // by looking at input->prev_pal[out_pal][cost_type]
  // this then becomes best_pal[cost_type] for the previous nodes and we ascend
  // depth first until we reach a node with no inputs (src)
  ascend_tree(n, 0, factors, 0);
}


static void accumulate_slack(inst_node_t *n) {
  ascend_tree(n, 1, NULL, 0);
}


static void set_missing_sizes(inst_node_t *n) {
  ascend_tree(n, 2, NULL, 0);
}


static void consolidate_size_costs(inst_node_t *n, int flags) {
  ascend_tree(n, 3, NULL, flags);
}


// so this is still a WIP
// the idea is that we would optimise the node model, firstly by targetting large cost deltas to try to reduc them
// then secondly by adding "Creative" changes
// another thing that could be done is to try to combine io heavy operations (e.g pulling from a local source) with
// COU intensive operations (fx processing
// and memory intensive (e.g pal conversions)
// if we measure a bottleneck in on of these we can trya djusting itmings
/* static void optimise_node(inst_node_t *n, double * factors) { */
/* int nins = n->n_inputs; */
/* if (nins) { */
/* int *opals = lives_calloc(nins, sizint); */
/* boolean *used = (boolean *)lives_calloc(nins, sizint); */
//backtrack(n, 0, factors);
/* lives_free(popals); */
/* lives_free(used); */
//}
// TODO
//}


static inst_node_t *desc_and_do_something(int do_what, inst_node_t *n, inst_node_t *p,
					  void *data, int flags, double * factors);

static inst_node_t *desc_and_compute(inst_node_t *n, lives_nodemodel_t *nodemodel,
				     int flags, double * factors) {
  return desc_and_do_something(0, n, NULL, nodemodel, flags, factors);
}

static inst_node_t *desc_and_set_sizes(inst_node_t *n, lives_nodemodel_t *nodemodel) {
  return desc_and_do_something(1, n, NULL, nodemodel, 0, NULL);
}

static inst_node_t *desc_and_total(inst_node_t *n, int flags, double * factors) {
  return desc_and_do_something(4, n, NULL, NULL, flags, factors);
}

static inst_node_t *desc_and_reinit(inst_node_t *n) {
  return desc_and_do_something(5, n, NULL, NULL, 0, NULL);
}

static inst_node_t *desc_and_add_steps(inst_node_t *n, exec_plan_t *plan) {
  return desc_and_do_something(6, n, NULL, plan, 0, NULL);
}

static inst_node_t *desc_and_align(inst_node_t *n, lives_nodemodel_t *nodemodel) {
  return desc_and_do_something(7, n, NULL, (void *)nodemodel, 0, NULL);
}

static inst_node_t *desc_and_do_something(int do_what, inst_node_t *n, inst_node_t *p,
					  void *data, int flags, double * factors) {
  input_node_t *in;
  int i, nfails = 0;

  if (!n) return NULL;

  // if we find any other inputs, just return, eventually these iwll be processed
  // and we will contnue down outputs
  for (i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
    if (!(n->inputs[i]->flags & NODEFLAG_PROCESSED)) return NULL;
  }
  // reset flags
  for (i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
    n->inputs[i]->flags &= ~NODEFLAG_PROCESSED;
  }

  if (do_what < 5 && n->n_inputs) {
    if (!(n->flags & NODEFLAG_PROCESSED)) {
      if (do_what == 0) {
	// calculate min costs for each pal / cost_type
	lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)data;
	if (n->n_inputs) {
	  d_print_debug("%s @ %s\n", "CAC begin", lives_format_timing_string(lives_get_session_time() - ztime));
	  compute_all_costs(nodemodel, n, COST_TYPE_COMBINED, factors, flags);
	  d_print_debug("%s @ %s\n", "CAC end", lives_format_timing_string(lives_get_session_time() - ztime));
	}
      } else if (do_what == 1) {
	// set init sizes
	lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)data;
	if (n->n_inputs) calc_node_sizes(nodemodel, n);
      } else if (do_what == 4) {
	// set abs_cost for nodes
	if (n->n_inputs) {
	  double **costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
	  double **oabs_costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
	  for (int ni = 0; ni < n->n_inputs; ni++) {
	    output_node_t *out;
	    cost_delta_t *cdelta;
	    int in_pal, out_pal;
	    in = n->inputs[ni];
	    if (in->flags & NODEFLAGS_IO_SKIP) continue;

	    p = in->node;
	    out = p->outputs[in->oidx];
	    if (out->npals) out_pal = out->pals[out->optimal_pal];
	    else out_pal = p->pals[p->optimal_pal];

	    if (in->npals) in_pal = in->pals[in->optimal_pal];
	    else in_pal = n->pals[n->optimal_pal];

	    // ignore gamma for now
	    cdelta = find_cdelta(in->true_cdeltas, out_pal, in_pal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
	    costs[ni] = cdelta->deltas;
	    oabs_costs[ni] = p->abs_cost;
	  }

	  total_costs(n, n->n_inputs, oabs_costs, costs, factors, n->abs_cost);
	  lives_free(oabs_costs); lives_free(costs);
	  n->ready_ticks = n->abs_cost[COST_TYPE_TIME] * TICKS_PER_SECOND_DBL;
	}
      }
    }
  }
  if (do_what == 6) {
    // create plan from model
    plan_step_t *step = NULL, *ostep, **xdeps = NULL;
    exec_plan_t *plan = (exec_plan_t *)data;
    if (n->n_inputs) {
      plan_step_t **deps = (plan_step_t **)lives_calloc(n->n_inputs, sizeof(plan_step_t *));
      int d = 0;
      // add one or more steps for each output:
      // if node is a src - clip, src or generator, we first have a load or apply_inst
      // then a convert - to the srcgrp pal, gamma, size

      // if we have any cloned outputs we make a clone for each

      // then we convert each to the connected input

      // the convert or copy layer step is stored in the inputs
      // then at the next node we add an apply_inst step, collating the deps from all inputs
      for (int ni = 0; ni < n->n_inputs; ni++) {
	in = n->inputs[ni];
	if (in->flags & NODEFLAGS_IO_SKIP) continue;

	// if we have any cloned inputs then we add a layer_copy step for each
	if (in->flags & NODEFLAG_IO_CLONE) {
	  xdeps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
	  xdeps[0] = n->inputs[in->origin]->dep;
	  step = create_step(plan, STEP_TYPE_COPY_IN_LAYER, n, ni, xdeps, 1);
	  plan->steps = lives_list_prepend(plan->steps, (void *)step);
	  deps[d++] = step;
	} else deps[d++] = in->dep;
      }

      if (n->virtuals) {
	// first virtual gets deps from inst
	// second virtual has de p on first, etc
	// final virtual becomes dep for inst
	for (LiVESList *virt = n->virtuals; virt; virt = virt->next) {
	  inst_node_t *n = (inst_node_t *)virt->data;
	  step = create_step(plan, STEP_TYPE_APPLY_INST, n, n->model_idx, deps, d);
	  plan->steps = lives_list_prepend(plan->steps, (void *)step);
	  deps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
	  deps[0] = step;
	  d = 1;
	}
      }
      step = create_step(plan, STEP_TYPE_APPLY_INST, n, n->model_idx, deps, d);
      plan->steps = lives_list_prepend(plan->steps, (void *)step);
    } else {
      // sources - we have 2 steps - LOAD or APPLY_INST
      // then a CONVERT to srcgroup->apparent_pal, srcgroup->apparent->gamma, sfile->hsize X sfile->vsize
      int track = n->outputs[0]->track;
      step = create_step(plan, STEP_TYPE_LOAD, n, track, NULL, 0);
      plan->steps = lives_list_prepend(plan->steps, (void *)step);
    }

    if (n->n_outputs) {
      ostep = step;
      for (int no = 0; no < n->n_outputs; no++) {
	plan_step_t *xstep;

	// now for each ouput: if it is a clone, we add a layer copy step
	//next, if node is a src we want to convert to the srcgroup pal, gamma, size

	// then we add convert step to the connected input, and store the step in the input
	// so it can be collated as a dep for the next node
	// we can avoid the CONVERT step if ou width, height, pal and gamma matche the next input
	output_node_t *out = n->outputs[no];
	if (out->flags & NODEFLAGS_IO_SKIP) continue;
	in = out->node->inputs[out->iidx];
	if (in->flags & NODEFLAGS_IO_SKIP) continue;

	step = ostep;

	if (out->flags & NODEFLAG_IO_CLONE) {
	  xdeps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
	  xdeps[0] = step;
	  step = create_step(plan, STEP_TYPE_COPY_OUT_LAYER, n, no, xdeps, 1);
	  plan->steps = lives_list_prepend(plan->steps, (void *)step);
	}
	// add a convert step for going to the next input, and store step in in->dep
	// - for clip srcs this may be 2 conversions in a row - first to srgroup, then the usual

	xdeps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
	xdeps[0] = step;

	xstep = create_step(plan, STEP_TYPE_CONVERT, n, no, xdeps, 1);
	if (xstep) {
	  step = xstep;
	  plan->steps = lives_list_prepend(plan->steps, (void *)step);
	}
	in->dep = step;
      }
    }
  }

  if (do_what == 7) {
    lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)data;
    align_with_node(nodemodel, n);
  }

  if (do_what == 5) {
    // reinit fx
    lives_filter_error_t retval;
    weed_instance_t *inst = rte_keymode_get_instance(n->model_idx + 1, rte_key_getmode(n->model_idx + 1));
    if (inst) {
      d_print_debug("REINIT key %d\n", n->model_idx);
      if (!weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL))
	retval = weed_reinit_effect(inst, FALSE);
      else if (n->needs_reinit) retval = weed_reinit_effect(inst, TRUE);
      n->needs_reinit = FALSE;
      weed_instance_unref(inst);
    }
    /* if (retval == FILTER_ERROR_COULD_NOT_REINIT || retval == FILTER_ERROR_INVALID_PLUGIN */
    /* 	  || retval == FILTER_ERROR_INVALID_FILTER) { */
    /* 	weed_instance_unref(inst); */
    /* } */

    for (LiVESList *list = n->virtuals; list; list = list->next) {
      inst_node_t *n = (inst_node_t *)list->data;
      weed_instance_t *inst = rte_keymode_get_instance(n->model_idx + 1, rte_key_getmode(n->model_idx + 1));
      if (inst) {
	if (prefs->dev_show_timing)
	  d_print_debug("REINIT key %d\n", n->model_idx);
	if (!weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL))
	  retval = weed_reinit_effect(inst, FALSE);
	else if (n->needs_reinit) retval = weed_reinit_effect(inst, TRUE);
	weed_instance_unref(inst);
      }
      IGN_RET(retval);

      n->needs_reinit = FALSE;
    }
  }

  // recurse: for each output, we descend until we the sink node
  // a fail return code indicates that somewhere lower in the chain we reached a node which
  // had unprocessed input(s), if this is the first time round or we had fewer fails than last time
  // we try again, it may be that a later output unblocked the route for an earlier output
  // otherwise we leave connected inputs flagged as processed and return, so we move on to the next source
  // eventually one should connect to the missing input, and we can then continue to descend
  //
  if (n->n_outputs) {
    do {
      int last_nfails = -nfails;
      nfails = 0;
      for (i = 0; i < n->n_outputs; i++) {
	if (!(n->outputs[i]->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_PROCESSED))) {
	  output_node_t *xout = n->outputs[i];
	  input_node_t *xin = xout->node->inputs[xout->iidx];
	  if (xin->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
	  xin->flags |= NODEFLAG_PROCESSED;
	  if (prefs->dev_show_timing)
	    if (do_what == 0) d_print_debug("DESC\n");
	  if (desc_and_do_something(do_what, xout->node, n, data, flags, factors)) nfails++;
	  else n->outputs[i]->flags |= NODEFLAG_PROCESSED;
	}
      }
      // check nfails vs last_nfails. If we have fails, but fewer than last time,
      // try again
      // If we had an identical amount as last time then, the lower node is waiting for another source
      if (!last_nfails || nfails < last_nfails) nfails = -nfails;
    } while (nfails < 0);
  }

  if (nfails) {
    for (i = 0; i < n->n_outputs; i++)
      n->outputs[i]->flags &= ~NODEFLAG_PROCESSED;
    return n;
  }

  n->flags |= NODEFLAG_PROCESSED;

  return NULL;
}


// optimeise combined cost at sink, adjusting each node in isolation
#define OPT_SGL_COMBINED	1

//static void optimise_cost_model(lives_nodemodel_t *nodemodel, int method, double * thresh) {
// here we have set an intial state for the node palettes, and now we try to find a more optimal solution,
// minimising combined cost at the sink, whilst staying with threshold limits

// we will start from the sink(s) and trace back up to the sources first
// this is because if we test ascending, we can optimise the subtree belwo the node being tested by
// by picking the lowest ccost delteas for each node descending,
// in fact we only set the part of the tree above in the case that her are multplr inputs,
// however we can set guess palettes for all inputs, which allows an estimate for the input being varied as we go back down
// else we would have to tes all inputs combined which could be expensive
// - we need only set palettes in case of multiple inputs, otherwise we set the out palette because we know all in
// palettes (because we test going down, which gives the lowest ccost, at each lower node we pick the smallest ccost delta)
// this allows for tesing non optmial combos at eahc node. when we reach a node, we set the guessed optimal
// palette, each time we make a change, we return to source and recalulate
//
// and we will be setting out palettes as we descend, thus it could be that each input prefers a differnt out
// at each nod we check if all outputs have been proeceesed, if not we return and we should fill
// the output by another path.
// if we have all outputs, then we begin examining inputs and test each in / out palette pair for every input
// (all combinations). For every VALID combination, since we know already qcost and tcost at each input node, we can
// then

// when descending we calulated the cost for every possible in / out permutation.
// if we had infinite time, we could check each permutation from sources to sink
// however we do not, so we need to find a near optmial route through. initally we chose the route with min
// ccost delta at each node, however it may be that at some node we should have picked a valu other than as this may
// allow a later optimisation, compensating the increase with a greater decrease
// we can imagin tuples formed from in pal / out pal paris for each input, and associate wwith each tuple a corresponding
// ccost delta we can then make routes with these tuples, an out pal from one tuple "slots into" the next tuple
// restricting the choic of in pal for the input to jsut one value
// now what we must do is to connect tuples via in / out palettes and work out ccost at the sink
// ccost is A0 * cost0 + A1 * cost1 + ....
// thus if qe have A0 * tcost + A1 + qloss,  qloss is a multiplier so for one input
// Cnes = A0 tnew + A1 . qnew
// now if we increase tcost for an input, then tcnesr = tcold - tdelt_old + tdelta
// and increasing qcost : qlnew = qlold / qdelt_old * qdelta_new
// so reintrducing scaling factors, cost new = A0(tcost = tdelt_old + tdelta_new) + A1 * qlold / qdelta_old * qdelta_new)

// we can reframe this as follows. going from one node to the next we have dccost / dtcost and dccost / dqcost
// thefirst of these is just A0 (since partial dccost = A0 / dtcst), the second is dccost = A1 . (qoldd * qdelta)
// = A1 . qcold - where qcold is qloss at the prior node (this is multiplied by the product of
// qloss for all other inputs, but since we vary only one input at atime we can consider this part of the A1 const
// also dccost / dtcost only applies if tcold + dtcost > tmax, else cost is zero.

/// now to find the value at ths ink, we can integrate tcost over all nodes, this is simply A0 . SIGMA tmax
// for find qcost we must find the product of all the qdeltas, thus if we keep all nodes except one the same
// then we can know ccost at the sink, firstly we can add A0 * d(dtccost) and add A1 * qratio, where qrtaio = qnew / qold
// however this multiplier affectsall nodes from the node being changed to the sink, so the change is actually
// A1 * qratio ** n, where n is the numberof nodes including current node and sink along the path from node to sink
// however we can always find the value n, this will not change, thus if we change tcost for a node, we
// can find tcost at the sink simply by adding A0 * delta, however this may also affect qloss at the sink
// for each node, the additional tcost may allow palette conversion such that qloss reduces.
// changin qloss alone will affect C, adding a factor A1 * qdelta ** n
// thus we can change several things in the model:
// increase tcost for an in / out - no effect unles new tcost > tmax
// reduce tcost for a node - only has effet if tcost == tmax, then tcost reduces to next lower tcost
// increase qloss - incresase at sink
// reduce qloss, decrease at sink
//
// the complexity comes from the relation between tcost and qcost
// decreasing qcost means incresing tcost and vice versa
//
// since we know the factors, any change where A0 . tcost delta < -A1 . qloss delta, we should do
// and likewise any change wher qA1 . loss_delta < -A0 tcost_Delta
// now the true complexity is that we need ot do this over the entire tree,
// since both cost deltas depend on in / out pals, changin a value fro one node has a ripple effect
//
// this is due to the tple connections, the problem may even be NP complete - depnding on which tuples are pselected
// we have a new set of tuples at the next node, it may be that we need to test all possible combinations

// however, beginning at a src we cna pick a palette, then proceeding to next node, we already know that the
// optimal thing to do is to leave the palette as is, so we model this untl we reach a node which does not suport
// that palette,. Only here do we need to check the various options. Then again we can continue until
// reahcing anothe node where we are forced to change palette. The only additional cost is the processing time
// whci we accumulate as we go. Thus we can "collapse" these nodes and conside them as a singl nod
// ewith cumulativve processing time, having done this we now optimise for the collapsed model
// if we canntoc continue with our current paltte, genrally this means the plugin wants RGB, or wants packed palettes
// in which case the choice will be around 4 or 5 possiblities, we ought to be able to calulate
// this for up to about 10 "Collapsed" nodes in resonaaeel time. other we can fall back to other optimisation methods
// or we can break the tree into smaller parts. optimising each part separately
//

/* for (LiVESList *list = nodemodel->node_chains; list; list = list->next) { */
/*   node_chain_t *nchain = (node_chain_t *)list->data; */
/*   inst_node_t *n = nchain->last_node; */
/*   // ignore any srcs which have inputs, we will reach these via input sources */
/*   if (n->n_inputs) continue; */
/*   // */
/*   ascend_and_backtrace(n, nodemodel->factors, NULL); */
/*   break; */
/* } */
/* } */


// when creating a node src from a layer, there is a range of possible model types
// if the layer is NULL or out of range, we create a blank frame src node
// otherwise we check the clip type for the layer clip
// this can be generator - then we are going to model an instance with 0 inputs
// otherwise it will likely be a standard clip - then we model the clip_srcs
// or it could be some exotic type, such as a webcam, fifo, tvcard, etc
static inst_node_t *create_node_for_layer(lives_nodemodel_t *nodemodel, int xtrack) {
  lives_clip_t *sfile = NULL;
  inst_node_t *n;
  int *pxtracks = (int *)lives_calloc(1, sizint);
  void *model_for = NULL;
  void *model_inst = NULL;
  int nins = 0;
  int clipno = -1;
  int model_type = NODE_MODELS_SRC;

  pxtracks[0] = xtrack;

  if (xtrack >= 0 && xtrack < nodemodel->ntracks) {
    clipno = nodemodel->clip_index[xtrack];
    sfile = RETURN_VALID_CLIP(clipno);
  }

  if (sfile) {
    switch (sfile->clip_type) {
    case CLIP_TYPE_GENERATOR:
      model_type = NODE_MODELS_GENERATOR;
      model_for = get_primary_actor(sfile);
      model_inst = get_primary_inst(sfile);
      break;
    default:
      model_for = sfile;
      switch (sfile->clip_type) {
      case CLIP_TYPE_NULL_VIDEO:
	//model_for = static_srcs[LIVES_SRC_TYPE_BLANK];
	break;
      case CLIP_TYPE_FILE:
	model_type = NODE_MODELS_CLIP;
	nins = 2;
	break;
      case CLIP_TYPE_DISK:
	model_type = NODE_MODELS_CLIP;
	nins = 1;
	break;
      case CLIP_TYPE_VIDEODEV:
	model_for = get_primary_actor(sfile);
	break;
      default: break;
      }
      break;
    }
  }

  n = create_node(nodemodel, model_type, model_for, nins, NULL, 1, pxtracks);
  n->model_idx = clipno;
  n->model_inst = model_inst;

  if (sfile) {
    if (sfile->clip_type == CLIP_TYPE_VIDEODEV) {
      int gamma;
      full_pal_t pally;
      int *ppal = malloc(sizint);
      for (int i = 0; i < n->n_outputs; i++) {
	output_node_t *out = n->outputs[i];
	out->width = sfile->hsize;
	out->height = sfile->vsize;
	out->flags |= NODEFLAG_IO_FIXED_SIZE;
      }
      get_primary_apparent(clipno, &pally, &gamma);
      n->npals = 1;
      *ppal = pally.pal;
      n->pals = ppal;
      n->free_pals = TRUE;
      n->gamma_type = gamma;
    }
  }

  if (model_type != NODE_MODELS_CLIP) return n;

  if (!sfile) {
    // will need to check for this to mapped later, and we will neeed to set apparent_pal
    // layer has no clipsrc, so we need to find all connected inputs and mark as ignore
    for (int no = 0; no < n->n_outputs; no++) {
      input_node_t *in = n->outputs[no]->node->inputs[n->outputs[no]->iidx];
      in->flags |= NODEFLAG_IO_IGNORE;
    }
    //model_for = static_srcs[LIVES_SRC_TYPE_BLANK];
  } else {
    // create fake inputs for clip source nodes. Store class_id, as this will not change even if srcgrps are swapped
    lives_clipsrc_group_t *srcgrp;
    int fcounts[2];
    double f_ratios[2];
    if (sfile->frame_index) {
      // prop. of decoder frames
      fcounts[1] = count_virtual_frames(sfile->frame_index, 1, sfile->frames);
      // prop. of img frames
      f_ratios[1] = (double)((int)(fcounts[1] * 10) / sfile->frames) / 10.;
    } else f_ratios[1] = 0.;

    f_ratios[0] = 1. - f_ratios[1];

    srcgrp = mainw->track_sources[xtrack];
    if (!srcgrp) srcgrp = get_primary_srcgrp(clipno);

    for (int i = 0; i < srcgrp->n_srcs; i++) {
      lives_clip_src_t *mysrc = srcgrp->srcs[i];
      input_node_t *in = n->inputs[i];

      in->src_uid = mysrc->class_uid;
      switch (mysrc->src_type) {
      case LIVES_SRC_TYPE_IMAGE:
	// create an input src for img_decoder
	in->width = sfile->hsize;
	in->height = sfile->vsize;
	if (sfile->bpp == 32) in->cpal = WEED_PALETTE_RGBA32;
	else in->cpal = WEED_PALETTE_RGB24;
	in->pals = (int *)lives_calloc(3, sizint);
	in->npals = 2;
	in->pals[0] = WEED_PALETTE_RGB24;
	in->pals[1] = WEED_PALETTE_RGBA32;
	in->pals[2] = WEED_PALETTE_END;
	in->free_pals = TRUE;
	in->f_ratio = f_ratios[0];
	break;

      case LIVES_SRC_TYPE_DECODER: {
	// create an input src for the decoder plugin
	lives_decoder_t *dplug = (lives_decoder_t *)mysrc->actor;
	if (dplug) {
	  lives_clip_data_t *cdata = dplug->cdata;
	  in->pals = cdata->palettes;
	  for (int i = 0; in->pals[i] != WEED_PALETTE_END; i++) in->npals++;
	  in->cpal = cdata->current_palette;
	  in->width = cdata->width;
	  in->height = cdata->height;
	  // gamma type not set until we actually pull pixel_data
	  // yuv details (subsopace, sampling, clamping)
	  // not set until we actually pull pixel_data
	}
	in->f_ratio = f_ratios[1];
      }
	break;
      default: break;
      }
    }
    for (int i = 0; i < n->n_outputs; i++) {
      output_node_t *out = n->outputs[i];
      out->width = sfile->hsize;
      out->height = sfile->vsize;
    }
  }

  return n;
}


static void free_all_nodes(lives_nodemodel_t *nodemodel) {
  for (LiVESList *list = nodemodel->nodes; list; list = list->next) {
    inst_node_t *n = (inst_node_t *)list->data;
    if (n) {
      free_node(n);
      list->data = NULL;
    }
  }
  if (nodemodel->nodes) {
    lives_list_free(nodemodel->nodes);
    nodemodel->nodes = NULL;
  }
}


char *get_node_name(inst_node_t *n) {
  char *ntype = NULL;
  if (n) {
    switch (n->model_type) {
    case NODE_MODELS_CLIP:
      ntype = lives_strdup("clip source");
      break;
    case NODE_MODELS_GENERATOR:
    case NODE_MODELS_FILTER: {
      weed_filter_t *filt = (weed_filter_t *)n->model_for;
      if (filt) ntype = weed_filter_get_name(filt);
    }
      break;
    case NODE_MODELS_OUTPUT:
      ntype = lives_strdup("output plugin");
      break;
    case NODE_MODELS_INTERNAL:
      ntype = lives_strdup("internal sink");
      break;
    case NODE_MODELS_SRC:
      ntype = lives_strdup("internal src");
      break;
    default: break;
    }
  }
  return ntype;
}


static void print_node_dtl(inst_node_t *n) {
  char *ntype = get_node_name(n);
  char *node_dtl = lives_strdup_printf("< %s > (gamma - %s)\n", ntype, weed_gamma_get_name(n->gamma_type));
  lives_free(ntype);
  d_print_debug("%s", node_dtl);
  lives_free(node_dtl);
}


static void explain_node(inst_node_t *n, int idx) {
  input_node_t *in;
  output_node_t *out = NULL;
  char *node_dtl;
  int pal, i;
  boolean svary = FALSE;

  if (n->flags & NODESRC_ANY_SIZE) svary = TRUE;

  // what we want to do is - if we arrived from an unprocessed input, describe the input and the node
  // flag the input as procesed
  //
  // if there are other unprocessed inputs we dont want to descend yet, what we want is to describe the chains which
  // lead to the unprocessed inputs
  // so picking any unprocessed input, we ascend from there.
  //
  // the previous node will be either one which has not yet been reached, so no inputs processed
  // or a node with all inputs processed and multiple outputs.
  // in the former cae we just pick any input and ascend again until we reach a source (no inputs)
  // in the latter case we must have descend some output already, one of these will have arrived at out porcessed input
  // so now we want to descend back dwon the output we just arrived from. So we ddesscribe the node the descend back
  // to the unfilled input. The only thing we must be wary of is if we ascend to a node with no processed inputs and
  // multiple outputs. We will ascend an input, then return back down, but we want to continue along the output we
  // arrived at. To enssure this, as we ascend, we flag outputs as processed but crucially, NOT the inputs they connect to
  // thus when descnding we check all outputs, if we find one that is flagged as processed but its connected input
  // iss not flagge likewise, then we descend that first. Thus - descendig we describe input and the node
  // ascending we do not describe anything. Once we reach a source or a node with all inputs processed, we describe it,
  // desscend the marked output and continue describing until we rach either the sink or another node with
  // unprocessed inputs.

  if (idx >= 0) {
    if (n->n_inputs) {
      input_node_t *in = n->inputs[idx];
      in->flags |= NODEFLAG_PROCESSED;

      node_dtl = lives_strdup_printf("input %d of %d, ", idx + 1, n->n_inputs);
      d_print_debug("%s", node_dtl);
      lives_free(node_dtl);

      if (svary) {
	node_dtl = lives_strdup("has variant ");
	d_print_debug("%s", node_dtl);
	lives_free(node_dtl);
      }

      node_dtl = lives_strdup_printf("size %d X %d (%d X %d) ",
				     in->width, in->height, in->inner_width, in->inner_height);
      d_print_debug("%s", node_dtl);
      lives_free(node_dtl);

      if (in->npals) {
	node_dtl = lives_strdup("has variant ");
	d_print_debug("%s", node_dtl);
	lives_free(node_dtl);
      }

      if (in->npals) pal = in->pals[in->optimal_pal];
      else pal = n->pals[n->optimal_pal];

      node_dtl = lives_strdup_printf("palette %s ", weed_palette_get_name(pal));
      d_print_debug("%s", node_dtl);
      lives_free(node_dtl);

      print_node_dtl(n);
    }
  }

  for (i = 0; i < n->n_inputs; i++) {
    in = n->inputs[i];
    if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
    if (!(in->flags & NODEFLAG_PROCESSED)) {
      out = in->node->outputs[in->oidx];
      // ascend until we reach a node with no inputs, dropping breadcrumbs
      // we found an unprocessed input, we want to start ascending, but we need to know the route to follow back down
      // again. We will flag the output as processed but leave input no processed
      out->flags |= NODEFLAG_PROCESSED;
      //if (idx >= 0) print_node_dtl(n);
      explain_node(in->node, -1);
    }
  }

  for (i = 0; i < n->n_outputs; i++) {
    if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    out = n->outputs[i];
    if (out->flags & NODEFLAG_PROCESSED) {
      in = out->node->inputs[out->iidx];
      if (in->flags & NODEFLAG_PROCESSED) continue;
      // if out is flagged as PROCESSED, but in not, then we are descending back down
      // after ascending
      break;
    }
  }

  if (i == n->n_outputs) {
    for (i = 0; i < n->n_outputs; i++) {
      if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
      out = n->outputs[i];
      if (!(out->flags & NODEFLAG_PROCESSED)) break;
    }
  }

  if (i < n->n_outputs) {
    out->flags |= NODEFLAG_PROCESSED;

    print_node_dtl(n);

    node_dtl = lives_strdup_printf("\t output %d of %d: size %d X %d, est. costs: time = %.4f, "
				   "qloss = %.4f, combined = %.4f] ", i + 1, n->n_outputs,
				   out->width, out->height, n->abs_cost[COST_TYPE_TIME],
				   n->abs_cost[COST_TYPE_QLOSS_P], n->abs_cost[COST_TYPE_COMBINED]);
    d_print_debug("%s", node_dtl);
    lives_free(node_dtl);

    d_print_debug("\n\t\t====>");

    explain_node(out->node, out->iidx);
  }

  n->flags |= NODEFLAG_PROCESSED;
}


void describe_chains(lives_nodemodel_t *nodemodel) {
  for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nch = (node_chain_t *)list->data;
    if (nch) {
      inst_node_t *n = nch->first_node;
      int track = nch->track;
      if (n->n_inputs) continue;

      d_print_debug("Found %s node_chain for track %d\n", nch->terminated
		    ? "terminated" : "unterminated", track);
      d_print_debug("Showing palette computation for COST_TYPE_COMBINED\n");
      explain_node(n, -1);
      break;
    }
  }

  d_print_debug("No more node_chains. Finished.\n\n");

  reset_model(nodemodel);
}


#define DO_PRINT

static void _make_nodes_model(lives_nodemodel_t *nodemodel) {
  inst_node_t *n, *ln;
  void *model_for = NULL;
  int *pxtracks;
  int xtrack, i;
  int model_type;
  int nsinks = 0;
  LiVESList *virtuals = NULL;

  // - begin by creating the instance nodes for active fx, and link these in a chain
  // whenever a node has an undefined input, it will return the track number
  //
  // we then either create a source and prepend, or else we follow along the existing chain
  // for the track, if we find a node which does not output to track, then we backtrack to
  // the previous node and add an extra output

  // instance nodes can also act as (temporary) sources - e.g in_tracks 0,1 -> out track 2
  // then we may later have another node: in_tracks 0,2 out track 0
  // in this case we would create a track 2, and set 1st node as its source
  // then at the second node we try to connect 0, and find no output for it, so we add a second output
  // before the first node, and connect this. Then we add a source for track 2, which is the first node.
  // each track may have several sources, for example. we could later have another instance whcih also
  // outputs to 2, this is OK since the final node merged back into 0
  // - if the final node output to 2, and is unconnected then this is invalid.as the teack
  // has an active source already.
  // so what we have is a linked list of (track, src_node) pairs and a flag that informs us whether
  // the final node is an end point (a node which does not output to th same track)
  // this is done very simply. When we prepend a source, we check the out_tracks.
  // If we do not find the in_track listed, then we set the connected flag for the source.
  // if we find an output listed which is not an input then we are going to add this node as a source
  // and we step through the list of sources, raising an error if we find an unconected one for the track

  if (!n_allpals) for (; allpals[n_allpals] != WEED_PALETTE_END; n_allpals++);

  node_idx = 0;
  if (!mainw->multitrack) {
    if (mainw->rte) {
      weed_filter_t *filter;
      weed_instance_t *instance;
      int nins, nouts;
      int *in_tracks = NULL, *out_tracks = NULL;
      for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
	if (rte_key_valid(i + 1, TRUE)) {
	  if (rte_key_is_enabled(i, FALSE)) {
	    // for clip editor we construct instance nodes according to the order of
	    // mainw->rte_keys
	    // for multitrack iterate over filter_map filter_init events (TODO)
	    filter = rte_keymode_get_filter(i + 1, rte_key_getmode(i + 1));
	    if (!filter) continue;

	    if (is_pure_audio(filter, TRUE)) continue;

	    // check if we have an instance for this key
	    instance = rte_keymode_get_instance(i + 1, rte_key_getmode(i + 1));
	    if (instance) weed_instance_unref(instance);
	  } else continue;

	  // create an input / output for each non (permanently) disabled channel
	  nins = count_ctmpls(filter, LIVES_INPUT);
	  nouts = count_ctmpls(filter, LIVES_OUTPUT);

	  if (LIVES_CE_PLAYBACK && nins > 1
	      && (mainw->blend_file == -1 ||
		  (mainw->blend_file == mainw->playing_file)) && !prefs->tr_self)
	    continue;

	  if (!nins && !nouts) {
	    // this is a virtual node
	    // - create node for it and add it to vvirutals list
	    n = create_node(nodemodel, NODE_MODELS_FILTER, filter, 0, NULL, 0, NULL);
	    virtuals = lives_list_append(virtuals, n);
	    continue;
	  }

	  // for generators, the out_track is defined by the clip container
	  // and clip_index
	  if (!nins) continue;

	  if (!mainw->multitrack) {
	    in_tracks = (int *)lives_calloc(nins, sizint);
	    out_tracks = (int *)lives_calloc(nouts, sizint);
	    if (nins > 1) in_tracks[1] = 1;
	  } else {
	    // for multitrack, we have to check to ensure that at least one in_track
	    // has a non-NULL layer, or a node_chain for the track
	    // otherwise the instance has no active inputs, so we will just skip over it
	    // we can also have repeated channels, and need in_count, out_count for this
	    // these values all come from the
	    // TODO
	  }

	  n = create_node(nodemodel, NODE_MODELS_FILTER, filter, nins, in_tracks, nouts, out_tracks);
	  n->virtuals = virtuals;
	  virtuals = NULL;

	  n->model_idx = i;

	  do {
	    xtrack = check_node_connections(nodemodel, n);
	    if (xtrack != -1) {
	      // inst node needs a src, and couldnt find one
	      // either create a layer source or a blank frame source
	      ln = create_node_for_layer(nodemodel, xtrack);
	      add_src_node(nodemodel, ln, xtrack);
	    }
          } while (xtrack != -1);
	  // *INDENT-OFF*
	}}}}
  // *INDENT-ON*

  // finally we add one more node, this represents the display or sink

  nsinks = 0;
  
  if (!mainw->ext_playback) {
    nsinks = 1;
    model_type = NODE_MODELS_INTERNAL;
    if (!(mainw->vpp->capabilities & VPP_LOCAL_DISPLAY)) nsinks = 2;
  }
  if (nsinks != 1) {
    model_type = NODE_MODELS_OUTPUT;
    model_for = (void *)mainw->vpp;
  }

  if (nsinks == 2) {
    // TODO - add a virtual 'splitter' node
    // it eill have 1 input which will feed ditectly to ouy 0, which will connect to display node
    // than we will also create a 2nd output which connects to a non-locsl pb plugin
  }
  // create a node for each sink
  // for now we have only one sink,
  //
  // the output will always connect to the lowest non-NULL, unterminated track
  // unless th track is marked as pass thru
  //
  // this means usually track 0, unless layers[0] is NULL and there is no unteminated
  // chain which outputs to track 0, or track 0 is pass thur, in which case we check track 1, etc
  //
  // so the check seuqence is
  // : unconnected chain -> track 0, layers[0], unconnect chain -> track 1, layers[1]
  // i.e if a layer is non-NULL, and there is no unconnected chain on that track, we see the layer
  // and evrything els is considered "behind" it
  //
  // if there is no such thing then it is possible that the layer for display is being sent to an analyser
  // - the track would have on a terminated node chain; in this case the layer should have been sent to a splitter
  //
  // if we fail to find a non NULL layer, or unterminated node chain, then we create a blank frame source
  // and connect that to the output
  //
  pxtracks = (int *)lives_calloc(1, sizint);

  // find unteminated chain with lowest output track, or lowest non-NULL layer with no node chains
  pxtracks[0] = find_output_track(nodemodel);///lowest_unterm_chain(nodemodel->node_chains);

  n = create_node(nodemodel, model_type, model_for, 1, pxtracks, 0, NULL);
  n->virtuals = virtuals;
  virtuals = NULL;

  do {
    // connect the unterminated output to the sink
    xtrack = check_node_connections(nodemodel, n);
    if (xtrack != -1) {
      // this means that the sink connects to a layer wih no node_chains (or frame  blank src)
      // so, create a node for the source and try again
      inst_node_t *ln = create_node_for_layer(nodemodel, xtrack);
      add_src_node(nodemodel, ln, xtrack);
    }
  } while (xtrack != -1);

  if (prefs->use_screen_gamma)
    n->gamma_type = WEED_GAMMA_MONITOR;

  // since we prepend sources as they are added, we reverse the ordeign so sources whcih are pulled earlier
  // appear earlier in the list
  nodemodel->node_chains = lives_list_reverse(nodemodel->node_chains);

  // now for each node_chain in nodemodel->node_chains, we will check if it is terminated or not
  // if we find an unterminated chain, then we will remove all the nodes in it.
  // - this is likely a secondary output which never reaches a sink
  // so starting from the lastnode we will ascend th tree removing nodes - if we reach a node with
  // multiple outputs then we mark ours as pruned - if a node has all outputs flagged a pruned then we continu up
  // TOOD !!
  //prune_chains(node_chains);
}


void _get_costs(lives_nodemodel_t *nodemodel, boolean fake_costs) {
  LiVESList *list;
  node_chain_t *nchain;
  inst_node_t *n, *retn = NULL;
  int flags = _FLG_GHOST_COSTS;

  // after constructin gthe treemodel by descending, we now proceed in phases
  // we alternate descending and ascending phases, with descending ones passing values from srcs to sink
  // and asceinding propogating values in the opposit direction

  // phase 0, building the model was descending so next wave is ascending

  // when constructing the model we set sizes, but only if the node_chain connected to a src with fixed size
  // now we wil ascend from the sink up to all srcs. Starting with the display size, we con up all inputs
  // keeping track of layer size.

  // If we reach a node with 0, 0- size, then we set the node size to the carried size
  // then ascend inputs from there.

  reset_model(nodemodel);

  // on another ascending wave we note anywhere we upscale and calculate the cost adding to carried cost (qloss_size)
  // if we reach a node with qloss from a downscale

  // phase 3 ascending

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->last_node;
    if (!NODE_IS_SINK(n) && n->n_outputs) continue;
    if (n->flags & NODEFLAG_PROCESSED) continue;

    n->flags |= NODEFLAG_PROCESSED;
    consolidate_size_costs(n, flags);
  }

  reset_model(nodemodel);
  d_print_debug("%s @ %s\n", "pass 3 complete", lives_format_timing_string(lives_get_session_time() - ztime));

  // then we have all size costs set up, and the next wave is descending
  // we will calculate remaining costs going down the branches (edges) of the tree. We would like to
  // minimise the costs at the root node(s). We define two cost types, Let Q be the "quality" of the image at the
  // display, with starting value 1.0 at each source, then passing down the branches and going from node to node,
  // we multiply Q by delta_q (quality loss). At the next node we multiply the Q values for all inputs to find the new
  // Q value. We have q costs for palette changes and qcosts for resizing.
  //
  // We also have cost type T (time). Moving along a branch we add delta_t to T and at thenext node we find max T,
  // then backtrack and find max Q for T up to Tmax. Propogating these values we find Q and T at the root.
  // We want to minimise C (combbined cost), which is tfactor * T + qfactor * (1. - Q)
  // we will find at each output going to the input, delta T, delta Q pairs. The pairs correspond to conversions
  // (size, palette, gamma change) going from output to input. There is an additional cost for T at each node,
  // i.e proc_time.

  // phase 4 descending


  // CALCULATE COST DELTAS NODE - NODE, for all palette pairings
  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      retn = desc_and_compute(n, nodemodel, flags, nodemodel->factors);
    }
  } while (retn);

  reset_model(nodemodel);
  d_print_debug("%s @ %s\n", "pass 4 complete", lives_format_timing_string(lives_get_session_time() - ztime));

  flags &= ~_FLG_GHOST_COSTS;

  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      retn = desc_and_compute(n, nodemodel, flags, nodemodel->factors);
    }
  } while (retn);

  reset_model(nodemodel);
  d_print_debug("%s @ %s\n", "pass 5 complete", lives_format_timing_string(lives_get_session_time() - ztime));

  // we will do this in cycles - first we do not have any absolute timing values, but we just calulate using deltas.
  // this is then used to assign absolute times to the nodes, and the next cycle will use these absolute values
  // and add deltas. This may give different results, and again we use the best values to recalulate aboslute imtes
  //  then go back through, adding deltas and so on

  for (int cyc = 0; cyc < 4; cyc++) {
    d_print_debug("costing cycle %d\nfind best palettes\n", cyc);
    // phase 5 ascending

    // FIND LOWEST COST PALETTES for each cost type
    for (list = nodemodel->node_chains; list; list = list->next) {
      nchain = (node_chain_t *)list->data;
      n = nchain->last_node;
      if (!NODE_IS_SINK(n) && n->n_outputs) continue;
      if (n->flags & NODEFLAG_PROCESSED) continue;

      // checking each input to find prev_pal[cost_type]
      // create cost tuples
      n->flags |= NODEFLAG_PROCESSED;
      map_least_cost_palettes(n, nodemodel->factors);
    }

    reset_model(nodemodel);
    d_print_debug("%s @ %s\n", "pass 5.1 complete", lives_format_timing_string(lives_get_session_time() - ztime));

    d_print_debug("assign abs costs\n");

    // descend, using previously calculated best_pals, find the total costs
    // we pick one cost type to focus on (by default, COMBINED_COST)

    // at each node. we total costs over all inputs, setting abs_cost[cost_type] for the node
    // we find cdeltas for in_pal / out_pal in_gamma / out_gamma
    // (in case we have multi outputs)

    // the tcost becomes ready time, and we can also calulate slack for a node

    // we need to add this to the cdeltas for the next inputs
    // and we also multiply qval by input qvals

    // all sources begin with tcost == 0.0, and qval == 1.0, if a source connects to a later node,
    // this can create slack for irs output(s)
    // (the only other way to get slack is if a node has multiple outputs)

    // TODO -
    // we will also find best pals going down. Starting from a source, we find min deltas going downwards

    // phase 6 descending

    // when calculating min costs, we added some fiddle factors (ghost costs)
    // these are the values we use for optimising costs

    // now when computing abs values we need the versions without these (i.e treu costs)

    do {
      for (list = nodemodel->node_chains; list; list = list->next) {
	nchain = (node_chain_t *)list->data;
	n = nchain->first_node;
	if (n->n_inputs) continue;
	retn = desc_and_total(n, flags, nodemodel->factors);
      }
    } while (retn);

    reset_model(nodemodel);
    d_print_debug("%s @ %s\n", "pass 5.2 complete", lives_format_timing_string(lives_get_session_time() - ztime));
  }

  //// cyc loop done

  // ASCEND, TOTALING SLACK - here we ascend nodes and for each node with outputs we find min slack
  // over all outputs. This value is then added to outputs connected to the node inputs to give
  // total slack for that output

  // phase 7 ascending

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->last_node;
    if (!NODE_IS_SINK(n) && n->n_outputs) continue;
    if (n->flags & NODEFLAG_PROCESSED) continue;

    n->flags |= NODEFLAG_PROCESSED;
    accumulate_slack(n);
  }

  reset_model(nodemodel);

  d_print_debug("%s @ %s\n", "pass 6 complete", lives_format_timing_string(lives_get_session_time() - ztime));

#ifdef OPTIM_MORE

  // phase 8 - descend, creating tuples and prio list. We can "spend" slack to reduce tcost at the sink node
  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      retn = desc_and_make_tuples(n, nodemodel->factors, fake_costs);
    }
  } while (retn);

  reset_model(nodemodel);

#endif
}


// do the following,
// take each node_chain which starts from a source with no inputs
// compute the costs as we pass down nodes.
// if we reach a node with unprocessed inputs, then we continue to the next node_chain
// eventually we will fill all inputs and can contue downwards
void find_best_routes(lives_nodemodel_t *nodemodel, double * thresh) {
  LiVESList *list;
  node_chain_t *nchain;
  inst_node_t *n, *retn = NULL;

  // pass 1 descending - set sizes for known objects
  d_print_debug("SET init node sizes\n");
  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      retn = desc_and_set_sizes(n, nodemodel);
    }
  } while (retn);

  reset_model(nodemodel);
  if (prefs->dev_show_timing) {
    // show model but with no costs yet
    describe_chains(nodemodel);
    reset_model(nodemodel);
  }
  d_print_debug("%s @ %s\n", "pass 1 complete", lives_format_timing_string(lives_get_session_time() - ztime));

  // pass 2 ascending - set missing sizes
  d_print_debug("SET mising node sizes\n");
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->last_node;
    if (!NODE_IS_SINK(n) && n->n_outputs) continue;
    if (n->flags & NODEFLAG_PROCESSED) continue;

    n->flags |= NODEFLAG_PROCESSED;
    set_missing_sizes(n);
  }

  reset_model(nodemodel);

  if (prefs->dev_show_timing) {
    // show model but with no costs yet
    describe_chains(nodemodel);
    reset_model(nodemodel);
  }
  d_print_debug("%s @ %s\n", "pass 2 complete", lives_format_timing_string(lives_get_session_time() - ztime));

  d_print_debug("all sizes set ! Ready for cost estimation\n");

  // having set initial sizes, we can now estimate costs
  _get_costs(nodemodel, TRUE);
}


inst_node_t *src_node_for_track(lives_nodemodel_t *nodemodel, int track) {
  LiVESList *list;
  node_chain_t *nchain;
  inst_node_t *n;
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    if (nchain->track != track) continue;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    return n;
  }
  return NULL;
}


lives_result_t inst_node_set_flags(inst_node_t *n, uint64_t flags) {
  // check flags against all pals - if none, return ERROR
  // check flags against node pals - if none, return FAIL
  int i;
  for (i = 0; i < n->npals; i++) {
    if (pal_permitted(n, n->pals[i])) break;
  }
  if (i == n->npals) {
    for (i = 0; i < n_allpals; i++) if (pal_permitted(n, allpals[i])) break;
    if (allpals[i] == WEED_PALETTE_END) return LIVES_RESULT_ERROR;
    return LIVES_RESULT_FAIL;
  }

  n->flags = flags;
  return LIVES_RESULT_SUCCESS;
}


/* double get_min_cost_to_node(inst_node_t *n, int cost_type) { */
/*   // TODO - we may not have abest pal in future, but rather a set of best_pals */
/*   // for the inputs. Then we need to appl */
/*   int pal; */
/*   if (!n || cost_type < 0 || cost_type >= N_REAL_COST_TYPES) */
/*     return -1.; */
/*   return n->min_cost[cost_type]; */
/* } */


/* int get_best_palette(inst_node_t *n, int idx, int cost_type) { */
/*   if (!n || cost_type < 0 || cost_type >= N_REAL_COST_TYPES) */
/*     return WEED_PALETTE_INVALID; */
/*   if (idx < 0 || idx >= n->n_inputs) */
/*     return WEED_PALETTE_INVALID; */
/*   if (!n->inputs[idx]->npals) { */
/*     if (n->best_pal[cost_type] < 0 || n->best_pal[cost_type] >= n->npals) */
/*       return WEED_PALETTE_INVALID; */
/*     return n->pals[n->best_pal[cost_type]]; */
/*   } */
/*   else { */
/*     input_node_t *in = n->inputs[idx]; */
/*     if (in->best_pal[cost_type] < 0 || in->best_pal[cost_type] >= in->npals) */
/*       return WEED_PALETTE_INVALID; */
/*     return in->pals[in->best_pal[cost_type]]; */
/*   } */
/* } */



void free_nodemodel(lives_nodemodel_t **pnodemodel) {
  if (!pnodemodel || !*pnodemodel) return;
  if ((*pnodemodel)->fx_list) lives_list_free((*pnodemodel)->fx_list);
  if ((*pnodemodel)->clip_index) lives_free((*pnodemodel)->clip_index);
  free_all_nodes(*pnodemodel);
  lives_list_free_all(&(*pnodemodel)->node_chains);
  lives_free(*pnodemodel);
  *pnodemodel = NULL;
}


static void _build_nodemodel(lives_nodemodel_t **pnodemodel, int ntracks, int *clip_index) {
  inst_node_t *retn = NULL;
  char *tmp;

  ____FUNC_ENTRY____(build_nodemodel, "", "viv");

  if (!glob_timing) glob_timing_init();

  //MSGMODE_ON(DEBUG);

  ztime = lives_get_session_time();

  if (pnodemodel) {
    lives_nodemodel_t *nodemodel;

    if (*pnodemodel) free_nodemodel(pnodemodel);

    *pnodemodel = nodemodel = (lives_nodemodel_t *)lives_calloc(1, sizeof(lives_nodemodel_t));

    nodemodel->ntracks = ntracks;

    nodemodel->clip_index = (int *)lives_calloc(ntracks, sizint);
    for (int i = 0; i < ntracks; i++) nodemodel->clip_index[i] = clip_index[i];

    get_player_size(&nodemodel->opwidth, &nodemodel->opheight);
    while (nodemodel->opwidth < 4) {
      if (LIVES_IS_PLAYING) {
	BG_THREADVAR(hook_hints) = HOOK_CB_PRIORITY | HOOK_CB_BLOCK;
	if (mainw->play_window && LIVES_IS_XWINDOW(lives_widget_get_xwindow(mainw->play_window))) {
	  MAIN_THREAD_EXECUTE_RVOID(lives_layer_draw, "vv", LIVES_DRAWING_AREA(mainw->preview_image), NULL);
	} else {
	  MAIN_THREAD_EXECUTE_RVOID(lives_layer_draw, "vv", LIVES_DRAWING_AREA(mainw->play_image), NULL);
	}
	BG_THREADVAR(hook_hints) = 0;
      }
      lives_widget_context_update();
      get_player_size(&nodemodel->opwidth, &nodemodel->opheight);
    }

    // needs adjusting - qloss_p will be around 0.2 bad qloss
    // or 0.5 for catastrrophic
    // qloss_s - downsizing to half size and back up would be catastrophic - qloss_s of 4.
    // downsize by 25% and back would be bad
    // time will be around 0.02 - 0.05 bad with 0.2 being catastrophic
    // if running in highq mode, we should be under 0.02 per frame, an increase of 0.01 sec
    // in trade for a q increase of 0.05 q seems reaonable so we actually want to increase qcost
    // 5:1
    // in medq, we could be running aroun 0.05 tcost , we woul increase by 0.01 in trade for a 5% qloss reduction
    // 5:1
    // in lowq wr may be running at 0.1 tcost and reducing tcost by 0.04 say, for a 0.1 increase in qloss
    // is reasonable so 2.5:1
    nodemodel->factors[1] = 4.;
    nodemodel->factors[2] = 1.;
    nodemodel->factors[3] = .15;

    // fill the nodemodel. We begin by passing through the fx instances is temporal order. At each
    // instance node, we look at the in+_tracks, and find the most current  node_chain for that track
    // if we have an unterminated chain, we simply connect to the last node of the chain. If we find
    // a terminated chain, then we go back one node from the last node and add a clone output, whcih we
    // connect to the input. If there is no node_chain at all then we ccreate a source node for the track
    // and connect that.
    // after passing through all the instances, we add a sink node  (display, usually). Usually this has
    // only a single input, and we are going to connect this to the unterminated
    // node chain with lowest track number. I there is no such chain (which can only happen if there are no
    // instance nodes, the we connect to the lowest track with a non-NULL layer. If there are no non-NULL
    // nodemodel->layers then we create a blank frame source and connect to that.
    //
    // what we end up with is an acyclic, directed, connected graph with a single root node, and all edges directed
    // towards the root, in other words a Reversed Rooteed Tree. If we have multiple sinks (e.g. a display and stream out)
    // then we could have multiple root nodes, and these would connect either to the same tree or to  multiple trees.
    //

    _make_nodes_model(nodemodel);

    if (prefs->dev_show_timing) {
      // show model but with no costs yet
      describe_chains(nodemodel);
      reset_model(nodemodel);
    }

    d_print_debug("%s @ %s\n", "made node model", (tmp = lives_format_timing_string(lives_get_session_time() - ztime)));
    lives_free(tmp);

    // now we have all the details, we can calulate the costs for time and qloss
    // and then for combined costs. For each cost type, we find the sequence of palettes
    // which minimises the cost type. Mostly we are interested in the Combined Cost, but we will
    // calclate for each cost type separately as well.
    find_best_routes(nodemodel, NULL);

    reset_model(nodemodel);

    // show model but now with costs
    describe_chains(nodemodel);
    reset_model(nodemodel);

    // there is one more condition we should check for, YUV detail changes, as these may
    // trigger a reinit. However to know this we need to have actual nodemodel->layers loaded.
    // So we set this flag, which will cause a second dry run with loaded nodemodel->layers before the first actual
    // instance cycle. In doing so we have the opportunity to reinit the instances asyn.
    nodemodel->flags |= NODEMODEL_NEW;

    /* do { */
    /*   for (LiVESList *list = nodemodel->node_chains; list; list = list->next) { */
    /* 	node_chain_t *nchain = (node_chain_t *)list->data; */
    /* 	inst_node_t *n = nchain->first_node; */
    /* 	retn = desc_and_reinit(n); */
    /*   } */
    /* } while (retn); */

    /* reset_model(nodemodel); */
  }
  //MSGMODE_OFF(DEBUG);

  ____FUNC_EXIT____;
}


void build_nodemodel(lives_nodemodel_t **pnodemodel) {
  if (mainw->num_tracks > 2) abort();
  _build_nodemodel(pnodemodel, mainw->num_tracks, mainw->clip_index);
}


void cleanup_nodemodel(lives_nodemodel_t **nodemodel) {
  if (mainw->plan_runner_proc)
    lives_proc_thread_request_cancel(mainw->plan_runner_proc, FALSE);

  if (mainw->layers) {
    // before doing anything else, we need to invalidate
    int maxl;
    if (*nodemodel) maxl = (*nodemodel)->ntracks;
    else maxl = mainw->num_tracks;
    for (int i = 0; i < maxl; i++) {
      if (mainw->layers[i]) {
	lock_layer_status(mainw->layers[i]);
	if (_lives_layer_get_status(mainw->layers[i]) != LAYER_STATUS_READY) {
	  _weed_layer_set_invalid(mainw->layers[i], TRUE);
	}
	unlock_layer_status(mainw->layers[i]);
      }
    }
  }

  if (mainw->plan_runner_proc) {
    if (mainw->plan_cycle) {
      int state = mainw->plan_cycle->state;
      if (state == PLAN_STATE_WAITING ||
	  state == PLAN_STATE_QUEUED)
	plan_cycle_trigger(mainw->plan_cycle);
      lives_millisleep_while_true(mainw->plan_cycle->state == PLAN_STATE_WAITING ||
				  mainw->plan_cycle->state == PLAN_STATE_QUEUED);
    }
    lives_proc_thread_request_cancel(mainw->plan_runner_proc, FALSE);
    lives_proc_thread_join(mainw->plan_runner_proc);
    mainw->plan_runner_proc = NULL;
  }

  if (mainw->plan_cycle) {
    exec_plan_free(mainw->plan_cycle);
    mainw->plan_cycle = NULL;
  }

  if (mainw->exec_plan) {
    exec_plan_free(mainw->exec_plan);
    mainw->exec_plan = NULL;
  }

  if (mainw->layers) {
    int maxl;
    if (*nodemodel) maxl = (*nodemodel)->ntracks;
    else maxl = mainw->num_tracks;
    mainw->frame_layer = NULL;
    reset_old_frame_layer();

    for (int i = 0; i < maxl; i++) {
      if (mainw->layers[i]
	  && mainw->layers[i] != mainw->ext_player_layer
	  && mainw->layers[i] != mainw->cached_frame)
	weed_layer_unref(mainw->layers[i]);
    }
    lives_free(mainw->layers);
    mainw->layers = NULL;
  }
  if (*nodemodel) free_nodemodel(nodemodel);
}


lives_result_t run_next_cycle(void) {
  // run a fresh iteration of mainw->exec_plan
  // WARNING - will alter mainw->plan_cycle
  if (mainw->refresh_model) return LIVES_RESULT_INVALID; 
  if (!mainw->exec_plan) return LIVES_RESULT_FAIL;
  if (mainw->cancelled != CANCEL_NONE) return LIVES_RESULT_INVALID;
  if (mainw->plan_runner_proc
      && lives_proc_thread_get_cancel_requested(mainw->plan_runner_proc))
    return LIVES_RESULT_INVALID;

  if (mainw->plan_cycle) {
    if (mainw->plan_cycle->state == PLAN_STATE_QUEUED
	|| mainw->plan_cycle->state == PLAN_STATE_WAITING
	|| mainw->plan_cycle->state == PLAN_STATE_RUNNING
	|| mainw->plan_cycle->state == PLAN_STATE_PAUSED
	|| mainw->plan_cycle->state == PLAN_STATE_RESUMING) {
      lives_proc_thread_request_cancel(mainw->plan_runner_proc, FALSE);
      lives_proc_thread_join(mainw->plan_runner_proc);
      mainw->plan_runner_proc = NULL;

      exec_plan_free(mainw->plan_cycle);
      mainw->plan_cycle = NULL;
    }
  }

  // free pixel data in all layers except mainw->frame_layer (nullify this to clear it, eg. on error)
  if (mainw->layers) {
    // all our pixel_data should have been free'd already
    for (int i = 0; i < mainw->num_tracks; i++) {
      if (mainw->layers[i]) {
	if (mainw->layers[i] != mainw->frame_layer && mainw->layers[i] != get_old_frame_layer()
	    && mainw->layers[i] != mainw->cached_frame && mainw->layers[i] != mainw->ext_player_layer
	    && mainw->layers[i] != mainw->frame_layer_preload) {
	  weed_layer_set_invalid(mainw->layers[i], TRUE);
	  weed_layer_pixel_data_free(mainw->layers[i]);
	} else mainw->layers[i] = NULL;
      }
    }
  }

  // reset layers for next cycle. Do NOT free them now
  mainw->layers = map_sources_to_tracks(FALSE, TRUE);

  if (!mainw->layers) return LIVES_RESULT_FAIL;

  mainw->plan_cycle = create_plan_cycle(mainw->exec_plan, mainw->layers);

  execute_plan(mainw->plan_cycle, TRUE);

  if (mainw->cancelled != CANCEL_NONE
      || (!mainw->plan_runner_proc
	  || lives_proc_thread_get_cancel_requested(mainw->plan_runner_proc)))
    return LIVES_RESULT_INVALID;

  if (!IS_PHYSICAL_CLIP(mainw->playing_file)) {
    // trigger next plan cycle. We can start loading background frames while displaying current one
    weed_set_boolean_value(mainw->layers[0], LIVES_LEAF_PLAN_CONTROL, TRUE);
    plan_cycle_trigger(mainw->plan_cycle);
    lives_layer_set_clip(mainw->layers[0], mainw->playing_file);
    lives_layer_set_frame(mainw->layers[0], 1);
    mainw->plan_cycle->frame_idx[0] = 1;
    lives_layer_set_status(mainw->layers[0], LAYER_STATUS_PREPARED);
  }

  if (mainw->blend_file != -1 && mainw->num_tracks > 1) {
    frames64_t blend_frame = get_blend_frame(mainw->currticks);
    if (blend_frame > 0) {
      weed_set_boolean_value(mainw->layers[1], LIVES_LEAF_PLAN_CONTROL, TRUE);
      plan_cycle_trigger(mainw->plan_cycle);
      mainw->plan_cycle->frame_idx[1] = blend_frame;
      lives_layer_set_clip(mainw->layers[1], mainw->blend_file);
      lives_layer_set_frame(mainw->layers[1], blend_frame);
      lives_layer_set_status(mainw->layers[1], LAYER_STATUS_PREPARED);
    }
  }
  return LIVES_RESULT_SUCCESS;
}

// split lobx into resize / overlay
// add node to ann_lpt for resize with gamma
// check ann functioning
// add estimates for proc_time + deinterlacing

///

/////////
//
// prerequisites - before calling build_model, we must have the clip_index, and all clips must be created
// e.g generators must have a clip
// ideally map_sources_to_tracks ibs also run now, but this can be skipped if calculating a hypothetical model
// after building the model, make_plan can be called at any time. Clip index must not have changed however and
// each must point to a valid clip - except that index vals may be set to < 0 (NULL layer)
// before or after creating plan, call align_with_model.
// The plan can then be executed. Instances may be reinited on the first execution.
// The layers will have been created from the clip_index, but frame numbers not set
// as soon as frame is set, the source may begin loading.
// The instances will be run in seuqence, and finally an output layer(s) will be ready to send to the sink
// The plan will then be sent to the timing analyser. The plan is re-entrant / parallel, so at any moment it can be
// executed again. This will create a new array of layer waiting for frame number to be created.
// The only resstriction is tha an APPLY_INSTANCE or LOAD step from an earlier plan
// must be finished before it can be run by the following plan. CONVERSION and LAYER_COPY_STEPS can be run out of sequence
// provided the rlimits allow this. Before running any step we check to ensure there ar sufficient resources for the
// prior plan to complete, otherwise the subsequent plan is paused.

// thus the order is - get clip_index - (opt map_sources_to_tracks), build nodemodel - (opt, create instances),
// build plan - align_with_model - (set instance channels, attach track_sources,
// execute plan - set frames in layers

// NOTES: - if we have output clones, we can send layer down multiple outs, provided it is not inplace
// we can optionally replace a copy_laeyer with the cost to the next node for the parent or any clones
//
// then we can simply keep the layer around, so we can avoid a layer copy
// w.g out 0 ---> inst A tcost = t0, qcost = q0
// w.g out 1 ---> inst B tcost = t1, qcost = q1

// normally we would add a layer_copy cost (tcost) to BOTH outputs,
// instead we have options:
// A can convert, B can set the out p

// BUT if combined cost (A) < combined_cost(layer_copy), then we can send the layer to A,
// wait for conversion (if there is any), then, if not  inplace,
// the layer, provided it is not converted, and not inplace, can be used in B

// so with copy we have:
// A ->copy time -> conv cost -> apply inst
// B ->copy time -> conv cost -> apply inst

// if we eliminate copy then
// A -> conv cost -> !inplace ?  -> apply inst  :  resv bb ?  make no inpl     :: suitable for B and B not inplace ? apply B
// B                            -> inplace ? apply inst
// or                                          -> conv cost -> apply inst

// or do we have slack for B ? can we burn slack + elim copy and wait for conv (if A non inpl or rsv bb, and no B conv)
// or wait for conv + apply inst + B conv cost from A conv Vs copy + conv from out pal

// so cpy + conv B

// or conv A      (both non inplace, B can use A conv)
// or conv A, inst A, conv B   (b does conv, or fb7b62ca B cannot non inplace)
// or conv A  apply inst B, appply inst A - b need no conv, b can non inplace, a is inplce

// then: copy_layer OR conv A - can we non inplace A OR can B use cpal and is a proc_time < copy layer
// if we can non inplace, then can we non inplace B ? if so  then is delta to rdy time + proc_cost < lct ?
//     -- if yes then  wait for B rdy time, do non inplace but do not free, then do A inplace

// A non inpl:
//
//    --  otherwise B has to wait for A, do its conve so is A conv + proc_time + A->B conve (discount slack)
//  < copy cost + conv B

// A inpl, B non inpl
// is A conv + B proc < copy cost ?
// is conv A < copy_cost

// B, A ise same pla
// both non inpl
// conv A -> apply A

// so - when we come to do a layer_copy, check - is the input node inplace ?
// - if no, is inpal of orig equal to inpal of clone ?
// - - if no - can we switch one or othe in pals at no cost ?
// - - - if yes, switch it, proceed as yes
// - - if yes, then is other output input inplace ?
//  - - - if no, then elminate layer copy, dep is conv for orig
// - - ---if yes, do we start it now ?
// ......... if yes, can we reserve a bigblock ?
// ............. if yes, reserve bigblock, make it non inplace, proceed as non inplace
// ----------if no, keep as inplace,
//-----------if not inplace - is ready time _ proc_time < dealine  ?
// - - - - - if yes, eliminate layer_copy, apply inst orig becomes dep
//  -------  if no, can we burn slack to wait for apply inst ?
// ------------ if yes, reduce slack, eliminate layer_copy, apply inst orig becomes dep
//
