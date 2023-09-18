// nodemodel.h
// LiVES
// (c) G. Finch 2023 <salsaman+lives@gmail.com>
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

// node modelling - this is used to model the effect chain before it is appplied
// in order to optimise the operational sequence and to compare hypothetical alternatives

// in the current implementation, the focus is on optimising the sequence of palette convesrsion
// as layers pass through the effects chain from sources to display sink

#ifndef HAS_LIVES_NODEMODEL_H
#define HAS_LIVES_NODEMODEL_H

#define LIVES_LEAF_PLAN_CONTROL "plan_control"

#define NODE_PASSTHRU 0
#define NODE_INPUT 1
#define NODE_OUTPUT 2

#define NODE_DIRN(n) (n->model_type % 4)
#define NODE_IS_SOURCE(n) (NODE_DIRN(n) == NODE_INPUT)
#define NODE_IS_SINK(n) (NODE_DIRN(n) == NODE_OUTPUT)

// node models frames which are pulled for a clip
// model_for point to the lives_clip_t
#define NODE_MODELS_EXTERN		0

#define NODE_MODELS_INTERN	      	4

// node models an fx instance (filter or generator)
// model_for points to the weed_instance_t
#define NODE_MODELS_PLUGIN		8

// combos

#define NODE_MODELS_CLIP		(NODE_MODELS_EXTERN + NODE_INPUT)

// node models an output plugin
// model_for points to a playback plugin
#define NODE_MODELS_OUTPUT		(NODE_MODELS_PLUGIN + NODE_OUTPUT)

// node models a non-clip src (e.g blank frame producer)
// model for points to a constructed clip_src

#define NODE_MODELS_SRC		      	(NODE_MODELS_INTERN + NODE_INPUT)

// node models some internal object (e.g pixbuf for diaplay)
// model_for is an opaque pointer
#define NODE_MODELS_INTERNAL		(NODE_MODELS_INTERN + NODE_OUTPUT)

// geerator plugins are enumerated as this, but with no in channels, so as to differentiate from
// static sources
#define NODE_MODELS_FILTER		(NODE_MODELS_PLUGIN + NODE_PASSTHRU)

#define NODE_MODELS_GENERATOR		(NODE_MODELS_PLUGIN + NODE_INPUT)

// defined cost types which we can optimise for:
#define COST_TYPE_COMBINED	0
#define COST_TYPE_TIME		1

// this is qloss due to palette changes
#define COST_TYPE_QLOSS_P      	2

// this is qloss due to resizing
#define COST_TYPE_QLOSS_S      	3

// normal cost types
#define N_COST_TYPES		4

/* OPERATION :: there are 5 phases  */

/* - graph construction  :: create_nodes_model()*/
// starting with an array of LAYERS of a size mainw->ntracks, which may include NULL values
// we consider the array index to be a TRACK number
// clip / frame numbers will correspond to mainw->clip_index and mainw->track_index
// in corresponding oreder - a clip_index value < 0 representign a NULL layer

// to construct the model we begin with a temporally ordered list of effect instances
// for ech of these in turn we construct a node, then examine the in_tracks for the instance
// for each of these we search for an unterminated node_chain in node_srcs - a linked list which is initially NULL
// failing to locate such a node_chain, we then construct a node for the correspodning track
// and prepend this to the instance node. In doing so, we also create a new node_chain with first_node = last_node
// pointing the new source node, flagged as unterminated.
// Now, finding a node_chain in node_srcs with track corresponding to the instance node
// in_track and being flagged as unterminated, we prepend this node to the instance node, and extend the node_chain
// updating last_node to point to the instance node.
// If we fail to find an unterminated node_chain for the track,
// and the layer for the track number in question is NULL then we create a blank_frame source for the track.
// and prepend that to the instance node, adding the blank_frame source to node_srcs.
//
// If a node has multiple inputs then we do this for each of the (non-cloned) inputs. We then examine the out_tracks
// for the instance node. If an in_track has a correspondign out_track with equal track number then the node_chain
// is left as unterminated. If there is no out_track for an in_track, this is a terminator node for the node_chain
// and we flag it as terminated, so it cannot be used as an input in any subsequent node
// If we find an out_track with no corresponding in_track, this is a source node for the out_track. If there is
// an unterminated node_chain already for that track, this is an ERROR - we cna never have more than one
// unterminated node_chain for any given track.
//
// if an instance node has the same track more than once in its in_tracks, all but the first are flagged as input CLONES
// in this case we assume that the layer will be duplicated and sent to the instance channels. Clones are
// ignored except for the fact the we can mark these inputs as filled.
//
// If the same track appears multiple times in out_tracks, then all but the first is an output clones.
// Again we assume that the layer will be copied and the copies used as inputs to separate nodes
// These are ignored for now, however when we reach the node for a clone input, we should find only terminated nodes
// for the track, which will prompt the algorithm to back track until it finds the first unused output clone
// which then becomes the input for that later node - this node will become a new source for the track,
// creating an unterminated node_chain beginning from this node.
//
// This continues until we reach the end of the list of instances, and finally then, we create a node for the
// sink (display) which has normally a single in_track, but its value is variable. We find the unterminated node_chain
// with the lowest track number, and prepend it. In the unlikely case that we do not find such a node_chain,
// we iterate through the layers array and create a source node for the first non-NULL layer we encounter
// if all layers are NULL, then we create a blank frame source and prepend that.

