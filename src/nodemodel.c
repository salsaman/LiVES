// nodemodel.c
// LiVES
// (c) G. Finch 2023 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


// TODO !!! - var size srcs connected to analysers - what size ???
// check size restr. when ssetting sizes

// node modelling - this is used to model the effect chain before it is appplied
// in order to optimise the operational sequence and to compare hypothetical alternatives

// in the current implementation, the focus is on optimising the sequence of palette convesrsion
// and resizes
// as layers pass through the effects chain from sources to display sink

// prerequisites - before calling build_model, we must have the clip_index, and all clips must be created
// e.g generators must have a clip
// ideally map_sources_to_tracks is also run now, but this can be skipped if calculating a hypothetical model
// after building the model, make_plan can be called at any time. Clip index must not have changed however and
// each must point to a valid clip - except that index vals may be set to < 0 (NULL layer)
// before or after creating plan, call align_with_model.
// The plan can then be exectued. Instances may be reinited on the first execution.
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
// the layer, prvided it is not converted, and not inplace, can be used in B

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


#include "main.h"
#include "nodemodel.h"
#include "effects-weed.h"
#include "effects.h"
#include "cvirtual.h"

static int allpals[] = ALL_STANDARD_PALETTES;
static int n_allpals = 0;

/* if (weed_get_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, NULL) == WEED_TRUE) { */
/*   weed_timecode_t tc = weed_get_int64_value(layer, WEED_LEAF_HOST_TC, NULL); */
/*   deinterlace_frame(layer, tc); */
/*   weed_set_boolean_value(layer, WEED_LEAF_HOST_DEINTERLACE, WEED_FALSE); */
/* } */

// still to do:
/// do asynch reinits

// elsewhere:
// create static clip_srcs
// clean up code in load_Frame_layer and weed_appl_instace
// create new func - execute_plan()
// include provision  for deinterlace
// implemnt 2 way letterboxing
// early loading for layers
// measure actual timings

// create model deltas, bypass nodes


static inst_node_t *desc_and_add_steps(lives_nodemodel_t *, inst_node_t *, exec_plan_t *);
static inst_node_t *desc_and_align(inst_node_t *, lives_nodemodel_t *);
static inst_node_t *desc_and_clear(inst_node_t *);

#define OP_RESIZE 0
#define OP_PCONV 1
#define OP_GAMMA 2
#define N_OP_TYPES 3

static lives_result_t get_op_order(int out_size, int in_size, int outpl, int inpl,
                                   int out_gamma_type, int in_gamma_type, int *op_order);

static void reset_model(lives_nodemodel_t *nodemodel) {
  inst_node_t *retn = NULL;
  do {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      // since we ran only the first part of the process, finding the costs
      // but not the second, finding the palette sequence, we need to clear the Processed flag
      // else we would be unbale to reacluate costs again.
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      if (n->n_inputs) continue;
      retn = desc_and_clear(n);
    }
  } while (retn);
}


LIVES_LOCAL_INLINE boolean skip_ctmpl(weed_filter_t *filter, weed_chantmpl_t *ctmpl) {
  // returns TRUE if chentmpl is audio, is disabled. or is alpha only channel
  return weed_chantmpl_is_audio(ctmpl) == WEED_TRUE || weed_chantmpl_is_disabled(ctmpl)
         || !has_non_alpha_palette(ctmpl, filter);
}


static weed_chantmpl_t *get_nth_chantmpl(inst_node_t *n, int cn, int *counts, boolean in_out) {
  // return the nth in or out chantmpl for filter.
  // ignoring audio chans, disabled chans, alpha chans
  // for some filters we have repeatable chantmpls, if counts is supplied, then a value > 1
  // indicates the number of copies of that chantmpl
  //
  weed_filter_t *filter = (weed_filter_t *)n->model_for;
  weed_chantmpl_t **ctmpls, *ctmpl = NULL;
  int nctmpls, i;

  if (in_out) ctmpls = weed_filter_get_in_chantmpls(filter, &nctmpls);
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


static int count_ctmpls(weed_filter_t *filter, int *counts, boolean in_out) {
  // return the nth in or out chantmpl for filter.
  // ignoring audio chans, disabled chans, alpha chans
  // for some filters we have repeatable chantmpls, if counts is supplied, then a value > 1
  // indicates the number of copies of that chantmpl
  //
  weed_chantmpl_t **ctmpls, *ctmpl = NULL;
  int nctmpls, tot = 0, i;

  if (in_out) ctmpls = weed_filter_get_in_chantmpls(filter, &nctmpls);
  else ctmpls = weed_filter_get_out_chantmpls(filter, &nctmpls);
  if (!ctmpls) return 0;

  for (i = 0; i < nctmpls; i++) {
    ctmpl = ctmpls[i];
    if (skip_ctmpl(filter, ctmpl)) continue;
    if (counts) tot += counts[i];
    else tot++;
  }
  lives_free(ctmpls);
  return tot;
}


static void get_resize_ops(int outpl, int inpl, int *ops) {
  int oclamp = WEED_YUV_CLAMPING_UNCLAMPED;
  int iclamping = WEED_YUV_CLAMPING_UNCLAMPED;

  ops[OP_RESIZE] = TRUE;
  ops[OP_PCONV] = FALSE;
  ops[OP_GAMMA] = FALSE;

#ifdef USE_SWSCALE
  // only swscale can convert and resize together
  if (weed_palette_conv_resizable(outpl, oclamp, TRUE) &&
      weed_palette_conv_resizable(inpl, iclamping, FALSE)) {
    ops[OP_PCONV] = TRUE;
  }
#endif

  // gamma conversion not done in resize but can be done in pconve
}


static boolean does_pconv_gamma(int outpl, int inpl) {
  if (can_inline_gamma(outpl, inpl)) return TRUE;
  if (weed_palette_is_rgb(outpl) && weed_palette_is_yuv(inpl)) return TRUE;
  return FALSE;
}


static lives_result_t get_op_order(int out_size, int in_size, int outpl, int inpl,
                                   int out_gamma_type, int in_gamma_type, int *op_order) {
  // we can define order 1, 2, 3
  // if multiple ops have same number, they are done simultaneously
  // if an op is not needed, order remains at 0

  // each of resize / palconv / gamma can be needed or not
  // resize can do resize OR resize + palconv OR ressize + gamma OR resize + palconv + gamma
  // palconv can do palconv or palconv + gamma

  // also, we can NEED resize / palconv / gamma

  boolean resize_ops[N_OP_TYPES];
  boolean ops_needed[N_OP_TYPES];

  boolean pconv_does_gamma = FALSE;
  boolean is_upscale = FALSE;
  boolean in_yuv = FALSE;
  boolean out_yuv = FALSE;

  if (weed_palette_is_yuv(inpl)) in_yuv = TRUE;
  if (weed_palette_is_yuv(outpl)) out_yuv = TRUE;

  if (in_size != out_size) {
    ops_needed[OP_RESIZE] = TRUE;
    if (in_size > out_size) is_upscale = TRUE;
  } else ops_needed[OP_RESIZE] = FALSE;

  if ((in_gamma_type != out_gamma_type && (!out_yuv || !in_yuv))
      || (!out_yuv && in_yuv && out_gamma_type != WEED_GAMMA_UNKNOWN
          && out_gamma_type != WEED_GAMMA_SRGB))
    ops_needed[OP_GAMMA] = TRUE;
  else ops_needed[OP_GAMMA] = FALSE;

  if (inpl != outpl) ops_needed[OP_PCONV] = TRUE;

  // handle trivial situations first
  if (!ops_needed[OP_RESIZE]) {
    if (!ops_needed[OP_PCONV]) {
      if (!ops_needed[OP_GAMMA]) {
        op_order[OP_RESIZE] = op_order[OP_PCONV] = op_order[OP_GAMMA] = 0;
        return LIVES_RESULT_SUCCESS;
      }
      // only gamma
      op_order[OP_RESIZE] = op_order[OP_PCONV] = 0;
      op_order[OP_GAMMA] = 1;
      return LIVES_RESULT_SUCCESS;
    }
    // no resize; pconv needed
    if (!ops_needed[OP_GAMMA]) {
      // no gamma
      op_order[OP_RESIZE] = op_order[OP_GAMMA] = 0;
      op_order[OP_PCONV] = 1;
      return LIVES_RESULT_SUCCESS;
    }
    // pconv / gamma
    // pconv may do both
    pconv_does_gamma = does_pconv_gamma(outpl, inpl);
    if (pconv_does_gamma) {
      op_order[OP_RESIZE] = 0;
      op_order[OP_PCONV] = op_order[OP_GAMMA] = 1;
      return LIVES_RESULT_SUCCESS;
    }
    if (in_yuv) {
      // gamma then pconv
      op_order[OP_RESIZE] = 0;
      op_order[OP_GAMMA] = 1;
      op_order[OP_PCONV] = 2;
      return LIVES_RESULT_SUCCESS;
    }
    // pconv then gamma
    op_order[OP_RESIZE] = 0;
    op_order[OP_PCONV] = 1;
    op_order[OP_GAMMA] = 2;
    return LIVES_RESULT_SUCCESS;
  }

  // resize needed
  get_resize_ops(outpl, inpl, resize_ops);
  if (ops_needed[OP_PCONV]) {
    if (resize_ops[OP_PCONV]) {
      if (ops_needed[OP_GAMMA]) {
        /* if (resize_ops[OP_GAMMA]) { */
        /*   // resize does everything */
        /*   op_order[OP_RESIZE] = op_order[OP_PCONV] = op_order[OP_GAMMA] = 1; */
        /*   return LIVES_RESULT_SUCCESS; */
        /* } */
        // resize does palconv but not gamma
        if (in_yuv || (is_upscale && !out_yuv)) {
          // do gamma b4 palconv
          op_order[OP_GAMMA] = 1;
          op_order[OP_RESIZE] = op_order[OP_PCONV] = 2;
          return LIVES_RESULT_SUCCESS;
        }
        // do pal_conv b4 gamma
        op_order[OP_RESIZE] = op_order[OP_PCONV] = 1;
        op_order[OP_GAMMA] = 2;
        return LIVES_RESULT_SUCCESS;
      }
      // resize does palconv, no gamma needed
      op_order[OP_RESIZE] = op_order[OP_PCONV] = 1;
      op_order[OP_GAMMA] = 0;
      return LIVES_RESULT_SUCCESS;
    }

    // resize does not do palconv, palconv needed

    if (ops_needed[OP_GAMMA]) {
      // resize may do gamma
      /* if (resize_ops[OP_GAMMA]) { */
      /*   if (out_yuv) { */
      /*     op_order[OP_PCONV] = 1; */
      /*     op_order[OP_GAMMA] = 2; */
      /*     op_order[OP_RESIZE] = 2; */
      /*     return LIVES_RESULT_SUCCESS; */
      /*   } */
      /*   if (is_upscale && !in_yuv) { */
      /*     op_order[OP_PCONV] = 1; */
      /*     op_order[OP_GAMMA] = 2; */
      /*     op_order[OP_RESIZE] = 2; */
      /*     return LIVES_RESULT_SUCCESS; */
      /*   } */
      /*   op_order[OP_GAMMA] = 1; */
      /*   op_order[OP_RESIZE] = 1; */
      /*   op_order[OP_PCONV] = 2; */
      /*   return LIVES_RESULT_SUCCESS; */
      /* } */

      // gamma needed
      // palconv may do gamma
      pconv_does_gamma = does_pconv_gamma(outpl, inpl);
      if (is_upscale) {
        // upscale - do resize last if we can
        // do palconv / gamma before resize
        if (pconv_does_gamma) {
          op_order[OP_GAMMA] = op_order[OP_PCONV] = 1;
          op_order[OP_RESIZE] = 2;
          return LIVES_RESULT_SUCCESS;
        }
        // palconv dont do gamma
        if (!out_yuv) {
          // gamma, palconv, resize
          op_order[OP_GAMMA] = 1;
          op_order[OP_PCONV] = 2;
          op_order[OP_RESIZE] = 3;
          return LIVES_RESULT_SUCCESS;
        }
        // palconv, gamma, resize
        op_order[OP_PCONV] = 1;
        op_order[OP_GAMMA] = 2;
        op_order[OP_RESIZE] = 3;
        return LIVES_RESULT_SUCCESS;
      }
      // resize does not do palconv, palconv needed
      // mo upscale, do rresize first

      if (pconv_does_gamma) {
        op_order[OP_RESIZE] = 1;
        op_order[OP_PCONV] = 2;
        op_order[OP_GAMMA] = 2;
        return LIVES_RESULT_SUCCESS;
      }

      /* if (resize_ops[OP_GAMMA]) { */
      /*   op_order[OP_RESIZE] = 1; */
      /*   op_order[OP_GAMMA] = 1; */
      /*   op_order[OP_PCONV] = 2; */
      /*   return LIVES_RESULT_SUCCESS; */
      /* } */

      if (in_yuv) {
        // gamma before palconv
        op_order[OP_RESIZE] = 1;
        op_order[OP_GAMMA] = 2;
        op_order[OP_PCONV] = 3;
        return LIVES_RESULT_SUCCESS;
      }
      // palconv before gamma
      op_order[OP_RESIZE] = 1;
      op_order[OP_PCONV] = 2;
      op_order[OP_GAMMA] = 3;
    }

    // resize, palconv, no gamma
    if (is_upscale) {
      op_order[OP_PCONV] = 1;
      op_order[OP_RESIZE] = 2;
      op_order[OP_GAMMA] = 0;
      return LIVES_RESULT_SUCCESS;
    }
    op_order[OP_RESIZE] = 1;
    op_order[OP_PCONV] = 2;
    op_order[OP_GAMMA] = 0;
    return LIVES_RESULT_SUCCESS;
  }
  // resize, no palconv, maybe gamma
  if (ops_needed[OP_GAMMA]) {
    if (resize_ops[OP_GAMMA]) {
      // resize does gamma
      op_order[OP_RESIZE] = 1;
      op_order[OP_GAMMA] = 1;
      op_order[OP_PCONV] = 0;
      return LIVES_RESULT_SUCCESS;
    }
    if (is_upscale) {
      op_order[OP_GAMMA] = 1;
      op_order[OP_RESIZE] = 2;
      op_order[OP_PCONV] = 0;
      return LIVES_RESULT_SUCCESS;
    }
    op_order[OP_RESIZE] = 1;
    op_order[OP_GAMMA] = 2;
    op_order[OP_PCONV] = 0;
    return LIVES_RESULT_SUCCESS;
  }
  // no gamma needed, only resize
  op_order[OP_RESIZE] = 1;
  op_order[OP_PCONV] = 0;
  op_order[OP_GAMMA] = 0;
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
    if (in_size < out_size)
      return out_size / in_size;
    else return in_size / out_size;
    break;
  case COST_TYPE_TIME:
    // return esstimate of time using pal., and max in_size, out_size
    break;
  default: break;
  }
  return 10.;
}


