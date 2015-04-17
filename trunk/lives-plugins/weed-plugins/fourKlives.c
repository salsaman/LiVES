// fourKlives.c
// weed plugin to generate parametric audio
// (c) G. Finch (salsaman) 2012
//
// released under the GNU GPL 3 or later
// see file COPYING or www.gnu.org for details

#define DEBUG

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

/*
    The implementation of Syna functions

    - Marq + Antti
*/


// ported to LiVES by salsaman@gmail.com

//#define MONO

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include "syna.h"

#define MAX_TUNES 1024
#define MAX_TUNELEN 1024
#define TUNE_DIR "data/fourKlives/songs/"

static char *tunes[MAX_TUNES];

static void set_tempo(_sdata *sdata, int tempo) {
  sdata->update=sdata->new_update=6*sdata->global/(tempo*10/25);
  //        ekolen=update*3;
}

static void set_base_freq(_sdata *sdata, int freq) {
  //base_freq=(freq<<13)/BFREQ;
  sdata->base_freq=BFREQ+freq;
  if (sdata->base_freq<=0) sdata->base_freq=1;
}



static int syna_init(_sdata *sdata, int freq) {
  register int n,i;

  sdata->module=NULL;

  sdata->base_freq=BFREQ;

  sdata->maxtracks=0;

  for (n=0; n<WAVES; n++) {
    sdata->aalto[n]=NULL;
  }

  for (n=0; n<INSTR; n++) {
    sdata->echo[n]=NULL;
    sdata->instr[n]=NULL;
    sdata->pi[n]=0;
  }

  sdata->slen=freq/BFREQ;
  sdata->global=freq;
  sdata->counter=0;

  /* Generate lower octaves from 6 */
  for (n=4; n>=0; n--)
    for (i=0; i<12; i++)
      notei[n*12+i]=notei[(n+1)*12+i]/2;

  /* Generate the waveforms */
  for (n=0; n<WAVES; n++) {
    if (n!=KOHINA) {
      sdata->aalto[n]=weed_malloc(sdata->slen*sizeof(float));
      if (sdata->aalto[n]==NULL) {
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
      weed_memset(sdata->aalto[n],0,sdata->slen*sizeof(float));
    }
  }

  for (n=0; n<sdata->slen; n++) {
    sdata->aalto[KANTTI][n]=(n<sdata->slen/2)?-1.0:1.0;
    sdata->aalto[SINI][n]  =sin(n*2.0*M_PI/(float)sdata->slen);
    sdata->aalto[SAHA][n]  =-1.0+fmod(1.0+2.0*n/(float)sdata->slen,2.0);
  }

  /* Noise needs to be longer */
  sdata->aalto[KOHINA]=weed_malloc(sdata->global*sizeof(float));
  if (sdata->aalto[KOHINA]==NULL) {
    return WEED_ERROR_MEMORY_ALLOCATION;
  }

  for (n=0; n<sdata->global; n++)
    sdata->aalto[KOHINA][n]=(rand()%2000-1000)/(float)1000.0;

  /* Set starting values */
  for (n=0; n<INSTR; n++) {
    sdata->live_row[n]=0;
    sdata->new_live_row[n]=0;

    sdata->off[n]=-1;
    sdata->plus[n]=0;
    sdata->trak[n][0]=END;
    sdata->ti[n]=-1;
    sdata->eko[n]=0;
    sdata->pan[n]=(n&1)?128-64:128+64;
#ifdef MONO
    sdata->pan[n]=128;
#endif

    sdata->pola[n]=0;
    sdata->vol[n]=255;
    sdata->prev[n]=0;
    sdata->slide[n]=0;
  }
  return WEED_NO_ERROR;
}

#ifndef TINY
static int syna_load(_sdata *sdata, const char *tune) {
  FILE    *f;
  long    length;

  int retval;

  /* Read the file and call syna_get */
  f=fopen(tune,"rb");
  if (f==NULL) return WEED_ERROR_INIT_ERROR;
  fseek(f,0,SEEK_END);
  length=ftell(f);
  fseek(f,0,SEEK_SET);
  sdata->module=weed_malloc(length+1);
  retval=fread(sdata->module,1,length,f);
  sdata->module[length]=0; /* String ends in zero */
  fclose(f);

  if (retval<length) return WEED_ERROR_INIT_ERROR;

  return (syna_get(sdata));
}
#endif


static int syna_get(_sdata *sdata) {
  static char *rows[MAXR],*key,*tnp;
  int tmp,a,d,s,r,mod,sweep,wave,patt,track,note,wave_mod;

  register int i,j,n;

#ifndef TINY
  cleanup(sdata->module);  /* You better have a clean module in intros... */
#endif

  /* Extract rows */
  for (n=0; n<MAXR; n++) {
    rows[n]=strtok((n)?NULL:sdata->module,"\n");
    if (rows[n]==NULL)
      break;
  }

  /* Extract data from rows */
  for (i=0; i<n; i++) {
    if (rows[i][0]=='#')
      continue;

    key=strtok(rows[i],":");

    if (key) {
      if (!strcmp(key,"bpm")) {
        tmp=atoi(strtok(NULL,":"))*10/25;
        sdata->song_bpm=tmp;
        sdata->new_update=sdata->update=6*sdata->global/tmp;
        sdata->ekolen=sdata->update*3;
      }
      if (key[0]=='i') {
        /* Get instrument number and wave form */
        tmp=atoi(&key[1]);
        if (tmp<INSTR) {
          if (sdata->echo[tmp]!=NULL) weed_free(sdata->echo[tmp]);
          sdata->echo[tmp]=weed_malloc(sdata->ekolen*sizeof(int));
          if (sdata->echo[tmp]==NULL) {
            return WEED_ERROR_MEMORY_ALLOCATION;
          }
          weed_memset(sdata->echo[tmp],0,sdata->ekolen*sizeof(int));
        }

        tnp=strtok(NULL,",");

        wave=0;

        if (!strcmp(tnp,"kantti") || !strcmp(tnp,"square"))
          wave=0;
        else if (!strcmp(tnp,"sini") || !strcmp(tnp,"sin"))
          wave=1;
        else if (!strcmp(tnp,"saha") || !strcmp(tnp,"saw"))
          wave=2;
        else if (!strcmp(tnp,"kohina") || !strcmp(tnp,"noise"))
          wave=3;

        /* Get ADSR */
        a=atoi(strtok(NULL,","));
        d=atoi(strtok(NULL,","));
        s=atoi(strtok(NULL,","));
        r=atoi(strtok(NULL,","));

        if (tmp<INSTR) {
          sdata->len[tmp]=a+d+s+r+1;
          sdata->instr[tmp]=weed_malloc(sdata->len[tmp]*sizeof(int));
          if (sdata->echo[tmp]==NULL) {
            return WEED_ERROR_MEMORY_ALLOCATION;
          }
          weed_memset(sdata->instr[tmp],0,sdata->len[tmp]*sizeof(int));
        }

        mod=atoi(strtok(NULL,","));
        if ((tnp=strtok(NULL,",")))
          sweep=atoi(tnp);
        else
          sweep=0;
        if ((tnp=strtok(NULL,",")))
          if (tmp<INSTR) sdata->pan[tmp]=(float)atoi(tnp)*255/100;
        if ((tnp=strtok(NULL,",")))
          if (tmp<INSTR) sdata->pola[tmp]=atoi(tnp)*255/100;

        wave_mod=1;

        if ((tnp=strtok(NULL,","))) {
          if (!strcmp(tnp,"kantti") || !strcmp(tnp,"square"))
            wave_mod=0;
          else if (!strcmp(tnp,"sini") || !strcmp(tnp,"sin"))
            wave_mod=1;
          else if (!strcmp(tnp,"saha") || !strcmp(tnp,"saw"))
            wave_mod=2;
          else if (!strcmp(tnp,"kohina") || !strcmp(tnp,"noise"))
            wave_mod=3;
        }

        if (tmp<INSTR) adsr(sdata,a,d,s,r,mod,sweep,tmp,wave,wave_mod);
      }

      if (key[0]=='p') { /* Handle pattern */
        patt=atoi(&key[1]);
        j=0;
        while (1) {
          tnp=strtok(NULL,",");
          if (tnp!=NULL) {
            note=0;
            for (tmp=0; notes[tmp][0]!='0'; tmp++)
              if (!strcmp(notes[tmp],tnp))
                note=notei[tmp];
            sdata->ptn[patt][j]=note;
            if (note==VOL || note==SLIDE) {
              j++;
              sdata->ptn[patt][j]=atoi(strtok(NULL,","));
            }

            j++;
          } else {
            sdata->ptn[patt][j]=END;
            break;
          }
        }
      }

      if (key[0]=='t') { /* Handle track */
        track=atoi(&key[1]);
        j=0;
        while (1) {
          tnp=strtok(NULL,",");
          if (tnp!=NULL) {
            if (!strcmp(tnp,"loop"))
              sdata->trak[track][j]=LOOP;
            else {
              patt=atoi(&tnp[1]);
              sdata->trak[track][j]=patt;
            }
            if (j>sdata->maxtracks) sdata->maxtracks=j;
            j++;
          } else {
            sdata->trak[track][j]=END;
            break;
          }
        }
      }
    }
  }

  return WEED_NO_ERROR;
}

#if 0
// added by Antti Silvast for setting all live the rows
static void set_live_rows(_sdata *sdata, int *the_rows) {
  register int i;

  for (i=0; i<INSTR; i++) {
    sdata->new_live_row[i]=the_rows[i];
    //pi[i]=0;
  }
}
#endif

// added by Antti Silvast for setting just one live row
static void set_live_row(_sdata *sdata, int channel, int the_row) {
  sdata->new_live_row[channel]=the_row;
}

static void syna_play(_sdata *sdata, float *dest, int length, int channels, int interleave) {
  int note,li; // li is the "live i"
  int left,right,smp;
  int ceko,c1eko;

  register int i,n;

  ceko=sdata->counter%sdata->ekolen;

  for (n=0; n<length; n++,sdata->counter++) {
    c1eko=ceko+1;
    if (c1eko==sdata->ekolen)
      c1eko=0;

    /* New row */
    if (sdata->counter > sdata->update) {
      sdata->counter=0;
      sdata->update=sdata->new_update;
      for (i=1; sdata->trak[i][0]!=END; i++) {
        li=sdata->live_row[i];
        //li=i;
        if (li==END)
          continue;
        sdata->pi[i]++;

        if (li==-1 || sdata->ptn[sdata->trak[i][li]][sdata->pi[i]]==END) {
          li=sdata->live_row[i]=sdata->new_live_row[i];
          //ti[i]++;
          /*
            if(trak[i][li]==LOOP)
            li=0;
            if(trak[i][li]==END)
            li=END;
          */
          sdata->pi[i]=0;
        }
        //printf("%d=%d ",i,trak[i][li]);

        if (li!=END) {
          if ((note=sdata->ptn[sdata->trak[i][li]][sdata->pi[i]])) {
            switch (note) {
            case STOP  :
              sdata->off[i]=-1;
              break;
            case ECHON :
              sdata->eko[i]=1;
              break;
            case ECHOFF:
              sdata->eko[i]=0;
              break;
            case VOL   :
              sdata->pi[i]++;
              sdata->vol[i]=sdata->ptn[sdata->trak[i][li]][sdata->pi[i]]*255/100;
              break;
            case SLIDE :
              sdata->pi[i]++;
              sdata->slide[i]=sdata->ptn[sdata->trak[i][li]][sdata->pi[i]]*164/1000;
              break;
            default    :
              sdata->plus[i]=note<<13;
              //plus[i]/=BFREQ;
              sdata->off[i]=0;
            }
          }
        }
      }

      //printf("\n");

    }

#define MARQ_ARM_OPT

#ifndef MARQ_ARM_OPT
    /* Sum the instruments */
    left=right=0;
    for (i=1; sdata->trak[i][0]!=END; i++) {
      smp=sdata->echo[i][(sdata->counter+1)%sdata->ekolen];

      sdata->echo[i][sdata->counter%sdata->ekolen]=sdata->echo[i][(sdata->counter+1)%sdata->ekolen]*6/10;
      if (sdata->off[i]>=0) {
        smp+=sdata->instr[i][sdata->off[i]>>13];

        if (sdata->eko[i])
          sdata->echo[i][sdata->counter%sdata->ekolen]=smp*2/10;

        sdata->off[i]+=(sdata->plus[i]+sdata->base_speed);
        sdata->plus[i]+=sdata->slide[i];
        if ((sdata->off[i]>>13)>=sdata->len[i] || sdata->off[i]<0)
          sdata->off[i]=-1;
      }

      if (sdata->pola[i])
        smp=(smp*(255^sdata->pola[i])>>8)+(sdata->prev[i]*sdata->pola[i]>>8);
      sdata->prev[i]=smp;

      smp=smp*sdata->vol[i]>>8;
      left+=(255^sdata->pan[i])*smp>>8;
      right+=sdata->pan[i]*smp>>8;
    }

    if (left<-98301)
      left=-98301;
    if (left>98301)
      left=98301;
    if (!interleave||channels==1)
      dest[n]=(float)left/98301.;
    else
      dest[n*2]=(float)left/98301.;

    if (channels==2) {
      if (right<-98301)
        right=-98301;
      if (right>98301)
        right=98301;
      if (interleave) dest[n*2+1]=(float)right/98301.;
      else dest[n+length]=(float)right/98301.;
    }

#else
    // sum the instruments
    left=right=0;
    for (i=1; sdata->trak[i][0]!=END; i++) {

      smp=sdata->echo[i][c1eko];

      //fprintf(stderr,"ok %d\n",smp);

      sdata->echo[i][ceko]=smp*19>>5;

      if (sdata->off[i]>=0) {
        smp+=sdata->instr[i][sdata->off[i]>>13];
        //fprintf(stderr,"ok2 %d\n",smp);

        if (sdata->eko[i])
          sdata->echo[i][ceko]=smp*13>>6;

        sdata->off[i]+=(sdata->plus[i]/sdata->base_freq);
        sdata->plus[i]+=sdata->slide[i]*sdata->base_freq;
        if ((sdata->off[i]>>13)>=sdata->len[i] || sdata->off[i]<0)
          sdata->off[i]=-1;
      }

      if (sdata->pola[i])
        smp=(smp*(255^sdata->pola[i])>>8)+(sdata->prev[i]*sdata->pola[i]>>8);
      sdata->prev[i]=smp;
      //fprintf(stderr,"ok23 %d\n",smp);

      smp=smp*sdata->vol[i]>>8;
      //fprintf(stderr,"ok24 %d\n",smp);
      left+=(255^sdata->pan[i])*smp>>8;
      if (channels==2)
        right+=sdata->pan[i]*smp>>8;
    }

    if (left<-98301)
      left=-98301;
    else if (left>98301)
      left=98301;
    if (!interleave||channels==1)
      dest[n]=(float)(left*21>>6)/32767.;
    else {
      dest[n*2]=(float)(left*21>>6)/32767.;

    }

    if (channels==2) {
      if (right<-98301)
        right=-98301;
      else if (right>98301)
        right=98301;
      if (interleave) dest[n*2+1]=(float)(right*21>>6)/32767.;
      else dest[n+length]=(float)(right*21>>6)/32767.;
    }

    //fprintf(stderr,"vals %d and %d, %f and %f\n",left,right,(float)(left*21>>6)/32767.,(float)(right*21>>6)/32767.);

    ceko++;
    if (ceko==sdata->ekolen)
      ceko=0;

#endif


  }
}

/* Make ADSR to instruments */
static void adsr(_sdata *sdata, int a, int d, int s, int r, int mod, int swp, int ins, int wave, int wave_mod) {
  int n,modulo=sdata->slen,id=0;
  float  i=0,vol=0.0,dv,oh=0.0,op,ip=1,sweep;

  if (!a) a=1;
  if (!r) r=1;

  if (wave==KOHINA)
    modulo=sdata->global;

  if (mod) /* We modulate! */
    if (wave_mod==1)
      op=mod/100.0*2.0*M_PI/(float)sdata->slen;
    else
      op=(float)mod/100.0;
  else
    op=0;

  sweep=(float)swp/1000.0/(float)sdata->slen;

  dv=32767.0/(float)a;
  for (n=0; n<a; n++,i+=ip,ip+=sweep,vol+=dv,oh+=op)
    if (wave_mod!=1)
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sdata->aalto[wave_mod][((int)oh)%modulo]:1.0);
    else
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sin(oh):1.0);

  for (n=0; n<d; n++,i+=ip,ip+=sweep,vol-=dv,oh+=op)
    if (wave_mod!=1)
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sdata->aalto[wave_mod][((int)oh)%modulo]:1.0);
    else
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sin(oh):1.0);


  for (n=0; n<s; n++,i+=ip,ip+=sweep,oh+=op)
    if (wave_mod!=1)
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sdata->aalto[wave_mod][((int)oh)%modulo]:1.0);
    else
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sin(oh):1.0);

  dv=vol/(float)r;

  for (n=0; n<r; n++,i+=ip,ip+=sweep,vol-=dv,oh+=op)
    if (wave_mod!=1)
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sdata->aalto[wave_mod][((int)oh)%modulo]:1.0);
    else
      sdata->instr[ins][id++]=vol*sdata->aalto[wave][((int)i)%modulo]*((mod)?sin(oh):1.0);

}

