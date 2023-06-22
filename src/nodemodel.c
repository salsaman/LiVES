// nodemodel.c
// LiVES
// (c) G. Finch 2023 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// node modelling - this is used to model the effect chain before it is appplied
// in order to optimise the operational sequence and to compare hypothetical alternatives

// in the current implementation, the focus is on optimising the sequence of palette convesrsion
// as layers pass through the effects chain from sources to display sink

#include "main.h"
#include "nodemodel.h"
#include "effects-weed.h"
#include "effects.h"

// still to do:
// - creat outputs for all src types
// calc sizes, pass 2
// calc costd for inputs from srcs
// fine tune sizes for channels
// implement dry run 1 and set skip
// premult alpha flag
// check for reinits
/// do asynch reinits

// elsewhere:
// create static clip_srcs
// clean up code in load_Frame_layer and weed_appl_instace
//create new func - execute_plan()
// include provision  for deinterlace
// implemnt 2 way letterboxing
// lerly loading for layers
// measure actual timings

// create model deltas, bypass nodes




static void align_with_model(lives_nodemodel_t *nodemodel) {
  // after creating and optimising the model, we now align the real objects with the nodemodel map

  // for CLIP, set the clip_src palettes
  
  /* if (dplug->dpsys->set_palette && target_palette != opal) { */
  /*   int pal = best_palette_match(dplug->cdata->palettes, -1, target_palette); */
  /*   if (pal != cpal) { */
  /*     pthread_mutex_lock(&dplug->mutex); */
  /*     dplug->cdata->current_palette = pal; */
  /*     if (!(*dplug->dpsys->set_palette)(dplug->cdata)) { */
  /* 	dplug->cdata->current_palette = cpal; */
  /* 	(*dplug->dpsys->set_palette)(dplug->cdata); */
  /*     } else if (dplug->cdata->rec_rowstrides) { */
  /* 	lives_free(dplug->cdata->rec_rowstrides); */
  /* 	dplug->cdata->rec_rowstrides = NULL; */
  /* 	cpal = pal; */
  /*     } */
  /*     pthread_mutex_unlock(&dplug->mutex); */
  /*   } */
  /* } */

  // for instances, set the channel palettes (sizes are set when testing)

  // for display output set the vpp palette
  
}


// all operations that have associated costs may be handled here
// depending on the in and out palettes we may be abel to combine two or three of rsize, palconv, gammconv
// this will reduce the overall time compared to performing the operations in sequence
// we also have a number of pool threads available, if we know what operations are occuring in paralell,
// we can model distrubuting the threads approximately over simultaneous operations
// and discount some time cost
// this can affect resize, palcovn, gamma conv and proc tcosts

double get_resize_cost(int cost_type, int inpl, int outpl, int *outpals, boolean ghost) {
  // there is a tcost based on input size, and a qloss for downscaling
  // there is a ghost cost for upscaling based on the fact that the quality los is more perceptible
  // the time cost is partly based on measurements, and should factor in the input size and palette
  // for some palettes, this can be combined with palette conversion and gamma change
  // in some cases we letterbox instead
  // then the cost is calculted as resize + layer copy


}


double get_proc_cost(int cost_type, int inpl, int outpl, int *outpals, boolean ghost) {
  // get processing cost for applying an instance. The 
  // we do not assing a qloss value
  // but here will be a tcost based on measurements, and assumed to be depndent on the input size and palette


}


double get_layer_copy_cost(int cost_type, int inpl, int outpl, int *outpals, boolean ghost) {
  // find the cost for copying (duplicating) a layer. This needs to be factored in if we have cloned inputs,
  // after resizing / converting palette and gamma
  // thishould be dependent on byte size of the pixel data
  // in reality this may be adjusted according to the number of threads available for processing theinsstance
  // there is a time cost only, no associated qloss
  
}




double get_gammaconv_cosst(int cost_type, int inpl, int outpl, int *outpals, boolean ghost) {
  // find the time cst for converting gamma - this should be similar to the layer copy time
  // and may be paralellisable
  // there is a small qloss associated, which varies with pb_quality
  // ther eiss also a ghost negative qloss for using linear gamma if a plugin prefers this
  


}



// calculate estimated cost of type cost_type for converting a palette
double get_pal_conversion_cost(int cost_type, int inpl, int outpl, int *outpals, boolean ghost) {
  double cost = 0.;
  // these are ignored for now
  int in_sampling, out_sampling, in_clamping, out_clamping, in_subspace, out_subspace;
  in_sampling = out_sampling = WEED_YUV_SAMPLING_DEFAULT;
  in_clamping = out_clamping = WEED_YUV_CLAMPING_UNCLAMPED;
  in_subspace = out_subspace = WEED_YUV_SUBSPACE_YUV;
  if (inpl == outpl) return cost;
  switch (cost_type) {
  case COST_TYPE_QLOSS: {
    double q = 1.;
    switch (inpl) {
    case WEED_PALETTE_RGB24:
    case WEED_PALETTE_RGBA32:
    case WEED_PALETTE_BGR24:
    case WEED_PALETTE_BGRA32:
    case WEED_PALETTE_ARGB32:
      switch (outpl) {
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
	q *= 0;
	if (out_clamping == WEED_YUV_CLAMPING_UNCLAMPED) q *= 1.01;
	break;
      }
    case WEED_PALETTE_YUV888:
    case WEED_PALETTE_YUV444P:
    case WEED_PALETTE_YUVA8888:
    case WEED_PALETTE_YUVA4444P:
      switch (outpl) {
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
      default:
	q *= 0;
	break;
      }
    case WEED_PALETTE_YUV422P:
    case WEED_PALETTE_UYVY:
    case WEED_PALETTE_YUYV:
      switch (outpl) {
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
      default:
	q *= 0;
	break;
      }
    case WEED_PALETTE_YUV420P:
    case WEED_PALETTE_YVU420P:
      switch (outpl) {
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
      default:
	q *= 0;
	break;
      }
    case WEED_PALETTE_YUV411:
      switch (outpl) {
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
      default:
	cost = -1.;
	break;
      }
    default:
      cost = -1.;
      break;
    }
    if (cost == 0.) {
      cost = 1. - q;
      if (prefs->pb_quality == PB_QUALITY_HIGH) cost *= .75;
      else if (prefs->pb_quality == PB_QUALITY_LOW) cost *= 1.5;
    }
    if (ghost && weed_palette_is_yuv(outpl)) cost *= 1.1;
    break;
  }
  case COST_TYPE_TIME:
    cost = get_conv_time(inpl, outpl, outpals);
    break;
  default:
    cost = -1.;
    break;
  }

 done:
  return cost;
}


