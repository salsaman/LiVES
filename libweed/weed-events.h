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


   Weed events is developed by:

   Gabriel "Salsaman" Finch - http://lives.sourceforge.net

*/

#ifndef __WEED_EVENTS_H__
#define __WEED_EVENTS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define WEED_EVENT_API_VERSION 120
#define WEED_EVENT_API_VERSION_100
#define WEED_EVENT_API_VERSION_110
#define WEED_EVENT_API_VERSION_120

#define WEED_PLANT_EVENT 256
#define WEED_PLANT_EVENT_LIST 257

#define WEED_EVENT_HINT_UNDEFINED 0
#define WEED_EVENT_HINT_FRAME 1
#define WEED_EVENT_HINT_FILTER_INIT 2
#define WEED_EVENT_HINT_FILTER_DEINIT 3
#define WEED_EVENT_HINT_FILTER_MAP 4
#define WEED_EVENT_HINT_PARAM_CHANGE 5
#define WEED_EVENT_HINT_MARKER 6

#ifndef WEED_AUDIO_LITTLE_ENDIAN
#define WEED_AUDIO_LITTLE_ENDIAN 0
#endif

#ifndef WEED_AUDIO_BIG_ENDIAN
#define WEED_AUDIO_BIG_ENDIAN 1
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_EVENTS_H_
