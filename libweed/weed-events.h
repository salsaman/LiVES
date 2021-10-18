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

   Gabriel "Salsaman" Finch - http://lives-video.com
*/

#ifndef __WEED_EVENTS_H__
#define __WEED_EVENTS_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define WEED_EVENT_API_VERSION 124
#define WEED_EVENT_API_VERSION_124

#define HAS_EVENT_TYPEDEFS
typedef weed_plant_t weed_event_t;
typedef weed_plant_t weed_event_list_t;

#define WEED_PLANT_EVENT 256
#define WEED_PLANT_EVENT_LIST 257

#define WEED_EVENT_TYPE_UNDEFINED 0
#define WEED_EVENT_TYPE_FRAME 1
#define WEED_EVENT_TYPE_FILTER_INIT 2
#define WEED_EVENT_TYPE_FILTER_DEINIT 3
#define WEED_EVENT_TYPE_FILTER_MAP 4
#define WEED_EVENT_TYPE_PARAM_CHANGE 5
#define WEED_EVENT_TYPE_MARKER 6

#ifndef WEED_AUDIO_LITTLE_ENDIAN
#define WEED_AUDIO_LITTLE_ENDIAN 0
#endif

#ifndef WEED_AUDIO_BIG_ENDIAN
#define WEED_AUDIO_BIG_ENDIAN 1
#endif

#ifndef WEED_TICKS_PER_SECOND
#define WEED_TICKS_PER_SECOND 100000000
#endif

// event_list
#define WEED_LEAF_WEED_EVENT_API_VERSION "weed_event_api_version"
#ifndef WEED_LEAF_AUTHOR
#define WEED_LEAF_AUTHOR "author"
#endif
#define WEED_LEAF_TITLE "title"
#define WEED_LEAF_COMMENTS "comments"
#define WEED_LEAF_LIVES_CREATED_VERSION "created_version"
#define WEED_LEAF_LIVES_EDITED_VERSION "edited_version"

#ifndef WEED_LEAF_FPS
#define WEED_LEAF_FPS "fps"
#endif

#ifndef WEED_LEAF_WIDTH
#define WEED_LEAF_WIDTH "width"
#endif

#ifndef WEED_LEAF_HEIGHT
#define WEED_LEAF_HEIGHT  "height"
#endif

#ifndef WEED_LEAF_AUDIO_CHANNELS
#define WEED_LEAF_AUDIO_CHANNELS "audio_channels"
#endif

#ifndef WEED_LEAF_AUDIO_RATE
#define WEED_LEAF_AUDIO_RATE "audio_rate"
#endif

#define WEED_LEAF_AUDIO_SAMPLE_SIZE "audio_sample_size"
#define WEED_LEAF_AUDIO_FLOAT "audio_float"
#define WEED_LEAF_AUDIO_SIGNED "audio_signed"
#define WEED_LEAF_AUDIO_ENDIAN "audio_endian"
#define WEED_LEAF_AUDIO_VOLUME_TRACKS "audio_volume_tracks"
#define WEED_LEAF_AUDIO_VOLUME_VALUES "audio_volume_values"
#define WEED_LEAF_TRACK_LABEL_TRACKS "track_label_tracks"
#define WEED_LEAF_TRACK_LABEL_VALUES "track_label_values"

#define WEED_LEAF_KEEP_ASPECT "keep_aspect"

#define WEED_LEAF_EVENT_TYPE "event_type"
#define WEED_LEAF_TIMECODE "timecode"

// frame event
#define WEED_LEAF_CLIPS "clips"
#define WEED_LEAF_FRAMES "frames"
#define WEED_LEAF_AUDIO_CLIPS "audio_clips"
#define WEED_LEAF_AUDIO_SEEKS "audio_seeks"

// init_event
#define WEED_LEAF_FILTER "filter"
#define WEED_LEAF_IN_COUNT "in_count"
#define WEED_LEAF_OUT_COUNT "out_count"
#define WEED_LEAF_IN_TRACKS "in_tracks"
#define WEED_LEAF_OUT_TRACKS "out_tracks"
#define WEED_LEAF_EVENT_ID "event_id"

// deinit / param_change
#define WEED_LEAF_INIT_EVENT "init_event"

// filter map
#define WEED_LEAF_INIT_EVENTS "init_events"

// param change
#define WEED_LEAF_INDEX "index"

#ifndef WEED_LEAF_VALUE
#define WEED_LEAF_VALUE "value"
#endif

#define WEED_LEAF_IGNORE "ignore"

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // #ifndef __WEED_EVENTS_H_
