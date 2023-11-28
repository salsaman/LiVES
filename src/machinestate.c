// machinestate.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

// functions for dealing with externalities

/// TODO - create an active "hardware manager" object. It should constantly monitor the system
// environment in real time, and be able to receive performance data from other parts of the code
// It should handle autotuning and configure things like like thread resources, memory usage
// disk buffering to maximise performance without overloading the system
//
// It should also manage memory dynamically, for example refreshing smallblocks
// seeing if we need to allocate more memory or if we can free some
// It can also keep track of cpu load, and expose this to other threads, such as the player
// monitor disk io, and in active mode. take over the role of diskspace monitoring.
// In addition in could measure things like cpu temperature, cpu frequency and respond accordingly.
// It should also manage the adaptive quality settings in the background.
// A lot of that is already here but needs combining and running in the bacjground as well
// as communicating with threads. The lives_object / attributes model should help with this.


//   #include <sys/resource.h> // needed for getrusage
//       struct rusage usage;   #include <sys/time.h> // needed for getrusage

//getrusage(RUSAGE_SELF, &usage);

// getrlimit RLIMIT_MEMLOCK, RLIMIT_AS, RLIMIT_NICE. RLIMIT_NPROC, RLIMIT_RTPRIO, RLIMIT_STACK
//

#include "main.h"
#include "callbacks.h"
#include "startup.h"
#include "nodemodel.h"
#include "diagnostics.h"
#include "nodemodel.h"

LIVES_LOCAL_INLINE char *mini_popen(char *cmd);

#if IS_X86_64

#define LIVES_REG_RBX  "rbx"
#define LIVES_REG_RSI  "rsi"
#define LIVES_REG_EAX  regs[0]
#define LIVES_REG_EBX  regs[1]
#define LIVES_REG_ECX  regs[2]
#define LIVES_REG_EDX  regs[3]

//////////////#define cpuid(index, eax, ebx, ecx, edx)

#if defined(_MSC_VER)
/* Microsoft C/C++-compatible compiler */
#include <intrin.h>
// SSE SIMD intrinsics
#include <xmmintrin.h>
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
/* GCC-compatible compiler, targeting x86/x86-64 */
// AVX SIMD intrinsics included
#include <x86intrin.h>
// SSE SIMD intrinsics
#include <xmmintrin.h>
#elif defined(__GNUC__) && defined(__ARM_NEON__)
/* GCC-compatible compiler, targeting ARM with NEON */
#include <arm_neon.h>
#elif defined(__GNUC__) && defined(__IWMMXT__)
/* GCC-compatible compiler, targeting ARM with WMMX */
#include <mmintrin.h>
#endif

#define cpuid(index, regs)						\
  __asm__ volatile (							\
		    "mov    %%"LIVES_REG_RBX", %%"LIVES_REG_RSI" \n\t"	\
		    "cpuid                       \n\t"			\
		    "xchg   %%"LIVES_REG_RBX", %%"LIVES_REG_RSI		\
		    : "=a" (LIVES_REG_EAX), "=S" (LIVES_REG_EBX), "=c" (LIVES_REG_ECX), "=d" (LIVES_REG_EDX) \
		    : "0" (index), "2"(0))

static void get_cpuinfo(void) {
  union {uint i[4]; char c[16];} vendor;
  uint regs[4]; // eax, ebx, ecx, edx
  char *vendstr;

  vendor.i[0] = 0; // stop gcc from complaining
  cpuid(0x00000000, vendor.i); // max_level, vendor0, vendor2, vendor1
  capable->hw.cpu_maxlvl = vendor.i[0];

  if (!capable->hw.cpu_maxlvl) return;

  capable->hw.cpu_vendor = lives_strdup_printf("%.4s%.4s%.4s", &vendor.c[4], &vendor.c[12], &vendor.c[8]);
  vendstr = lives_string_tolower(capable->hw.cpu_vendor);
  if (strstr(vendstr, CPU_VENDOR_INTEL)) capable->hw.cpu_type = CPU_TYPE_INTEL;
  else if (strstr(vendstr, CPU_VENDOR_AMD)) capable->hw.cpu_type = CPU_TYPE_AMD;
  else capable->hw.cpu_type = CPU_TYPE_OTHER;
  lives_free(vendstr);

  cpuid(0x00000001, regs);
  capable->hw.cacheline_size = ((LIVES_REG_EBX >> 8) & 0xFF) << 3;

  if (LIVES_REG_ECX & (1 << 27)) capable->hw.cpu_features |= CPU_FEATURE_HAS_SSE;
  if (LIVES_REG_EDX & (1 << 30)) capable->hw.cpu_features |= CPU_FEATURE_HAS_SSE2;
  if (LIVES_REG_ECX & (1 << 28)) capable->hw.cpu_features |= CPU_FEATURE_HAS_AVX;
  if (LIVES_REG_ECX & (1 << 12)) capable->hw.cpu_features |= CPU_FEATURE_HAS_FMA;
  if (LIVES_REG_ECX & (1 << 16)) capable->hw.cpu_features |= CPU_FEATURE_HAS_F16C;
  if (LIVES_REG_ECX & (1 << 30)) capable->hw.cpu_features |= CPU_FEATURE_HAS_AVX2;
  if (LIVES_REG_ECX & (1 << 28) && LIVES_REG_ECX & (1 << 27) && LIVES_REG_ECX & (1 << 26))
    capable->hw.cpu_features |= CPU_FEATURE_HAS_AVX512;

  LIVES_REG_EAX = 4;
  if (capable->hw.cpu_type == CPU_TYPE_AMD) cpuid(0x8000001D, regs);
  else cpuid(0x00000000, regs);
  capable->hw.cache_size = (get_bits32(LIVES_REG_EBX, 31, 22) + 1) * (get_bits32(LIVES_REG_EBX, 21, 12) + 1)
                           * (get_bits32(LIVES_REG_EBX, 11, 0) + 1) * (LIVES_REG_ECX + 1);
}

#else
static void get_cpuinfo(void) {;}
#endif

static uint64_t fastrand_val = 0;

LIVES_GLOBAL_INLINE uint64_t fastrand(void) {
  fastrand_val ^= (fastrand_val << 13); fastrand_val ^= (fastrand_val >> 7);
  fastrand_val = ((fastrand_val & 0xFFFFFFFF00000000) >> 32) | ((fastrand_val & 0xFFFFFFFF) << 32);
  fastrand_val ^= (fastrand_val << 17);
  return fastrand_val;
}

LIVES_GLOBAL_INLINE void fastrand_add(uint64_t entropy) {fastrand_val += entropy;}

LIVES_GLOBAL_INLINE double fastrand_dbl(double range) {
  static const double divd = (double)(0xFFFFFFFFFFFFFFFF); return (double)fastrand() / divd * range;
}

/// pick a pseudo random uint between 0 and range (inclusive)
LIVES_GLOBAL_INLINE uint32_t fastrand_int(uint32_t range) {return (uint32_t)(fastrand_dbl((double)(++range)));}

LIVES_GLOBAL_INLINE void lives_srandom(unsigned int seed) {srandom(seed);}

static boolean force_rng64 = FALSE;
static int strikes = 0;


static void badrand(uint64_t last_rnum, uint64_t rnum) {
  char *msg = lives_strdup_printf("Insufficient entropy for RNG (%ld and %ld), cannot continue", last_rnum, rnum);
  lives_abort(msg);
}

static uint64_t nrcalls = 0;

#if HAVE_GETENTROPY
static boolean use_getentropy_raw = FALSE;
static boolean use_getentropy_cooked = FALSE;
#endif

#define RNG_MINDIFF BILLIONS(1)
// the random range is approximately 18 quintillion. The probability
// of getting two sequential numbers with delta < 1 billion is thus 1 in a 18 billion
// the probability of this happening twice should be 1 in 3 quadrillion
// if the randomness wre only 40 bits, the range wold be about 1 trillion, p would be about 1000, thus
// p twice would be about 1 / 1 million
LIVES_GLOBAL_INLINE uint64_t lives_random(void) {
  static uint64_t last_rnum = 0;
  uint64_t rnum = 0;

#if HAVE_GETENTROPY
  uint64_t rnum1 = 0, rnum2 = 0;
  if (!use_getentropy_cooked || getentropy(&rnum, 4))
#endif

    rnum = random();

  nrcalls++;

  while (force_rng64) {
#if HAVE_GETENTROPY
    if (use_getentropy_cooked && !getentropy(&rnum1, 4) && !getentropy(&rnum2, 4))
      rnum = (rnum << 22) ^ (rnum1 << 39) ^ rnum2;
    else
#endif
      rnum = (rnum << 22) ^ (random() << 39) ^ random();
    nrcalls += 2;

    if (labs(rnum - last_rnum) > RNG_MINDIFF) break;

    if (++strikes > 2) badrand(last_rnum, rnum);
    rnum = random();
  }

  last_rnum = rnum;
  return rnum;
}


void show_nrcalls(void) {
  g_print("nrcalls == %lu\n", nrcalls);
}


void lives_get_randbytes(void *ptr, size_t size) {
  if (size <= 8) {
    uint64_t rbytes = gen_unique_id();
    lives_memcpy(ptr, &rbytes, size);
  }
}


uint64_t gen_unique_id(void) {
  uint64_t rnum;
  int randres = 1;

#if HAVE_GETENTROPY
  if (use_getentropy_raw) randres = getentropy(&rnum, 8);
#endif

  if (randres) {
    fastrand_val = lives_random();
    fastrand();
    fastrand_val ^= lives_get_current_ticks();
    rnum = fastrand() ^ lives_random();
  }

  return rnum;
}

#define RB_THRESH 62
#define NR_TESTS 10000

static void check_random(void) {
  // check first with default method
  char *tmp;
  int rbits1, rbits2, rbits3, rbits4, best = 1;
  int maxbits;
  double qual, maxqual;

  rbits1 = benchmark_rng(NR_TESTS, lives_random, &qual);
  maxqual = qual;
  maxbits = rbits1;
  if (rbits1 < RB_THRESH) {
    tmp = lives_strdup_printf(_("Randomness of %d bits is TOO LOW for generating unique IDs\n"
                                "Will attempt to increase random bits to at least %d\n"),
                              rbits1, RB_THRESH);
  } else {
    tmp = lives_strdup_printf("RNG quality is %f, bits == %d,"
                              "checking if we can improve on this\n", qual, rbits1);
  }

  d_print(tmp);
  lives_free(tmp);

  // use an alternate strategy to try to increase randomnees
  force_rng64 = TRUE;

  rbits2 = benchmark_rng(NR_TESTS, gen_unique_id, &qual);

  if (rbits2 > maxbits || (rbits2 == maxbits && qual > maxqual)) {
    best = 2;
    maxbits = rbits2;
    maxqual = qual;
    tmp = lives_strdup_printf("RNG bits increased to %d\n", rbits2);
    d_print(tmp);
    lives_free(tmp);
    if (qual > maxqual) {
      maxqual = qual;
      tmp = lives_strdup_printf("RNG quality is now %f, this is better\n", qual);
      d_print(tmp);
      lives_free(tmp);
    }
  }

#if HAVE_GETENTROPY
  if (maxbits > RB_THRESH)
    tmp = lives_strdup("Compare quality with getentropy\n");
  else
    tmp = lives_strdup("Randomness still too low, trying  with getentropy\n");

  d_print(tmp);
  lives_free(tmp);

  use_getentropy_raw = TRUE;
  rbits3 = benchmark_rng(NR_TESTS, gen_unique_id, &qual);

  if (rbits3 < maxbits || (rbits3 == maxbits && qual < maxqual)) {
    tmp = lives_strdup_printf("Quality %f is worse ! Sticking with alt methods\n", qual);
  } else {
    maxbits = rbits3;
    maxqual = qual;
    best = 3;
    tmp = lives_strdup_printf("Quality %f is better, using getentropy.\n", qual);
  }

  d_print(tmp);
  lives_free(tmp);

  d_print("Testing hybrid method\n");

  use_getentropy_raw = FALSE;
  use_getentropy_cooked = TRUE;
  rbits4 = benchmark_rng(NR_TESTS, gen_unique_id, &qual);

  if (rbits4 < maxbits || (rbits4 == maxbits && qual < maxqual)) {
    tmp = lives_strdup_printf("Hybrid is worse, qual == %f, bits == %d, disabling hybrid\n", qual, rbits4);
  } else {
    maxbits = rbits4;
    maxqual = qual;
    best = 4;
    tmp = lives_strdup_printf("Hybrid is better, qual == %f, enabling hybrid\n", qual);
  }

  d_print(tmp);
  lives_free(tmp);

  use_getentropy_raw = FALSE;
  use_getentropy_cooked = FALSE;

  if (best == 3) use_getentropy_raw = TRUE;
  if (best == 4) use_getentropy_cooked = TRUE;
#endif

  if (maxbits < RB_THRESH) badrand(maxbits, RB_THRESH);

  if (best == 1) force_rng64 = FALSE;

  if (best > 1) {
    tmp = lives_strdup_printf("Randomness increased to %d bits\n", maxbits);
    d_print(tmp);
    lives_free(tmp);
  }
}


void init_random() {
  uint32_t rseed;
#ifdef HAVE_GETENTROPY
  if (getentropy(&rseed, 4)) rseed = (gen_unique_id() & 0xFFFFFFFF);
#else
  rseed = (gen_unique_id() & 0xFFFFFFFF);
#endif
  lives_srandom(rseed);
  check_random();
  fastrand_val = gen_unique_id();
}


//// AUTO-TUNING ///////

void autotune_u64_start(weed_plant_t *tuner,  uint64_t min, uint64_t max, int ntrials) {
  if (tuner) {
    int trials = weed_get_int_value(tuner, LIVES_LEAF_TRIALS, NULL);
    if (trials == 0) {
      weed_set_int_value(tuner, LIVES_LEAF_NTRIALS, ntrials);
      weed_set_int64_value(tuner, WEED_LEAF_MIN, min);
      weed_set_int64_value(tuner, WEED_LEAF_MAX, max);
    }
    weed_set_int64_value(tuner, LIVES_LEAF_TSTART, lives_get_current_ticks());
  }
}

#define NCYCS 16

static const char *get_tunert(int idx) {
  switch (idx) {
  case 2: return "orc_memcpy cutoff";
  case 3: return "read buffer size (small)";
  case 4: return "read buffer size (small / medium)";
  case 5: return "read buffer size (medium)";
  case 6: return "read buffer size (large)";
  default: break;
  }
  return "unknown variable";
}


