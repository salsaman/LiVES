/* Frei0r wrapper for Weed
   author: Salsaman (G. Finch) <salsaman@xs4all.nl>

   Released under the Lesser Gnu Public License (LGPL) 3 or later
   See www.gnu.org for details

 (c) 2005 - 2009, Salsaman
*/




#include <frei0r.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

///////////////////////////////////////////////////////////////////

#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////
typedef f0r_instance_t (*f0r_construct_f)(unsigned int width, unsigned int height);
typedef void (*f0r_destruct_f)(f0r_instance_t instance);
typedef void (*f0r_deinit_f)(void);
typedef int (*f0r_init_f)(void);
typedef void (*f0r_get_plugin_info_f)(f0r_plugin_info_t *info);
typedef void (*f0r_get_param_info_f)(f0r_param_info_t *info, int param_index);
typedef void (*f0r_update_f)(f0r_instance_t instance, double time, const uint32_t *inframe, uint32_t *outframe);
typedef void (*f0r_update2_f)(f0r_instance_t instance, double time, const uint32_t *inframe1, const uint32_t *inframe2, const uint32_t *inframe3, uint32_t *outframe);
typedef void (*f0r_set_param_value_f)(f0r_instance_t *instance, f0r_param_t *param, int param_index);

////////////////////////////////////////////////////////////////



int frei0r_init (weed_plant_t *inst) {
  weed_plant_t *out_channel,*filter;
  int error,height,width;
  f0r_instance_t f0r_inst;
  f0r_construct_f f0r_construct;

  filter=weed_get_plantptr_value(inst,"filter_class",&error);

  out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  width=weed_get_int_value(out_channel,"rowstrides",&error)>>2;
  height=weed_get_int_value(out_channel,"height",&error);

  f0r_construct=weed_get_voidptr_value(filter,"plugin_f0r_construct",&error);

  if ((f0r_inst=(*f0r_construct) (width,height))==NULL) return WEED_ERROR_INIT_ERROR;
  weed_set_voidptr_value(inst,"plugin_f0r_inst",f0r_inst);
  return WEED_NO_ERROR;
}


int frei0r_deinit (weed_plant_t *inst) {
  int error;
  f0r_instance_t f0r_inst;
  f0r_destruct_f f0r_destruct;
  weed_plant_t *filter;

  filter=weed_get_plantptr_value(inst,"filter_class",&error);

  f0r_inst=weed_get_voidptr_value(inst,"plugin_f0r_inst",&error);
  f0r_destruct=weed_get_voidptr_value(filter,"plugin_f0r_destruct",&error);
  (*f0r_destruct)(f0r_inst);

  return WEED_NO_ERROR;
}


static void weed_params_to_frei0r_params (weed_plant_t *inst, weed_plant_t **in_params, int num_weed_params) {
  int i,error,hint;
  int pnum=0;
  weed_plant_t *ptmpl;
  int vali;
  double vald,vald2;
  double *cols;
  f0r_instance_t f0rinst=weed_get_voidptr_value(inst,"plugin_f0r_inst",&error);
  weed_plant_t *filter=weed_get_plantptr_value(inst,"filter_class",&error);
  f0r_set_param_value_f f0r_set_param_value=weed_get_voidptr_value(filter,"plugin_f0r_set_param_value",&error);
  f0r_param_position_t f0rpos;
  f0r_param_color_t f0rcol;
  char *string;

  for (i=0;i<num_weed_params;i++) {
    ptmpl=weed_get_plantptr_value(in_params[i],"template",&error);
    hint=weed_get_int_value(ptmpl,"hint",&error);
    switch (hint) {
    case WEED_HINT_SWITCH:
      vali=weed_get_boolean_value(in_params[i],"value",&error);
      vald=(double)vali;
      (*f0r_set_param_value)(f0rinst,(f0r_param_t)&vald,pnum);
      break;
    case WEED_HINT_FLOAT:
      vald=weed_get_double_value(in_params[i],"value",&error);
      if (!weed_plant_has_leaf(ptmpl,"plugin_f0r_position")) (*f0r_set_param_value)(f0rinst,(f0r_param_t)&vald,pnum);
      else {
	i++;
	vald2=weed_get_double_value(in_params[i],"value",&error);
	f0rpos.x=vald;
	f0rpos.y=vald2;
	(*f0r_set_param_value)(f0rinst,(f0r_param_t)&f0rpos,pnum);
      }
      break;
    case WEED_HINT_COLOR:
      cols=weed_get_double_array(in_params[i],"value",&error);
      f0rcol.r=cols[0];
      f0rcol.g=cols[1];
      f0rcol.b=cols[2];
      (*f0r_set_param_value)(f0rinst,(f0r_param_t)&f0rcol,pnum);
      weed_free(cols);
      break;
    case WEED_HINT_TEXT:
      string=weed_get_string_value(in_params[i],"value",&error);
      (*f0r_set_param_value)(f0rinst,(f0r_param_t)string,pnum);
      weed_free(string);
      break;
    }
    pnum++;
  }
}