static int calc_node_sizes(inst_node_t *inode) {
  // CALCULATE CHANNEL SIZES

  // the sizes set here are just initial values to bootstrap the nodemodel

  // in the final implementation, sizes will be a thing that gets optimised, alongside palettes
  // and gamma types

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
  // ins and outs must be paired, so we will set ouput 0 to the size, and hence we must resize 1st input to this size
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

  weed_instance_t *instance = (weed_instance_t *)dsource->source;
  weed_channel_t *channel = get_enabled_channel(instance, 0,
						dsource->src_type == LIVES_SRC_TYPE_EFFECT);
  weed_chantmpl_t *chantmpl = weed_channel_get_template(channel);
  weed_filter_t *filter = weed_instance_get_filter(instance, TRUE);

  doublke minaar = 0., maxar = 0., ar, op_ar, w0, w1, h0, h1;
  double rmaxw, rmaxh, rminw = 0., rminh = 0.;
  double op_ar, bb_ar, 
    boolean non_lb_layer = FALSE, letterbox = FALSE;
  boolean svary = FALSE, is_converter = FALSE;
  int xopwidth = opwidth;
  int xopheight = oheight;

  int filter_flags = weed_filter_get_flags(filter);
  int sflags = weed_chantmpl_get_flags(chantmpl);

  if (filter_flags & WEED_FILTER_CHANNEL_SIZES_MAY_VARY) svary = TRUE;
  if (filter_flags & WEED_FILTER_IS_CONVERTER) is_converter = TRUE;

  if ((mainw->multitrack && prefs->letterbox_mt) || (prefs->letterbox && !mainw->multitrack)
      || (LIVES_IS_RENDERING && prefs->enc_letterbox))
    if (!svary) letterbox = TRUE;

  rminw = rmaxw = 0.;
  rminh = rmaxh = 0.;
  
  // get max and min widths and heights for all input layers
  for (i = 0; i < num_in_tracks; i++) {
    lives_clip_t *sfile = NULL;
    channel = in_channels[i];

    // TODO - flag as SKIP in nodemodel
    if (weed_channel_is_alpha(channel)) continue;
   
    // TODO - flag as SKIP in nodemodel
    if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
	weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) {
      continue;
    }

    // get size from prev output
    outnode = get_out_for_input(inpnode);

    if (!outnode) continue;

    if (!non_lb_layesr) {
      if (num_in_tracks > 1 && sfile && sfile->clip_type == CLIP_TYPE_GENERATOR
	  && prefs->no_lb_gens) {
	// if we have mixed letterboxing / non letterboxin layers, we not this
	// then all layers which dont letterbox will get size opwidth X opheight
	// these layers will then define the lb_size (rather than rmaxw X rmaxh)
	//
	non_lb_layer = TRUE;
      }
    }

    inpnode->wdidth = outnode->width;
    inpnode->height = outnode->height;

    if (num_in_tracks == 1) break;
    
    // if prevnode has no size set - which can be the case if we have variable size sources
    // then if we have other inputs with size set, we set this to the bounding box size
    // if there are no other inputs with non zero height, then we leave it as zero, and set ouput to zero

    // after the first pass we will begin with the sink size, and ascend up the node tree
    // if we arrive at a node with output size, 0, 0, 
    // we set the output size for the node to be equal to the
    // connected input. Then we will ascend. setting input size == output size until we
    // just as with palettes, the first output to reach a multi output node will set the size for the node

    if (inpnode->width == 0. || inpnode->height == 0.) continue;

    if (rminw == 0. || (double)inpnode->width < rminw) rminw = (double)inpnode->width;
    if ((double)inpnode->width > rmaxw) rmaxw = (double)inpnode->width;

    if (rminh == 0.|| (double)inpnode->height < rminh) rminh = (double)inpnode->height;
    if ((double)inpnode->height > (double)rmaxh) rmaxh = (double)inpnode->height;

    ar = (double)inpnode->width / (double)inpnode->height;
    if (minar == 0. ar < minar) minar = ar;
    if (ar > maxar) maxar = ar;
  }

  if (ramxw > 0. && rmaxh > 0.) {
    // we can have cases where all inputs are from srcs with var. size
    // if so then we are going to set node and output sizes to 0
    // and fix this ascending
    op_ar = (double)opwidth / (double)opheight;

    // we found bounding box rmaxw X rmaxh
    // find 2 new boxes

    // if we have only one in channel, we avoid resizing
    // if we have mutliple in channels we will find the bounding box of all layers
    // set this to a.r of op and then resize all layers to this

    // (if letterboxing then opwidht X opheight becomes lb_size, and inner sizes keep layer aspect ratios
    // letterbox size is rmaxw X rmaxh, unless we hav layers which do not lettrebox, then they are scaled
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
    switch (pb_quality) {
    case PB_QUALITY_HIGH:
      // we have 2 options, keep width same, increas or redeuce height,
      // or keep hieght same, reduce or increase width
      // try reducing first, if this is > op size we use that
      // if < op size, then try increasing
      // if still < opsize, we use opsize
      
      // for high qu we use the op size or inner box, whcihever is largest
      if (w0 <= rmaxw) {
	// size w0 X rmaxh
	if (w0 >= (double)opwidth && rmaxh >= (double)opheight) {
	  // use inner
	  opwidth = (int)w0;
	  opheight = (int)rmaxh;
	  break;
	}
      }
      if (h0 <= (double)rmaxh) {
	// size w0 
	if (h0 >= (double)opheight && rmaxw >= (double)opwidth) {
	  // use inner
	  opwidth = (int)rmaxw;
	  opheight = (int)h0;
	  break;
	}
      }
      // inner was < displaye
      // try with max instead
      if (w0 >= rmaxw) {
	// size w0 X rmaxh
	if (w0 >= (double)opwidth && rmaxh >= (double)opheight) {
	  // use inner
	  opwidth = (int)w0;
	  opheight = (int)rmaxh;
	  break;
	}
      }
      if (h0 = (double)rmaxh) {
	// size w0 
	if (h0 >= (double)opheight && rmaxw >= (double)opwidth) {
	  // use inner
	  opwidth = (int)rmaxw;
	  opheight = (int)h0;
	  break;
	}
      }
      // if neither inner or outer is > screen size, use screen size
      break;
    case PB_QUALITY_MED:
      // for med we will use outer box, inner box  or op size, whichever is smaller
      if (w0 >= rmaxw) {
	if (w0 <= (double)opwidth && rmaxh <= (double)opheight) {
	  opwidth = (int)w0;
	  opheight = (int)rmaxh;
	  break;
	}
      }
      if (h0 >= rmaxh) {
	if (h0 <= (double)opheight && rmaxw <= (double)opwidth) {
	  opwidth = (int)rmaxw;
	  opheight = (int)h0;
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
    case PB_QUALITY_LOW:
      // for low we will use outer box of min vals, or inner box of max vals, whichever is smaller
      // if both are > op size, we use inner box of min vals or op size, whichever issmaller
      if (w1 > rminww) {
	if (w1 <= (double)opwidth && rminh <= (double)opheight && rminh < h0 ) {
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

    if (!letterbox || svary) {
      // if not letterboxing, all layers will be resized to new opwidth
      innode->width = opwidth;
      innode->height = opheight;
    }
    else {
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
      // so thre ratios - redced dimension / op, for each layer, final expanded dmiension . max expanded,
      // and expanded dim. strectch over redux
      // the first tesll how much of op is banded
      // the second the internal banding,
      // and third how much the layers get rudced by due to a wide / tall outlier layer
      // eg we have one very wide layer, we should band op horizontally
      // unless we halso have on very tall layer outlier
      // this owudl be worst case option 1 vey wide laye and 1 very tall
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
      // calcate with ha baning
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

      double vcov, hcov, hspread = 1., vspread = 1., hshrinkage = 1, vshrinlage = 1.;
      double rw, rwh, rh, rhw, new_maxw = 0., new_maxh = 0.;
      int lb_width, lb_height;

      // get new opbox

      bb_ar = rmaxw / rmaxh;

      if (pb_quality == PB_QUALITY_LOW) {
	if (opwidth <= rmaxw && opheight <= rmaxh) {
	  if (opwidth / bb_ar > opheight) {
	    rmaxw = opwidth;
	    rmaxh = rmaxw / bb_ar;
	  }
	  if (opheight * bb_ar > owidth) {
	    rmaxh = opheight;
	    rmaxw = rmaxh * bb_ar;
	  }
	}
      }

      else {
	if (opwidth < rmaxw) {
	  opwidth = rmaxw;
	  opheight = rmaxw / op_ar;
	}
	if (opheight < rmaxh) {
	  opheight = rmaxh;
	  opwidth = rmaxh * op_ar;
	}
      }

      if (opwidth > rmaxw) banding = 1;
      
      for (i = 0; i < num_in_tracks; i++) {
	lives_clip_t *sfile = NULL;
	channel = in_channels[i];

	// TODO - flag as SKIP in nodemodel
	if (weed_channel_is_alpha(channel)) continue;
      
	/* cpalette = weed_channel_get_palette(channel); */
	/* if (weed_palette_is_alpha(cpalette)) continue; */

	// TODO - flag as SKIP in nodemodel
	if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
	    weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) {
	  continue;
	}

	if (rmaxw > 0. && inpdnode->node->dsource
	    && inpnode->source->dsource->src_type == LIVES_SRC_TYPE_FILTER
	    && prefs->no_lb_gens) continue;

	width = inpnode->width;
	height = inpnode->height;

	if (banding == 0) {
	  width = (int)((double)width * rmaxh / (double)height + .5);
	  if (width > new_maxw) new_maxw = width;
	  height = (int)(rmaxh + .5);
	}
	else {
	  height = (int)((double)height * rmaxw / (double)width + .5);
	  if (height > new_maxh) new_maxh = height;
	  width = (int)(rmaxw + .5);
	}
	inpnode->inner_width = width;
	inpnode->inner_height = height;
      }

      bb_ar = new_max_width / rmaxh;
      scf = 1;
      
      if (banding == 0) {
	rmaxw = new_max_w;
	if (pb_quality == PB_QUALITY_MED) {
	  if (rmaxw > (double)xopwidth) scf = rmaxw / (double)xopwidth;
	}
	if (pb_quality == PB_QUALITY_LOW) {
	  if (rmaxw > (double)opwidth) scf = rmaxw / (double)opwidth;
	}
      }
      else {
	rmaxh = new_max_h;
	if (pb_quality == PB_QUALITY_MED) {
	  if (rmaxh > (double)xopheight) scf = rmaxh / (double)xopheight;
	}
	if (pb_quality == PB_QUALITY_LOW) {
	  if (rmaxh > (double)opheight) scf = rmaxw / (double)opheight;
	}
      }

      if (scf != 1.) {
	rmaxw /= scf;
	rmaxh /= scf;
	opwidth = (int)((double)opwidth * scf + .5);
	opheight = (int)((double)opheight * scf + .5);
      }
    }

    if (has_non_lb) {
      lb_wddith = opwidth;
      lb_height = opheight;
    }
    else {
      lb_wddith = (int)(rmaxw + .5);
      lb_height = (int)(rmaxh + .5);
    };

    for (i = 0; i < num_in_tracks; i++) {
      lives_clip_t *sfile = NULL;
      channel = in_channels[i];

      // TODO - flag as SKIP in nodemodel
      if (weed_channel_is_alpha(channel)) continue;
      
      /* cpalette = weed_channel_get_palette(channel); */
      /* if (weed_palette_is_alpha(cpalette)) continue; */

      // TODO - flag as SKIP in nodemodel
      if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
	  weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE)
	continue;

      if (rmaxw > 0. && inpdnode->node->dsource
	  && inpnode->source->dsource->src_type == LIVES_SRC_TYPE_FILTER
	  && prefs->no_lb_gens) {
	inpnode->width = inpnode->inner_width = opwidth;
	inpnode->height = inpnode->inner_height = opheight;
	continue;
      }

      inpnode->inner_wodth = scf * inpnode->inner_width;
      inpnode->inner_height = scf * inpnode->inner_height;
      inpnode->width = lb_width;
      inpnode->height = lb_height;
    }
  }

  // calculate score for each variant (horiz / vert banding)
  // the score is derived from: coversge (the ratio of inner frame to outer frame)
  // shrinkage - how much a layer is squashed - we first exapnd the layers to max, then reduce this to
  // opwidth or opheight - this gives factors for the bounding box, rrw, rh
  // however we look at this layer by layer each layer expands by a different amount and then is reduced
  // by the same amount. Layers which expand less, are stil shrunk by same ratio, this is the shrinkage.
  // we expand in both x and y, then reduce in both x and y simultaneously, we dont want to upscalelayers too much
  // since this reduces quality, and costs time, so  we divide score by each shrinkage * a weight. Also
  // if we had say, a wide, shrot layer, we would expand height a lot, expand with a lot, shrin widht a lot
  // the othe r layers would expand les, but still shrink a lot
  // on ther hand if we increa all to max width, then heigh increases a lot, then shrnking back fit it
  // reduces a lot, more or less cancelling the expansion, thus shrinkage is less, except for thewide layer
  // theheight would not increase at all, but would be shrunk down, howerve thi is only one layer.
  // finally we have spread - this is a measure of the differenc in widths or heighta after shrinking / expanding
  // this defines the amount of internal letterboxing. This is realted to the a.r of all the layers
  // this is least important, since if this differes we will always have to lb
  // and finally we consider parallel banding, this where we have to reduc in opposite direction
  // due to bounding box a.r so we calulate this also
  // we end up with a final score, and use whichever method has the highest weighted
  // coverage + porduct of shrinkages +
  // product of spreads, + product of cross banding
  /* /\* hscore = 0.; *\/ */
  /* /\* vscore = 0,; *\/ */

  /* /\* // coverage *\/ */
  /* /\* hcov = rmaxh / rw * / (double)opheight * (double)opwidth; *\/ */
  /* /\* vcov = rmaxw / rh / (double)opwidth * (double)opheight; *\/ */

  /* /\* // shrinkage *\/ */
  /* /\* for (i = 0; i < num_in_tracks; i++) { *\/ */
  /* /\* 	channel = in_channels[i]; *\/ */

  /* /\* 	// TODO - flag as SKIP in nodemodel *\/ */
  /* /\* 	if (weed_channel_is_alpha(channel)) continue; *\/ */

  /* /\* 	// TODO - flag as SKIP in nodemodel *\/ */
  /* /\* 	if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE || *\/ */
  /* /\* 	    weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) { *\/ */
  /* /\* 	  continue; *\/ */
  /* /\* 	} *\/ */

  /* /\* 	width = innode->width; *\/ */
  /* /\* 	height = innode->height; *\/ */

  /* /\* 	// we want this to be large as poss. *\/ */
  /* /\* 	// we want to avoid upscalign too mucha as this can introduce artefacts *\/ */
  /* /\* 	// this is less of an issue at high q, as we use slower higher quality scalers *\/ */

  /* /\* 	// so we want (new_width * new_height) / (width * height) *\/ */
  /* /\* 	// this is ((width * rmaxh / height) * rmaxh) / (width * height) *\/ */
  /* /\* 	// this is correct because we already reduced rmaxh and rmahxw by factor rw *\/ */
  /* /\* 	// so we end up with (rmaxh *rmaxh) / (height * height) *\/ */
  /* /\* 	// then cancelling width from top and bottom, we end up with rmaxh / (height * height). This is correct because *\/ */
  /* /\* 	// we increase width proportianlly to height, then  *\/ */
  /* /\* 	hshrinkage *= ((double)height * (double)height) / (rmaxh * rmaxh); *\/ */
  /* /\* 	vshrinkage *= ((double)width * (double)width) / (rmaxw * rmaxw); *\/ */

  /* /\* 	// now spread, we want to be large as possible *\/ */
  /* /\* 	// rmaxh we already reduced by rw, so thsi gives expanded width / rw  *\/ */
  /* /\* 	hspread *= rmaxh / (double)height * (double)width / (double)opwidth; *\/ */
  /* /\* 	vspread *= rmaxw / (double)width * (double)height / (double)opheight;  *\/ */
  /* /\* } *\/ */

  /* /\* if (horiz_bands) *\/ */
  /* /\* 	hscore = hcov * COV_WEIGHT + hshrinkage * SHRK_WEIGHT + hdpread * SPRD_WEIGHT; *\/ */
  /* /\* if (vert_bands) *\/ */
  /* /\* 	vscore = vcov * COV_WEIGHT + vshrinkage * SHRK_WEIGHT + vdpread * SPRD_WEIGHT; *\/ */

  /* /\* // now we know banding we can get the sizes for each layer (input) *\/ */
  /* /\* // we set two sizes - the inner size, whcih we first expand / shrink the layer to *\/ */
  /* /\* // and outer size, which is the size we letterbox to *\/ */
  /* /\* // if both sizes are equal, or either is 0., 0. we do not letterbox *\/ */
  /* /\* // for the sink, different rules apply depending on the nature of the source it represents, *\/ */
  /* /\* // We may resize and / or letterbox the input(s), this is not included here *\/ */
  /* /\* // *\/ */
  /* /\* if (hscore > vscore) { *\/ */
  /* /\* 	// horiz banding *\/ */
	
  /* /\* 	rmaxh /= rw; *\/ */

  /* /\* 	if (non_lb_layesr) { *\/ */
  /* /\* 	  lb_width = (int)(rmaxw + .5); *\/ */
  /* /\* 	  lb_height = (int)(rmaxw / op_ar + .5); *\/ */
  /* /\* 	} *\/ */
  /* /\* 	else { *\/ */
  /* /\* 	  lb_width = rmaxw; *\/ */
  /* /\* 	  lb_height = rmaxh; *\/ */
  /* /\* 	} *\/ */

  /* /\* 	for (i = 0; i < num_in_tracks; i++) { *\/ */
  /* /\* 	  channel = in_channels[i]; *\/ */

  /* /\* 	  // TODO - flag as SKIP in nodemodel *\/ */
  /* /\* 	  if (weed_channel_is_alpha(channel)) continue; *\/ */

  /* /\* 	  // TODO - flag as SKIP in nodemodel *\/ */
  /* /\* 	  if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE || *\/ */
  /* /\* 	      weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) { *\/ */
  /* /\* 	    continue; *\/ */
  /* /\* 	  } *\/ */

  /* /\* 	  width = innode->width; *\/ */
  /* /\* 	  height = innode->height; *\/ */

  /* /\* 	  inpnode->lb_width = lb_width; *\/ */
  /* /\* 	  inpnode->lb_height = lb_height; *\/ */

  /* /\* 	  // generators can be set to not letterbox - in this case we find the outer aspect box *\/ */
  /* /\* 	  // for rmaxw, rmaxh, and this is their size, then everthing else letterboxes inside them *\/ */
  /* /\* 	  // instead of  *\/ */
  /* /\* 	  // and everything else will letterbox inside them *\/ */
  /* /\* 	  if (num_in_tracks > 1 && sfile && sfile->clip_type == CLIP_TYPE_GENERATOR *\/ */
  /* /\* 	      && prefs->no_lb_gens) { *\/ */
  /* /\* 	    inpnode->width = lb_width; *\/ */
  /* /\* 	    inpnode->height = lb_height; *\/ */
  /* /\* 	  } *\/ */
  /* /\* 	  else { *\/ */
  /* /\* 	    // this is correct, since we reduced rmaxh already by rw *\/ */
  /* /\* 	    width = (int)((double)width *rmaxh / (double)height + .5); *\/ */
  /* /\* 	    height = (int)(rmaxh +.5); *\/ */
  /* /\* 	    inpnode->width = width; *\/ */
  /* /\* 	    inpnode->height = height; *\/ */
  /* /\* 	  } *\/ */
  /* /\* 	} *\/ */
  /* /\* } *\/ */
  /* /\* else { *\/ */
  /* /\* 	// vertical banding *\/ */
  /* /\* 	rmaxw /= rh; *\/ */

  /* /\* 	if (non_lb_layesr) { *\/ */
  /* /\* 	  lb_width = (int)(rmaxw + .5); *\/ */
  /* /\* 	  lb_height = (int)(rmaxw / op_ar + .5); *\/ */
  /* /\* 	} *\/ */
  /* /\* 	else { *\/ */
  /* /\* 	  lb_width = rmaxw; *\/ */
  /* /\* 	  lb_height = rmaxh; *\/ */
  /* /\* 	} *\/ */

  /* /\* 	for (i = 0; i < num_in_tracks; i++) { *\/ */
  /* /\* 	  channel = in_channels[i]; *\/ */

  /* /\* 	  // TODO - flag as SKIP in nodemodel *\/ */
  /* 	  if (weed_channel_is_alpha(channel)) continue; */

  /* 	  // TODO - flag as SKIP in nodemodel */
  /* 	  if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE || */
  /* 	      weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) { */
  /* 	    continue; */
  /* 	  } */

  /* 	  // get size from prev output */
  /* 	  width = innode->width; */
  /* 	  height = innode->height; */

  /* 	  inpnode->lb_width = lb_width; */
  /* 	  inpnode->lb_height = lb_height; */

  /* 	  if (num_in_tracks > 1 && inpdnode->node->dsource */
  /* 	      && inpnode->source->dsource->src_type == LIVES_SRC_TYPE_FILTER */
  /* 	      && prefs->no_lb_gens) { */
  /* 	    inpnode->width = lb_width; */
  /* 	    inpnode->height = lb_height; */
  /* 	  } */
  /* 	  else { */
  /* 	    // this is correct, since we reduced rmaxh already by rw */
  /* 	    width = (int)((double)width *rmaxh / (double)height + .5); */
  /* 	    height = (int)(rmaxh +.5); */
  /* 	    inpnode->width = width; */
  /* 	    inpnode->height = height; */
  /* 	  } */

  /* // set size and pal for input - check id we need reinit */
  /* channel = get_enabled_channel(inst, i, FALSE); */
  /* if (!channel) break; // compound fx */

  /* // TODO - flag as SKIP in nodemodel */
  /* if (weed_channel_is_alpha(channel)) continue; */

  /* // TODO - flag as SKIP in nodemodel */
  /* if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE || */
  /*     weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE) */
  /*   continue; */
	  
  /* // */
  /* inpnode->cwidth = weed_channel_get_width_pixels(channel); */
  /* inpnode->cheight = weed_channel_get_height(channel); */

  /* set_channel_size(filter, channel, width, height); */

  /* inpnode->width = weed_channel_get_width_pixels(channel); */
  /* inpnode->height = weed_channel_get_height(channel); */

  /* // check if we need to reinit due to size change */
  /* if (channel_flags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE) { */
  /*   if (width != inwidth || height != inheight) */
  /*     inode->needs_reinit = TRUE; */
  /* } */
    
  // check if we need to reinit due to size change
  /* if (channel_flags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE) { */
  /*   // get before and after sizes in pixels, then multiply by bits per pixel */
  /*   // if this veries then the rowsstride will almost certainly change */
  /*if (width * weed_palette_get_pixels_per_macropixel(opalette) * weed_palette_bits_per_pixel_planar(opalette, 0) */
  /* 	  != inwidth * weed_palette_get_pixels_per_macropixel(cpalette)
  //  * weed_palette_bits_per_pixel_planar(cpalette, 0)) */
  /* 	needs_reinit = TRUE; */
  /* } */
}
}
}
}
// having calculated in input node / in channel sizes we can move to output nodes / out_channels
// currently, each out chanel / output gets the same size as corresponding input
// this may change in future in favour of size groups