/* check and prune - TODO*/
/* here we iterate over the node_srcs list */
/* if a node_chain is not flagged as terminated, */
// then we jump to the first_node, locate the outptu corresponding to this track, and follow the route
// down until we reach the last_node. We can free any node without without extra outputs or inputs.
// IF the node has other outputs, we do not free it, but we flag our output as PRUNED. If there are other inputs
// we go up one level to the input node, and apply the same rules, freeing or marking as pruned, nad ascending
// inputs. on reaching the node_chain last-node, we stop and can remove the node_chain entriely from node_srcs.
// outputs flegged as pruned are ignored during further processing.

/* least cost routing */
// iterating ove node_srcs, we consider only nodes which have no inputs (true sources) ignoring any
// first_nodes with inputs (instance sources)

// we begin by setting initial sizes, this is started when building the model and connecting nodes
// if  we have a src with set dize, then this size is carried to the input of the next node.

// since we may have some srcs with variable size, this cannot bes set descending. If we rach anode with multiple inputs
// we compute the in and out sizes. If we multiple inputs, if any of them have a calulated size, then we set in / out
// sizes using the sizes which we have.otherwise we continu down with undefined size.

// then on the ascending wave we check if an output has undefined size,
// this is set to size from the input it connects to
// eventually definng sizes for src

// on the descender,

// when a layer is downsized this creates a virtual cost at the input equal to in_size / out_size
// we also keep track of the "max_possible" size.

// on the ascending wave if we find we upscale thenwe calc  cost for upscale
// it and this valuie can be "spent"

// the size contiues down until we have goen down all outputs
// then ascending we want to do 2 things, set he sizes for any srcs with variable size, transfeer the size cost up
// and convert vsize cost to real size cost
// if we reach an output with virtual size cost this is converted to real size cost, until we exhaust the upscale cost
// if we exhaust virtual cost we continue up until we hit a node with more virtual size cost
// when calulate qloss cossts we multiply sizes for all inputs as with palette qlos, however, we also add ln(qloss)
// for the next node/ Thus qloss for palettes occurs once, wheraas size qloss accumulates over each node until we get to
// the sink. If we have vcost left anywhere this can be "spent" to reduce tcost, the same way as slack.
// at the sink we multiply size qloss by a factor then multiply pal qloss and size qloss to give total qloss

// after calulating size costs, then we move on to palette costs

// we calculate costs for each palette available lat the source node, then recurse ove over all outputs
// at the following node we add on the additional cost for each in / out combination, fidning the lowest cost
// in palette per out palette.

// reverse routing
/* after calculating all the min_costs for each out palette / cost type, we start from sinks */
/* (node-chain last_nodes with no outputs), and work backwards to the sources, tracing best palettes */
/* for each cost type, recursing over each non-cloned input  */
// this is fairly simple, we begin by finding the palette with least cost for each cost_type at the sink,
// then iterate over all inputs, for each we know the palette which has least cost for the out palette
// this is set in best_pal for the node / cost_type
// on reaching a source node, if this is a true source, or if it is an instance source and we arrived
// the second or higher output, then we stop

// completed model
// now we can use this info, we can find a node for any node source, descend the node_chains, etc
// optionally we can restrict / relax  the model nodes and recalculate costs and re run reverse routing
// to observe the changes in costs / palettes this causes