uint64_t autotune_u64_end(weed_plant_t **tuner, uint64_t val, double cost) {
  if (!tuner || !*tuner) return val;
  else {
    ticks_t tottime = lives_get_current_ticks();
    int ntrials, trials;
    int64_t max;
    int64_t min = weed_get_int64_value(*tuner, WEED_LEAF_MIN, NULL);
    double tc = weed_get_double_value(*tuner, LIVES_LEAF_TOT_COST, NULL);
    weed_set_double_value(*tuner, LIVES_LEAF_TOT_COST, tc + cost);

    if (val < min) {
      val = min;
      weed_set_int_value(*tuner, LIVES_LEAF_TRIALS, 0);
      weed_set_int64_value(*tuner, LIVES_LEAF_TOT_TIME, 0);
      weed_set_double_value(*tuner, LIVES_LEAF_TOT_COST, 0);
      return val;
    }
    max = weed_get_int64_value(*tuner, WEED_LEAF_MAX, NULL);
    if (val > max) {
      val = max;
      weed_set_int_value(*tuner, LIVES_LEAF_TRIALS, 0);
      weed_set_int64_value(*tuner, LIVES_LEAF_TOT_TIME, 0);
      weed_set_double_value(*tuner, LIVES_LEAF_TOT_COST, 0);
      return val;
    }

    ntrials = weed_get_int_value(*tuner, LIVES_LEAF_NTRIALS, NULL);
    trials = weed_get_int_value(*tuner, LIVES_LEAF_TRIALS, NULL);

    weed_set_int_value(*tuner, LIVES_LEAF_TRIALS, ++trials);
    tottime += (weed_get_int64_value(*tuner, LIVES_LEAF_TOT_TIME, NULL)) - weed_get_int64_value(*tuner, LIVES_LEAF_TSTART, NULL);

    weed_set_int64_value(*tuner, LIVES_LEAF_TOT_TIME, tottime);

    if (trials >= ntrials) {
      int cycs = weed_get_int_value(*tuner, LIVES_LEAF_CYCLES, NULL) + 1;
      if (cycs < NCYCS) {
        double tcost = (double)weed_get_double_value(*tuner, LIVES_LEAF_TOT_COST, NULL);
        double totcost = (double)tottime * tcost;
        double avcost = totcost / (double)(cycs * ntrials);
        double ccosts, ccostl;
        boolean smfirst = FALSE;
        char *key1 = lives_strdup_printf("tottrials_%lu", val);
        char *key2 = lives_strdup_printf("totcost_%lu", val);

        weed_set_int_value(*tuner, key1, weed_get_int_value(*tuner, key1, NULL) + trials);

        weed_set_double_value(*tuner, key2, weed_get_double_value(*tuner, key2, NULL) + totcost);

        lives_free(key1);
        lives_free(key2);

        if (cycs & 1) smfirst = TRUE;
        weed_set_int_value(*tuner, LIVES_LEAF_CYCLES, cycs);

        weed_set_int_value(*tuner, LIVES_LEAF_TRIALS, 0);
        weed_set_int64_value(*tuner, LIVES_LEAF_TOT_TIME, 0);
        weed_set_double_value(*tuner, LIVES_LEAF_TOT_COST, 0);

        if (smfirst) {
          if (val > max || weed_plant_has_leaf(*tuner, LIVES_LEAF_SMALLER)) {
            ccosts = weed_get_double_value(*tuner, LIVES_LEAF_SMALLER, NULL);
            if (val > max || (ccosts < avcost)) {
              weed_set_double_value(*tuner, LIVES_LEAF_LARGER, avcost);
              weed_leaf_delete(*tuner, LIVES_LEAF_SMALLER);
              if (val > max) return max;
              return nxtval(val, min, TRUE); // TRUE to get smaller val
            }
          }
        }

        if (val < min || weed_plant_has_leaf(*tuner, LIVES_LEAF_LARGER)) {
          ccostl = weed_get_double_value(*tuner, LIVES_LEAF_LARGER, NULL);
          if (val < min || (ccostl < avcost)) {
            weed_set_double_value(*tuner, LIVES_LEAF_SMALLER, avcost);
            weed_leaf_delete(*tuner, LIVES_LEAF_LARGER);
            if (val < min) return min;
            return nxtval(val, max, FALSE);
          }
        }

        if (!smfirst) {
          if (val > max || weed_plant_has_leaf(*tuner, LIVES_LEAF_SMALLER)) {
            ccosts = weed_get_double_value(*tuner, LIVES_LEAF_SMALLER, NULL);
            if (val > max || (ccosts < avcost)) {
              weed_set_double_value(*tuner, LIVES_LEAF_LARGER, avcost);
              weed_leaf_delete(*tuner, LIVES_LEAF_SMALLER);
              if (val > max) return max;
              return nxtval(val, min, TRUE);
            }
          }

          if (!weed_plant_has_leaf(*tuner, LIVES_LEAF_LARGER)) {
            weed_set_double_value(*tuner, LIVES_LEAF_SMALLER, avcost);
            weed_leaf_delete(*tuner, LIVES_LEAF_LARGER);
            return nxtval(val, max, FALSE);
          }
        }

        if (!weed_plant_has_leaf(*tuner, LIVES_LEAF_SMALLER)) {
          weed_set_double_value(*tuner, LIVES_LEAF_LARGER, avcost);
          weed_leaf_delete(*tuner, LIVES_LEAF_SMALLER);
          return nxtval(val, min, TRUE);
        }

        if (smfirst) {
          if (!weed_plant_has_leaf(*tuner, LIVES_LEAF_LARGER)) {
            weed_set_double_value(*tuner, LIVES_LEAF_SMALLER, avcost);
            weed_leaf_delete(*tuner, LIVES_LEAF_LARGER);
            return nxtval(val, max, FALSE);
          }
        }

        weed_leaf_delete(*tuner, LIVES_LEAF_SMALLER);
        weed_leaf_delete(*tuner, LIVES_LEAF_LARGER);
        if (!smfirst) {
          return nxtval(nxtval(val, max, FALSE), max, FALSE);
        } else {
          return nxtval(nxtval(val, min, TRUE), min, TRUE);
        }
      } else {
        weed_size_t nleaves;
        char **res = weed_plant_list_leaves(*tuner, &nleaves);
        uint64_t bestval = val, xval;
        const char *key1 = "totcost_";
        char *key2;
        double avcost, costmin = 0.;
        boolean gotcost = FALSE;
        int j;

        for (int i = 1; i < nleaves; i++) {
          if (!strncmp(res[i], key1, 8)) {
            xval = strtoul((const char *)(res[i] + 8), NULL, 10);
            key2 = lives_strdup_printf("totrials_%lu", xval);
            for (j = i + 1; j < nleaves; j++) {
              if (!strcmp(res[j], key2)) break;
            }
            if (j == nleaves) {
              for (j = 0; j < i; j++) {
                if (!strcmp(res[j], key2)) break;
              }
            }
            if ((avcost = weed_get_double_value(*tuner, res[i], NULL)
                          / (double)weed_get_int_value(*tuner, res[j], NULL)) < costmin
                || !gotcost) {
              costmin = avcost;
              bestval = xval;
              gotcost = TRUE;
            }
            lives_free(key2);
          }
        }
        val = bestval;
        if (prefs->show_dev_opts)
          g_printerr("\n\n *** value of %s tuned to %lu ***\n\n\n",
                     get_tunert(weed_get_int64_value(*tuner, WEED_LEAF_INDEX, NULL)), val);
        // TODO: store value so we can recalibrate again later
        //tuned = (struct tuna *)lives_malloc(sizeof(tuna));
        //tuna->wptpp = tuner;
        //tuna->id = weed_get_in
        //lives_list_prepend(tunables, tuned);
        weed_plant_free(*tuner);
        *tuner = NULL;
        for (j = 0; j < nleaves; lives_free(res[j++]));
        lives_free(res);
      }
      return val;
    }
    weed_set_int64_value(*tuner, LIVES_LEAF_TOT_TIME, tottime);
  }
  return val;
}


LIVES_GLOBAL_INLINE char *get_md5sum(const char *filename) {
  return lives_md5_sum(filename, NULL);
}


boolean check_dev_busy(char *devstr) {
  int ret;
#ifdef IS_SOLARIS
  struct flock lock;
  lock.l_start = 0;
  lock.l_whence = SEEK_SET;
  lock.l_len = 0;
  lock.l_type = F_WRLCK;
#endif
  int fd = open(devstr, O_RDONLY | O_NONBLOCK);
  if (fd == -1) return FALSE;
#ifdef IS_SOLARIS
  ret = fcntl(fd, F_SETLK, &lock);
#else
  ret = flock(fd, LOCK_EX | LOCK_NB);
#endif
  close(fd);
  if (ret == -1) return FALSE;
  return TRUE;
}


boolean compress_files_in_dir(const char *dir, int method, void *data) {
  /// compress all files in dir with gzip (directories are not compressed)
  /// gzip default action is to compress all files, replacing foo.bar with foo.bar.gz
  /// if a file already has a .gz extension then it will be left uncchanged

  /// in future, method and data may be used to select compression method
  /// for now they are ignored
  char buff[65536];
  char *com, *cwd;
  boolean retval = FALSE;

  if (!check_for_executable(&capable->has_gzip, EXEC_GZIP)) return FALSE;
  if (lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    boolean needs_norem = FALSE;
    char *norem = lives_build_filename(dir, LIVES_FILENAME_NOREMOVE, NULL);
    if (lives_file_test(norem, LIVES_FILE_TEST_EXISTS)) {
      needs_norem = TRUE;
      lives_rm(norem);
    }
    cwd = lives_get_current_dir();
    THREADVAR(chdir_failed) = FALSE;
    lives_chdir(dir, TRUE);
    if (THREADVAR(chdir_failed)) {
      THREADVAR(chdir_failed) = FALSE;
      lives_chdir(cwd, TRUE);
      lives_free(cwd);
      lives_free(norem);
      return FALSE;
    }
    com = lives_strdup_printf("%s * 2>&1", EXEC_GZIP);
    lives_popen(com, TRUE, buff);
    lives_free(com);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    else retval = TRUE;
    lives_chdir(cwd, TRUE);
    lives_free(cwd);
    if (needs_norem) {
      lives_touch(norem);
      lives_free(norem);
    }
  }
  return retval;
}


// check with list like dirname, subdir|subsubdir,..
// for subdirs in dirname. Returns list wuth matched subdirs removed.
// i.e returned list will contain only those sudirs in the original list which are NOT found in dirname
// see also dir_to_pieces()
LiVESList *check_for_subdirs(const char *dirname, LiVESList *subdirs) {
  DIR *tldir, *xsubdir;
  struct dirent *tdirent, *xtdirent;
  LiVESList *xlist, *nlist;
  char *xdirname, *xxdirname;
  int part;
  if (!dirname) return subdirs;
  tldir = opendir(dirname);
  if (!tldir) return subdirs;
  while (subdirs && (tdirent = readdir(tldir))) {
    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
    for (xlist = subdirs; xlist; xlist = nlist) {
      int nparts = get_token_count((const char *)xlist->data, '|');
      char **array = lives_strsplit((const char *)xlist->data, "|", nparts);
      xtdirent = tdirent;
      xdirname = (char *)dirname;
      nlist = xlist->next;
      for (part = 0; part < nparts; part++) {
        if (part == 0) {
          if (lives_strcmp(array[0], xtdirent->d_name)) {
            lives_strfreev(array);
            break;
          }
          continue;
        }

        // get next level of subdirs
        xxdirname = lives_build_path(xdirname, xtdirent->d_name, NULL);

        if (xdirname != dirname) lives_free(xdirname);
        xdirname = xxdirname;
        xsubdir = opendir(xdirname);
        if (!xsubdir) break;
        while ((xtdirent = readdir(xsubdir))) {
          if (!lives_strcmp(array[part], xtdirent->d_name)) break;
        }

        if (xtdirent) continue;
        closedir(xsubdir);

        lives_strfreev(array);
        if (xdirname != dirname) lives_free(xdirname);
        break; // no match for array[part]
      }
      if (part == nparts) {
        // matched, we can remove from the list
        subdirs = lives_list_remove_node(subdirs, xlist, TRUE);
      }
    }
  }
  closedir(tldir);
  return subdirs;
}