static double get_layer_copy_cost(int cost_type, int width, int height, int pal) {
  double cost = 0.;
  if (cost_type != COST_TYPE_TIME) return 0.;
  // find the cost for copying (duplicating) a layer. This needs to be factored in if we have cloned inputs,
  // after resizing / converting palette and gamma
  // thishould be dependent on byte size of the pixel data
  // in reality this may be adjusted according to the number of threads available for processing the instance
  // there is a time cost only, no associated qloss
  return cost;
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
                                  int outpl, int inpl, int *inpals, int out_gamma_type, int in_gamma_type,
                                  boolean ghost) {
  // get cost for resize, pal_conv, gamma_change + misc_costs
  // qloss costs for size_changes (QLOSS_S) are already baked into the model so we do not calculate those
  // for QLOSS_P, we have conversion cosst + possible ghost cost
  // for time cost, some operations can be combined - resize, pal_conv, gamma_conversion
  // in addiition we may have misc costs (e.g for deinterlacing)
  // so, valid cost_types are COST_TYPE_TIME and COST_TYPE_QLOSS_P

  int op_order[N_OP_TYPES];

  get_op_order(out_width * out_height, in_width * in_height, outpl, inpl,
               out_gamma_type, in_gamma_type, op_order);

  if (!op_order[OP_RESIZE] && !op_order[OP_PCONV] && !op_order[OP_GAMMA]) return 0.;

  // for COST_TYPE_QLOSS_P it can be more conveneint to call calc_pal_conv_costs directly
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

  // check if resize or letterbox - for lbox we consider only inner size
  // now we should have the operations in order
  if (op_order[OP_RESIZE] == 1) {
    // 1 - -
    if (op_order[OP_PCONV] == 1) {
      // 1 1 -
      if (op_order[OP_GAMMA] == 1) {
        // all 3 ops - so only resize cost : R
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // resize + pconv : R
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize + palconv / gamma : R G
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl)
               + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (!op_order[OP_PCONV]) {
      // 1 0 -
      if (op_order[OP_GAMMA] == 1) {
        // resize + gamma : R
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // resize only : R
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize / gamma : R G
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, inpl)
               + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_PCONV] == 2) {
      // 1 2 -
      if (op_order[OP_GAMMA] == 1) {
        // resize + gamma / pconv: R P
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
               + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (!op_order[OP_GAMMA]) {
        // resize / pconv : R P
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
               + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 2) {
        // resize / palconv + gamma : R P
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
               + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 3) {
        // resize / pconv / gamma : R P G
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
               + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals)
               + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_PCONV] == 3) {
      // 1 3 2
      if (op_order[OP_GAMMA] == 2) {
        // resize / gamma / pconv : R G P
        return get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
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
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 1) {
        // pconv + gamma : P
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (op_order[OP_GAMMA] == 2) {
        // pconv / gamma : P G
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_gamma_cost(COST_TYPE_TIME, out_width, out_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 2) {
      if (op_order[OP_GAMMA] == 1) {
        // pconv + gamma / resize : P R
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (!op_order[OP_GAMMA]) {
        // pconv / resize : P R
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (op_order[OP_GAMMA] == 2) {
        // pconv / resize + gamma : P R
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
      if (op_order[OP_GAMMA] == 3) {
        // pconv / resize / gamma : P R G
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl)
               + get_gamma_cost(COST_TYPE_TIME, in_width, in_height, inpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 3) {
      if (op_order[OP_GAMMA] == 2) {
        // pconv / gamma / resize : P G R
        return get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
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
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
      if (op_order[OP_PCONV] == 2) {
        // gamma / pconv : G P
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
               + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals);
      }
      if (!op_order[OP_PCONV]) {
        // gamma only : G
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost);
      }
    }
    if (op_order[OP_RESIZE] == 2) {
      if (op_order[OP_PCONV] == 2) {
        // gamma / resize + pconv : G R
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl);
      }
      if (op_order[OP_PCONV] == 3) {
        // gamma / resize / pconv : G R P
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, outpl, outpl)
               + get_pconv_cost(COST_TYPE_TIME, in_width, in_height, outpl, inpl, inpals);
      }
    }
    if (op_order[OP_RESIZE] == 3) {
      if (op_order[OP_PCONV] == 2) {
        // gamma / pconv / resize : G P R
        return get_gamma_cost(COST_TYPE_TIME, out_width, out_height, outpl, out_gamma_type, in_gamma_type, ghost)
               + get_pconv_cost(COST_TYPE_TIME, out_width, out_height, outpl, inpl, inpals)
               + get_resize_cost(COST_TYPE_TIME, out_width, out_height, in_width, in_height, inpl, inpl);
      }
    }
  }
  return 0.;
}

// CONVERT STEP // (need LOAD STEP, LAYER COPY STEP, APPLY INST step)

static lives_filter_error_t palconv(lives_layer_t *layer) {
  GET_PROC_THREAD_SELF(self);
  _prefs *prefs = (_prefs *)GET_SELF_VALUE(voidptr, "prefs");
  int tgt_gamma = GET_SELF_VALUE(int, "tgt_gamma");
  int osampling = GET_SELF_VALUE(int, "osampling");
  int osubspace = GET_SELF_VALUE(int, "osubspace");
  int oclamping = GET_SELF_VALUE(int, "oclamping");
  int opalette = GET_SELF_VALUE(int, "opalette");

  int inpalette = weed_layer_get_palette(layer);

  lives_filter_error_t retval = FILTER_SUCCESS;

  if (prefs->dev_show_timing)
    g_printerr("clpal1 pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);

  if (inpalette != opalette) {
    if (!convert_layer_palette_full(layer, opalette, oclamping,
                                    osampling, osubspace, tgt_gamma)) {
      retval = FILTER_ERROR_INVALID_PALETTE_CONVERSION;
      lives_proc_thread_error(self, (int)retval, NULL);
    }
  }

  if (prefs->dev_show_timing) {
    g_printerr("clpal1 post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
  }
  return retval;
}


static lives_filter_error_t gamma_conv(lives_layer_t *layer) {
  GET_PROC_THREAD_SELF(self);
  _prefs *prefs = (_prefs *)GET_SELF_VALUE(voidptr, "prefs");
  lives_filter_error_t retval = FILTER_SUCCESS;
  int tgt_gamma = GET_SELF_VALUE(int, "tgt_gamma");
  int xwidth = GET_SELF_VALUE(int, "xwidth");
  int xheight = GET_SELF_VALUE(int, "xheight");
  int width = weed_layer_get_width(layer);
  int height = weed_layer_get_height(layer);

  if (prefs->dev_show_timing)
    g_printerr("gamma1 pre @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
  // if we letterboxed then we can save a few cycles by not gamma converting the blank regions
  // in med, low this seems not to work
  if (GET_SELF_VALUE(boolean, "letterboxed"))
    gamma_convert_sub_layer(tgt_gamma, 1.0, layer, (width - xwidth) >> 1, (height - xheight) >> 1,
                            xwidth, xheight, TRUE);
  else gamma_convert_layer(tgt_gamma, layer);
  if (prefs->dev_show_timing)
    g_printerr("gamma1 post @ %f\n", lives_get_current_ticks() / TICKS_PER_SECOND_DBL);
  return retval;
}


static lives_filter_error_t res_or_lbox(lives_layer_t *layer) {
  GET_PROC_THREAD_SELF(self);

  int width = GET_SELF_VALUE(int, "width");
  int height = GET_SELF_VALUE(int, "height");
  int xwidth = GET_SELF_VALUE(int, "xwidth");
  int xheight = GET_SELF_VALUE(int, "xheight");
  int opalette = GET_SELF_VALUE(int, "opalette");
  int oclamping = GET_SELF_VALUE(int, "oclamping");
  int interp = GET_SELF_VALUE(int, "interp");

  lives_filter_error_t retval = FILTER_SUCCESS;

  boolean resized = FALSE;

  if (xwidth && xheight && (xwidth < width || xheight < height))
    resized = letterbox_layer(layer, width, height, xwidth, xheight, interp, opalette, oclamping);
  if (!resized)
    resized = resize_layer(layer, width, height, interp, opalette, oclamping);

  if (!resized) {
    retval = FILTER_ERROR_UNABLE_TO_RESIZE;
    lives_proc_thread_error(self, (int)retval, NULL);
  }

  return retval;
}

/////////////////////////////


static void run_plan(exec_plan_t *plan) {
  // cycle through the plan steps
  // skip over any flagged as running, finished, error or ignore
  GET_PROC_THREAD_SELF(self);
  plan_step_t *step;
  lives_layer_t *layer;
  lives_proc_thread_t lpt;
  double xtime;
  boolean complete = FALSE;
  boolean cancelled = FALSE;
  boolean paused = FALSE;
  int error = 0;
  int lstatus, i, state;

  if (!plan) return;

  do {
    boolean can_resume = TRUE;
    complete = TRUE;
    lives_microsleep;

    for (LiVESList *steps = plan->steps; steps; steps = steps->next) {
      if (!plan->layers) {
        error = 1;
      }

      if (lives_proc_thread_get_cancel_requested(self)) cancelled = TRUE;
      if (lives_proc_thread_get_pause_requested(self)) paused = TRUE;
      if (lives_proc_thread_get_resume_requested(self)) {
        if (paused) {
          paused = FALSE;
          plan->state = PLAN_STATE_RESUMING;
        }
      }

      step = (plan_step_t *)steps->data;

      if (!step) continue;

      state = step->state;

      if (state == STEP_STATE_ERROR || state == STEP_STATE_FINISHED
          || state == STEP_STATE_IGNORE) continue;

      if (state != STEP_STATE_RUNNING) {
        if (cancelled || paused || error) continue;

        complete = FALSE;

        // ensure dependencie are fullfilled
        if (step->st_type == STEP_TYPE_LOAD) {
          // ensure the layer exists and is in state PREPARED
          if (!plan->layers) {
            error = 1;
            continue;
          }

          layer = plan->layers[step->track];

          if (!layer) {
            complete = FALSE;
            continue;
          }

          mainw->frame_index[step->track] = lives_layer_get_frame(layer);

          lstatus = lives_layer_get_status(layer);

          if (lstatus == LAYER_STATUS_READY || lstatus == LAYER_STATUS_LOADED) {
            step->state = STEP_STATE_FINISHED;
            continue;
          }
          if (lstatus != LAYER_STATUS_PREPARED) continue;
        } else {
          for (i = 0; i < step->ndeps; i++) {
            plan_step_t *xstep = step->deps[i];
            if (xstep->state != STEP_STATE_FINISHED) break;
          }
          if (i < step->ndeps) continue;
        }

        xtime = lives_get_session_time() * 1000.;

        switch (step->st_type) {
        case STEP_TYPE_LOAD: {
          g_print("RUN LOAD @ %.4f msec\n", xtime);
          layer = plan->layers[step->track];
          // frame will be loaded from whatever clip_src
          step->state = STEP_STATE_RUNNING;
          pull_frame_threaded(layer, 0, 0);
        }
        break;
        case STEP_TYPE_CONVERT:
          g_print("RUN CONVERT @ %.4f msec\n", xtime);
          layer = plan->layers[step->track];
          if (1) {
            // figure out the sequence of operations needed, construct a prochthread chain,
            // then queue it
            int op_order[N_OP_TYPES];
            int out_width = weed_layer_get_width(layer);
            int out_height = weed_layer_get_height(layer);
            int in_width = step->fin_width;
            int in_height = step->fin_height;
            int outpl = weed_layer_get_palette(layer);
            int inpl = step->fin_pal;
            int out_gamma_type = weed_layer_get_gamma(layer);
            int in_gamma_type = step->fin_gamma;

            // if this is a pre-conversion for a clip, we won't have a target pal or gamma
            // we have to get these from the srcgroup

            if (inpl == WEED_PALETTE_NONE) {
              lives_clipsrc_group_t *srcgrp = mainw->track_sources[step->track];
              inpl = srcgrp->apparent_pal;
              in_gamma_type = srcgrp->apparent_gamma;
            }

            get_op_order(out_width * out_height, in_width * in_height, outpl, inpl,
                         out_gamma_type, in_gamma_type, op_order);

            if (!op_order[OP_RESIZE] && !op_order[OP_PCONV] && !op_order[OP_GAMMA]) {
              step->state = STEP_STATE_FINISHED;
              continue;
            }

            if (op_order[OP_RESIZE] == 1)
              lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                             res_or_lbox, WEED_SEED_INT, "v", layer);
            else if (op_order[OP_PCONV] == 1)
              lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                             palconv, WEED_SEED_INT, "v", layer);
            else lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                                  gamma_conv, WEED_SEED_INT, "v", layer);

            if (op_order[OP_RESIZE] == 2)
              lives_proc_thread_chain(lpt, res_or_lbox, WEED_SEED_INT, "v", layer, NULL);
            else if (op_order[OP_PCONV] == 2)
              lives_proc_thread_chain(lpt, palconv, WEED_SEED_INT, "v", layer, NULL);
            else if (op_order[OP_GAMMA] == 2)
              lives_proc_thread_chain(lpt, gamma_conv, WEED_SEED_INT, "v", layer, NULL);

            if (op_order[OP_RESIZE] == 3)
              lives_proc_thread_chain(lpt, res_or_lbox, WEED_SEED_INT, "v", layer, NULL);
            else if (op_order[OP_PCONV] == 3)
              lives_proc_thread_chain(lpt, palconv, WEED_SEED_INT, "v", layer, NULL);
            else if (op_order[OP_GAMMA] == 3)
              lives_proc_thread_chain(lpt, gamma_conv, WEED_SEED_INT, "v", layer, NULL);

            SET_LPT_VALUE(lpt, int, "width", step->fin_width);
            SET_LPT_VALUE(lpt, int, "height", step->fin_height);
            SET_LPT_VALUE(lpt, int, "xwidth", step->fin_iwidth);
            SET_LPT_VALUE(lpt, int, "xheight", step->fin_iheight);
            SET_LPT_VALUE(lpt, int, "interp", get_interp_value(prefs->pb_quality, TRUE));
            SET_LPT_VALUE(lpt, plantptr, "layer", layer);
            SET_LPT_VALUE(lpt, voidptr, "prefs", (void *)prefs);
            SET_LPT_VALUE(lpt, int, "opalette", step->fin_pal);
            SET_LPT_VALUE(lpt, int, "tgt_gamma", step->fin_gamma);

            /* SET_LPT_VALUE(lpt, int, "osubspace", osubspace); */
            /* SET_LPT_VALUE(lpt, int, "osampling", osampling); */
            /* SET_LPT_VALUE(lpt, int, "oclamping", oclamping); */

            //if (!mainw->debug_ptr) mainw->debug_ptr = lpt;

            step->proc_thread = lpt;

            // queue the proc thread for execution
            weed_set_plantptr_value(layer, LIVES_LEAF_PRIME_LPT, lpt);
            step->state = STEP_STATE_RUNNING;
            lives_proc_thread_queue(lpt, LIVES_THRDATTR_PRIORITY);
          }
          break;
        case STEP_TYPE_APPLY_INST:
          g_print("RUN APPLY_INST @ %.4f msec\n", xtime);
          if (!step->target) {
            g_print("APPLY_INST done @ %.4f msec\n", xtime);
            step->state = STEP_STATE_FINISHED;
            continue;
          }
          weed_instance_t *inst = (weed_instance_t *)step->target_inst;
          lives_layer_t *layer = plan->layers[step->track];
          int layer_gamma = weed_layer_get_gamma(layer);
          if (layer_gamma == WEED_GAMMA_LINEAR) {
            // if we have RGBA type in / out params, and instance runs with linear gamma
            // then we scale the param values according to the gamma correction
            gamma_conv_params(WEED_GAMMA_LINEAR, inst, TRUE);
            gamma_conv_params(WEED_GAMMA_LINEAR, inst, FALSE);
          }

          // apply inst

          if (layer_gamma == WEED_GAMMA_LINEAR) {
            // if we scaled params, scale them back so they are displayed correctly in interfaces
            gamma_conv_params(WEED_GAMMA_SRGB, inst, TRUE);
            gamma_conv_params(WEED_GAMMA_SRGB, inst, FALSE);
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
          lstatus = lives_layer_get_status(layer);
          lpt = lives_layer_get_procthread(layer);
          if (lstatus == LAYER_STATUS_LOADING) {
            if (lpt) {
              if (error || cancelled) {
                if (!lives_proc_thread_get_cancel_requested(lpt)) {
                  weed_layer_set_invalid(layer, TRUE);
                  lives_proc_thread_request_cancel(lpt, FALSE);
                  if (lives_proc_thread_was_cancelled(lpt)) {
                    weed_layer_set_invalid(layer, TRUE);
                    step->state = STEP_STATE_CANCELLED;
                  }
                  continue;
                }
                if (paused) {
                  if (!lives_proc_thread_get_pause_requested(lpt))
                    lives_proc_thread_request_pause(lpt);
                  if (lives_proc_thread_is_paused(lpt)) {
                    step->state = STEP_STATE_PAUSED;
                  }
                  continue;
                }
                if (lives_proc_thread_was_cancelled(lpt)) {
                  step->state = STEP_STATE_CANCELLED;
                  weed_layer_set_invalid(layer, TRUE);
                  continue;
                }
                if (plan->state == STEP_STATE_RESUMING) {
                  if (!lives_proc_thread_get_resume_requested(lpt)) {
                    lives_proc_thread_request_resume(lpt);
                  }
                  if (lives_proc_thread_is_paused(lpt)) {
                    can_resume = FALSE;
                    continue;
                  }
                  step->state = STEP_STATE_RUNNING;
                }
                if (lives_proc_thread_is_paused(lpt)) {
                  step->state = STEP_STATE_PAUSED;
                  continue;
                }
              }
            }
          }
          if (lstatus == LAYER_STATUS_LOADED || lstatus == LAYER_STATUS_READY) {
            xtime = lives_get_session_time() * 1000.;
            g_print("LOAD done @ %.4f msec\n", xtime);
            step->state = STEP_STATE_FINISHED;
            continue;
          }
        }
        break;

        case STEP_TYPE_CONVERT: {
          layer = plan->layers[step->track];
          lpt = step->proc_thread;
          if (lpt) {
            if (error || cancelled) {
              if (!lives_proc_thread_get_cancel_requested(lpt))
                lives_proc_thread_request_cancel(lpt, FALSE);
              if (lives_proc_thread_was_cancelled(lpt)) {
                step->state = STEP_STATE_CANCELLED;
                weed_layer_set_invalid(layer, TRUE);
                lives_proc_thread_join_int(lpt);
                lives_proc_thread_unref(lpt);
              }
              continue;
            }
            if (paused) {
              if (!lives_proc_thread_get_pause_requested(lpt))
                lives_proc_thread_request_pause(lpt);
              if (lives_proc_thread_is_paused(lpt)) {
                step->state = STEP_STATE_PAUSED;
              } else complete = FALSE;
              continue;
            }
            if (lives_proc_thread_was_cancelled(lpt)) {
              step->state = STEP_STATE_CANCELLED;
              weed_layer_set_invalid(layer, TRUE);
              lives_proc_thread_join_int(lpt);
              lives_proc_thread_unref(lpt);
              continue;
            }
            if (plan->state == STEP_STATE_RESUMING) {
              if (!lives_proc_thread_get_resume_requested(lpt)) {
                lives_proc_thread_request_resume(lpt);
              }
              if (lives_proc_thread_is_paused(lpt)) {
                can_resume = FALSE;
                complete = FALSE;
                continue;
              }
              step->state = STEP_STATE_RUNNING;
            }
            if (lives_proc_thread_is_paused(lpt)) {
              step->state = STEP_STATE_PAUSED;
              continue;
            }
          }

          if (lives_proc_thread_is_done(lpt)) {
            g_print("CONVERT done\n");
            //int retval =
            lives_proc_thread_join_int(lpt);
            if (lives_proc_thread_had_error(lpt)) {
              //int retval = lives_proc_thread_get_errnum(lpt);
              //mainw->debug_ptr = NULL;
              step->state = STEP_STATE_ERROR;
            } else {
              xtime = lives_get_session_time() * 1000.;
              g_print("CONVERT done @ %.4f msec\n", xtime);
              step->state = STEP_STATE_FINISHED;
            }
            lives_proc_thread_unref(lpt);
            weed_leaf_delete(layer, LIVES_LEAF_PRIME_LPT);
            step->proc_thread = NULL;
          }
        }
        break;
        default:
          break;
        }
      }
    }
    if (plan->state == PLAN_STATE_RESUMING && can_resume) {
      plan->state = PLAN_STATE_RUNNING;
    }

    if (complete && paused) {
      plan->state = PLAN_STATE_PAUSED;
      lives_proc_thread_pause(self);
      plan->state = PLAN_STATE_RESUMING;
      complete = FALSE;
      paused = FALSE;
    }
  } while (!complete);
  if (cancelled) {
    plan->state = PLAN_STATE_CANCELLED;
    lives_proc_thread_cancel(self);
  } else if (error) {
    plan->state = PLAN_STATE_ERROR;
    lives_proc_thread_error(self, error, "%s", "plan error");
  } else plan->state = PLAN_STATE_FINISHED;

  g_print("PLAN DONE\n");
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


// if an inst has linear gamm convert its params

lives_proc_thread_t execute_plan(exec_plan_t *plan, boolean async) {
  // execute steps in plan. If asynch is TRUE, then this is done in a proc_thread whichis returned
  // otherwise it runs synch and NULL is returned
  lives_proc_thread_t lpt = NULL;
  if (async) {
    if (plan->state == PLAN_STATE_RUNNING) return mainw->plan_runner_proc;
    plan->state = PLAN_STATE_RUNNING;
    mainw->plan_runner_proc = lpt
                              = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED, run_plan, WEED_SEED_INT, "v", plan);
    lives_proc_thread_set_cancellable(lpt);
    lives_proc_thread_set_pauseable(lpt, TRUE);
    lives_proc_thread_queue(lpt, LIVES_THRDATTR_PRIORITY);
  } else run_plan(plan);
  return lpt;
}


static plan_step_t *locate_step_for(exec_plan_t *plan, int st_type, void *id, void *alt_id) {
  for (LiVESList *list = plan->steps; list; list = list->next) {
    plan_step_t *step = (plan_step_t *)list->data;
    if (step->st_type == st_type
        && ((id && step->target == id)
            || (alt_id && step->target == alt_id))) return step;
  }
  return NULL;
}


static plan_step_t *plan_step_copy(exec_plan_t *dplan, exec_plan_t *splan, plan_step_t *sstep) {
  plan_step_t *dstep = (plan_step_t *)lives_calloc(1, sizeof(plan_step_t));
  lives_memcpy(dstep, sstep, sizeof(plan_step_t));
  dstep->deps = NULL;

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
    cycle->uid = template->uid;
    cycle->model = template->model;
    cycle->iteration = ++template->iteration;
    cycle->ntracks = template->ntracks;
    cycle->layers = layers;
    for (LiVESList *list = template->steps; list; list = list->next) {
      plan_step_t *step = (plan_step_t *)list->data, *xstep = plan_step_copy(cycle, template, step);
      cycle->steps = lives_list_append(cycle->steps, (void *)xstep);
    }
  }
  return cycle;
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
  int step_number = 0;
  int pal;

  if (idx >= 0) {
    if (st_type == STEP_TYPE_COPY_IN_LAYER)
      in = n->inputs[idx];
    else out = n->outputs[idx];
  }

  while (1) {
    g_print("add step type %d\n", st_type);
    step_number++;

    step = (plan_step_t *)lives_calloc(1, sizeof(plan_step_t));
    step->st_type = st_type;
    step->deps = deps;
    step->ndeps = ndeps;
    step->node = n;
    step->idx = idx;

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

    switch (st_type) {
    case STEP_TYPE_LOAD: {
      step->node = n;
      step->st_time = -1;
      step->dur = -1;
      step->ded = 0;

      // all we really need is the track number
      step->track = idx;
      step->clip = n->model_idx;

      // can substitue a bblock cost
      step->start_res[RES_TYPE_THRD] += 1;
      step->end_res[RES_TYPE_MEM] =
        lives_frame_calc_bytesize(step->fin_width, step->fin_height, step->fin_pal, NULL);
      step->end_res[RES_TYPE_BBLOCK] = 1;
    }
    break;

    case STEP_TYPE_CONVERT: {
      weed_filter_t *filter = NULL;
      int *pal_list;
      int ipal, opal;
      boolean inplace = FALSE;

      step->track = out->track;

      // for srcs we want to ad a "pre" convert to srcgroup
      if (!n->n_inputs && step_number == 1) {
        // convert to the tack_source srcgroup
        lives_clipsrc_group_t *srcgrp;
        lives_clip_t *sfile;

        step->clip = n->model_idx;
        srcgrp = get_srcgrp(step->clip, step->track, SRC_PURPOSE_ANY);

        sfile = RETURN_VALID_CLIP(step->clip);

        step->fin_width = sfile->hsize;
        step->fin_height = sfile->vsize;
        step->fin_pal = srcgrp->apparent_pal;
        step->fin_gamma = srcgrp->apparent_gamma;

        plan->steps = lives_list_prepend(plan->steps, (void *)step);
        deps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
        ndeps = 1;
        deps[0] = step;
        continue;
      }

      in = out->node->inputs[out->iidx];

      filter = (weed_filter_t *)n->model_for;
      if (in->npals) {
        ipal = in->pals[in->optimal_pal];
        pal_list = in->pals;
      } else {
        ipal = n->pals[n->optimal_pal];
        pal_list = n->pals;
      }
      if (out->npals) opal = out->pals[out->optimal_pal];
      else opal = n->pals[n->optimal_pal];
      step->st_time = n->ready_ticks;
      if (filter) {
        step->st_time += get_proc_cost(COST_TYPE_TIME, filter,
                                       n->width, n->height, n->optimal_pal);
        step->dur = get_conversion_cost(COST_TYPE_TIME, out->width, out->height, in->width, in->height,
                                        opal, ipal, pal_list, n->gamma_type, out->node->gamma_type, FALSE);
      }
      step->ded = out->node->ready_ticks;

      step->track = in->track;

      step->fin_width = in->width;
      step->fin_height = in->height;
      step->fin_iwidth = in->inner_width;
      step->fin_iheight = in->inner_height;
      step->fin_pal = ipal;
      step->fin_gamma = out->node->gamma_type;

      if (in->width == out->width && in->height == out->height
          && (ipal == opal || pconv_can_inplace(opal, ipal))) inplace = TRUE;

      if (!inplace) {
        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(step->fin_width, step->fin_height, step->fin_pal, NULL);
        step->end_res[RES_TYPE_MEM] = step->start_res[RES_TYPE_MEM]
                                      - lives_frame_calc_bytesize(out->width, out->height, opal, NULL);
        step->start_res[RES_TYPE_BBLOCK]++;
        step->end_res[RES_TYPE_BBLOCK] = 1;
      }
    }
    break;

    case STEP_TYPE_COPY_OUT_LAYER:
      break;
    case STEP_TYPE_COPY_IN_LAYER: {
      if (st_type == STEP_TYPE_COPY_IN_LAYER) {
        input_node_t *orig = n->inputs[in->origin];
        p = orig->node;
        out = p->outputs[orig->oidx];
        if (orig->npals) pal = orig->pals[orig->optimal_pal];
        else  pal = n->pals[n->optimal_pal];
        step->schan = in->origin;
        step->dchan = idx;
        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(orig->width, orig->height, pal, NULL);
        step->dur = get_layer_copy_cost(COST_TYPE_TIME, orig->width, orig->height, pal);
      } else {
        output_node_t *orig = n->outputs[out->origin];
        if (orig->npals) pal = orig->pals[orig->optimal_pal];
        else pal = n->pals[n->optimal_pal];
        step->schan = out->origin;
        step->dchan = idx;
        step->start_res[RES_TYPE_MEM] +=
          lives_frame_calc_bytesize(orig->width, orig->height, pal, NULL);
        step->dur = get_layer_copy_cost(COST_TYPE_TIME, orig->width, orig->height, pal);
      }

      step->end_res[RES_TYPE_MEM] = step->start_res[RES_TYPE_MEM];

      step->st_time = -1;
      step->ded = n->ready_ticks;
      step->dchan = idx;

      step->start_res[RES_TYPE_BBLOCK]++;
      step->end_res[RES_TYPE_BBLOCK] = step->start_res[RES_TYPE_BBLOCK];
    }
    break;

    case STEP_TYPE_APPLY_INST: {
      step->st_time = n->ready_ticks;

      if (n->model_type == NODE_MODELS_FILTER || n->model_type == NODE_MODELS_GENERATOR) {
        size_t memused = 0;
        weed_filter_t *filter = (weed_filter_t *)n->model_for;
        if (n->model_inst) step->target = (weed_instance_t *)n->model_inst;
        else step->target = filter;
        step->dur = get_proc_cost(COST_TYPE_TIME, filter, n->width, n->height, n->optimal_pal);
        step->ded = -1;

        for (int i = 0; i < n->n_outputs; i++) {
          // if we had bblocks in inputs, if an out is inplace and input is bblock
          // then we keep it. If not inplace, we can add a bblock if we have spares
          out = n->outputs[i];
          if (out->flags & NODEFLAGS_IO_SKIP) continue;
          if (out->npals) pal = out->optimal_pal;
          else pal = n->optimal_pal;
          memused += lives_frame_calc_bytesize(out->width, out->height, pal, NULL);
        }

        step->start_res[RES_TYPE_MEM] += memused;
        step->end_res[RES_TYPE_MEM] = memused;
        step->start_res[RES_TYPE_BBLOCK] += n->n_outputs;
        step->end_res[RES_TYPE_BBLOCK] = n->n_outputs;
      } else {
        // this must be an output sink
        size_t memused;
        in = n->inputs[0];
        pal = n->optimal_pal;
        memused = lives_frame_calc_bytesize(in->width, in->height, pal, NULL);
        step->start_res[RES_TYPE_THRD] = step->end_res[RES_TYPE_THRD] = 0;
        step->end_res[RES_TYPE_MEM] = memused;
        step->end_res[RES_TYPE_BBLOCK] = 1;
      }
    }
    break;
    default: break;
    }
    break;
  }
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
  //plan->n_layers = nodemodel->ntracks;

  reset_model(nodemodel);

  do {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      // since we ran only the first part of the process, finding the costs
      // but not the second, finding the palette sequence, we need to clear the Processed flag
      // else we would be unbale to reacluate costs again.
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      retn = desc_and_add_steps(nodemodel, n, plan);
    }
  } while (retn);

  reset_model(nodemodel);

  plan->steps = lives_list_reverse(plan->steps);
  display_plan(plan);
  return plan;
}


static void plan_step_free(plan_step_t *step) {
  if (step) {
    if (step->deps) lives_free(step->deps);
    lives_free(step);
  }
}


void exec_plan_free(exec_plan_t *plan) {
  if (plan) {
    if (plan->steps) {
      for (LiVESList *list = plan->steps; list; list = list->next) {
        if (list->data) plan_step_free((plan_step_t *)list->data);
      }
    }
    lives_free(plan);
  }
}


void display_plan(exec_plan_t *plan) {
  int stepno = 1;
  if (!plan) return;

  g_print("\n\nDISPLAYING PLAN 0X%016lX created from nodemodel %p\n", plan->uid, plan->model);
  if (plan->state == PLAN_STATE_TEMPLATE)
    g_print("This is a template. It has been used to create %ld cycles\n", plan->iteration);
  else {
    g_print("This is a plan cycle, iteration %ld\n", plan->iteration);
    switch (plan->state) {
    case PLAN_STATE_NONE:
      g_print("Plan cycle is inactive");
      break;
    case PLAN_STATE_RUNNING:
      g_print("Plan cycle is active, running");
      break;
    case PLAN_STATE_FINISHED:
      g_print("Plan cycle is complete");
      break;
    default:
      g_print("Plan cycle encountered an error");
      break;
    }
  }
  g_print("\nStep sequence:\nBEGIN:\n");

  for (LiVESList *list = plan->steps; list; list = list->next) {
    plan_step_t *step = (plan_step_t *)list->data;
    char *memstr;
    g_print("Step %d\n", stepno++);
    switch (step->st_type) {
    case STEP_TYPE_LOAD:
      g_print("LOAD ");
      g_print("into Track %d - source: clip number %d: size %d X %d pal %s (gamma %s)\n",
              step ->track, step->clip, step->fin_width, step->fin_height,
              weed_palette_get_name(step->fin_pal), weed_gamma_get_name(step->fin_gamma));
      break;
    case STEP_TYPE_CONVERT:
      g_print("CONVERT ");
      g_print("layer on Track %d: new size %d X %d, new pal %s, new gamma %s\n",
              step ->track, step->fin_width, step->fin_height,
              weed_palette_get_name(step->fin_pal), weed_gamma_get_name(step->fin_gamma));
      break;
    case STEP_TYPE_APPLY_INST:
      g_print("APPLY INSTANCE ");
      g_print("filter: %s\n", weed_filter_get_name(step->target));
      break;
    default:
      g_print("copy layer\n");
      break;
    }
    switch (step->state) {
    case STEP_STATE_NONE:
      if (plan->state == PLAN_STATE_RUNNING)
        g_print("Step status is standby / waiting");
      else
        g_print("Step status is inactive");
      break;
    case STEP_STATE_RUNNING:
      g_print("Step status is active, running");
      break;
    case STEP_STATE_FINISHED:
      g_print("Step status is complete");
      break;
    default:
      g_print("Step  encountered an error");
      break;
    }
    g_print("\n");
    g_print("Resources:\n");
    memstr = lives_format_storage_space_string(step->start_res[RES_TYPE_MEM]);
    g_print("\tOn entry: %ld threads, %s OR %ld bigblocks\n",
            step->start_res[RES_TYPE_THRD], memstr, step->start_res[RES_TYPE_BBLOCK]);
    lives_free(memstr);
    memstr = lives_format_storage_space_string(step->end_res[RES_TYPE_MEM]);
    g_print("\tOn exit: %ld threads, %s OR %ld bigblocks\n",
            step->end_res[RES_TYPE_THRD], memstr, step->end_res[RES_TYPE_BBLOCK]);
    lives_free(memstr);
    g_print("Timing estimates: start time %.4f msec, duration %.4f msec, deadline %.4f msec\n",
            step->st_time / TICKS_PER_SECOND_DBL * 1000.,
            step->dur / TICKS_PER_SECOND_DBL * 1000.,
            step->ded / TICKS_PER_SECOND_DBL * 1000.);
    g_print("Dependencies: ");
    if (step->st_type == STEP_TYPE_LOAD) g_print("Wait for LAYER_STATUS_PREPARED");
    else {
      if (!step->ndeps) g_print("None");
      else {
        for (int i = 0; i < step->ndeps; i++) {
          int j = 1;
          for (LiVESList *xlist = plan->steps; xlist; xlist = xlist->next) {
            plan_step_t *xstep = (plan_step_t *)xlist->data;
            if (xstep == step->deps[i]) {
              g_print("step %d\n", j);
              break;
	      // *INDENT-OFF*
	    }
	    j++;
	  }}}}
    // *INDENT-ON*
    g_print("\n");
  }
  g_print("END\n");
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

      for (int ni = 0; n->inputs[ni]; ni++) {
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
      break;
    case NODE_MODELS_GENERATOR:
    case NODE_MODELS_FILTER: {
      // set channel palettes and sizes, get actual size after channel limitations are applied.
      // check if weed to reinit
      weed_filter_t *filter = (weed_filter_t *)n->model_for;
      weed_instance_t *inst = weed_instance_obtain(n->model_idx, rte_key_getmode(n->model_idx));
      int nins, nouts;
      weed_channel_t **in_channels = weed_instance_get_in_channels(inst, &nins);
      weed_channel_t **out_channels = weed_instance_get_out_channels(inst, &nouts);
      int i = 0, k, cwidth, cheight;
      boolean is_first = TRUE;

      for (k = 0; k < nins; k++) {
	weed_channel_t *channel = in_channels[k];

	if (weed_channel_is_alpha(channel)) continue;
	if (weed_channel_is_disabled(channel)) continue;

	if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL)
	    == WEED_TRUE) {
	  continue;
	}
	in = n->inputs[k];
	if (in->npals) {
	  pal = in->optimal_pal;
	  cpal = in->cpal = weed_channel_get_palette(channel);
	} else {
	  pal = n->optimal_pal;
	  cpal = n->cpal = weed_channel_get_palette(channel);
	}
	weed_channel_set_gamma_type(channel, n->gamma_type);

	in = n->inputs[i++];

	if (in->flags & NODEFLAGS_IO_SKIP) continue;

	weed_channel_set_palette_yuv(channel, pal, WEED_YUV_CLAMPING_UNCLAMPED,
				     WEED_YUV_SAMPLING_DEFAULT, WEED_YUV_SUBSPACE_YUV);

	cwidth = weed_channel_get_pixel_width(channel);
	cheight = weed_channel_get_height(channel);
	set_channel_size(filter, channel, &in->width, &in->height);

	if (in->inner_width && in->inner_height
	    && (in->inner_width < in->width || in->inner_height < in->height)) {
	  int lbvals[4];
	  lbvals[0] = (in->width - in->inner_width) >> 1;
	  lbvals[1] = (in->height - in->inner_height) >> 1;
	  lbvals[2] = in->inner_width;
	  lbvals[3] = in->inner_height;
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
	weed_channel_t *channel = out_channels[k];

	if (weed_channel_is_alpha(channel) ||
	    weed_channel_is_disabled(channel)) continue;

	out = n->outputs[i++];

	if (out->flags & NODEFLAGS_IO_SKIP) continue;

	if (n->model_type == NODE_MODELS_GENERATOR && is_first) {
	  // this would have been set during ascending phase of size setting
	  // now we can set in sfile
	  lives_clip_t *sfile = (lives_clip_t *)n->model_for;
	  sfile->hsize = out->width;
	  sfile->vsize = out->height;
	}

	is_first = FALSE;

	cpal = weed_channel_get_palette(channel);

	if (out->npals) pal = out->optimal_pal;
	else pal = n->optimal_pal;

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

      if (weed_get_boolean_value(inst, WEED_LEAF_HOST_INITED, NULL) == WEED_FALSE)
	n->needs_reinit = TRUE;

      if (!n->model_inst) n->model_inst = inst;
      else weed_instance_unref(inst);

      lives_freep((void **)in_channels);
      lives_freep((void **)out_channels);
    }
      break;

    case NODE_MODELS_OUTPUT: {
      _vid_playback_plugin *vpp = (_vid_playback_plugin *)n->model_for;

      n->cpal = vpp->palette;
      pal = n->optimal_pal;

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

  // used: // some tracks may be in clip_index, but they never actually connect to anyhting else
  // in this case we can set the clip_index val to -1, and this will umap the clip_ssrc in the track_sources
  used = (boolean *)lives_calloc(nodemodel->ntracks, sizeof(boolean));
  for (int i = 0; i < nodemodel->ntracks; i++) used[i] = FALSE;

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    used[nchain->track] = TRUE;
  }

  for (int i = 0; i < nodemodel->ntracks; i++) {
    if (!used[i]) nodemodel->clip_index[i] = -1;
  }

  // TODO - nodemodel->clip_index, nodemodel->frame_index
  /* nodemodel->layers = map_sources_to_tracks(nodemodel->clip_index, CURRENT_CLIP_IS_VALID */
  /* 					    && !mainw->proc_ptr && cfile->next_event, TRUE); */
  //

  do {
    for (list = nodemodel->node_chains; list; list = list->next) {
      lives_clipsrc_group_t *srcgrp;
      lives_clip_t *sfile;
      full_pal_t pally;
      nchain = (node_chain_t *)list->data;
      n = nchain->first_node;
      if (n->n_inputs) continue;
      sfile = (lives_clip_t *)n->model_for;
      srcgrp = mainw->track_sources[nchain->track];
      if (!srcgrp) {
        // will need to check for this to mapped later, and we will neeed to set apparent_pal
        // layer has no clipsrc, so we need to find all connected inputs and mark as ignore
        for (int no = 0; no < n->n_outputs; no++) {
          output_node_t *out = n->outputs[no];
          inst_node_t *nxt = out->node;
          input_node_t *in = nxt->inputs[out->iidx];
          if (nxt->model_type == NODE_MODELS_FILTER) {
            weed_instance_t *inst = (weed_instance_t *)nxt->model_inst;
            if (inst) {
              weed_channel_t *channel = get_enabled_channel(inst, out->iidx, IN_CHAN);
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
      srcgrp_set_apparent(sfile, srcgrp, pally, n->gamma_type);
      retn = desc_and_align(n, nodemodel);
    }
  } while (retn);

  reset_model(nodemodel);
}


#define HIGH_DISPX_MAX 2.

static void calc_node_sizes(lives_nodemodel_t *nodemodel, inst_node_t *n) {
  // CALCULATE CHANNEL SIZES

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

  double minar = 0., maxar = 0., w0, w1, h0, h1;
  double rmaxw, rmaxh, rminw = 0., rminh = 0.;
  double op_ar, bb_ar, layer_ar;
  double distort = 1., scf = 1.;

  boolean has_non_lb_layer = FALSE, letterbox = FALSE;
  boolean svary = FALSE, is_converter = FALSE;

  int opwidth = nodemodel->opwidth;
  int opheight = nodemodel->opheight;
  int lb_width, lb_height;

  int banding = 0;

  int ni, no;
  int nins = n->n_inputs;
  int nouts = n->n_outputs;
  int new_max_w = 0, new_max_h = 0;

  g_print("CALC in / out sizes\n");

  if (n->flags & NODESRC_ANY_SIZE) svary = TRUE;
  if (n->flags & NODESRC_IS_CONVERTER) is_converter = TRUE;
  if ((mainw->multitrack && prefs->letterbox_mt) || (prefs->letterbox && !mainw->multitrack)
      || (LIVES_IS_RENDERING && prefs->enc_letterbox))
    if (!svary) letterbox = TRUE;

  rminw = rmaxw = 0.;
  rminh = rmaxh = 0.;

  op_ar = (double)opwidth / (double)opheight;

  // get max and min widths and heights for all input layers
  for (ni = 0; ni < nins; ni++) {
    g_print("set input %d\n", ni);

    /* channel = in_channels[ni]; */

    /* // if a in channel is alpha only we do not associate it with an input - these inputs / ouutputs */
    /* // fall outside the layers model */
    /* if (weed_channel_is_alpha(channel)) continue; */

    /* if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE) continue; */

    /* // if a channel is temp disabled this means that it is currently not connected to an active layer */
    /* // we will create an extra input for it, increae n_inputs, but then marni it as IGNORE */
    /* // at a latrer point it may need to be enabled (if we hav more in_tracnis than active inputs) */
    /* // in some circumstances this allows for the new inptu to be enabled and connected to a new source node without */
    /* // having to rebuild the entire model */
    /* // */

    /* if (weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) { */
    /*   //  what we need to here is to find the channel template for the channel, checni the in_count */
    /*   // to find how many repetitions of the tmeplate are in use, then count the current number of channels */
    /*   // if there are fewer specified in the in_count than actually exist, some of them will be marnied as temp_disabled */
    /*   // and will not be connected to anode. If there are more incount than active chans, we can enable som disabled ones */
    /*   // if there are fewer in in count than enabled, we need to diable some */
    /*   // what we should do first is looni in in_Count, create a new count of how many copies of each template then compare */
    /*   // if we need more we can enabel some, if we still need more we need to crate more copies, */
    /*   // but this reuires a reinit and rebuild. */
    /*   // If we havew fewr in in_Count we can diable channels and marni inputs as INGORE */
    /*   // here we assume all this has been done, we will create an extre input, and marni it as ignore */
    /*   n->inputs = (input_node_t **)lives_recalloc(n->inputs, n->n_inputs + 1, */
    /* 						  n->n_inputs, sizeof(input_node_t *)); */
    /*   in = n->inputs[n->n_inputs] = (input_node_t *)lives_calloc(1, sizeof(input_node_t)); */
    /*   in->flags |= NODEFLAG_IO_DISABLED; */
    /*   n->n_inputs++; */
    /*   // added at end, then when we get there, it will be flagged as SNIIP */
    /*   continue; */
    /* } */

    in = n->inputs[ni];

    if (in->flags & NODEFLAGS_IO_SKIP) continue;

    g_print("pt a1\n");

    // get size from prev output
    out = in->node->outputs[in->oidx];
    if (!out) continue;

    g_print("pt a2\n");

    if (!(in->flags & NODEFLAG_IO_FIXED_SIZE)) {
      in->width = out->width;
      in->height = out->height;
      g_print("pt a4\n");

      if (n->model_type == NODE_MODELS_OUTPUT) {
	// if we have a sinni output that tanies any size, it may nevertheless not be able to do letterboxing
	// in this case we do need to adjust the input size to the aspect ratio of the displsay
	_vid_playback_plugin *vpp = (_vid_playback_plugin *)n->model_for;
	if (letterbox && (in->node->model_type != NODE_MODELS_GENERATOR || !prefs->no_lb_gens)
	    && !(vpp->capabilities & VPP_CAN_LETTERBOX)) {
          calc_maxspect(nodemodel->opwidth, nodemodel->opheight, &in->width, &in->height);
	}
	if (!in->width || !in->height) {
	  in->width = nodemodel->opwidth;
	  in->height = nodemodel->opheight;
	}
      }
      g_print("SET in size to %d X %d (%d)\n", in->width, in->height, n->n_outputs);
    }

    if (in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens) {
      // if we have mixed letterboxing / non letterboxin layers, we not this
      // then all layers which dont letterbox will get size opwidth X opheight
      // these layers will then define the lb_size (rather than rmaxw X rmaxh)
      //
      has_non_lb_layer = TRUE;
    }

    // if prevnode has no size set - which can be the case if we have variable size sources
    // then if we have other inputs with size set, we set this to the bounding box size
    // if there are no other inputs with non zero height, then we leave it as zero, and set output to zero

    if (!in->width || !in->height) continue;

    // for a single layer, avoid resizing as long ass possible
    if (n->n_inputs == 1) continue;

    layer_ar = (double)in->width / (double)in->height;

    if (!letterbox) {
      // if we have sizes set and we are not letterboxing, set layer ar to op_ar
      if (prefs->pb_quality == PB_QUALITY_HIGH) {
        // stretch to op_ar unless inner box > display size
        double width = (double)in->width;
        double height = (double)in->height;
        if (height * op_ar > width) width = height * op_ar;
        else height = width / op_ar;
        if (width > rmaxw) rmaxw = width;
        distort *= (width * height) / ((double)in->width * (double)in->height);
      } else {
        if (op_ar > layer_ar) {
          // op_ar is wider
          if (prefs->pb_quality == PB_QUALITY_MED) {
            // stretch to op_ar unless either axis is > display axis
            if (in->width >= opwidth || (double)in->width / op_ar >= (double)opheight) {
              in->height = (int)((double)in->width / op_ar + .5);
            } else {
              in->width = (int)((double)in->height * op_ar + .5);
            }
          }
          if (prefs->pb_quality == PB_QUALITY_LOW) {
            // change a.r. but keep size constant
            double sclf;
            int area = in->width * in->height, new_area;
            in->width = (int)((double)in->height * op_ar + .5);
            new_area = in->width * in->height;
            sclf = sqrt((double)new_area - (double)area);
            in->width = (int)((double)in->width / sclf + .5);
            in->height = (int)((double)in->height / sclf + .5);
          }
        } else if (op_ar < layer_ar) {
          // op_ar is taller
          if (prefs->pb_quality == PB_QUALITY_HIGH) {
            if (in->height >= opheight && (double)in->height * op_ar >= (double)opwidth) {
              in->width = (int)((double)in->height / op_ar + .5);
            } else {
              in->height = (int)((double)in->width * op_ar + .5);
            }
          }
          if (prefs->pb_quality == PB_QUALITY_MED) {
            if (in->height >= opheight || (double)in->height / op_ar >= (double)opwidth) {
              in->width = (int)((double)in->height / op_ar + .5);
            } else {
              in->height = (int)((double)in->width * op_ar + .5);
            }
          }
          if (prefs->pb_quality == PB_QUALITY_LOW) {
            // change a.r. but keep size constant
            double sclf;
            int area = in->height * in->width, new_area;
            in->height = (int)((double)in->width * op_ar + .5);
            new_area = in->height * in->width;
            sclf = sqrt((double)new_area - (double)area);
            in->height = (int)((double)in->height / sclf + .5);
            in->width = (int)((double)in->width / sclf + .5);
          }
        }
        if ((double)in->width > rmaxw) {
          rmaxw = (double)in->width;
          rmaxh = (double)in->height;
        }
        if (rminw == 0. || (double)in->width < rminw) {
          rminw = (double)in->width;
          rminh = (double)in->height;
        }
      }
    } else {
      // letterboxing - depending on banding direction we take either tallest or widest
      // layer as baseline
      if (rminw == 0. || (double)in->width < rminw) rminw = (double)in->width;
      if ((double)in->width > rmaxw) rmaxw = (double)in->width;

      if (rminh == 0. || (double)in->height < rminh) rminh = (double)in->height;
      if ((double)in->height > (double)rmaxh) rmaxh = (double)in->height;

      if (minar == 0 || layer_ar < minar) minar = layer_ar;
      if (layer_ar > maxar) maxar = layer_ar;
    }
  }

  if (rmaxw > 0. && rmaxh > 0.) {
    // we can have cases where all inputs are from srcs with var. size
    // if so then we are going to set node and output sizes to 0
    // and fix this ascending

    // for resize we scaled to op_ar
    // now we want to set all sizes equal

    // for HIGH_Q we will use MAX(op_size, MAX(layer_sizes))
    // for MED_Q we will use MIN(op_size, MAX(layer_sizes))
    // for LOW_Q we will use MIN(op_size, MIN(layer_sizes))

    if (!letterbox) {
      double op_area = (double)opwidth * (double)opheight;
      double max_larea, min_larea;
      rmaxh = rmaxw * op_ar;
      rminh = rminw * op_ar;
      max_larea = rmaxw * rmaxh;
      min_larea = rminw * rminh;
      switch (prefs->pb_quality) {
      case PB_QUALITY_HIGH:
        // we have 2 options, keep width same, increase or reduce height,
        // or keep height same, reduce or increase width
        //
        // ideally we would have input layers close to or equal to screen ar

        // we can expand to op_ar and calc area, then divide by orig layer area
        // the closer this is to 1.0, the less the layer is deformed
        // we also want to allow frames to be a bit larger than op size, but not hugely so.
        // so we can find max of these and then find ratio max(streched_areas) / op_area
        // (if this is < 1.0 we clamp it to 1.0)
        // now we can multiply together all of these values and set a threshold
        //
        // if max_area / op_area > 1.0, we can reduce this to hit the threshold
        // so max_area / op_area * (str product) == thresh --> max / op = thresh / str pod
        // if this value >= 1.0 we are good. Otherwise we force the value to 1.0 by using op size
        if (max_larea > op_area) {
          double val = max_larea / op_area * distort;
          // thresh = C . maxl / opa * d -> C = thresh * opa / maxl / d
          if (val > HIGH_DISPX_MAX) {
            scf = HIGH_DISPX_MAX / val;
            scf = sqrt(scf);
            rmaxw *= scf;
            rmaxh *= scf;
          }
          if (rmaxw > opwidth) {
            opwidth = rmaxw;
            opheight = rmaxh;
          }
        }
        break;
      case PB_QUALITY_MED:
        if (op_area > max_larea) {
          opwidth = rmaxw;
          opheight = rmaxh;
        }
        break;
      case PB_QUALITY_LOW:
        if (op_area > min_larea) {
          opwidth = rminw;
          opheight = rminh;
        }
      }
    } else {
      // we found bounding box rmaxw X rmaxh
      // find 2 new boxes

      // if we have only one in channel, we avoid resizing
      // if we have multiple in channels we will find the bounding box of all layers
      // set this to a.r of op and then resize all layers to this

      // (if letterboxing then opwidht X opheight becomes lb_size, and inner sizes keep layer aspect ratios
      // letterbox size is rmaxw X rmaxh, unless we have layers which do not lettrebox, then they are scaled
      // to opwidht X opheight and  this becomes lb_size

      // test width, maintinaing height and adjusting to ar of op
      w0 = rmaxh * op_ar;
      // test width, maintinaing height and adjusting to ar of op
      h0 = rmaxw / op_ar;
      // in one case one dimension will now be smaller, this is the inner frame
      // in the other it will be larger this is the outer frame
      // or all ar are the same

      w1 = rminh * op_ar;
      h1 = rminw / op_ar;

      // here we will define a new opwidth, opheight
      // this is the size we will set all frames to
      // - ie. lb size for all frames
      switch (prefs->pb_quality) {
      case PB_QUALITY_HIGH: {
        // for high we start with outer box of max size. If we can expand to diplay we do,
        // else we find MAX( boxw / opwidth, boxh / opheight) and this has to be < sqrt(HIGH_DISPX_MAX)
        // this is clamped to max
        //use the op size or inner box, whcihever is largest
        double maxscf = sqrt(HIGH_DISPX_MAX);
        if (rmaxw > (double)opwidth * maxscf) {
          scf = (double)opwidth * maxscf / rmaxw;
          rmaxw *= scf;
          rmaxh *= scf;
        }
        if (rmaxh > (double)opheight * maxscf) {
          scf = (double)opheight * maxscf / rmaxh;
          rmaxw *= scf;
          rmaxh *= scf;
        }
        if (rmaxw < opwidth && rmaxh < opheight) {
          if (w0 > rmaxw)
            scf = (double)opwidth / rmaxw;
          else
            scf = (double)opheight / rmaxh;
          rmaxw *= scf;
          rmaxh *= scf;
        }
        opwidth = (int)rmaxw;
        opheight = (int)rmaxh;
      }
	break;
      case PB_QUALITY_MED:
        // for med we will use max_size, shrunk so it fits in opsize
        if (w0 >= rmaxw) {
          if (w0 > (double)opwidth) scf = (double)opwidth / w0;
          opwidth = (int)(w0 * scf);
          opheight = (int)(rmaxh * scf);
          break;
        }
        if (h0 > (double)opheight) scf = (double)opheight / h0;
        opwidth = (int)(rmaxw * scf);
        opheight = (int)(h0 * scf);
        break;
      case PB_QUALITY_LOW:
        // for low we will use outer box of min vals, or inner box of max vals, whichever is smaller
        // if both are > op size, we use inner box of min vals or op size, whichever issmaller
        if (w1 > rminw) {
          if (w1 <= (double)opwidth && rminh <= (double)opheight && rminh < h0) {
            opwidth = (int)w1;
            opheight = (int)rminh;
            break;
          }
        }
        if (h1 > rminh) {
          if (h1 <= (double)opheight && rminw <= (double)opwidth && rminw < w0) {
            opwidth = (int)rminw;
            opheight = (int)h1;
            break;
          }
        }
        if (w0 < rmaxw) {
          if (w0 <= (double)opwidth && rmaxh <= (double)opheight) {
            opwidth = (int)w0;
            opheight = (int)rmaxh;
            break;
          }
        }
        if (h0 < rmaxh) {
          if (h0 <= (double)opheight && rmaxw <= (double)opwidth) {
            opwidth = (int)rmaxw;
            opheight = (int)h0;
            break;
          }
        }
        break;
      default: break;
      }
    }

    for (ni = 0; ni < n->n_inputs; ni++) {
      in = n->inputs[ni];

      if (in->flags & NODEFLAGS_IO_SKIP) continue;

      if (!letterbox || svary) {
        // if not letterboxing, all layers will be resized to new opwidth
        in->width = opwidth;
        in->height = opheight;
      } else {
        // when letterboxing, all layers that implement it keep their original a.r. but may shrink or expand
        // we know ar of opwidth / opheight and ar of rmaxw, rmaxh and rminw, rminh
        // when letterboxing, we want to opwidth / opheith and fit rmaxw, rmax inside it
        // but we are not going to resize layers to opwidth X opheit, instead we will fit the extended bb inside it
        // the bb extends when we expand some layers to all havam rmaxw eidth or all to have rmaxh heigth
        // then we take the expanded bbox and try to fit this in output
        //  if we had a very tall bbox to start with and width did not change, then we cannt use the max width
        // and hve height fit in op. If we have very thin layers in each direction, then when expanding
        // to max width or height, the orer dimension gets very large no matter which way round we do it

        //
        // we calculate - set all layer heigths to max height and get expanded widdth
        // versus set all to max width and calc ne hithg
        // then check ratios - proportion of op frame filled by baning axis * a wigth
        // , then for each laye calc ratio of non max direction as opwidth . reduc use wighting
        // calc for each layer width layer / full with (or height), multiply toget with bangin proportion * wieghting,
        // find which give higher total
        // and a thrid ratio which is op size / expanded value * another weighting
        // also need to factor in how much size reduction
        // each layer gets expanded, thenthis is recuded, so multiplying togetr then  divide by maxed dimension
        // so three ratios - redced dimension / op, for each layer, final expanded dmiension . max expanded,
        // and expanded dim. strectch over redux
        // the first tesll how much of op is banded
        // the second the internal banding,
        // and third how much the layers get rudced by due to a wide / tall outlier layer
        // eg we have one very wide layer, we should band op horizontally
        // unless we halso have on very tall layer outlier
        // this woudl be worst case option 1 vey wide laye and 1 very tall
        // another consideration we want to avoid constanly switching from h bands
        // to v bands as layers are added or q value alters
        //
        // if we have one row like layer and another very large square layerwe should avoid vertical banding since
        // the row like layer would expand vertially and exttend ven more horiozontally
        // then shrunk to fit in op which would , the bb expands less so is less
        // reduced, the big layer epand to the wo width then all cut down to op width, maning nothign gets very reduced
        // thus exp size / opsize is important for this
        // ratio of layer in lb axis would be small but we should discount originsal opproportion
        //
        // thus most important is max w or h / lare w or h eninon fixed direction
        // second cosideration is ratio of op willed
        // least important in tra lyer proportion
        // so calculate both ways, see which is best. also if op ar > 1 we prefer horiz  (cinemasocpe)
        //
        //
        // set all heights to opheight,
        // calc new width rmax
        // shirnk / expand to  opwidth. for each layer then, find how much reduction this is expand / div shrin * width
        // this is just a constant now so it just depends on

        // summary: we found rmaw, rmaxh, rminw, rminh
        // calulated a box of size depending on quality
        // if not letterboxing, all channels get set to this

        // for letterboxing, we go back to rmaxw, rmaxh, and construct the opsize around it
        // then if opbox is taller, we are going to add hbands, if wider, vbands
        // if adding hbands, we want to set all layers to have rmaxh. This will expanf rmaxw
        // we then construct anothe op box around this, and then if it is > screen size, shrink it down
        // for hi we use the  final box as is, but 2 sides must hit orig rmaxw or rmaxh
        //
        // for mid we are going to shrink it down to fit in orig op size,
        //
        // for low we shrink it down so two sides touch rminh rminw

        // get new opbox

        bb_ar = rmaxw / rmaxh;

        if (prefs->pb_quality == PB_QUALITY_LOW) {
          if (opwidth <= rmaxw && opheight <= rmaxh) {
            if (opwidth / bb_ar > opheight) {
              rmaxw = opwidth;
              rmaxh = rmaxw / bb_ar;
            }
            if (opheight * bb_ar > opwidth) {
              rmaxh = opheight;
              rmaxw = rmaxh * bb_ar;
            }
          }
        } else {
          if (opwidth < rmaxw) {
            opwidth = rmaxw;
            opheight = rmaxw / op_ar;
          }
          if (opheight < rmaxh) {
            opheight = rmaxh;
            opwidth = rmaxh * op_ar;
          }
        }
      }
    }

    if (letterbox && !svary) {
      scf = 1.;

      if (opwidth > rmaxw) banding = 1;

      for (ni = 0; ni < nins; ni++) {
        int width, height;
        in = n->inputs[ni];
        if (in->flags & NODEFLAGS_IO_SKIP) continue;

        if (rmaxw > 0. && in->node->model_type == NODE_MODELS_GENERATOR && prefs->no_lb_gens)
          continue;

        width = in->width;
        height = in->height;

        if (banding == 0) {
          width = (int)((double)width * rmaxh / (double)height + .5);
          if (width > new_max_w) new_max_w = width;
          height = (int)(rmaxh + .5);
        } else {
          height = (int)((double)height * rmaxw / (double)width + .5);
          if (height > new_max_h) new_max_h = height;
          width = (int)(rmaxw + .5);
        }
        // set inner_width, inner_height
        in->inner_width = width;
        in->inner_height = height;
      }

      bb_ar = new_max_w / rmaxh;

      if (banding == 0) {
        rmaxw = new_max_w;
        if (prefs->pb_quality == PB_QUALITY_MED) {
          if (rmaxw > (double)nodemodel->opwidth) scf = rmaxw / (double)nodemodel->opwidth;
        }
        if (prefs->pb_quality == PB_QUALITY_LOW) {
          if (rmaxw > (double)opwidth) scf = rmaxw / (double)opwidth;
        }
      } else {
        rmaxh = new_max_h;
        if (prefs->pb_quality == PB_QUALITY_MED) {
          if (rmaxh > (double)nodemodel->opheight) scf = rmaxh / (double)nodemodel->opheight;
        }
        if (prefs->pb_quality == PB_QUALITY_LOW) {
          if (rmaxh > (double)opheight) scf = rmaxh / (double)opheight;
        }
      }

      if (scf != 1.) {
        rmaxw /= scf;
        rmaxh /= scf;
        /* opwidth = (int)((double)opwidth * scf + .5); */
        /* opheight = (int)((double)opheight * scf + .5); */
      }

      if (has_non_lb_layer) {
        lb_width = opwidth;
        lb_height = opheight;
      } else {
        lb_width = (int)(rmaxw + .5);
        lb_height = (int)(rmaxh + .5);
      };

      lb_width = (lb_width >> 2) << 2;
      lb_height = (lb_height >> 2) << 2;

      opwidth = (opwidth >> 2) << 2;
      opheight = (opwidth >> 2) << 2;

      for (ni = 0; ni < nins; ni++) {
        in = n->inputs[ni];
        if (in->flags & NODEFLAGS_IO_SKIP) continue;

        if (rmaxw > 0. && in->node->model_type == NODE_MODELS_GENERATOR
            && prefs->no_lb_gens) {
          in->width = in->inner_width = opwidth;
          in->height = in->inner_height = opheight;
          continue;
        }

        in->inner_width = (int)(scf * (double)in->inner_width + .5);
        in->inner_height = (int)(scf * (double)in->inner_height + .5);
        in->inner_width = (in->inner_width << 2) >> 2;
        in->inner_height = (in->inner_height << 2) >> 2;
        in->width = lb_width;
        in->height = lb_height;
      }
    }
  }

  for (no = 0; no < nouts; no++) {
    out = n->outputs[no];

    if (is_converter && svary) {
      /// resizing - use the value we set in channel template
      /// this allows us to, for example, resize the same in_channel to multiple out_channels at various sizes
      weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, no, NULL, OUT_CHAN);
      int width = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
      int height = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
      out->width = width;
      out->height = height;
    } else {
      if (svary && no < n->n_inputs) {
        in = n->inputs[no];
        out->width = in->width;
        out->height = in->height;
      } else {
        if (!(out->flags & NODEFLAG_IO_FIXED_SIZE)) {
          out->width = n->inputs[0]->width;
          out->height = n->inputs[0]->height;
        }
      }
    }
  }
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

  g_print("prepend node %p to %p for track %d\n", n, target, track);

  // locate output in n to connect from

  for (i = 0; i < n->n_outputs; i++) {
    if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
    if (n->outputs[i]->track == track) {
      g_print("found output idx %d\n", i);
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
    if (target->inputs[i]->track == track) {
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

  g_print("ADD output node %p as %d for %p\n", target, op_idx, n);
  out = n->outputs[op_idx];
  out->node = target;
  out->iidx = ip_idx;

  g_print("VALLL %d and %d\n", main_idx, op_idx);
  if (main_idx != -1 && main_idx != op_idx) {
    out->origin = main_idx;
    out->flags |= NODEFLAG_IO_CLONE;
    //
  } else {
    // if n is a filter, set the details for the src output
    // for other source types (layer, blank), the outputs will produce whatever the input requires
    switch (n->model_type) {
    case NODE_MODELS_SRC:
      out->flags |= NODESRC_ANY_SIZE;
      out->flags |= NODESRC_ANY_PALETTE;
      break;

    case NODE_MODELS_CLIP: {
      // for clip_srcs
      for (i = 0; n->inputs[i]; i++) {
        n->inputs[i]->best_src_pal = (int *)lives_calloc(N_COST_TYPES * n_allpals, sizint);
        n->inputs[i]->min_cost = (double *)lives_calloc(N_COST_TYPES * n_allpals, sizdbl);
      }
      n->npals = n_allpals;
      n->pals = allpals;
    }
      break;

    case NODE_MODELS_FILTER: {
      weed_filter_t *filter = (weed_filter_t *)n->model_for;
      weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, op_idx, NULL, OUT_CHAN);
      int *pals, npals;
      boolean pvary = weed_filter_palettes_vary(filter);
      int sflags = weed_chantmpl_get_flags(chantmpl);
      int filter_flags = weed_filter_get_flags(filter);

      if (sflags & WEED_CHANNEL_CAN_DO_INPLACE) {
        weed_chantmpl_t *ichantmpl = get_nth_chantmpl(n, op_idx, NULL, IN_CHAN);
        if (ichantmpl) {
          n->inputs[op_idx]->flags |= NODESRC_INPLACE;
        }
      }

      pals = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);
      for (npals = 0; pals[npals] != WEED_PALETTE_END; npals++);

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

      if (!n->n_inputs) {
        // generator
        // check gamma_type of output
        if (prefs->apply_gamma) {
          if (filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA)
            n->gamma_type = WEED_GAMMA_LINEAR;
          else
	    n->gamma_type = WEED_GAMMA_SRGB;
	  // *INDENT-OFF*
	}}}
  // *INDENT-ON*
  break;
// OUTPUT, INTERNAL should have no out chans, and in any case they dont get
// prepended to anything !
  default: break;
  }
}

in = target->inputs[ip_idx] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));

in->node = n;
in->oidx = op_idx;

npals = target->npals;
if (in->npals) npals = in->npals;

//in->min_cost = (double *)lives_calloc(N_COST_TYPES * npals, sizdbl);

in = target->inputs[ip_idx];

switch (target->model_type) {
case NODE_MODELS_FILTER: {
  weed_filter_t *filter = (weed_filter_t *)target->model_for;
  weed_chantmpl_t *chantmpl = get_nth_chantmpl(n, op_idx, NULL, OUT_CHAN);
  int sflags = weed_chantmpl_get_flags(chantmpl);
  boolean svary = weed_filter_channel_sizes_vary(filter);
  boolean pvary = weed_filter_palettes_vary(filter);

  if (sflags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
    in->flags |= NODESRC_REINIT_SIZE;
  if (sflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
    in->flags |= NODESRC_REINIT_PAL;
  if (sflags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
    in->flags |= NODESRC_REINIT_RS;

  pals = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);
  for (npals = 0; pals[npals] != WEED_PALETTE_END; npals++);

  if (pvary) {
    out->pals = pals;
    out->npals = npals;
  } else {
    n->pals = pals;
    n->npals = npals;
  }

  if (svary) in->flags |= NODESRC_ANY_SIZE;

  for (i = ip_idx + i; i < n->n_inputs; i++) {
    // mark any clone inputs
    if (n->inputs[i]->track == track) {
      n->inputs[i]->flags |= NODEFLAG_IO_CLONE;
      n->inputs[i]->origin = ip_idx;
    }
  }
}
break;

case NODE_MODELS_OUTPUT: {
  _vid_playback_plugin *vpp = (_vid_playback_plugin *)n->model_for;
  int *xplist;
  int npals = 1;
  int cpal = mainw->vpp->palette;
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
  in->width = nodemodel->opwidth;
  in->height = nodemodel->opheight;
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
    in->width = nodemodel->opwidth;
    in->height = nodemodel->opheight;
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
    if (!in_chain) return track;

    if (in_chain->terminated) {
      // if we do have a node_chain for the track, but it is terminated
      // then we are going to back up one node and a clone output
      // this means that the track sources will fork, one output will go to
      // the terminator node, while the other output will be a layer copy
      // creating a new node_chain for this track,
      // which will be an input to this node
      in_chain = fork_output(nodemodel, in_chain->last_node, n, track);

      if (!in_chain) {
        // this means something has gone wrong, eitehr we found an unterminated node_chain
        // which should have prepend before the teminated one
        // if this is the cae it means eithe the node_chains are out of order, or
        // a chain was terminated, whilst another unerminated node_chain existed
        // - it should not be possible to add a new node_chain for a track when
        // there is still an exisitn unterminated one - we check for this when adding and the code should
        // check for this as well before creating a new node_chain
        // OR, we failed to find an input in this node for 'track'
        // or we failed to find an existtng output
        abort();
      }
    }

    // here we found an unterminated chain - either from a source just added, or a forekd output
    // or else from the normal output from another instance
    prepend_node(nodemodel, in_chain->last_node, track, n);
    in_chain->last_node = n;

    for (j = 0; j < n->n_outputs; j++) {
      if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
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
calc_node_sizes(nodemodel, n);

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
int i, n_clip_srcs = 0;

if (!n) return NULL;

n->idx = node_idx++;
n->model_type = model_type;
n->model_for = model_for;

if (1) {
  char *nname = get_node_name(n);
  g_print("created node %p, type %s with %d ins aand %d outs\n", n, nname, nins, nouts);
  lives_free(nname);
}

// allocate input and output subnodes, but do not set the details yet
// for efficiency we only do this when a connection is made

if (n->model_type == NODE_MODELS_CLIP) {
  // for clip models, we create an input for each lip_src, bur do not count thes in n->n_inputs
  // instead we create teh inputs with an extra NULL input at the end
  n_clip_srcs = nins;
} else n->n_inputs = nins;

if (nins) {
  n->inputs = (input_node_t **)lives_calloc(n_clip_srcs ? nins + 1 : nins, sizeof(input_node_t *));
  for (i = 0; i < nins; i++) {
    n->inputs[i] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));
    if (in_tracks) n->inputs[i]->track = in_tracks[i];
  }
  if (n_clip_srcs) n->inputs[i] = NULL;
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
  } else n->gamma_type = WEED_GAMMA_SRGB;
}
break;

case NODE_MODELS_FILTER: {
  weed_filter_t *filter = (weed_filter_t *)n->model_for;

  if (weed_filter_prefers_linear_gamma(filter)) {
    n->flags |= NODESRC_LINEAR_GAMMA;
    n->gamma_type = WEED_GAMMA_LINEAR;
  } else n->gamma_type = WEED_GAMMA_SRGB;

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
return n;
}


// TODO - check if we need to free pals

static void free_inp_nodes(int ninps, input_node_t **inps) {
for (int i = 0; i < ninps; i++) {
  if (inps[i]) {
    if (inps[i]->best_src_pal) lives_free(inps[i]->best_src_pal);
    if (inps[i]->min_cost) lives_free(inps[i]->min_cost);
    if (inps[i]->cdeltas) lives_list_free_all(&inps[i]->cdeltas);
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
    lives_free(outs[i]);
  }
}
}


static void free_node(inst_node_t *n) {
if (n) {
  if (n->model_type == NODE_MODELS_FILTER)
    if (n->model_inst) weed_instance_unref((weed_instance_t *)n->model_inst);
  if (n->n_inputs || n->n_clip_srcs) {
    free_inp_nodes(n->n_inputs ? n->n_inputs : n->n_clip_srcs, n->inputs);
    lives_free(n->inputs);
  }
  if (n->n_inputs) {
    free_outp_nodes(n->n_outputs, n->outputs);
    lives_free(n->outputs);
  }
  //
  //
  if (n->free_pals && n->pals) lives_free(n->pals);
  //
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


static LiVESList *_add_sorted_cdeltas(LiVESList * cdeltas, int out_pal, int in_pal,
                                    double * costs,  int sort_by, boolean replace,
                                    int out_gamma_type, int in_gamma_type) {
// update cdeltas for an input
// if replace is set we find an old entry to remove and re-add
// sort_by is the cost type to order the cdeltas

LiVESList *list = NULL, *listend = NULL;
cost_delta_t *cdelta;

if (cdeltas) list = cdeltas;

if (replace) {
  for (list = cdeltas; list; list = list->next) {
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
    lives_list_free(list);
  }
}

cdelta = (cost_delta_t *)lives_calloc(1, sizeof(cost_delta_t));
cdelta->out_pal = out_pal;
cdelta->in_pal = in_pal;

for (int i = 0; i < N_COST_TYPES; i++) {
  cdelta->deltas[i] = costs[i];
}

cdelta->out_gamma_type = out_gamma_type;
cdelta->in_gamma_type = in_gamma_type;

for (list = cdeltas; list; list = list->next) {
  cost_delta_t *xcdelta = (cost_delta_t *)list->data;
  if (xcdelta->deltas[sort_by] > cdelta->deltas[sort_by]) {
    cdeltas = lives_list_prepend(list, (void *)cdelta);
    break;
  }
  listend = list;
}
if (!list) cdeltas = lives_list_append(listend, (void *)cdelta);

return cdeltas;
}


static void calc_costs_for_source(inst_node_t *n, int track, double * factors,
                                boolean have_trk_srcs, boolean ghost) {
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
lives_clipsrc_group_t *srcgrp = NULL;
lives_clip_t *sfile;
double *costs;
int i, j, k, ni, no;

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

if (have_trk_srcs) srcgrp = mainw->track_sources[track];

for (j = 0; j < n_allpals; j++) {
  // step through list of ALL palettes
  double ccost = 0;
  for (ni = 0; n->inputs[ni]; ni++) {
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
    // combo with all ouy_pals for clipsrc
    // generally we only have a single palette choice for clip_srcs
    for (i = 0; i < in->npals; i++) {
      // find costs to convert from width, height, ipal gamma_type to
      // owidth, oheight, opal, ogamma_type
      for (k = 0; k < N_COST_TYPES; k++) {
        double cost;
        if (k == COST_TYPE_COMBINED) continue;
        // we dont know th out_size, so we assume th sfile size
        // or in gamma type
        cost = get_conversion_cost(k, owidth, oheight, owidth, oheight,
                                   in->pals[i], allpals[j], allpals, n->gamma_type, n->gamma_type, FALSE);
        if (k == COST_TYPE_QLOSS_P) {
          // we will only have cpal if we have fully defined track_sources
          // i.e. if map_sources_to_tracks was called with the current clip_index
          cost += gcost - cost * gcost;
          // TODO - do same for clipsrc cpal ->
        }

        if (!i || cost < in->min_cost[j * N_COST_TYPES + k]) {
          in->min_cost[j * N_COST_TYPES + k] = cost;
          in->best_src_pal[j * N_COST_TYPES + k] = i;
        }
        ccost += cost * factors[k];
      }
      if (!i || ccost < in->min_cost[j * N_COST_TYPES + COST_TYPE_COMBINED]) {
        in->min_cost[j * N_COST_TYPES + COST_TYPE_COMBINED] = ccost;
        in->best_src_pal[j * N_COST_TYPES + COST_TYPE_COMBINED] = i;
      }
    }
    for (k = 0; k < N_COST_TYPES; k++) {
      costs[k] += in->min_cost[j * N_COST_TYPES + k] * in->f_ratio;
    }
  }
  // we now have costs for each cost type for this palette j
  for (no = 0; no < n->n_outputs; no++) {
    output_node_t *out = n->outputs[no];
    out->cdeltas = _add_sorted_cdeltas(out->cdeltas, j, -1, costs, COST_TYPE_COMBINED, FALSE,
                                       n->gamma_type, n->gamma_type);
  }
}
lives_free(costs);
}


static cost_delta_t *find_cdelta(LiVESList * cdeltas, int out_pal, int in_pal, int out_gamma_type, int in_gamma_type) {
cost_delta_t *cdelta = NULL;
for (LiVESList *list = cdeltas; list; list = list->next) {
  cdelta = (cost_delta_t *)list->data;
  if (cdelta->out_pal == out_pal
      && (in_pal == WEED_PALETTE_ANY || cdelta->in_pal == in_pal)) {
    // TODO - check gammas
    break;
  }
}
return cdelta;
}


#define _FLG_GHOST_COSTS	1
#define _FLG_ORD_DESC		2
#define _FLG_TRK_SRCS		4

#define _FLG_ENA_GAMMA		16

static void _calc_costs_for_input(inst_node_t *n, int idx, int *pal_list, int j, int flags,
                                double * glob_costs, boolean * glob_mask, double * factors) {
// calc costs for in converting to pal_list[j]
weed_filter_t *filter;
input_node_t *in = n->inputs[idx];
inst_node_t *p = in->node;
output_node_t *out = p->outputs[in->oidx];
double *costs, *srccosts = NULL;
int npals, *pals;
boolean clone_added = FALSE;
boolean ghost = FALSE;//, enable_gamma = FALSE;
boolean have_trk_srcs = FALSE;
int owidth, oheight;
int out_gamma_type = WEED_GAMMA_UNKNOWN, in_gamma_type = WEED_GAMMA_UNKNOWN;

if (n->model_type == NODE_MODELS_FILTER)
  filter = (weed_filter_t *)n->model_for;

// if p is an input, we dont really have palettes, we have composit clip srcs

if (flags & _FLG_TRK_SRCS) have_trk_srcs = TRUE;
if (flags & _FLG_GHOST_COSTS) ghost = TRUE;
//if (flags & _FLG_ENA_GAMMA) enable_gamma = TRUE;

if (p->n_inputs && out->npals) {
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
    if (!out->cdeltas)
      calc_costs_for_source(p, in->track, factors, have_trk_srcs, ghost);
    xcdelta = find_cdelta(out->cdeltas, WEED_PALETTE_ANY, pals[i], WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
    srccosts = xcdelta->deltas;
  }

  // find cost_delta. This is computed according to
  // cost_type, in_pal, out_pal, out_pal_list, in size and out size

  for (int k = 0; k < N_COST_TYPES; k++) {
    double delta_cost = 0.;
    if (glob_mask[k]) costs[k] = glob_costs[k];
    else {
      if (k == COST_TYPE_TIME) {
        if (k == COST_TYPE_COMBINED) continue;
        delta_cost = get_conversion_cost(k, out->width, out->height, in->width, in->height,
                                         pals[i], pal_list[j], pal_list, out_gamma_type, igamma_type, ghost);
        // add on the (possibly palette dependent) processing time for the previous node
        // if the connected output has its own palette list, then we cannot determine the palette
        // dependent processing times
        if (k == COST_TYPE_TIME && !out->npals)
          delta_cost += get_proc_cost(COST_TYPE_TIME, filter, owidth, oheight, pals[i]);
      }
      // TODO: misc_costs

      if (srccosts) {
        // if we are pulling from a clip src, we triangluate fro clip_srcs to apparent_pal to in_pal
        // and we will join both cost
        if (k == COST_TYPE_QLOSS_P)
          delta_cost += srccosts[k] - delta_cost * srccosts[k];
        else delta_cost += srccosts[k];
      }
      costs[k] = delta_cost;
    }
    if (k == COST_TYPE_COMBINED) continue;
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
      xin->cdeltas = _add_sorted_cdeltas(xin->cdeltas, j, i, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
                                         igamma_type);
    }
  }

  // combined cost here is just an intial extimate, since we don't yet have absolute / max tcost
  costs[COST_TYPE_COMBINED] = ccost;
  // add to cdeltas for input, sorted by increasing ccost
  in->cdeltas = _add_sorted_cdeltas(in->cdeltas, j, i, costs, COST_TYPE_COMBINED, FALSE, out_gamma_type,
                                    igamma_type);
}

lives_free(costs);
if (flags & _FLG_ORD_DESC) in->cdeltas = lives_list_reverse(in->cdeltas);
}


static void compute_all_costs(inst_node_t *n, int ord_ctype, double * factors, int flags) {
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
  if (npals) {
    pal_list = in->pals;
  } else {
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
      _calc_costs_for_input(n, ni, pal_list, j, flags, glob_costs, glob_mask, factors);
      // *INDENT-OFF*
  }}
// *INDENT-ON*
}


static LiVESList *add_sorted_tuple(LiVESList * priolist, cost_tuple_t *tup, int ord_type) {
LiVESList *lastl = NULL, *list;
double mycost = tup->tot_cost[ord_type];
for (list = priolist; list; list = list->next) {
  cost_tuple_t *xtup = (cost_tuple_t *)list->data;
  if (xtup->tot_cost[ord_type] > mycost) {
    priolist = lives_list_prepend(list, (void *)tup);
    break;
  }
  lastl = list;
}
if (!list) {
  priolist = lives_list_append(lastl, (void *)tup);
}
return priolist;
}


static cost_tuple_t *best_tuple_for(LiVESList * priolist, inst_node_t *n, int cost_type) {
cost_tuple_t *mintup = NULL;
double minval = 0;
for (LiVESList *list = priolist; list; list = list->next) {
  cost_tuple_t *xtup = (cost_tuple_t *)list->data;

  // we may have some out / in palettes fixed. If so we need to check the values in the tuple for the corresponding
  // cost_type and reject it
  conversion_t *palconv = xtup->palconv;
  for (int ni = 0; ni < n->n_inputs; ni++) {
    if (n->inputs[ni]->best_out_pal[cost_type] != WEED_PALETTE_NONE
        && palconv[ni].out_pal != n->inputs[ni]->best_out_pal[cost_type]) continue;

    if (n->inputs[ni]->best_in_pal[cost_type] != WEED_PALETTE_NONE
        && palconv[ni].in_pal != n->inputs[ni]->best_in_pal[cost_type]) continue;

    if (!mintup || xtup->tot_cost[cost_type] < minval) {
      minval = xtup->tot_cost[cost_type];
      mintup = xtup;
    }
  }
}
return mintup;
}


static void free_tuple(cost_tuple_t *tup) {
if (tup->palconv) lives_free(tup->palconv);
lives_free(tup);
}


static void free_priolist(LiVESList * plist) {
for (LiVESList *list = plist; list; list = list->next) {
  free_tuple((cost_tuple_t *)list->data);
}
lives_list_free(plist);
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
double max[N_COST_TYPES];

if (!sumty) {
  // set summation methods and def values for cost types
  sumty = TRUE;
  cost_summation[COST_TYPE_TIME] = SUM_MAX;
  cost_summation[COST_TYPE_QLOSS_P] = SUM_MPY;
  cost_summation[COST_TYPE_QLOSS_S] = SUM_LN_ADD;
}

for (int k = 0; k < N_COST_TYPES; k++) {
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
    if (cost_summation[k] == SUM_LN_ADD) {
      out_costs[k] += abs_costs[ni][k] + log(abs_costs[ni][k]) + costs[ni][k];
    }
    if (cost_summation[k] & SUM_MPY) {
      // want here; 1 - (1 - a) * (1 - b) == 1 - (1 - b - a + ab) == a + b - ab
      // eg a = .1, b = .15,  1 - .9 * 85 == .9 + .8 - .9 * .8
      out_costs[k] += abs_costs[ni][k] - out_costs[k] * abs_costs[ni][k];
      out_costs[k] += costs[ni][k] - out_costs[k] * costs[ni][k];
    }
  }
}

for (int k = 1; k < N_COST_TYPES; k++) {
  out_costs[COST_TYPE_COMBINED] += factors[k] * out_costs[k];
}

return out_costs;
}



static cost_tuple_t *make_tuple(inst_node_t *n, conversion_t *conv, double * factors) {
// conv is array of n->n_inputs, get in+pal, out+pal, find cdelta at input
// get deltas and copy to tuple; if in->node has abs_costs, add these on
cost_tuple_t *tup = (cost_tuple_t *)lives_calloc(1, sizeof(cost_tuple_t));
double **costs, **oabs_costs;
cost_delta_t *cdelta;
if (!n->n_inputs) return NULL;

costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
oabs_costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));

tup->node = n;
for (int ni = 0; ni < n->n_inputs; ni++) {
  int ipal, opal;
  input_node_t *in = n->inputs[ni];
  inst_node_t *p;
  if (in->flags & NODEFLAGS_IO_SKIP) continue;

  opal = conv[ni].out_pal;
  ipal = conv[ni].in_pal;
  cdelta = find_cdelta(in->cdeltas, opal, ipal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
  costs[ni] = cdelta->deltas;
  p = in->node;
  oabs_costs[ni] = p->abs_cost;
}

total_costs(n, n->n_inputs, oabs_costs, costs, factors, tup->tot_cost);
lives_free(oabs_costs); lives_free(costs);
return tup;
}


static LiVESList *backtrack(inst_node_t *n, int ni, double * factors) {
// we have calculated tcost deltas and qloss deltas for each in / out pair
// we now create tuples, checking all combos for all inputs
// then combining these, we find the cost for the node
// then for each cost type, we find the tuple that gives the lowest delta

static int common = WEED_PALETTE_NONE;
static conversion_t *conv = NULL;
static LiVESList *priolist = NULL;

input_node_t *in;
int npals, *pals;

if (!conv) {
  priolist = NULL;
  conv = (conversion_t *)lives_calloc(n->n_inputs, sizeof(conversion_t));
}

if (ni >= n->n_inputs) {
  cost_tuple_t *tup = make_tuple(n, conv, factors);
  priolist = add_sorted_tuple(priolist, tup, COST_TYPE_COMBINED);
  return priolist;
}

in = n->inputs[ni];

if (in->npals > 0) {
  npals = in->npals;
  pals = in->pals;
} else {
  npals = n->npals;
  pals = n->pals;
}

for (int j = 0; j < npals; j++) {
  conversion_t *myconv;
  output_node_t *out;
  int *opals;
  int onpals;
  inst_node_t *p = in->node;
  myconv = &conv[ni];
  out = p->outputs[in->oidx];

  if (out->npals) {
    // if out palettes can vary, the palette is usually set by the host
    // or by the filter, however we can still calculate it
    onpals = out->npals;
    opals = out->pals;
  } else {
    inst_node_t *p = in->node;
    onpals = p->npals;
    opals = p->pals;
  }

  // we already calculated costs for each in / out palette, these are in cdeltas for the input
  if (!ni) common = WEED_PALETTE_NONE;

  if (!in->npals) {
    if (common == WEED_PALETTE_NONE) common = j;
    else j = npals = common;
  }

  myconv->in_pal = pals[j];

  for (ni++; ni < n->n_inputs; ni++) {
    in = n->inputs[ni];
    if (!(in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE))) break;
  }

  for (int i = 0; i < onpals; i++) {
    if (out->flags & NODESRC_ANY_PALETTE) {
      myconv->out_pal = myconv->in_pal;
      i = onpals;
    } else myconv->out_pal = opals[i];
    priolist = backtrack(n, ni, factors);
    // TODO - add gammas
  }
}
conv = NULL;
return priolist;
}


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

if (oper == 1) {
  // recalc costs, but now with abs times - this is same as 0, but we do not ascend yet
  ascend_tree(n, -1, factors, flags);
}

depth++;

//g_print("trace to src %d %d\n", depth, n->n_inputs);

if (oper == 2) {
  if (n->flags & NODESRC_ANY_SIZE) svary = TRUE;
}

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
      if (n->model_type != NODE_MODELS_CLIP) {
        // for a clip source the output pal will b the same as next node input
        // so we do not touch that
        // here we could use in->prev-pal, but that is optimal for onl thatinput, instead we use
        // best_out_pal, whcih is the optimal value considering all inputs together
        for (k = 0; k < N_COST_TYPES; k++) {
          if (in->npals) pal = in->best_out_pal[k];
          else pal = out->node->best_pal_up[k];
          out->best_out_pal[k] = pal;
          if (is_first) n->best_pal_up[k] = pal;
        }
        out->optimal_pal = out->best_out_pal[COST_TYPE_COMBINED];
        if (is_first) n->optimal_pal = n->best_pal_up[COST_TYPE_COMBINED];
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
        out->width = in->width;
        out->height = in->height;
        if (!width) {
          width = in->width;
          height = in->height;
        }
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
        if (in->npals) in_pal = in->optimal_pal;
        else in_pal = n->optimal_pal;
        if (out->npals) out_pal = out->optimal_pal;
        else out_pal = p->optimal_pal;
        cdelta = find_cdelta(in->cdeltas, out_pal, in_pal, p->gamma_type, n->gamma_type);
        slack = (double)(n->ready_ticks - p->ready_ticks) / TICKS_PER_SECOND_DBL
                - cdelta->deltas[COST_TYPE_TIME];
        out->slack = slack;
        out->tot_slack = slack + minslack;
        // when optimising, we can "spend" slack to increase tcost, without increasing combined
        // cost at the sink
      }
      break;

      case 2:
        if (n->n_outputs) {
          if (svary && ni < n->n_outputs) {
            in->width = n->outputs[ni]->width;
            in->height = n->outputs[ni]->height;
          } else {
            in->width = width;
            in->height = height;
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
            in->cdeltas = _add_sorted_cdeltas(in->cdeltas, WEED_PALETTE_NONE, WEED_PALETTE_NONE,
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
          out->cdeltas = _add_sorted_cdeltas(out->cdeltas, WEED_PALETTE_NONE, WEED_PALETTE_NONE,
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
      for (ni = 0;; ni++) {
        in = n->inputs[ni];
        if (!in) break;
        for (int k = 0; k < N_COST_TYPES; k++) {
          int tpal = n->best_pal_up[k];
          in->best_in_pal[k] = tpal;
          in->best_out_pal[k] = in->best_src_pal[tpal * N_COST_TYPES + k];
        }
        // this is the palette we will use for the clip_src
        in->optimal_pal = in->best_out_pal[COST_TYPE_COMBINED];
        n->optimal_pal = n->best_pal_up[COST_TYPE_COMBINED];
      }
    }
  } else {
    // has inputs
    LiVESList *prio_list = backtrack(n, 0, factors);
    boolean is_converter = FALSE;
    if (n->flags & NODESRC_IS_CONVERTER) is_converter = TRUE;

    for (k = 0; k < N_COST_TYPES; k++) {
      for (ni = 0; ni < n->n_inputs; ni++) {
        in = n->inputs[ni];

        // again, we ignore cloned inputs, they don't affect the previous palette
        if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;

        in->best_in_pal[k] = WEED_PALETTE_NONE;
        if (svary && !is_converter && ni < n->n_outputs) {
          in->best_in_pal[k] = n->outputs[ni]->best_out_pal[k];
        } else if (!in->npals) {
          in->best_in_pal[k] = n->best_pal_up[k];
        }
        p = in->node;
        /* if (p->flags & NODEFLAG_LOCKED) { */
        /*   out = p->outputs[in->oidx]; */
        /*   if (out->npals) in->best_out_pal[k] = out->cpal; */
        /*   else in->best_out_pal[k] = p->cpal; */
        /* } */
        //else
        in->best_out_pal[k] = WEED_PALETTE_NONE;
      }
    }

    // ....has inputs...
    for (k = 0; k < N_COST_TYPES; k++) {
      // find a tuple with min cost for each type
      // given the limitations of best_in_pal[k], best_out_pal[k] already set
      cost_tuple_t *tup = best_tuple_for(prio_list, n, k);
      for (ni = 0; ni < n->n_inputs; ni++) {
        in = n->inputs[ni];
        if (in->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
        if (in->best_in_pal[k] == WEED_PALETTE_NONE)
          in->best_in_pal[k] = tup->palconv[ni].in_pal;
        if (in->best_out_pal[k] == WEED_PALETTE_NONE)
          in->best_out_pal[k] = tup->palconv[ni].out_pal;

        if (k == COST_TYPE_COMBINED && in->npals) in->optimal_pal = in->best_in_pal[k];
      }
    }
    if (prio_list) free_priolist(prio_list);
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
static void optimise_node(inst_node_t *n, double * factors) {
int nins = n->n_inputs;
if (nins) {
  /* int *opals = lives_calloc(nins, sizint); */
  /* boolean *used = (boolean *)lives_calloc(nins, sizint); */
  backtrack(n, 0, factors);
  /* lives_free(popals); */
  /* lives_free(used); */
}
// TODO
}


static inst_node_t *desc_and_do_something(int do_what, inst_node_t *n, inst_node_t *p,
  void *data, int flags, double * factors);

static inst_node_t *desc_and_compute(inst_node_t *n, int flags, double * factors) {
return desc_and_do_something(0, n, NULL, NULL, flags, factors);
}

static inst_node_t *desc_and_free(inst_node_t *n) {
return desc_and_do_something(1, n, NULL, NULL, 0, NULL);
}

static inst_node_t *desc_and_clear(inst_node_t *n) {
return desc_and_do_something(2, n, NULL, NULL, 0, NULL);
}

/* static inst_node_t *desc_and_find_src(inst_node_t *n, lives_clip_src_t *dsource) { */
/*   return desc_and_do_something(3, n, NULL, dsource,  0, NULL); */
/* } */

static inst_node_t *desc_and_total(inst_node_t *n, int flags, double * factors) {
return desc_and_do_something(4, n, NULL, NULL, flags, factors);
}

static inst_node_t *desc_and_reinit(inst_node_t *n) {
return desc_and_do_something(5, n, NULL, NULL, 0, NULL);
}

static inst_node_t *desc_and_add_steps(lives_nodemodel_t *nodemodel, inst_node_t *n, exec_plan_t *plan) {
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

if (do_what == 0 || do_what == 4) {
  // for op type 0, calculate min costs for each pal / cost_type
  // for op type 4, set abs_cost for node
  if (!(n->flags & NODEFLAG_PROCESSED)) {
    boolean ghost = FALSE;
    if ((flags & _FLG_GHOST_COSTS) && (n->flags & NODESRC_LINEAR_GAMMA)) ghost = TRUE;
    g_print("computing min costs\n");
    if (do_what == 0) compute_all_costs(n, COST_TYPE_COMBINED, factors, ghost);
    else {
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
        if (out->npals) out_pal = out->optimal_pal;
        else out_pal = p->optimal_pal;

        if (in->npals) in_pal = in->optimal_pal;
        else in_pal = n->optimal_pal;

        // ignore gamma for now
        cdelta = find_cdelta(in->cdeltas, out_pal, in_pal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
        costs[ni] = cdelta->deltas;
        oabs_costs[ni] = p->abs_cost;
      }

      total_costs(n, n->n_inputs, oabs_costs, costs, factors, n->abs_cost);
      lives_free(oabs_costs); lives_free(costs);
      n->ready_ticks = n->abs_cost[COST_TYPE_TIME];
    }
  }
}
if (do_what == 6) {
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
    step = create_step(plan, STEP_TYPE_APPLY_INST, n, -1, deps, d);
  } else {
    // sources - we have 2 steps - LOAD or APPLY_INST
    // then a CONVERT to srcgroup->apparent_pal, srcgroup->apparent->gamma, sfile->hsize X sfile->vsize
    if (n->model_type != NODE_MODELS_GENERATOR) {
      // CLIP, SRC
      int track = n->outputs[0]->track;
      step = create_step(plan, STEP_TYPE_LOAD, n, track, NULL, 0);
    } else {
      // generator
      step = create_step(plan, STEP_TYPE_APPLY_INST, n, -1, NULL, 0);
    }
  }

  plan->steps = lives_list_prepend(plan->steps, (void *)step);

  if (n->n_outputs) {
    ostep = step;
    for (int no = 0; no < n->n_outputs; no++) {
      // now for each ouput: if it is a clone, we add a layer copy step
      //next, if node is a src we want to convert to the srcgroup pal, gamma, size

      // then we add convert step to the connected input, and store the step in the input
      // so it can be collated as a dep for the next node
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
      // - for srcs this may be 2 conversions in a row - first to srgroup, then the usual
      xdeps = (plan_step_t **)lives_calloc(1, sizeof(plan_step_t *));
      xdeps[0] = step;
      step = create_step(plan, STEP_TYPE_CONVERT, n, no, xdeps, 1);
      plan->steps = lives_list_prepend(plan->steps, (void *)step);
      in->dep = step;
    }
  }
}

if (do_what == 7) {
  lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)data;
  align_with_node(nodemodel, n);
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
      if (!(n->outputs[i]->flags &
            (NODEFLAGS_IO_SKIP | NODEFLAG_PROCESSED))) {
        // anything which needs all inputs processed
        output_node_t *xout = n->outputs[i];
        input_node_t *xin = xout->node->inputs[xout->iidx];
        if (xin->flags & (NODEFLAGS_IO_SKIP | NODEFLAG_IO_CLONE)) continue;
        xin->flags |= NODEFLAG_PROCESSED;
        if (desc_and_do_something(do_what, n->outputs[i]->node, n, data, flags, factors)) nfails++;
        else n->outputs[i]->flags |= NODEFLAG_PROCESSED;
      }
    }
    // check nfails vs last_nfails. If we have fails, but fewer than last time,
    // try again
    // If we had an identical amount as last time then, the lower node is waiting for another source
    if (!last_nfails || nfails < last_nfails) nfails = -nfails;
  } while (nfails < 0);
}

n->flags |= NODEFLAG_PROCESSED;

if (nfails) {
  for (i = 0; i < n->n_outputs; i++) {
    n->outputs[i]->flags &= ~NODEFLAG_PROCESSED;
  }
  return n;
}

if (do_what == 1) free_node(n);
else if (do_what == 2) {
  // clear processed flag from nodes
  for (i = 0; i < n->n_outputs; i++)
    n->outputs[i]->flags &= ~NODEFLAG_PROCESSED;
  n->flags &= ~NODEFLAG_PROCESSED;
}
return NULL;
}


// optimeise combined cost at sink, adjusting each node in isolation
#define OPT_SGL_COMBINED	1

static void optimise_cost_model(lives_nodemodel_t *nodemodel, int method, double * thresh) {
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

for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
  node_chain_t *nchain = (node_chain_t *)list->data;
  inst_node_t *n = nchain->last_node;
  // ignore any srcs which have inputs, we will reach these via input sources
  if (n->n_inputs) continue;
  //
  ascend_and_backtrace(n, nodemodel->factors, NULL);
  break;
}
}


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
int nins = 0;
int clipno = -1;
int model_type = NODE_MODELS_SRC;

pxtracks[0] = xtrack;

if (xtrack >= 0 && xtrack < nodemodel->ntracks) {
  clipno = nodemodel->clip_index[xtrack];
  sfile = RETURN_VALID_CLIP(clipno);
}

if (sfile) {
  model_for = sfile;
  switch (sfile->clip_type) {
  case CLIP_TYPE_NULL_VIDEO:
    //model_for = static_srcs[LIVES_SRC_TYPE_BLANK];
    break;
  case CLIP_TYPE_GENERATOR:
    model_type = NODE_MODELS_GENERATOR;
    model_for = get_primary_actor(sfile);
    break;
  case CLIP_TYPE_FILE:
    model_type = NODE_MODELS_CLIP;
    nins = 2;
    break;
  case  CLIP_TYPE_DISK:
    model_type = NODE_MODELS_CLIP;
    nins = 1;
    break;
  default: break;
  }
}

n = create_node(nodemodel, model_type, model_for, nins, NULL, 1, pxtracks);
n->model_idx = nodemodel->clip_index[xtrack];

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
        in->cpal = cdata->current_palette;
        in->width = cdata->width;
        in->height = cdata->height;
        // gamma type not set until we actually pull pixel_data
        // yuv details (subsopace, sampling, clamping)
        // not set until we actually pull pixel_data
      }
      in->f_ratio = f_ratios[1];
    }
    for (i = 0; i < n->n_outputs; i++) {
      output_node_t *out = n->outputs[i];
      out->width = sfile->hsize;
      out->height = sfile->vsize;
    }
    break;
    default:	break;
    }
  }
}

