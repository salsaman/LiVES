/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA


   Weed is developed by:

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   mainly based on LiViDO, which is developed by:


   Niels Elburg - http://veejay.sf.net

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

   Denis "Jaromil" Rojo - http://freej.dyne.org

   Tom Schouten - http://zwizwa.fartit.com

   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:

   Silvano "Kysucix" Galliani - http://freej.dyne.org

   Kentaro Fukuchi - http://megaui.net/fukuchi

   Jun Iio - http://www.malib.net

   Carlo Prelz - http://www2.fluido.as:8080/

*/

/* (C) Gabriel "Salsaman" Finch, 2005 - 2019 */

#ifndef __WEED_PLUGIN_H__
#define __WEED_PLUGIN_H__

#ifdef __WEED_HOST__
#error Plugins must not include weed-host.h
#endif

#define __WEED_PLUGIN__

// Define EXPORTED for any platform
#if defined _WIN32 || defined __CYGWIN__ || defined IS_MINGW
#ifdef WIN_EXPORT
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllexport))
#else
#define EXPORTED __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define EXPORTED __attribute__ ((dllimport))
#else
#define EXPORTED __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#define NOT_EXPORTED
#else
#if __GNUC__ >= 4
#define EXPORTED __attribute__ ((visibility ("default")))
#define NOT_EXPORTED  __attribute__ ((visibility ("hidden")))
#else
#define EXPORTED
#define NOT_EXPORTED
#endif
#endif

#ifdef __cplusplus
#define WEED_SETUP_START(weed_api_version, filter_api_version) extern "C" { EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    EXPORTED weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_version, weed_api_version, filter_api_version, filter_api_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_START_MINMAX(weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version) extern "C" { EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    EXPORTED weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_min_version, weed_api_max_version, filter_api_min_Version, filter_api_max_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_END } return plugin_info;}}

#define WEED_DESETUP_START extern "C" { EXPORTED void weed_desetup(void) {
#define WEED_DESETUP_END }}

#else

#define WEED_SETUP_START(weed_api_version, filter_api_version) EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_version, weed_api_version, filter_api_version, filter_api_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_START_MINMAX(weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version) EXPORTED weed_plant_t *weed_setup(weed_bootstrap_f weed_boot) { \
    weed_plant_t *plugin_info = weed_plugin_info_init(weed_boot, weed_api_min_version, weed_api_max_version, filter_api_min_version, filter_api_max_version); \
 if (plugin_info == NULL) {return NULL;} {

#define WEED_SETUP_END } return plugin_info;}

#define WEED_DESETUP_START EXPORTED void weed_desetup(void) {
#define WEED_DESETUP_END }

#endif

#endif // #ifndef __WEED_PLUGIN_H__