ni = 0;

for (i = 0; i < num_out_tracks + num_out_alpha; i++) {
  /// for the time being we only have a single out channel, so it simplifes things greatly

  channel = get_enabled_channel(inst, i, FALSE);
  if (!channel) break; // compound fx

  // TODO - flag as SKIP in nodemodel
  if (weed_channel_is_alpha(channel)) continue;

  // TODO - flag as SKIP in nodemodel
  if (weed_get_boolean_value(channel, WEED_LEAF_DISABLED, NULL) == WEED_TRUE ||
      weed_get_boolean_value(channel, WEED_LEAF_HOST_TEMP_DISABLED, NULL) == WEED_TRUE)
    continue;

  if (is_converter && svary) {
    /// resizing - use the value we set in channel template
    /// this allows us to, for example, resize the same in_channel to multiple out_channels at various sizes
    chantmpl = weed_channel_get_template(channel);
    cpalette = weed_channel_get_palette(channel);
    width = weed_get_int_value(chantmpl, WEED_LEAF_HOST_WIDTH, NULL);
    width /= weed_palette_get_pixels_per_macropixel(weed_channel_get_palette(channel));
    height = weed_get_int_value(chantmpl, WEED_LEAF_HOST_HEIGHT, NULL);
    outnode->width = widthl
      outnode->height = height;
  }
  else {
    inpnode = inp(innode, ni++);
    outnode = outp(innode, ni++);
    outnode->width = inpnode->width;
    outnode->height = inpnode->height;
  }
 }
}


static int cost_summation[N_COST_TYPES];
static boolean sumty = FALSE;