return n;
}


void free_nodes_model(lives_nodemodel_t **pnodemodel) {
// use same algo as for computing costs, except we free instead
lives_nodemodel_t *nodemodel = *pnodemodel;
inst_node_t *retn = NULL;
if (nodemodel->node_chains) {
  do {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      // ignore any srcs which have inputs, we will reach these via input sources
      if (n->n_inputs) continue;
      retn = desc_and_free(n);
    }
  } while (retn);
  lives_list_free_all(&(nodemodel->node_chains));
}
lives_free(nodemodel);
*pnodemodel = NULL;
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


static void explain_node(inst_node_t *n, int idx) {
input_node_t *in;
output_node_t *out;

char *node_dtl;
char *ntype;
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

if (n->n_inputs && idx >= 0) {
  input_node_t *in = n->inputs[idx];
  in->flags |= NODEFLAG_PROCESSED;

  node_dtl = lives_strdup_printf("input %d of %d, ", idx, n->n_inputs);
  g_print("%s", node_dtl);
  lives_free(node_dtl);

  if (svary) {
    node_dtl = lives_strdup_printf("has variant ");
    g_print("%s", node_dtl);
    lives_free(node_dtl);
  }

  node_dtl = lives_strdup_printf("size %d X %d ", in->width, in->height);
  g_print("%s", node_dtl);
  lives_free(node_dtl);

  if (in->npals) {
    node_dtl = lives_strdup_printf("has variant ");
    g_print("%s", node_dtl);
    lives_free(node_dtl);
  }

  if (in->npals) pal = in->pals[in->optimal_pal];
  else pal = n->pals[n->optimal_pal];

  node_dtl = lives_strdup_printf("palette %s ", weed_palette_get_name(pal));
  g_print("%s", node_dtl);
  lives_free(node_dtl);
}

if (idx >= 0) {
  ntype = get_node_name(n);
  node_dtl = lives_strdup_printf("< %s > (gamma - %s)\n", ntype, weed_gamma_get_name(n->gamma_type));
  lives_free(ntype);
  g_print("%s", node_dtl);
  lives_free(node_dtl);
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
    explain_node(in->node, -1);
  }
}