boolean is_empty_dir(const char *dirname) {
  DIR *tldir;
  struct dirent *tdirent;
  boolean empty = TRUE;
  if (!dirname) return TRUE;
  tldir = opendir(dirname);
  if (!tldir) return FALSE;
  while (empty && (tdirent = readdir(tldir))) {
    if (tdirent->d_name[0] == '.'
        && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
    empty = FALSE;
  }
  closedir(tldir);
  return empty;
}


LiVESList *allthrds_list(void) {
  LiVESList *allthrds = NULL;
  DIR *proc_dir = opendir(LIVES_PROC_DIR);
  if (proc_dir) {
    struct dirent *entry;
    while ((entry = readdir(proc_dir)) != NULL) {
      if (entry->d_name[0] == '.') continue;
      int tid = atoi(entry->d_name);
      allthrds = lives_list_prepend(allthrds, LIVES_INT_TO_POINTER(tid));
    }
    closedir(proc_dir);
  }
  return allthrds;
}


LIVES_GLOBAL_INLINE char *get_symlink_for(const char *link) {
  char buff[PATH_MAX];
  ssize_t nbytes = readlink(link, buff, PATH_MAX);
  if (nbytes < 0) return lives_strdup(link);
  return lives_strndup(buff, nbytes);
}


char *get_mountpoint_for(const char *dirx) {
  char *mp = NULL, *tmp, *com, *res, *dir;
  size_t lmatch = 0, slen;
  int j;

  if (!dirx) return NULL;
  dir = get_symlink_for(dirx);
  slen = lives_strlen(dir);

  com = lives_strdup_printf("%s -lP", EXEC_DF);
  res = mini_popen(com);
  if (res && *res) {
    int lcount = get_token_count(res, '\n');
    char **array0 = lives_strsplit(res, "\n", lcount);
    for (int l = 0; l < lcount; l++) {
      int pccount = get_token_count(array0[l], ' ');
      char **array1 = lives_strsplit(array0[l], " ", pccount);
      lives_chomp(array1[pccount - 1], FALSE);
      for (j = 0; array1[pccount - 1][j] && j < slen; j++) if (array1[pccount - 1][j] != dir[j]) break;
      if (j > lmatch && !array1[pccount - 1][j]) {
        lmatch = j;
        if (mp) lives_free(mp);
        tmp = lives_strdup(array1[0]);
        mp = lives_filename_to_utf8(tmp, -1, NULL, NULL, NULL);
        lives_free(tmp);
      }
      lives_strfreev(array1);
    }
    lives_strfreev(array0);
  } else abort();
  if (res) lives_free(res);
  if (dir) lives_free(dir);
  return mp;
}


LIVES_GLOBAL_INLINE boolean file_is_ours(const char *fname) {
  if (fname) return !lives_strcmp(get_mountpoint_for(fname), capable->mountpoint);
  return FALSE;
}


char *get_fstype_for(const char *volx) {
  char *fstype = NULL;
  if (volx) {
    char *vol = get_symlink_for(volx);
    char *res, *com = lives_strdup_printf("%s -l --output=fstype,target", EXEC_DF);
    if ((res = mini_popen(com))) {
      int lcount = get_token_count(res, '\n');
      char **array0 = lives_strsplit(res, "\n", lcount);
      for (int l = 0; l < lcount; l++) {
        int pccount = get_token_count(array0[l], ' ');
        char **array1 = lives_strsplit(array0[l], " ", pccount);
        if (!lives_strcmp(array1[pccount - 1], vol)) fstype = lives_strdup(array1[0]);
        lives_strfreev(array1);
        if (fstype) break;
      }
      lives_strfreev(array0);
      lives_free(res);
    }
    lives_free(vol);
  }
  return fstype;
}


void free_fdets_list(LiVESList **listp) {
  LiVESList *list = *listp;
  lives_file_dets_t *filedets;
  for (; list && list->data; list = list->next) {
    filedets = (lives_file_dets_t *)list->data;
    lsd_struct_free(filedets->lsd);
    list->data = NULL;
  }
  if (*listp) {
    lives_list_free(*listp);
    *listp = NULL;
  }
}


int stat_to_file_dets(const char *fname, lives_file_dets_t *fdets) {
  struct stat filestat;
  int ret = stat(fname, &filestat);
  if (ret) {
    if (prefs->show_dev_opts) {
      char *msg = lives_strdup_printf("\nstat failed for file %s\n", fname);
      perror(msg);
      lives_free(msg);
    }
    fdets->size = -2;
    fdets->type = LIVES_FILE_TYPE_UNKNOWN;
    return ret;
  }
  fdets->type = (uint64_t)((filestat.st_mode & S_IFMT) >> 12);
  fdets->size = filestat.st_size;
  fdets->mode = (uint64_t)(filestat.st_mode & 0x0FFF);
  fdets->uid = filestat.st_uid;
  fdets->gid = filestat.st_gid;
  fdets->blk_size = (uint64_t)filestat.st_blksize;
  fdets->atime_sec = filestat.st_atim.tv_sec;
  fdets->atime_nsec = filestat.st_atim.tv_nsec;
  fdets->mtime_sec = filestat.st_mtim.tv_sec;
  fdets->mtime_nsec = filestat.st_mtim.tv_nsec;
  fdets->ctime_sec = filestat.st_ctim.tv_sec;
  fdets->ctime_nsec = filestat.st_ctim.tv_nsec;
  return ret;
}


static char *file_to_file_details(const char *filename, lives_file_dets_t *fdets,
                                  lives_proc_thread_t tinfo, uint64_t extra) {
  char *tmp, *tmp2;
  char *extra_details = lives_strdup("");

  if (!stat_to_file_dets(filename, fdets)) {
    // if stat fails, we have set set size to -2, type to LIVES_FILE_TYPE_UNKNOWN
    // and here we set extra_details to ""
    if (tinfo && lives_proc_thread_get_cancel_requested(tinfo)) {
      lives_free(extra_details);
      return NULL;
    }
    if (LIVES_FILE_IS_DIRECTORY(fdets->type)) {
      boolean emptyd = FALSE;
      if (extra & EXTRA_DETAILS_EMPTY_DIRS) {
        if ((emptyd = is_empty_dir(filename))) {
          fdets->type |= LIVES_FILE_TYPE_FLAG_EMPTY;
          tmp2 = lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                     (tmp = _("(empty)")));
          lives_free(tmp);
          lives_free(extra_details);
          extra_details = tmp2;
        }
        if (tinfo && lives_proc_thread_get_cancel_requested(tinfo)) {
          lives_free(extra_details);
          return NULL;
        }
      }
      if ((extra & EXTRA_DETAILS_DIRSIZE) &&
          check_for_executable(&capable->has_du, EXEC_DU)
          && !emptyd && fdets->type == LIVES_FILE_TYPE_DIRECTORY) {
        fdets->size = get_dir_size(filename);
      }

      if (!emptyd && (extra & EXTRA_DETAILS_CLIPHDR)) {
        int clipno;

        clipno = create_nullvideo_clip("tmp");

        if (clipno && IS_VALID_CLIP(clipno)) {
          if (read_headers(clipno, filename, NULL)) {
            lives_clip_t *sfile = mainw->files[clipno];
            char *name = lives_strdup(sfile->name);
            char *hdrstring =
              lives_strdup_printf
              (_("Header version %d (%s)"), sfile->header_version,
               (tmp =
                  (sfile->header_version == LIVES_CLIP_HEADER_VERSION) ?
                  _("Same as current") :
                  sfile->header_version < LIVES_CLIP_HEADER_VERSION ?
                  lives_strdup_printf(_("Current is %d"),
                                      LIVES_CLIP_HEADER_VERSION) :
                  lives_strdup_printf(_("Current is %d; consider updating before loading"),
                                      LIVES_CLIP_HEADER_VERSION)));
            lives_free(tmp);
            extra_details =
              lives_strdup_printf
              ("%s%s%s", extra_details, *extra_details ? ", " : "",
               (tmp = lives_strdup_printf
                      (_("Source: %s\n%s\nFrames: %d, size: %d X %d, fps: %.3f"),
                       name, hdrstring, sfile->frames, sfile->hsize,
                       sfile->vsize, sfile->fps)));
            lives_free(tmp); lives_free(name); lives_free(hdrstring);
            lives_freep((void **)&mainw->files[clipno]);
            if (mainw->first_free_file == ALL_USED || mainw->first_free_file > clipno)
              mainw->first_free_file = clipno;
          }
          if (mainw->hdrs_cache) cached_list_free(&mainw->hdrs_cache);
	  // *INDENT-OFF*
	}}}
    if (extra & EXTRA_DETAILS_MD5SUM) {
      fdets->md5sum = get_md5sum(filename);
    }
    if (extra & EXTRA_DETAILS_SYMLINK) {
      if (lives_file_test(filename, LIVES_FILE_TEST_IS_SYMLINK))
	fdets->type |= LIVES_FILE_TYPE_FLAG_SYMLINK;
    }
    if (extra & EXTRA_DETAILS_EXECUTABLE) {
      if (lives_file_test(filename, LIVES_FILE_TEST_IS_EXECUTABLE))
	fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE;
    }
    /// TODO
    /* if (extra & EXTRA_DETAILS_WRITEABLE) { */
    /*   if (LIVES_FILE_TEST_IS_EXECUTABLE(filename)) fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE; */
    /* } */
    /* if (extra & EXTRA_DETAILS_ACCESSIBLE) { */
    /*   if (LIVES_FILE_TEST_IS_EXECUTABLE(filename)) fdets->type |= LIVES_FILE_TYPE_FLAG_EXECUTABLE; */
    /* } */
  }
  // *INDENT-ON*
  else {
    /// stat failed
    if (extra & EXTRA_DETAILS_CHECK_MISSING) {
      if (!lives_file_test(filename, LIVES_FILE_TEST_EXISTS)) {
        fdets->type |= LIVES_FILE_TYPE_FLAG_MISSING;
        tmp2 = lives_strdup_printf("%s%s%s", extra_details, *extra_details ? ", " : "",
                                   (tmp = _("(ABSENT)")));
        lives_free(tmp);
        lives_free(extra_details);
        extra_details = tmp2;
      }
    }
  }
  return extra_details;
}


/**
   @brief create a list from a (sub)directory
   '.' and '..' are ignored
   subdir can be NULL
*/
void *_item_to_file_details(LiVESList **listp, const char *item,
                            const char *orig_loc, uint64_t extra, int type) {
  // type 0 = dir
  // type 1 = ordfile
  GET_PROC_THREAD_SELF(self);
  lives_file_dets_t *fdets;
  LiVESList *list;
  char *extra_details;
  const char *dir = NULL;
  char *subdirname;
  boolean empty = TRUE;

  if (self) lives_proc_thread_set_cancellable(self);

  switch (type) {
  case 0: {
    DIR *tldir;
    struct dirent *tdirent;
    // dir
    dir = item;
    if (!dir) return NULL;
    tldir = opendir(dir);
    if (!tldir) {
      *listp = lives_list_append(*listp, NULL);
      return NULL;
    }

    while (1) {
      tdirent = readdir(tldir);
      if (lives_proc_thread_get_cancel_requested(self) || !tdirent) {
        closedir(tldir);
        if (lives_proc_thread_get_cancel_requested(self)) return NULL;
        break;
      }
      if (tdirent->d_name[0] == '.'
          && (!tdirent->d_name[1] || tdirent->d_name[1] == '.')) continue;
      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);
      fdets->name = lives_strdup(tdirent->d_name);
      //g_print("GOT %s\n", fdets->name);
      fdets->size = -1;
      *listp = lives_list_append(*listp, fdets);
      if (lives_proc_thread_get_cancel_requested(self)) {
        closedir(tldir);
        return NULL;
      }
    }
    break;
  }
  case 1: {
    FILE *orderfile;
    char buff[PATH_MAX];
    const char *ofname = item;

    if (!(orderfile = fopen(ofname, "r"))) return NULL;
    while (1) {
      if (lives_proc_thread_get_cancel_requested(self) || !orderfile) {
        if (orderfile) {
          fclose(orderfile);
        }
        return NULL;
      }
      if (!lives_fgets(buff, PATH_MAX, orderfile)) {
        fclose(orderfile);
        break;
      }
      lives_chomp(buff, FALSE);

      fdets = (lives_file_dets_t *)struct_from_template(LIVES_STRUCT_FILE_DETS_T);

      fdets->name = lives_strdup(buff);
      fdets->size = -1;
      *listp = lives_list_append(*listp, fdets);
      if (lives_proc_thread_get_cancel_requested(self)) {
        fclose(orderfile);
        return NULL;
      }
    }
    break;
  }
  default: return NULL;
  }

  if (*listp) empty = FALSE;
  *listp = lives_list_append(*listp, NULL);

  if (empty || lives_proc_thread_get_cancel_requested(self)) return NULL;

  // listing done, now get details for each entry
  list = *listp;
  while (list && list->data) {
    if (lives_proc_thread_get_cancel_requested(self)) return NULL;

    extra_details = lives_strdup("");
    fdets = (lives_file_dets_t *)list->data;

    if (orig_loc && *orig_loc) subdirname = lives_build_filename(orig_loc, fdets->name, NULL);
    else subdirname = lives_build_path(dir, fdets->name, NULL);

    // need to call even with no extra, because it gets size / type tc.
    if (!(extra_details = file_to_file_details(subdirname, fdets, self, extra))) {
      lives_free(subdirname);
      lives_free(extra_details);
      return NULL;
    }

    lives_free(subdirname);

    if (self && lives_proc_thread_get_cancel_requested(self)) {
      lives_free(extra_details);
      return NULL;
    }
    fdets->extra_details = lives_strdup(extra_details);
    lives_free(extra_details);
    list = list->next;
  }

  return NULL;
}

/**
   @brief create a list from a (sub)directory
   '.' and '..' are ignored
   subdir can be NULL
   runs in a proc_htread
*/
lives_proc_thread_t dir_to_file_details(LiVESList **listp, const char *dir,
                                        const char *orig_loc, uint64_t extra) {
  return lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                  (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
                                  listp, dir, orig_loc, extra, 0);
}


lives_proc_thread_t ordfile_to_file_details(LiVESList **listp, const char *ofname,
    const char *orig_loc, uint64_t extra) {
  return lives_proc_thread_create(LIVES_THRDATTR_NONE,
                                  (lives_funcptr_t)_item_to_file_details, -1, "vssIi",
                                  listp, ofname, orig_loc, extra, 1);
}


#ifdef PRODUCE_LOG
// disabled by default
void lives_log(const char *what) {
  char *lives_log_file = lives_build_filename(prefs->workdir, LIVES_LOG_FILE, NULL);
  if (mainw->log_fd < 0) mainw->log_fd = open(lives_log_file, O_WRONLY | O_CREAT, DEF_FILE_PERMS);
  if (mainw->log_fd != -1) {
    char *msg = lives_strdup("%s|%d|", what, mainw->current_file);
    write(mainw->log_fd, msg, strlen(msg));
    lives_free(msg);
  }
  lives_free(lives_log_file);
}
#endif


// fast hash from: http://www.azillionmonkeys.com/qed/hash.html
// (c) Paul Hsieh
#define get16bits(d) (*((const uint16_t *) (d)))

#define HASHROOT 5381

LIVES_GLOBAL_INLINE uint32_t fast_hash(const char *key, size_t ss) {
  /// approx 5 - 10 % faster than lives_string_hash
  if (key && *key) {
    int len = ss ? ss : lives_strlen(key), rem = len & 3;
    uint32_t hash = len + HASHROOT, tmp;
    len >>= 2;
    for (; len > 0; len--) {
      hash  += get16bits(key);
      tmp    = (get16bits(key + 2) << 11) ^ hash;
      hash   = (hash << 16) ^ tmp;
      key  += 4;
      hash  += hash >> 11;
    }

    /* Handle end cases */
    switch (rem) {
    case 3: hash += get16bits(key);
      hash ^= hash << 16;
      hash ^= ((int8_t)key[2]) << 18;
      hash += hash >> 11;
      break;
    case 2: hash += get16bits(key);
      hash ^= hash << 11; hash += hash >> 17;
      break;
    case 1: hash += (int8_t) * key;
      hash ^= hash << 10; hash += hash >> 1;
      break;
    default: break;
    }

    /* Force "avalanching" of final 127 bits */
    hash ^= hash << 3; hash += hash >> 5; hash ^= hash << 4;
    hash += hash >> 17; hash ^= hash << 25; hash += hash >> 6;
    return hash;
  }
  return 0;
}


