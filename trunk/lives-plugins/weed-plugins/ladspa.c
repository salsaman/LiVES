// ladspa.c
// weed plugin wrapper for LADSPA effects
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h> // optional
#include <weed/weed-effects.h>
#else
#include "../../libweed/weed.h"
#include "../../libweed/weed-palettes.h" // optional
#include "../../libweed/weed-effects.h"
#endif


///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <dlfcn.h>

#include <limits.h>

#ifndef PATH_MAX
#ifdef MAX_PATH
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif

#include <ladspa.h>

typedef LADSPA_Descriptor *(*lad_descriptor_f)(unsigned long);
typedef LADSPA_Handle(*lad_instantiate_f)(const struct _LADSPA_Descriptor *Descriptor, unsigned long SampleRate);
typedef void (*lad_activate_f)(LADSPA_Handle Instance);
typedef void (*lad_deactivate_f)(LADSPA_Handle Instance);
typedef void (*lad_connect_port_f)(LADSPA_Handle Instance, unsigned long Port, LADSPA_Data *DataLocation);
typedef void (*lad_run_f)(LADSPA_Handle Instance, unsigned long SampleCount);
typedef void (*lad_cleanup_f)(LADSPA_Handle Instance);

typedef struct {
  LADSPA_Handle handle_l;
  int activated_l;
  LADSPA_Handle handle_r;
  int activated_r;
} _sdata;


#define DEF_ARATE 44100

//////////////////////////////////////////////////////////////////

static void getenv_piece(char *target, size_t tlen, char *envvar, int num) {
  // get num piece from envvar path and set in target
  char *str1;
  memset(target,0,1);

  /* extract first string from string sequence */
  str1 = strtok(envvar, ":");

  /* loop until finishied */
  while (--num>=0) {
    /* extract string from string sequence */
    str1 = strtok(NULL, ":");

    /* check if there is nothing else to extract */
    if (str1 == NULL) break;
  }

  if (str1!=NULL) snprintf(target,tlen,"%s",str1);

}


static int float_deinterleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i,j;

  float *tmpfbuffer=(float *)weed_malloc(nsamps*nchans*sizeof(float));
  if (tmpfbuffer==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0; i<nsamps; i++) {
    for (j=0; j<nchans; j++) {
      tmpfbuffer[nsamps*j+i]=fbuffer[i*nchans+j];
    }
  }
  weed_memcpy(fbuffer,tmpfbuffer,nsamps*nchans*sizeof(float));
  weed_free(tmpfbuffer);
  return WEED_NO_ERROR;
}


static int float_interleave(float *fbuffer, int nsamps, int nchans) {
  // deinterleave a float buffer
  register int i,j;

  float *tmpfbuffer=(float *)weed_malloc(nsamps*nchans*sizeof(float));
  if (tmpfbuffer==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  for (i=0; i<nsamps; i++) {
    for (j=0; j<nchans; j++) {
      tmpfbuffer[i*nchans+j]=fbuffer[j*nsamps+i];
    }
  }
  weed_memcpy(fbuffer,tmpfbuffer,nsamps*nchans*sizeof(float));
  weed_free(tmpfbuffer);
  return WEED_NO_ERROR;
}



/////////////////////////////////////////////////////////////


int ladspa_init(weed_plant_t *inst) {
  int error;
  weed_plant_t *channel=NULL;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);

  lad_instantiate_f lad_instantiate_func=(lad_instantiate_f)weed_get_voidptr_value(filter,"plugin_lad_instantiate_func",&error);
  LADSPA_Descriptor *laddes=(LADSPA_Descriptor *)weed_get_voidptr_value(filter,"plugin_lad_descriptor",&error);

  _sdata *sdata;

  int rate=0;
  int pinc,poutc;

  register int i;

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));
  if (sdata==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  if (weed_plant_has_leaf(inst,"in_channels")) channel=weed_get_plantptr_value(inst,"in_channels",&error);
  if (channel==NULL&&weed_plant_has_leaf(inst,"out_channels")) channel=weed_get_plantptr_value(inst,"out_channels",&error);

  if (channel!=NULL) rate=weed_get_int_value(channel,"audio_rate",&error);

  if (rate==0) rate=DEF_ARATE;

  pinc=weed_get_int_value(filter,"plugin_in_channels",&error);
  poutc=weed_get_int_value(filter,"plugin_out_channels",&error);

  sdata->activated_l=sdata->activated_r=WEED_FALSE;

  sdata->handle_l=(lad_instantiate_func)(laddes,rate);
  if (pinc==1||poutc==1) sdata->handle_r=(lad_instantiate_func)(laddes,rate);
  else sdata->handle_r=NULL;

  weed_set_voidptr_value(inst,"plugin_data",sdata);

  if (weed_get_boolean_value(filter,"plugin_dual",&error)==WEED_TRUE) {
    if (weed_plant_has_leaf(inst,"in_parameters")) {
      weed_plant_t **in_params=(weed_plant_t **)weed_get_plantptr_array(inst,"in_parameters",&error);
      int ninps=(weed_leaf_num_elements(inst,"in_parameters")-2)/2;
      int link=weed_get_boolean_value(in_params[ninps*2],"value",&error);
      for (i=0; i<ninps; i++) {
        weed_plant_t *ptmpl=weed_get_plantptr_value(in_params[i],"template",&error);
        weed_plant_t *gui=weed_parameter_template_get_gui(ptmpl);
        if (link==WEED_TRUE) {
          weed_set_int_value(gui,"copy_value_to",i+ninps);
          ptmpl=weed_get_plantptr_value(in_params[i+ninps],"template",&error);
          gui=weed_parameter_template_get_gui(ptmpl);
          weed_set_int_value(gui,"copy_value_to",i);
        } else {
          weed_set_int_value(gui,"copy_value_to",-1);
          ptmpl=weed_get_plantptr_value(in_params[i+ninps],"template",&error);
          gui=weed_parameter_template_get_gui(ptmpl);
          weed_set_int_value(gui,"copy_value_to",-1);
        }
      }
    }
  }

  return WEED_NO_ERROR;

}


