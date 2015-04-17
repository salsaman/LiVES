// kaleidoscope.c
// weed plugin
// (c) G. Finch (salsaman) 2013
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


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

static int num_versions=2; // number of different weed api versions supported
static int api_versions[]= {131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

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

#include <math.h>


#define FIVE_PI3 5.23598775598f
#define FOUR_PI3 4.18879020479f

#define THREE_PI2 4.71238898038f

#define TWO_PI 6.28318530718f
#define TWO_PI3 2.09439510239f

#define ONE_PI2 1.57079632679f
#define ONE_PI3 1.0471975512f

#define RT3  1.73205080757f //sqrt(3)
#define RT32 0.86602540378f //sqrt(3)/2

#define RT322 0.43301270189f

typedef struct {
  float angle;
  weed_timecode_t old_tc;
  int revrot;
  int owidth;
  int oheight;
} sdata;

static void calc_center(float side, float j, float i, float *x, float *y) {
  // find nearest hex center
  int gridx,gridy;

  float secx,secy;

  float sidex=side*RT3; // 2 * side * cos(30)
  float sidey=side*1.5; // side + sin(30)

  float hsidex=sidex/2.,hsidey=sidey/2.;

  i-=side/5.3;

  if (i>0.) i+=hsidey;
  else i-=hsidey;
  if (j>0.) j+=hsidex;
  else j-=hsidex;


  // find the square first
  gridy=i/sidey;
  gridx=j/sidex;

  // center
  *y=gridy*sidey;
  *x=gridx*sidex;

  secy=i-*y;
  secx=j-*x;

  if (secy<0.) secy+=sidey;
  if (secx<0.) secx+=sidex;

  if (!(gridy&1)) {

    // even row (inverted Y)
    if (secy>(sidey-(hsidex-secx)*RT322)) {
      *y+=sidey;
      *x-=hsidex;
    } else if (secy>sidey-(secx-hsidex)*RT322) {
      *y+=sidey;
      *x+=hsidex;
    }
  }


  else {
    // odd row, center is left or right (Y)
    if (secx<=hsidex) {
      if (secy<(sidey-secx*RT322)) {
        *x-=hsidex;
      } else *y+=sidey;
    } else {
      if (secy<sidey-(sidex-secx)*RT322) {
        *x+=hsidex;
      } else *y+=sidey;
    }
  }
}



static float calc_angle(float y, float x) {
  if (x>0.) {
    if (y>=0.) return atanf(y/x);
    return TWO_PI+atanf(y/x);
  }
  if (x<-0.) {
    return atanf(y/x)+M_PI;
  }
  if (y>0.) return ONE_PI2;
  return THREE_PI2;
}


static float calc_dist(float x, float y) {
  return sqrtf((x*x+y*y));
}


static void rotate(float r, float theta, float angle, float *x, float *y) {
  theta+=angle;
  if (theta<0.) theta+=TWO_PI;
  else if (theta>=TWO_PI) theta-=TWO_PI;

  *x=r*cos(theta);
  *y=r*sin(theta);
}



static int put_pixel(void *src, void *dst, int psize, float angle, float theta, float r, int irowstride, int hheight, int hwidth) {
  // dest point is at i,j; r tells us which point to copy, and theta related to angle gives us the transform

  // return 0 if src is oob

  float adif=theta-angle;
  float stheta;

  int sx,sy;

  if (adif<0.) adif+=TWO_PI;
  else if (adif>=TWO_PI) adif-=TWO_PI;

  theta-=angle;

  if (theta<0.) theta+=TWO_PI;
  else if (theta>TWO_PI) theta-=TWO_PI;

  if (adif < ONE_PI3) {
    stheta=theta;
  }

  else if (adif < TWO_PI3) {
    // get coords of src point
    stheta=TWO_PI3-theta;
  }


  else if (adif < M_PI) {
    // get coords of src point
    stheta=theta-TWO_PI3;
  }

  else if (adif < FOUR_PI3) {
    // get coords of src point
    stheta=FOUR_PI3-theta;
  }

  else if (adif < FIVE_PI3) {
    // get coords of src point
    stheta=theta-FOUR_PI3;
  } else {
    // get coords of src point
    stheta=TWO_PI-theta;
  }

  stheta+=angle;

  sx=r*cos(stheta)+.5;
  sy=r*sin(stheta)+.5;

  if (sy<-hheight||sy>=hheight||sx<-hwidth||sx>=hwidth) {
    return 0;
  }

  weed_memcpy(dst,src-sy*irowstride+sx*psize,psize);
  return 1;
}



int kal_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",
                           &error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  sdata *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  float theta,r,xangle;

  float x,y,a,b;

  float side,fi,fj;

  float anglerot=0.;
  double dtime,sfac,angleoffs;

  int width=weed_get_int_value(in_channel,"width",&error),hwidth=width>>1;
  int height=weed_get_int_value(in_channel,"height",&error),hheight=height>>1;
  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=4;

  int sizerev;

  int start,end;

  int upd=1;

  register int i,j;

  if (width<height) side=width/2./RT32;
  else side=height/2.;

  sfac=log(weed_get_double_value(in_params[0],"value",&error))/2.;

  angleoffs=weed_get_double_value(in_params[1],"value",&error);

  if (sdata->old_tc!=0&&timestamp>sdata->old_tc) {
    anglerot=(float)weed_get_double_value(in_params[2],"value",&error);
    dtime=(double)(timestamp-sdata->old_tc)/100000000.;
    anglerot*=(float)dtime;
    while (anglerot>=TWO_PI) anglerot-=TWO_PI;
  }

  if (weed_get_boolean_value(in_params[4],"value",&error)==WEED_TRUE) anglerot=-anglerot;

  sizerev=weed_get_boolean_value(in_params[5],"value",&error);

  weed_free(in_params);

  xangle=sdata->angle+(float)angleoffs/360.*TWO_PI;
  if (xangle>=TWO_PI) xangle-=TWO_PI;
  sdata->old_tc=timestamp;

  if (sdata->owidth!=width||sdata->oheight!=height) {
    if (sizerev&&sdata->owidth!=0&&sdata->oheight!=0) sdata->revrot=1-sdata->revrot;
    sdata->owidth=width;
    sdata->oheight=height;
  }

  if (sdata->revrot) anglerot=-anglerot;

  side*=(float)sfac;

  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) psize=3;

  src+=hheight*irowstride+hwidth*psize;

  start=hheight;
  end=-hheight;

  // new threading arch
  if (weed_plant_has_leaf(out_channel,"offset")) {
    int offset=weed_get_int_value(out_channel,"offset",&error);
    int dheight=weed_get_int_value(out_channel,"height",&error);

    if (offset>0) upd=0;

    start-=offset;
    dst+=offset*orowstride;
    end=start-dheight;
  }

  orowstride-=psize*(hwidth<<1);

  for (i=start; i>end; i--) {
    for (j=-hwidth; j<hwidth; j++) {
      // rotate point to line up with hex grid
      theta=calc_angle((fi=(float)i),(fj=(float)j)); // get angle of this point from origin
      r=calc_dist(fi,fj); // get dist of point from origin
      rotate(r,theta,-xangle+ONE_PI2,&a,&b); // since our central hex has rotated by angle, so has the hex grid - so compensate

      // find hex center and angle to it
      calc_center(side,a,b,&x,&y);

      // rotate hex center
      theta=calc_angle(y,x);
      r=calc_dist(x,y);
      rotate(r,theta,xangle-ONE_PI2,&a,&b);

      theta=calc_angle(fi-b,fj-a);
      r=calc_dist(b-fi,a-fj);

      if (r<10.) r=10.;

      if (!put_pixel(src,dst,psize,xangle,theta,r,irowstride,hheight,hwidth)) {
        if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) {
          weed_memset(dst,0,3);
        } else if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_BGRA32) {
          weed_memset(dst,0,3);
          dst[3]=255;
        } else if (palette==WEED_PALETTE_ARGB32) {
          weed_memset(dst+1,0,3);
          dst[0]=255;
        }
      }

      dst+=psize;
    }
    dst+=orowstride;
  }

  if (upd) {
    sdata->angle+=anglerot*TWO_PI;
    if (sdata->angle>=TWO_PI) sdata->angle-=TWO_PI;
    else if (sdata->angle<0.) sdata->angle+=TWO_PI;
  }

  return WEED_NO_ERROR;
}