LIVES_GLOBAL_INLINE uint64_t bin_hash64(void *data, size_t dlen) {
  // write alternare bytes to 2 memblocks. hash both blocks and get upper and lower
  // 32 bits
  uint64_t hash64 = 0;
  char *c = (char *)data;
  size_t hslen = (dlen + 3) >> 1, ss1 = 0, ss2 = 0;
  char *str1 = (char *)lives_malloc(hslen);
  char *str2 = (char *)lives_malloc(hslen);
  for (int j = 0; j < hslen; j++) {
    str1[j] = *c;
    ss1++;
    c++;
    str2[j] = *c;
    ss2++;
    c++;
  }
  hash64 = fast_hash(str1, ss1);
  hash64 = (hash64 << 32) | fast_hash(str2, ss2);
  lives_free(str1); lives_free(str2);
  return hash64;
}

LIVES_GLOBAL_INLINE uint64_t fast_hash64(const char *key) {
  char *c = (char *)key;
  uint64_t hash64 = 0;
  if (*c) {
    size_t slen = lives_strlen(key);
    if (slen <= 8) {
      lives_memcpy(&hash64, key, slen);
      return hash64;
    }

    size_t hslen = (slen + 3) >> 1, ss1 = 0, ss2 = 0;
    char *str1 = (char *)lives_malloc(hslen);
    char *str2 = (char *)lives_malloc(hslen);
    --hslen; // make doubly sure to allow for \0
    for (int j = 0; j < hslen; j++) {
      str1[j] = *c;
      ss1++;
      if (!*(++c)) break;
      str2[j] = *c;
      ss2++;
      if (!*(++c)) break;
    }
    hash64 = fast_hash(str1, ss1);
    hash64 = (hash64 << 32) | fast_hash(str2, ss2);
    lives_free(str1); lives_free(str2);
  }
  return hash64;
}


/////////////// move to other file ////

LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new(int subtype) {
  weed_plant_t *plant = weed_plant_new(WEED_PLANT_LIVES);
  weed_set_int_value(plant, WEED_LEAF_LIVES_SUBTYPE, subtype);
  weed_set_int64_value(plant, LIVES_LEAF_UID, gen_unique_id());
  //weed_add_plant_flags(plant, WEED_FLAG_IMMUTABLE | WEED_FLAG_UNDELETABLE, NULL);
  return plant;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_with_index(int subtype, int64_t index) {
  weed_plant_t *plant = lives_plant_new(subtype);
  weed_set_int64_value(plant, WEED_LEAF_INDEX, index);
  return plant;
}


LIVES_GLOBAL_INLINE weed_plant_t *lives_plant_new_with_refcount(int subtype) {
  weed_plant_t *plant = lives_plant_new(subtype);
  weed_add_refcounter(plant);
  return plant;
}

///////////////// to do - move to performance manager ////

/// estimate the machine load
static boolean inited = FALSE;
static int struggling = 0;
static tab_data_t *force = NULL;

void reset_effort(void) {
  if (force) {
    free_tabdata(force);
    force = NULL;
  }
  prefs->pb_quality = future_prefs->pb_quality;
  inited = TRUE;
  struggling = 0;
  if ((mainw->is_rendering || (mainw->multitrack
                               && mainw->multitrack->is_rendering)) && !mainw->preview_rendering)
    mainw->effort = -EFFORT_RANGE_MAX;
  else {
    if (mainw->effort > EFFORT_LIMIT_MED) mainw->effort = EFFORT_LIMIT_MED;
    if (mainw->effort < -EFFORT_LIMIT_MED) mainw->effort = -EFFORT_LIMIT_MED;
  }
}


void update_effort(float impulse) {
  short pb_quality = prefs->pb_quality;

  if (LIVES_IS_RENDERING) {
    mainw->effort = -EFFORT_RANGE_MAX;
    prefs->pb_quality = PB_QUALITY_HIGH;
    return;
  }

  if (!force) force = init_tab_data(1, EFFORT_RANGE_MAX);

  tabdata_get_avgs(force, &impulse);
  mainw->effort = force->tots[0];

  if (mainw->effort > EFFORT_RANGE_MAX) mainw->effort = EFFORT_RANGE_MAX;
  if (mainw->effort < -EFFORT_RANGE_MAX) mainw->effort = -EFFORT_RANGE_MAX;

  if (mainw->effort <= 0) struggling--;
  else struggling++;

  if (struggling > EFFORT_LIMIT_MED) struggling = EFFORT_LIMIT_MED;
  if (struggling < -EFFORT_LIMIT_MED) struggling = -EFFORT_LIMIT_MED;

  if (mainw->effort > 0) {
    if (struggling >= EFFORT_LIMIT_MED && mainw->effort >= EFFORT_LIMIT_MED)
      pb_quality = PB_QUALITY_LOW;
    else if (struggling > 0 && pb_quality == PB_QUALITY_HIGH)
      pb_quality = PB_QUALITY_MED;
  }

  if (mainw->effort < 0) {
    if (struggling <= -EFFORT_LIMIT_MED && mainw->effort <= EFFORT_LIMIT_MED)
      pb_quality = PB_QUALITY_HIGH;
    else if (struggling > 0 && pb_quality == PB_QUALITY_LOW)
      pb_quality = PB_QUALITY_MED;
  }

  if (pb_quality != future_prefs->pb_quality)
    future_prefs->pb_quality = pb_quality;
  //g_print("STRG %d and %d %d\n", struggling, mainw->effort, prefs->pb_quality);
}


char *grep_in_cmd(const char *cmd, int mstart, int npieces, const char *mphrase, int ridx, int rlen, boolean partial) {
  char **lines, **words, **mwords;
  char *match = NULL, *wline;
  char buff[65536];
  size_t nlines, mwlen, mlen = 0;
  int m, minpieces;

  //BREAK_ME("GIC");

  if (!mphrase || npieces < -1 || !npieces || rlen < 1 || (ridx <= mstart && ridx + rlen > mstart)
      || (npieces > 0 && (ridx + rlen > npieces || mstart >= npieces))) return NULL;

  mwlen = get_token_count(mphrase, ' ');
  if (mstart + mwlen > npieces
      || (ridx + rlen > mstart && ridx < mstart + mwlen)) return NULL;

  mwords = lives_strsplit(mphrase, " ", mwlen);

  if (!cmd || !mphrase || !*cmd || !*mphrase) goto grpcln;
  lives_popen(cmd, FALSE, buff);
  if (THREADVAR(com_failed)
      || (!*buff || !(nlines = get_token_count(buff, '\n')))) {
    THREADVAR(com_failed) = FALSE;
    goto grpcln;
  }

  if (partial) mlen = lives_strlen(mwords[mwlen - 1]);

  minpieces = MAX(mstart + mwlen, ridx + rlen);

  lines = lives_strsplit(buff, "\n", nlines);
  for (int l = 0; l < nlines; l++) {
    if (*lines[l]) {
      // reduce multiple spaces to one
      char *tmp = lives_strdup(lines[l]);
      size_t llen = lives_strlen(tmp), tlen;
      while (1) {
        wline = subst(tmp, "  ", " ");
        lives_free(tmp);
        if ((tlen = lives_strlen(wline)) == llen) break;
        tmp = wline;
        llen = tlen;
      }
      //lives_free(tmp);


      if (*wline && get_token_count(wline, ' ') >= minpieces) {
        words = lives_strsplit(wline, " ", npieces);
        for (m = 0; m < mwlen; m++) {
          if (partial && m == mwlen - 1) {
            if (lives_strncmp(words[m + mstart], mwords[m], mlen)) break;
          } else if (lives_strcmp(words[m + mstart], mwords[m])) break;
        }
        if (m == mwlen) {
          match = lives_strdup(words[ridx]);
          for (int w = 1; w < rlen; w++) {
            char *tmp = lives_strdup_printf(" %s", words[ridx + w]);
            match = lives_concat(match, tmp);
          }
        } else {
          if (partial) {
            // check end as well
            if (!lives_strncmp(mphrase, wline + llen - mlen, mlen)) {
              match = lives_strdup(words[ridx]);
              break;
            }
          }
        }
        lives_strfreev(words);
      }
      //lives_free(wline);
      if (match) break;
    }
  }
  lives_strfreev(lines);
grpcln:
  lives_strfreev(mwords);
  return match;
}

LIVES_LOCAL_INLINE boolean mini_run(char *cmd) {
  if (!cmd) return FALSE;
  lives_system(cmd, TRUE);
  lives_free(cmd);
  if (THREADVAR(com_failed)) return FALSE;
  return TRUE;
}

LIVES_LOCAL_INLINE char *mini_popen(char *cmd) {
  if (!cmd) return NULL;
  else {
    char buff[PATH_MAX];
    //char *com = lives_strdup_printf("%s $(%s)", capable->echo_cmd, EXEC_MKTEMP);
    lives_popen(cmd, TRUE, buff);
    lives_free(cmd);
    lives_chomp(buff, FALSE);
    return lives_strdup(buff);
  }
}


LiVESResponseType send_to_trash(const char *item) {
  LiVESResponseType resp = LIVES_RESPONSE_NONE;
  boolean retval = TRUE;
  char *reason = NULL;
#ifndef IMPL_TRASH
  do {
    resp = LIVES_RESPONSE_NONE;
    if (check_for_executable(&capable->has_gio, EXEC_GIO) != PRESENT) {
      reason = lives_strdup_printf(_("%s was not found\n"), EXEC_GIO);
      retval = FALSE;
    } else {
      char *com = lives_strdup_printf("%s trash \"%s\"", EXEC_GIO, item);
      retval = mini_run(com);
    }
#else
  /// TODO *** - files should be moved to
  /// 1) if not $HOME partition, capable->mountpoint/.Trash; also check all toplevels
  /// check for sticky bit and also non symlink. Then create uid subdir
  /// else try to create mountpoint / .Trash-$uid
  /// else (or if in home dir):
  /// capable->xdg_data_home/Trash/

  /// create an entry like info/foo1.trashinfo (O_EXCL)

  /// [Trash Info]
  /// Path=/home/user/livesprojects/foo1
  /// DeletionDate=2020-07-11T14:57:00

  /// then move / copy file or dir to files/foo1
  /// - if already exists, append .2, .3 etc.
  // see: https://specifications.freedesktop.org/trash-spec/trashspec-latest.html
  int vnum = 0;
  char *trashdir;
  char *mp1 = get_mountpount_for(item);
  char *mp2 = get_mountpount_for(capable->home_dir);
  if (!lives_strcmp(mp1, mp2)) {
    char *localshare = get_localsharedir(NULL);
    trashdir = lives_build_path(localshare, "Trash", NULL);
    lives_free(localshare);
    trashinfodir = lives_build_path(trashdir, "info", NULL);
    trashfilesdir = lives_build_path(trashdir, "files", NULL);
    umask = capable->umask;
    capable->umask = 0700;
    if (!make_writeable_dir(trashinfodir)) {
      retval = FALSE;
      reason = lives_strdup_printf(_("Could not write to %s\n"), trashinfodir);
    }
    if (retval) {
      if (!make_writeable_dir(trashfilesdir)) {
        retval = FALSE;
        reason = lives_strdup_printf(_("Could not write to %s\n"), trashfilesdir);
      }
    }
    capable->umask = umask;
    if (retval) {
      char *trashinfo;
      int fd;
      while (1) {
        if (!vnum) trashinfo = lives_strdup_printf("%s.trashinfo", basenm);
        else trashinfo = lives_strdup_printf("%s.%d.trashinfo", basenm, vnum);
        fname = lives_build_filename(trashinfodir, trashinfo, NULL);
        fd = lives_open2(fname, O_CREAT | O_EXCL);
        if (fd) break;
        vnum++;
      }
      // TODO - write stuff, close, move item


    }
  }
  /// TODO...
#endif
    if (!retval) {
      char *msg = lives_strdup_printf(_("LiVES was unable to send the item to trash.\n%s"),
                                      reason ? reason : "");
      lives_freep((void **)&reason);
      resp = do_abort_retry_cancel_dialog(msg);
      lives_free(msg);
      if (resp == LIVES_RESPONSE_CANCEL) return resp;
    }
  } while (resp == LIVES_RESPONSE_RETRY);
  return LIVES_RESPONSE_OK;
}


/// x11 stuff

#ifdef GDK_WINDOWING_X11
/* Window *find_wm_toplevel(Window xid) { */
/*   Window root, parent, *children; */
/*   unsigned int nchildren; */
/*   if (XQueryTree (gdk_x11_get_default_xdisplay (), xid, &root, */
/*   		  &parent, &children, &nchildren) == 0) { */
/*     // None found */
/*   } */
/* } */


static boolean rec_desk_done(livespointer data) {
  rec_args *recargs = (rec_args *)data;

  if (lives_get_status() != LIVES_STATUS_IDLE) return TRUE;
  else {
    lives_clip_t *sfile;
    double tsec = recargs->tottime / TICKS_PER_SECOND_DBL;
    char *com, *timestr = format_tstr(tsec, 0);
    int current_file = mainw->current_file;

    if (!IS_VALID_CLIP(recargs->clipno)) goto ohnoes2;
    sfile = mainw->files[recargs->clipno];
    sfile->is_loaded = TRUE;
    sfile->cb_src = -1;

    if (sfile->frames <= 0) goto ohnoes;
    add_to_clipmenu_any(recargs->clipno);

    do_info_dialogf(_("Grabbed %d frames in %s (average %.3f fps)"),
                    sfile->frames, timestr, tsec ? (double)sfile->frames / tsec : 0.);
    lives_free(timestr);

    switch_clip(1, recargs->clipno, FALSE);

    cfile->undo1_dbl = recargs->fps;
    cfile->fps = 0.;
    mainw->cancelled = CANCEL_NONE;

    THREAD_INTENTION = OBJ_INTENTION_RECORD;

    on_resample_vid_ok(NULL, NULL);
    if (mainw->cancelled != CANCEL_NONE) {
      close_current_file(current_file);
      return FALSE;
    }

    reget_afilesize(recargs->clipno);
    get_total_time(sfile);

    com = lives_strdup_printf("%s clear_tmp_files \"%s\"", prefs->backend, cfile->handle);
    lives_system(com, FALSE);
    lives_free(com);
    cfile->end = cfile->frames;

    switch_clip(1, mainw->current_file, TRUE);
    get_play_times();

    save_clip_values(mainw->current_file);

    lives_widget_set_sensitive(mainw->desk_rec, TRUE);
    lives_free(recargs);

    return FALSE;

ohnoes:
    mainw->current_file = recargs->clipno;
    close_current_file(current_file);
    lives_free(recargs);
ohnoes2:
    do_error_dialog(_("Screen grab failed"));
    lives_widget_set_sensitive(mainw->desk_rec, TRUE);
  }
  return FALSE;
}


void rec_desk(void *args) {
  // experimental
  // TODO - start disk space monitor
  GET_PROC_THREAD_SELF(self);
  savethread_priv_t *saveargs = NULL;
  lives_proc_thread_t saver_lpt = NULL;
  lives_painter_surface_t *csurf = NULL;
  rec_args *recargs = (rec_args *)args;
  LiVESWidget *win;
  lives_clip_t *sfile;
  weed_timecode_t tc;
  weed_layer_t *layer;
  LiVESPixbuf *pixbuf;
  LiVESXCursor *cursor;
  int clips[1];
  int64_t frames[1];
  double cx, cy;
  char *imname;
  lives_alarm_t alarm_handle, fps_alarm = LIVES_NO_ALARM;
  boolean cancelled = FALSE;
  int x = 0, y = 0, frameno = 0;
  int w = GUI_SCREEN_WIDTH, h = GUI_SCREEN_HEIGHT;

  if (self) lives_proc_thread_set_cancellable(self);

  if (recargs->screen_area == SCREEN_AREA_FOREGROUND) {
    win = LIVES_MAIN_WINDOW_WIDGET;
    get_border_size(win, &x, &y);
    x = abs(x);
    y = abs(y);
    w = lives_widget_get_allocation_width(win);
    h = lives_widget_get_allocation_height(win);
  }

  sfile = mainw->files[recargs->clipno];
  clips[0] = recargs->clipno;
  migrate_from_staging(recargs->clipno);

  lives_widget_set_sensitive(mainw->desk_rec, TRUE);
  alarm_handle = lives_alarm_set(TICKS_PER_SECOND_DBL * recargs->delay_time);
  lives_sleep_while_false(!lives_alarm_check(alarm_handle) == 0
                          || (self && lives_proc_thread_get_cancel_requested(self)));
  lives_alarm_clear(alarm_handle);

  if (self && lives_proc_thread_get_cancel_requested(self)) goto done;
  //lives_widget_set_sensitive(mainw->desk_rec, FALSE);

  saveargs = (savethread_priv_t *)lives_calloc(1, sizeof(savethread_priv_t));
  saveargs->compression = 100 - prefs->ocp;

  alarm_handle = lives_alarm_set(TICKS_PER_SECOND_DBL * recargs->rec_time);

  // temp kludge ahead !
  open_ascrap_file(recargs->clipno);

  IF_APLAYER_PULSE({
    pulse_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_DESKTOP_GRAB_INT);
  })

  IF_APLAYER_JACK({
    jack_rec_audio_to_clip(mainw->ascrap_file, -1, RECA_DESKTOP_GRAB_INT);
  })

  add_primary_src(recargs->clipno, NULL, LIVES_SRC_TYPE_RECORDER);

  mainw->rec_samples = -1; // record unlimited
  //
  recargs->tottime = lives_get_current_ticks();

  while (1) {
    if ((recargs->rec_time && !lives_alarm_check(alarm_handle))
        || (self && (cancelled = lives_proc_thread_get_cancel_requested(self))))
      break;

    fps_alarm = lives_alarm_set(TICKS_PER_SECOND_DBL / recargs->fps);

    // TODO - use idle proc_thread
    if (saver_lpt) {
      lives_proc_thread_join(saver_lpt);
      saver_lpt = NULL;
      if (saveargs->error
          || ((recargs->rec_time && !lives_alarm_check(alarm_handle))
              || (self && lives_proc_thread_get_cancel_requested(self)))) {
        lives_alarm_clear(fps_alarm);
        break;
      }
    }

    tc = lives_get_current_ticks();
    layer = weed_layer_new(WEED_LAYER_TYPE_VIDEO);

#ifdef FEEDBACK
    lives_widget_set_opacity(LIVES_MAIN_WINDOW_WIDGET, 0.);
    if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 0.);
    lives_widget_context_update();
#endif

    pixbuf = gdk_pixbuf_get_from_window(capable->wm_caps.root_window, x, y, w, h);
    if (!pixbuf) {
      lives_alarm_clear(fps_alarm);
      break;
    }
    if (!pixbuf_to_layer(layer, pixbuf)) lives_widget_object_unref(pixbuf);

#ifdef FEEDBACK
    if (LIVES_IS_PLAYING) {
      weed_layer_ref(layer);
      mainw->ext_layer = layer;
    }
    if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 1.);
    lives_widget_context_update();