if (idx < 0) {
  // we were ascending to fill an input lower down and we reached this node, which has all inpuits filled.
  // describe this node and descend odown the flagged ouitput
  ntype = get_node_name(n);
  node_dtl = lives_strdup_printf("< %s > (gamma - %s)\n", ntype, weed_gamma_get_name(n->gamma_type));
  lives_free(ntype);
  g_print("%s", node_dtl);
  lives_free(node_dtl);
  if (!n->n_inputs) idx = 0;
}

for (i = 0; i < n->n_outputs; i++) {
  if (n->outputs[i]->flags & NODEFLAGS_IO_SKIP) continue;
  out = n->outputs[i];
  if (out->flags & NODEFLAG_PROCESSED) {
    in = out->node->inputs[out->iidx];
    if (idx >= 0 || (in->flags & NODEFLAG_PROCESSED)) continue;
  } else if (idx < 0) continue;

  out->flags |= NODEFLAG_PROCESSED;

  node_dtl = lives_strdup_printf("\t output %d of %d: size (%d X %d), est. costs: time = %.4f, "
                                 "qloss = %.4f, combined = %.4f] ", i, n->n_outputs,
                                 out->width, out->height,
                                 n->abs_cost[COST_TYPE_TIME],
                                 n->abs_cost[COST_TYPE_QLOSS_P], n->abs_cost[COST_TYPE_COMBINED]);
  g_print("%s", node_dtl);
  lives_free(node_dtl);

  g_print("\n\t\t====>");
  explain_node(out->node, out->iidx);
  if (idx < 0) {
    idx = 0;
    i = -1;
  }
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

    g_print("Found %s node_chain for track %d\n", nch->terminated
            ? "terminated" : "unterminated", track);
    g_print("Showing palette computation for COST_TYPE_COMBINED\n");
    explain_node(n, -1);
    break;
  }
}
// *INDENT-ON*

