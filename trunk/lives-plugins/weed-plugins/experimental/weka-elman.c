// weka-elman.c
// weed plugin
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details


// Use weka to classify data using Elman


#ifdef HAVE_SYSTEM_WEED
#include <weed/weed.h>
#include <weed/weed-palettes.h>
#include <weed/weed-effects.h>
#else
#include "../../../libweed/weed.h"
#include "../../../libweed/weed-palettes.h"
#include "../../../libweed/weed-effects.h"
#endif

///////////////////////////////////////////////////////////////////

static int num_versions=1; // number of different weed api versions supported
static int api_versions[]= {131}; // array of weed api versions supported in plugin, in order of preference (most preferred first)

static int package_version=1; // version of this package

//////////////////////////////////////////////////////////////////

#ifdef HAVE_SYSTEM_WEED_PLUGIN_H
#include <weed/weed-plugin.h> // optional
#else
#include "../../../libweed/weed-plugin.h" // optional
#endif

#include "../weed-utils-code.c" // optional
#include "../weed-plugin-utils.c" // optional


/////////////////////////////////////////////////////////////////////



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "cj.h"


#define CHECK_RC(rc) assert((rc) == CJ_ERR_SUCCESS)

typedef enum {
  MODE_STORE,
  MODE_TRAIN,
  MODE_LIVE
} pmode_t;


static cjJVM_t jvm;
static cjClass_t proxyClass;
static cjObject_t proxy;

static pmode_t mode;
static char floatdata[8192];

/**
 * Calls CJWeka proxy via CJ facade
 */


#define NCLASSES 8 // number of distinct classes - this is currently hardcoded in CJWeka


static void make_floatdata(double *data, int ndata, int tclass) {
  register int i;
  char tmp[1024];

  memset(floatdata,0,1024);

  for (i=0; i<ndata; i++) {
    snprintf(tmp,1024,"%8f ",data[i]);
    strncat(floatdata,tmp,1024);
  }
  snprintf(tmp,1024,"%d",tclass);
  strncat(floatdata,tmp,1024);

}

static int parse_output(const char *sout) {
  double val,max=0;
  char *sptr=(char *)sout;

  int maxi=0;

  register int i;

  for (i=0; i<NCLASSES; i++) {
    val=strtod(sptr,&sptr);
    if (val>max) maxi=i;
  }

  return maxi;


}



int weka_init(weed_plant_t *inst) {
  int error,rc;
  char sout[1024];

  char *initdata="";
  char *savefile="";

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t *gui=weed_parameter_template_get_gui(in_params[2]);

  int train=weed_get_boolean_value(in_params[1],"value",&error);

  weed_free(in_params);

  memset(&sout, 0, 1024);

  rc=cjProxyStartString(&proxy,initdata,sout);

  if (mode==MODE_STORE && !train) {
    // train with data
    rc=cjProxyBuildModelString(&proxy,"",sout);
    rc=cjProxySaveModelString(&proxy,savefile,sout);
    rc=cjProxyResetModelString(&proxy,"",sout);
    mode=MODE_LIVE;
    rc=cjProxyLoadModelString(&proxy,savefile,sout);
    weed_set_boolean_value(gui,"hidden",WEED_TRUE); // hide Class in param
  } else if (mode==MODE_LIVE && train) {
    mode=MODE_STORE;
    weed_set_boolean_value(gui,"hidden",WEED_FALSE); // show Class in param
  } else if (mode==MODE_LIVE) {
    rc=cjProxyResetModelString(&proxy,"",sout);
  }

  return WEED_NO_ERROR;
}


int weka_deinit(weed_plant_t *inst) {
  int rc;

  char sout[1024];
  memset(&sout, 0, 1024);

  rc=cjProxyEndString(&proxy,"",sout);

  return WEED_NO_ERROR;
}




int weka_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error,rc;

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);
  weed_plant_t **out_params=weed_get_plantptr_array(inst,"out_parameters",&error);

  char sout[1024];

  char *savefile="";

  double *data=weed_get_double_array(in_params[0],"value",&error);
  int tclass;

  int ndata=weed_leaf_num_elements(in_params[0],"value");

  int mostlikely;

  memset(&sout, 0, 1024);

  switch (mode) {
  case MODE_STORE:
    tclass=weed_get_int_value(in_params[2],"value",&error);
    make_floatdata(data,ndata,tclass);
    rc=cjProxyAddInstanceString(&proxy,floatdata,sout);
    printf("N inst is %s, %s %d\n",sout,floatdata,tclass);
    break;

  case MODE_LIVE:
    make_floatdata(data,ndata,-1);
    rc=cjProxyRunModelString(&proxy,floatdata,sout);
    printf("Result was |%s|\n",sout);
    mostlikely=parse_output(sout);
    weed_set_int_value(out_params[0],"value",mostlikely);
    break;

  default:
    break;

  }

  weed_free(data);
  weed_free(in_params);
  weed_free(out_params);

  return 0;
}



weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);
  if (plugin_info!=NULL) {
    int rc;

    // set PATH to weka and org.pentaho.packagemanager here
    char *args[] = {"-Djava.class.path=./data:/home/gabriel/masters/jni/java/src/main/java:/home/gabriel/masters/jni/java/", "-Xms256m", "-Xmx512m"};


    weed_plant_t *in_params[]= {weed_float_init("data","_Data",0.,-1000000000.,1000000000.),weed_switch_init("train","_Training mode",WEED_TRUE),
                                weed_integer_init("class","_Class",0,0,NCLASSES-1),NULL
                               };

    weed_plant_t *out_params[]= {weed_out_param_integer_init("class",0,0,NCLASSES-1),NULL};

    weed_plant_t *filter_class=weed_filter_class_init("weka-elman","salsaman",1,0,&weka_init,&weka_process,&weka_deinit,NULL,NULL,in_params,
                               out_params);

    weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_VARIABLE_ELEMENTS);
    weed_set_int_value(in_params[1],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);


    //setenv("LD_LIBRARY_PATH","$LD_LIBRARY_PATH:$HOME/src/jdk1.7.0_04/jre/lib/amd64/server",1); // path to libjvm.so

    memset(&jvm, 0, sizeof(cjJVM_t));
    memset(&proxyClass, 0, sizeof(cjClass_t));
    memset(&proxy, 0, sizeof(cjObject_t));

    jvm.argc = 3;
    jvm.argv = args;
    rc = cjJVMConnect(&jvm);
    if (rc!=CJ_ERR_SUCCESS) return NULL;

    rc = cjProxyClassCreate(&proxyClass, "CJWeka", &jvm);
    if (rc!=CJ_ERR_SUCCESS) return NULL;

    proxy.clazz = &proxyClass;
    rc = cjProxyCreate(&proxy);
    if (rc!=CJ_ERR_SUCCESS) return NULL;

    weed_plugin_info_add_filter_class(plugin_info,filter_class);
    weed_set_int_value(plugin_info,"version",package_version);

    mode=MODE_STORE;

  }
  return plugin_info;
}



void weed_desetup(void) {
  int rc;
  rc = cjFreeObject((proxy.clazz)->jvm, proxy.object);
  rc = cjClassDestroy(&proxyClass);
  rc = cjJVMDisconnect(&jvm);
}

