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

#define NODE_IS_SRC(n) (!n->inputs)
#define NODE_IS_SINK(n) (!n->outputs)

// node models frames which are pulled for a clip
// model_for point to the lives_clip_t 
#define NODE_MODELS_CLIP		1

// node models an fx instance (filter or generator)
// model_for points to the weed_instance_t
#define NODE_MODELS_INSTANCE		2

// node models an output plugin
// model_for points to a playback plugin
#define NODE_MODELS_OUTPUT		3

// node models a non-clip src (e.g blank frame producer)
// model for points to a constructed clip_src
#define NODE_MODELS_SRC			4

// node models some internal object (e.g pixbuf for diaplay)
// model_for is an opaque pointer
#define NODE_MODELS_INTERNAL		5

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

// we calulate costs for each palette available lat the source node, then recurse ove over all outputs
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

// when recalculating costs, if this is set, then we keep eveything fixed for thsi node (inst, input ot output)
#define NODEFLAG_LOCKED	       	(1ull << 1)

// bypass this nod - act-s if outputs from prev node were contected directly to oututs from this node
// and we will reduce tcost by an amount
#define NODEFLAG_BYPASS		(1ull << 2)

// defines an input as "inplace", ie. the input layer is read / write
// if not set, then it is possibel to make a layer copy after conversion, during the instance processing
// this could be useful in case we have a cloned output, with same pal and size, which is not nneded
// until a later time, and the instance has a long processing cycle
#define NODEFLAG_INPUT_INPLACE			(1ull << 4)

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

#define NODEFLAGS_IO_SKIP	       i(NODEFLAG_OUTPUT_PRUNED | NODEFLAG_IO_IGNORE | NODEFLAG_IO_DISABLED \
					 | NODEFLAG_LOCKED)

// flagbits corresponding to node source flags
// input / output flagbits

// flags that indicate when to reinit an instance
// input / output specific
#define NODESRC_REINIT_PAL		(1ull << 32)
#define NODESRC_REINIT_RS		(1ull << 33)
#define NODESRC_REINIT_SIZE		(1ull << 34)

// indicates that the input accepts layers of any size, no resize is necessary
// (or for sources, indicates that the output can be set to any desired size)
#define NODESRC_ANY_SIZE		(1ull << 35)

// indicate that the input / output can use any listed palette
#define NODESRC_ANY_PALETTE		(1ull << 36)

//indicates that inputs and outputs may have differing sizes or palettes
#define NODESRC_IS_CONVERTOR		(1ull << 37)

// inst_node src flagbits

// flag tells us to add "ghost" qloss costs for YUV palettes - these represent the
// theoretical "loss" for not being RGB and able to gamma convert, but are not counted when
// calculating the actual costs
#define NODESRC_LINEAR_GAMMA		(1ull << 40)

// flagbit indicates src prefers premultiplied alpha
// this may add an additional set of costs
#define NODESRC_ALPHA_PREMULT		(1ull << 41)

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

// nodemodel flagbits
#define NODEMODEL_NEW		(1 << 0)
#define NODEMODEL_INVALID	(1 << 16)

typedef struct {
  // list of node_chains, first node will either be a src_node, or a src_inst
  // last node will either be a sink_inst or a sink_node
  int flags;
  double factors[N_COST_TYPES -1];
  int opwidth, opheight;

  LiVESList *node_chains;
  //
  // this is an array of LiVESList[track], each entry (one per track0, constains
  // a list of pointesr to elements in node_chains, in reverse temporal order
  LiVESList **track_nodechains;

  // list of instances in running order
  LiVESList *fx_nodes;

  // these values are used during construction, and hold placeholder layers
  int ntracks;
  weed_layer_t **layers;

  // we can use this to create subsets of nodes - for example when testing to apply gamma changes
  // we may us it
  LiVESList *sel_nodes;
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
  // pixels ?
  int in_width, in_height;
  int out_width, out_height;
  //
  int lb_width, lb_height;

  // (copied from cdelta->deltas)
  
  int in_pal, out_pal;
  int in_gamma, out_gamma;

  // delta costs for this specific conversion

  double deltas[N_COST_TYPES];

  // cumulative costs for this specific conversion
  double abs_costs[N_COST_TYPES];
} conversion_t;

