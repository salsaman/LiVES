// bindings.hpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details



//-fvisibility=hidden

#if defined _WIN32 || defined __CYGWIN__
  #ifdef BUILDING_DLL
    #ifdef __GNUC__
      #define LIVES_DLL_PUBLIC __attribute__ ((dllexport))
    #else
      #define LIVES_DLL_PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #else
    #ifdef __GNUC__
      #define LIVES_DLL_PUBLIC __attribute__ ((dllimport))
    #else
      #define LIVES_DLL_PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
    #endif
  #endif
  #define DLL_LOCAL
#else
  #if __GNUC__ >= 4
    #define LIVES_DLL_PUBLIC __attribute__ ((visibility ("default")))
    #define LIVES_DLL_LOCAL  __attribute__ ((visibility ("hidden")))
  #else
    #define LIVES_DLL_PUBLIC
    #define LIVES_DLL_LOCAL
  #endif
#endif

//extern "C" DLL_PUBLIC void function(int a);

/*class DLL_PUBLIC SomeClass
{
   int c;
   DLL_LOCAL void privateMethod();  // Only for use within this DSO
public:
   Person(int _c) : c(_c) { }
   static void foo(int a);
   };*/


static boolean is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return FALSE;
  return TRUE;
}


extern "C" {
  int real_main(int argc, char *argv[]);
  void lives_exit(void);
  boolean lives_osc_cb_fgclip_select(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
  boolean lives_osc_record_start(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
  boolean lives_osc_record_stop(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
  boolean lives_osc_record_toggle(void *context, int arglen, const void *vargs, OSCTimeTag when, NetworkReturnAddressPtr ra);
}

/////////////////////////////////////////////////////

#include <list>


using namespace std;

namespace LiVES {

  class LIVES_DLL_PUBLIC clip {
  public:
    boolean select(int cnum);
  };
  
  
  class LIVES_DLL_PUBLIC record {
  public:
    static boolean enable();
    static boolean disable();
    static boolean toggle();
  };


  class LIVES_DLL_PUBLIC LiVES {
  public:
    LiVES(int argc, char *argv[]);
    ~LiVES();

    list<clip *> clipList();

  };
}