/* Fix the fscking Windoze/DOS newlines */
#ifndef TINY
void cleanup(char *s) {
  char    *d=strdup(s);

  for (; *d; d++)
    if (*d!='\r' && *d!=' ')
      *s++=*d;
  *s=0;
}
#endif


static void syna_deinit(_sdata *sdata) {
  register int n;

  if (sdata==NULL) return;

  for (n=0; n<WAVES; n++) {
    if (sdata->aalto[n]!=NULL) weed_free(sdata->aalto[n]);
  }
  for (n=0; n<INSTR; n++) {
    if (sdata->echo[n]!=NULL) weed_free(sdata->echo[n]);
  }
  if (sdata->module!=NULL) weed_free(sdata->module);

  weed_free(sdata);
}


/////////////////////////////////////////////


int fourk_deinit(weed_plant_t *inst) {
  int error;
  _sdata *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);
  if (sdata!=NULL) syna_deinit(sdata);
  weed_set_voidptr_value(inst,"plugin_internal",NULL);
  return WEED_NO_ERROR;
}



int fourk_init(weed_plant_t *inst) {
  int error,retval;
  int rate;

  _sdata *sdata;

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);
  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  char tune[MAX_TUNELEN];

  snprintf(tune,MAX_TUNELEN-4,"%s%s",TUNE_DIR,tunes[weed_get_int_value(in_params[0],"value",&error)]);

  weed_free(in_params);

  sdata=(_sdata *)weed_malloc(sizeof(_sdata));

  if (sdata==NULL) return WEED_ERROR_MEMORY_ALLOCATION;

  weed_set_voidptr_value(inst,"plugin_internal",sdata);

  rate=weed_get_int_value(out_channel,"audio_rate",&error);

  if ((retval=syna_init(sdata,rate))!=WEED_NO_ERROR) {
#ifdef DEBUG
    fprintf(stderr,"4k init failed\n");
#endif
    fourk_deinit(inst);
    return retval;
  }