// cost for node have been computed (used during cost calculation)
#define NODEFLAG_PROCESSED		(1ull << 0)

// can be set for input or output to indicate that the width and height cannot be altered
#define NODEFLAG_IO_FIXED_SIZE		(1ull << 1)

// can be set for a node, input or ouptu -- indicates that the node will use optial_pal
// and this cannot be changed
#define NODEFLAG_FIXED_PAL		(1ull << 2)

// bypass this node - act-s if outputs from prev node were contected directly to oututs from this node
// and we will reduce tcost by an amount
#define NODEFLAG_BYPASS			(1ull << 4)

// we have two types of clones - cloned outputs - created when an output goes to one node
// and terminates, and another copy bypasses this and inputs into a later node
// input clones - created when a track is mapped to multiple in channels in an instance
#define NODEFLAG_IO_CLONE		(1ull << 8)

// for instance nodes corresponding to instances with optional in channels
// setting this flsagbit indicates that the optional input/output is disabled currently
#define NODEFLAG_IO_DISABLED		(1ull << 9)

// indicates that this output /input  is not part of the cost model and should be ignored
// mostly this would be used for outputs with audio, or with alpha channel data
// for inputs / outputs
#define NODEFLAG_IO_IGNORE		(1ull << 10)

// output_specific node flags (4 - 7)
// output has been removed since it did not terminate at a sink
// used during forward routing
#define NODEFLAG_OUTPUT_PRUNED		(1ull << 11)

// if we have an instance with no in / out channels, but with in / out params, this is a data processing node
// it will not have any effect on the nodemodel, except that we add it as a "virtual" instance to the preceding node
// - then instead of the parent instance being a depenendancy, the virtual instances have first a dependency on the instance, then
// on the preceding virtual instance (if there are multiple), and the final virtual inst becomes th dependancy for next node
#define NODEFLAG_IO_VIRTUAL		(1ull << 12)

#define NODEFLAGS_IO_SKIP		(NODEFLAG_OUTPUT_PRUNED | NODEFLAG_IO_IGNORE | NODEFLAG_IO_DISABLED \
					 | NODEFLAG_IO_VIRTUAL)

// used to flag outputs which teminate in non display nodes
// ie. node_chain->last_node has no ouputs, butis a FILTER node. Ascending from there we flag all  inputs
// until we reach a node with > 1 output (which we must have, else no frames would ever reach the sink)
// - either it would be a plitter or we would have added an output clone
#define NODEFLAG_OUTPUT_NODISPLAY		(1ull << 16)

// flagbits corresponding to node source flags

// chantmpl flag - actually from out_chantmpol, but we set it in input
#define NODESRC_INPLACE		(1ull << 31)

// input / output flagbits - these are actually set for the filter, but we set them in
// input / ouput nodes to facilitate

// flags that indicate when to reinit an instance
#define NODESRC_REINIT_PAL		(1ull << 32)
#define NODESRC_REINIT_RS		(1ull << 33)
#define NODESRC_REINIT_SIZE		(1ull << 34)

// indicates that the input accepts layers of any size, no resize is necessary
// (or for sources, indicates that the output can be set to any desired size)
#define NODESRC_ANY_SIZE		(1ull << 35)

// indicate that the input / output can use any listed palette
#define NODESRC_ANY_PALETTE		(1ull << 36)

// inst_node src flagbits

//indicates that inputs and outputs may have differing sizes or palettes
#define NODESRC_IS_CONVERTER		(1ull << 40)

// flag tells us to add "ghost" qloss costs for YUV palettes - these represent the
// theoretical "loss" for not being RGB and able to gamma convert, but are not counted when
// calculating the actual costs
#define NODESRC_LINEAR_GAMMA		(1ull << 41)

// flagbit indicates src prefers premultiplied alpha
// this may add an additional set of costs
#define NODESRC_ALPHA_PREMULT		(1ull << 42)

// RESTRICTIONS THAT CAN BE APPLIED FOR COST OPTIMISATION

#define NODEFLAG_NO_REINIT		(1ull << 48)

// indicates that the palette MUST include an alpha channel
// to function properly this flag bit needs to be set on all nodes feeding into an input for this node
#define NODEFLAG_PRESERVE_ALPHA		(1ull << 50)