#endif

    cursor = gdk_window_get_cursor(lives_widget_get_xwindow(LIVES_MAIN_WINDOW_WIDGET));
    if (cursor) csurf = gdk_cursor_get_surface(cursor, &cx, &cy);
    else csurf = NULL;
    /* GdkCursor *curs = gdk_window_get_device_cursor(NULL, device); */
    /* gdk_cursor_get_image(curs); */

    if (!csurf) g_print("no cursor\n");
    else g_print("Curs at %f, %f\n", cx, cy);

    if (recargs->scale < 1.) {
      if (!resize_layer(layer, (double)w * recargs->scale, (double)h * recargs->scale,
                        LIVES_INTERP_FAST, WEED_PALETTE_END, 0)) {
        lives_alarm_clear(fps_alarm);
        weed_layer_unref(layer);
        break;
      }
    }

    imname = make_image_file_name(sfile, ++frameno, NULL);

    frames[0] = frameno;
    sfile->event_list = append_frame_event(sfile->event_list, tc, 1, clips, frames);

    if (saveargs->layer) weed_layer_unref(saveargs->layer);
    saveargs->layer = layer;
    if (saveargs->fname) lives_free(saveargs->fname);
    saveargs->fname = imname;

    saver_lpt = lives_proc_thread_create(LIVES_THRDATTR_NONE, layer_to_png_threaded,
                                         WEED_SEED_BOOLEAN, NULL, "v", saveargs);

    // TODO - check for timeout / cancel here too
    lives_sleep_until_zero(lives_alarm_check(fps_alarm) && (!recargs->rec_time || lives_alarm_check(alarm_handle))
                           && (!self || !(cancelled = lives_proc_thread_get_cancel_requested(self))));
    lives_alarm_clear(fps_alarm);
  }
  lives_alarm_clear(alarm_handle);

  recargs->tottime = lives_get_current_ticks() - recargs->tottime;

#ifdef FEEDBACK
  lives_widget_set_opacity(LIVES_MAIN_WINDOW_WIDGET, 1.);
  if (mainw->play_window) lives_widget_set_opacity(mainw->play_window, 1.);
  lives_widget_context_update();

  mainw->ext_layer = NULL;
#endif

  if (saver_lpt) lives_proc_thread_join(saver_lpt);

  if (saveargs) {
    if (saveargs->layer) weed_layer_unref(saveargs->layer);
    if (saveargs->fname) lives_free(saveargs->fname);
    lives_free(saveargs);
  }

done: // timed out or cancelled
  lives_signal_handler_block(mainw->desk_rec, mainw->desk_rec_func);
  lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->desk_rec), FALSE);
  lives_signal_handler_unblock(mainw->desk_rec, mainw->desk_rec_func);
  sfile->fps = recargs->fps;
  sfile->hsize = w;
  sfile->vsize = h;
  sfile->start = 1;
  sfile->end = sfile->frames = frameno;
  if (!save_clip_values(recargs->clipno)) sfile->frames = -1;
  if (sfile->frames > 0 && prefs->crash_recovery) add_to_recovery_file(sfile->handle);
  recargs->lpt = self;
  lives_widget_set_sensitive(mainw->desk_rec, FALSE);
  lives_idle_add(rec_desk_done, recargs);
}

#endif

char *get_wid_for_name(const char *wname) {
#ifndef GDK_WINDOWING_X11
  return NULL;
#else
  char *wid = NULL, *cmd;
  if (!wname || !*wname) return NULL;

  if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL)) {
    cmd = lives_strdup_printf("%s -l", EXEC_WMCTRL);
    wid = grep_in_cmd(cmd, 3, 4, wname, 0, 1, TRUE);
    lives_free(cmd);
    if (wid) {
      //g_print("GOT wm wid %s\n", wid);
      return wid;
    }
  }

  // need to use -root -tree

  if (check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    cmd = lives_strdup_printf("%s -name \"%s\" 2>/dev/null", EXEC_XWININFO, wname);
    wid = grep_in_cmd(cmd, 1, -1, "Window id:", 3, 1, FALSE);
    lives_free(cmd);
    if (wid) {
      //g_print("GOT xw wid %s\n", wid);
      return wid;
    }
  }

  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    char buff[65536];
    size_t nlines;
    // returns a list, and we need to check each one
    cmd = lives_strdup_printf("%s search \"%s\"", EXEC_XDOTOOL, wname);
    lives_popen(cmd, FALSE, buff);
    lives_free(cmd);
    if (THREADVAR(com_failed)
        || (!*buff || !(nlines = get_token_count(buff, '\n')))) {
      THREADVAR(com_failed) = FALSE;
      return wid;
    } else {
      char buff2[1024];
      char **lines = lives_strsplit(buff, "\n", nlines);
      for (int l = 0; l < nlines; l++) {
        if (!*lines[l]) continue;
        cmd = lives_strdup_printf("%s getwindowname %s", EXEC_XDOTOOL, lines[l]);
        lives_popen(cmd, FALSE, buff2);
        lives_free(cmd);
        if (THREADVAR(com_failed)) {
          THREADVAR(com_failed) = FALSE;
          break;
        }
        lives_chomp(buff2, FALSE);
        if (!lives_strcmp(wname, buff2)) {
          wid = lives_strdup_printf("0x%lX", lives_strtol(lines[l]));
          break;
        }
      }
      lives_strfreev(lines);
    }
  }
  if (wid) {
    //g_print("GOT xxxxxxwm wid %s\n", wid);
    return wid;
  }
  return wid;
#endif
}


static lives_proc_thread_t enable_ss_lpt = NULL;

static boolean enable_ss_cb(void) {
  lives_reenable_screensaver();
  return FALSE;
}

boolean lives_reenable_screensaver(void) {
  char *com = NULL, *tmp;
#ifdef GDK_WINDOWING_X11
  uint64_t awinid = lives_xwindow_get_xwinid(capable->wm_caps.root_window, NULL);
  com = lives_strdup_printf("%s s on 2>%s; %s +dpms 2>%s;",
                            EXEC_XSET, LIVES_DEVNULL, EXEC_XSET, LIVES_DEVNULL);
  if (capable->has_gconftool_2) {
    char *xnew = lives_strdup_printf(" %s --set --type bool /apps/gnome-screensaver/"
                                     "idle_activation_enabled true 2>/dev/null ;",
                                     EXEC_GCONFTOOL_2);
    tmp = lives_strconcat(com, xnew, NULL);
    lives_free(com); lives_free(xnew);
    com = tmp;
  }
  if (capable->has_xdg_screensaver && awinid) {
    char *xnew = lives_strdup_printf(" %s resume %" PRIu64 " 2>%s;",
                                     EXEC_XDG_SCREENSAVER, awinid, LIVES_DEVNULL);
    tmp = lives_strconcat(com, xnew, NULL);
    lives_free(com); lives_free(xnew);
    com = tmp;
  }
#else
  if (capable->has_gconftool_2) {
    com = lives_strdup_printf("%s --set --type bool /apps/gnome-screensaver/"
                              "idle_activation_enabled true 2>%s;",
                              EXEC_GCONFTOOL_2, LIVES_DEVNULL);
  } else com = lives_strdup("");
#endif

  lives_hook_remove(enable_ss_lpt);

  if (com) {
    lives_cancel_t cancelled = mainw->cancelled;
    lives_system(com, TRUE);
    lives_free(com);
    mainw->cancelled = cancelled;
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
    }
    return TRUE;
  }
  return FALSE;
}


boolean lives_disable_screensaver(void) {
  char *com = NULL, *tmp;

#ifdef GDK_WINDOWING_X11
  uint64_t awinid = lives_xwindow_get_xwinid(capable->wm_caps.root_window, NULL);

  com = lives_strdup_printf("%s s off 2>%s; %s -dpms 2>%s;",
                            EXEC_XSET, LIVES_DEVNULL, EXEC_XSET, LIVES_DEVNULL);

  if (capable->has_gconftool_2) {
    char *xnew = lives_strdup_printf(" %s --set --type bool /apps/gnome-screensaver/"
                                     "idle_activation_enabled false 2>%s;",
                                     EXEC_GCONFTOOL_2, LIVES_DEVNULL);
    tmp = lives_concat(com, xnew);
    com = tmp;
  }
  if (capable->has_xdg_screensaver && awinid) {
    char *xnew = lives_strdup_printf(" %s suspend %" PRIu64 " 2>%s;",
                                     EXEC_XDG_SCREENSAVER, awinid, LIVES_DEVNULL);
    tmp = lives_concat(com, xnew);
    com = tmp;
  }
#else
  if (capable->has_gconftool_2) {
    com = lives_strdup_printf("%s --set --type bool /apps/gnome-screensaver/"
                              "idle_activation_enabled false 2>%s;",
                              EXEC_GCONFTOOL_2, LIVES_DEVNULL);
  } else com = lives_strdup("");
#endif

  if (com) {
    lives_cancel_t cancelled = mainw->cancelled;
    lives_system(com, TRUE);
    lives_free(com);
    mainw->cancelled = cancelled;
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
    } else {
      enable_ss_lpt = lives_hook_prepend_full(mainw->global_hook_stacks, FATAL_HOOK,
                                              0, enable_ss_cb, 0, "", NULL);
    }
    return TRUE;
  }
  return FALSE;
}


boolean hide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return NULL;
#endif
  if (!wid) return FALSE;
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    cmd = lives_strdup_printf("%s windowminimize \"%s\"", EXEC_XDOTOOL, wid);
    return mini_run(cmd);
  }
  return FALSE;
}


boolean unhide_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (!wid) return FALSE;
  if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
    cmd = lives_strdup_printf("%s windowmap \"%s\"", EXEC_XDOTOOL, wid);
  return mini_run(cmd);
}