static lives_result_t prepend_node(inst_node_t *n, int track, inst_node_t *target) {
  // create a forward link from n to target
  // first check out_tracks in n, ensure track is in the list - this will tell us which output to use
  // - there may only be 1 output for any track
  // then check in_tracks in target, this tells which inputs to use, there has to 1 or more with matching track
  // out with track number will be connected to matching input(s)
  // then for each input we set the details, the second and subsquent get the clone flag set
  // this indicates the output layer will be duplicated

  // we match an input from target to an output from n, using track numbers
  // we fill the the input values for the target and the output details for n
  
  input node_t *prime_ip;
  output node_t *prime_op;

  int op_idx = -1, ip_idx = -1, i;

  g_print("prepend node %p to %p for track %d\n", n, target, track);

  // locate output in n to connect from
  
  for (i = 0; i < n->n_outputs; i++) {
    if (n->outputs[i]->flags & NODEFLAGS_SKIP) continue;
    if (n->out_tracks[i] == track) {
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

  // if this is a clone, then we add it to clones of the parent output
  if (main_idx != op_idx) {
    output_node_t *prime_op = n->outputs[main_idx];
    nclones = 0;
    if (prime_op->clones) {
      while (prime_op->clones[nclones++]); // eg x[0], x[1] null, nclones == 2
      // nclones + 1 == 3
      prime_op->clones = lives_recalloc(prime_op->clones, nclones + 1, nclones, sizeof(output_node_t *));
      // nclones - 1 == 1, x[2] null
      prime_op->clones[nclones - 1] = n->outputs[op_idx];
    }
    else {
      prime_op->clones = lives_calloc(2, sizeof(output_node_t *));
      prime_op->clones[0] = n->outputs[op_idx];
    }
  }

  // locate input in target to connect to
  
  for (i = 0; i < target->n_inputs; i++) {
    if (target->inputs[i]->flags & NODEFLAGS_SKIP) continue;
    if (target->in_tracks[i] == track) {
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
  outnode = n->outputs[op_idx];
  outnode->node = target;
  outnode->iidx = ip_idx;

  // if n is a filter, set the details for the src output
  // for other source types (layer, blank), the ouputs will produce whatever the input requires
  switch (n->model_type) {
  case NODE_MODELS_INSTANCE: {
    weed_channel_t *channel;
    weed_chantmpl_t *chantmpl;
    uint64_t sflags;

    if (outnode->flags & NODEFLAGS_SKIP) continue;

    channel = get_enabled_channel(instance, 0, TRUE);
    chantmpl = weed_channel_get_template(channel);
    sflags = weed_chantmpl_get_flags(chantmpl);

    // get palette list from channel template
    outnode->pals = weed_chantmpl_get_palette_list(filter, chantmpl, &npals);

    // get current palette, width and height from channel
    outnode->cpal = weed_channel_get_palette(channel);
    outnode->cwidth = weed_channel_get_width_pixels(channel);
    outnode->cheight = weed_channel_get_height(channel);
      
    if (sflags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
      outnode->flags |= NODESRC_REINIT_SIZE;
    if (sflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
      outnode->flags |= NODESRC_REINIT_PAL;
    if (sflags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
      outnode->flags |= NODESRC_REINIT_RS;
  }
    break;
  case NODE_MODELS_SRC:
  case NODE_MODELS_CLIP:
    outnode->flags |= NODESRC_ANY_SIZE;
    outnode->flags |= NODESRC_ANY_PALETTE;
    break;
  default: break;
  }

  prime_ip = target->inputs[ip_idx] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));

  prime_ip->node = n;
  prime_ip->oidx = op_idx;

  npals = target->npals;
  if (prime_ip->npals) npals = prime_ip->npals;

  prime_ip->min_cost = (double *)lives_calloc(N_COST_TYPES * npals, sizdbl);
  prime_ip->prev_pal = (int *)lives_calloc(N_COST_TYPES * npals, sizint);

  for (int j = 0; j < npals; j++) {
    int idx = 0;
    for (int k = 0; k < N_COST_TYPES; k++) {
      prime_ip->min_cost[idx] = 0.;
      prime_ip->prev_pal[idx] = -1;
      idx++;
    }
  }

  if (prime_op->clones) {
    while (prime_op->clones[nclones++]); // eg x[0], x[1] null, nclones == 2
    // nclones + 1 == 3
    prime_op->clones = lives_recalloc(prime_op->clones, nclones + 1, nclones, sizeof(output_node_t *));
    // nclones - 1 == 1, x[2] null
    prime_op->clones[nclones - 1] = n->outputs[op_idx];
  }
  else {
    prime_op->clones = lives_calloc(2, sizeof(output_node_t *));
    prime_op->clones[0] = n->outputs[op_idx];
  }

  nclones = 0;
  if (prime_ip->clones) while (prime_ip->clones[nclones++]);
  
  if (target->moodel_type == NODE_MODELS_INSTANCE) {
    // for fx instances, now we set up the inputs according to instance in_channels
    weed_instance_t *instance = (weed_instance_t *)target->model_for;
    for (i = ip_idx + 1; i < target->n_inputs; i++) {
      weed_channel_t *channel;
      weed_chantmpl_t *chantmpl;
      uint64_t sflags;
      input_node_t *inpnode = target->inputs[i];

      if (inpnode->flags & NODEFLAGS_SKIP) continue;

      if (target->in_tracks[i] == track) {
	nclones++;
	inpnode->flags |= NODEFLAG_IO_CLONE;
      }

      channel = get_enabled_channel(instance, 0, TRUE);
      chantmpl = weed_channel_get_template(channel);
      sflags = weed_chantmpl_get_flags(chantmpl);

      if (sflags & WEED_CHANNEL_REINIT_ON_SIZE_CHANGE)
	inpnode->flags |= NODESRC_REINIT_SIZE;
      if (sflags & WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE)
	inpnode->flags |= NODESRC_REINIT_PAL;
      if (sflags & WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE)
	inpnode->flags |= NODESRC_REINIT_RS;

      inpnode->cwidth = weed_channel_get_width_pixels(channel);
      inpnode->cheight = weed_channel_get_height(channel);
      inpnode->cpal = weed_channel_get_palette(channel);
    }
  }
  return LIVES_RESULT_SUCCESS;
}


/* static lives_result_t append_node(inst_node_t *n, int track, inst_node_t * target) { */
/*   return prepend_node(target, track, n); */
/* } */


static node_chain_t *get_node_chain(LiVESList *node_chains, int track) {
  for (LiVESList *list = node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    if (nchain->first_node->dsource->track  == track) return nchain;
  }
  return NULL;
}


static node_chain_t *lowest_unterm_chain(LiVESList *node_chains) {
  node_chain_t *lchn = NULL;
  int min_track = -1;
  for (LiVESList *list = node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    if (!(nchain->flags & NODEFLAG_TERMINATED)
	&& (min_track == -1 || nchain->first_node->dsource->track < min_track)) {
      min_track = nchain->first_node->dsource->track;
      lchn = nchain;
    }
  }
  return lchn;
}


static void est_Costs_for_src(inst_node_t * n, int ni) {
// for srcs we have a set of possible palettes and various clip_srcs
// for the region betweemn start and end we can count the number of frames for each clip_src
// then this acts like a type of fractional palette

//  --- for example we may have img src - palette RGBA32
// -- decoder src - palette yuv420p
// if we have 20% decoded frames and 80% undecodec then we calculate costs for RGBA32 and for YUV420P and

// multiply the formar by .2 and the later by .8 to give the calulate d cost

// we treat each clip_src as a "ghost" output, wiht own palette list, and we get the tcost and qcost
// connected to a ghsot output with palette list equivalnt to the clipo_ssrc pals
// the palette list for the src is " borrowed" from the first ouput, (to be optimised...)
// the as for any other node, for each out pal, we find min in pal from the clip srcs, however when totalling to din 

// strating from the src, we go throught the "borrowed" out palettes,
// then as usual find min in pals, and total over thsi to get value fro "out pal"
// this gives the optimal "out pal", which becoems the src out pal, there is no conversion cost to the first  next
// inst_node / sink node,

// additionally when counting toalling the frames for each clip_src we can apply
///a distance factro from current frame to prioritse the near region

// when we total across inputs, rather than finding max or multiplying we, multiply the values by the

// we have a hiddend "internal cost" caused by switching between clip_srcs
// thus for each possible intermeadi
// all clip src wil convert to this intermediary,

// calc costs_from_src(inst_node_t *n, int ni) {
// set pals, npals for src to be equivalnt to input

// calc percentage of frames for eachinput, by finding clip and its frame_index


// temporarily add clip_srcs as ouputs connecting to inputs for the node
// - for each "out_pal"m, calc costs for each input
// calc costs for each "input"





}




static lives_result_t add_src_node(lives_nodemodel_T
				   nst_node_t *n, int track) {
  // check first to ensure we don't have an active uncommited node_chain
  node_chain_t *nchain = get_node_chain(*node_chains, track);
  if (nchain && !(nchain->flags & NODEFLAG_TERMINATED)) return LIVES_RESULT_ERROR;

  n->dsource->track = track;
  nchain = (node_chain_t *)lives_calloc(1, sizeof(node_chain_t));
  nchain->first_node = nchain->last_node = n;
  *node_chains = lives_list_prepend(*node_chains, (void *)nchain);
  return LIVES_RESULT_SUCCESS;
}


static node_chain_t *fork_output(LiVESList **node_chains, inst_node_t *n, int track) {
  // find node for in_track track,
  // and another output for the track, flagged with CLONE
  // when we arrive at node, we then just add a layer copy time
  for (int i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
    if (n->in_tracks[i] == track) {
      inst_node_t *p = n->inputs[i]->node;
      p->outputs = (output_node_t **)lives_recalloc(p->outputs, p->n_outputs + 1,
						    p->n_outputs, sizeof(output_node_t *));
      p->outputs[p->n_outputs] = (output_node_t *)lives_calloc(1, sizeof(output_node_t));
      p->outputs[p->n_outputs]->flags = NODEFLAG_IO_CLONE;
      p->out_tracks = (int *)lives_recalloc(p->out_tracks, p->n_outputs + 1,
					    p->n_outputs, sizint);
      p->out_tracks[p->n_outputs] = track;
      p->n_outputs++;
      
      add_src_node(node_chains, p, track);
      return (*node_chains)->data;
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
  // begin by checking the inputs
  weed_layer_t **layers = nodemodel->layers;
  int ntracks = nodemodel->ntracks;

  for (int i = 0; i < n->n_inputs; i++) {
    if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
    if (!n->inputs[i]->node) {
      int track = n->in_tracks[i];


      NOOO;
      xyzzy
  
  


      node_chain_t *in_chain = get_node_chain(nodemodel, track);
      // if we have no src, returnn to caller so it can add a src for the track
      if (!in_chain) return i;
      if (in_chain->terminated) {
	// if we do have a node_chain for the track, but it is terminated
	// then we are going to back up one node and a clone output
	// this means that the track sources will fork, one output will go to
	// the terminator node, while the other output will be a layer copy
	// creating a new node_chain for this track, 
	// which will be an input to this node
	in_chain = fork_output(&nodemodel->node_chains, in_chain->last_node, track);
	// mark this input as a clone, otherewise we would be recalculating the
	// palette for each output
	
	n->inputs[i]->flags |= NODEFLAG_IO_CLONE;
      }
      prepend_node(in_chain->last_node, track, n);
      in_chain->last_node = n;

      for (j = 0; j < n->n_outputs; j++) {
	if (n->outputs[i]->flags & NODEFLAGS_SKIP) continue;
	if (n->out_tracks[j] == track) break;
      }
      if (j == n->n_outputs) in_chain->terminated = TRUE;
    }
  }

  // after connecting all inputs, we check outputs
  // if the the out_track lacks a corresponding in track
  // if the track has an unconnected src this is
  // an error, otherwise this node becomes a src for the track
  for (j = 0; j < n->n_outputs; j++) {
    if (n->outputs[j]->flags & NODEFLAGS_SKIP) continue;
    for (i = 0; i < n->n_inputs; i++) {
      if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
      if (n->out_tracks[j] == n->in_tracks[i]) break;
    }
    if (i == n->n_inputs) {
      // output but no input, add src
      node_chain_t *in_chain = get_node_chain(nodemodel, n->out_tracks[j]);
      // check to make sure there is no unterminated node_chain for this track
      // we cannot have > 1 unterminated chain simultaneously on same track
      // in addition, if there is a layer for the track, and no node_chain,
      // this is not allowed - we can consider this an unterminated chain with 0 nodes
      if (!in_chain && layers[out_tracks[j|]] ||
	  (in_chain && !in_chain->terminated)) {
	// TODO - throw error
	n->outputs[j]->flags |= NODEFLG_IO_IGNORE;
      }
      else add_src_node(nodemodel, n, n->out_tracks[j]);
    }
  }
  return -1;
}


static int node_idx = 0;

// create a new inst_node, create teh inputs and outputs
//

// nodetpye can heve the follwoing values:
/* NODE_MODELS_CLIP		*/
/* NODE_MODELS_INSTANCE		*/
/* NODE_MODELS_OUTPUT		*/
/* NODE_MODELS_SRC		*/
/* NODE_MODELS_INTERNAL		*/

static inst_node_t *create_node(int nodetype, void *source, int nins, int *in_tracks, int nouts,
				int *out_tracks) {
  inst_node_t *n = (inst_node_t *)lives_calloc(1, sizeof(inst_node_t));
  int i;

  if (!n) return NULL;

  n->idx = node_idx++;
  n->model_type = modeltype;
  n->models_for = source;

  g_print("created node %p with %d ins aand %d outs\n", n, nins, nouts);

  // allocate input and output subnodes, but do not set the details yet
  // for efficiency we only do this when a connection is made

  if (modeltpye == NODE_MODELS_CLIP) n_clip_srcs = nins;
  else n->n_inputs = nins;

  if (nins) {
    n->inputs = (input_node_t **)lives_calloc(nins, sizeof(input_node_t *));
    for (i = 0; i < n->n_inputs; i++) {
      n->inputs[i] = (input_node_t *)lives_calloc(1, sizeof(input_node_t));
    }
  }

  if (nouts) {
    n->n_outputs = nouts;
    n->outputs = (output_node_t **)lives_calloc(nouts, sizeof(output_node_t *));
    for (i = 0; i < n->n_outputs; i++)
      n->outputs[i] = (output_node_t *)lives_calloc(1, sizeof(output_node_t));
  }

  // now set some type specific things
  // - 
  switch (modeltype) {
  case NODE_MODELS_CLIP: {
    lives_clip_t *sfile = (lives_clip_t *)source;
    int fcounts[2];

    fcounts[0] = fcounts[1] = 0;
    if (sfile->frame_index) {
      // TODO - calulate only for region being played
      fcount[1] = count_virtual_frames(sfile->frame_index, 1, sfile->frames);
    }
    fcount[0] = sfile->frames - fcount[1];

    // create fake inputs from the clip_srcs
    for (i = 0; i < nins; i++) {
      // node srcs are currently hardcoded, this iwll change
      inpnode = n->inputs[i];
      // set pals, cpal, size and f_ratio
      if (!i) {
	// create an input src for img_decoder
	inpnode->width = sfile->hsize;
	inpnode->height = sfile->vsize;
	if (sfile->bpp == 32) inpnode->cpal = WEED_PALETTE_RGBA32;
	else inpnode->cpal = WEED_PALETTE_RGB24;
	inpnode->pals = (int *)lives_calloc(2, sizint);
	inpnode->npals = 1;
	inpnode->pals[0] = inpnode->cpal;
	inpnode->pals[1] = WEED_PALETTE_END;
	inpnode->gamma_type = WEED_GAMMA_SRGB;
      }
      else {
	// create an input src for the decoder plugin
	inpnode->f_ratio = (double)((int)(fcount[1] * 10) / sfile->frames) / 10.;
	pthread_mutex_lock(&sfile->source_mutex);
	if (sfile->primary_src && sfile->primary_src->src_type == LIVES_SRC_TYPE_DECODER) {
	  lives_decoder_t *dplug = (lives_decoder_t *)sfile->primary_src->source;
	  if (dplug) {
	    lives_clip_data_t *cdata = dplug->cdata;
	    inpnode->pals = cdata->palettes;
	    inpnode->cpal = cdata->current_palette;
	    inpnode->width = cdata->width;
	    inpnode->height = cdata->height;
	    // gamma type not set until we actually pull pixel_data
	    // yuv details (subsopace, sampling, clamping)
	    // not set until we actually pull pixel_data
	  }
	}
	pthread_mutex_unlock(&sfile->source_mutex);
      }
    }
    n->inputs[0]->f_ratio = 1. - n->inputs[1]->f_ratio;
  }
    break;
  case NODE_MODELS_INSTANCE:
    weed_instance_t *instance = (weed_instance_t *)source;
    weed_filter_t *filter = weed_instance_get_filter(instance, TRUE);
    int filter_flags = weed_filter_get_flags(filter);

     n->out_tracks = out_tracks;

    // prefers linear gamma
    if (filter_flags & WEED_FILTER_PREF_LINEAR_GAMMA)
      n->flags |= NODESRC_LINEAR_GAMMA;

    if (nins) {
      n->in_tracks = in_tracks;
      // filter instance
    }
    else {
      // generator
    }
    break;
  case NODE_MODELS_SRC:
    // some other type, eg blank frame src

    lives_clip_t *sfile = (lives_clip_t *)source;
    // create outputs




    
    
    break;
  case NODE_MODELS_OUTPUT: {
    int npals = 1;
    int cpal = mainw->vpp->palette;
    if (mainw->vpp->capabilities & VPP_CAN_CHANGE_PALETTE) {
      int *pal_list = (*mainw->vpp->get_palette_list)();
      for (npals = 0; pal_list[npals] != WEED_PALETTE_END; npals++);
      xplist = (int *)lives_calloc(npals + 1, sizint);
      for (i = 0; i < npals; i++) xplist[i] = pal_list[i];
    }
    else {
      xplist = (int *)lives_calloc(2, sizint);
      xplist[0] = cpal;
    }
    xplist[npals] = WEED_PALETTE_END;

    n->inputs[0]->cpal = cpal;
    n->inputs[0]->npals = npals;
    n-->inputs[0]->pals = xplist;

    get_player_size(&nodemodel->opwidth, &nodemodel->opheight);
    n->inputs[0]->width = nodemodel->opwidth;
    n->inputs[0]->height = nodemodel->opheight;
  }
    break;
  case NODE_MODELS_INTERNAL:
    if (!nouts) {
      int *xplist = (int *)lives_calloc(3, sizint);
      xplist[0] = WEED_PALETTE_RGB24;
      xplist[1] = WEED_PALETTE_RGBA32;
      xplist[2] = WEED_PALETTE_END;
      n->inputs[0]->cpal = WEED_PALETTE_RGB24;
      n->inputs[0]->npals = 2;
      n-->inputs[0]->pals = xplist;

      get_player_size(&nodemodel->opwidth, &nodemodel->opheight);
      n->inputs[0]->width = nodemodel->opwidth;
      n->inputs[0]->height = nodemodel->opheight;
    }
    break;
  default: break;
  }
  return n;
}


static void free_inp_nodes(int ninps, input_node_t **inps) {
  for (int i = 0; i < ninps; i++) {
    if (inps[i]) {
      if (inps[i]->prev_pal) lives_free(inps[i]->prev_pal);
      if (inps[i]->min_cost) lives_free(inps[i]->min_cost);
      lives_free(inps[i]);
    }
  }
}


static void free_node(inst_node_t *n) {
  if (n) {
    if (n->n_inputs) free_inp_nodes(n->n_inputs, n->inputs);
    for (int i = 0; i < n->n_outputs; i++)
      if (n->outputs[i]) lives_free(n->outputs[i]);
    if (n->min_cost) lives_free(n->min_cost);
    if (n->pals) lives_free(n->pals);
    if (n->in_tracks) lives_free(n->in_tracks);
    if (n->out_tracks) lives_free(n->out_tracks);
    if (n->dsource) {
      if (n->dsource->purpose == SRC_PURPOSE_MODEL)
	lives_free(n->dsource);
    }
    lives_free(n);
  }
}


static boolean pal_permitted(inst_node_t *n, int pal) {
  // check if palette is allowed according to node restriction flagbits
  if (pal != n->cpal
      && ((n->flags & NODEFLAG_LOCKED)
	  || ((n->flags & NODEFLAG_NO_REINIT)
	      && ((n->flags & NODESRC_REINIT_PAL)
		  || ((n->flags & NODESRC_REINIT_RS)
		      && weed_palette_get_bytes_per_pixel(n->cpal)
		      != weed_palette_get_bytes_per_pixel(pal))))))
    return FALSE;

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
  if (in->flags & NODEFLAGS_SKIP) return FALSAE:
				    if (!in->npals) return pal_permitted(n, pal);
  if (pal != in->cpal
      && ((in->flags & NODEFLAG_LOCKED)
	  || ((n->lags & NODEFLAG_NO_REINIT)
	      && ((n->flags & NODESRC_REINIT_PAL)
		  || ((n->flags & NODESRC_REINIT_RS)
		      && weed_palette_get_bytes_per_pixel(in->cpal)
		      != weed_palette_get_bytes_per_pixel(pal))))))
    return FALSE;

  if ((in->flags & NODEFLAG_ONLY_RGB) && !weed_palette_is_rgb(pal)) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_YUV) && !weed_palette_is_yuv(pal)) return FALSE;
  if ((in->flags & NODEFLAG_PRESERVE_ALPHA) && !weed_palette_has_alpha(pal)) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_PACKED) && weed_palette_get_nplanes(pal) > 1) return FALSE;
  if ((in->flags & NODEFLAG_ONLY_PLANAR) && weed_palette_get_nplanes(pal) == 1) return FALSE;
  return TRUE;
}

static boolean output_pal_permitted(inst_node_t *n, int idx, int pal) {
  // check if palette is allowed according to node restriction flagbits
  output_node_t *in;
  if (!n || idx < 0 || idx >= n->n_outputs) return FALSE;
  out = n->outputs[idx];
  if (out->flags & NODEFLAGS_SKIP) return FALSE;
  if (!out->npals) return pal_permitted(n, pal);
  if (pal != out->cpal
      && ((out->flags & NODEFLAG_LOCKED)
	  || ((n->flags & NODEFLAG_NO_REINIT)
	      && ((n->src_flags & NODESRC_REINIT_PAL)
		  || ((n->src_flags & NODESRC_REINIT_RS)
		      && weed_palette_get_bytes_per_pixel(out->cpal)
		      != weed_palette_get_bytes_per_pixel(pal))))))
    return FALSE;

  if ((out->flags & NODEFLAG_ONLY_RGB) && !weed_palette_is_rgb(pal)) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_YUV) && !weed_palette_is_yuv(pal)) return FALSE;
  if ((out->flags & NODEFLAG_PRESERVE_ALPHA) && !weed_palette_has_alpha(pal)) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_PACKED) && weed_palette_get_nplanes(pal) > 1) return FALSE;
  if ((out->flags & NODEFLAG_ONLY_PLANAR) && weed_palette_get_nplanes(pal) == 1) return FALSE;
  return TRUE;
}