// only consider RGB palettes when selecting best_pal for a cost_type
#define NODEFLAG_ONLY_RGB		(1ull << 51)

// only consider YUV palettes when selecting best_pal for a cost_type
#define NODEFLAG_ONLY_YUV		(1ull << 52)

// only consider packed palettes when selecting best_pal for a cost_type
#define NODEFLAG_ONLY_PACKED		(1ull << 53)

// only consider planar palettes when selecting best_pal for a cost_type
#define NODEFLAG_ONLY_PLANAR		(1ull << 54)

typedef struct _inst_node inst_node_t;

// node chains - when constructing the node model, we will end up with a linked list of
// node_chain_t *
// each track can have 0, 1, or multiple node_chains. The first_node is always
// a src node, and last_node is the terminator (sink) node
// while building the graph, we may only have one active (unterminated) chain
// per track at any moment.
// the src can be either a layer src, blank frame src, or an instance node
// last node will be either an instance node, or else a sink (node with no out tracks)

typedef struct {
  // track number correcsponding to out_track in first_node
  // we trace thiss through until reaching a node which does not output on that track
  // we then set last_node, and set the trminated flad
  // when a instance requires input from a specific track, we first check node_chains to see
  // if there is an unterminated chain for the track. we connect to the last_node
  // if there is a teminted chain then we will add a fork output at the node before last node.
  // if there are no chains and the layer iss no null, we create a new ssource node and a new unterminated
  // node chain. If the layer is NULL we will create a blank frame source node and this becomes the first_node

  int track;
  // pointers to first and last nodes in the chain
  inst_node_t *first_node, *last_node;
  boolean terminated;
} node_chain_t;

// resources with limits
#define RES_TYPE_MEM			0
#define RES_TYPE_BBLOCK			1
#define RES_TYPE_THRD			2

#define N_RES_TYPES			3

// nodemodel flagbits
#define NODEMODEL_NEW		(1 << 0)
#define NODEMODEL_TRAINED	(1 << 1)
#define NODEMODEL_INVALID	(1 << 16)

typedef struct {
  // list of node_chains, first node will either be a src_node, or a src_inst
  // last node will either be a sink_inst or a sink_node
  int flags;
  double factors[N_COST_TYPES];
  int opwidth, opheight;

  // list of all nodes in any order
  LiVESList *nodes;

  // chains starting from src nodes
  LiVESList *node_chains;
  //
  // list of weed_filter_t * in running order
  LiVESList *fx_list;

  // these values are used during construction, and hold placeholder layers
  int ntracks;
  int *clip_index;
} lives_nodemodel_t;

// we may have two values for a single in_pal, out_pal, one including conversion
// to / from linear gamma, and the other without. Using linear gamm if the filter requests it
// adds a quality BONUS, (negative qloss cost), but may increas time cost, and swithcing
// between gamm types adds a "ghost" qloss cost. The time cost also depends on the gamma of prev an next nodes
#define CDELTA_FLAG_LGAMMA		1

typedef struct {
  // we make a list of these for each input
  // to find real in_pal we need p->pals[in_pal] where p is either in->node, or in->node->outputs[oidx]
  // to find real out_pal we need n->pals[out_pal] where n is either the inst node, or the input node

  int out_pal, in_pal;
  int out_gamma_type, in_gamma_type;

  // delta_t represents the time (seconds) to convert (and possibly copy) the layer from the prev node
  // out_channel to this node in_channel, this is added to p->dp_cost[out_pal][COST_TYPE_TIME]
  //p is either input->node pt input->node->outputs[oidx]
  // delta_q represents (1. - qloss) for the conversion, we find Q from the previous node and multiply by
  // delta_q
  double deltas[N_COST_TYPES];
} cost_delta_t;


// representation of a layer conversion from node output to next node input
typedef struct {
  // pixels
  int out_width, out_height;
  int in_width, in_height;
  //
  int lb_width, lb_height;

  // (copied from cdelta->deltas)

  int out_pal, in_pal;
  int out_gamma, in_gamma;
} conversion_t;

typedef struct {
  // pointer to node
  inst_node_t *node;

  // array of size node->n_inputs
  conversion_t *palconv;

  // total cost found by totaling over all inputs
  double tot_cost[N_COST_TYPES];
} cost_tuple_t;

typedef struct _plan_step plan_step_t;

typedef struct _input_node input_node_t;