boolean activate_x11_window(const char *wid) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (!wid) return FALSE;

  if (capable->has_xdotool != MISSING) {
    if (check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL))
      cmd = lives_strdup_printf("%s windowactivate \"%s\" --sync", EXEC_XDOTOOL, wid);
  } else if (capable->has_wmctrl != MISSING) {
    if (check_for_executable(&capable->has_wmctrl, EXEC_WMCTRL))
      cmd = lives_strdup_printf("%s -Fa \"%s\"", EXEC_WMCTRL, wid);
  } else return FALSE;
  return mini_run(cmd);
}


// TODO: xprop -name "title"

char *wm_property_get(const char *key, int *type_guess) {
  char *com, *val = NULL, *res = NULL;
  if (check_for_executable(&capable->has_gsettings, EXEC_GSETTINGS) == PRESENT) {
    com = lives_strdup_printf("%s list-recursively | %s %s", EXEC_GSETTINGS, EXEC_GREP, key);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      if (res) lives_free(res);
      return NULL;
    }
    if (!res) return NULL;
    if (!*res) {
      lives_free(res);
      return NULL;
    } else {
      char *separ = lives_strdup_printf(" %s ", key);
      char **array = lives_strsplit(res, separ, 2);
      lives_free(separ);
      lives_free(res);
      if (*array[1] == '\'') {
        val = lives_strndup(array[1] + 1, lives_strlen(array[1]) - 2);
        if (type_guess) *type_guess = WEED_SEED_STRING;
      } else {
        val = lives_strdup(array[1]);
        if (type_guess) {
          if (!lives_strcmp(val, "true") || !lives_strcmp(val, "false")) {
            *type_guess = WEED_SEED_BOOLEAN;
          }
          if (atoi(val)) {
            *type_guess = WEED_SEED_INT;
	    // *INDENT-OFF*
	  }}}
      lives_strfreev(array);
    }}
  // *INDENT-OFF*
  return val;
}


boolean wm_property_set(const char *key, const char *val) {
  char *com, *res = NULL;
  if (check_for_executable(&capable->has_gsettings, EXEC_GSETTINGS) == PRESENT) {
    com = lives_strdup_printf("%s list-recursively | %s %s", EXEC_GSETTINGS, EXEC_GREP, key);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      THREADVAR(com_failed) = FALSE;
      if (res) lives_free(res);
      return FALSE;
    }
    if (!res) return FALSE;
    if (!*res) {
      lives_free(res);
      return FALSE;
    }
    else {
      char *separ = lives_strdup_printf(" %s ", key);
      char **array = lives_strsplit(res, separ, 2);
      char *schema = array[0];
      lives_free(separ);
      lives_free(res);
      com = lives_strdup_printf("%s set %s %s %s", EXEC_GSETTINGS, schema, key, val);
      g_print("COM is %s\n", com);
      lives_system(com, TRUE);
      lives_free(com);
      lives_strfreev(array);
    }}
  // *INDENT-OFF*
  return TRUE;
}


boolean get_wm_caps(void) {
  char *wmname;
  if (capable->has_wm_caps) return TRUE;
  capable->has_wm_caps = TRUE;

#if IS_MINGW
  capable->wm_caps.is_composited = TRUE;
  capable->wm_caps.root_window = gdk_screen_get_root_window(mainw->mgeom[widget_opts.monitor].screen);
#else
#ifdef GUI_GTK
  capable->wm_caps.is_composited = gdk_screen_is_composited(mainw->mgeom[widget_opts.monitor].screen);
  capable->wm_caps.root_window = gdk_screen_get_root_window(mainw->mgeom[widget_opts.monitor].screen);
#else
  capable->wm_caps.is_composited = FALSE;
  capable->wm_caps.root_window = NULL;
#endif
#endif

  capable->wm_caps.wm_focus = wm_property_get("focus-new-windows", NULL);

  capable->wm_type = getenv(XDG_SESSION_TYPE);
  wmname = getenv(XDG_CURRENT_DESKTOP);

  if (!wmname) {
    if (capable->wm_name) wmname = capable->wm_name;
  }
  if (!wmname) return FALSE;

  capable->has_wm_caps = TRUE;
  lives_snprintf(capable->wm_caps.wm_name, 64, "%s", wmname);

  if (!strcmp(capable->wm_caps.wm_name, WM_XFWM4) || !strcmp(capable->wm_name, WM_XFWM4)) {
    lives_snprintf(capable->wm_caps.panel, 64, "%s", WM_XFCE4_PANEL);
    capable->wm_caps.pan_annoy = ANNOY_DISPLAY | ANNOY_FS;
    capable->wm_caps.pan_res = RES_HIDE | RESTYPE_ACTION;
    lives_snprintf(capable->wm_caps.ssave, 64, "%s", WM_XFCE4_SSAVE);
    lives_snprintf(capable->wm_caps.color_settings, 64, "%s", WM_XFCE4_COLOR);
    lives_snprintf(capable->wm_caps.display_settings, 64, "%s", WM_XFCE4_DISP);
    lives_snprintf(capable->wm_caps.ssv_settings, 64, "%s", WM_XFCE4_SSAVE);
    lives_snprintf(capable->wm_caps.pow_settings, 64, "%s", WM_XFCE4_POW);
    lives_snprintf(capable->wm_caps.settings, 64, "%s", WM_XFCE4_SETTINGS);
    lives_snprintf(capable->wm_caps.term, 64, "%s", WM_XFCE4_TERMINAL);
    lives_snprintf(capable->wm_caps.taskmgr, 64, "%s", WM_XFCE4_TASKMGR);
    lives_snprintf(capable->wm_caps.sshot, 64, "%s", WM_XFCE4_SSHOT);
    return TRUE;
  }
  if (!strcmp(capable->wm_caps.wm_name, WM_KWIN) || !strcmp(capable->wm_name, WM_KWIN)) {
    lives_snprintf(capable->wm_caps.panel, 64, "%s", WM_KWIN_PANEL);
    lives_snprintf(capable->wm_caps.ssave, 64, "%s", WM_KWIN_SSAVE);
    lives_snprintf(capable->wm_caps.color_settings, 64, "%s", WM_KWIN_COLOR);
    lives_snprintf(capable->wm_caps.display_settings, 64, "%s", WM_KWIN_DISP);
    lives_snprintf(capable->wm_caps.ssv_settings, 64, "%s", WM_KWIN_SSAVE);
    lives_snprintf(capable->wm_caps.pow_settings, 64, "%s", WM_KWIN_POW);
    lives_snprintf(capable->wm_caps.settings, 64, "%s", WM_KWIN_SETTINGS);
    lives_snprintf(capable->wm_caps.term, 64, "%s", WM_KWIN_TERMINAL);
    lives_snprintf(capable->wm_caps.taskmgr, 64, "%s", WM_KWIN_TASKMGR);
    lives_snprintf(capable->wm_caps.sshot, 64, "%s", WM_KWIN_SSHOT);
    return TRUE;
  }
  return FALSE;
}


int get_window_stack_level(LiVESXWindow *xwin, int *nwins) {
#ifndef GUI_GTK
  if (nwins) *nwins = -1;
  return -1;
#else
  int mywin = -1, i = 0;
  LiVESList *winlist = gdk_screen_get_window_stack(mainw->mgeom[widget_opts.monitor].screen),
    *list = winlist;
  for (; list; list = list->next, i++) {
    if ((LiVESXWindow *)list->data == xwin) mywin = i;
    lives_widget_object_unref(list->data);
  }
  lives_list_free(winlist);
  if (nwins) *nwins = ++i;
  return mywin;
#endif
}


static lives_proc_thread_t show_dpanel_lpt = NULL;

static boolean show_dpanel_cb(void) {
  show_desktop_panel();
  return FALSE;
}


boolean show_desktop_panel(void) {
  boolean ret = FALSE;
#ifdef GDK_WINDOWING_X11
  char *wid = get_wid_for_name(capable->wm_caps.panel);
  if (wid) {
    ret = unhide_x11_window(wid);
    lives_free(wid);
    lives_hook_remove(show_dpanel_lpt);
  }
#endif
  return ret;
}


boolean hide_desktop_panel(void) {
  boolean ret = FALSE;
#ifdef GDK_WINDOWING_X11
  char *wid = get_wid_for_name(capable->wm_caps.panel);
  if (wid) {
    ret = hide_x11_window(wid);
    lives_free(wid);
    show_dpanel_lpt = lives_hook_prepend_full(mainw->global_hook_stacks, FATAL_HOOK,
					      0, show_dpanel_cb, 0, "", NULL);
  }
#endif
  return ret;
}


boolean get_x11_visible(const char *wname) {
  char *cmd = NULL;
#ifndef GDK_WINDOWING_X11
  return FALSE;
#endif
  if (!wname || !*wname) return FALSE;
  if (check_for_executable(&capable->has_xwininfo, EXEC_XWININFO)) {
    char *state;
    cmd = lives_strdup_printf("%s -name \"%s\"", EXEC_XWININFO, wname);
    state = grep_in_cmd(cmd, 2, -1, "Map State:", 4, 1, FALSE);
    lives_free(cmd);
    if (state && !strcmp(state, "IsViewable")) {
      lives_free(state);
      return TRUE;
    }
  }
  if (wname && check_for_executable(&capable->has_xdotool, EXEC_XDOTOOL)) {
    char buff[65536];
    size_t nlines;

    // returns a list, and we need to check each one
    cmd = lives_strdup_printf("%s search --all --onlyvisible \"%s\" 2>/dev/null", EXEC_XDOTOOL, wname);
    lives_popen(cmd, FALSE, buff);
    lives_free(cmd);
    if (THREADVAR(com_failed)
	|| (!*buff || !(nlines = get_token_count(buff, '\n')))) {
      if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    }
    else {
      char *wid = get_wid_for_name(wname);
      if (wid) {
	int l;
	char **lines = lives_strsplit(buff, "\n", nlines), *xwid;
	for (l = 0; l < nlines; l++) {
	  if (!*lines[l]) continue;
	  xwid = lives_strdup_printf("0x%08lX", atol(lines[l]));
	  if (!strcmp(xwid, wid)) break;
	}
	lives_strfreev(lines);
	lives_free(wid);
	if (l < nlines)  return TRUE;
      }
    }
  }
  return FALSE;
}

#define XTEMP "XXXXXXXXXX"

static char *get_systmp_inner(const char *suff, boolean is_dir, const char *prefix) {
  /// create a file or dir in /tmp or prefs->workdir
  /// check the name returned has the length we expect
  /// check it was created
  /// ensure it is not a symlink
  /// if a directory, ensure we have rw access
  char *res = NULL;

  if (!check_for_executable(&capable->has_mktemp, EXEC_MKTEMP)) return NULL;
  else {
    size_t slen;
    char *tmp, *com;
    const char *dirflg, *tmpopt;
    if (!prefix) {
      // no prefix, create in $TMPDIR
      if (suff) tmp = lives_strdup_printf("lives-%s-%s", XTEMP, suff);
      else tmp = lives_strdup_printf("lives-%s", XTEMP);
      tmpopt = "t";
      slen = lives_strlen(tmp) + 2;
    }
    else {
      /// suff here is the directory name
      char *tmpfile = lives_strdup_printf("%s%s", prefix, XTEMP);
      tmp = lives_build_filename(suff, tmpfile, NULL);
      lives_free(tmpfile);
      tmpopt = "";
      slen = lives_strlen(tmp);
    }

    if (is_dir) dirflg = "d";
    else dirflg = "";

    com = lives_strdup_printf("%s -n $(%s -q%s%s \"%s\")", capable->echo_cmd, EXEC_MKTEMP, tmpopt,
			      dirflg, tmp);
    lives_free(tmp);
    res = mini_popen(com);
    if (THREADVAR(com_failed)) {
      if (res) lives_free(res);
      THREADVAR(com_failed) = FALSE;
      return NULL;
    }
    if (!res) return NULL;
    if (lives_strlen(res) < slen) {
      lives_free(res);
      return NULL;
    }
  }
  if (!lives_file_test(res, LIVES_FILE_TEST_EXISTS)
      || lives_file_test(res, LIVES_FILE_TEST_IS_SYMLINK)) {
    lives_free(res);
    return NULL;
  }
  if (is_dir) {
    if (!is_writeable_dir(res)) {
      lives_free(res);
      return NULL;
    }
  }
  return res;
}


LIVES_GLOBAL_INLINE char *get_systmp(const char *suff, boolean is_dir)
{return get_systmp_inner(suff, is_dir, NULL);}


static char *_get_worktmp(const char *prefix, boolean is_dir) {
  char *dirname = NULL;
  char *tmpdir = get_systmp_inner(prefs->workdir, is_dir, prefix);
  if (tmpdir) {
    dirname = lives_path_get_basename(tmpdir);
    lives_free(tmpdir);
  }
  return dirname;
}

LIVES_GLOBAL_INLINE char *get_worktmp(const char *prefix) {
  if (!prefix) return NULL;
  return _get_worktmp(prefix, TRUE);
}

LIVES_GLOBAL_INLINE char *get_worktmpfile(const char *prefix) {
  if (!prefix) return NULL;
  return _get_worktmp(prefix, FALSE);
}


LIVES_GLOBAL_INLINE char *get_localsharedir(const char *subdir) {
  char *localshare;
  if (!capable->xdg_data_home) capable->xdg_data_home = getenv(XDG_DATA_HOME);
  if (!capable->xdg_data_home) capable->xdg_data_home = lives_strdup("");
  if (!*capable->xdg_data_home)
    localshare = lives_build_path(capable->home_dir, LOCAL_HOME_DIR, LIVES_SHARE_DIR, subdir, NULL);
  else localshare = lives_build_path(capable->xdg_data_home, subdir, NULL);
  return localshare;
}


boolean notify_user(const char *detail) {
  if (check_for_executable(&capable->has_notify_send, EXEC_NOTIFY_SEND)) {
    char *msg = lives_strdup_printf("\n\n%s-%s says '%s completed\n\n'",
				    lives_get_application_name(), LiVES_VERSION, detail);
    char *cmd = lives_strdup_printf("%s \"%s\"", EXEC_NOTIFY_SEND, msg);
    boolean ret = mini_run(cmd);
    lives_free(msg);
    if (THREADVAR(com_failed)) THREADVAR(com_failed) = FALSE;
    return ret;
  }
  return FALSE;
}