int frei0r_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;

  f0r_instance_t f0r_inst;
  f0r_update_f f0r_update;
  f0r_update2_f f0r_update2;
  weed_plant_t *filter;
  weed_plant_t **in_channels,**out_channels,**in_params;
  int f0r_plugin_type;

  double time=timestamp/100000000.;

  filter=weed_get_plantptr_value(inst,"filter_class",&error);
  f0r_inst=weed_get_voidptr_value(inst,"plugin_f0r_inst",&error);
  f0r_plugin_type=weed_get_int_value(filter,"plugin_f0r_type",&error);

  if (weed_plant_has_leaf(inst,"in_parameters")&&(in_params=weed_get_plantptr_array(inst,"in_parameters",&error))!=NULL) {
    weed_params_to_frei0r_params(inst,in_params,weed_leaf_num_elements(inst,"in_parameters"));
  }

  switch (f0r_plugin_type) {
  case F0R_PLUGIN_TYPE_SOURCE:
    f0r_update=weed_get_voidptr_value(filter,"plugin_f0r_update",&error);
    out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
    (*f0r_update)(f0r_inst,time,NULL,weed_get_voidptr_value(out_channels[0],"pixel_data",&error));
    weed_free(out_channels);
    break;
  case F0R_PLUGIN_TYPE_FILTER:
    f0r_update=weed_get_voidptr_value(filter,"plugin_f0r_update",&error);
    out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
    in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
   (*f0r_update)(f0r_inst,time,weed_get_voidptr_value(in_channels[0],"pixel_data",&error),weed_get_voidptr_value(out_channels[0],"pixel_data",&error));
    weed_free(out_channels);
    weed_free(in_channels);
    break;
  case F0R_PLUGIN_TYPE_MIXER2:
    f0r_update2=weed_get_voidptr_value(filter,"plugin_f0r_update2",&error);
    out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
    in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
    (*f0r_update2)(f0r_inst,time,weed_get_voidptr_value(in_channels[0],"pixel_data",&error),weed_get_voidptr_value(in_channels[1],"pixel_data",&error),NULL,weed_get_voidptr_value(out_channels[0],"pixel_data",&error));
    weed_free(out_channels);
    weed_free(in_channels);
    break;
  case F0R_PLUGIN_TYPE_MIXER3:
    f0r_update2=weed_get_voidptr_value(filter,"plugin_f0r_update2",&error);
    out_channels=weed_get_plantptr_array(inst,"out_channels",&error);
    in_channels=weed_get_plantptr_array(inst,"in_channels",&error);
   (*f0r_update2)(f0r_inst,time,weed_get_voidptr_value(in_channels[0],"pixel_data",&error),weed_get_voidptr_value(in_channels[1],"pixel_data",&error),weed_get_voidptr_value(in_channels[2],"pixel_data",&error),weed_get_voidptr_value(out_channels[0],"pixel_data",&error));
    weed_free(out_channels);
    weed_free(in_channels);
    break;
  }
  return WEED_NO_ERROR;
}



weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info;
  if (FREI0R_MAJOR_VERSION!=1||FREI0R_MINOR_VERSION!=1) return NULL;

  plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    int *pal;
    int pnum,wnum,num_weed_params;
    char **rfx_strings=NULL;
    weed_plant_t **out_chantmpls,**in_chantmpls=NULL;
    weed_plant_t **in_params=NULL,*gui,*pgui;

    weed_plant_t *filter_class;
    f0r_plugin_info_t f0rinfo;
    f0r_param_info_t pinfo;
    int num_filters=0;

    int finished=0;

    char *vdir1="/usr/lib/frei0r-1/";
    char *vdir2="/usr/local/lib/frei0r-1/";
    char vdir3[512];

    char dir1[512],dir2[512],dir3[512];
    char plug1[512],plug2[512],plug3[512];

    struct dirent *vdirent=NULL,*dirent;

    char homedir[256];

    char vendor_name[256],plugin_name[256],weed_name[256];

    DIR *curvdir=NULL,*curdir=NULL;

    int vdirval=0;

    void *handle;

    int pversion;

    char *label;

    f0r_deinit_f f0r_deinit;
    f0r_init_f f0r_init;
    f0r_get_plugin_info_f f0r_get_plugin_info;
    f0r_get_param_info_f f0r_get_param_info;
    f0r_construct_f f0r_construct;
    f0r_destruct_f f0r_destruct;
    f0r_update_f f0r_update=NULL;
    f0r_update2_f f0r_update2=NULL;
    f0r_set_param_value_f f0r_set_param_value=NULL;

    // quick and dirty fix for 64bit systems
    char *fpp=getenv("FREI0R_PLUGIN_DIR");
    if (fpp!=NULL) vdir2=fpp;

    snprintf(homedir,256,"%s",getenv("HOME"));
    snprintf (vdir3,256,"%s/frei0r-1/",homedir);

    while (vdirval<6) {
      // step through each of our frei0r dirs
      if (vdirval==0) {
	curvdir=opendir(vdir3);
	if (curvdir==NULL) vdirval=2;
	else vdirval=1;
      }

      if (vdirval==2) {
	if (curvdir!=NULL) closedir(curvdir);
	curvdir=opendir(vdir2);
	if (curvdir==NULL) vdirval=4;
	else vdirval=3;
      }

      if (vdirval==4) {
	if (curvdir!=NULL) closedir(curvdir);
	curvdir=opendir(vdir1);
	if (curvdir==NULL) {
	  vdirval=6;
	  continue;
	}
	vdirval=5;
      }

      weed_memset(vendor_name,0,1);

      do {

	snprintf(dir1,512,"%s/%s",vdir1,vendor_name);
	snprintf(dir2,512,"%s/%s",vdir2,vendor_name);
	snprintf(dir3,512,"%s/%s",vdir3,vendor_name);
	
	vdirent=readdir(curvdir);

	if (vdirent==NULL) {
	  closedir(curvdir);
	  curvdir=NULL;
	  vdirval++;
	  break;
	}

	if (!strncmp(vdirent->d_name,"..",strlen(vdirent->d_name))) continue;

	snprintf(vendor_name,256,"%s",vdirent->d_name);

	if (vdirval==1) {
	  curdir=opendir(dir3);
	  if (curdir==NULL) continue;
	}
	else if (vdirval==3) {
	  if (curdir!=NULL) closedir(curdir);
	  curdir=opendir(dir2);
	  if (curdir==NULL) continue;
	}
	else if (vdirval==5) {
	  if (curdir!=NULL) closedir(curdir);
	  curdir=opendir(dir1);
	  if (curdir==NULL) continue;
	}

	finished=0;

	while (!finished) {
	  // step through our plugins
	  dirent=readdir(curdir);

	  if (dirent==NULL) {
	    finished=1;
	    continue;
	  }

	  if (!strncmp(dirent->d_name,"..",strlen(dirent->d_name))) continue;
	  
	  snprintf(plugin_name,256,"%s",dirent->d_name);

	  snprintf(plug1,512,"%s/%s",dir1,plugin_name);
	  snprintf(plug2,512,"%s/%s",dir2,plugin_name);
	  snprintf(plug3,512,"%s/%s",dir3,plugin_name);

	  handle=dlopen(plug3,RTLD_NOW);
	  if ((handle!=NULL&&(vdirval>1))||(handle==NULL&&vdirval==1)) {
	    if (handle!=NULL) dlclose(handle);
	    continue;
	  }
	  
	  if (vdirval>1) {
	    handle=dlopen(plug2,RTLD_NOW);
	    if ((handle!=NULL&&(vdirval>3))||(handle==NULL&&vdirval==3)) {
	      if (handle!=NULL) dlclose(handle);
	      continue;
	    }
	  }
	  
	  if (vdirval==5) {
	    handle=dlopen(plug1,RTLD_NOW);
	    if (handle==NULL) continue;
	  }
	  
	  if ((f0r_deinit=dlsym(handle,"f0r_deinit"))==NULL) {
	    dlclose(handle);
	    continue;
	  }
	  
	  if ((f0r_init=dlsym(handle,"f0r_init"))==NULL) {
	    dlclose(handle);
	    continue;
	  }
	  
	  if ((f0r_get_plugin_info=dlsym(handle,"f0r_get_plugin_info"))==NULL) {
	    dlclose(handle);
	    continue;
	  }

	  if ((f0r_get_param_info=dlsym(handle,"f0r_get_param_info"))==NULL) {
	    dlclose(handle);
	    continue;
	  }
	  
	  if ((f0r_construct=dlsym(handle,"f0r_construct"))==NULL) {
	    dlclose(handle);
	    continue;
	  }
	  
	  if ((f0r_destruct=dlsym(handle,"f0r_destruct"))==NULL) {
	    dlclose(handle);
	    continue;
	  }
	  
	  (*f0r_init)();
	  (*f0r_get_plugin_info)(&f0rinfo);
	  
	  if (f0rinfo.frei0r_version!=FREI0R_MAJOR_VERSION) {
	    (*f0r_deinit)();
	    dlclose(handle);
	    continue;
	  }

	  switch (f0rinfo.plugin_type) {
	  case F0R_PLUGIN_TYPE_SOURCE:
	  case F0R_PLUGIN_TYPE_FILTER:
	    if ((f0r_update=dlsym(handle,"f0r_update"))==NULL) {
	      (*f0r_deinit)();
	      dlclose(handle);
	      continue;
	    }
	    break;
	  case F0R_PLUGIN_TYPE_MIXER2:
	  case F0R_PLUGIN_TYPE_MIXER3:
	    if ((f0r_update2=dlsym(handle,"f0r_update2"))==NULL) {
	      (*f0r_deinit)();
	      dlclose(handle);
	      continue;
	    }
	    break;
	  default:
	    (*f0r_deinit)();
	    dlclose(handle);
	    continue;
	  }

	  if (f0rinfo.color_model==F0R_COLOR_MODEL_BGRA8888) {
	    pal=weed_malloc(2*sizeof(int));
	    pal[0]=WEED_PALETTE_BGRA8888;
	    pal[1]=WEED_PALETTE_END;
	  }
	  else if (f0rinfo.color_model==F0R_COLOR_MODEL_RGBA8888) {
	    pal=weed_malloc(2*sizeof(int));
	    pal[0]=WEED_PALETTE_RGBA8888;
	    pal[1]=WEED_PALETTE_END;
	  }
	  else if (f0rinfo.color_model==F0R_COLOR_MODEL_PACKED32) {
	    pal=weed_malloc(7*sizeof(int));
	    pal[0]=WEED_PALETTE_RGBA8888;
	    pal[1]=WEED_PALETTE_BGRA8888;
	    pal[2]=WEED_PALETTE_ARGB8888;
	    pal[3]=WEED_PALETTE_UYVY8888;
	    pal[4]=WEED_PALETTE_YUYV8888;
	    pal[5]=WEED_PALETTE_YUVA8888;
	    pal[6]=WEED_PALETTE_END;
	  }
	  else {
	    f0r_deinit();
	    dlclose(handle);
	    continue;
	  }

	  out_chantmpls=weed_malloc(2*sizeof(weed_plant_t *));
	  out_chantmpls[0]=weed_channel_template_init("out channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	  weed_set_int_value(out_chantmpls[0],"hstep",8);
	  weed_set_int_value(out_chantmpls[0],"vstep",8);
	  weed_set_int_value(out_chantmpls[0],"maxwidth",2048);
	  weed_set_int_value(out_chantmpls[0],"maxheight",2048);
	  weed_set_int_value(out_chantmpls[0],"alignment",16);
	  out_chantmpls[1]=NULL;
	  
	  switch (f0rinfo.plugin_type) {
	  case F0R_PLUGIN_TYPE_SOURCE:
	    in_chantmpls=NULL;
	    break;
	  case F0R_PLUGIN_TYPE_FILTER:
	    in_chantmpls=weed_malloc(2*sizeof(weed_plant_t *));
	    in_chantmpls[0]=weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[0],"hstep",8);
	    weed_set_int_value(in_chantmpls[0],"vstep",8);
	    weed_set_int_value(in_chantmpls[0],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[0],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[0],"alignment",16);
	    in_chantmpls[1]=NULL;
	    break;
	  case F0R_PLUGIN_TYPE_MIXER2:
	    in_chantmpls=weed_malloc(3*sizeof(weed_plant_t *));
	    in_chantmpls[0]=weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[0],"hstep",8);
	    weed_set_int_value(in_chantmpls[0],"vstep",8);
	    weed_set_int_value(in_chantmpls[0],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[0],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[0],"alignment",16);
	    
	    in_chantmpls[1]=weed_channel_template_init("in channel 1",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[1],"hstep",8);
	    weed_set_int_value(in_chantmpls[1],"vstep",8);
	    weed_set_int_value(in_chantmpls[1],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[1],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[1],"alignment",16);
	    in_chantmpls[2]=NULL;
	    break;
	  case F0R_PLUGIN_TYPE_MIXER3:
	    in_chantmpls=weed_malloc(4*sizeof(weed_plant_t *));
	    in_chantmpls[0]=weed_channel_template_init("in channel 0",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[0],"hstep",8);
	    weed_set_int_value(in_chantmpls[0],"vstep",8);
	    weed_set_int_value(in_chantmpls[0],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[0],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[0],"alignment",16);
	    
	    in_chantmpls[1]=weed_channel_template_init("in channel 1",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[1],"hstep",8);
	    weed_set_int_value(in_chantmpls[1],"vstep",8);
	    weed_set_int_value(in_chantmpls[1],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[1],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[1],"alignment",16);
	    
	    in_chantmpls[2]=weed_channel_template_init("in channel 2",WEED_CHANNEL_REINIT_ON_SIZE_CHANGE,pal);
	    weed_set_int_value(in_chantmpls[2],"hstep",8);
	    weed_set_int_value(in_chantmpls[2],"vstep",8);
	    weed_set_int_value(in_chantmpls[2],"maxwidth",2048);
	    weed_set_int_value(in_chantmpls[2],"maxheight",2048);
	    weed_set_int_value(in_chantmpls[2],"alignment",16);
	    in_chantmpls[3]=NULL;
	    break;
	  default:
	    (*f0r_deinit)();
	    dlclose(handle);
	    weed_free(pal);
	    weed_free(out_chantmpls);
	    if (in_chantmpls!=NULL) weed_free(in_chantmpls);
	    continue;
	  }
	  
	  num_weed_params=0;

	  if (f0rinfo.num_params>0) {
	    if ((f0r_set_param_value=dlsym(handle,"f0r_set_param_value"))==NULL) {
	      (*f0r_deinit)();
	      dlclose(handle);
	      weed_free(pal);
	      weed_free(out_chantmpls);
	      if (in_chantmpls!=NULL) weed_free(in_chantmpls);
	      continue;
	    }

	    for (pnum=0;pnum<f0rinfo.num_params;pnum++) {
	      num_weed_params++;
	      (*f0r_get_param_info)(&pinfo,pnum);
	      if (pinfo.type==F0R_PARAM_POSITION) num_weed_params++;
	    }

	    if (num_weed_params>f0rinfo.num_params) {
	      rfx_strings=weed_malloc((f0rinfo.num_params)*sizeof(char *));
	      for (pnum=0;pnum<f0rinfo.num_params;pnum++) {
		rfx_strings[pnum]=(char *)weed_malloc(256);
	      }
	    }
	    in_params=weed_malloc((num_weed_params+1)*sizeof(weed_plant_t *));

	    wnum=0;
	    for (pnum=0;pnum<f0rinfo.num_params;pnum++) {
	      (*f0r_get_param_info)(&pinfo,pnum);
	      label=malloc(strlen((char *)pinfo.name)+2);
	      sprintf(label,"_%s",(char *)pinfo.name);
	      switch (pinfo.type) {
	      case F0R_PARAM_BOOL:
		in_params[wnum]=weed_switch_init((char *)pinfo.name,label,0);
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		if (num_weed_params>f0rinfo.num_params) sprintf(rfx_strings[pnum],"layout|p%d|",wnum);
		break;
	      case F0R_PARAM_DOUBLE:
		in_params[wnum]=weed_float_init((char *)pinfo.name,label,0.,0.,1.);
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		if (num_weed_params>f0rinfo.num_params) sprintf(rfx_strings[pnum],"layout|p%d|",wnum);
		pgui=weed_parameter_template_get_gui(in_params[wnum]);
		weed_set_double_value(pgui,"step_size",.01);
		weed_set_int_value(pgui,"decimals",2);
		break;
	      case F0R_PARAM_COLOR:
		in_params[wnum]=weed_colRGBd_init((char *)pinfo.name,label,0.,0.,0.);
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		if (num_weed_params>f0rinfo.num_params) sprintf(rfx_strings[pnum],"layout|p%d|",wnum);
		break;
	      case F0R_PARAM_POSITION:
		in_params[wnum]=weed_float_init((char *)pinfo.name,label,0.,0.,1.);
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		weed_set_boolean_value(in_params[wnum],"plugin_is_position",WEED_TRUE);
		pgui=weed_parameter_template_get_gui(in_params[wnum]);
		weed_set_double_value(pgui,"step_size",.01);
		weed_set_int_value(pgui,"decimals",2);
		wnum++;
		in_params[wnum]=weed_float_init((char *)pinfo.name,"",0.,0.,1.);
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		sprintf(rfx_strings[pnum],"layout|p%d|\"X\"|fill|p%d|\"Y\"|fill|",wnum-1,wnum);
		pgui=weed_parameter_template_get_gui(in_params[wnum]);
		weed_set_double_value(pgui,"step_size",.01);
		weed_set_int_value(pgui,"decimals",2);
		break;
	      case F0R_PARAM_STRING:
		in_params[wnum]=weed_text_init((char *)pinfo.name,label,"Frei0r");
		weed_set_string_value(in_params[wnum],"description",(char *)pinfo.explanation);
		if (num_weed_params>f0rinfo.num_params) sprintf(rfx_strings[pnum],"layout|p%d|",wnum);
		break;
	      default:
		(*f0r_deinit)();
		dlclose(handle);
		weed_free(pal);
		weed_free(out_chantmpls);
		if (in_chantmpls!=NULL) weed_free(in_chantmpls);
		if (in_params!=NULL) weed_free(in_params);
		if (rfx_strings!=NULL) {
		  int j;
		  for (j=0;j<pnum;j++) free(rfx_strings[j]);
		  weed_free(rfx_strings);
		}
		continue;
	      }
	      free(label);
	      wnum++;
	    }
	    in_params[pnum]=NULL;
	  }

	  snprintf(weed_name,256,"Frei0r: %s",f0rinfo.name);
	  pversion=f0rinfo.major_version*1000+f0rinfo.minor_version;

	  filter_class=weed_filter_class_init(weed_name,(char *)f0rinfo.author,pversion,0,&frei0r_init,&frei0r_process,&frei0r_deinit,in_chantmpls,out_chantmpls,in_params,NULL);

	  if (num_weed_params>f0rinfo.num_params) {
	    gui=weed_filter_class_get_gui(filter_class);
	    weed_set_string_value(gui,"layout_scheme","RFX");
	    weed_set_string_value(gui,"rfx_delim","|");
	    weed_set_string_array(gui,"rfx_strings",f0rinfo.num_params,rfx_strings);
	    for (wnum=0;wnum<f0rinfo.num_params;wnum++) free(rfx_strings[wnum]);
	    weed_free(rfx_strings);
	    rfx_strings=NULL;
	  }

	  if (f0rinfo.explanation!=NULL) weed_set_string_value(filter_class,"description",(char *)f0rinfo.explanation);
	  num_filters++;
	  weed_free(pal);
	  weed_free(out_chantmpls);
	  if (in_chantmpls!=NULL) weed_free(in_chantmpls);
	  if (in_params!=NULL) weed_free(in_params);
	  in_params=NULL;
	  
	  weed_set_voidptr_value(filter_class,"plugin_f0r_construct",f0r_construct);
	  weed_set_voidptr_value(filter_class,"plugin_f0r_destruct",f0r_destruct);
	  if (f0rinfo.num_params>0) weed_set_voidptr_value(filter_class,"plugin_f0r_set_param_value",f0r_set_param_value);
	  weed_set_int_value(filter_class,"plugin_f0r_type",f0rinfo.plugin_type);

	  switch (f0rinfo.plugin_type) {
	  case F0R_PLUGIN_TYPE_SOURCE:
	  case F0R_PLUGIN_TYPE_FILTER:
	    weed_set_voidptr_value(filter_class,"plugin_f0r_update",f0r_update);
	    break;
	  default:
	    weed_set_voidptr_value(filter_class,"plugin_f0r_update2",f0r_update2);
	    break;
	  }

	  weed_plugin_info_add_filter_class (plugin_info,filter_class);
	  // end plugin
	}
	// end vendor dir
      } while (vdirent!=NULL);
      if (curdir!=NULL) closedir(curdir);
    }
    // end frei0r dirs
    if (curvdir!=NULL) closedir(curvdir);

    if (num_filters==0) return NULL;
    weed_set_int_value(plugin_info,"version",package_version);
  }

  return plugin_info;
}