struct _input_node {
  // pointer to prev node
  inst_node_t *node;

  // for fake inputs of clip nodes, this is the value of class_uid for the src
  uint64_t src_uid;

  plan_step_t *dep; // dependency step for this input - used in plan building

  int track;

  // the size for this chennel. This is set during node construction, then updated during dry run 1
  // if this changes during dry run, then this may set needs_reinit in the node
  int width, height;

  // if we are letterboxing, these values are for the inner part, we actually resize to this size
  // then letterbox to width / height
  int inner_width, inner_height;

  int oidx; // idx of output node (>= 0) in prev which this is connected to

  int cpal; // current palette for the input

  // detials for yuv_clamping, sampling, subspace
  // when we set a yuv palette, these will be set to defaults
  // unless filter overrides this.
  // if we do not have npals then we use gloobal vals
  // these can be changed IFF we connect to a clip_src which is also yuv, and has clamped
  // THEN we may choose to set input to clamped, because this will introduce an extra time cost
  // clamped ->unclamped for width * height, pal, however anywhere where we resize with yuv clamped
  // an dnot resizable direct, we need to add this cost anyway
  int c_clamp, c_samp, c_sub;

  // these values are only used if the input has a divergent pal_list
  // in this case npals will be > 0
  int npals;
  int *pals;

  boolean free_pals;

  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  // generally best_in_pal[COST_TYPE_COMBINED]
  int optimal_pal;

  // when optimising this can be set to a speculative palette and the node / input / output locked
  int spec_pal;

  // if npals > 0, we store here the best "out" palette per cost type for the input
  // otherwise best_pal from the node is used. This can be cross referenced with prev_pal
  // to find best_pal for the output there
  int best_out_pal[N_COST_TYPES];

  // this is only used in cases where the input has npals > 0
  int best_in_pal[N_COST_TYPES];

  // for clip_srcs, we can use best_pal[next_in_pal][cost_type]
  // this is simpler than for filter nodes, because we only add together values
  // fom clip_srcs, there is no totalling across them, since we only use one or other at any time
  int *best_src_pal;
  double *min_cost;

  //double least_cost[N_COST_TYPES];

  // for each in / pal out pal / cost type, we create a cdelta
  //
  LiVESList *cdeltas;
  LiVESList *true_cdeltas;

  uint64_t flags; // clone; processed, ignore

  // for cloned inputs this is a pointer to the cloned input
  int origin;

  // for srcs we will create virutal inputs, and f_ratio prepresents the fraction of frames in the clip which
  // use that particular clip_src
  double f_ratio;
};

typedef struct _output_node output_node_t;

struct _output_node {
  inst_node_t *node;

  int track;

  int iidx; // idx of input node (>= 0) in prev which this is connected to

  // the layer size as it exits from this output. Width is measured in pixels
  // generally all inputs and outputs have identical sizes
  // if the input it connects to has a different size, we must resize
  int width, height;

  int maxwidth, maxheight, minwidth, minheight;

  // in some cases the out palette can vary per output track
  // if npals is 0, then we use the global values from the node
  // if npals > 0, then cpal is set during dry_run 2, and may affect the recalc
  int cpal; // current palette for the output
  int c_clamp, c_samp, c_sub;

  //
  int npals;
  int *pals;
  boolean free_pals;

  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  // == best_out_pal[COST_TYPE_COMBINED]
  int optimal_pal;

  // when optimising this can be set to a speculative palette and the node / input / output locked
  int spec_pal;

  // best out pal per cost type, if npals == 0, the we use best_pal_up from node
  int best_out_pal[N_COST_TYPES];

  // least cost is for all out palettes, we set this and set best_pal_up (down)
  // either for the input (if we have in->npals) or for the node as a whole

  // the final choice is set in best_pal_up, then eventually into optimal_pal
  // either for prev node or for the output

  double least_cost[N_COST_TYPES];

  // we use this to store potential  qloss_s due to downscaling
  LiVESList *cdeltas;
  LiVESList *true_cdeltas;

  // feature for use during optimisation, for selected palette, holds tmax - tcost
  double slack;

  // htis measures the cumulative slack from here to the sink node, we can increase tcost by up to this amount
  // without changing the overall tcost, as the increase will be absorbed
  // reduced by eliminating slack sequentially
  double tot_slack;
  // if npals > 0, this is an advisory to the host, suggesting the optimal palette to set if it has free choice
  // of out palettes. This is determind from evaluating whatever node the output feeds into

