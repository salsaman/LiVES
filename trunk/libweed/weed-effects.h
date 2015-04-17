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

/* (C) Gabriel "Salsaman" Finch, 2005 - 2011 */

#ifndef __WEED_EFFECTS_H__
#define __WEED_EFFECTS_H__

#ifndef __WEED_H__
#include <weed/weed.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* API version * 131 */
#define WEED_API_VERSION 131
#define WEED_API_VERSION_131

/* plant types */
#define WEED_PLANT_PLUGIN_INFO 1
#define WEED_PLANT_FILTER_CLASS 2
#define WEED_PLANT_FILTER_INSTANCE 3
#define WEED_PLANT_CHANNEL_TEMPLATE 4
#define WEED_PLANT_PARAMETER_TEMPLATE 5
#define WEED_PLANT_CHANNEL 6
#define WEED_PLANT_PARAMETER 7
#define WEED_PLANT_GUI 8
#define WEED_PLANT_HOST_INFO 255

/* Parameter hints */
#define WEED_HINT_UNSPECIFIED     0
#define WEED_HINT_INTEGER         1
#define WEED_HINT_FLOAT           2
#define WEED_HINT_TEXT            3
#define WEED_HINT_SWITCH          4
#define WEED_HINT_COLOR           5

/* Colorspaces for Color parameters */
#define WEED_COLORSPACE_RGB   1
#define WEED_COLORSPACE_RGBA  2

/* Filter flags */
#define WEED_FILTER_NON_REALTIME    (1<<0)
#define WEED_FILTER_IS_CONVERTER    (1<<1)
#define WEED_FILTER_HINT_IS_STATELESS (1<<2)
#define WEED_FILTER_HINT_IS_POINT_EFFECT (1<<3) // deprecated !!

/* API version 132 */
#define WEED_FILTER_HINT_MAY_THREAD (1<<5)

/* API version 131 */
#define WEED_FILTER_PROCESS_LAST (1<<4)

/* Channel template flags */
#define WEED_CHANNEL_REINIT_ON_SIZE_CHANGE    (1<<0)

/* API version 130 */
#define WEED_CHANNEL_REINIT_ON_ROWSTRIDES_CHANGE    (1<<6)
#define WEED_CHANNEL_OUT_ALPHA_PREMULT (1<<7)

#define WEED_CHANNEL_REINIT_ON_PALETTE_CHANGE (1<<1)
#define WEED_CHANNEL_CAN_DO_INPLACE           (1<<2)
#define WEED_CHANNEL_SIZE_CAN_VARY            (1<<3)
#define WEED_CHANNEL_PALETTE_CAN_VARY         (1<<4)


/* Channel flags */
#define WEED_CHANNEL_ALPHA_PREMULT (1<<0)


/* Parameter template flags */
#define WEED_PARAMETER_REINIT_ON_VALUE_CHANGE (1<<0)
#define WEED_PARAMETER_VARIABLE_ELEMENTS      (1<<1)

/* API version 110 */
#define WEED_PARAMETER_ELEMENT_PER_CHANNEL    (1<<2)

/* Plugin errors */
#define WEED_ERROR_TOO_MANY_INSTANCES 6
#define WEED_ERROR_HARDWARE 7
#define WEED_ERROR_INIT_ERROR 8
#define WEED_ERROR_PLUGIN_INVALID 64

/* host bootstrap function */
typedef weed_plant_t *(*weed_bootstrap_f)(weed_default_getter_f *value, int num_versions, int *plugin_versions);

/* plugin only functions */
typedef weed_plant_t *(*weed_setup_f)(weed_bootstrap_f weed_boot);
typedef void (*weed_desetup_f)(void);
typedef int (*weed_init_f)(weed_plant_t *filter_instance);
typedef int (*weed_process_f)(weed_plant_t *filter_instance, weed_timecode_t timestamp);
typedef int (*weed_deinit_f)(weed_plant_t *filter_instance);

/* special plugin functions */
typedef void (*weed_display_f)(weed_plant_t *parameter);
typedef int (*weed_interpolate_f)(weed_plant_t **in_values, weed_plant_t *out_value);

/* prototype here */
weed_plant_t *weed_setup(weed_bootstrap_f weed_boot);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_EFFECTS_H__
