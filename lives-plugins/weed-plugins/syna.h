/*
    Definitions for Syna

    - Marq
*/

enum {  Nc1=0,NC1,Nd1,ND1, Ne1,Nf1,NF1,Ng1, NG1,Na1,NA1,Nb1,
        Nc2,  NC2,Nd2,ND2, Ne2,Nf2,NF2,Ng2, NG2,Na2,NA2,Nb2,
        Nc3,  NC3,Nd3,ND3, Ne3,Nf3,NF3,Ng3, NG3,Na3,NA3,Nb3,
        Nc4,  NC4,Nd4,ND4, Ne4,Nf4,NF4,Ng4, NG4,Na4,NA4,Nb4,
        Nc5,  NC5,Nd5,ND5, Ne5,Nf5,NF5,Ng5, NG5,Na5,NA5,Nb5,
        Nc6,  NC6,Nd6,ND6, Ne6,Nf6,NF6,Ng6, NG6,Na6,NA6,Nb6,
        NECHON, NECHOFF, NSTOP, NVOL, NSLIDE, NEMPTY /* Special "notes" */
     };

// the base frequency

#define BFREQ 262




// TODO - use enum
#define KANTTI 0
#define SINI   1
#define SAHA   2
#define KOHINA 3
#define WAVES 4 // must be KOHINA+1

#define INSTR 30
#define PTLEN 100
#define MAXR 1000

#define ECHON -4
#define ECHOFF -5
#define STOP -6
#define END -2
#define LOOP -3
#define VOL -7
#define SLIDE -8

#ifndef M_PI
#define M_PI 3.141592
#endif

#undef TINY

#define NCHANNELS 10


typedef struct {
  float   *aalto[WAVES];

  int  *instr[INSTR],*echo[INSTR],vol[INSTR],pola[INSTR],
       prev[INSTR],pan[INSTR],off[INSTR],plus[INSTR],slide[INSTR];

  // an addition for Syna Live -Antti
  int	live_row[INSTR];
  int	new_live_row[INSTR];

  int  global,slen,update,new_update,trak[INSTR][PTLEN],ptn[MAXR][PTLEN],
       ti[INSTR],pi[INSTR],len[INSTR],ekolen;

  char *module,eko[INSTR];

  int song_bpm;

  int base_freq;

  int counter;

  int maxtracks;
} _sdata;


static char *notes[]= {
  "c1","C1","d1","D1", "e1","f1","F1","g1", "G1","a1","A1","b1",
  "c2","C2","d2","D2", "e2","f2","F2","g2", "G2","a2","A2","b2",
  "c3","C3","d3","D3", "e3","f3","F3","g3", "G3","a3","A3","b3",
  "c4","C4","d4","D4", "e4","f4","F4","g4", "G4","a4","A4","b4",
  "c5","C5","d5","D5", "e5","f5","F5","g5", "G5","a5","A5","b5",
  "c6","C6","d6","D6", "e6","f6","F6","g6", "G6","a6","A6","b6",
  "echo1","echo0","stop","vol","sld","0"
};

static int notei[]= {
  0,0,0,0, 0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0, 0,0,0,0,
  0,0,0,0, 0,0,0,0, 0,0,0,0,
  1047,1109,1175,1245, 1319,1397,1480,1568, 1661,1760,1864,1976,
  ECHON,ECHOFF,STOP,VOL,SLIDE,0
};


static void adsr(_sdata *, int a, int d, int s, int r, int mod, int swp, int ins, int wave, int wave_mod);
static void cleanup(char *s);


/* Some variables that might be useful */
//extern int syna_counter,syna_row;

/* Init the whole shit. Instruments etc. Call this first of all.
   freq=main frequency */
static int syna_init(_sdata *, int freq);

#if 0
// Change all the live rows.
static void set_live_rows(_sdata *, int *the_rows);
#endif

// Change just one live row.
// input: channel. Should be larger than 1, maximum according to song's number of channels.
// input: the row. Should be larger than 0, maximum according to song's number of patterns.
static void set_live_row(_sdata *, int channel, int the_row);

// Alter the tempo realtime.
// input: tempo in bpm. Should be larger than 1.
static void set_tempo(_sdata *, int tempo);

// Alter the frequency realtime.
// input: freq relative to 262 Hz, so the actual base frequency is (262+freq). Should be larger than -262.
static void set_base_freq(_sdata *sdata, int freq);

#ifndef TINY
/* Loads a tune from a file. 0=OK, -1=error */
static int syna_load(_sdata *, const char *tune);
#endif

/* Gets a tune from memory */
static int syna_get(_sdata *);

/* Plays a fragment to destination */
static void syna_play(_sdata *sdata, float *dest, int length, int channels, int interleave);

/* Sets channel volume [0..1]. Channels start from 1 */
//static void syna_setvol(_sdata *, int chn, float nvol);

/* Sets BPM */
//static void syna_setbpm(_sdata *, int bpm);

/* Trigs channel with note (see enum above) */
//static void syna_trig(int chn,int note);

/* (Un)mutes channel 0=normal 1=mute, delay=0 - mute at once, delay=1 -
   mute when the next row starts */
//static void syna_mute(int chn,int nmute,int delay);

/* Transposes a channel. Negative tp transposes down, positive up. Not
   relative to the previous transposition! */
//static void syna_transpose(int chn,int tp);