  uint64_t flags; // pruned, processed, ignore

  // in case of cloned outputs, for a clone, this points to the original output
  int origin;
};

typedef struct _inst_node {
  int idx;

  // this is an estimate of when the instance will begin processing
  // time == 0. represents the moment that the first intance node begins pprocessing
  // == ready_time of first node
  // since some sources are loaded before procesing, they may have a negative ready_time
  ticks_t ready_ticks;

  // this is a measure of the difference between the estimated ready_time and the previous actual
  // ready_time, this can be used to calibrate future tcost estimates
  ticks_t ready_time_delta;

  // this is the global size, in some cases inputs or outputs may override this
  // if their values are 0, 0 then we use the global value
  // these values are set duirng dry run 1, and there we also determine if we need to reinit is set
  // due to size or rowstride changes
  // if lbwidth, lbheight are non zero, then letterboxing will be used - there is a cost to resize to
  // lb_width X lb_height + cost_to create + overlay on blank frame
  // if we are converting gamma, this is done on the inner image, this may slightly affect the gamma time cost
  int width, height;

  // indicates the instance needs reinit before it can be used to process
  boolean needs_reinit;

  // the real object being modelled
  // type can be: clip, instnace, output, src, or internal
  int model_type;

  // pointer to the object (template) being modelled by the node
  void *model_for;

  // pointer to the object instance being modelled by the node
  void *model_inst;

  // for filters, the filter keymap key, for clip srcs, the clip number etc
  int model_idx;

  int *in_count, *out_count;

  // a lsit of "virtual" outputs -data processing nodes which are run before this inst
  LiVESList *virtuals;
  /////////////////////////////////
  // global palette values for the node. These values are used for inputs / outputs which dont have an own palette_list
  //

  int cpal; // current palette for node->dsource
  int c_clamp, c_samp, c_sub;

  int npals; // num values in pals, excluding WEED_PALETTE_END
  // int [npals + 1]
  int *pals; // array of input pals (out pals to next node)

  boolean free_pals;

  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  // == best_pal_up[COST_TYPE_TIME]
  int optimal_pal;

  // optimised gamm_type for inputs / outputs
  int gamma_type;

  // when optimising this can be set to a speculative palette and the node / input / output locked
  int spec_pal;
  int spec_gamma;

  // this is only used when we have no inputs, otherwise each input stores its own copy

  // here is where we store the results of the calulation - for each cost type,
  // the optimal palette to use, this is an index value - i.e palette is pals[idx]
  // some output_nodes may have their own palette lists, and they will then have their own best_pal
  // also some inputs may be allowed to set indivdual palettes, they will then have their own best_pal
  // in these case npals will be > 0, and the instance itself will convert between the input node palette
  // and best_pal. The value here is used for input nodes and output nodes which have 0 npals
  // in such cases, the actual pallete currently being used is set in the cpal value

  // we actually have two arrays - the first is used when we find best palettes starting from sink and going up
  // the second when finding palettes descendig from source
  //
  int best_pal_up[N_COST_TYPES];
  int best_pal_down[N_COST_TYPES];

  // this is a measure of the cumulative cost up to the point where the object represented by the node is ready
  // to process
  double abs_cost[N_COST_TYPES];
  /////////////////////////////////////////////

  // nodes following this one, these are just for reference; when tracing back
  // we need to make sure we traced back up from all outputs
  // inputs are in order identical to out_tracks
  int n_outputs;
  output_node_t **outputs;

  // nodes feeding into this one, each of these has (updated) cumulative costs, from which we
  // can ascertain the min cost per cost type for a given out palette
  // inputs are in order identical to in_tracks
  int n_inputs;

  // value only used for src nodess
  int n_clip_srcs;

  input_node_t **inputs;

  uint64_t flags; // inst_node flags, restictions and prefs
} inst_node_t;

#define STEP_FLAG_SKIP			(1ull << 0)
#define STEP_FLAG_INVALID		(1ull << 1)

//#define STEP_FLAG_OPTIMISE		(1ull << 1)

#define STEP_FLAG_NO_READY_STAT		(1ull << 8)
#define STEP_FLAG_RUN_AS_LOAD		(1ull << 9)

#define STEP_FLAG_TAGGED_LAYER		(1ull << 16)

