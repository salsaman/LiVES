// sethandler.h
// (c) G. Finch 2019 - 2021 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

#ifndef _SETHANDLER_H_
#define _SETHANDLER_H_

#define CLIP_ORDER_FILENAME "order"
#define SET_LOCK_FILENAME "lock."

#define CLIP_ARCHIVE_NAME "__CLIP_ARCHIVE-"

#define LAYOUT_FILENAME "layout"
#define LAYOUT_MAP_FILENAME LAYOUT_FILENAME "."  LIVES_FILE_EXT_MAP
#define LAYOUT_NUMBERING_FILENAME LAYOUT_FILENAME "_numbering"

#define SET_DIR(set_name) (lives_build_path(prefs->workdir, (set_name), NULL))
#define FUTURE_SET_DIR(set_name) (lives_build_path(future_prefs->workdir, (set_name), NULL))
#define CURRENT_SET_DIR SET_DIR(mainw->set_name)

#define SET_LOCK_FILE(set_name, lockfile) lives_build_filename(SET_DIR((set_name)), (lockfile), NULL)
#define SET_LOCK_FILES_PREFIX(set_name) SET_LOCK_FILE((set_name), SET_LOCK_FILENAME);

// directory where we store 1 clip / all clips if handle is NULL
#define _MAKE_CLIPS_DIRNAME_(set, handle) lives_build_filename(SET_DIR((set)), CLIPS_DIRNAME, handle, NULL)

// directory where we store 1 clip / all clips if handle is NULL
#define _MAKE_FUTURE_CLIPS_DIRNAME_(set, handle) lives_build_filename(FUTURE_SET_DIR((set)), CLIPS_DIRNAME, handle, NULL)

// directory for all clips in set
#define CLIPS_DIR(set) (_MAKE_CLIPS_DIRNAME_((set), NULL))

#define FUTURE_CLIPS_DIR(set) (_MAKE_FUTURE_CLIPS_DIRNAME_((set), NULL))

// directory of a clip in the current set
#define CURRENT_SET_CLIP_DIR(handle) (_MAKE_CLIPS_DIRNAME_(mainw->set_name, (handle)))

#define LAYOUTS_DIR(set) (lives_build_path(SET_DIR((set)), LAYOUTS_DIRNAME, NULL))
#define FUTURE_LAYOUTS_DIR(set) (lives_build_path(FUTURE_SET_DIR((set)), LAYOUTS_DIRNAME, NULL))

#define CURRENT_SET_LAYOUTS_DIR (LAYOUTS_DIR(mainw->set_name))

#define LAYOUT_MAP_FILE(set) (lives_build_filename(LAYOUTS_DIR((set)), LAYOUT_MAP_FILENAME, NULL))
#define FUTURE_LAYOUT_MAP_FILE(set) (lives_build_filename(FUTURE_LAYOUTS_DIR((set)), LAYOUT_MAP_FILENAME, NULL))

#define LAYOUTS_DIRNAME "layouts"
#define CLIPS_DIRNAME "clips"
#define IMPORTS_DIRNAME "imports"

#define MAX_SET_NAME_LEN 128

/////////////////////////

LiVESList *get_set_list(const char *dir, boolean utf8);

boolean is_legal_set_name(const char *set_name, boolean allow_dupes, boolean leeway);

boolean check_for_lock_file(const char *set_name, int type);

void open_set_file(int clipnum);

boolean on_save_set_activate(LiVESWidget *, livespointer);

char *on_load_set_activate(LiVESMenuItem *, livespointer);

void lock_set_file(const char *set_name);
void unlock_set_file(const char *set_name);

boolean reload_set(const char *set_name);

void del_current_set(boolean exit_after);

LiVESResponseType prompt_for_set_save(void);

boolean do_save_clipset_warn(void);

void new_clipgroup(LiVESWidget *, livespointer);

#endif // _SETHANDLER_H_