boolean get_distro_dets(void) {
#if !IS_LINUX_GNU
  capable->distro_name = lives_strdup(capable->os_name);
  capable->distro_ver = lives_strdup(capable->os_release);
#else
#define LSB_OS_FILE "/etc/lsb-release"
  char *com = lives_strdup_printf("%s %s", capable->cat_cmd, LSB_OS_FILE), *ret;
  if ((ret = mini_popen(com))) {
    int xlen = get_token_count(ret, '=');
    char **array = lives_strsplit(ret, "=", xlen);
    lives_free(ret);
    if (xlen > 1) {
      lives_strstop(array[1], '\n');
      capable->distro_name = lives_strdup(array[1]);
      if (xlen > 2) {
	lives_strstop(array[2], '\n');
	capable->distro_ver = lives_strdup(array[2]);
	if (xlen > 3) {
	  lives_strstop(array[3], '\n');
	  capable->distro_codename = lives_strdup(array[3]);
	}}}
    lives_strfreev(array);
    return TRUE;
  }
#endif
  return FALSE;
}


#ifdef GUI_GTK
static double get_screen_scale(GdkScreen *screen, double *pdpi) {
  double scale = 1.0;
  double dpi = gdk_screen_get_resolution(screen);
  if (dpi == 120.) scale = 1.25;
  else if (dpi == 144.) scale = 1.5;
  else if (dpi == 192.) scale = 2.0;
  if (pdpi) *pdpi = dpi;
  return scale;
}
#endif


void get_monitors(boolean reset) {
#ifdef GUI_GTK
  GdkDisplay *disp;
  GdkScreen *screen;
#if GTK_CHECK_VERSION(3, 22, 0)
  GdkMonitor *moni;
#endif
  GdkRectangle rect;
  GdkDevice *device;
  double scale, dpi;
  int play_moni = 1;
  char buff[256];
#if !GTK_CHECK_VERSION(3, 22, 0)
  GSList *dlist, *dislist;
  int nscreens, nmonitors;
#if LIVES_HAS_DEVICE_MANAGER
  GdkDeviceManager *devman;
  LiVESList *devlist;
  int k;
#endif
  int i, j;
#endif
  int idx = 0;

  if (mainw->ignore_screen_size) return;

  lives_freep((void **)&mainw->mgeom);
  capable->nmonitors = 0;

#if !GTK_CHECK_VERSION(3, 22, 0)
  dlist = dislist = gdk_display_manager_list_displays(gdk_display_manager_get());
  // gdk_display_manager_get_default_display(gdk_display_manager_get());
  // for each display get list of screens

  while (dlist) {
    disp = (GdkDisplay *)dlist->data;

    // get screens
    nscreens = lives_display_get_n_screens(disp);
    for (i = 0; i < nscreens; i++) {
      screen = gdk_display_get_screen(disp, i);
      capable->nmonitors += gdk_screen_get_n_monitors(screen);
    }
    dlist = dlist->next;
  }
#else
  disp = gdk_display_get_default();
  capable->nmonitors += gdk_display_get_n_monitors(disp);
#endif

  mainw->mgeom = (lives_mgeometry_t *)lives_calloc(capable->nmonitors, sizeof(lives_mgeometry_t));


#if !GTK_CHECK_VERSION(3, 22, 0)
  dlist = dislist;

  while (dlist) {
    disp = (GdkDisplay *)dlist->data;

#if LIVES_HAS_DEVICE_MANAGER
    devman = gdk_display_get_device_manager(disp);
    devlist = gdk_device_manager_list_devices(devman, GDK_DEVICE_TYPE_MASTER);
#endif
    // get screens
    nscreens = lives_display_get_n_screens(disp);
    for (i = 0; i < nscreens; i++) {
      screen = gdk_display_get_screen(disp, i);
      scale = get_screen_scale(screen, &dpi);
      nmonitors = gdk_screen_get_n_monitors(screen);
      for (j = 0; j < nmonitors; j++) {
        gdk_screen_get_monitor_geometry(screen, j, &(rect));
        mainw->mgeom[idx].x = rect.x;
        mainw->mgeom[idx].y = rect.y;
        mainw->mgeom[idx].width = mainw->mgeom[idx].phys_width = rect.width;
        mainw->mgeom[idx].height = mainw->mgeom[idx].phys_height = rect.height;
        mainw->mgeom[idx].mouse_device = NULL;
        mainw->mgeom[idx].dpi = dpi;
        mainw->mgeom[idx].scale = scale;
#if GTK_CHECK_VERSION(3, 4, 0)
        gdk_screen_get_monitor_workarea(screen, j, &(rect));
        mainw->mgeom[idx].width = rect.width;
#endif
	mainw->mgeom[idx].height = ((rect.height + 1) >> 1) << 1;
#if LIVES_HAS_DEVICE_MANAGER
        // get (virtual) mouse device for this screen
        for (k = 0; k < lives_list_length(devlist); k++) {
          device = (GdkDevice *)lives_list_nth_data(devlist, k);
          if (gdk_device_get_display(device) == disp &&
              gdk_device_get_source(device) == GDK_SOURCE_MOUSE) {
            mainw->mgeom[idx].mouse_device = device;
            break;
          }
        }
#endif
        mainw->mgeom[idx].disp = disp;
        mainw->mgeom[idx].screen = screen;
        idx++;
        if (idx >= capable->nmonitors) break;
      }
    }
#if LIVES_HAS_DEVICE_MANAGER
    lives_list_free(devlist);
#endif
    dlist = dlist->next;
  }

  lives_slist_free(dislist);
#else
  screen = gdk_display_get_default_screen(disp);
  scale = get_screen_scale(screen, &dpi);
  device = gdk_seat_get_pointer(gdk_display_get_default_seat(disp));
  for (idx = 0; idx < capable->nmonitors; idx++) {
    mainw->mgeom[idx].disp = disp;
    mainw->mgeom[idx].monitor = moni = gdk_display_get_monitor(disp, idx);
    mainw->mgeom[idx].screen = screen;
    gdk_monitor_get_geometry(moni, (GdkRectangle *)&rect);
    mainw->mgeom[idx].x = rect.x;
    mainw->mgeom[idx].y = rect.y;
    mainw->mgeom[idx].phys_width = rect.width;
    mainw->mgeom[idx].phys_height = rect.height;
    mainw->mgeom[idx].mouse_device = device;
    mainw->mgeom[idx].dpi = dpi;
    mainw->mgeom[idx].scale = scale;
    gdk_monitor_get_workarea(moni, &(rect));
    mainw->mgeom[idx].width = rect.width;
    mainw->mgeom[idx].height = ((rect.height + 1) >> 1) << 1;
    if (gdk_monitor_is_primary(moni)) {
      capable->primary_monitor = idx;
      mainw->mgeom[idx].primary = TRUE;
    } else if (play_moni == 1) play_moni = idx + 1;
  }
#endif
#endif

  if (prefs->force_single_monitor) capable->nmonitors = 1; // force for clone mode

  if (!reset) return;

  prefs->gui_monitor = 0;
  prefs->play_monitor = play_moni;

  if (capable->nmonitors > 1) {
    get_string_pref(PREF_MONITORS, buff, 256);

    if (*buff && get_token_count(buff, ',') > 1) {
      char **array = lives_strsplit(buff, ",", 2);
      prefs->gui_monitor = atoi(array[0]);
      prefs->play_monitor = atoi(array[1]);
      lives_strfreev(array);
    }

    if (prefs->gui_monitor < 1) prefs->gui_monitor = 1;
    if (prefs->play_monitor < 0) prefs->play_monitor = 0;
    if (prefs->gui_monitor > capable->nmonitors) prefs->gui_monitor = capable->nmonitors;
    if (prefs->play_monitor > capable->nmonitors) prefs->play_monitor = capable->nmonitors;
  }

  /* /\* if (prefs->gui_monitor >= capable->nmonitors) *\/ */
  /* /\*   prefs->gui_monitor = 0; *\/ */
  /* /\* if (prefs->play_monitor >= capable->nmonitors) *\/ */
  /* /\*   prefs->play_monitor = capable->nmonitors - 1; *\/ */

  widget_opts.monitor = prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : capable->primary_monitor;

  mainw->old_scr_width = GUI_SCREEN_WIDTH;
  mainw->old_scr_height = GUI_SCREEN_HEIGHT;

  prefs->screen_scale = get_double_prefd(PREF_SCREEN_SCALE, 0.);
  if (prefs->screen_scale == 0.) {
    prefs->screen_scale = (double)GUI_SCREEN_WIDTH / (double)SCREEN_SCALE_DEF_WIDTH;
    prefs->screen_scale = (prefs->screen_scale - 1.) * 1.5 + 1.;
  }

  widget_opts_set_scale(prefs->screen_scale);
}


///////////////// future directions --- this will become part of the performance manager object //////
// the idea is to have it continually montioring the hardware and optimising for memory, io, disksapce
// cpu load, temperature, battery status and so on
// as well as managing the autotuning parameters which should be rechecked periodically
// also it would be good to have other objects feeding data into it, for example frame decode times
// av sync drift, instant fps, widget event calls, as well as timing data for running key functions
//
// in addition it can manage the adaptive quality settings from the input data, and also adjust the
// amount of processing devoted to each thread, using


void set_thread_loveliness(uint64_t tuid, double howmuch) {
  // set the loveliness for a thread, a prettier one can run faster than a more homely instance
  lives_thread_data_t *tdata = get_thread_data_by_uid(tuid);
  if (tdata) tdata->vars.var_loveliness = howmuch;
}

int get_num_cpus(void) {
#ifdef IS_DARWIN
  kerr = host_processor_info(mach_host_self(), PROCESSOR_BASIC_INFO, &numProcessors, &processorInfo, &numProcessorInfo);
  if (kerr == KERN_SUCCESS) {
    vm_deallocate(mach_task_self(), (vm_address_t) processorInfo, numProcessorInfo * sizint);
  }
  return numProcessors;
#else
  char buffer[1024];
  char command[PATH_MAX];
#ifdef _SC_NPROCESSORS_ONLN
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (num_cores > 0) return num_cores;
#endif
#if IS_FREEyBSD
  lives_snprintf(command, PATH_MAX, "sysctl -n kern.smp.cpus");
#else
  lives_snprintf(command, PATH_MAX, "%s processor /proc/cpuinfo 2>/dev/null | %s -l 2>/dev/null",
                 capable->grep_cmd, capable->wc_cmd);
#endif
  lives_popen(command, TRUE, buffer);
  return atoi(buffer);
#endif
}


boolean get_machine_dets(int phase) {
  struct timespec res;
  if (phase == 0) {
    cache_msg("\n");
#ifdef _SC_PAGESIZE
    capable->hw.pagesize = sysconf(_SC_PAGESIZE);
    cache_msg("Memory page size is %d\n", capable->hw.pagesize);
#endif
#if IS_X86_64
    get_cpuinfo();
    cache_msg("CPU cacheline size is %d\n", capable->hw.cacheline_size);
#else
    capable->hw.cacheline_size = capable->hw.cpu_bits;
    cache_msg("CPU cacheline size is %d (guessed from CPU bits)\n",
	      capable->hw.cacheline_size);
#endif

  if (!clock_getres(CLOCK_REALTIME, &res))
    capable->hw.rt_clock_res = res.tv_sec * ONE_BILLION + res.tv_nsec;
  if (!clock_getres(CLOCK_MONOTONIC, &res))
    capable->hw.mono_clock_res = res.tv_sec * ONE_BILLION + res.tv_nsec;
  if (capable->hw.rt_clock_res)
    cache_msg("Clock resolution is %s\n",
	      lives_format_timing_string((double)capable->hw.rt_clock_res / ONE_BILLION_DBL));
    return TRUE;
  }

#if IS_FREEBSD
  char *com = lives_strdup("sysctl -n hw.model");
#else
  char *com = lives_strdup_printf("%s -m1 \"^model name\" /proc/cpuinfo | "
				  "%s -e \"s/.*: //\" -e \"s:\\s\\+:/:g\"",
				  capable->grep_cmd, capable->sed_cmd);
#endif

  capable->hw.cpu_name = mini_popen(com);

  com = lives_strdup("uname -o");
  capable->os_name = mini_popen(com);

  com = lives_strdup("uname -r");
  capable->os_release = mini_popen(com);

  com = lives_strdup("uname -m");
  capable->os_hardware = mini_popen(com);

  com = lives_strdup("uname -n");
  capable->mach_name = mini_popen(com);

  com = lives_strdup("whoami");
  capable->username = mini_popen(com);

  if (!mainw->debug && !strcmp(capable->os_hardware, "x86_64"))
    get_cpuinfo();

  if (THREADVAR(com_failed)) {
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  }
  return TRUE;
}


boolean parse_valfile(const char *fname, const char delim, const char **keys, char **vals) {
  // parse fname - read it line by line, in each line we split at delim
  // if part[0] == a key, then we assign part[1] (with whitespace trimmed) in corresponding val
  // key is a NULL terminated list
  char buffer[8192];
  size_t ntok;
  const char dstr[2] = {delim, 0};
  FILE *file = fopen(fname, "r");
  if (!file) return -1;
  while (1) {
    if (!fgets(buffer, 8192, file)) {
      boolean bret = !!feof(file);
      fclose(file);
      return bret;
    }
    ntok = get_token_count(buffer, delim);
    if (ntok > 1) {
      char **array = lives_strsplit(buffer, dstr, ntok);
      for (int i = 0; keys[i]; i++) {
	if (!lives_strcmp(keys[i], array[0])) {
	  vals[i] = lives_strdup(lives_strstrip(array[1]));
	}
      }
      lives_strfreev(array);
    }
  }
  // should never reach here
  fclose(file);
  return TRUE;
}


#define PROC_MEMINFO "/proc/meminfo"
#define MEM_CRIT 100000

boolean get_memstatus(void) {
#if !IS_LINUX_GNU
  char *rets[4];
  const char *valx[] = {"MemTotal", "MemFree", "MemAvailable", "Mlocked", NULL};
  for (int i = 0; valx[i]; i++) rets[i] = NULL;
  bret = parse_valfile(PROC_MEMINFO, ':', valx, rets);
  capable->hw.memtotall = lives_strtol(rets[0]);
  capable->hw.memfree = lives_strtol(rets[1]);
  capable->hw.memavail = lives_strtol(rets[2]);
  capable->hw.memlocked = lives_strtol(rets[3]);
  for (int i = 0; valx[i]; i++) if (rets[i]) lives_free(rets[i]);
  return TRUE;
#else
  return FALSE;
#endif
}