typedef struct {
  // pointer to node
  inst_node_t *node;
  
  // array of size node->n_inputs
  conversion_t *palconv;

  // total cost found by totaling over all inputs
  double tot_cost[N_COST_TYPES];
} cost_tuple_t;

typedef struct _input_node input_node_t;

struct _input_node {
  // pointer to prev node
  inst_node_t *node;

  // the size for this chennel. This is set during node construction, then updated during dry run 1
  // if this changes during dry run, then this may set needs_reinit in the node
  int width, height;

  // if we are letterboxing, these values are for the inner part, we actually resize to this sie
  // then letterbox to width / height
  int inner_width, inner_height;
  
  // during model building, holds current size, so we can check if reinit is needed
  int cwidth, cheight;

  // during descend and srch for downscales, we cna maintian the size at the src. This used for blame costs
  // at mixing nodes
  int src_Width, src_height;
  
  int oidx; // idx of output node (>= 0) in prev which this is connected to

  // these values are only used if the input has a divergent pal_list
  // in this case npals will be > 0
  int npals;
  int *pals;
  int cpal; // current palette for the input
  
  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  int optimal_pal;

  // when optimising this can be set to a speculative palette and the node / input / output locked
  int spec_pal;

  // if npals > 0, we store here the best "out" palette per cost type for the input
  // otherwise best_pal from the node is used. This can be cross referenced with prev_pal
  // to find best_pal for the output there
  int best_pal_up[N_COST_TYPES];
  int best_pal_down[N_COST_TYPES];

  ////
  
  // for each in / pal out pal / cost type, we create a cdelta
  // 
  LiVESList *cdeltas;

  // for each cost type, and each out pal, we can find the in pal which provides the lowest cost for the type
  // this is used when ascending, after resolving conflicts in cases of multiple outputs
  //
  // this can also be found via perusing the cdeltas, but it is more convenient to store this here
  //

  double least_cost[N_COST_TYPES];
  int best_in_pal[N_COST_TYPES], best_out_pal[N_COST_TYPES];

  uint64_t flags; // clone; processed, ignore

  // either NULL or a NULL terminated array of clone inputs for the same node
  // similar to cloned outputs, except that each output goes to a separate next node
  // these are simple copies of the input layer, they must have identical palette, size, gamma_type
  input_node_t **clones;

  // in case of cloned inputs, for a clone, this points to the original input
  input_node_t *origin;

  // the gamma_type for this input, and the output
  // if these do not match, we need to do a gamma conversion
  int gamma_type, prev_gamma_type;

  // for srcs we will create virutal inputs, and f_ratio prepresents the fraction of frames in the clip which
  // use that particular clip_src
  double f_ratio;
};
  
typedef struct _output_node output_node_t;

struct _output_node {
  inst_node_t *node;

  int iidx; // idx of input node (>= 0) in prev which this is connected to

  // the layer size as it exits from this output. Width is measured in pixels
  // generally all inputs and outputs have identical sizes
  // if the input it connects to has an different size, we must resize
  int width, height;

  // current width height when model building
  int cwidth, cheight;

  // in some cases the out palette can vary per output track
  // if npals is 0, then we use the global values from the node
  // if npals > 0, then cpal is set during dry_run 2, and may affect the recalc
  int cpal; // current palette for the output

  //
  int npals;
  int *pals;

  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  int optimal_pal;

  // when optimising this can be set to a speculative palette and the node / input / output locked
  int spec_pal;

  int best_pal_up[N_COST_TYPES];
  int best_pal_down[N_COST_TYPES];
  //

  // going down, we find the best palette at the input connecting to each output
  // (going up we set best pal for the output, connecting from input)
  // to calculate this we look at cdeltas for the next inputs, then reverse the ordering
  // instead of finding best in for each out, we find best out for each in
  // (out here becomes in at next node)
  
  int *next_pal;
  
  // min_cost is per out_pal, we find next_pal and min_cost
  double *min_costs;

  // least cost is for all out palettes, we set this and set best_pal_up (down)
  // either for the input (if we have in->npals) or for the node as a whole

  // the final choice is set in best_pal_up, then eventually into optimal_pal
  // either for prev node or for the output

  double least_cost[N_COST_TYPES];
  int best_out_pal, best_in_pal;