int kal_init(weed_plant_t *inst) {
  sdata *sd=(sdata *)weed_malloc(sizeof(sdata));
  if (sd==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  sd->angle=0.;
  sd->old_tc=0;
  sd->revrot=0;
  sd->owidth=sd->oheight=0;

  weed_set_voidptr_value(inst,"plugin_internal",sd);

  return WEED_NO_ERROR;


}


int kal_deinit(weed_plant_t *inst) {
  int error;
  sdata *sd=weed_get_voidptr_value(inst,"plugin_internal",&error);

  weed_free(sd);

  return WEED_NO_ERROR;
}





weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]= {WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]= {weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]= {weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *in_params[]= {			       weed_float_init("szlen","_Size (log)",5.62,1.,10.),
                                             weed_float_init("offset","_Offset angle",0.,0.,359.),
                                             weed_float_init("rotsec","_Rotations per second",0.2,0.,4.),
                                             weed_radio_init("acw","_Anti-clockwise",WEED_TRUE,1),
                                             weed_radio_init("cw","_Clockwise",WEED_FALSE,1),
                                             weed_switch_init("szc","_Switch direction on frame size change",WEED_FALSE),
                                             NULL
                               };

    weed_plant_t *filter_class=weed_filter_class_init("kaleidoscope","salsaman",1,WEED_FILTER_HINT_MAY_THREAD,
                               &kal_init,&kal_process,&kal_deinit,in_chantmpls,out_chantmpls,in_params,NULL);

    weed_plant_t *gui=weed_parameter_template_get_gui(in_params[2]);

    weed_set_boolean_value(in_params[1],"wrap",WEED_TRUE);

    weed_set_double_value(gui,"step_size",.1);

    gui=weed_parameter_template_get_gui(in_params[0]);

    weed_set_double_value(gui,"step_size",.1);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }

  return plugin_info;
}