#define STEP_TYPE_LOAD	 		1
#define STEP_TYPE_CONVERT 		2
#define STEP_TYPE_APPLY_INST 	       	3
#define STEP_TYPE_COPY_IN_LAYER        	4
#define STEP_TYPE_COPY_OUT_LAYER      	5

typedef struct {
  // offsets from plan trigger time
  // since we do not know exact frame load times
  // we only set est dur for now
  // real_start / real_end are in session_time
  ticks_t
  // steps / template
  est_start,
  est_end,
  deadline;
  //
  double
  est_duration,
  real_start,
  real_end,
  paused_time,
  real_duration,
  // plan only
  actual_start,
  preload_time, // actual_start - real_start
  active_pl_time, // step time until actual_start
  tgt_time, // 1. / pb_fps
  concurrent_time, // total time when > 1 steps were active
  sequential_time, // sum of all steps if run sequentially
  exec_time,
  trun_time,
  queued_time, // trun_time - exec_time
  trigger_time,
  start_wait, // time between thread running and trigger (trigger - trun)
  waiting_time; // after triggering, time when no steps were running
} timedata_t;

#define OP_RESIZE	0
#define OP_PCONV	1
#define OP_GAMMA	2
#define OP_LETTERBOX	3
#define OP_MEMCPY	5
#define N_OP_TYPES	6

#define OP_APPLY_INST	16
#define OP_LOAD		17

typedef struct {
  int op_idx;
  double start, end, paused, copy_time;
  int width, height;
  int pal, pb_quality;
  double cpuload;
} exec_plan_substep_t;

// letterboxing is represented as a resize followed by a memcpy
// deinterlace is represented by an apply_inst

typedef struct {
  int op_idx;
  double time;
  int out_size;
  int in_size;
  int out_pal, in_pal; // OP_PCONV, OP_RESIZE
  int out_clamping, in_clamping;
  int out_sampling, in_sampling;
  int out_subspace, in_subspace;
  boolean gamma;
  double cpu_load;
  int pb_quality;
} tcost_data;

// define the neural network for estimating resize / palconv tcosts
#define TIMING_ANN_NLAYERS 3
#define TIMING_ANN_LCOUNTS {36, 48, 8}

// substep -> testdata mapping for ANN
#define TIMING_ANN_OUTSIZE	0
#define TIMING_ANN_INSIZE	1
#define TIMING_ANN_CPULOAD	2
#define TIMING_ANN_PBQ_BASE 	2 //(base is 1)
#define TIMING_ANN_OUT_PAL_BASE 5 //(base is 1)
#define TIMING_ANN_IN_PAL_BASE 20 //(base is 1)

typedef struct {
  int filt_idx;
  int pal;
  double c0, c1, c2;
} proctime_consts;


typedef struct {
  lives_ann_t *ann;
  int ann_gens;
  pthread_mutex_t ann_mutex;
  LiVESList *proc_times;
  int cpu_nsamples;
  float av_cpuload;
  double tot_duration;
  double bytes_per_sec;
  double gbytes_per_sec;
} glob_timedata_t;

typedef struct _exec_plan exec_plan_t;

struct _plan_step {
  int st_type;
  lives_proc_thread_t proc_thread;
  //
  int ndeps;
  plan_step_t **deps;
  size_t start_res[N_RES_TYPES], end_res[N_RES_TYPES];
  double real_st, real_end, paused_time;
  volatile int state;  // same values as PLAN_STATE
  uint64_t flags;

  // pointer to the plan containing step
  exec_plan_t *plan;

  // timing data
  timedata_t *tdata;

  weed_filter_t *target;

  // clip number, fx key
  int target_idx;

  int track; // for conv / load
  int dchan, schan;
  //
  int ini_width, ini_height;
  int ini_iwidth, ini_iheight;
  int ini_pal, ini_gamma;
  int ini_sampling, ini_subspace, ini_clamping;

  int fin_width, fin_height;
  int fin_iwidth, fin_iheight;
  int fin_pal, fin_gamma;
  int fin_sampling, fin_subspace, fin_clamping;

  LiVESList *substeps;

  char *errmsg;

  // eg. apply deinterlace
  uint64_t opts;
}; // plan_step_t

// flagbits for plan and for steps

// initial, pre-execution state
#define PLAN_STATE_INERT	0