g_print("No more node_chains. Finished.\n\n");

reset_model(nodemodel);
}


#define DO_PRINT

static void _make_nodes_model(lives_nodemodel_t *nodemodel) {
inst_node_t *n, *ln;
void *model_for = NULL;
int *pxtracks;
int xtrack, i;
int model_type;

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
// before the fist node, and connect this. Then we add a source for track 2, which is the first node.
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
    int nins, nouts;
    int *in_tracks = NULL, *out_tracks = NULL;
    for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
      if (rte_key_is_enabled(i, TRUE)) {
        // for clip editor we consstruct instance nodes according to the order of
        // mainw->rte_keys
        // for multitrack iterate over filter_map filter_init events (TODO)
        filter = rte_keymode_get_filter(i + 1, rte_key_getmode(i));
        if (!filter) continue;

        // create an input / output for each non (permanently) disabled channel
        nins = count_ctmpls(filter, NULL, IN_CHAN);
        nouts = count_ctmpls(filter, NULL, OUT_CHAN);

        if (!nins) continue;

        if (!mainw->multitrack) {
          in_tracks = (int *)lives_calloc(nins, sizint);
          out_tracks = (int *)lives_calloc(nouts, sizint);
          in_tracks[0] = 0;
          if (nins > 1) in_tracks[1] = 1;
        } else {
          // for  multitrack, we have to check to ensure that at least one in_track
          // has a non-NULL layer, or a node_chain for the track
          // otherwise the instance has no active inputs, so we will just skip over it
          // we can also have repeated channels, and need in_count, out_count for this
          // these values all come from the
          // TODO
        }

        n = create_node(nodemodel, NODE_MODELS_FILTER, filter, nins, in_tracks, nouts, out_tracks);
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

if (!mainw->ext_playback) model_type = NODE_MODELS_INTERNAL;
else {
  model_type = NODE_MODELS_OUTPUT;
  model_for = (void *)mainw->vpp;
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

reset_model(nodemodel);

// after constructin gthe treemodel by descending, we now proceed in phases
// we alternate descending and ascending phases, with descending ones passing values from srcs to sink
// and asceinding propogating values in the opposit direction

// phase 0, building the model was descending so next wave is ascending

// when constructing the model we set sizes, but only if the node_chain connected to a src with fixed size
// now we wil ascend from the sink up to all srcs. Starting with the display size, we con up all inputs
// keeping track of layer size.

// If we reach a node with 0, 0- size, then we set the node size to the carried size
// then ascend inputs from there.

// pass 1 ascending - set missing sizes
for (list = nodemodel->node_chains; list; list = list->next) {
  nchain = (node_chain_t *)list->data;
  n = nchain->last_node;
  if (!NODE_IS_SINK(n) && n->n_outputs) continue;
  if (n->flags & NODEFLAG_PROCESSED) continue;

  n->flags |= NODEFLAG_PROCESSED;
  set_missing_sizes(n);
}

reset_model(nodemodel);

// on another ascending wave we note anywher we upscale and calculate the cost adding to carried cost (qloss_size)
// if we reach a node with qloss from a downscale


// if we reach a node with mutli inputs, we find smallest layer. resize costs dicounted
// for all other ins, but cal downsize fr. src size to premix size. take osts from other layers and min(src_sz
// other, our src siz, scrn size)  is mutlipled in for each,
// then at nodes, we mltiply together qloss size for all ins and get tot.
// when calculating in qloss we use not pre qloss, bu prev qloss + ln(prev qloss)

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
    retn = desc_and_compute(n, flags, nodemodel->factors);
  }
} while (retn);

reset_model(nodemodel);

// phase 5 ascending

// FIND LOWEST COST PALETTES for each cost type
for (list = nodemodel->node_chains; list; list = list->next) {
  nchain = (node_chain_t *)list->data;
  n = nchain->last_node;
  if (!NODE_IS_SINK(n) && n->n_outputs) continue;
  if (n->flags & NODEFLAG_PROCESSED) continue;

  // checking each input to find prev_pal[cost_type]
  // create cost tuples
  //however this is not accurate as we do not know abss_cost tcost
  // when optimising we take this into account
  n->flags |= NODEFLAG_PROCESSED;
  map_least_cost_palettes(n, nodemodel->factors);
}

reset_model(nodemodel);

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

do {
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    retn = desc_and_total(n, flags, nodemodel->factors);
  }
} while (retn);