  // we use this to store upscale qloss_s as we ascned and convert into actual qloss_s
  LiVESList *cdeltas;
  
  // feature for use during optimisation, for each out palette, holds tmax - tcost
  double *slack;

  // htis measures the cumulative slack from here to the sink node, we can increase tcost by up to this amount
  // without changing the overall tcost, as the increase will be absorbed // reduced by elliminating slack sequentially
  double *tot_slack;

  // if the out
  double *copy_cost;

  // if npals > 0, this is an advisory to the host, suggesting the optimal palette to set if it has free choice
  // of out palettes. This is determind from evaluating whatever node the output feeds into

  uint64_t flags; // pruned, processed, ignore

  // either NULL or a NULL terminated array of clone outputs for the same node
  // the model  assumes palette conversion / resize gamma happens as soon as the node has finished processing
  // for cloned outputs we copy the main output channel / layer for the track
  // similar to cloned inputs, except that each output goes to a separate next node
  // unlike input clones, each cloned output can have its own next palette and size and gamma_type for conversion
  output_node_t **clones;

  // in case of cloned outputs, for a clone, this points to the original output
  output_node_t *origin;
};

typedef struct _inst_node {
  int idx;

  // this is an estimate of when the instance will begin processing
  // time == 0. represents the moment that the first intance node begins pprocessing
  // == ready_time of first node
  // since some sources are loaded before procesing, they may have a negative ready_time
  ticks_t ready_time;

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

  // pointer to the object being modelled by the node
  void *model_for;

  // this is used for non-CLIP sources
  int src_type;
  
  // for instances, these values hold the mapping from tracks to in and out channels
  int *in_tracks, *out_tracks;

  // when modelling instances there will be a certain number of in channels and out channels
  // some of these channels may be disabled permanently - in which case we ignore them
  // some may be alpha channels we also ignore
  // others may be optional / repeatable, and these can be disabled / re-enabled
  // entries in in_tracks / out_tracks determine the mapping of layers (in track order) -> channels
  // we beign by creating an input node for each in_track, and output_node for each out
  // some channel templates are repeateable optional, there can be any number of channels created from these
  // and they will either map to a layer or be temp disabled. in_count / out_count (TODO) will be used to determine
  // number of copies of each template
  // temp diabled channels are not noted in the in_tracks or out_tracks - if we discover one when checking cahnnels
  // we will add an estra input or output flagged as IGNORE. This allows for these channels to be re-enabled
  // and connected to a layer source. Gnerally these types of inputs are used in compositors and will have pvary and svary
  // so there will be no additional costs on adding (except prehaps proc_time).
  // in this case we would also change in_tracks, and increase nintracks accordingly
  // in other words, n_inputs can be > nintracks if we have ingored inputs, thus we need to store nintracks
  // spearately
  int nintracks, noutracks;
  
  /////////////////////////////////
  // global palette values for the node. These values are used for inputs / outputs which dont have an own palette_list
  // 

  int cpal; // current palette for node->dsource
  int npals; // num values in pals, excluding WEED_PALETTE_END
  // int [npals + 1]
  int *pals; // array of input pals (out pals to next node)

  // this is set for the node, and for inputs and outputs with own pal_list
  // this is the actua palette that the object repressented by the node should use
  int optimal_pal;
  int optimal_gamma;

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
  in n_clip_srcs;

  input_node_t **inputs;

  uint64_t flags; // inst_node flags, restictions and prefs
} inst_node_t;

void build_nodes_model(lives_nodemodel_t **);
void free_nodes_model(lives_nodemodel_t **);

void find_best_routes(lives_nodemodel_t *, 
		      double *factors, ticks_t t_thresh);

inst_node_t *find_node_for_source(lives_nodemodel_t *, lives_clip_src_t *dsource);

inst_node_t *get_inode_for_track(inst_node_t *n, int track);

void get_true_costs(lives_nodemodel_t *, double *factors);

int  get_best_in_palette(inst_node_t *, int inp_idx, int cost_type);
int  get_best_out_palette(inst_node_t *, int out_idx, int cost_type);

double get_min_cost_to_node(inst_node_t *, int cost_type);

lives_result_t inst_node_set_flags(inst_node_t *n, int flags);

void describe_chains(LiVESList *node_srcs);

#endif
