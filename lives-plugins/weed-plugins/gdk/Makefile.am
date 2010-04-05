## Process this with automake to create Makefile.in

LIBS =

fxpluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -Wl,-Bsymbolic -module -avoid-version --tag=disable-static
shared_cflags = -fPIC -DPIC
endif

AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ -O3  ../../../libweed/libweed-utils.la ../../../libweed/libweed-plugin-utils.la
AM_CFLAGS = $(shared_cflags) -I$(srcdir)/.. @GTK_CFLAGS@ -O3 -ffast-math

if ENABLE_VJACK
vjack_libs = vjack_rcv.la
vjack_rcv_la_LDFLAGS = -ljack $(AM_LDFLAGS)
endif

fxpluginslib_LTLIBRARIES = gdk_fast_resize.la compositor.la videowall.la $(vjack_libs) 