reset_model(nodemodel);

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


void get_true_costs(lives_nodemodel_t *nodemodel) {
// recalc costs, but now ignoring ghost costs for gamma_conversions
node_chain_t *nchain;
inst_node_t *n, *retn = NULL;
LiVESList *list;
do {
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    retn = desc_and_compute(n, 0, nodemodel->factors);
  }
} while (retn);

reset_model(nodemodel);

// we dont adjust any palettes

do {
  // descend and total costs for ech node to get abs_cost
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    retn = desc_and_total(n, 0, nodemodel->factors);
  }
} while (retn);

reset_model(nodemodel);
// only recalc slack and optimise if soemthing has changed in the model
}


// do the following,
// take each node_chain which starts from a source with no inputs
// compute the costs as we pass down nodes.
// if we reach a node with unprocessed inputs, then we continue to the next node_chain
// eventually we will fill all inputs and can contue downwards
void find_best_routes(lives_nodemodel_t *nodemodel, double * thresh) {

// haveing set inital sizes, we can now estimate time costs
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
  int allpals[] = ALL_STANDARD_PALETTES;
  for (i = 0; i < n_allpals; i++) {
    if (pal_permitted(n, allpals[i])) break;
  }
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
/*   if (!n || cost_type < 0 || cost_type >= N_COST_TYPES) */
/*     return -1.; */
/*   return n->min_cost[cost_type]; */
/* } */


/* int get_best_palette(inst_node_t *n, int idx, int cost_type) { */
/*   if (!n || cost_type < 0 || cost_type >= N_COST_TYPES) */
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
lives_list_free_all(&(*pnodemodel)->node_chains);
lives_free(*pnodemodel);
*pnodemodel = NULL;
}