static LiVESList *_add_sorted_cdeltas(LiVESList *cdeltas, int out_pal, int in_pal,
				      double *costs,  int sort_by, boolean replace,
				      int n_gamma_type, int p_gamma_type) {
  // update cdeltas for an input
  // if replace is set we find an old entry to remove and re-add
  // sort_by is the cost type to order the cdeltas
  
  LiVESList *list = NULL;
  cost_delt_t *cdelta;

  if (cdeltas) list = cdeltas;

  if (replace) {
    for (list = cdeltas; list; list = list->next) {
      cdelta = (cost_delta_t *)list->data;
      if (cdelta->out_pal == out_pal && cdetla->in_pal == in_pal && cdelta->gamma_type == gamma_type) break;
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

  cdelta = (cost_delta_t *)lives_calloc(1, sizeof(list_delta_t));
  cdelta->in_pal = in_pal;
  cdelta->out_pal = out_pal;
  cdelta->deltas = costs;
  cdelta->gamma = gamma_type;

  for (list = cdeltas; list; list = list->next) {
    cost_delta_t *xcdelta = (cost_delta_t *)list->data;
    if (xcdelta->deltas[sort_by] > c_delta) {
      cdeltas = lives_list_prepend(list, (void *)cdelta);
      break;
    }
    listend = list;
  }
  if (!list) cdeltas = lives_list_append(listend, (void *)cdelta);

  return cdeltas;
}


#define _FLG_GHOST_COSTS	1
#define _FLG_ORD_DESC		2

#define _FLG_ENA_GAMMA		16

static void _calc_costs_for_input(input_node_t *in, int *pal_list, int j, 
				  int flags, double *factors) {
  inst_node_t *p = in->node;
  out_node_t *out = p->outputs[in->oidx];
  int npals, cpal, *pals;
  int ncopies = 0;
  int gamma_type = WEED_GAMMA_UNKNOWN, p_gamma_type = WEED_GAMMA_UNKNOWN;
  boolean ghost = FALSE, enable_gamma = FALSE;

  if (flags & _FLG_GHOST_COSTS) ghost = TRUE;
  if (flags & _FLG_ENA_GAMMA) enable_gamma = TRUE;

  if (out->npals) {
    npals = out->npals;
    pals = out->pals;
    cpal = out->cpal;
  }
  else {
    npals = p->npals;
    pals = p->pals;
    cpal = p->cpal;
  }

  out_size = p->width * p->height;
  in_size = in->width * in->height;

  in->flags & NODESRC_ANY_SIZE) in_size = out_size;

  if (in->clones) while (in->clones[ncopies - 1]) ncopies++;

  // test with gamma conversions applied; when optmising we will test the effects of disabling this
  if (enable_gamma && (n->flags & NODESRC_LINEAR_GAMMA))
    gamma_type = WEED_GAMMA_LINEAR;
  if (p->sflags & NODESRC_LINEAR_GAMMA) p_gamma_type = WEED_GAMMA_LINEAR;

  for (int i = 0; i < npals; i++) {
    int ogamma_type = gamma_type;
    double ccost = 0.;

    // apply node restrictions
    if ((!out->npals && !pal_permitted(p, pals[i]))
	|| (out->npals && !output_pal_permitted(out, pals[i]))) continue;

    if (gamma_type == WEED_GAMMA_LINEAR && !weed_palette_is_rgb(pal_list[j])
	&& !weed_palette_is_rgb(pals[i]) && p_gamma_type != WEED_GAMMA_LINEAR)
      ogamma_type = WEED_GAMMA_UNKNOWN;

    // find cost_delta. This is computed according to
    // cost_type, in_pal, out_pal, out_pal_list, in size and out size

    // for time:
    // - if the output is a clone, or has clones, add the layer copy time, based on out_pal and size
    //   layer
    //  - (for clones, we add a copy time to original and clone -in fact if we have n clones,
    //	we need to first copy the original, then select a copy to make another copy from)
    //   - add 1 copy time to the orignal, then we can select whether to send one or both coppies
    //    to inputs, or whether to copy again. If there are enough copies for out clones then all
    //    copies now go to remaining outputs. Otherwise if we have N copies not sent to layers,
    //    we can copy up to N of them in threads. However if two inpputs connected to outputs
    //    have same in palette, same size, same gamme, and the input is not inplace, we can
    //   elect to do the copy starting from the Ready Time for the node connected to the output we will copy from)
    //   in this case there is only a time cost for the clone. IN all other cases, we add 1 X copy time
    //   to the oriignal output,
    //  We will do this calculation after building the inital cost model, where we assume simply that each output
    // has 1 copy time cost. When we do the step to reconcile ouput palettes, we will adjust this, along with the
    // node palette.

    // - if the output size and input size differ, and the input is not flagged as size_any
    //     then we add the resize cost, based on out_pal, in_pals and max(out_size, in_size)
    //	 in some cases we use letterboing, the resize cost may vary in this case
    //  - if out_pal and in_pal differ, for some palette combinations, the conversion is done
    //	 in combination with resize, otherwise the conversion time cost is added
    //	 this is dependant on in_pal, out_pal, and out_size
    //  - if the node prefers linear gamma, this may alter costs
    //  but we calulate this via the priority list adjustments
    //
    //	 - if the out palette is flagged as linear gamme, then there is no extra cost at the
    //	  input, but there may be an addition for converting back, at the ouptut
    //	 - otherwise:
    //       - in some cases, the gamma conversion can be combined with a resize / pal conversion
    //	     in which case there is no extra cost, we simply do the conversion simultaneously
    //	   - otherwise we calculate deltas twice, once without the conversion, once with
    //	     for the latter:
    //
    //	     - if the input palette is YUV, then
    //		- if out pal is YUV, then ther is a possibilty it was converted from RGB with
    //		  linear gamma, in this case there is no cost to keep it in linear  gamma
    //	        - if the out_pal is RGB
    //	         - if resize, pal conv are combined, do the gamma change before resizing
    //		 - else do this before resize if upsizing, or after, if downsizing
    //	       what we have now is YUV with linear gamma. This is non standard, but it is what we will
    //	       go with.
    //		- If the following node does not support linear gamma:
    //		 - if it is YUV, this is worst case scenario, we must conver to RGB, convert gamme
    //		     then convert back to YUV, therfore we set the "qbonus" for conveting to lienar gamma
    //			< the qloss converitng to rgb and back
    //
    // otherwise the extra time cost / qloss is borne by teh following node
    //
    // what we shall do is to get the initial costs without the gamma conversion
    // then add the alteration via the priority list

    for (k = 1; k < N_COST_TYPES; k++) {
      double delta_cost = get_pal_conversion_cost(k, pals[i], pal_list[j], pal_list,
						  out_size, in_size, ncopies, ogamma_type, p_gamma_type);

      // add on the (possibly palette dependent) processing time for the prvious node
      // if the connected output has its own palette list, then we cannot determine the palette
      // dependent processing times
      if (k == COST_TYPE_TIME && !out->npals)
	delta_cost += _get_proc_time(p, pals[i], out_size);
	
      costs[k] = delta_cost;
      ccost += factors[k - 1] * delta_cost;
    }

    costs[COST_TYPE_COMBINED] = ccost;
	
    // add to cdeltas for input, sorted by increasing ccost
    in->cdeltas = _add_sorted_cdeltas(in->cdeltas, j, i, costs, COST_TYPE_COMBINED, FALSE, ogamma_type,
				      p_gamma_type);

    /* g_print("%d %f and %f\n", in->prev_pal[j * N_COST_TYPES + k], */
    /* 	  total_cost, in->min_cost[j * N_COST_TYPES + k]); */
    for (k = 0; k < N_COST_TYPES; k++) {
      // note the palette producing the lowest delta for each cost_type
      if (in->prev_pal[j * N_COST_TYPES + k] == -1
	  || costs[k] < in->min_cost[j * N_COST_TYPES + k]) {
	in->min_cost[j * N_COST_TYPES + k] = costs[k];
	in->prev_pal[j * N_COST_TYPES + k] = i;
	g_print("SET2 a prev_pal to %d for node %d->%d\n", i, n->idx, in->node->idx);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
  if (flags & _FLG_ORD_DESC) in->cdeltas = lives_list_reverse(in->cdeltas);
}


static void compute_all_costs(inst_node_t *n, int ord_ctype, double *factors, int flags) {
  // given a node n, we compute costs for all in / out pairs and add these to in->cdeltas for each input
  // the entries will be ordered by ascending cost of the specifies type
  // setting ORD_DESC will reverse the cdeltas list ordering
  // GHOST_COSTS will add "fake" costs which are transitory costs only applied when changin palette
  //  such costs are ignored when computing "true" costs
  // setting ENA_GAMMA will force linear gamma if the node prefers it
  //   (invalid if palette is yuv, prev palette was also yuv, and prev gamma type was not linear)

  inst_node_t *p = NULL;
  input_node_t *in;
  cost_delta_t *cdelta;
  double tcost = 0;
  double delta_cost;

  boolean engamma = FALSE;

  int ni, i, j, k;
  int max_in = n->n_inputs;
  int npals, cpal, *pal_list;

  if (flags & _FLG_GHOST_COSTS) ghost = TRUE;

  if (!n->inputs) max_in = 1;
  
  for (ni = 0; ni < max_in; ni++) {
    // we will go tehough each input separately, in case any has a private palette list,
    // then after that for the node itself, which will take care of the mojority of inputs, which
    // use the instance palette
    if (ni < n->n_inputs) {
      in = n->inputs[xni];
      if (in->flags & NODEFLAGS_SKIP) continue;
      npals = in->npals;
      if (npals) {
	pal_list = in->pals;
	cpal = in->cpal;
      }
      else {
	npals = n->npals;
	pal_list = n->pals;
	cpal = n->cpal;
      }

      // iterate over all out pals, either for the node or for a specific input
      // cross reference this against the in pals available at the input 
      for (j = 0; j < npals; j++) {
	//double xtracost_post = 0.;
	double prev_cost, total_cost;

	// apply node restrictions
	if (in && !input_pal_permitted(n, xni, pal_list[j])) continue;
	if (!in && !pal_permitted(n, pal_list[j])) continue;

	// go through all inputs and calculate costs for each in_pal / out_pal pair
	// storing this in cdeltas for the input_node
	// we find the ouput_node which the input connects to, and if it has npals > 0
	// we use the palette list from the output, otherwise we use the list from the node
	// we optimise independently for each output, if this leads to conflicting palettes in prv node
	// then that will be resolved once we are able to calulate combined cost at the sink

	if (!n->inputs) {
	  // for sources, there is no time cost to switch palette, however thie may affect proc_time
	  // and in addition we will have a qloss ghost cost to represent visual changes,
	  // the cost being equivalent to the cost converting from cpal to pal, or 0.5 * cost for pal to cpal
	  // whichever is the greater
	  n->min_cost[j * N_COST_TYPES + COST_TYPE_TIME] = proc_time;//_get_proc_time(n, pal_list[j], out_size);
	  if (ghost) {
	    // TODO - make this part of the calc function
	    double delta_costf + get_pal_conversion_cost(COST_TYPE_QLOSS, cpal, pal_list[j], pal_list,
							 out_size, out_size, ncopies, FALSE);
	    fouble delta_costb + (1. - (1. - get_pal_conversion_cost(COST_TYPE_QLOSS, pal_list[j], cpal, pal_list,
								     out_size, out_size, ncopies, FALSE)) / 2.);

	    if (delta_costf > delta_costb)
	      n->min_cost[j * N_COST_TYPES + COST_TYPE_QLOSS] = delta_costf;
	    else n->min_cost[j * N_COST_TYPES + COST_TYPE_QLOSS] = delta_costb;
	  }
	  else n->min_cost[j * N_COST_TYPES + COST_TYPE_QLOSS] = 1.;
	  continue;
	}
	// make cdeltas for each in / out pair

	calc_costs_for_input(in, pal_list, j, flags, factors);
	// *INDENT-OFF*
      }}}
  // *INDENT-ON*
}


static void trace_to_src(inst_node_t *n, int oper) {
  // for each input, we iterate over cost_types. Since for the current node we already have
  // best_pal[cost_type], we can thus look up the prev node in pal with min cost,
  // and we set best_pal[cost_type] for each input node, ascedning the branches depth first recursively

  static int depth = 0;
  int k, ni;

  depth++;
  g_print("trace to src %d %d\n", depth, n->n_inputs);

  // in case a node has multiple outputs, we can only optimise for one of these (unless the output has its
  // own palette_list)
  // we will iterate over all inputs
  // we will find the output idx for the prev node feeding to the input
  // if the output has its own palette_list, then we find best_pal for the output
  // if the output does not, then we need to optimise the global palette, to find which of the outputs we
  // should use/ For now we just use the first output to reach here

  if (oper == 0) {
    if (n->n_outputs) {
      for (no = 0; no < n->n_outputs; no++) {
	output_node_t *out = n->outputs[no];
	indt_node_t *nxt = out->node->inputs[out->iidx];
	if (out->flags & NODEFLAGS_SKIP)continue;{
	  if (out->npals) {
	    out->optimal_pal = out->best_out_pal[k] = nxt->best_in_pal[k];
	  }
	  else {
	    if (no == 0) {
	      n->optimal_pal = n->best_pal_up[k] = nxt->best_in_pal[k];
	      // *INDENT-OFF*
	    }}}}}
    // *INDENT-OFF*
  
    if (n->n_inputs) {
      if (oper == 1) {
	// if we have unprocessed outputs, we have to pause here
	// once all are processed, we find min slack for all, and this value gets added for each input
	for (int no = 0; no < n->n_outputs; no++) {
	  if (!(n->outputs[no]->flags & NODEFLAG_PROCESSED)) return;

	  // otherwise, find min tot_slack for all outputs, and we add this to the next node (going up)
	  // outputs slack to deriver their total slack
	  if (!no || n->outputs[no]->total_slack < minslack)
	    minslack = n->outputs[no]->total_slack;
	}
      }

      for (int ni = 0; ni < n->n_inputs; ni++) {
	input_node_t *in = n->inputs[ni];
	output_node *out;
	inst_node_t *p;
	// again, we ignore cloned inputs, they don't affect the previous palette
	if (in->flags & (NODEFLAGS_SKIP | NODEFLAG_IO_CLONE)) continue;

	p = in->node;
	out = p->outputs[in->oidx];

	if (oper == 1) {
	  // find slack and add to total
	  double slack = (double)(n->ready_time - p->ready_time) / TICKS_PRE_SECOND_DBL
	    - cdelta->deltas[COST_TYPE_TIME];
	  out->slack = slack;
	  out->total_slack = slack + minslack;
	  // when optimising, we can "spend" slack to increase tcost, without increasing combined
	  // cost at the sink
	}
	else {
	  if (p->flags & NODEFLAG_LOCKED) continue;
	  // nodes can have more than one output, but pnly the first output is capable of defining the
	  if (in->oidx > 0 && !out->npals) continue;
	}
      }

      if (oper == 1) {
	// create tuples for all combos of in / out pals for all inputs
	LiVESList *prio_list = backtrack(n, 1);
	for (k = 0; k < N_COST_TYPES; k++) {
	  // find a tuple with min cost for each type
	  cost_tuple_t *tup = best_tuple_for(k);
	  for (ni = 0; ni < n->n_inputs; ni++) {
	    n->inputs[ni]->best_in_pal = tuple->palconv[ni]->in_pal;
	    n->inputs[ni]->best_out_pal = tuple->palconv[ni]->out_pal;
	    if (!n->inputs[i]->npals) n->best_pal_up = n->optimal_pal = tuple->palconv[ni]->out_pal;
	  }
	}
	free_prio_list(prio_list);
      }
      trace_to_src(p, oper);
    }

    depth--;
  }
}


// trace back up from sink to sources, at each node we test all valid in / out pal combos
// then descend and find costs at sink
static void ascend_and_backtrace(inst_node_t *n, double *factors, double *thresh) {
  // TODO -
  // when we set up initial palettes, we note the "slack" in in_pal -> out_pal, the slack being tmax - tcost for the input
  // what thsi means is that for any node, we can increase tcost upt to tcost + slack without any additional overhead.
  // however, all routes between nod and sink must be checked, and we can only consider min(slack)
  // the reason is, suppose we increase tcost at n, and this pushes up tmax at the next node, the extra time will
  // be the delta - slack between src node and target. The reaming tcost is carried over to the next node,
  // again we may increase tmax, but only by an amount after subtracting the slack.
  // Thus if the increas is < total slack between n and sink, by the time we reach sink,
  // the cost has been consumed by reducing slack
  // thus, one optimisation is to find the best use of "slack", we can examine potential changes along the path
  // and find some combination which minimises qloss whilst increasing tcostonly up to slack
  // in addition, each time we increase tmax, this creates extra slack for all other inputs,
  // so we recalc the slack passing thsi back up to the src
  //
  // one potential optimisation is "slack rebalancing"
  // if we convert an in_pal to out pal, and this is tmax, then if the prev node supports the out+pal itself
  // we can consider doing the conversion ar prev node
  
  if (n->n_inputs) {
    // find optmial in pals / out pal or the node, whichever produces min ccosst at sink
    // and we set estimated in pals for inputs
    optimise_node(n, factors, thresh);
    // now we vary palettes for one input, and descend to find costs at sink
    for (int ni = 0; ni < n->n_inputs; ni++) {
      input_node_t *in = n->inputs[ni];
      inst_node_t *p;
      // again, we ignore cloned inputs, they don't affect the previous palette
      if (in->flags & (NODEFLAGS_SKIP | NODEFLAG_IO_CLONE)) continue;
      // nodes can have more than one output, but pnly the first output is capable of defining the
    }
  }
}


static void map_least_cost_palettes(inst_node_t *n) {
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
  trace_to_src(n, 0);
}


// we can also use this to combine any pair of costs
// when summing combined costs, we cannot just use C_tot = C_a + C_delta
// since C_a == f0 . tcost_a + f1 . qloss_a
// and C_delta == f0 . tcost_delta + f1 . qloss_delta

// adding we would get f0 . (tcost_a + tcost_delta) + f1 . (qloss_a + qloss_delta)
// this is wrong as we need qloss_a * qloss_delta
// so C_delta == f0. t_delta + f1 . q_delta
// unless we know eithr tcos or qloss, we cannot find the total
// thus whwn storing ccosts, we also need to store the componets costs

static double *total_costs(int nvals, double **abs_costs, double **costs, double *factors, double *out_costs, boolean *ign) {
  // here we sum costs for a set of inputs, to give the value for a node (set in out_costs)
  // abs_costs are the value from the prveious node 
  // for tcost, we take the max value, for qloss we multiply
  // then for combined we multiply by the factors and add
  double max[N_COST_TYPES];

  if (!sumty) {
    // set summation methods and def values for cost types
    sumty = TRUE;
    cost_summation[COST_TYPE_TIME] = SUM_MAX;
    cost_summation[COST_TYPE_QLOSS] = SUM_MPY;

    DEF_COST[COST_TYPE_COMBINED] = 0.;
    DEF_COST[COST_TYPE_TIME] = 0.;
    DEF_COST[COST_TYPE_QLOSS] = 1.;
  }

  for (int k = 0; k < N_COST_TYPES; k++) {
    out_costs[k] = DEF_COST[k];
    if (k == COST_TYPE_COMBINED) continue;
    for (int ni = 0; ni < nvals; ni++) {
      if (ign[ni]) continue;
      if (cost_summation[k] & SUM_MAX) {
	cost = abs_costs[ni][k] + costs[ni][k];
	if (!ni || cost > max[k]) {
	  out_costs[k] = max[k] = cost;
	}
      }
      if (cost_summation[k] & SUM_MPY) {
	out_costs[k] *= (1. - abs_costs[ni][k]) * costs[ni][k];
      }
    }
    if (cost_summation[k] & SUM_MPY)
      out_costs[k] = 1. - out_costs[k];
  }

  for (int k = 0; k < N_COST_TYPES; k++) {
    if (k != COST_TYPE_COMBINED) continue;
    out_costs[k] += factors[k - 1] * out_costs[k];
  }

  return out_costs;
}


static cost_tuple_t *make_tuple(inst_node_t *n, conversion_t *conv) {
  // conv is array of n->n_inputs, get in+pal, out+pal, find cdelta at input
  // get deltas and copy to tuple; if in->node has abs_costs, add these on
  cost_tuple_t *tup = (cost_tuple_t *)lives_calloc(1, sizeof(cost_tuple_t));
  double **cossts, **oabs_costs;
  cost_delta_t *cdelta;
  boolean *ign;
  if (!n->n_inputs) return NULL;

  costs + (double **)lives_calloc(n->n_inputs, sizeof(double *));
  oabs_costs + (double **)lives_calloc(n->n_inputs, sizeof(double *));
  ign + (boolean *)lives_calloc(n->n_inputs, sizeof(boolean));

  tup->node = n;
  for (int ni = 0; ni < n->n_inputs; ni++) {
    if (conv->flags & CONV_IGN) ign[ni] = TRUE;
    else {
      int ipal, opal;
      in = n->inputs[ni];
      if (in->flags & NODEFLAGS_SKIP) {
	ign[ni] = TRUE;
	continue;
      }
      ipal = conv[ni]->in_pal;
      opal = conv[ni]->out_pal;
      ign[ni] = FALSE;
      cdelta = find_cdelta(in, ipal, opal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
      costs[ni] = cdelta->deltas;
      p = in->node;
      oabs_costs[ni] = p->abs_cost;
      ign[ni] = FALSE;
    }
  }
  total_costs(n->n_inputs, oabs_costs, factors, tup->out_cost, ign);
  lives_free(ign); lives_free(oabs_costs); lives_free(costs);
  return tup;
}


statc void accumulate_slack(inst_node_t *n) {
  trace_to_src(n, 1);
}


static LiVESList *add sorted_tuple(LiVESList *priolist, cost_tuple_t *tup, int ord_type) {
  LiVESList *lastl = NULL;
  double mycost = tup->tot_cost[ord_type];
  for (LiVESList *list = priolist; list; list = list->next) {
    cost_tuple_t *xtup = (cost_tuple_t *)list->data;
    if (xtup->tot_cost[ord_type] > mycost) {
      priolist = lives_list_prepend(list, (void *)tup);
      break;
    }
    lastl = list;
  }
  if (!list){
    priolist = lives_list_append(lasll, (void *)tup);
  }
  return priolist;
}


static cost_tuple_t *best_tuple_for(int cost_Type) {
  cost_tuple_t *xtup = NULL;
  double minval = 0;
  for (LiVESList *list = priolist; list; list = list->next) {
    xtup = (cost_tuple_t *)list->data;
    if (lisst == priolist || xtup->tot_cost[cost_type] < minval) {
      minval = xtup->tot_cost[cost_type];
      mintup = xtup;
    }
  }
  return xtup;
}


static void free_tuple(cost_tuple_t *tup) {
  if (tup->palconv) lives_free(tup->palconv);
  lives_free(tup);
}


static ovid free_priolisst(LiVESList *plist) {
  for (LiVESList *list = plist; list; list = list->next) {
    free_tuple((cost_tuple_t *)list->data);
  }
  lives_list_free(plist);
}


static void backtrack(inst_node_t *n, int ni) {
  // we have calculated tcost deltas and qloss deltas for each in / out pair
  // we find the current in pal for each input, then
  // we now create tuples, going through the pal list for each input, we hav in / ou pairs
  // adding tcost delta to cumulative tcost at the ouputs, we can find absolute tcost
  // from this we go back over the out pals, and for the tuple find mas absolute tost
  // this then allows us to calculate the slack for the tuple for each input
  // we can also find cumulative values for other  cost ypes and with this and tmax, calulate combined cost for the node
  // we then pick whichever tuple has lowest ccost and set and lock the palettes accordingly
  // then recursvely we follow down all outputs until reaching the sink
  // from the sink then we go back up the tree setting cumulative values for slack
  static int common = WEED_PALETTE_NONE;
  static conversion_t *conv = NULL;
  static LiVESList *priolist = NULL;

  input_node_t *in = n->inputs[ni];
  int npals, *pals;
 
  if (!conv) {
    priolist = NULL;
    conv = (conversion_t *)lives_calloc(n->n_inputs, sizeof(conversion_t));
  }

  if (ni == n->n_inputs) {
    cost_tuple_t *tup = make_tuple(n, conv);
    return add_sorted_tuple(priolist, tup, COST_TYPE_COMBINED);
  }

  if (in->npals > 0) {
    npals = in->npals;
    pals = in->pals;
  }
  else {
    npals = n->npals;
    pals = n->pals;
  }

  for (int j = 0; j < npals; j++) {
    out_node_t *out = p->outputs[in->oidx];
    myconv = conv[ni];

    if (out->npals) {
      // if out palettes can vary, the palette is usually set by the host
      // or by the filter, however we can still calulate it
      inpals = out->npals;
      ipals = out->pals;
    }
    else {
      inst_node_t *p = in->node;
      // we can find delta costs for any palette, but abscosts only when on second descent
      inpals = p->npals;
      ipals = p->pals;
    }
    // we 
    // we already calculated costs for each in / out palette, these are in cdeltas for the input
    // the only delta 
       
    if (ni == 0) common = WEED_PALETTE_NONE;

    if (!in->npals) {
      if (common == WEED_PALETTE_NONE) common = j;
      else j = npals = common;
    }

    for (int i = 0; i < inpals; i++) {
      myconv->in_pal = ipals[i];
      myconv->out_pal = pals[j];
      // TODO - add gammas
    }

    backtrack(n, ni + 1);
  }
  conv = NULL;
  return priolist;
}


// so this is still a WIP
// the idea is that we would optimise the node model, firstly by targetting large cost deltas to try to reduc them
// then secondly by adding "Creative" changes
// another thing that could be done is to try to combine io heavy operations (e.g pulling from a local source) with
// COU intensive operations (fx processing
// and memory intensive (e.g pal conversions)
// if we measure a bottleneck in on of these we can trya djusting itmings
static void optimise_node(inst_node_t *n) {
  int *opals = NULL;
  double *tcmax, *tcmin;
  int *common = NULL;
  int *used = NULL;
  int nins = n->n+inputs;
  if (nins) {
    int *opals = lives_calloc(nins, sizint);
    boolean *used = (boolean *)lives_calloc(nins, sizint);
    double tmax, tmin;
    backtrack(n, 0);
    lives_free(popals);
    lives_free(used);
  }
  // TODO
}


static cost_delta_t *find_cdelta(input_node_t *in, int in_pal, int out_pal, int gamma_type, int p_gamma_type) {
  cost_delta_t *cdelta = NULL;
  for (LiVESList *list = in->cdelta; list; list = list_next) {
    cdelta = (cost_delta_t *)list->data;
    if (cdelta->in_pal == in_pal && cdelta->out_pal == out_pal) {
      // TODO - check gammas
      break;
    }
  }
  return cdelta;
}


static inst_node_t *desc_and_do_something(int do_what, inst_node_t *n, inst_node_t *p, lives_clip_src_t *,
					  int flags, double * factors);

static void desc_and_compute(inst_node_t *n, int flags, double * factors) {
  desc_and_do_something(0, n, NULL, NULL, flags, factors);
}

static void desc_and_free(inst_node_t *n) {
  desc_and_do_something(1, n, NULL, NULL, 0, NULL);
}

static void desc_and_clear(inst_node_t *n) {
  desc_and_do_something(2, n, NULL, NULL, 0, NULL);
}

static inst_node_t *desc_and_find_src(inst_node_t *n, lives_clip_src_t *dsource) {
  return desc_and_do_something(3, n, NULL, dsource,  0, NULL);
}

static void desc_and_total(inst_node_t *n, int flags, double *factors) {
  desc_and_do_something(4, n, NULL, NULL, flags, factors);
}

static void desc_and_get_sizes(inst_node_t *n) {
  desc_and_do_something(5, n, NULL, NULL, 0, NULL);
}

static inst_node_t *desc_and_do_something(int do_what, inst_node_t *n, inst_node_t *p,
					  lives_clip_src_t *dsource, int flags, double *factors) {
  static inst_node_t *first_inst = NULL;
  input_node_t *xn;
  boolean *ign;
  int i;
  
  if (!n) return NULL;
  if (do_what == 3 && (!dsource || !dsource->source)) return NULL;

  if (do_what == 4)
    ign = (boolean *)lives_calloc(n->n_inputs, sizeof(boolean));
  
  if (do_what == 4 && !p) first_inst = NULL;

  if (do_what == 0 || do_what == 1 || do_what == 4 || dp_what == 5) {
    // anything which needs all inputs processed
    if (p) {
      // mark input from prev node as "Processed"
      for (i = 0; i < n->n_inputs; i++) {
	if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
	if (n->inputs[i]->node == p && !(n->inputs[i]->flags & NODEFLAG_IO_CLONE))
	  n->inputs[i]->flags |= NODEFLAG_PROCESSED;
      }
    }

    // if we find any other inputs, just return, eventually these iwll be processed
    // and we will contnue down outputs
    for (i = 0; i < n->n_inputs; i++) {
      if (do_what == 4) ign[i] = TRUE;
      if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
      if (!(n->inputs[i]->flags & (NODEFLAG_PROCESSED | NODEFLAG_IO_CLONE)))
	return NULL;
      if (do_what == 4) ign[i] = FALSE;
    }
    // reset flags
    for (i = 0; i < n->n_inputs; i++) {
      if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
      n->inputs[i]->flags &= ~NODEFLAG_PROCESSED;
    }
    g_print("all inputs defined\n");


    if (do_what == 0 || do_what == 4) {
      // for op type 0, calculate min costs for each pal / cost_type
      // for op type 4, set abs_cost for node
      int nfails = 0;

      if (!(n->flags & NODEFLAG_PROCESSED)) {
	boolean ghost = FALSE;
	if ((flags & _FLG_GHOST_COSTS) && (n->flags & NODESRC_LINEAR_GAMMA)) ghost = TRUE;
	g_print("computing min costs\n");
	if (do_what == 0) compute_all_costs(n, factors, ghost);
	else if (do_what == 5) calc_node_sizes(n);
	else {
	  double **costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
	  double **oabs_costs = (double **)lives_calloc(n->n_inputs, sizeof(double *));
	  for (int ni = 0; ni < n->n_inputs; ni++) {
	    if (ign[ni]) continue;
	    xn = n->inputs[ni];
	    p = xn->node;
	    out = p->outputs[xn->oidx];
	    if (out->npals) in_pal = out->optimal_pal;
	    else in_pal = p->optimal_pal;

	    if (xn->npals) out_pal = xn->optimal_pal;
	    else out_pal = n->optimal_pal;

	    // ignore gamma for now
	    cdelta = find_cdelta(xn, in_pal, out_pal, WEED_GAMMA_UNKNOWN, WEED_GAMMA_UNKNOWN);
	    costs[ni] = cdelta->deltas;

	    oabs_costs[ni] = p->abs_cost;
	  }

	  total_costs(n->n_inputs, oabs_costs, factors, n->abs_cost, ign);
	  lives_free(oabs_costs); lives_free(costs); lives_free(ign);
	  n->ready_time = n->abs_cost[COST_TYPE_TIME] * TICKS_PER_SECOND_DBL;
	}
      }
    }

    // recurse: for each output, we descend until we the sink node
    // a fail return code indicates that somewhere lower in the chain we reached a node which
    // had unprocessed input(s), and then all we do is to continue going through the sources
    // eventually one will connect to the missing input, and we can continu to descend
    if (n->n_outputs) {
      do {
	int last_nfails = -nfails;
	nfails = 0;
	for (i = 0; i < n->n_outputs; i++) {
	  if (!(n->outputs[i]->flags &
		(NODEFLAGS_SKIP | NODEFLAG_PROCESSED))) {
	    if (!desc_and_do_something(do_what, n->outputs[i]->node, n, dsource, idx, factors)) nfails++;
	    else n->outputs[i]->flags |= NODEFLAG_PROCESSED;
	  }
	}
	if (!last_nfails || nfails < last_nfails) nfails = -nfails;
      } while (nfails < 0);
    }

    n->flags |= NODEFLAG_PROCESSED;
  
    if (!nfails) {
      // first we will flag the first output as pruned iff it has a matching track in in_tracks
      // (meaning it was part of the layer src node_chain)
      // if there are additional outputs, we will check for pruned flag
      // - these will be srcs for other node_chains
      // either the node_chain would have been processed and the output flagged as pruned
      // or else we have not yet reached the node_chain(s) starting here
      // when we do so, we need to have this node still around, so that we can check n->n_inputs
      // if this is > 0 then this node is going to be an instance src. When checking we will not descend the
      // outputs, because the real src will do that, or has done that.
      // however we will locate the first unflagged output for the node_chain track
      // track, and flag it as pruned
      // if all outputs are flagged as pruned and the node flagged as processed, it can be freed

      if (n->n_outputs) {
	int track = -1;
	for (i = 0; i < n->n_inputs; i++) {
	  if (n->inputs[i]->flags & NODEFLAGS_SKIP) continue;
	  if (n->inputs[i]->node == p) {
	    track = n->in_tracks[i];
	    break;
	  }
	}
	if (!n->n_inputs || track != -1) {
	  if (track != -1) {
	    for (i = 0; i < n->n_outputs; i++) {
	      if (n->outputs[i]->flags & (NODEFLAGS_SKIP | NODEFLAG_IO_CLONE)) continue;
	      if (n->out_tracks[i] == track) {
		n->outputs[i]->flags |= NODEFLAG_OUTPUT_PRUNED;
		break;
		// *INDENT-OFF*
	      }}}}}}
    // *INDENT-ON*
    else {
      for (i = 0; i < n->n_outputs; i++) {
	n->outputs[i]->flags &= ~NODEFLAG_PROCESSED;
      }
    }
    return n;
  }
  if (do_what == 1) {
    // free
    for (i = 0; i < n->n_outputs; i++) {
      desc_and_do_something(do_what, n->outputs[i]->node, n, dsource, idx, factors);
    }
    free_node(n);
  }
  else if (do_what == 2) {
    // clear processed flag from nodes
    for (i = 0; i < n->n_outputs; i++) {
      if (n->outputs[i]->flags & NODEFLAGS_SKIP) continue;
      desc_and_do_something(do_what, n->outputs[i]->node, n, dsource, idx, factors);
      n->flags &= ~NODEFLAG_PROCESSED;
    }
  }
  else if (do_what == 3) {
    // find node with matching source / src_type
    // check this node, then all outputs recursively
    if (n->dsource) {
      if ((!dsource->source && n->dsource->src_type == dsource->src_type)
	  || (dsource->source && n->dsource->source == dsource->source)) return n;
    }
    for (i = 0; i < n->n_outputs; i++) {
      if (n->outputs[i]->flags & NODEFLAGS_SKIP) continue;
      xn = desc_and_do_something(do_what, n->outputs[i]->node, n, dsource, idx, factors);
      if (xn) return xn;
    }
  }
  return NULL;
}
x

// optimeise combined cost at sink, adjusting each node in isolation
#define OPT_SGL_COMBINED	1

static void optimise_cost_model(lives_nodemodel_t *nodemodel, int method, double *thresh) {
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

  // however, beginning aat a src we cna pick a palette, then proceeding to next node, we already know that the
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
    if (n->n_ioututs) continue;
    //
    ascend_and_backtrack(n);;
    break;
  }
}


// when creating a node src from a layer, there is a range of possible model types
// if the layer is NULL or out of range, we create a blank frame src node
// otherwise we check the clip type for the layer clip
// this can be generator - then we are going to model an instance with - inputs
// otherwise it will likley be a standard clip - then we model the clip_srcs
// or it could be some exotic type, such as a webcam, fifo, tvcard, etc
static inst_node_t *create_node_for_layer(weed_layer_t **layers, int xtrack, int ntracks) {
  lives_clip_t *sfile;
  inst_node_t *n;
  int *pxtracks = (int *)lives_calloc(1, sizint);
  int nins = 0;

  // out_track[0]
  pxtracks[0] = xtrack;

  if (xtrack >= 0 && xtrack < ntracks && layers[xtrack]) {
    weed_layer_t *layer = layers[xtrack];
    int clipno = lives_layer_get_clip(layer);

    sfile = RETURN_VALID_CLIP(clipno);
    if (sfile) {
      switch (sfile->clip_type) {
      case CLIP_TYPE_NULL_VIDEO:
	sfile = NULL;
	break;
      case CLIP_TYPE_GENERATOR:
	n->model_type = NODE_MODELS_INSTANCE;
	if (sfile->primary_src && sfile->primary_src->source) {
	  n->model_for = sfile->primary_src->source;
	}
	break;
      case CLIP_TYPE_FILE:
	n->model_type = NODE_MODELS_CLIP;
	n->model_for = sfile;
	nins = -2;
	break;
      case CLIP_TYPE_DISK:
	n->model_type = NODE_MODELS_CLIP;
	n->model_for = sfile;
	nins = -1;
	break;
#ifdef HAVE_YUV4MPEG
      case CLIP_TYPE_YUV4MPEG:
	n->model_type = NODE_MODELS_SRC;
	n->model_for = sfile;
	break;
#endif

#ifdef HAVE_UNICAP
      case CLIP_TYPE_VIDEODEV:
	n->model_type = NODE_MODELS_SRC;
	n->model_for = sfile;
	break;
#endif	
	//case CLIP_TYPE_LIVES2LIVES:
	//break;
      default: break;
      }
    }
  }

  if (!sfile) {
    n->model_type = NODE_MODELS_SRC;
    n->models_for = static_srcs[LIVES_SRC_TYPE_BLANK];
  }

  n = create_node(n->model_type, nins, NULL, 1, pxtracks);

  for (int k = 0; k < _COST_TYPES; k++) n->abs_cost[k] = DEF_COST[k];

  return n;
}


void free_nodes_model(lives_nodemodel_t **nodemodel) {
  // use same algo as for computing costs, except we free instead
  if (nodemodel->node_chains) {
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      // ignore any srcs which have inputs, we will reach these via input sources
      if (n->n_inputs) continue;
      desc_and_free(n);
    }
    lives_list_free_all(&nodemodel->node_chains);
  }
}


char *get_node_name(inst_node_t *n) {
  char *ntype = NULL;
  if (n && n->dsource) {
    switch (n->dsource->src_type) {
    case LIVES_SRC_TYPE_LAYER:
      ntype = lives_strdup("layer source");
      break;
    case LIVES_SRC_TYPE_EFFECT:
    case LIVES_SRC_TYPE_FILTER: {
      weed_instance_t *inst = (weed_instance_t *)n->dsource->source;
      if (inst) ntype = weed_instance_get_filter_name(inst, TRUE);
    }
      break;
    case LIVES_SRC_TYPE_SINK:
      ntype = lives_strdup("sink");
      break;
    default: break;
    }
  }
  return ntype;
}


void describe_chains(LiVESList *node_chains) {
  for (LiVESList *list = node_chains; list; list = list->next) {
    node_chain_t *nch = (node_chain_t *)list->data;
    if (nch) {
      inst_node_t *n = nch->first_node;
      int track = n->dsource->track;
      char *str = NULL, *in_str = NULL, *out_str = NULL;

      g_print("Found %s node_chain for track %d\n", nch->terminate
	      ? "terminated" : "unterminated", track);
      g_print("Showing palette computation for COST_TYPE_COMBINED\n");
      while (1) {
	int pal = pal_list[n->optimal_pal];
	int gamma = n->optimal_gamma;
	char *node_dtl, *in_dtl, *out_dtl;
	char *ntype = get_node_name(n);

	node_dtl = lives_strdup_printf("[(%d) %s - %s (%s) (%d X %d), est. costs: time = %.4f, "
				       "qloss = %.4f, combined = %.4f]",
				       n->idx, ntype ? ntype : "??????",
				       weed_palette_get_name(pal), weed_gamma_get_name(n->gamma_type),
				       n->width, n->height, n->abs_cost[COST_TYPE_TIME],
				       n->abs_cost[COSTY_TYPE_QLOSS], n->abs_cost[COSTY_TYPE_COMBINED]);
	for (i = 0; i < n->n_inputs; i++) {
	  if (n->inputs[i]->npals) {
	    if (!in_str) in_str = lives_strdup(_"The following nodes have variant inputs:");
	    in_dtl = lives_strdup_printf("node %d:", n->idx); 
	    in_str = lives_concat_sep(in_str, "\n", in_dtl);
	    break;
	  }
	}
	if (i < n->n_inputs) {
	  for (i = 0; i < n->n_inputs; i++) {
	    if (n->inputs[i]->npals) {
	      in_dtl = lives_strdup_printf("\tinput %d: palette %s", i,
					   weed_palette_get_name(n->inputs[i]->optimal_pal)); 
	      in_str = lives_concat_sep(in_str, "\n", in_dtl);
	    }
	  }
	}
	for (i = 0; i < n->n_outputs; i++) {
	  if (n->outputs[i]->npals) {
	    if (!out_str) out_str = lives_strdup(_"The following nodes have variant outputs:");
	    out_dtl = lives_strdup_printf("node %d:", n->idx); 
	    out_str = lives_concat_sep(out_str, "\n", out_dtl);
	    break;
	  }
	}
	if (i < n->n_outputs) {
	  for (i = 0; i < n->n_outputs; i++) {
	    if (n->outputs[i]->npals) {
	      out_dtl = lives_strdup_printf("\toutput %d: palette %s", i,
					    weed_palette_get_name(n->outputs[i]->optimal_pal)); 
	      out_str = lives_concat_sep(out_str, "\n", out_dtl);
	    }
	  }
	}

	if (ntype) lives_free(ntype);
	str = lives_concat_sep(str, " -> ", node_dtl);
	if (n == nch->last_node) break;
	for (int i = 0; i < n->n_outputs; i++) {
	  if (n->outputs[i]->flags & NODEFLAGS_SKIP) continue;
	  if (n->out_tracks[i] == track) {
	    n = n->outputs[i]->node;
	    break;
	    // *INDENT-OFF*
	  }}}
      g_print("%s\n\n", str);
      lives_free(str);
      if (in_str) {
	g_print("%s\n\n", in_str);
	lives_free(in_str);
      }
      if (out_str) {
	g_print("%s\n\n", out_str);
	lives_free(out_str);
      }
    }
  }
  // *INDENT-ON*

  g_print("No more node_chains. Finished.\n\n");
}


#define DO_PRINT
  layers = nodemodel->layers;
  ntracks = nodemodel->ntracks;

static void _make_nodes_model(lives_nodemodel_t *nodemodel) {
  weed_layer_t **layers;
  inst_node_t *n, *ln;
  node_chain_t *nchain;
  lives_clip_src_t *mysrc;  
  int *pxtracks, *xplist;
  int xtrack, cpal, i, ntracks;

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
  // if we find an ouput listed which is not an input then we are going to add this node as a source
  // and we step through the list of sources, raising an error if we find an unconected one for the track

  node_idx = 0;
  layers = nodemodel->layers;
  ntracks = nodemodel->ntracks;
  if (!mainw->multitrack) {
    if (mainw->rte) {
      weed_instance_t *instance = NULL, *filter;
      int nins, nouts;
      int *in_tracks, *out_tracks;
      for (i = 0; i < FX_KEYS_MAX_VIRTUAL; i++) {
	if (rte_key_is_enabled(i, TRUE)) {
	  // for multitrack iterate over filter_map filter_init events
	  if ((instance = weed_instance_obtain(i, rte_key_getmode(i))) != NULL) {
	    filter = weed_instance_get_filter(instance, TRUE);
	    if (!filter) {
	      weed_instance_unref(instance);
	      continue;
	    }
	    nins = enabled_in_channels(filter, FALSE);
	    
	    nouts = enabled_out_channels(filter, FALSE);

	    if (!nins || !nouts){
	      weed_instance_unref(instance);
	      continue;
	    }

	    in_tracks = (int *)lives_calloc(nins, sizint);
	    out_tracks = (int *)lives_calloc(nouts, sizint);

	    if (!mainw->multitrack) {
	      in_tracks[0] = 0;
	      if (nins > 1) in_tracks[1] = 1;
	    }

	    n = create_node(NODE_MODEL_INSTANCE, nins, in_tracks, nouts, out_tracks);

	    do {
	      xtrack = check_node_connections(nodemodel, n);
	      if (xtrack != -1) {
		// inst node needs a src, and couldnt find one
		// either create a layer source or a blank frame source
		ln = create_node_for_layer(layers, xtrack, ntracks);
		add_src_node(nodemodel, ln, xtrack);
	      }
	    } while (xtrack != -1);
	    weed_instance_unref(instance);
	    // *INDENT-OFF*
	  }}}}}
  // *INDENT-ON*

  // finally we add one more node, this represents the display or sink



  if (!mainw->ext_playback)
    modeltype = NODE_MODELS_INTERNAL;
  }
  else
    modeltype = NODE_MODELS_OUTPUT;

  // create a node for each sink
  // for now we have only one sink, it outputs from the unconnected node_chain with lowest track number
  // if we have no chains it will create a source node for the lowest non null layer
  // if all layers are null, it will create a blank frame src
  pxtracks = (int *)lives_calloc(1, sizint);
  nchain = lowest_unterm_chain(nodemodel->node_chains);
  if (nchain && nchain->first_node && nchain->first_node->dsource)
    pxtracks[0] = nchain->first_node->dsource->track;
  if (pxtracks[0] == -1) {
    pxtracks[0] = 0;
    for (i = 0; i < ntracks; i++) {
      if (layers[i]) pxtracks[0] = i;
      break;
    }
  }

  mysrc->purpose = SRC_PURPOSE_MODEL;

  n = create_node(mysrc, 1, pxtracks, 0, NULL, cpal, xplist);

  do {
    xtrack = check_node_connections(nodemodel, n);
    if (xtrack != -1) {
      // inst node needs a src, and couldnt find one
      // either create a layer source or a blank frame source
      inst_node_t *ln = create_node_for_layer(layers, xtrack, ntracks);
      add_src_node(&nodemodel, ln, xtrack);
    }
  } while (xtrack != -1);
  nodemodel->node_chains = lives_list_reverse(nodemodel->node_chains);

  // now for each node_chain in nodemodel->node_chains, we will check if it is terminated or not
  // if we find an unterminated chain, then we will remove all the nodes in it.
  // starting from last node and going backwards. If in doing so we reach a node with
  // other outputs, we mark this output as unused. If this is the only output
  // or all outputs are flagged unused, then we cna remove the node and go backwards.
  // if other chains feed into the node they also need to now be marked as unterminated
  // so we need to locate and remove those

  //prune_chains(node_chains);
}


static void set_node_sizes(lives_nodemodel_t *nodemodel, boolean fake_costs) {
  LiVESList *list;
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    calc_channel_sizes(n);
  }

  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    n->flags &= ~NODEFLAG_PROCESSED;
  }
}