int ladspa_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  lad_deactivate_f lad_deactivate_func=(lad_activate_f)weed_get_voidptr_value(filter,"plugin_lad_deactivate_func",&error);
  lad_cleanup_f lad_cleanup_func=(lad_cleanup_f)weed_get_voidptr_value(filter,"plugin_lad_cleanup_func",&error);

  if (sdata->activated_l==WEED_TRUE) {
    if (lad_deactivate_func!=NULL)(*lad_deactivate_func)(sdata->handle_l);
    if (lad_cleanup_func!=NULL)(*lad_cleanup_func)(sdata->handle_l);
  }

  if (sdata->activated_r==WEED_TRUE) {
    if (lad_deactivate_func!=NULL)(*lad_deactivate_func)(sdata->handle_r);
    if (lad_cleanup_func!=NULL)(*lad_cleanup_func)(sdata->handle_r);
  }

  if (sdata!=NULL) {
    weed_free(sdata);
  }

  return WEED_NO_ERROR;

}



int ladspa_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  int nsamps=0;

  int pinc,poutc;
  int iinc=0,ioutc=0,oinc,ooutc;
  int iinp=0,ioutp=0,pinp=0,poutp=0;
  int inplace=WEED_FALSE;
  int inter=WEED_FALSE;
  int dual=WEED_FALSE;
  int rate=DEF_ARATE;

  weed_plant_t *in_channel=NULL,*out_channel=NULL;
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  weed_plant_t *ptmpl;

  lad_connect_port_f lad_connect_port_func;
  lad_run_f lad_run_func;

  float *src=NULL,*dst=NULL,*psrc,*pdst;
  float *invals=NULL,*outvals=NULL;

  LADSPA_Descriptor *laddes;
  LADSPA_PortDescriptor ladpdes;
  LADSPA_PortRangeHint ladphint;
  LADSPA_PortRangeHintDescriptor ladphintdes;
  LADSPA_Handle handle;

  weed_plant_t **in_params=NULL;
  weed_plant_t **out_params=NULL;

  _sdata *sdata=(_sdata *)weed_get_voidptr_value(inst,"plugin_data",&error);

  register int i;


  if (weed_plant_has_leaf(inst,"in_channels")) {
    in_channel=weed_get_plantptr_value(inst,"in_channels",&error);
    if (in_channel!=NULL) {
      src=(float *)weed_get_voidptr_value(in_channel,"audio_data",&error);
      if (src==NULL) return WEED_NO_ERROR;
      iinc=weed_get_int_value(in_channel,"audio_channels",&error);
      nsamps=weed_get_int_value(in_channel,"audio_data_length",&error);
      inter=weed_get_boolean_value(in_channel,"audio_interleaf",&error);
      rate=weed_get_int_value(in_channel,"audio_rate",&error);
    }
  }

  if (weed_plant_has_leaf(inst,"out_channels")) {
    out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
    if (out_channel!=NULL) {
      dst=(float *)weed_get_voidptr_value(out_channel,"audio_data",&error);
      if (dst==NULL) return WEED_NO_ERROR;
      ioutc=weed_get_int_value(out_channel,"audio_channels",&error);
      nsamps=weed_get_int_value(out_channel,"audio_data_length",&error);
      inter=weed_get_boolean_value(out_channel,"audio_interleaf",&error);
      rate=weed_get_int_value(out_channel,"audio_rate",&error);
    }
  }

  ooutc=ioutc;
  oinc=iinc;

  if (src==dst) inplace=WEED_TRUE;

  pinc=weed_get_int_value(filter,"plugin_in_channels",&error);
  poutc=weed_get_int_value(filter,"plugin_out_channels",&error);

  if (pinc>0&&iinc==0) return WEED_NO_ERROR;
  if (poutc>0&&ioutc==0) return WEED_NO_ERROR;

  if (pinc==2&&iinc==1) return WEED_NO_ERROR;
  if (poutc==2&&ioutc==1) return WEED_NO_ERROR;


  if (pinc==1&&iinc==2) dual=WEED_TRUE;
  if (poutc==1&&ioutc==2) dual=WEED_TRUE;

  if (sdata->activated_l==WEED_FALSE) {
    lad_activate_f lad_activate_func=(lad_activate_f)weed_get_voidptr_value(filter,"plugin_lad_activate_func",&error);
    if (lad_activate_func!=NULL)(*lad_activate_func)(sdata->handle_l);
    sdata->activated_l=WEED_TRUE;
  }

  if (dual&&sdata->activated_r==WEED_FALSE) {
    lad_activate_f lad_activate_func=(lad_activate_f)weed_get_voidptr_value(filter,"plugin_lad_activate_func",&error);
    if (lad_activate_func!=NULL)(*lad_activate_func)(sdata->handle_r);
    sdata->activated_r=WEED_TRUE;
  }

  if (iinc>1&&inter==WEED_TRUE) {
    // we asked the host for non-interleaved audio, but just in case...
    if (float_deinterleave(src,nsamps,iinc)!=WEED_NO_ERROR) return WEED_ERROR_HARDWARE;
  }

  laddes=(LADSPA_Descriptor *)weed_get_voidptr_value(filter,"plugin_lad_descriptor",&error);
  lad_connect_port_func=(lad_connect_port_f)weed_get_voidptr_value(filter,"plugin_lad_connect_port_func",&error);

  lad_run_func=(lad_run_f)weed_get_voidptr_value(filter,"plugin_lad_run_func",&error);

  handle=sdata->handle_l;

  psrc=src;
  pdst=dst;

  if (pinc>0||poutc>0) {
    // connect audio ports

    for (i=0; i<laddes->PortCount; i++) {
      ladpdes=laddes->PortDescriptors[i];

      if (ladpdes&LADSPA_PORT_AUDIO) {
        // channel
        if (ladpdes&LADSPA_PORT_INPUT) {
          // connect to next instance audio in
          (*lad_connect_port_func)(handle,i,(LADSPA_Data *)psrc);
          psrc+=nsamps;
          iinc--;
        } else {
          // connect to next instance audio out
          (*lad_connect_port_func)(handle,i,(LADSPA_Data *)pdst);
          pdst+=nsamps;
          ioutc--;
        }
      }
    }

  }

  if (weed_plant_has_leaf(inst,"in_parameters")) {
    iinp=weed_leaf_num_elements(inst,"in_parameters");
    in_params=(weed_plant_t **)weed_get_plantptr_array(inst,"in_parameters",&error);
    if (weed_get_boolean_value(filter,"plugin_dual",&error)==WEED_TRUE) iinp-=2;
  }

  if (weed_plant_has_leaf(inst,"out_parameters")) {
    ioutp=weed_leaf_num_elements(inst,"out_parameters");
    out_params=(weed_plant_t **)weed_get_plantptr_array(inst,"out_parameters",&error);
  }


  if (ioutp>0) outvals=weed_malloc(ioutp*sizeof(float));
  if (iinp>0) invals=weed_malloc(iinp*sizeof(float));

  if (iinp>0||ioutp>0) {

    for (i=0; i<laddes->PortCount; i++) {
      ladpdes=laddes->PortDescriptors[i];

      if (ladpdes&LADSPA_PORT_CONTROL) {
        // control ports
        ladphint=laddes->PortRangeHints[i];
        ladphintdes=ladphint.HintDescriptor;
        ptmpl=weed_get_plantptr_value(in_params[pinp],"template",&error);

        if (ladpdes&LADSPA_PORT_INPUT) {
          // connect to next instance in param
          if (ladphintdes&LADSPA_HINT_TOGGLED) {
            invals[pinp]=(float)weed_get_boolean_value(in_params[pinp],"value",&error);
          } else if (ladphintdes&LADSPA_HINT_TOGGLED) {
            invals[pinp]=(float)weed_get_int_value(in_params[pinp],"value",&error);
          } else {
            invals[pinp]=(float)weed_get_double_value(in_params[pinp],"value",&error);
          }
          if (weed_get_boolean_value(ptmpl,"plugin_sample_rate",&error)==WEED_TRUE) invals[pinp]*=(float)rate;
          (*lad_connect_port_func)(handle,i,(LADSPA_Data *)&invals[pinp++]);
        } else {
          // connect to store for out params
          (*lad_connect_port_func)(handle,i,(LADSPA_Data *)&outvals[poutp++]);
        }
      }
    }

  }


  (*lad_run_func)(handle,nsamps);

  handle=sdata->handle_r;


  if (pinc>0||poutc>0) {
    if (ioutc>0||iinc>0) {
      // the second instance out channel is unconnected; we need to connect it to the second lad instance

      // if we have an unconnected (second) in channel, we connect it here; otherwise we connect both input ports again
      if (iinc==0) psrc=src;

      for (i=0; i<laddes->PortCount; i++) {
        ladpdes=laddes->PortDescriptors[i];

        if (ladpdes&LADSPA_PORT_AUDIO) {
          // channel
          if (ladpdes&LADSPA_PORT_INPUT) {
            // connect to next instance audio in
            (*lad_connect_port_func)(handle,i,(LADSPA_Data *)psrc);
            psrc+=nsamps;
          } else {
            // connect to next instance audio out
            (*lad_connect_port_func)(handle,i,(LADSPA_Data *)pdst);
          }
        }
      }
    }
  }


  if (iinp>0||ioutp>0) {

    if (pinp<iinp||poutp<ioutp) {
      // need to use second instance
      for (i=0; i<laddes->PortCount; i++) {
        ladpdes=laddes->PortDescriptors[i];

        if (ladpdes&LADSPA_PORT_CONTROL) {
          // control ports
          ladphint=laddes->PortRangeHints[i];
          ladphintdes=ladphint.HintDescriptor;
          ptmpl=weed_get_plantptr_value(in_params[pinp],"template",&error);

          if (ladpdes&LADSPA_PORT_INPUT) {
            // connect to next instance in param
            if (ladphintdes&LADSPA_HINT_TOGGLED) {
              invals[pinp]=(float)weed_get_boolean_value(in_params[pinp],"value",&error);
            } else if (ladphintdes&LADSPA_HINT_TOGGLED) {
              invals[pinp]=(float)weed_get_int_value(in_params[pinp],"value",&error);
            } else {
              invals[pinp]=(float)weed_get_double_value(in_params[pinp],"value",&error);
            }
            if (weed_get_boolean_value(ptmpl,"plugin_sample_rate",&error)==WEED_TRUE) invals[pinp]*=(float)rate;

            (*lad_connect_port_func)(handle,i,(LADSPA_Data *)&invals[pinp++]);
          } else {
            // connect to store for out params
            (*lad_connect_port_func)(handle,i,(LADSPA_Data *)&outvals[poutp++]);
          }
        }
      }

    }

  }


  if (dual)(*lad_run_func)(handle,nsamps);

  // copy out values to out_params
  for (i=0; i<ioutp; i++) {
    switch (weed_leaf_seed_type(out_params[i],"value")) {
    case WEED_SEED_BOOLEAN:
      weed_set_boolean_value(out_params[i],"value",(int)outvals[i]);
      weed_set_int64_value(out_params[i],"timecode",timestamp);
      break;
    case WEED_SEED_INT:
      weed_set_int_value(out_params[i],"value",(int)outvals[i]);
      weed_set_int64_value(out_params[i],"timecode",timestamp);
      break;
    default:
      weed_set_double_value(out_params[i],"value",(double)outvals[i]);
      weed_set_int64_value(out_params[i],"timecode",timestamp);
      break;
    }
  }

  if (invals!=NULL) weed_free(invals);
  if (outvals!=NULL) weed_free(outvals);
  if (in_params!=NULL) weed_free(in_params);
  if (out_params!=NULL) weed_free(out_params);

  // just in case the host wanted interleaved audio...
  if (inter==WEED_TRUE&&oinc>1) {
    if (float_interleave(src,nsamps,oinc)!=WEED_NO_ERROR) return WEED_ERROR_HARDWARE;
  }

  if (inter==WEED_TRUE&&ooutc>1&&!inplace) {
    if (float_interleave(dst,nsamps,ooutc)!=WEED_NO_ERROR) return WEED_ERROR_HARDWARE;
  }


  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  unsigned long num_plugins=0;

  int num_filters=0;

  if (plugin_info!=NULL) {
    // get LADSPA path

    char *lpp=getenv("LADSPA_PATH");

    char **rfx_strings=NULL;
    weed_plant_t **out_chantmpls=NULL,**in_chantmpls=NULL;
    weed_plant_t **in_params=NULL,**out_params=NULL,*gui;

    struct dirent *vdirent=NULL;
    weed_plant_t *filter_class;

    int vdirval=0;
    int pnum,wnum;

    int new_stdout,new_stderr;

    LADSPA_Descriptor *laddes;
    LADSPA_Properties ladprop;
    LADSPA_PortDescriptor ladpdes;
    LADSPA_PortRangeHint ladphint;
    LADSPA_PortRangeHintDescriptor ladphintdes;
    LADSPA_Data lbound;
    LADSPA_Data ubound;
    LADSPA_Data defval;

    lad_descriptor_f lad_descriptor_func;
    lad_instantiate_f lad_instantiate_func;
    lad_activate_f lad_activate_func;
    lad_deactivate_f lad_deactivate_func;
    lad_connect_port_f lad_connect_port_func;
    lad_run_f lad_run_func;
    lad_cleanup_f lad_cleanup_func;

    char cname[128];

#define LABSIZE 1024

    char label[LABSIZE];
    char vdir[PATH_MAX];
    char plugin_name[PATH_MAX],weed_name[PATH_MAX],plug1[PATH_MAX];
    DIR *curvdir=NULL;

    void *handle;

    int ninps,noutps,ninchs,noutchs;
    int oninps,onoutps,oninchs,onoutchs;
    int cninps,cnoutps;
    int stcount;

    int dual;

    register int i,j;

    //#define DEBUG
#ifndef DEBUG
    new_stdout=dup(1);
    new_stderr=dup(2);

    close(1);
    close(2);
#endif

    while (1) {
      // split by :
      getenv_piece(vdir,PATH_MAX,lpp,vdirval);
      if (!strlen(vdir)||vdir==NULL) break;
      curvdir=opendir(vdir);
      if (curvdir==NULL) break;

      while (1) {
        // check each plugin
        vdirent=readdir(curvdir);
        if (vdirent==NULL) {
          closedir(curvdir);
          curvdir=NULL;
          vdirval++;
          break;
        }

        if (!strncmp(vdirent->d_name,"..",strlen(vdirent->d_name))) continue;

        snprintf(plugin_name,PATH_MAX,"%s",vdirent->d_name);
        snprintf(plug1,PATH_MAX,"%s/%s",vdir,plugin_name);

        //fprintf(stderr,"checking %s\n",plug1);

        handle=dlopen(plug1,RTLD_NOW);
        if (handle==NULL) continue;

        if ((lad_descriptor_func=dlsym(handle,"ladspa_descriptor"))==NULL) {
          dlclose(handle);
          continue;
        }

        num_plugins=0;

        while ((laddes=((*lad_descriptor_func)(num_plugins))) != NULL) {

          if ((lad_instantiate_func=laddes->instantiate)==NULL) {
            continue;
          }

          lad_activate_func=laddes->activate;

          lad_deactivate_func=laddes->deactivate;

          if ((lad_connect_port_func=laddes->connect_port)==NULL) {
            continue;
          }
          if ((lad_run_func=laddes->run)==NULL) {
            continue;
          }

          lad_cleanup_func=laddes->cleanup;

          //fprintf(stderr,"2checking %s : %ld\n",plug1,num_plugins);
          ladprop=laddes->Properties;

          ninps=noutps=ninchs=noutchs=0;

          // create the in_parameters, out_parameters, in_channels, out_channels

          for (i=0; i<laddes->PortCount; i++) {
            // count number of each type first
            ladpdes=laddes->PortDescriptors[i];
            ladphint=laddes->PortRangeHints[i];

            if (ladpdes&LADSPA_PORT_CONTROL) {
              // parameter
              if (ladpdes&LADSPA_PORT_INPUT) {
                // in_param
                ninps++;
              } else {
                // out_param
                noutps++;
              }
            } else {
              // channel
              if (ladpdes&LADSPA_PORT_INPUT) {
                // in_channel
                ninchs++;
              } else {
                // out_channel
                noutchs++;
              }
            }
          }


          // do we need to double up the channels ?
          // if we have 0,1 or 2 input channels, 1 outputs
          // 1 input, 0 or 2 outputs

          if ((ninchs<2&&noutchs<2&&(ninchs+noutchs>0))||noutchs==1) {
            dual=WEED_TRUE;
            stcount=1;
          } else {
            dual=WEED_FALSE;
            stcount=0;
          }

          oninchs=ninchs;
          onoutchs=noutchs;
          oninps=ninps;
          onoutps=noutps;

          if (dual&&oninchs==1) ninchs=2;
          if (dual&&onoutchs==1) noutchs=2;

          if (dual==WEED_TRUE) {
            ninps=oninps*2;
            noutps=onoutps*2;

            rfx_strings=weed_malloc((ninps+3+stcount)*sizeof(char *));
            for (pnum=0; pnum<ninps+3+stcount; pnum++) {
              rfx_strings[pnum]=(char *)weed_malloc(256);
            }

            if (ninchs==0) {
              sprintf(rfx_strings[stcount],"layout|\"Left output channel\"|");
              sprintf(rfx_strings[oninps+2+stcount],"layout|\"Right output channel\"|");
            } else {
              if (ninchs==1) sprintf(rfx_strings[stcount],"layout|\"Left/mono channel\"");
              else sprintf(rfx_strings[stcount],"layout|\"Left channel\"");
              sprintf(rfx_strings[oninps+2+stcount],"layout|\"Right channel\"");
            }
            sprintf(rfx_strings[oninps+1+stcount],"layout|hseparator|");
          }

          if (ninps>0) {
            // add extra in 2 extra params for "link channels"
            in_params=(weed_plant_t **)weed_malloc((++ninps+stcount*2)*sizeof(weed_plant_t *));
            in_params[ninps+stcount*2-1]=NULL;
            if (dual==WEED_TRUE) {
              in_params[ninps-1]=weed_switch_init("link","_Link left and right parameters",WEED_TRUE);
              gui=weed_parameter_template_get_gui(in_params[ninps-1]);
              weed_set_int_value(gui,"copy_value_to",ninps);
              sprintf(rfx_strings[0],"layout|p%d|",ninps-1);

              // link it to a dummy param which is hidden and reinit - this allows the value to be updated and the plugin to be reinited at any time
              in_params[ninps]=weed_switch_init("link dummy","linkdummy",WEED_TRUE);
              weed_set_int_value(in_params[ninps],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);
              gui=weed_parameter_template_get_gui(in_params[ninps]);
              weed_set_boolean_value(gui,"hidden",WEED_TRUE);
            }

          } else in_params=NULL;

          if (noutps>0) {
            out_params=(weed_plant_t **)weed_malloc(++noutps*sizeof(weed_plant_t *));
            out_params[noutps-1]=NULL;
          } else out_params=NULL;

          //fprintf(stderr,"%s has inc %d and outc %d\n",laddes->Name,ninchs,noutchs);

          if (ninchs>0) {
            in_chantmpls=(weed_plant_t **)weed_malloc(2*sizeof(weed_plant_t *));
            if (ninchs==1) sprintf(cname,"Mono channel");
            else if (ninchs==2) sprintf(cname,"Stereo channel");
            else sprintf(cname,"Multi channel");
            in_chantmpls[0]=weed_audio_channel_template_init(cname,0);
            in_chantmpls[1]=NULL;
            if (!dual||oninchs!=1) weed_set_int_value(in_chantmpls[0],"audio_channels",ninchs);
            weed_set_boolean_value(in_chantmpls[0],"audio_interleaf",WEED_FALSE);
          } else in_chantmpls=NULL;

          if (noutchs>0) {
            out_chantmpls=(weed_plant_t **)weed_malloc(++noutchs*sizeof(weed_plant_t *));
            if (noutchs==1) sprintf(cname,"Mono channel");
            else if (noutchs==2) sprintf(cname,"Stereo channel");
            else sprintf(cname,"Multi channel");
            out_chantmpls[0]=weed_audio_channel_template_init(cname,0);
            out_chantmpls[1]=NULL;
            if (!dual||onoutchs!=1) weed_set_int_value(out_chantmpls[0],"audio_channels",noutchs);
            weed_set_boolean_value(out_chantmpls[0],"audio_interleaf",WEED_FALSE);
            if (oninchs==onoutchs&&!(ladprop&LADSPA_PROPERTY_INPLACE_BROKEN)) weed_set_int_value(out_chantmpls[0],"flags",WEED_CHANNEL_CAN_DO_INPLACE);
          } else out_chantmpls=NULL;

          cnoutps=cninps=0;

          for (i=0; i<laddes->PortCount; i++) {
            ladpdes=laddes->PortDescriptors[i];
            ladphint=laddes->PortRangeHints[i];
            ladphintdes=ladphint.HintDescriptor;

            if (ladpdes&LADSPA_PORT_CONTROL) {
              // parameter
              if (ladpdes&LADSPA_PORT_INPUT) {
                // in_param
                snprintf(label,LABSIZE,"_%s",laddes->PortNames[i]);

                if (ladphintdes&LADSPA_HINT_TOGGLED) {
                  // boolean type
                  if (ladphintdes&LADSPA_HINT_DEFAULT_1) {
                    in_params[cninps]=weed_switch_init(laddes->PortNames[i],label,WEED_TRUE);
                    if (dual) in_params[cninps+oninps]=weed_switch_init(laddes->PortNames[i],label,WEED_TRUE);
                  } else {
                    in_params[cninps]=weed_switch_init(laddes->PortNames[i],label,WEED_FALSE);
                    if (dual) in_params[cninps+oninps]=weed_switch_init(laddes->PortNames[i],label,WEED_FALSE);
                  }
                } else {
                  // int/float type
                  lbound=ladphint.LowerBound;
                  ubound=ladphint.UpperBound;

                  if (lbound==0.&&ubound==0.) {
                    // try some sanity
                    lbound=0.;
                    ubound=1.;
                  }

                  defval=lbound;

                  if (ladphintdes&LADSPA_HINT_DEFAULT_LOW) {
                    defval+=(ubound-lbound)/4.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_MIDDLE) {
                    defval+=(ubound-lbound)/2.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_HIGH) {
                    defval+=(ubound-lbound)*3./4.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_MAXIMUM) {
                    defval=ubound;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_0) {
                    defval=0.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_1) {
                    defval=1.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_100) {
                    defval=100.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_440) {
                    defval=440.;
                  }
                  if (ladphintdes&LADSPA_HINT_INTEGER) {
                    in_params[cninps]=weed_integer_init(laddes->PortNames[i],label,defval+.5,lbound+.5,ubound+.5);
                    if (dual) in_params[cninps+oninps]=weed_integer_init(laddes->PortNames[i],label,defval+.5,lbound+.5,ubound+.5);
                  } else {
                    in_params[cninps]=weed_float_init(laddes->PortNames[i],label,defval,lbound,ubound);
                    if (dual) in_params[cninps+oninps]=weed_float_init(laddes->PortNames[i],label,defval,lbound,ubound);
                  }

                }

                if (ladphintdes&LADSPA_HINT_SAMPLE_RATE) weed_set_boolean_value(in_params[cninps],"plugin_sample_rate",WEED_TRUE);
                else weed_set_boolean_value(in_params[cninps],"plugin_sample_rate",WEED_FALSE);

                if (ladphintdes&LADSPA_HINT_LOGARITHMIC) weed_set_boolean_value(in_params[cninps],"plugin_logarithmic",WEED_TRUE);
                else weed_set_boolean_value(in_params[cninps],"plugin_logarithmic",WEED_FALSE);

                if (dual) {
                  if (ladphintdes&LADSPA_HINT_SAMPLE_RATE) weed_set_boolean_value(in_params[cninps+oninps],"plugin_sample_rate",WEED_TRUE);
                  else weed_set_boolean_value(in_params[cninps+oninps],"plugin_sample_rate",WEED_FALSE);

                  if (ladphintdes&LADSPA_HINT_LOGARITHMIC) weed_set_boolean_value(in_params[cninps+oninps],"plugin_logarithmic",WEED_TRUE);
                  else weed_set_boolean_value(in_params[cninps+oninps],"plugin_logarithmic",WEED_FALSE);

                  sprintf(rfx_strings[cninps+1+stcount],"layout|p%d|",cninps);
                  sprintf(rfx_strings[cninps+oninps+3+stcount],"layout|p%d|",cninps+oninps);
                }

                cninps++;
              } else {
                // out_param
                if (ladphintdes&LADSPA_HINT_TOGGLED) {
                  // boolean type
                  if (ladphintdes&LADSPA_HINT_DEFAULT_1) {
                    out_params[cnoutps]=weed_out_param_switch_init(laddes->PortNames[i],WEED_TRUE);
                    if (dual) out_params[cnoutps+onoutps]=weed_out_param_switch_init(laddes->PortNames[i],WEED_TRUE);
                  } else {
                    out_params[cnoutps]=weed_out_param_switch_init(laddes->PortNames[i],WEED_FALSE);
                    if (dual) out_params[cnoutps+onoutps]=weed_out_param_switch_init(laddes->PortNames[i],WEED_FALSE);
                  }
                } else {
                  // int/float type
                  lbound=ladphint.LowerBound;
                  ubound=ladphint.UpperBound;
                  defval=lbound;

                  if (ladphintdes&LADSPA_HINT_DEFAULT_LOW) {
                    defval+=(ubound-lbound)/4.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_MIDDLE) {
                    defval+=(ubound-lbound)/2.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_HIGH) {
                    defval+=(ubound-lbound)*3./4.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_MAXIMUM) {
                    defval=ubound;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_0) {
                    defval=0.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_1) {
                    defval=1.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_100) {
                    defval=100.;
                  }

                  else if (ladphintdes&LADSPA_HINT_DEFAULT_440) {
                    defval=440.;
                  }

                  if (ladphintdes&LADSPA_HINT_INTEGER) {
                    out_params[cnoutps]=weed_out_param_integer_init(laddes->PortNames[i],defval+.5,lbound+.5,ubound+.5);
                    if (dual) out_params[cnoutps+onoutps]=weed_out_param_integer_init(laddes->PortNames[i],defval+.5,lbound+.5,ubound+.5);
                  } else {
                    out_params[cnoutps]=weed_out_param_float_init(laddes->PortNames[i],defval,lbound,ubound);
                    if (dual) out_params[cnoutps+onoutps]=weed_out_param_float_init(laddes->PortNames[i],defval,lbound,ubound);
                  }

                }

                if (ladphintdes&LADSPA_HINT_SAMPLE_RATE) weed_set_boolean_value(out_params[cnoutps],"plugin_sample_rate",WEED_TRUE);
                else weed_set_boolean_value(out_params[cnoutps],"plugin_sample_rate",WEED_FALSE);

                if (dual) {
                  if (ladphintdes&LADSPA_HINT_SAMPLE_RATE) weed_set_boolean_value(out_params[cnoutps],"plugin_sample_rate",WEED_TRUE);
                  else weed_set_boolean_value(out_params[cnoutps],"plugin_sample_rate",WEED_FALSE);
                }

                cnoutps++;
              }


            }

          }

          snprintf(weed_name,PATH_MAX,"LADSPA: %s",laddes->Name);

          filter_class=weed_filter_class_init(weed_name,"LADSPA developers",0,0,&ladspa_init,&ladspa_process,&ladspa_deinit,
                                              in_chantmpls,out_chantmpls,in_params,out_params);

          weed_set_string_value(filter_class,"extra_authors",laddes->Maker);

          weed_set_voidptr_value(filter_class,"plugin_lad_descriptor",laddes);

          weed_set_voidptr_value(filter_class,"plugin_lad_descriptor_func",(void *)lad_descriptor_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_instantiate_func",(void *)lad_instantiate_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_activate_func",(void *)lad_activate_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_deactivate_func",(void *)lad_deactivate_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_connect_port_func",(void *)lad_connect_port_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_run_func",(void *)lad_run_func);
          weed_set_voidptr_value(filter_class,"plugin_lad_cleanup_func",(void *)lad_cleanup_func);

          if (dual==WEED_TRUE) {
            weed_set_boolean_value(filter_class,"plugin_dual",WEED_TRUE);
            if (ninps>0) {
              gui=weed_filter_class_get_gui(filter_class);
              weed_set_string_value(gui,"layout_scheme","RFX");
              weed_set_string_value(gui,"rfx_delim","|");

              // subtract 1 from ninps since we incremented it
              weed_set_string_array(gui,"rfx_strings",ninps+2+stcount,rfx_strings);
              for (wnum=0; wnum<ninps+2+stcount; wnum++) weed_free(rfx_strings[wnum]);
              weed_free(rfx_strings);
              rfx_strings=NULL;

              for (j=0; j<oninps; j++) {
                gui=weed_parameter_template_get_gui(in_params[j]);
                weed_set_int_value(gui,"copy_value_to",j+oninps);
                gui=weed_parameter_template_get_gui(in_params[j+oninps]);
                weed_set_int_value(gui,"copy_value_to",j);
              }

            }
          } else weed_set_boolean_value(filter_class,"plugin_dual",WEED_FALSE);

          if (in_params!=NULL) weed_free(in_params);
          if (out_params!=NULL) weed_free(out_params);
          if (in_chantmpls!=NULL) weed_free(in_chantmpls);
          if (out_chantmpls!=NULL) weed_free(out_chantmpls);

          weed_set_int_value(filter_class,"plugin_in_channels",oninchs);
          weed_set_int_value(filter_class,"plugin_out_channels",onoutchs);
          weed_set_int_value(filter_class,"plugin_in_params",oninps);
          weed_set_int_value(filter_class,"plugin_out_params",onoutps);

          weed_plugin_info_add_filter_class(plugin_info,filter_class);
          num_plugins++;
          num_filters++;
          //fprintf(stderr,"DONE\n");
        }
      }
    }

#ifndef DEBUG
    dup2(new_stdout,1);
    dup2(new_stderr,2);
#endif

  }


  if (num_filters==0) {
    fprintf(stderr,"No LADSPA plugins found; if you have them installed please set the LADSPA_PATH environment variable to point to them.\n");
    return NULL;
  }
  weed_set_int_value(plugin_info,"version",package_version);

  return plugin_info;
}