static void _build_nodemodel(lives_nodemodel_t **pnodemodel, int ntracks, int *clip_index) {
if (pnodemodel) {
  lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)lives_calloc(1, sizeof(lives_nodemodel_t));

  if (*pnodemodel) {
    free_nodes_model(pnodemodel);
  }

  *pnodemodel = nodemodel = (lives_nodemodel_t *)lives_calloc(1, sizeof(lives_nodemodel_t));

  nodemodel->ntracks = ntracks;

  nodemodel->clip_index = (int *)lives_calloc(ntracks, sizint);
  for (int i = 0; i < ntracks; i++) nodemodel->clip_index[i] = clip_index[i];

  get_player_size(&nodemodel->opwidth, &nodemodel->opheight);
  while (!nodemodel->opwidth) {
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

  // now we have all the details, we can calulate the costs for time and qloss
  // and then for combined costs. For each cost type, we find the sequence of palettes
  // which minimises the cost type. Mostly we are interested in the Combined Cost, but we will
  // calclate for each cost type separately as well.
  find_best_routes(nodemodel, NULL);

  // for the calulation and routing, we may have added some "dummy" qloss costs to ensure we found the optimal
  // palette sequnce/ . Now we recalulate without the dummy values, so we can know the true cost.

  get_true_costs(nodemodel);

  reset_model(nodemodel);

  // there is one more condition we should check for, YUV detail changes, as these may
  // trigger a reinit. However to know this we need to have actual nodemodel->layers loaded.
  // So we set this flag, which will cause a second dry run with loaded nodemodel->layers before the first actual
  // instance cycle. In doing so we have the opportunity to reinit the instances asyn.
  nodemodel->flags |= NODEMODEL_NEW;

  // So we will run this in a background thread, which may smooth out the actual instance cycel and playback
  // since we can be reinitng as the earlier instances are processing.
  //retn = desc_and_reinit(nodemodel);
}
}