void _get_costs(lives_nodemodel_t *nodemodel, boolean fake_costs) {
  LiVESList *list;
  node_chain_t *nchain;
  inst_node_t *n;
  int flags = _FLG_GHOST_COSTS;

  // set the intiial state

  // pass 1, CALCULATE COST DELTAS NODE - NODE, for all palette pairings
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    desc_and_compute(n, flags, nodemodel->factors);
  }

  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    n->flags &= ~NODEFLAG_PROCESSED;
  }
  
  // pass 2 - ascend, FIND LOWEST COST PALETTES for each cost type
  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    if (n->n_outputs || !n->dsource || n->dsource->src_type != LIVES_SRC_TYPE_SINK) continue;
    if (n->flags & NODEFLAG_PROCESSED) continue;

    // checking each input to find prev_pal[cost_type]
    // create cost tuples
    //however this is not accurate as we do not know abss_cost tcost
    // when optimising we take this into account
    n->flags |= NODEFLAG_PROCESSED;
    map_least_cost_palettes(n);
  }

  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    n->flags &= ~NODEFLAG_PROCESSED;
  }

  // pass 3 descend, using previously calculated best_pals, find the total costs
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

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    desc_and_total(n, nodemodel);
  }
  
  // pass 4 ASCEND, TOTALING SLACK - here we ascend nodes and for each node with outputs we find min slack
  // over all outputs. This value is then added to outputs connected to the node inputs to give
  // total slack for that output

  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    if (n->n_outputs || !n->dsource || n->dsource->src_type != LIVES_SRC_TYPE_SINK) continue;
    if (n->flags & NODEFLAG_PROCESSED) continue;

    n->flags |= NODEFLAG_PROCESSED;
    accumulate_slack(n);
  }

  for (list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->last_node;
    n->flags &= ~NODEFLAG_PROCESSED;
  }