boolean check_mem_status(void) {
  boolean bret;
  int64_t memavail;
  if (!get_memstatus()) return LIVES_STORAGE_STATUS_UNKNOWN;
  memavail = capable->hw.memavail;
  if (memavail < MEM_CRIT) {
    // almost oom...backup all we can just in case...
    capable->hw.mem_status = LIVES_STORAGE_STATUS_CRITICAL;
    if (LIVES_IS_PLAYING && mainw->record) {
      if (prefs->crash_recovery && prefs->rr_crash) {
	lives_proc_thread_create(LIVES_THRDATTR_NO_GUI | LIVES_THRDATTR_PRIORITY,
				 (lives_funcptr_t)backup_recording, 0, "vv", NULL, NULL);
      }
    }
    for (LiVESList *list = mainw->cliplist; list; list = list->next) {
      int clpno = LIVES_POINTER_TO_INT(list->data);
      if (IS_NORMAL_CLIP(clpno)) {
	if (!mainw->files[clpno]->tsavedone) {
	  char *fname = lives_build_filename(prefs->workdir, mainw->files[clpno]->handle,
					     "." TOTALSAVE_NAME, NULL);
	  int fd = lives_create_buffered(fname, DEF_FILE_PERMS);
	  if (fd >= 0) {
	    lives_write_buffered(fd, (const char *)mainw->files[clpno], sizeof(lives_clip_t), TRUE);
	    lives_close_buffered(fd);
	  }
	  lives_free(fname);
	  mainw->files[clpno]->tsavedone = TRUE;
	}
      }
    }
  }
  else {
    if (capable->hw.mem_status) {
      if (memavail > MEM_CRIT) capable->hw.mem_status = LIVES_STORAGE_STATUS_WARNING;
      if (memavail > MEM_CRIT * 2) capable->hw.mem_status = LIVES_STORAGE_STATUS_NORMAL;
    }
  }
  return bret;
}


double get_disk_load(const char *mp) {
  // not really working yet...
  if (0 && !mp) return -1.;
  else {
#if !IS_LINUX_GNU
    return 0.;
#else
#define VM_STATS_FILE "/proc/vmstat"
#define _VM_SEEKSTR_ "pgpg"
#define INPART_ "pgpgin"
#define OUTPART_ "pgpgout"
    static int64_t linval = -1, loutval = -1;
    static ticks_t lticks = 0;
    int64_t inval = -1, outval = -1;
    ticks_t clock_ticks;
    double inret = -1., outret = -1.;
    char *res;
    char *com = lives_strdup_printf("%s -n $(%s %s %s)", capable->echo_cmd,
				    capable->grep_cmd, _VM_SEEKSTR_, VM_STATS_FILE);
    if ((res = mini_popen(com))) {
      g_print("RES was %s\n", res);
      int xlines = get_token_count(res, '\n');
      char **xarray = lives_strsplit(res, "\n", xlines);
      for (int i = 0; i < xlines; i++) {
	if (!xarray[i] || !*xarray[i]) continue;
	else {
	  size_t xbits = get_token_count(xarray[i], ' ');
	  if (xbits > 1) {
	    char **array = lives_strsplit(xarray[i], " ", xbits);
	    for (int j = 0; j < xbits - 1; j++) {
	      g_print("checking %s\n", array[j]);
	      if (!lives_strcmp(array[j], INPART_))
		inval = lives_strtol(array[j + 1]);
	      else if (!lives_strcmp(array[j], OUTPART_))
		outval = lives_strtol(array[j + 1]);
	      if (inval > -1 && outval > -1) break;
	    }
	    lives_strfreev(array);
	  }
	}
      }
      lives_free(res);
      lives_strfreev(xarray);
      if (LIVES_IS_PLAYING) clock_ticks = mainw->clock_ticks;
      else clock_ticks = lives_get_current_ticks();
      if (lticks > 0 && clock_ticks > lticks) {
	if (inval > linval) {
	  inret = (double)(inval - linval) / ((double)(clock_ticks - lticks) / TICKS_PER_SECOND_DBL);
	  linval = inval;
	}
	if (outval > loutval) {
	  outret = (double)(outval - loutval) / ((double)(clock_ticks - lticks) / TICKS_PER_SECOND_DBL);
	  loutval = outval;
	}

	g_print("DISK PRESS: %f %f %f\n", inret, outret,
		((double)(clock_ticks - lticks) / TICKS_PER_SECOND_DBL));
      }
      lticks = clock_ticks;
    }
  }
  return 0.;
#endif
}


LIVES_GLOBAL_INLINE double check_disk_pressure(double current) {
  return 0.;
}


int set_thread_cpuid(pthread_t pth) {
  // doesn't seem particularly useful - the kernel does a pretty good job
  // - code retained anyway, in case it becomes useful one day
#ifdef CPU_ZERO
  cpu_set_t cpuset;
  int mincore = 0;
  float minload = 1000.;
  int ret = 0;
  for (int i = 1; i <= capable->hw.ncpus; i++) {
    float load = *(get_core_loadvar(i));
    if (load > 0. && load < minload) {
      minload = load;
      mincore = i;
    }
  }
  if (mincore) {
    int err;
    CPU_ZERO(&cpuset);
    CPU_SET(--mincore, &cpuset);
    err = pthread_setaffinity_np(pth, sizeof(cpu_set_t), &cpuset);
    if (err) {LIVES_ERROR("pthread aff error");}
    else {
      ret = mincore;
    }
  }
#endif
  return ret;
}


typedef struct {
  uint64_t tot, idlet;
  int64_t ret;
} oldvalues;

static LiVESList *cpuloadlist = NULL;

#define CPU_STATS_FILE "/proc/stat"

static boolean get_cpu_loads(cpuloadvals_t *loadvals, int ncpus) {
  /// gets reported load for all CPUs (%)
  /// and boot time
  // returns FALSE if a value cannot be read
  oldvalues *ovals = NULL;
  FILE *file;
  char buffer[1024];
  unsigned long long boottime = 0;
  unsigned long long user = 0, nice = 0, system = 0, idle = 0;
  unsigned long long iowait = 0, irq = 0, softirq = 0, steal = 0;
  float load = 0.;
  uint64_t idlet, sum, tot;
  int xcpun = 0;

  if (!lives_file_test(CPU_STATS_FILE, LIVES_FILE_TEST_EXISTS)) return -1;

  file = fopen(CPU_STATS_FILE, "r");
  if (!file) return FALSE;

  if (!fgets(buffer, 1024, file)) goto err;

  while (1) {
    if (xcpun == ncpus && loadvals->boottime) break;

    if (!fgets(buffer, 1024, file)) {
      if (ferror(file)) goto err;
      break;
    }
    if (xcpun < ncpus) {
      if (sscanf(buffer,
		 "cpu  %16llu %16llu %16llu %16llu %16llu %16llu %16llu %16llu %*s %*s\n",
		 &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal) > 0) {
	idlet = idle + iowait;
	sum = user + nice + system + irq + softirq + steal;
	tot = sum + idlet;

	if (idx_list_get_data(cpuloadlist, xcpun, (void **)&ovals)) {
	  if (tot != ovals->tot) {
	    float totd = (float)(tot - ovals->tot);
	    float idled = (float)(idlet - ovals->idlet);
	    load = (totd - idled) / totd;
	  }
	  else load = ovals->ret;
	}
	else ovals = (oldvalues *)lives_malloc(sizeof(oldvalues));
	ovals->tot = tot;
	ovals->idlet = idlet;
	ovals->ret = load;
	cpuloadlist = idx_list_update(cpuloadlist, xcpun, ovals);
	loadvals->loads[xcpun++] = load * 100.;
	continue;
      }
    }
    if (!loadvals->boottime) {
      if (sscanf(buffer, "btime %16llu", &boottime) > 0) {
	loadvals->boottime = boottime;
      }
    }
  }
  fclose(file);
  return TRUE;

 err:
  fclose(file);
  return FALSE;
}

#define N_CPU_MEAS 64
#define CPU_MEAS_THRESH 1000000

static cpuloadvals_t *cpu_stats = NULL;

float *get_proc_loads(boolean reset) {
  // get processor load values, and keep a rolling average
  static cpuloadvals_t cpuvals;
  static tab_data_t *cpuloadtab = NULL;
  static ticks_t ltime = 0;
  ticks_t ctime;

  if (!cpuloadtab) {
    cpuvals.loads =
      (float *)lives_calloc(capable->hw.ncpus, sizeof(float));
    cpuvals.avgs =
      (float *)lives_calloc(capable->hw.ncpus, sizeof(float));
    reset = TRUE;
  }

  if (reset && cpuloadtab) cpuloadtab = free_tabdata(cpuloadtab);
  if (!cpuloadtab) cpuloadtab = init_tab_data(capable->hw.ncpus, N_CPU_MEAS);

  ctime = lives_get_session_ticks();
  if (reset || ctime - ltime > CPU_MEAS_THRESH) {
    ltime = ctime;
    if (get_cpu_loads(&cpuvals, capable->hw.ncpus)) {
      if (!cpu_stats) cpu_stats = &cpuvals;
      tabdata_get_avgs(cpuloadtab, cpuvals.loads);
      for (int i = capable->hw.ncpus; i--;) cpuvals.avgs[i] = cpuloadtab->avgs[i];
    }
  }
  return cpuvals.avgs;
}


volatile float const *get_core_loadvar(int corenum) {
  // return a pointer to the (static) array member containing the requested value
  // returning a pointer rather than the value allows for more in depth analysis
  float *vals = get_proc_loads(FALSE);
  return &vals[corenum];
}

uint64_t get_boottime(void) {return cpu_stats ? cpu_stats->boottime : 0;}


double analyse_cpu_stats(void) {
  // in future in might be nice to analyse the CPU / disk / memory load and respond accordingly
  // also looking for repeating / cyclic patterns we could take measures to minimise contention
#if 0
  static lives_obj_instance_t *statsinst = NULL;
  volatile float *cpuvals = vals[0];
  weed_param_t *param;
  lives_object_transform_t *tx;
  lives_object_status_t *st;

  // we should either create or update statsinst
  if (!statsinst) {
    const lives_obj_t *math_obj = lives_object_template_for_type(OBJECT_TYPE_MATH,
								 MATH_OBJECT_SUBTYPE_STATS);
    // get the math.stats template
    // we want to know how to get an instantc of it, we can look for the transform for
    // the OBJ_INTENTION_CREATE_INSTANCE intent
    tx = find_transform_for_intent(LIVES_OBJECT(math_obj), OBJ_INTENTION_CREATE_INSTANCE);
    if (requirements_met(tx)) {
      transform(LIVES_OBJECT(math_obj), tx, &statsinst);
    }
    else {
      // display unmet reqts.
    }
  }
  // now we have a few things we can do with the maths stats instance
  // in this case, look for deviance from the mean
  tx = find_transform_for_intent(statsinst, MATH_INTENTION_DEV_FROM_MEAN);

  // we could check but requts. are a value and an array of data
  // check if data can be satisifed internally from running_averags
  if (rules_lack_param(tx->prereqs, MATH_PARAM_DATA)) {
    param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_DATA);
    //set_float_array_param(&param->value, proc_load_stats, get_tab_size());
    weed_set_voidptr_value(param, WEED_LEAF_VALUE, proc_load_stats);

    param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_DATA_SIZE);
    weed_set_int_value(param, WEED_LEAF_VALUE, nfill);
  }
  // need to set the value too
  //param = rfx_param_from_name(tx->prereqs->reqs,
  param = weed_param_from_prereqs(tx->prereqs, MATH_PARAM_VALUE);
  weed_set_double_value(param, WEED_LEAF_VALUE, cpuvals[nfill - 1]);

  // call transform and check status
  st = transform(statsinst, tx, NULL);

  if (*st->status != LIVES_OBJECT_STATUS_NORMAL) {
    lives_object_status_free(st);
    lives_object_transform_free(tx);
    return 0.;
  }
  lives_object_transform_free(tx);
  lives_object_status_free(st);

  // now just get result
  param = weed_param_from_object(statsinst, MATH_PARAM_RESULT);
  return (float)weed_param_get_value_double(param);
#endif
  return 0.;
}


static void show_info(void) {
  char *memstr = get_memstats();
  char *thrdstr = get_threadstats();
  g_print("%s\n%sn", memstr, thrdstr);
  lives_free(memstr); lives_free(thrdstr);
}

#if 0
#define THE_TIMEY_WIMEY_KIND 1
// check when nothing happens
static uint64_t do_nothing(int what_type_of_nothing_did_you_expect, ticks_t the_start_of_nothing,
			   ticks_t **when_nothing_will_end) {
  uint64_t from_whence_you_came = THREADVAR(uid);
  if (what_type_of_nothing_did_you_expect == THE_TIMEY_WIMEY_KIND) {
    THREADVAR(ticks_to_activate) = lives_get_current_ticks() - the_start_of_nothing;
    *when_nothing_will_end = &THREADVAR(round_trip_ticks);
  }
  return from_whence_you_came;
}


ticks_t check_thrd_latency(void) {
  // ask a thread to do nothing, and then time how long i takes
  ticks_t alpha = lives_get_current_ticks(), *omega;
  /* lives_proc_thread_t lpt = lives_proc_thread_create(0, do_nothing, WEED_SEED_INT, "iIV", */
  /* 						     THE_TIMEY_WIMEY_KIND, alpha, &omega); */
  *omega = lives_get_current_ticks() - alpha;
  return *omega;
}
#endif

void perf_manager(void) {
  // this is designed to at some point be a self supporitn gobject
  ticks_t counter = 0;
  uint64_t seconds = 0, minutes = 0;
  boolean second_trigger = FALSE, minute_trigger = FALSE;
  boolean halfmin_trigger = FALSE;
  GET_PROC_THREAD_SELF(self);

  lives_proc_thread_set_cancellable(self);
  while (!lives_proc_thread_get_cancel_requested(self)) {
    if (second_trigger) {
      second_trigger = FALSE;
    }
    if (halfmin_trigger) {
      halfmin_trigger = FALSE;
      show_info();
    }
    if (minute_trigger) {
      minute_trigger = TRUE;
    }

    lives_nanosleep(100);
    ++counter;

    g_print("%lu\n", counter);

    if (counter == 1000) {
      second_trigger = TRUE;
      if (++seconds == 30) halfmin_trigger = TRUE;
      if (seconds == 60) {
	halfmin_trigger = TRUE;
	minute_trigger = TRUE;
	minutes++;
	seconds = 0;
      }
      counter = 0;
    }
  }
}