#ifdef DEBUG
  fprintf(stderr,"4k: loading tune %s\n",tune);
#endif

  if ((retval=syna_load(sdata,tune))!=WEED_NO_ERROR) {

    sprintf(tune+strlen(tune),"%s",".txt");

#ifdef DEBUG
    fprintf(stderr,"4k: loading tune %s\n",tune);
#endif

    if ((retval=syna_load(sdata,tune))!=WEED_NO_ERROR) {

      fourk_deinit(inst);
#ifdef DEBUG
      fprintf(stderr,"4k load failed\n");
#endif
      return retval;
    }
  }

  //set_tempo(sdata,136.); // bpm: maybe 8. to 263. ?
  //set_base_freq(sdata,272.); // 145. to 400.

  return WEED_NO_ERROR;
}





int fourk_process(weed_plant_t *inst, weed_timecode_t timestamp) {
  int error;
  int chans,nsamps,inter;

  weed_plant_t **in_params=weed_get_plantptr_array(inst,"in_parameters",&error);

  weed_plant_t *out_channel=weed_get_plantptr_value(inst,"out_channels",&error);

  float *dst=weed_get_voidptr_value(out_channel,"audio_data",&error);

  double tempo=weed_get_double_value(in_params[1],"value",&error);
  double bfreq=weed_get_double_value(in_params[2],"value",&error);

  _sdata *sdata=weed_get_voidptr_value(inst,"plugin_internal",&error);

  register int i;

  weed_free(in_params);

  chans=weed_get_int_value(out_channel,"audio_channels",&error);
  nsamps=weed_get_int_value(out_channel,"audio_data_length",&error);
  inter=weed_get_boolean_value(out_channel,"audio_interleaf",&error);
  //rate=weed_get_int_value(out_channel,"audio_rate",&error);

  for (i=0; i<NCHANNELS; i++) {
    set_live_row(sdata,i,(rand()%(sdata->maxtracks*1000-1))/1000.f+1.);  // 2nd val can be 2 (?) to npat (?)
  }

  set_tempo(sdata,tempo*255.+8.); // bpm: maybe 8. to 263. ?
  set_base_freq(sdata,bfreq*255.-128.); // 145. to 400.

  syna_play(sdata,dst,nsamps,chans,inter); // dlen is number of samps....does interleaved

  return WEED_NO_ERROR;
}




weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) {
  weed_plant_t *plugin_info;
  DIR *dir=NULL;
  struct dirent *dirent;
  size_t dlen;
  int tcount=0;

  // make list of tunes
  // scan entries in the ./data/fourklives/songs directory
  dir=opendir(TUNE_DIR);
  if (dir==NULL) return NULL;

  while (1) {
    if (tcount==MAX_TUNES-1) break;
    dirent=readdir(dir);
    if (dirent==NULL) break;
    dlen=strlen(dirent->d_name);
    if (!strncmp(dirent->d_name,"..",dlen)) continue;
    if (dlen>4 && !strcmp(dirent->d_name+dlen-4,".txt")) dlen-=4;
    tunes[tcount++]=strndup(dirent->d_name,dlen);
  }

  closedir(dir);

  tunes[tcount]=NULL;

  plugin_info=weed_plugin_info_init(weed_boot,num_versions,api_versions);

  if (plugin_info!=NULL) {
    weed_plant_t *out_chantmpls[2];
    weed_plant_t *in_params[NCHANNELS+4]; // tune name + channel rows + tempo + base_freq + NULL
    weed_plant_t *filter_class;


    register int i;

    in_params[0]=weed_string_list_init("tune_name","_Tune",0,(const char **const)tunes);
    weed_set_int_value(in_params[0],"flags",WEED_PARAMETER_REINIT_ON_VALUE_CHANGE);

    in_params[1]=weed_float_init("tempo","_Tempo",.5,0.,1.);
    in_params[2]=weed_float_init("bfreq","Base _Frequency",.5,0.,1.);


    for (i=3; i<NCHANNELS+3; i++) {
      // TODO - unique name
      in_params[i]=weed_float_init("cparam","cparam",.5,0.,1.);
    }

    in_params[i]=NULL;

    out_chantmpls[0]=weed_audio_channel_template_init("out channel 0",0);
    out_chantmpls[1]=NULL;

    filter_class=weed_filter_class_init("fourKlives","salsaman, anti and marq",1,0,&fourk_init,&fourk_process,
                                        &fourk_deinit,NULL,out_chantmpls,in_params,NULL);

    weed_plugin_info_add_filter_class(plugin_info,filter_class);

    weed_set_int_value(plugin_info,"version",package_version);
  }
  return plugin_info;
}




void weed_desetup(void) {
  register int i;
  for (i=0; tunes[i]!=NULL; i++) weed_free(tunes[i]);
}