#idef OPTIM_MORE
  
  // pass 5 - descend, creating tuples and prio list. We can "spend" slack to reduce tcost at the sink node

  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    desc_and_make_tuples(n, nodemodel->factors, fake_costs);
  }

  // pass 6 apply changes from prio list, until we hit a threshold or run out of changes
#endif
}


void get_true_costs(lives_nodemodel_t *nodemodel) {
  // recalc costs, but now ignoring ghost costs for gamma_conversions
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    desc_and_compute(n, 0, nodemodel->factors);
  }

  // we dont adjust any palettes

  // descend and total costs for ech node to get abs_cost
  for (list = nodemodel->node_chains; list; list = list->next) {
    nchain = (node_chain_t *)list->data;
    n = nchain->first_node;
    if (n->n_inputs) continue;
    desc_and_total(n, nodemodel);
  }

  // only recalc slack and optimise if soemthing has changed in the model
}


// do the following,
// take each node_chain which starts from a source with no inputs
// compute the costs as we pass down nodes.
// if we reach a node with unprocessed inputs, then we continue to the next node_chain
// eventually we will fill all inputs and can contue downwards
void find_best_routes(lives_nodemodel_t *nodemodel) {
  LiVESList *list;
  boolean done;
  
  // set initial sizes for inputs / outputs

  // if any sources have variable size ouput, we now set these by ascending
  set_src_sizes(nodemodel->node_chains);

  // haveing set inital sizes, we can now estimate time costs
  _get_costs(nodemodel->node_chains, nodemodel->factors, TRUE);

  // all nodes should have been flagged as PROCESSED, so now we clear that flagbit
  
  // we have now completed the graph, all node_chains were followed until the terminator node
  // and we set min inputs for each out_pal / cost_type
  //
  // now we need to work backwards from the sink(s), through inputs, until we reach a source node.

  do {
    done = TRUE;
    for (list = nodemodel->node_chains; list; list = list->next) {
      // iterate through
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *fn = nchain->first_node, *ln = nchain->last_node;
      if (ln->flags & NODEFLAG_PROCESSED) continue;
      if (ln->n_outputs) continue;
      ln->flags |= NODEFLAG_PROCESSED;
      // map anything that terminates in a sink
      // since we go back along every possible path, we will eventually map all srcs
      // which end up at a sink
      map_least_cost_palettes(nodemodel->node_chains, ln);
    }
  } while (!done);

#ifdef DO_PRINT
  describe_chains(nodemodel->node_chains);
#endif
}


