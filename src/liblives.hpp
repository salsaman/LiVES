// liblives.hpp
// LiVES (lives-exe)
// (c) G. Finch <salsaman@gmail.com> 2015
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#include "osc_notify.h"

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

#include <inttypes.h>

#ifndef HAVE_OSC_TT
typedef struct {
  uint32_t seconds;
  uint32_t fraction;
} OSCTimeTag;
#endif


static bool is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return false;
  return true;
}


extern "C" {
  void binding_cb (int msgnumber, const char *msgstring, uint64_t id);
}

/////////////////////////////////////////////////////

#include <list>
#include <string.h>

using namespace std;

typedef unsigned long ulong;


//// API start /////


namespace lives {
  typedef class livesApp livesApp;

  typedef bool (*callback_f)(void *, void *);

  typedef struct {
    int msgnum;
    callback_f func;
    void *data;
  } closure;


  typedef struct {
    ulong id;
    livesApp *app;
  } livesAppCtx;


  typedef struct {
    int mode;
  } modeChangedInfo;


  typedef bool (*modeChanged_callback_f)(modeChangedInfo *, void *);

  class LIVES_DLL_PUBLIC clip {
  public:
    bool select(int cnum);

  private:
    uint64_t handle;

  };
  

  class LIVES_DLL_PUBLIC set {
    friend livesApp;

  public:
    ~set();
    char *name();
    list<clip *> clipList();

  protected:
    void setName(const char *setname);

  private:
    char *m_name;
    list<clip *>clips;
    //list<layout *>layouts;
  };
  
  class LIVES_DLL_PUBLIC record {
  public:
    static bool enable();
    static bool disable();
    static bool toggle();
  };



  class LIVES_DLL_PUBLIC livesApp {
  public:
    livesApp();
    livesApp(int argc, char *argv[]);
    ~livesApp();

    set currentSet();

    void play();
    bool stop();

    bool addCallback(int msgnum, modeChanged_callback_f func, void *data);

    void showInfo(const char *text, bool blocking);

    list<closure*> closures();


  private:
    set m_set;
    uint64_t m_id;
    list<closure*> m_closures;
  };



}
