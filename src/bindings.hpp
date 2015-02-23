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

#include <inttypes.h>

#ifndef HAVE_OSC_TT
#ifdef __sgi
    #define HAS8BYTEINT
    /* You may have to change this typedef if there's some other
       way to specify 8 byte ints on your system */
    typedef long long int8;
    typedef unsigned long long uint8;
    typedef unsigned long uint4;
#else
    /* You may have to redefine this typedef if ints on your system 
       aren't 4 bytes. */
    typedef unsigned int uint4;
#endif


#ifdef HAS8BYTEINT
    typedef uint8 OSCTimeTag;
#else
    typedef struct {
        uint4 seconds;
        uint4 fraction;
    } OSCTimeTag;
#endif

#endif


static bool is_big_endian() {
  int32_t testint = 0x12345678;
  char *pMem;
  pMem = (char *) &testint;
  if (pMem[0] == 0x78) return false;
  return true;
}


/////////////////////////////////////////////////////

#include <list>
#include <string.h>

using namespace std;


//// API start /////


//namespace lives {

  class LIVES_DLL_PUBLIC clip {
  public:
    bool select(int cnum);
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

    list<clip *> clipList();

  };



//}