/* inst_node_t *get_inode_for_track(inst_node_t *n, int track) { */
/*   for (int i = 0; i < n->n_inputs; i++) { */
/*     if (n->inputs[i]->node->dsource->track == track) return n->inputs[i]->node; */
/*   } */
/*   return NULL; */
/* } */


inst_node_t *find_node_for_source(lives_nodemodel_t *nodemodel, lives_clip_src_t *dsource) {
  for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
    node_chain_t *nchain = (node_chain_t *)list->data;
    inst_node_t *n = nchain->first_node;
    if (n->n_inputs) continue;
    n = desc_and_find_src(n, dsource);
    if (n) return n;
  }
  return NULL;
}


lives_result_t inst_node_set_flags(inst_node_t *n, uint64_t flags) {
  // check flags against all pals - if none, return ERROR
  // check flags against node pals - if none, return FAIL
  int i;
  for (i = 0; i < n->npals; i++) {
    if (pal_permitted(n, pal_list[i])) break;
  }
  if (i == n->npals) {
    int allpals[] = ALL_STANDARD_PALETTES;
    for (i = 0; allpals[i] != WEED_PALETTE_END; i++) {
      if (pal_permitted(n, allpals[i])) break;
    }
    if (allpals[i] == WEED_PALETTE_END) return LIVES_RESULT_ERROR;
    return LIVES_RESULT_FAIL;
  }

  n->flags = flags;
  return LIVES_RESULT_SUCCESS;
}


double get_min_cost_to_node(inst_node_t *n, int cost_type) {
  // TODO - we may not have abest pal in future, but rather a set of best_pals
  // for the inputs. Then we need to appl
  int pal;
  if (!n || cost_type < 0 || cost_type >= N_COST_TYPES)
    return -1.;
  return n->min_cost[cost_type];
}


int get_best_palette(inst_node_t *n, int idx, int cost_type) {
  if (!n || cost_type < 0 || cost_type >= N_COST_TYPES)
    return WEED_PALETTE_INVALID;
  if (idx < 0 || idx >= n->n_inputs)
    return WEED_PALETTE_INVALID;
  if (n->inputs[idx]->npals == 0) {
    if (n->best_pal[cost_type] < 0 || n->best_pal[cost_type] >= n->npals)
      return WEED_PALETTE_INVALID;
    return n->pals[n->best_pal[cost_type]];
  }
  else {
    input_node_t *in = n->inputs[idx];
    if (in->best_pal[cost_type] < 0 || in->best_pal[cost_type] >= in->npals)
      return WEED_PALETTE_INVALID;
    return in->pals[in->best_pal[cost_type]];
  }
}


void build_nodes_model(lives_nodemodel_t **pnodemodel) {

  if (pnodemodel) {
    int opwidth, opheight, i;
    lives_nodemodel_t *nodemodel = (lives_nodemodel_t *)lives_calloc(1, siaeof(lives_nodemodel_t));

    if (*pnodemodel) {
      free_nodes_modelp(nodemodel);
      *pnodemodel = NULL;
    }

    *pnodemodel = nodemodel = (lives_nodemodel_t *)lives_calloc(1, siaeof(lives_nodemodel_t));

    nodemodel->layers = map_sources_to_tracks(CURRENT_CLIP_IS_VALID
					      && !mainw->proc_ptr && cfile->next_event, TRUE);
    nodemodel->ntracks = mainw->num_tracks;
    for (i = 0; i < mainw->num_tracks; i++) {
      // create fake nodemodel->layers here, howver these will have real clip numbers,
      // from which we can guess the sizes and palettes for layer sources
      nodemodel->layers[i] = lives_layer_create_with_metadata(mainw->clip_index[i], -1);
      lives_layer_set_track(nodemodel->layers[i], i);
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

    // having done this, we will calculate costs going down the branches (edges) of the tree. We would like to
    // minimes the costs at the root node(s). We define two cost types, Let Q be the "quality" of the image at the
    // display, with starting value 1.0 at each source, then passing down the branches and going from node to node,
    // we multiply Q by delta_q (quality loss). At the next node we multiply the Q vlaues for all inputs to find the new
    // Q value.
    // We also have cost type T (time). Moving along a breanch we add delta_t to T and at thenext node we find max T,
    // then backtrack and find max Q for T up to Tmax. Propogating these values we find Q and T at the root.
    // We want to minimise C (combbined cost), which is tfactor * T + qfactor * (1. - Q)
    // we will at each output going to the input, delta T, delta Q pairs. The pairs correspond to conversions
    // (size, palette, gamma change) going from output to input. There is an additional cost for T at eachnode,
    // proc_time. In order to estimate the time costs, we need to know the layer sizes at each node. Since this can be
    // tricky to figure out, we will do a dry run of the fx chain. For this we need to know the quality level,
    // and the player (sink) size.

    // first we do a dry, to get the channel (node) sizes
    // xyzzy;
    weed_apply_effects(nodemodel->layers, NULL, 0, nodemodel->opwidth, nodemodel->opheight, NULL, 1);

    // needs adjusting - qloss will be around 0.2 bad qloss
    // oe 0.5 for catastrrophic
    // time will be around 0.02 - 0.05 bad with 0.2 being catastrophic
    // if running in highq mode, we should be under 0.02 per frame, an increase of 0.01 sec
    // in trade for a q increase of 0.05 q seems reaonable so we actually want to increase qcost
    // 5:1
    // in medq, we could be running aroun 0.05 tcost , we woul increase by 0.01 in trade for a 5% qloss reduction
    // 5:1
    // in lowq wr may be running at 0.1 tcost and reducing tcost by 0.04 say, for a 0.1 increase in qloss
    // so 
    // is reasonable so 2.5:1
    nodemodel->factors[0] = 4.;
    nodemodel->factors[1] = 1.;

    // now we have all the details, we can calulate the costs for time and qloss
    // and then for combined costs. For each cost type, we find the sequence of palettes
    // which minimises the cost type. Mostly we are interested in the Combined Cost, but we will
    // calclate for each cost type separately as well.
    find_best_routes(nodemodel);

    // for the calulation and routing, we may have added some "dummy" qloss costs to ensure we found the optimal
    // palette sequnce/ . Now we recalulate without the dummy values, so we can know the true cost.

    get_true_costs(nodemodel);
    
    for (LiVESList *list = nodemodel->node_chains; list; list = list->next) {
      // since we ran only the first part of the process, finding the costs
      // but not the second, finding the palette sequence, we need to clear the Processed flag
      // else we would be unbale to reacluate costs again.
      node_chain_t *nchain = (node_chain_t *)list->data;
      inst_node_t *n = nchain->first_node;
      if (n->n_inputs) continue;
      desc_and_clear(n);
    }

    // now we can free our placeholder nodemodel->layers
    for (i = 0; i < mainw->num_tracks; i++) weed_layer_unref(nodemodel->layers[i]);
    lives_free(nodemodel->layers);
    nodemodel->layers = NULL;

    // there is one more condition we should check for, YUV detail changes, as these may
    // trigger a reinit. However to know this we need to have actual nodemodel->layers loaded.
    // So we set this flag, which will cause a second dry run with loaded nodemodel->layers before the first actual
    // instance cycle. In doing so we have the opportunity to reinit the instances asyn.
    nodemodel->flags |= NODEMODEL_NEW;

    // align the represented objects with the model
    align_with_model(nodemodel);

    // since we found which instances need reinting from the first dry run, we can do the reinits async.
    // So we will run this in a background thread, which may smooth out the actual instance cycel and playback
    // since we can be reinitng as the earlier instances are processing.
    descend_and_reinit(nodemodel);
  }
}
