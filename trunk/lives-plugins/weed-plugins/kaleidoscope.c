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
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_UTILS
#include <weed/weed-plugin.h> // optional
#else
#include "../../libweed/weed-plugin.h" // optional
#endif

#include "weed-utils-code.c" // optional
#include "weed-plugin-utils.c" // optional

/////////////////////////////////////////////////////////////

#include <math.h>

#define ABS(a) (a>0?a:-a)

#define SEVEN_PI6 3.66519142919f

#define FIVE_PI3 5.23598775598f
#define FIVE_PI6 2.61799387799f

#define FOUR_PI3 4.18879020479f

#define THREE_PI2 4.71238898038f

#define TWO_PI 6.28318530718f
#define TWO_PI3 2.09439510239f

#define ONE_PI2 1.57079632679f
#define ONE_PI3 1.0471975512f
#define ONE_PI6 0.52359877559f

#define RT32 0.86602540378f //sqrt(3)/2


static float angle;

static void calc_center(int side, int i, int j, int *x, int *y) {
  // find nearest hex center
  int gridx,gridy,secx,secy;

  int sidex=side*RT32*2.; // 2 * side * cos(30)
  int sidey=side*3./2.; // side + sin(30)

  float m=2./RT32;

  i-=side/2.;

  // find the square first
  if (i>0) gridy=(i+sidey/2)/sidey;
  else gridy=(i-sidey/2)/sidey;
  if (j>=0) gridx=(j+sidex/2)/sidex;
  else gridx=(j-sidex/2)/sidex;

  // center 
  *y=gridy*sidey;
  *x=gridx*sidex;


  secy=i-(i/sidey)*sidey;
  secx=j-(j/sidex)*sidex;

  if (secy<0) secy+=sidey;
  if (secx<0) secx+=sidex;

  if (!(gridy%2)) {
    // even row (inverted Y)
    if (secy>side+secx*m) {
      // *y+=sidey;
      //*x-=sidex/2.;
    }
    if (secy>sidey-(secx-sidex/2)*m) {
      //*y+=sidey;
      //*x+=sidex/2.;
    }
  }
  

  else {
    // odd row, center is left or right (Y)
    if (secx<=sidex/2.) {
      if (secy>sidey/2||secy<side+(secx-sidex/2)*m/2.) {
	*x-=sidex/2.;
      }
      else *y+=sidey;
    }
    else {
      if (secy>sidey/2||secy<((sidex-secx)*m/2.7)) {
	*x+=sidex/2.;
      }
      else *y+=sidey;
    }
  }
}



static float calc_angle(int y, int x) {
  if (x>0) {
    if (y>=0) return atanf((float)y/(float)x);
    return TWO_PI+atanf((float)y/(float)x);
  }
  if (x<0) {
    return atanf((float)y/(float)x)+M_PI;
  }
  if (y>0) return ONE_PI2;
  return -ONE_PI2;
}


static float calc_dist(int x, int y) {
  return sqrtf((float)(x*x+y*y));
}


static void rotate(float r, float theta, float angle, int *x, int *y) {
  theta+=angle;
  if (theta<0) theta+=TWO_PI;
  if (theta>TWO_PI) theta-=TWO_PI;

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
  }
  else {
    // get coords of src point
    stheta=TWO_PI-theta;
  }

  stheta+=angle;
  sx=r*cos(stheta)+.5;
  sy=r*sin(stheta)+.5;

  /*  if (sy<-hheight) sy=-hheight+ABS(sy%hheight);
  if (sy>=hheight) sy=hheight-ABS(sy%hheight);
  if (sx<-hwidth) sx=-hwidth+ABS(sx%hwidth);
  if (sx>=hwidth) sx=hwidth-ABS(sx%hwidth);
  */

  if (sy<-hheight||sy>=hheight||sx<-hwidth||sx>=hwidth) {
    return 0;
  }

  weed_memcpy(dst,src-sy*irowstride+sx*psize,psize);
  return 1;
}



int kal_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  //weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  unsigned char *src=weed_get_voidptr_value(in_channel,"pixel_data",&error);
  unsigned char *dst=weed_get_voidptr_value(out_channel,"pixel_data",&error);

  float theta,r;

  int x,y,a,b;

  int width=weed_get_int_value(in_channel,"width",&error),hwidth=width>>1;
  int height=weed_get_int_value(in_channel,"height",&error),hheight=height>>1;
  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  int irowstride=weed_get_int_value(in_channel,"rowstrides",&error);
  int orowstride=weed_get_int_value(out_channel,"rowstrides",&error);
  int psize=4;

  int minm,side;

  register int i,j;

  if (width<height) minm=width;
  else minm=height;

  minm>>=1;

  side=minm;

  if (width<height) side=(float)minm/RT32; // hex width should fit screen

  //side>>=2;

  //angle=weed_get_double_value(in_params[0],"value",&error);
  //weed_free(in_params);

  if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) psize=3;

  src+=hheight*irowstride+hwidth*psize;

  orowstride-=psize*width;

  for (i=hheight;i>-hheight;i--) {
    for (j=-hwidth;j<hwidth;j++) {
      // rotate point to line up with hex grid
      theta=calc_angle(i,j); // get angle of this point from origin
      r=calc_dist(i,j); // get dist of point from origin
      rotate(r,theta,-angle,&a,&b); // since our central hex has rotated by angle, so has the hex grid - so compensate

      // find hex center and angle to it
      calc_center(side,a,b,&x,&y);

      // rotate hex center
      theta=calc_angle(x,y);
      r=calc_dist(x,y);
      rotate(r,theta,angle,&a,&b);


      //s      theta=calc_angle(i-y,j-x);
      //s r=calc_dist(x-j,y-i);

      theta=calc_angle(i-b,j-a);
      r=calc_dist(b-i,a-j);

      if (!put_pixel(src,dst,psize,angle,theta,r,irowstride,hheight,hwidth)) {
	if (palette==WEED_PALETTE_RGB24||palette==WEED_PALETTE_BGR24) {
	  weed_memset(dst,0,3);
	}
	else if (palette==WEED_PALETTE_RGBA32||palette==WEED_PALETTE_BGRA32) {
	  weed_memset(dst,0,3);
	  dst[3]=255;
	}
	else if (palette==WEED_PALETTE_ARGB32) {
	  weed_memset(dst+1,0,3);
	  dst[0]=255;
	}
      }

      dst+=psize;
    }
    dst+=orowstride;
  }

  angle+=0.01;
  if (angle>=TWO_PI) angle=0.;

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_BGR24,WEED_PALETTE_RGB24,WEED_PALETTE_RGBA32,WEED_PALETTE_BGRA32,WEED_PALETTE_ARGB32,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",0,palette_list),NULL};
    //weed_plant_t *in_params[]={weed_switch_init("enabled","_Enabled",WEED_TRUE),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("kaleidoscope","salsaman",1,0,NULL,&kal_process,NULL,in_chantmpls,out_chantmpls,NULL,NULL);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }

  angle=0.;

  return plugin_info;
}