// plan is executing, but is passively waiting for st_time to be set
#define PLAN_STATE_QUEUED	1

// plan is executing, but is passively waiting for st_time to be set
#define PLAN_STATE_WAITING	2

// st_time set, plan is running steps as deps get filled
#define PLAN_STATE_RUNNING	3

// all steps in plan have completed
#define PLAN_STATE_COMPLETE	4

//
#define PLAN_STATE_RESETTING	5

// plan fully processed can be discarded and a new cycel created
#define PLAN_STATE_DISCARD	8

// plan or a step encountered a fatal error; execution halted
#define PLAN_STATE_ERROR	-1

// 8 - 15 STEP states

// plan was cancelled, all steps have also been cancelled
#define PLAN_STATE_CANCELLED	16

// plan is paused, all steps have either completed or are also paused
#define PLAN_STATE_PAUSED	17

// plan is resuming after a pause, some steps are still in the process of resuming
#define PLAN_STATE_RESUMING	18

// 32 - 64 STEP states

// template cannot be executed, but it can be used to make a plan_cycle which can be
#define PLAN_STATE_TEMPLATE	256

// step states - same numbers as PLAN states

#define STEP_STATE_INACTIVE	PLAN_STATE_INERT
#define STEP_STATE_WAITING	PLAN_STATE_WAITING
#define STEP_STATE_RUNNING	PLAN_STATE_RUNNING
#define STEP_STATE_FINISHED	PLAN_STATE_COMPLETE
#define STEP_STATE_ERROR	PLAN_STATE_ERROR

// state was skipped over (counted as FINISHED)
#define STEP_STATE_SKIPPED	8

#define STEP_STATE_CANCELLED	PLAN_STATE_CANCELLED
#define STEP_STATE_PAUSED	PLAN_STATE_PAUSED
#define STEP_STATE_RESUMING	PLAN_STATE_RESUMING

// idicates an ignorable step
#define STEP_STATE_IGNORE	64

// template cannot be executed, but it can be used to make a plan_cycle which can be

struct _exec_plan {
  uint64_t uid;
  exec_plan_t *template;
  lives_nodemodel_t *model;
  uint64_t iteration;
  // stack of layers in track order. These are created as soon as plan is executed
  // by calling map_sources_to_tracks
  // LOAD event will wait for the layer 'frame' to be set to non-zero before loading
  // occasionally, for repeatabel, optional channels as targets, the track_source may be unmapped
  // and the layer will be NULL, in this case when we try to LOAD from the layer, we will skip the step
  // and mark the channel as WEED_LEAF_HOST_TEMP_DISABLED. The corresponding input node in the model will
  // be flagged as IGNORE
  weed_layer_t **layers;

  frames64_t *frame_idx;

  timedata_t *tdata;

  int nsteps_running;

  // the actual steps of the plan, no step may appear before all of its dependent steps
  LiVESList *steps;

  volatile uint64_t state;
};

void build_nodemodel(lives_nodemodel_t **);
void free_nodemodel(lives_nodemodel_t **);

void ann_roll_cancel(void);

void cleanup_nodemodel(lives_nodemodel_t **);

exec_plan_t *create_plan_from_model(lives_nodemodel_t *);
void align_with_model(lives_nodemodel_t *);
exec_plan_t *create_plan_cycle(exec_plan_t *template, lives_layer_t **);
lives_proc_thread_t execute_plan(exec_plan_t *cycle, boolean async);
void plan_cycle_trigger(exec_plan_t *cycle);
void exec_plan_free(exec_plan_t *);

void display_plan(exec_plan_t *);

void find_best_routes(lives_nodemodel_t *nodemodel, double *thresh);

inst_node_t *find_node_for_source(lives_nodemodel_t *, lives_clip_src_t *dsource);

inst_node_t *src_node_for_track(lives_nodemodel_t *, int track);

void get_true_costs(lives_nodemodel_t *);

char *get_node_name(inst_node_t *);

int  get_best_in_palette(inst_node_t *, int inp_idx, int cost_type);
int  get_best_out_palette(inst_node_t *, int out_idx, int cost_type);

double get_min_cost_to_node(inst_node_t *, int cost_type);

lives_result_t inst_node_set_flags(inst_node_t *n, uint64_t flags);

void describe_chains(lives_nodemodel_t *);

void ann_roll_cancel(void);

#endif