void build_nodemodel(lives_nodemodel_t **pnodemodel) {
_build_nodemodel(pnodemodel, mainw->num_tracks, mainw->clip_index);
}


void cleanup_nodemodel(void) {
if (mainw->plan_cycle) {
  if (mainw->plan_runner_proc) {
    if (mainw->plan_cycle->state == PLAN_STATE_NONE
        || mainw->plan_cycle->state == PLAN_STATE_RUNNING) {
      lives_proc_thread_cancel(mainw->plan_runner_proc);
    }
    lives_proc_thread_join_int(mainw->plan_runner_proc);
    lives_proc_thread_unref(mainw->plan_runner_proc);
    mainw->plan_runner_proc = NULL;
  }

  exec_plan_free(mainw->plan_cycle);
  mainw->plan_cycle = NULL;
}

if (mainw->exec_plan) {
  exec_plan_free(mainw->exec_plan);
  mainw->exec_plan = NULL;
}

if (mainw->layers) {
  int maxl;
  if (mainw->nodemodel) maxl = mainw->nodemodel->ntracks;
  else maxl = mainw->num_tracks;
  mainw->blend_layer = mainw->frame_layer = NULL;
  for (int i = 0; i < maxl; i++) {
    if (mainw->layers[i]) {
      weed_layer_unref(mainw->layers[i]);
      mainw->layers[i] = NULL;
    }
  }
  lives_free(mainw->layers);
  mainw->layers = NULL;
}
if (mainw->nodemodel) free_nodemodel(&mainw->nodemodel);
}

