// comic.c
// weed plugin
// (c) G. Finch (salsaman) 2010
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

// thanks to Chris Yates for the idea

#include "../../libweed/weed.h"
#include "../../libweed/weed-effects.h"
#include "../../libweed/weed-plugin.h"

#include <inttypes.h>

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]={131,100}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#include "../../libweed/weed-utils.h" // optional
#include "../../libweed/weed-plugin-utils.h" // optional

/////////////////////////////////////////////////////////////

static uint32_t sqrti (uint32_t n) {
  register uint32_t root, remainder, place;

  root = 0;
  remainder = n;
  place = 0x40000000;

  while (place > remainder) place = place >> 2;
  while (place) {
    if (remainder >= root + place) {
      remainder = remainder - root - place;
      root = root + (place << 1);
    }
    root = root >> 1;
    place = place >> 2;
  }
  return root;
}



static void cp_chroma (unsigned char *dst, unsigned char *src, int irowstride, int orowstride, int width, int height) {

  if (irowstride==orowstride&&irowstride==width) weed_memcpy(dst,src,width*height);
  else {
    register int i;
    for (i=0;i<height;i++) {
      weed_memcpy(dst,src,width);
      src+=irowstride;
      dst+=orowstride;
    }
  }
}
  


int comic_process (weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  weed_plant_t *in_channel=weed_get_plantptr_value(inst,"in_channels",&error),*out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  uint8_t **srcp=(uint8_t **)weed_get_voidptr_array(in_channel,"pixel_data",&error);
  uint8_t **dstp=(uint8_t **)weed_get_voidptr_array(out_channel,"pixel_data",&error);
  int width=weed_get_int_value(in_channel,"width",&error);
  int height=weed_get_int_value(in_channel,"height",&error);
  int *irowstrides=weed_get_int_array(in_channel,"rowstrides",&error);
  int *orowstrides=weed_get_int_array(out_channel,"rowstrides",&error);
  int palette=weed_get_int_value(in_channel,"current_palette",&error);
  int clamping=weed_get_int_value(in_channel,"YUV_clamping",&error);
  int irowstride,orowstride;
  unsigned char *src,*dst,*end;
  int row0,row1,sum,scale=384,mix=192;
  int yinv,ymin,ymax,nplanes;
  register int i;

  // get the Y planes
  src=srcp[0];
  dst=dstp[0];

  irowstride=irowstrides[0];
  orowstride=orowstrides[0];

  weed_memcpy(dst,src,width);

  src+=irowstride;
  dst+=orowstride;

  end=src+(height-2)*irowstride;

  orowstride-=width;

  if (clamping==WEED_YUV_CLAMPING_UNCLAMPED) {
    yinv=255;
    ymin=0;
    ymax=255;
  }
  else {
    yinv=251;
    ymin=16;
    ymax=235;
  }

  width--;

  for (;src<end;src+=(irowstride-width-1)) {

    *(dst++)=*src;
    src++;

    for (i=1;i<width;i++) {

      // do edge detect and convolve
      row0=( *(src+irowstride-1) - *(src-irowstride-1) ) + ( ( *(src+irowstride) - *(src-irowstride) ) << 1 ) + ( *(src+irowstride+1) - *(src+irowstride-1) );
      row1=( *(src-irowstride+1) - *(src-irowstride-1) ) + ( ( *(src+1) - *(src-1) ) << 1) + ( *(src+irowstride+1) + *(src+irowstride-1) );
      
      sum= ( ( 3 * sqrti( row0 * row0 * row1 *row1 ) / 2 ) * scale ) >>8;

      // clamp and invert
      sum=yinv-sum<ymin?ymin:sum>ymax?ymax:sum;

      // mix 25% effected with 75% original
      sum=((256-mix)*sum+mix*(*src));

      *(dst++)=(unsigned char)sum;
      src++;
    }
    *(dst++)=*src;
    src++;
    dst+=orowstride;
  }

  width++;

  weed_memcpy(dst,src,width);

  if (palette==WEED_PALETTE_YUV420P||palette==WEED_PALETTE_YVU420P) height>>=1;
  if (palette==WEED_PALETTE_YUV420P||palette==WEED_PALETTE_YVU420P||palette==WEED_PALETTE_YUV422P) width>>=1;

  if (palette==WEED_PALETTE_YUVA4444P) nplanes=4;
  else nplanes=3;

  for (i=1;i<nplanes;i++) {
    cp_chroma(dstp[i],srcp[i],irowstrides[i],orowstrides[i],width,height);
  }

  weed_free(srcp);
  weed_free(dstp);
  weed_free(irowstrides);
  weed_free(orowstrides);

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup (weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int palette_list[]={WEED_PALETTE_YUV444P,WEED_PALETTE_YUVA4444P,WEED_PALETTE_YUV422P,WEED_PALETTE_YUV420P,WEED_PALETTE_YVU420P,WEED_PALETTE_END};

    weed_plant_t *in_chantmpls[]={weed_channel_template_init("in channel 0",0,palette_list),NULL};
    weed_plant_t *out_chantmpls[]={weed_channel_template_init("out channel 0",0,palette_list),NULL};
    weed_plant_t *filter_class=weed_filter_class_init("comic","salsaman",1,0,NULL,&comic_process,NULL,in_chantmpls,out_chantmpls,NULL,NULL);

    // set preference of unclamped
    weed_set_int_value(in_chantmpls[0],"YUV_clamping",WEED_YUV_CLAMPING_UNCLAMPED);

    weed_plugin_info_add_filter_class (plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}

