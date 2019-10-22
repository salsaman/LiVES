// utils.c
// LiVES
// (c) G. Finch 2003 - 2019 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/file.h>

#include "main.h"
#include "support.h"
#include "interface.h"
#include "audio.h"
#include "resample.h"
#include "callbacks.h"

#if HAVE_SYSTEM_WEED
#include <weed/weed-host.h>
#else
#include "../libweed/weed-host.h"
#endif

static boolean  omute,  osepwin,  ofs,  ofaded,  odouble;

typedef struct {
  int fd;
  ssize_t bytes;
  boolean eof;
  uint8_t *ptr;
  uint8_t *buffer;
  boolean read;
  boolean allow_fail;
  off_t offset;
  char *pathname;
} lives_file_buffer_t;

static lives_file_buffer_t *find_in_file_buffers(int fd);


char *get_md5sum(const char *filename) {
  char **array;
  char *md5;
  char *com = lives_strdup_printf("%s \"%s\"", EXEC_MD5SUM, filename);
  mainw->com_failed = FALSE;
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);
  if (mainw->com_failed) {
    mainw->com_failed = FALSE;
    return NULL;
  }
  array = lives_strsplit(mainw->msg, " ", 2);
  md5 = lives_strdup(array[0]);
  lives_strfreev(array);
  return md5;
}


char *filename_from_fd(char *val, int fd) {
  // return filename from an open fd, freeing val first

  // in case of error we return val

  // call like: foo = filename_from_fd(foo,fd); lives_free(foo);

  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (fbuff != NULL) {
    return lives_strdup(fbuff->pathname);
  } else {
    char *fdpath;
    char *fidi;
    char rfdpath[PATH_MAX];
    struct stat stb0, stb1;

    ssize_t slen;

    if (fstat(fd, &stb0)) return val;

    fidi = lives_strdup_printf("%d", fd);
    fdpath = lives_build_filename("/proc", "self", "fd", fidi, NULL);
    lives_free(fidi);

    if ((slen = lives_readlink(fdpath, rfdpath, PATH_MAX)) == -1) return val;
    lives_free(fdpath);

    memset(rfdpath + slen, 0, 1);

    if (stat(rfdpath, &stb1)) return val;

    if (stb0.st_dev != stb1.st_dev) return val;

    if (stb0.st_ino != stb1.st_ino) return val;

    if (val != NULL) lives_free(val);
    return lives_strdup(rfdpath);
  }
}


LIVES_INLINE void reverse_bytes(char *out, const char *in, size_t count) {
  register int i;
  for (i = 0; i < count; i++) {
    out[i] = in[count - i - 1];
  }
}


// system calls

LIVES_GLOBAL_INLINE void lives_srandom(unsigned int seed) {
  srandom(seed);
}


LIVES_GLOBAL_INLINE uint64_t lives_random(void) {
  return random();
}


LIVES_GLOBAL_INLINE pid_t lives_getpid(void) {
  return getpid();
}


LIVES_GLOBAL_INLINE int lives_open3(const char *pathname, int flags, mode_t mode) {
  int fd = open(pathname, flags, mode);
  return fd;
}


LIVES_GLOBAL_INLINE int lives_open2(const char *pathname, int flags) {
  int fd = open(pathname, flags);
  return fd;
}


LIVES_GLOBAL_INLINE int lives_getuid(void) {
  return geteuid();
}


LIVES_GLOBAL_INLINE int lives_getgid(void) {
  return getegid();
}


LIVES_GLOBAL_INLINE ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz) {
  return readlink(path, buf, bufsiz);
}


LIVES_GLOBAL_INLINE boolean lives_fsync(int fd) {
  // ret TRUE on success
  return !fsync(fd);
}


LIVES_GLOBAL_INLINE void lives_sync(int times) {
  int i;
  for (i = 0; i < times; i++) {
    sync();
  }
}


LIVES_GLOBAL_INLINE boolean lives_setenv(const char *name, const char *value) {
  // ret TRUE on success
#if IS_IRIX
  int len  = strlen(name) + strlen(value) + 2;
  char *env = malloc(len);
  if (env != NULL) {
    strcpy(env, name);
    strcat(env, "=");
    strcat(env, val);
    return !putenv(env);
  }
}
#else
  return !setenv(name, value, 1);
#endif
}


int lives_system(const char *com, boolean allow_error) {
  int retval;
  boolean cnorm = FALSE;

  //g_print("doing: %s\n",com);

  if (mainw->is_ready && !mainw->is_exiting &&
      ((mainw->multitrack == NULL && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
       (mainw->multitrack != NULL && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
    cnorm = TRUE;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    if (!mainw->go_away)
      lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  }

  retval = system(com);

  if (retval) {
    char *msg = NULL;
    mainw->com_failed = TRUE;
    if (!allow_error) {
      msg = lives_strdup_printf("lives_system failed with code %d: %s", retval, com);
      LIVES_ERROR(msg);
      do_system_failed_error(com, retval, NULL);
    }
#ifndef LIVES_NO_DEBUG
    else {
      msg = lives_strdup_printf("lives_system failed with code %d: %s (not an error)", retval, com);
      LIVES_DEBUG(msg);
    }
#endif
    if (msg != NULL) lives_free(msg);
  }

  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);

  return retval;
}


ssize_t lives_popen(const char *com, boolean allow_error, char *buff, size_t buflen) {
  // runs com, fills buff with a NUL terminated string (total length <= buflen)
  // returns number of bytes read. If an error occurs during popen or fread
  // then mainw->com_failed is set, and if allow_error is FALSE then an an error dialog is displayed to the user

  // on error we return err as a -ve number

  FILE *fp;
  size_t bytes_read = 0;
  int err;
  boolean cnorm = FALSE;

  //g_print("doing: %s\n",com);

  if (mainw->is_ready && !mainw->is_exiting &&
      ((mainw->multitrack == NULL && mainw->cursor_style == LIVES_CURSOR_NORMAL) ||
       (mainw->multitrack != NULL && mainw->multitrack->cursor_style == LIVES_CURSOR_NORMAL))) {
    cnorm = TRUE;
    lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
    lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);
  }

  fp = popen(com, "r");
  if (fp == NULL) {
    err = errno;
  } else {
    bytes_read = fread(buff, 1, buflen - 1, fp);
    err = ferror(fp);
    fclose(fp);
  }

  memset(buff + bytes_read, 0, 1);

  if (bytes_read == 0 && err != 0) {
    char *msg = NULL;
    mainw->com_failed = TRUE;
    if (!allow_error) {
      msg = lives_strdup_printf("lives_popen failed with code %d: %s", err, com);
      LIVES_ERROR(msg);
      do_system_failed_error(com, err, NULL);
    }
#ifndef LIVES_NO_DEBUG
    else {
      msg = lives_strdup_printf("lives_popen failed with code %d: %s (not an error)", err, com);
      LIVES_DEBUG(msg);
    }
#endif
    if (msg != NULL) lives_free(msg);
  }
  if (cnorm) lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
  if (err != 0) return -ABS(err);
  return bytes_read;
}


lives_pgid_t lives_fork(const char *com) {
  // returns a number which is the pgid to use for lives_killpg

  // mingw - return PROCESS_INFORMATION * to use in GenerateConsoleCtrlEvent (?)

  // to signal to sub process and all children
  // TODO *** - error check

  pid_t ret;

  if (!(ret = fork())) {
    int res;
    setsid(); // create new session id
    setpgid(capable->mainpid, 0); // create new pgid
    res = system(com);
    (void)res; // stop compiler complaining
    _exit(0);
  }

  return ret;
}


ssize_t lives_write(int fd, const void *buf, size_t count, boolean allow_fail) {
  ssize_t retval;
  retval = write(fd, buf, count);

  if (retval < count) {
    char *msg = NULL;
    mainw->write_failed = TRUE;
    mainw->write_failed_file = filename_from_fd(mainw->write_failed_file, fd);
    if (retval >= 0)
      msg = lives_strdup_printf("Write failed %"PRIu64" of %"PRIu64" in: %s", (uint64_t)retval,
                                (uint64_t)count, mainw->write_failed_file);
    else
      msg = lives_strdup_printf("Write failed with error %"PRIu64" in: %s", (uint64_t)retval,
                                mainw->write_failed_file);

    if (!allow_fail) {
      LIVES_ERROR(msg);
      close(fd);
    }
#ifndef LIVES_NO_DEBUG
    else {
      char *ffile = filename_from_fd(NULL, fd);
      if (retval >= 0)
        msg = lives_strdup_printf("Write failed %"PRIu64" of %"PRIu64" in: %s (not an error)", (uint64_t)retval,
                                  (uint64_t)count, ffile);
      else
        msg = lives_strdup_printf("Write failed with error %"PRIu64" in: %s (allowed)", (uint64_t)retval,
                                  mainw->write_failed_file);
      LIVES_DEBUG(msg);
      lives_free(ffile);
    }
#endif
    if (msg != NULL) lives_free(msg);
  }
  return retval;
}


ssize_t lives_write_le(int fd, const void *buf, size_t count, boolean allow_fail) {
  if (capable->byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
    char xbuf[count];
    reverse_bytes(xbuf, (const char *)buf, count);
    return lives_write(fd, xbuf, count, allow_fail);
  } else {
    return lives_write(fd, buf, count, allow_fail);
  }
}


int lives_fputs(const char *s, FILE *stream) {
  int retval;

  retval = fputs(s, stream);

  if (retval == EOF) {
    mainw->write_failed = TRUE;
  }

  return retval;
}


char *lives_fgets(char *s, int size, FILE *stream) {
  char *retval;

  retval = fgets(s, size, stream);

  if (retval == NULL && ferror(stream)) {
    mainw->read_failed = TRUE;
  }

  return retval;
}


size_t lives_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t bytes_read = fread(ptr, size, nmemb, stream);

  if (ferror(stream)) {
    mainw->read_failed = TRUE;
  }

  return bytes_read;
}


static lives_file_buffer_t *find_in_file_buffers(int fd) {
  lives_file_buffer_t *fbuff;
  LiVESList *fblist = mainw->file_buffers;

  while (fblist != NULL) {
    fbuff = (lives_file_buffer_t *)fblist->data;
    if (fbuff->fd == fd) return fbuff;
    fblist = fblist->next;
  }

  return NULL;
}


void lives_close_all_file_buffers(void) {
  lives_file_buffer_t *fbuff;

  while (mainw->file_buffers != NULL) {
    fbuff = (lives_file_buffer_t *)mainw->file_buffers->data;
    lives_close_buffered(fbuff->fd);
  }
}


static void do_file_read_error(int fd, ssize_t errval, size_t count) {
  char *msg = NULL;
  mainw->read_failed = TRUE;
  mainw->read_failed_file = filename_from_fd(mainw->read_failed_file, fd);

  if (errval >= 0)
    msg = lives_strdup_printf("Read failed %"PRId64" of %"PRIu64" in: %s", (int64_t)errval,
                              (uint64_t)count, mainw->read_failed_file);
  else
    msg = lives_strdup_printf("Read failed with error %"PRId64" in: %s", (int64_t)errval,
                              mainw->read_failed_file);

  LIVES_ERROR(msg);
  lives_free(msg);
}


ssize_t lives_read(int fd, void *buf, size_t count, boolean allow_less) {
  ssize_t retval = read(fd, buf, count);

  if (retval < count) {
    if (!allow_less || retval < 0) {
      do_file_read_error(fd, retval, count);
      close(fd);
    }
#ifndef LIVES_NO_DEBUG
    else {
      char *msg = NULL;
      char *ffile = filename_from_fd(NULL, fd);
      msg = lives_strdup_printf("Read got %"PRIu64" of %"PRIu64" in: %s (not an error)", (uint64_t)retval,
                                (uint64_t)count, ffile);
      LIVES_DEBUG(msg);
      lives_free(ffile);
      g_free(msg);
    }
#endif
  }
  return retval;
}


ssize_t lives_read_le(int fd, void *buf, size_t count, boolean allow_less) {
  if (capable->byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    char xbuf[count];
    ssize_t retval = lives_read(fd, buf, count, allow_less);
    if (retval < count) return retval;
    reverse_bytes((char *)buf, (const char *)xbuf, count);
    return retval;
  } else {
    return lives_read(fd, buf, count, allow_less);
  }
}

//// buffered io ////

// explanation of values

// read:
// fbuff->buffer holds (fbuff->ptr - fbuff->buffer + fbuff->bytes) bytes
// fbuff->offset is the next real read position

// read x bytes : fbuff->ptr increases by x, fbuff->bytes decreases by x
// if fbuff->bytes is < x, then we concat fbuff->bytes, refill buffer from file, concat remaining bytes
// on read: fbuff->ptr = fbuff->buffer. fbuff->offset += bytes read, fbuff->bytes = bytes read

// on seek (read only):
// forward: seek by +z: if z < fbuff->bytes : fbuff->ptr += z, fbuff->bytes -= z
// if z > fbuff->bytes: subtract fbuff->bytes from z. Increase fbuff->offset by remainder. Fill buffer.

// backward: if fbuff->ptr - z >= fbuff->buffer : fbuff->ptr -= z, fbuff->bytes += z
// fbuff->ptr - z < fbuff->buffer:  z -= (fbuff->ptr - fbuff->buffer) : fbuff->offset -= (fbuff->ptr - fbuff->buffer + fbuff->bytes - z) : Fill buffer

// seek absolute: current vitual posn is fbuff->offset - fbuff->bytes : subtract this from absolute posn

// return value is: fbuff->offset - fbuff->bytes ?

// when writing we simply fill up the buffer until it full, then flush the buffer to file io
// buffer is finally flushed when we close the file (or we call file_buffer_flush)

// in this case fbuff->bytes holds the number of bytes written to fbuff->buffer, fbuff->offset contains the offset in the underlying fil

#define BUFFER_FILL_BYTES 65536

size_t buffer_fill_bytes;

static ssize_t file_buffer_flush(int fd) {
  // returns number of bytes written to file io, or error code
  ssize_t res = 0;
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);

  if (fbuff == NULL) {
    // normal non-buffered file+
    LIVES_DEBUG("file_buffer_flush: no file buffer found");
    return res;
  }

  if (fbuff->buffer != NULL) res = lives_write(fbuff->fd, fbuff->buffer, fbuff->bytes, fbuff->allow_fail);
  if (res > 0) fbuff->offset += res;

  if (!fbuff->allow_fail && res < fbuff->bytes) {
    lives_close_buffered(-fbuff->fd); // use -fd as lives_write will have closed
  }

  return res;
}


static int lives_open_real_buffered(const char *pathname, int flags, int mode, boolean isread) {
  lives_file_buffer_t *fbuff, *xbuff;
  int fd = lives_open3(pathname, flags, mode);
  if (fd >= 0) {
    fbuff = (lives_file_buffer_t *)lives_malloc(sizeof(lives_file_buffer_t));
    fbuff->fd = fd;
    fbuff->bytes = 0;
    fbuff->eof = FALSE;
    fbuff->ptr = NULL;
    fbuff->buffer = NULL;
    fbuff->read = isread;
    fbuff->offset = 0;
    fbuff->pathname = lives_strdup(pathname);
    if ((xbuff = find_in_file_buffers(fd)) != NULL) {
      char *msg = lives_strdup_printf("Duplicate fd (%d) in file buffers !\n%s was not removed, and\n%s will be added.", fd, xbuff->pathname,
                                      fbuff->pathname);
      LIVES_ERROR(msg);
      lives_free(msg);
      lives_close_buffered(fd);
    }
    mainw->file_buffers = lives_list_append(mainw->file_buffers, (livespointer)fbuff);
  }

  return fd;
}


LIVES_GLOBAL_INLINE int lives_open_buffered_rdonly(const char *pathname) {
  int fd = lives_open_real_buffered(pathname, O_RDONLY, 0, TRUE);
#ifdef HAVE_POSIX_FADVISE
  posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);
#endif
  return fd;
}


LIVES_GLOBAL_INLINE int lives_creat_buffered(const char *pathname, int mode) {
  return lives_open_real_buffered(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode, FALSE);
}


int lives_close_buffered(int fd) {
  lives_file_buffer_t *fbuff;
  boolean should_close = TRUE;
  int ret = 0;

  if (fd < 0) {
    should_close = FALSE;
    fd = -fd;
  }

  fbuff = find_in_file_buffers(fd);

  if (fbuff == NULL) {
    // normal non-buffered file
    LIVES_DEBUG("lives_close_buffered: no file buffer found");
    if (should_close) ret = close(fd);
    return ret;
  }

  if (!fbuff->read && should_close) {
    boolean allow_fail = fbuff->allow_fail;
    size_t bytes = fbuff->bytes;

    if (bytes > 0) {
      ret = file_buffer_flush(fd);
      if (!allow_fail && ret < bytes) return ret; // this is correct, as flush will have called close again with should_close=FALSE;
    }
#ifdef HAVE_POSIX_FALLOCATE
    int dummy;
    (void)(dummy = ftruncate(fbuff->fd, fbuff->offset));
#endif
  }

  if (should_close && fbuff->fd >= 0) ret = close(fbuff->fd);

  lives_free(fbuff->pathname);

  mainw->file_buffers = lives_list_remove(mainw->file_buffers, (livesconstpointer)fbuff);
  if (fbuff->buffer != NULL) lives_free(fbuff->buffer);
  lives_free(fbuff);
  return ret;
}


static ssize_t file_buffer_fill(lives_file_buffer_t *fbuff) {
  ssize_t res;

  if (fbuff->buffer == NULL) fbuff->buffer = (uint8_t *)lives_malloc(BUFFER_FILL_BYTES);

  lseek(fbuff->fd, fbuff->offset, SEEK_SET);
  res = lives_read(fbuff->fd, fbuff->buffer, BUFFER_FILL_BYTES, TRUE);
  if (res < 0) {
    lives_close_buffered(-fbuff->fd); // use -fd as lives_read will have closed
    return res;
  }

  fbuff->bytes = res;
  fbuff->ptr = fbuff->buffer;
  fbuff->offset += res;

  if (res < BUFFER_FILL_BYTES) fbuff->eof = TRUE;
  else fbuff->eof = FALSE;

  return res;
}


static off_t _lives_lseek_buffered_rdonly_relative(lives_file_buffer_t *fbuff, off_t offset) {
  if (offset > 0) {
    // seek forwards
    if (offset < fbuff->bytes) {
      fbuff->ptr += offset;
      fbuff->bytes -= offset;
      return fbuff->offset - fbuff->bytes;
    }
    offset -= fbuff->bytes;
    fbuff->offset += offset;
    fbuff->bytes = 0;
    return fbuff->offset;
  }

  // seek backwards
  offset = -offset;

  if (offset <= fbuff->ptr - fbuff->buffer) {
    fbuff->ptr -= offset;
    fbuff->bytes += offset;
    return fbuff->offset - fbuff->bytes;
  }

  offset -= fbuff->ptr - fbuff->buffer;
  fbuff->offset = fbuff->offset - (fbuff->ptr - fbuff->buffer + fbuff->bytes + offset);
  if (fbuff->offset < 0) fbuff->offset = 0;
  fbuff->bytes = 0;
  fbuff->eof = FALSE;

  return fbuff->offset;
}


off_t lives_lseek_buffered_rdonly(int fd, off_t offset) {
  // seek relative
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_lseek_buffered_rdonly: no file buffer found");
    return lseek(fd, offset, SEEK_CUR);
  }

  return _lives_lseek_buffered_rdonly_relative(fbuff, offset);
}


off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t offset) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_lseek_buffered_rdonly_absolute: no file buffer found");
    return lseek(fd, offset, SEEK_SET);
  }

  offset -= fbuff->offset - fbuff->bytes;
  return _lives_lseek_buffered_rdonly_relative(fbuff, offset);
}


ssize_t lives_read_buffered(int fd, void *buf, size_t count, boolean allow_less) {
  lives_file_buffer_t *fbuff;
  ssize_t retval = 0, res;
  size_t ocount = count;
  uint8_t *ptr = (uint8_t *)buf;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered: no file buffer found");
    return lives_read(fd, buf, count, allow_less);
  }

  if (!fbuff->read) {
    LIVES_ERROR("lives_read_buffered: wrong buffer type");
    return 0;
  }

  // read bytes from fbuff
  while (1) {
    if (fbuff->bytes <= 0) {
      if (!fbuff->eof) {
        // refill the buffer
        //fbuff->offset += fbuff->ptr - fbuff->buffer;
        res = file_buffer_fill(fbuff);
        if (res < 0) return res;
        continue;
      }
      break;
    }
    if (fbuff->bytes < count) {
      // use up buffer
      lives_memcpy(ptr, fbuff->ptr, fbuff->bytes);
      retval += fbuff->bytes;
      count -= fbuff->bytes;
      fbuff->ptr += fbuff->bytes;
      ptr += fbuff->bytes;
      fbuff->bytes = 0;
      if (fbuff->eof) {
        break;
      }
      continue;
    }
    // buffer is sufficient
    lives_memcpy(ptr, fbuff->ptr, count);
    retval += count;
    fbuff->ptr += count;
    fbuff->bytes -= count;
    count = 0;
    break;
  }

  if (!allow_less && count > 0) {
    do_file_read_error(fd, retval, ocount);
    lives_close_buffered(fd);
  }

  return retval;
}


ssize_t lives_read_le_buffered(int fd, void *buf, size_t count, boolean allow_less) {
  if (capable->byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    char xbuf[count];
    ssize_t retval = lives_read_buffered(fd, buf, count, allow_less);
    if (retval < count) return retval;
    reverse_bytes((char *)buf, (const char *)xbuf, count);
    return retval;
  } else {
    return lives_read_buffered(fd, buf, count, allow_less);
  }
}


ssize_t lives_write_buffered(int fd, const char *buf, size_t count, boolean allow_fail) {
  lives_file_buffer_t *fbuff;
  ssize_t retval = 0, res;
  size_t space_left;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_write_buffered: no file buffer found");
    return lives_write(fd, buf, count, allow_fail);
  }

  if (fbuff->read) {
    LIVES_ERROR("lives_write_buffered: wrong buffer type");
    return 0;
  }

  if (fbuff->buffer == NULL) {
    fbuff->buffer = (uint8_t *)lives_malloc(BUFFER_FILL_BYTES);
    fbuff->ptr = fbuff->buffer;
    fbuff->bytes = 0;
  }

  fbuff->allow_fail = allow_fail;

  // write bytes from fbuff
  while (count) {
    space_left = BUFFER_FILL_BYTES - fbuff->bytes;
    if (space_left < count) {
      lives_memcpy(fbuff->ptr, buf, space_left);
      fbuff->bytes = BUFFER_FILL_BYTES;
      res = file_buffer_flush(fd);
      retval += res;
      if (res < BUFFER_FILL_BYTES) return (res < 0 ? res : retval);
#ifdef HAVE_POSIX_FALLOCATE
      // pre-allocate space for next buffer, we need to ftruncate this when closing the file
      posix_fallocate(fbuff->fd, fbuff->offset, BUFFER_FILL_BYTES);
#endif
      fbuff->bytes = 0;
      fbuff->ptr = fbuff->buffer;
      count -= space_left;
      buf += space_left;
    } else {
      lives_memcpy(fbuff->ptr, buf, count);
      retval += count;
      fbuff->ptr += count;
      fbuff->bytes += count;
      count = 0;
    }
  }
  return retval;
}


ssize_t lives_write_le_buffered(int fd, const void *buf, size_t count, boolean allow_fail) {
  if (capable->byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
    char xbuf[count];
    reverse_bytes((char *)xbuf, (const char *)buf, count);
    return lives_write_buffered(fd, xbuf, count, allow_fail);
  } else {
    return lives_write_buffered(fd, (char *)buf, count, allow_fail);
  }
}


/////////////////////////////////////////////

char *lives_format_storage_space_string(uint64_t space) {
  char *fmt;

  if (space > lives_10pow(18)) {
    // TRANSLATORS: Exabytes
    fmt = lives_strdup_printf(_("%.2f EB"), (double)space / (double)lives_10pow(18));
  } else if (space > lives_10pow(15)) {
    // TRANSLATORS: Petabytes
    fmt = lives_strdup_printf(_("%.2f PB"), (double)space / (double)lives_10pow(15));
  } else if (space > lives_10pow(12)) {
    // TRANSLATORS: Terabytes
    fmt = lives_strdup_printf(_("%.2f TB"), (double)space / (double)lives_10pow(12));
  } else if (space > lives_10pow(9)) {
    // TRANSLATORS: Gigabytes
    fmt = lives_strdup_printf(_("%.2f GB"), (double)space / (double)lives_10pow(9));
  } else if (space > lives_10pow(6)) {
    // TRANSLATORS: Megabytes
    fmt = lives_strdup_printf(_("%.2f MB"), (double)space / (double)lives_10pow(6));
  } else if (space > 1024) {
    // TRANSLATORS: Kilobytes (1024 bytes)
    fmt = lives_strdup_printf(_("%.2f KiB"), (double)space / 1024.);
  } else {
    fmt = lives_strdup_printf(_("%d bytes"), space);
  }

  return fmt;
}


lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, uint64_t *dsval) {
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)
  uint64_t ds;
  if (!is_writeable_dir(dir)) return LIVES_STORAGE_STATUS_UNKNOWN;
  ds = get_fs_free(dir);
  if (dsval != NULL) *dsval = ds;
  if (ds < prefs->ds_crit_level) return LIVES_STORAGE_STATUS_CRITICAL;
  if (ds < warn_level) return LIVES_STORAGE_STATUS_WARNING;
  return LIVES_STORAGE_STATUS_NORMAL;
}


int lives_chdir(const char *path, boolean allow_fail) {
  int retval;

  retval = chdir(path);

  if (retval) {
    char *msg = lives_strdup_printf("Chdir failed to: %s", path);
    mainw->chdir_failed = TRUE;
    if (!allow_fail) {
      LIVES_ERROR(msg);
      do_chdir_failed_error(path);
    } else LIVES_DEBUG(msg);
    lives_free(msg);
  }
  return retval;
}


LIVES_GLOBAL_INLINE boolean lives_freep(void **ptr) {
  // free a pointer and nullify it, only if it is non-null to start with
  // pass the address of the pointer in
  if (ptr != NULL && *ptr != NULL) {
    lives_free(*ptr);
    *ptr = NULL;
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE int lives_kill(lives_pid_t pid, int sig) {
  if (pid == 0) {
    LIVES_ERROR("Tried to kill pid 0");
    return -1;
  }
  return kill(pid, sig);
}


LIVES_GLOBAL_INLINE int lives_killpg(lives_pgid_t pgrp, int sig) {
  return killpg(pgrp, sig);
}


LIVES_GLOBAL_INLINE void clear_mainw_msg(void) {
  memset(mainw->msg, 0, MAINW_MSG_SIZE);
}


LIVES_GLOBAL_INLINE uint64_t lives_10pow(int pow) {
  register int i;
  uint64_t res = 1;
  for (i = 0; i < pow; i++) res *= 10;
  return res;
}


LIVES_GLOBAL_INLINE double lives_fix(double val, int decimals) {
  double factor = (double)lives_10pow(decimals);
  return (double)((int)(val * factor + 0.5)) / factor;
}


LIVES_GLOBAL_INLINE int get_approx_ln(uint32_t x) {
  x |= (x >> 1);
  x |= (x >> 2);
  x |= (x >> 4);
  x |= (x >> 8);
  x |= (x >> 16);
  x++;
  return x >> 1;
}


LIVES_GLOBAL_INLINE ticks_t lives_get_relative_ticks(int64_t origsecs, int64_t origusecs) {
#ifdef USE_MONOTONIC_TIME
  return (lives_get_monotonic_time() - origusecs) * USEC_TO_TICKS;
#else
  gettimeofday(&tv, NULL);
  return TICKS_PER_SECOND * (tv.tv_sec - origsecs) + tv.tv_usec * USEC_TO_TICKS - origusecs * USEC_TO_TICKS;
#endif
}


ticks_t lives_get_current_playback_ticks(int64_t origsecs, int64_t origusecs, lives_time_source_t *time_source) {
  // get the time using a variety of methods
  // time_source may be NULL or LIVES_TIME_SOURCE_NONE to set auto
  // or another value to force it (EXTERNAL cannot be forced)

  if (time_source != NULL && *time_source == LIVES_TIME_SOURCE_EXTERNAL) *time_source = LIVES_TIME_SOURCE_NONE;

  if (mainw->foreign || prefs->force_system_clock) {
    if (time_source != NULL) *time_source = LIVES_TIME_SOURCE_SYSTEM;
    return lives_get_relative_ticks(origsecs, origusecs);
  }

  if (time_source == NULL || *time_source == LIVES_TIME_SOURCE_NONE) {
#ifdef ENABLE_JACK_TRANSPORT
    if (mainw->jack_can_stop && (prefs->jack_opts & JACK_OPTS_TIMEBASE_CLIENT) &&
        (prefs->jack_opts & JACK_OPTS_TRANSPORT_CLIENT) && !(mainw->record && !(prefs->rec_opts & REC_FRAMES))) {
      // calculate the time from jack transport
      if (time_source != NULL) *time_source = LIVES_TIME_SOURCE_EXTERNAL;
      return jack_transport_get_time() * TICKS_PER_SECOND_DBL;
    }
#endif
  }

  if (is_realtime_aplayer(prefs->audio_player) && (time_source == NULL || *time_source == LIVES_TIME_SOURCE_NONE ||
      *time_source == LIVES_TIME_SOURCE_SOUNDCARD)) {
    if ((!mainw->is_rendering || (mainw->multitrack != NULL && !cfile->opening && !mainw->multitrack->is_rendering)) &&
        (!(mainw->fixed_fpsd > 0. || (mainw->vpp != NULL && mainw->vpp->fixed_fpsd > 0. && mainw->ext_playback)))) {
      // get time from soundcard
      // this is done so as to synch video stream with the audio
      // we do this in two cases:
      // - for internal audio, playing back a clip with audio (writing)
      // - or when audio source is set to external (reading) and we are recording, no internal audio generator is running

      // we ignore this if we are running with a playback plugin which requires a fixed framerate (e.g a streaming plugin)
      // in that case we will adjust the audio rate to fit the system clock

      // or if we are rendering

#ifdef ENABLE_JACK
      if (prefs->audio_player == AUD_PLAYER_JACK &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->jackd != NULL && mainw->jackd->in_use &&
            IS_VALID_CLIP(mainw->jackd->playing_file) && mainw->files[mainw->jackd->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->jackd_read != NULL && mainw->jackd_read->in_use))) {
        if (time_source != NULL) *time_source = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          return lives_jack_get_time(mainw->jackd_read);
        return lives_jack_get_time(mainw->jackd);
      }
#endif

#ifdef HAVE_PULSE_AUDIO
      if (prefs->audio_player == AUD_PLAYER_PULSE &&
          ((prefs->audio_src == AUDIO_SRC_INT && mainw->pulsed != NULL && mainw->pulsed->in_use &&
            IS_VALID_CLIP(mainw->pulsed->playing_file) && mainw->files[mainw->pulsed->playing_file]->achans > 0) ||
           (prefs->audio_src == AUDIO_SRC_EXT && mainw->pulsed_read != NULL && mainw->pulsed_read->in_use))) {
        if (time_source != NULL) *time_source = LIVES_TIME_SOURCE_SOUNDCARD;
        if (prefs->audio_src == AUDIO_SRC_EXT && mainw->agen_key == 0 && !mainw->agen_needs_reinit)
          return lives_pulse_get_time(mainw->pulsed_read);
        return lives_pulse_get_time(mainw->pulsed);
      }
#endif
    }
  }

  if (time_source != NULL) *time_source = LIVES_TIME_SOURCE_SYSTEM;
  return lives_get_relative_ticks(origsecs, origusecs);
}


LIVES_GLOBAL_INLINE ticks_t lives_get_current_ticks(void) {
  //  return current (wallclock) time in ticks (units of 10 nanoseconds)
  return lives_get_relative_ticks(0, 0);
}


/** set alarm for now + delta ticks (10 nanosec)
 * param ticks (10 nanoseconds) is the offset when we want our alarm to trigger
 * returns int handle or -1
 * call lives_get_alarm(handle) to test if time arrived
 */

lives_alarm_t lives_alarm_set(ticks_t ticks) {
  int i;
  ticks_t cticks;

  // we will assign [this] next
  lives_alarm_t ret = mainw->next_free_alarm;

  if (ret > LIVES_MAX_USER_ALARMS) ret--;
  else {
    // no alarm slots left
    if (mainw->next_free_alarm == ALL_USED) {
      LIVES_WARN("No alarms left");
      return ALL_USED;
    }
  }

  // get current ticks
  cticks = lives_get_current_ticks();

  // set to now + offset
  mainw->alarms[ret] = cticks + ticks;

  // system alarms
  if (ret >= LIVES_MAX_USER_ALARMS) return ++ret;

  i = ++mainw->next_free_alarm;

  // find free slot for next time
  while (mainw->alarms[i] != LIVES_NO_ALARM_TICKS && i < LIVES_MAX_USER_ALARMS) i++;

  if (i == LIVES_MAX_USER_ALARMS) mainw->next_free_alarm = ALL_USED; // no more alarm slots
  else mainw->next_free_alarm = i; // OK

  return ++ret;
}


/*** check if alarm time passed yet, if so clear that alarm and return TRUE
 * else return FALSE
 */
ticks_t lives_alarm_check(lives_alarm_t alarm_handle) {
  ticks_t cticks;

  // invalid alarm number
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid alarm handle");
    return -1;
  }

  // offset of 1 was added for caller
  alarm_handle--;

  // alarm time was never set !
  if (mainw->alarms[alarm_handle] == LIVES_NO_ALARM_TICKS) {
    LIVES_WARN("Alarm time not set");
    return 0;
  }

  // get current ticks
  cticks = mainw->alarms[alarm_handle] - lives_get_current_ticks();

  if (cticks <= 0) {
    // reached alarm time, free up this timer and return TRUE
    mainw->alarms[alarm_handle] = LIVES_NO_ALARM_TICKS;
    LIVES_DEBUG("Alarm reached");
    return 0;
  }

  // alarm time not reached yet
  return cticks;
}


boolean lives_alarm_clear(lives_alarm_t alarm_handle) {
  if (alarm_handle <= 0 || alarm_handle > LIVES_MAX_ALARMS) {
    LIVES_WARN("Invalid clear alarm handle");
    return FALSE;
  }

  mainw->alarms[--alarm_handle] = LIVES_NO_ALARM_TICKS;

  if (alarm_handle < LIVES_MAX_USER_ALARMS && (mainw->next_free_alarm == ALL_USED || alarm_handle < mainw->next_free_alarm)) {
    mainw->next_free_alarm = alarm_handle;
  }
  return TRUE;
}


char *lives_datetime(struct timeval *tv) {
  char buf[128];
  char *datetime = NULL;
  struct tm *gm = gmtime(&tv->tv_sec);
  ssize_t written;

  if (gm) {
    written = (ssize_t)strftime(buf, 128, "%Y-%m-%d    %H:%M:%S", gm);
    if ((written > 0) && ((size_t)written < 128)) {
      datetime = lives_strdup(buf);
    }
  }
  return datetime;
}


LIVES_GLOBAL_INLINE const char *lives_strappend(const char *string, int len, const char *xnew) {
  size_t sz = strlen(string);
  lives_snprintf((char *)(string + sz), len - sz, "%s", xnew);
  return string;
}


LIVES_GLOBAL_INLINE const char *lives_strappendf(const char *string, int len, const char *fmt, ...) {
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  lives_strappend(string, len, text);
  lives_free(text);
  return string;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_append_unique(LiVESList *xlist, const char *add) {
  LiVESList *list = xlist, *listlast = NULL;
  while (list != NULL) {
    listlast = list;
    if (!lives_utf8_strcasecmp((const char *)list->data, add)) return xlist;
    list = list->next;
  }
  list = lives_list_append(listlast, lives_strdup(add));
  if (xlist == NULL) return list;
  return xlist;
}


/* convert to/from a big endian 32 bit float for internal use */
LIVES_GLOBAL_INLINE float LEFloat_to_BEFloat(float f) {
  char *b = (char *)(&f);
  if (capable->byte_order == LIVES_LITTLE_ENDIAN) {
    float fl;
    uint8_t rev[4];
    rev[0] = b[3];
    rev[1] = b[2];
    rev[2] = b[1];
    rev[3] = b[0];
    fl = *(float *)rev;
    return fl;
  }
  return f;
}


LIVES_GLOBAL_INLINE double calc_time_from_frame(int clip, int frame) {
  return (frame - 1.) / mainw->files[clip]->fps;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, max is cfile->frames
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames) ? frame : mainw->files[filenum]->frames;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time2(int filenum, double time) {
  // return the nearest frame (rounded) for a given time
  // allow max (frames+1)
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time3(int filenum, double time) {
  // return the nearest frame (floor) for a given time
  // allow max (frames+1)
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.);
  return (frame < mainw->files[filenum]->frames + 1) ? frame : mainw->files[filenum]->frames + 1;
}


LIVES_GLOBAL_INLINE int calc_frame_from_time4(int filenum, double time) {
  // return the nearest frame (rounded) for a given time, no maximum
  int frame = 0;
  if (time < 0.) return mainw->files[filenum]->frames ? 1 : 0;
  frame = (int)(time * mainw->files[filenum]->fps + 1.49999);
  return frame;
}


static boolean check_for_audio_stop(int fileno, int first_frame, int last_frame) {
  // return FALSE if audio stops playback

#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && mainw->jackd != NULL && mainw->jackd->playing_file == fileno) {
    if (!mainw->loop) {
      if (!mainw->loop_cont) {
        if (mainw->aframeno < first_frame || mainw->aframeno > last_frame) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if (mainw->aframeno < 1 ||
            calc_time_from_frame(mainw->current_file, mainw->aframeno) > cfile->laudio_time) {
          return FALSE;
        }
      }
    }
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && mainw->pulsed != NULL && mainw->pulsed->playing_file == fileno) {
    if (!mainw->loop) {
      if (!mainw->loop_cont) {
        if (mainw->aframeno < first_frame || mainw->aframeno > last_frame) {
          return FALSE;
        }
      }
    } else {
      if (!mainw->loop_cont) {
        if (mainw->aframeno < 1 ||
            calc_time_from_frame(mainw->current_file, mainw->aframeno) > cfile->laudio_time) {
          return FALSE;
        }
      }
    }
  }
#endif
  return TRUE;
}


void calc_aframeno(int fileno) {
#ifdef ENABLE_JACK
  if (prefs->audio_player == AUD_PLAYER_JACK && ((mainw->jackd != NULL && mainw->jackd->playing_file == fileno) ||
      (mainw->jackd_read != NULL && mainw->jackd_read->playing_file == fileno))) {
    // get seek_pos from jack
    if (mainw->jackd_read != NULL) mainw->aframeno = lives_jack_get_pos(mainw->jackd_read) / cfile->fps + 1.;
    else mainw->aframeno = lives_jack_get_pos(mainw->jackd) / cfile->fps + 1.;
  }
#endif
#ifdef HAVE_PULSE_AUDIO
  if (prefs->audio_player == AUD_PLAYER_PULSE && ((mainw->pulsed != NULL && mainw->pulsed->playing_file == fileno) ||
      (mainw->pulsed_read != NULL && mainw->pulsed_read->playing_file == fileno))) {
    // get seek_pos from pulse
    if (mainw->pulsed_read != NULL) mainw->aframeno = lives_pulse_get_pos(mainw->pulsed_read) / cfile->fps + 1.;
    else mainw->aframeno = lives_pulse_get_pos(mainw->pulsed) / cfile->fps + 1.;
  }
#endif
}


int calc_new_playback_position(int fileno, ticks_t otc, ticks_t *ntc) {
  // returns a frame number (floor) using sfile->last_frameno and ntc-otc
  // takes into account looping modes

  // the range is first_frame -> last_frame

  // which is generally 1 -> sfile->frames, unless we are playing a selection

  // in case the frame is out of range and playing, returns 0 and sets mainw->cancelled

  // ntc is adjusted backwards to timecode of the new frame

  // the basic operation is quite simple, given the time difference between the last frame and
  // now, we calculate the new frame from the current fps and then ensure it is in the range
  // first_frame -> last_frame

  // Complications arise because we have ping-pong loop mode where the the play direction
  // alternates - here we need to determine how many times we have reached the start or end
  // play point. This is similar to the winding number in topological calculations.

  // caller should check return value of ntc, and if it differs from otc, show the frame

  // note we also calculate the audio "frame" and position for realtime audio players
  // this is done so we can check here if audio limits stopped playback

  int64_t dtc = *ntc - otc;
  lives_clip_t *sfile = mainw->files[fileno];

  int dir = 0;
  int cframe;

  static int nframe = -1;

  int first_frame, last_frame;

  boolean do_resync = FALSE;

  double fps;

  if (sfile == NULL) {
    nframe = -1;
    return 0;
  }

  if (nframe != -1) return nframe; // prevent infinite loops

  fps = sfile->pb_fps;

  if (!LIVES_IS_PLAYING) fps = sfile->fps;

  cframe = sfile->last_frameno;

  if (fps == 0.) {
    *ntc = otc;
    if (prefs->audio_src == AUDIO_SRC_INT) calc_aframeno(fileno);
    return cframe;
  }

  // dtc is delta ticks, quantise this to the frame rate and round down
  dtc = q_gint64_floor(dtc, fps);

  // ntc is the time when the frame should have been played
  *ntc = otc + dtc;

  // nframe is our new frame
  nframe = cframe + myround((double)dtc / TICKS_PER_SECOND_DBL * fps);
  if (nframe == cframe || mainw->foreign) {
    cframe = nframe;
    nframe = -1;
    return cframe;
  }

  // calculate audio "frame" from the number of samples played
  if (prefs->audio_src == AUDIO_SRC_INT && mainw->playing_file == fileno) {
    calc_aframeno(fileno);
  }

  if (mainw->playing_file == fileno && !mainw->clip_switched) {
    last_frame = (mainw->playing_sel && !mainw->is_rendering) ? sfile->end : mainw->play_end;
    if (last_frame > sfile->frames) last_frame = sfile->frames;
    first_frame = mainw->playing_sel ? sfile->start : mainw->loop_video ? mainw->play_start : 1;
    if (first_frame > sfile->frames) first_frame = sfile->frames;
  } else {
    last_frame = sfile->frames;
    first_frame = 1;
  }

  if (mainw->playing_file == fileno) {
    if (mainw->noframedrop) {
      // if noframedrop is set, we may not skip any frames
      // - the usual situation is that we are allowed to skip frames
      if (nframe > cframe) nframe = cframe + 1;
      else if (nframe < cframe) nframe = cframe - 1;
    }

    // check if video stopped playback
    if (IS_NORMAL_CLIP(fileno) && (nframe < first_frame || nframe > last_frame)) {
      if (mainw->whentostop == STOP_ON_VID_END) {
        mainw->cancelled = CANCEL_VID_END;
        nframe = -1;
        return 0;
      }
    }

    // check if audio stopped playback
#ifdef RT_AUDIO
    if (mainw->whentostop == STOP_ON_AUD_END && sfile->achans > 0 && sfile->frames > 0) {
      if (!check_for_audio_stop(fileno, first_frame, last_frame)) {
        mainw->cancelled = CANCEL_AUD_END;
        nframe = -1;
        return 0;
      }
    }
#endif
  }

  if (sfile->frames == 0) {
    nframe = -1;
    return 0;
  }

  // get our frame back to within bounds

  nframe -= first_frame;

  if (fps > 0) {
    dir = 0;
    if (mainw->ping_pong) {
      dir = (int)((double)nframe / (double)(last_frame - first_frame + 1));
      dir %= 2;
    }
  } else {
    dir = 1;
    if (mainw->ping_pong) {
      nframe -= (last_frame - first_frame);
      dir = (int)((double)nframe / (double)(last_frame - first_frame + 1));
      dir %= 2;
      dir++;
    }
  }

  nframe %= (last_frame - first_frame + 1);

  if (fps < 0) {
    // backwards
    if (dir == 1) {
      // even winding
      if (!mainw->ping_pong) {
        // loop
        if (nframe < 0) nframe += last_frame + 1;
        else nframe += first_frame;
        if (nframe > cframe && mainw->playing_file == fileno && mainw->loop_cont && !mainw->loop) {
          // resync audio at end of loop section (playing backwards)
          do_resync = TRUE;
        }
      } else {
        nframe += last_frame; // normal
        if (nframe > last_frame) {
          nframe = last_frame - (nframe - last_frame);
          if (mainw->playing_file == fileno) dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
          else sfile->pb_fps = -sfile->pb_fps;
        }
      }
    } else {
      // odd winding
      nframe = ABS(nframe) + first_frame;
      if (mainw->ping_pong) {
        // bounce
        if (mainw->playing_file == fileno) dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
        else sfile->pb_fps = -sfile->pb_fps;
      }
    }
  } else {
    // forwards
    nframe += first_frame;
    if (dir == 1) {
      // odd winding
      if (mainw->ping_pong) {
        // bounce
        nframe = last_frame - (nframe - (first_frame - 1));
        if (mainw->playing_file == fileno) dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
        else sfile->pb_fps = -sfile->pb_fps;
      }
    } else if (mainw->playing_sel && !mainw->ping_pong && mainw->playing_file == fileno && nframe < cframe && mainw->loop_cont &&
               !mainw->loop) {
      // resync audio at start of loop selection
      if (nframe < first_frame) {
        nframe = last_frame - (first_frame - nframe) + 1;
      }
      do_resync = TRUE;
    }
    if (nframe < first_frame) {
      // scratch or transport backwards
      if (mainw->ping_pong) {
        nframe = first_frame;
        if (mainw->playing_file == fileno) dirchange_callback(NULL, NULL, 0, (LiVESXModifierType)0, LIVES_INT_TO_POINTER(SCREEN_AREA_FOREGROUND));
        else sfile->pb_fps = -sfile->pb_fps;

      } else nframe = last_frame - nframe;
    }
  }

  if (nframe < first_frame) nframe = first_frame;
  if (nframe > last_frame) nframe = last_frame;

  if (prefs->audio_opts & AUDIO_OPTS_FOLLOW_FPS) {
    if (do_resync || (mainw->scratch != SCRATCH_NONE && mainw->playing_file == fileno)) {
      boolean is_jump = FALSE;
      if (mainw->scratch == SCRATCH_JUMP) is_jump = TRUE;
      mainw->scratch = SCRATCH_NONE;
      if (sfile->achans > 0) {
        resync_audio(nframe);
      }
      if (is_jump) mainw->video_seek_ready = TRUE;
      if (mainw->whentostop == STOP_ON_AUD_END && sfile->achans > 0) {
        // we check for audio stop here, but the seek may not have happened yet
        if (!check_for_audio_stop(fileno, first_frame, last_frame)) {
          mainw->cancelled = CANCEL_AUD_END;
          nframe = -1;
          return 0;
        }
      }
    }
  }
  cframe = nframe;
  nframe = -1;
  return cframe;
}


void calc_maxspect(int rwidth, int rheight, int *cwidth, int *cheight) {
  // calculate maxspect (maximum size which maintains aspect ratio)
  // of cwidth, cheight - given restrictions rwidth * rheight

  double aspect;

  if (*cwidth <= 0 || *cheight <= 0 || rwidth <= 0 || rheight <= 0) return;

  aspect = (double) * cwidth / (double) * cheight;

  *cwidth = rwidth;
  *cheight = (double)(*cwidth) / aspect;
  if (*cheight > rheight) {
    // image too tall shrink it
    *cheight = rheight;
    *cwidth = (double)(*cheight) * aspect;
  }
}


/////////////////////////////////////////////////////////////////////////////

void init_clipboard(void) {
  int current_file = mainw->current_file;
  char *com;

  if (clipboard == NULL) {
    // here is where we create the clipboard
    // use get_new_handle(clipnumber,name);
    if (!get_new_handle(0, "clipboard")) {
      mainw->error = TRUE;
      return;
    }
  } else if (clipboard->frames > 0) {
    // clear old clipboard
    // need to set current file to 0 before monitoring progress
    mainw->current_file = 0;
    cfile->cb_src = current_file;

    if (cfile->clip_type == CLIP_TYPE_FILE) {
      lives_freep((void **)&cfile->frame_index);
      close_decoder_plugin((lives_decoder_t *)cfile->ext_src);
      cfile->ext_src = NULL;
      cfile->clip_type = CLIP_TYPE_DISK;
    }

    mainw->com_failed = FALSE;
    com = lives_strdup_printf("%s delete_all \"%s\"", prefs->backend, clipboard->handle);
    lives_rm(clipboard->info_file);
    lives_system(com, FALSE);
    lives_free(com);

    if (mainw->com_failed) {
      mainw->current_file = current_file;
      return;
    }

    cfile->progress_start = cfile->start;
    cfile->progress_end = cfile->end;
    // show a progress dialog, not cancellable
    do_progress_dialog(TRUE, FALSE, _("Clearing the clipboard"));
  }

  clipboard->img_type = IMG_TYPE_BEST; // override the pref
  clipboard->cb_src = current_file;
  mainw->current_file = current_file;
}


weed_plant_t *get_nth_info_message(int n) {
  const char *leaf;
  int m = 0;
  int error;
  weed_plant_t *msg = mainw->msg_list;

  if (n < 0) return NULL;

  if (n >= mainw->n_messages) n = mainw->n_messages - 1;

  if (n >= (mainw->n_messages >> 1)) {
    m = mainw->n_messages - 1;
    msg = weed_get_plantptr_value(msg, WEED_LEAF_PREVIOUS, &error);
  }
  if (mainw->ref_message != NULL && ABS(mainw->ref_message_n - n) < ABS(m - n)) {
    m = mainw->ref_message_n;
    msg = mainw->ref_message;
  }

  if (m > n) leaf = WEED_LEAF_PREVIOUS;
  else leaf = WEED_LEAF_NEXT;

  while (m != n) {
    msg = weed_get_plantptr_value(msg, leaf, &error);
    if (error != WEED_NO_ERROR) return NULL;
    if (m > n) m--;
    else m++;
  }
  mainw->ref_message = msg;
  mainw->ref_message_n = n;
  return msg;
}


char *dump_messages(int start, int end) {
  weed_plant_t *msg = mainw->msg_list;
  char *text = lives_strdup(""), *tmp, *msgtext;
  boolean needs_newline = FALSE;
  int msgno = 0;
  int error;

  while (msg != NULL) {
    msgtext = weed_get_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
    if (error != WEED_NO_ERROR) break;
    if (msgno >= start) {
#ifdef SHOW_MSG_LINENOS
      tmp = lives_strdup_printf("%s%s(%d)%s", text, needs_newline ? "\n" : "", msgno, msgtext);
#else
      tmp = lives_strdup_printf("%s%s%s", text, needs_newline ? "\n" : "", msgtext);
#endif
      lives_free(text);
      text = tmp;
      needs_newline = TRUE;
    }
    lives_free(msgtext);
    if (++msgno > end) if (end > -1) break;
    msg = weed_get_plantptr_value(msg, WEED_LEAF_NEXT, &error);
    if (error != WEED_NO_ERROR) break;
  }
  return text;
}


static weed_plant_t *make_msg(const char *text) {
  // make single msg. text should have no newlines in it, except possibly as the last character.
  weed_plant_t *msg = weed_plant_new(WEED_PLANT_LIVES);
  if (msg == NULL) return NULL;

  weed_set_int_value(msg, WEED_LEAF_LIVES_SUBTYPE, LIVES_WEED_SUBTYPE_MESSAGE);
  weed_set_string_value(msg, WEED_LEAF_LIVES_MESSAGE_STRING, text);

  weed_set_plantptr_value(msg, WEED_LEAF_NEXT, NULL);
  weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, NULL);
  return msg;
}


int free_n_msgs(int frval) {
  int error;
  weed_plant_t *next, *end;

  if (frval <= 0) return WEED_NO_ERROR;
  if (frval > mainw->n_messages || mainw->msg_list == NULL) frval = mainw->n_messages;

  end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error); // list end
  if (error != WEED_NO_ERROR) {
    return error;
  }

  while (frval-- && mainw->msg_list != NULL) {
    next = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_NEXT, &error); // becomes new head
    if (error != WEED_NO_ERROR) {
      return error;
    }
    weed_plant_free(mainw->msg_list);
    mainw->msg_list = next;
    if (mainw->msg_list != NULL) {
      if (mainw->msg_list == end) weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, NULL);
      else weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, end);
    }
    mainw->n_messages--;
    if (mainw->ref_message != NULL) {
      if (--mainw->ref_message_n < 0) mainw->ref_message = NULL;
    }
  }

  if (mainw->msg_adj != NULL)
    lives_adjustment_set_value(mainw->msg_adj, lives_adjustment_get_value(mainw->msg_adj) - 1.);
  return WEED_NO_ERROR;
}




int add_messages_to_list(const char *text) {
  // append text to our message list, splitting it into lines
  // if we hit the max message limit then free the oldest one
  // returns a weed error
  weed_plant_t *msg, *end;;
  char **lines;
  int error, i, numlines;

  if (prefs->max_messages == 0) return WEED_NO_ERROR;
  if (text == NULL || strlen(text) == 0) return WEED_NO_ERROR;

  // split text into lines
  numlines = get_token_count(text, '\n');
  lines = lives_strsplit(text, "\n", numlines);

  for (i = 0; i < numlines; i++) {
    if (mainw->msg_list == NULL) {
      mainw->msg_list = make_msg(lines[i]);
      if (mainw->msg_list == NULL) {
        mainw->n_messages = 0;
        lives_strfreev(lines);
        return WEED_ERROR_MEMORY_ALLOCATION;
      }
      mainw->n_messages = 1;
      continue;
    }

    end = weed_get_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, &error);
    if (error != WEED_NO_ERROR) {
      lives_strfreev(lines);
      return error;
    }
    if (end == NULL) end = mainw->msg_list;

    if (i == 0) {
      // append first line to text of last msg
      char *strg2, *strg = weed_get_string_value(end, WEED_LEAF_LIVES_MESSAGE_STRING, &error);
      if (error != WEED_NO_ERROR) {
        lives_strfreev(lines);
        return error;
      }
      strg2 = lives_strdup_printf("%s%s", strg, lines[0]);
      weed_set_string_value(end, WEED_LEAF_LIVES_MESSAGE_STRING, strg2);
      lives_free(strg);
      lives_free(strg2);
      continue;
    }

    if (prefs->max_messages > 0 && mainw->n_messages + 1 > prefs->max_messages) {
      // retire the oldest if we reached the limit
      error = free_n_msgs(1);
      if (error != WEED_NO_ERROR) {
        lives_strfreev(lines);
        return error;
      }
      if (mainw->msg_list == NULL) {
        i = numlines - 2;
        continue;
      }
    }

    msg = make_msg(lines[i]);
    if (msg == NULL) {
      lives_strfreev(lines);
      return WEED_ERROR_MEMORY_ALLOCATION;
    }

    mainw->n_messages++;

    // head will get new previous (us)
    weed_set_plantptr_value(mainw->msg_list, WEED_LEAF_PREVIOUS, msg);
    // we will get new previous (end)
    weed_set_plantptr_value(msg, WEED_LEAF_PREVIOUS, end);
    // end will get new next (us)
    weed_set_plantptr_value(end, WEED_LEAF_NEXT, msg);
  }
  lives_strfreev(lines);
  return WEED_NO_ERROR;
}


boolean d_print_urgency(double timeout, const char *fmt, ...) {
  // overlay emergency message on playback frame
  va_list xargs;
  char *text;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  d_print(text);

  if (LIVES_IS_PLAYING && prefs->show_urgency_msgs) {
    int nfa = mainw->next_free_alarm;
    mainw->next_free_alarm = LIVES_URGENCY_ALARM;
    lives_freep((void **)&mainw->urgency_msg);
    lives_alarm_set(timeout * TICKS_PER_SECOND_DBL);
    mainw->next_free_alarm = nfa;
    mainw->urgency_msg = lives_strdup(text);
    lives_free(text);
    return TRUE;
  }
  lives_free(text);
  return FALSE;
}


void d_print(const char *fmt, ...) {
  // collect output for the main message area (and info log)

  // there are several small tweaks for this:

  // mainw->suppress_dprint :: TRUE - dont print anything, return (for silencing noisy message blocks)
  // mainw->no_switch_dprint :: TRUE - disable printing of switch message when maine->current_file changes

  // mainw->last_dprint_file :: clip number of last mainw->current_file;
  va_list xargs;

  char *tmp, *text;

  if (!prefs->show_gui) return;
  if (mainw->suppress_dprint) return;

  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);

  if (mainw->current_file != mainw->last_dprint_file && mainw->current_file != 0 && mainw->multitrack == NULL &&
      (mainw->current_file == -1 || (cfile != NULL && cfile->clip_type != CLIP_TYPE_GENERATOR)) && !mainw->no_switch_dprint) {
    if (mainw->current_file > 0) {
      char *swtext = lives_strdup_printf(_("\n==============================\nSwitched to clip %s\n"), tmp = get_menu_name(cfile, TRUE));
      lives_free(tmp);
      add_messages_to_list(swtext);
      lives_free(swtext);
    } else {
      add_messages_to_list(_("\n==============================\nSwitched to empty clip\n"));
    }
  }

  add_messages_to_list(text);
  lives_free(text);

  if (prefs->show_gui && mainw->msg_area != NULL && mainw->msg_adj != NULL) {
    msg_area_scroll_to_end(mainw->msg_area, mainw->msg_adj);
    lives_widget_queue_draw_if_visible(mainw->msg_area);
  }

  if ((mainw->current_file == -1 || (cfile != NULL && cfile->clip_type != CLIP_TYPE_GENERATOR)) &&
      (!mainw->no_switch_dprint || mainw->current_file != 0)) mainw->last_dprint_file = mainw->current_file;
}


static void d_print_utility(const char *text, int osc_note, const char *osc_detail) {
  boolean nsdp = mainw->no_switch_dprint;
  mainw->no_switch_dprint = TRUE;
  d_print(text);
  if (osc_note != LIVES_OSC_NOTIFY_NONE) lives_notify(osc_note, osc_detail);
  if (!nsdp) {
    mainw->no_switch_dprint = FALSE;
    d_print("");
  }
}


LIVES_GLOBAL_INLINE void d_print_cancelled(void) {
  d_print_utility(_("cancelled.\n"), LIVES_OSC_NOTIFY_CANCELLED, "");
}


LIVES_GLOBAL_INLINE void d_print_failed(void) {
  d_print_utility(_("failed.\n"), LIVES_OSC_NOTIFY_FAILED, "");
}


LIVES_GLOBAL_INLINE void d_print_done(void) {
  d_print_utility(_("done.\n"), 0, NULL);
}


LIVES_GLOBAL_INLINE void d_print_file_error_failed(void) {
  d_print_utility(_("error in file. Failed.\n"), 0, NULL);
}


LIVES_GLOBAL_INLINE void d_print_enough(int frames) {
  if (frames == 0) d_print_cancelled();
  else {
    char *msg = lives_strdup_printf(P_("%d frame is enough !\n", "%d frames are enough !\n", frames), frames);
    d_print_utility(msg, 0, NULL);
    lives_free(msg);
  }
}


void buffer_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  lmap_error *err = (lmap_error *)lives_malloc(sizeof(lmap_error));
  if (err == NULL) return;
  err->type = lerror;
  if (name != NULL) err->name = lives_strdup(name);
  else err->name = NULL;
  err->data = user_data;
  err->clipno = clipno;
  err->frameno = frameno;
  err->atime = atime;
  err->current = affects_current;
  mainw->new_lmap_errors = lives_list_prepend(mainw->new_lmap_errors, err);
}


void unbuffer_lmap_errors(boolean add) {
  LiVESList *list = mainw->new_lmap_errors;
  while (list != NULL) {
    lmap_error *err = (lmap_error *)list->data;
    if (add) add_lmap_error(err->type, err->name, err->data, err->clipno, err->frameno, err->atime, err->current);
    if (err->name != NULL) lives_free(err->name);
    lives_free(err);
    list = list->next;
  }
  if (mainw->new_lmap_errors != NULL) {
    lives_list_free(mainw->new_lmap_errors);
    mainw->new_lmap_errors = NULL;
  }
}


boolean add_lmap_error(lives_lmap_error_t lerror, const char *name, livespointer user_data, int clipno,
                       int frameno, double atime, boolean affects_current) {
  // potentially add a layout map error to the layout textbuffer
  LiVESTextIter end_iter;
  LiVESList *lmap;

  char *text, *name2;
  char **array;

  double orig_fps;
  double max_time;

  int resampled_frame;

  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);

  if (affects_current && user_data == NULL) {
    mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                   (livespointer)lives_text_buffer_create_mark
                                   (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    if (strlen(name) == 0) name2 = lives_strdup(_("(blank)"));
    else name2 = lives_strdup(name);
    text = lives_strdup_printf
           (_("The set name has been changed from %s to %s. Affected layouts have been updated accordingly\n"),
            name2, (char *)user_data);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(name2);
    lives_free(text);
    break;
  case LMAP_ERROR_MISSING_CLIP:
    if (prefs->warning_mask & WARN_MASK_LAYOUT_MISSING_CLIPS) return FALSE;
    text = lives_strdup_printf(_("The clip %s is missing from this set.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
  case LMAP_ERROR_CLOSE_FILE:
    text = lives_strdup_printf(_("The clip %s has been closed.\nIt is required by the following layouts:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
    text = lives_strdup_printf(_("Frames have been shifted in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_FRAMES:
    text = lives_strdup_printf(_("Frames have been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_DELETE_AUDIO:
    text = lives_strdup_printf(_("Audio has been deleted from the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
    text = lives_strdup_printf(_("Audio has been shifted in clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_AUDIO:
    text = lives_strdup_printf(_("Audio has been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  case LMAP_ERROR_ALTER_FRAMES:
    text = lives_strdup_printf(_("Frames have been altered in the clip %s.\nThe following layouts are affected:\n"), name);
    lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
    lives_free(text);
    break;
  }

  if (affects_current && user_data != NULL) {
    mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                   (livespointer)lives_text_buffer_create_mark
                                   (LIVES_TEXT_BUFFER(mainw->layout_textbuffer), NULL, &end_iter, TRUE));
  }

  switch (lerror) {
  case LMAP_INFO_SETNAME_CHANGED:
    lmap = mainw->current_layouts_map;
    while (lmap != NULL) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      // we could list all affected layouts, which could potentially be a lot !
      //mainw->affected_layouts_map=lives_list_append_unique(mainw->affected_layouts_map,array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_MISSING_CLIP:
  case LMAP_ERROR_CLOSE_FILE:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));

    }
    lmap = (LiVESList *)user_data;
    while (lmap != NULL) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      text = lives_strdup_printf("%s\n", array[0]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_FRAMES:
  case LMAP_ERROR_DELETE_FRAMES:
  case LMAP_ERROR_ALTER_FRAMES:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap != NULL) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      orig_fps = strtod(array[3], NULL);
      resampled_frame = count_resampled_frames(frameno, orig_fps, mainw->files[clipno]->fps);
      if (resampled_frame <= atoi(array[2])) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  case LMAP_ERROR_SHIFT_AUDIO:
  case LMAP_ERROR_DELETE_AUDIO:
  case LMAP_ERROR_ALTER_AUDIO:
    if (affects_current) {
      text = lives_strdup_printf("%s\n", mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
      lives_free(text);
      mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, mainw->string_constants[LIVES_STRING_CONSTANT_CL]);

      mainw->affected_layout_marks = lives_list_append(mainw->affected_layout_marks,
                                     (livespointer)lives_text_buffer_create_mark(LIVES_TEXT_BUFFER(mainw->layout_textbuffer),
                                         NULL, &end_iter, TRUE));
    }
    lmap = (LiVESList *)user_data;
    while (lmap != NULL) {
      array = lives_strsplit((char *)lmap->data, "|", -1);
      max_time = strtod(array[4], NULL);
      if (max_time > 0. && atime <= max_time) {
        text = lives_strdup_printf("%s\n", array[0]);
        lives_text_buffer_insert(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter, text, -1);
        lives_free(text);
        mainw->affected_layouts_map = lives_list_append_unique(mainw->affected_layouts_map, array[0]);
      }
      lives_strfreev(array);
      lmap = lmap->next;
    }
    break;
  }

  lives_widget_set_sensitive(mainw->show_layout_errors, TRUE);
  if (mainw->multitrack != NULL) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, TRUE);
  return TRUE;
}


void clear_lmap_errors(void) {
  LiVESTextIter start_iter, end_iter;
  LiVESList *lmap;

  lives_text_buffer_get_start_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter);
  lives_text_buffer_get_end_iter(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &end_iter);
  lives_text_buffer_delete(LIVES_TEXT_BUFFER(mainw->layout_textbuffer), &start_iter, &end_iter);

  lmap = mainw->affected_layouts_map;

  while (lmap != NULL) {
    lives_free((livespointer)lmap->data);
    lmap = lmap->next;
  }
  lives_list_free(lmap);

  mainw->affected_layouts_map = NULL;
  lives_widget_set_sensitive(mainw->show_layout_errors, FALSE);
  if (mainw->multitrack != NULL) lives_widget_set_sensitive(mainw->multitrack->show_layout_errors, FALSE);

  if (mainw->affected_layout_marks != NULL) {
    remove_current_from_affected_layouts(mainw->multitrack);
  }
}


boolean check_for_lock_file(const char *set_name, int type) {
  // check for lock file
  // do this via the back-end (smogrify)
  // this allows for the locking scheme to be more flexible

  // smogrify indicates a lock very simply by by writing >0 bytes to stdout
  // we redirect the output to info_file and read it

  char *msg = NULL;
  char *com = lives_strdup_printf("%s check_for_lock \"%s\" \"%s\" %d", prefs->backend_sync, set_name, capable->myname,
                                  capable->mainpid);

  threaded_dialog_spin(0.);
  mainw->com_failed = FALSE;
  lives_popen(com, TRUE, mainw->msg, MAINW_MSG_SIZE);
  threaded_dialog_spin(0.);
  lives_free(com);

  if (mainw->com_failed) return FALSE;

  if (strlen(mainw->msg) > 0) {
    if (type == 0) {
      if (mainw->recovering_files) {
        return do_set_locked_warning(set_name);
      }
      msg = lives_strdup_printf(_("Set %s\ncannot be opened, as it is in use\nby another copy of LiVES.\n"), set_name);
      threaded_dialog_spin(0.);
      do_error_dialog(msg);
      threaded_dialog_spin(0.);
    } else if (type == 1) {
      msg = lives_strdup_printf
            (_("\nThe set %s is currently in use by another copy of LiVES.\nPlease choose another set name.\n"), set_name);
      if (!mainw->osc_auto) do_blocking_error_dialog(msg);
    }
    if (msg != NULL) {
      lives_free(msg);
    }
    return FALSE;
  }
  return TRUE;
}


boolean do_std_checks(const char *type_name, const char *type, size_t maxlen, const char *nreject) {
  char *xtype = lives_strdup(type), *msg;
  const char *reject = " /\\*\"";

  size_t slen = strlen(type_name);

  register int i;

  if (nreject != NULL) reject = nreject;

  if (slen == 0) {
    msg = lives_strdup_printf(_("\n%s names may not be blank.\n"), xtype);
    if (!mainw->osc_auto) do_blocking_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (slen > MAX_SET_NAME_LEN) {
    msg = lives_strdup_printf(_("\n%s names may not be longer than %d characters.\n"), xtype, (int)maxlen);
    if (!mainw->osc_auto) do_blocking_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  if (strcspn(type_name, reject) != slen) {
    msg = lives_strdup_printf(_("\n%s names may not contain spaces or the characters%s.\n"), xtype, reject);
    if (!mainw->osc_auto) do_blocking_error_dialog(msg);
    lives_free(msg);
    lives_free(xtype);
    return FALSE;
  }

  for (i = 0; i < slen - 1; i++) {
    if (type_name[i] == '.' && (i == 0 || type_name[i + 1] == '.')) {
      msg = lives_strdup_printf(_("\n%s names may not start with a '.' or contain '..'\n"), xtype);
      if (!mainw->osc_auto) do_blocking_error_dialog(msg);
      lives_free(msg);
      lives_free(xtype);
      return FALSE;
    }
  }

  return TRUE;
}


boolean is_legal_set_name(const char *set_name, boolean allow_dupes) {
  // check (clip) set names for validity
  // - may not be of zero length
  // - may not contain spaces or characters / \ * "
  // - must NEVER be name of a set in use by another copy of LiVES (i.e. with a lock file)

  // - as of 1.6.0:
  // -  may not start with a .
  // -  may not contain ..

  // should be in FILESYSTEM encoding

  // may not be longer than MAX_SET_NAME_LEN chars

  // iff allow_dupes is FALSE then we disallow the name of any existing set (has a subdirectory in the working directory)

  char *msg;

  if (!do_std_checks(set_name, _("Set"), MAX_SET_NAME_LEN, NULL)) return FALSE;

  // check if this is a set in use by another copy of LiVES
  if (!check_for_lock_file(set_name, 1)) return FALSE;

  if (!allow_dupes) {
    // check for duplicate set names
    char *set_dir = lives_build_filename(prefs->workdir, set_name, NULL);
    if (lives_file_test(set_dir, LIVES_FILE_TEST_IS_DIR)) {
      lives_free(set_dir);
      msg = lives_strdup_printf(_("\nThe set %s already exists.\nPlease choose another set name.\n"), set_name);
      do_blocking_error_dialog(msg);
      lives_free(msg);
      return FALSE;
    }
    lives_free(set_dir);
  }

  return TRUE;
}


LIVES_GLOBAL_INLINE const char *get_image_ext_for_type(lives_image_type_t imgtype) {
  switch (imgtype) {
  case IMG_TYPE_JPEG:
    return LIVES_FILE_EXT_JPG; // "jpg"
  case IMG_TYPE_PNG:
    return LIVES_FILE_EXT_PNG; // "png"
  default:
    return "";
  }
}


LIVES_GLOBAL_INLINE lives_image_type_t lives_image_ext_to_type(const char *img_ext) {
  if (!strcmp(img_ext, LIVES_FILE_EXT_PNG)) return IMG_TYPE_PNG;
  if (!strcmp(img_ext, LIVES_FILE_EXT_JPG)) return IMG_TYPE_JPEG;
  return IMG_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE lives_image_type_t lives_image_type_to_image_type(const char *lives_img_type) {
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_PNG)) return IMG_TYPE_PNG;
  if (!strcmp(lives_img_type, LIVES_IMAGE_TYPE_JPEG)) return IMG_TYPE_JPEG;
  return IMG_TYPE_UNKNOWN;
}


LIVES_GLOBAL_INLINE char *make_image_file_name(lives_clip_t *sfile, int frame, const char *img_ext) {
  return lives_strdup_printf("%s/%s/%08d.%s", prefs->workdir, sfile->handle, frame, img_ext);
}


boolean check_frame_count(int idx) {
  // check number of frames is correct
  // for files of type CLIP_TYPE_DISK
  // - check the image files (e.g. jpeg or png)

  // use a "goldilocks" algorithm (just the right frames, not too few and not too many)

  // ingores gaps

  // make sure nth frame is there...
  char *frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames, get_image_ext_for_type(mainw->files[idx]->img_type));

  if (!lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
    // not enough frames
    lives_free(frame);
    return FALSE;
  }
  lives_free(frame);

  // ...make sure n + 1 th frame is not
  frame = make_image_file_name(mainw->files[idx], mainw->files[idx]->frames + 1, get_image_ext_for_type(mainw->files[idx]->img_type));

  if (lives_file_test(frame, LIVES_FILE_TEST_EXISTS)) {
    // too many frames
    lives_free(frame);
    return FALSE;
  }
  lives_free(frame);

  // just right
  return TRUE;
}


void count_opening_frames(void) {
  int cframes = cfile->frames;
  get_frame_count(mainw->current_file);
  mainw->opening_frames = cfile->frames;
  cfile->frames = cframes;
}


void get_frame_count(int idx) {
  // sets mainw->files[idx]->frames with current framecount

  // calls smogrify which physically finds the last frame using a (fast) O(log n) binary search method

  // for CLIP_TYPE_DISK only

  // (CLIP_TYPE_FILE should use the decoder plugin frame count)

  ssize_t bytes;

  char *com = lives_strdup_printf("%s count_frames \"%s\" %s", prefs->backend_sync, mainw->files[idx]->handle,
                                  get_image_ext_for_type(mainw->files[idx]->img_type));

  mainw->com_failed = FALSE;
  bytes = lives_popen(com, FALSE, mainw->msg, MAINW_MSG_SIZE);
  lives_free(com);

  if (mainw->com_failed) return;

  if (bytes > 0) {
    mainw->files[idx]->frames = atoi(mainw->msg);
  }
}


void get_frames_sizes(int fileno, int frame) {
  // get the actual physical frame size

  lives_clip_t *sfile = mainw->files[fileno];
  LiVESPixbuf *pixbuf;

  if ((pixbuf = pull_lives_pixbuf(fileno, frame, get_image_ext_for_type(mainw->files[fileno]->img_type), 0))) {
    sfile->hsize = lives_pixbuf_get_width(pixbuf);
    sfile->vsize = lives_pixbuf_get_height(pixbuf);
    lives_widget_object_unref(pixbuf);
  }
}


void get_next_free_file(void) {
  // get next free file slot, or -1 if we are full
  // can support MAX_FILES files (default 65536)
  while ((mainw->first_free_file != ALL_USED) && mainw->files[mainw->first_free_file] != NULL) {
    mainw->first_free_file++;
    if (mainw->first_free_file >= MAX_FILES) mainw->first_free_file = ALL_USED;
  }
}


boolean lives_string_ends_with(const char *string, const char *fmt, ...) {
  char *textx;
  va_list xargs;
  size_t slen, cklen;
  boolean ret = FALSE;

  if (string == NULL) return FALSE;

  va_start(xargs, fmt);
  textx = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  if (textx == NULL) return FALSE;
  slen = strlen(string);
  cklen = strlen(textx);
  if (cklen == 0 || cklen > slen) {
    lives_free(textx);
    return FALSE;
  }
  if (!strncmp(string + slen - cklen, textx, cklen)) ret = TRUE;
  lives_free(textx);
  return ret;
}


void get_dirname(char *filename) {
  char *tmp;
  // get directory name from a file
  //filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename

  lives_snprintf(filename, PATH_MAX, "%s%s", (tmp = lives_path_get_dirname(filename)), LIVES_DIR_SEP);
  if (!strcmp(tmp, ".")) {
    char *tmp1 = lives_get_current_dir(), *tmp2 = lives_build_filename(tmp1, filename + 2, NULL);
    lives_free(tmp1);
    lives_snprintf(filename, PATH_MAX, "%s", tmp2);
    lives_free(tmp2);
  }

  lives_free(tmp);
}


char *get_dir(const char *filename) {
  // get directory as string, should free after use
  char tmp[PATH_MAX];
  lives_snprintf(tmp, PATH_MAX, "%s", filename);
  get_dirname(tmp);
  return lives_strdup(tmp);
}


void get_basename(char *filename) {
  // get basename from a file
  // (filename without directory)
  // filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  char *tmp = lives_path_get_basename(filename);
  lives_snprintf(filename, PATH_MAX, "%s", tmp);
  lives_free(tmp);
}


void get_filename(char *filename, boolean strip_dir) {
  // get filename (part without extension) of a file
  //filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  char *ptr;
  if (strip_dir) get_basename(filename);
  ptr = strrchr(filename, '.');
  if (ptr != NULL) memset((void *)ptr, 0, 1);
}


char *get_extension(const char *filename) {
  // return file extension without the "."
  char *tmp = lives_path_get_basename(filename);
  char *ptr = strrchr(tmp, '.');
  if (ptr == NULL) {
    lives_free(tmp);
    return lives_strdup("");
  } else {
    char *ret = lives_strdup(ptr + 1);
    lives_free(tmp);
    return ret;
  }
}


char *ensure_extension(const char *fname, const char *ext) {
  // make sure filename fname has file extension ext
  // if ext does not begin with a "." we prepend one to the start of ext
  // we then check if fname ends with ext. If not we append ext to fname.
  // we return a copy of fname, possibly modified. The string returned should be freed after use.
  // NOTE: the original ext is not changed.

  size_t se = strlen(ext), sf;
  char *eptr = (char *)ext;

  if (fname == NULL) return NULL;

  if (se == 0) return lives_strdup(fname);

  if (eptr[0] == '.') {
    eptr++;
    se--;
  }

  sf = strlen(fname);
  if (sf < se + 1 || strcmp(fname + sf - se, eptr) || fname[sf - se - 1] != '.') {
    return lives_strconcat(fname, ".", eptr, NULL);
  }

  return lives_strdup(fname);
}


boolean ensure_isdir(char *fname) {
  // ensure dirname ends in a single dir separator
  // fname should be char[PATH_MAX]

  // returns TRUE if fname was altered

  size_t tlen = strlen(fname), slen, tlen2;
  size_t dslen = strlen(LIVES_DIR_SEP);
  ssize_t offs;
  boolean ret = FALSE;
  char *tmp = lives_strdup(fname), *tmp2;

  while (1) {
    // recursively remove double DIR_SEP
    tmp2 = subst(tmp, LIVES_DIR_SEP LIVES_DIR_SEP, LIVES_DIR_SEP);
    if ((tlen2 = strlen(tmp2)) < tlen) {
      ret = TRUE;
      lives_free(tmp);
      tmp = tmp2;
      tlen = tlen2;
    } else {
      lives_free(tmp2);
      break;
    }
  }

  if (ret) lives_snprintf(fname, PATH_MAX, "%s", tmp);
  lives_free(tmp);

  slen = tlen - 1;
  offs = slen;

  // we should now only have one or zero DIR_SEP at the end, but just in case we remove all but the last one
  while (offs >= 0 && !strncmp(fname + offs, LIVES_DIR_SEP, dslen)) offs -= dslen;
  if (offs == slen - dslen) return ret; // format is OK as-is

  // strip off all terminating DIR_SEP and then append one
  if (++offs < 0) offs = 0;
  if (offs < slen) fname[offs] = 0;
  fname = strncat(fname, LIVES_DIR_SEP, PATH_MAX - offs - 1);
  return TRUE;
}


boolean dirs_equal(const char *dira, const char *dirb) {
  // filenames in locale encoding
  char *tmp;
  char dir1[PATH_MAX];
  char dir2[PATH_MAX];
  lives_snprintf(dir1, PATH_MAX, "%s", (tmp = F2U8(dira)));
  lives_free(tmp);
  lives_snprintf(dir2, PATH_MAX, "%s", (tmp = F2U8(dirb)));
  lives_free(tmp);
  ensure_isdir(dir1);
  ensure_isdir(dir2);
  // TODO: for some (Linux) fstypes we should use strcasecmp
  // can get this using "df -T"
  return (!lives_utf8_strcmp(dir1, dir2));
}


void get_location(const char *exe, char *val, int maxlen) {
  // find location of "exe" in path
  // sets it in val which is a char array of maxlen bytes

  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_snprintf(val, maxlen, "%s", loc);
    lives_free(loc);
  } else {
    memset(val, 0, 1);
  }
}


LIVES_GLOBAL_INLINE boolean has_executable(const char *exe) {
  char *loc;
  if ((loc = lives_find_program_in_path(exe)) != NULL) {
    lives_free(loc);
    return TRUE;
  }
  return FALSE;
}



uint64_t get_version_hash(const char *exe, const char *sep, int piece) {
  /// get version hash output for an executable from the backend
  char val[16];
  char *com = lives_strdup_printf("%s get_version_hash \"%s\" \"%s\" %d", prefs->backend_sync, exe, sep, piece);
  lives_popen(com, TRUE, val, 16);
  lives_free(com);
  return strtol(val, NULL, 10);
}


#define VER_MAJOR_MULT 1000000
#define VER_MINOR_MULT 1000
#define VER_MICRO_MULT 1

uint64_t make_version_hash(const char *ver) {
  /// convert a version to uint64_t hash, for comparing

  uint64_t hash;
  int ntok;
  char **array;

  if (ver == NULL) return 0;

  ntok = get_token_count((char *)ver, '.');
  array = lives_strsplit(ver, ".", ntok);

  hash = atoi(array[0]) * VER_MAJOR_MULT;

  if (ntok > 1) {
    hash += atoi(array[1]) * VER_MINOR_MULT;
  }

  if (ntok > 2) {
    hash += atoi(array[2]) * VER_MICRO_MULT;
  }

  lives_strfreev(array);

  return hash;
}


char *repl_workdir(const char *entry, boolean fwd) {
  // replace prefs->workdir with string workdir or vice-versa. This allows us to relocate workdir if necessary.
  // used for layout.map file
  // return value should be freed

  // fwd TRUE replaces "/tmp/foo" with "workdir"
  // fwd FALSE replaces "workdir" with "/tmp/foo"

  char *string = lives_strdup(entry);

  if (fwd) {
    if (!strncmp(entry, prefs->workdir, strlen(prefs->workdir))) {
      lives_free(string);
      string = lives_strdup_printf("%s%s", WORKDIR_LITERAL, entry + strlen(prefs->workdir));
    }
  } else {
    if (!strncmp(entry, WORKDIR_LITERAL, WORKDIR_LITERAL_LEN)) {
      lives_free(string);
      string = lives_build_filename(prefs->workdir, entry + WORKDIR_LITERAL_LEN, NULL);
    }
  }
  return string;
}


void remove_layout_files(LiVESList *map) {
  // removes a LiVESList of layouts from the set layout map

  // removes from: - global layouts map
  //               - disk
  //               - clip layout maps

  // called after, for example: a clip is removed or altered and the user opts to remove all associated layouts

  LiVESList *lmap, *lmap_next, *cmap, *cmap_next, *map_next;

  size_t maplen;

  char **array;

  char *fname, *fdir;

  boolean is_current;

  register int i;

  while (map != NULL) {
    map_next = map->next;
    if (map->data != NULL) {
      if (!lives_utf8_strcasecmp((char *)map->data, mainw->string_constants[LIVES_STRING_CONSTANT_CL])) {
        is_current = TRUE;
        fname = lives_strdup(mainw->string_constants[LIVES_STRING_CONSTANT_CL]);
      } else {
        is_current = FALSE;
        maplen = strlen((char *)map->data);

        // remove from mainw->current_layouts_map
        cmap = mainw->current_layouts_map;
        while (cmap != NULL) {
          cmap_next = cmap->next;
          if (!lives_utf8_strcasecmp((char *)cmap->data, (char *)map->data)) {
            lives_free((livespointer)cmap->data);
            mainw->current_layouts_map = lives_list_delete_link(mainw->current_layouts_map, cmap);
            break;
          }
          cmap = cmap_next;
        }

        array = lives_strsplit((char *)map->data, "|", -1);
        fname = repl_workdir(array[0], FALSE);
        lives_strfreev(array);
      }

      // fname should now hold the layout name on disk
      d_print(_("Removing layout %s\n"), fname);

      if (!is_current) {
        lives_rm(fname);

        // if no more layouts in parent dir, we can delete dir

        // ensure that parent dir is below our own working dir
        if (!strncmp(fname, prefs->workdir, strlen(prefs->workdir))) {
          // is in workdir, safe to remove parents

          char *protect_file = lives_build_filename(prefs->workdir, "noremove", NULL);

          mainw->com_failed = FALSE;
          // touch a file in tpmdir, so we cannot remove workdir itself
          lives_touch(protect_file);

          if (!mainw->com_failed) {
            // ok, the "touch" worked
            // now we call rmdir -p : remove directory + any empty parents
            fdir = lives_path_get_dirname(fname);
            lives_rmdir_with_parents(fdir);
            lives_free(fdir);
          }

          // remove the file we touched to clean up
          lives_rm(protect_file);
          lives_free(protect_file);
        }

        // remove from mainw->files[]->layout_map
        for (i = 1; i <= MAX_FILES; i++) {
          if (mainw->files[i] != NULL) {
            if (mainw->files[i]->layout_map != NULL) {
              lmap = mainw->files[i]->layout_map;
              while (lmap != NULL) {
                lmap_next = lmap->next;
                if (!strncmp((char *)lmap->data, (char *)map->data, maplen)) {
                  lives_free((livespointer)lmap->data);
                  mainw->files[i]->layout_map = lives_list_delete_link(mainw->files[i]->layout_map, lmap);
                }
                lmap = lmap_next;
              }
            }
          }
        }
      } else {
        // asked to remove the currently loaded layout

        if (mainw->stored_event_list != NULL || mainw->sl_undo_mem != NULL) {
          // we are in CE mode, so event_list is in storage
          stored_event_list_free_all(TRUE);
        }
        // in mt mode we need to do more
        else remove_current_from_affected_layouts(mainw->multitrack);

        // and we dont want to try reloading this next time
        prefs->ar_layout = FALSE;
        set_string_pref(PREF_AR_LAYOUT, "");
        memset(prefs->ar_layout_name, 0, 1);
      }
      lives_free(fname);
    }
    map = map_next;
  }

  // save updated layout.map
  save_layout_map(NULL, NULL, NULL, NULL);
}


LIVES_GLOBAL_INLINE void get_play_times(void) {
  update_timer_bars(0, 0, 0, 0, 0);
}


void update_play_times(void) {
  // force a redraw, reread audio
  if (!CURRENT_CLIP_IS_VALID) return;
  if (cfile->audio_waveform != NULL) {
    int i;
    for (i = 0; i < cfile->achans; lives_freep((void **)&cfile->audio_waveform[i++]));
    lives_freep((void **)&cfile->audio_waveform);
    lives_freep((void **)&cfile->aw_sizes);
  }
  get_play_times();
}


void get_total_time(lives_clip_t *file) {
  // get times (video, left and right audio)

  file->laudio_time = file->raudio_time = file->video_time = 0.;

  if (file->opening && file->frames != 123456789) {
    if (file->frames * file->fps > 0) {
      file->video_time = file->frames / file->fps;
    }
    return;
  }

  if (file->fps > 0.) {
    file->video_time = file->frames / file->fps;
  }

  if (file->asampsize > 0 && file->arate > 0 && file->achans > 0) {
    file->laudio_time = (double)(file->afilesize / (file->asampsize >> 3) / file->achans) / (double)file->arate;
    if (file->achans > 1) {
      file->raudio_time = file->laudio_time;
    }
  }

  if (file->laudio_time + file->raudio_time == 0. && !file->opening) {
    file->achans = file->afilesize = file->asampsize = file->arate = file->arps = 0;
  }
}


void find_when_to_stop(void) {
  // work out when to stop playing
  //
  // ---------------
  //        no loop              loop to fit                 loop cont
  //        -------              -----------                 ---------
  // a>v    stop on video end    stop on audio end           no stop
  // v>a    stop on video end    stop on video end           no stop
  // generator start - not playing : stop on vid_end, unless pure audio;
  if (mainw->alives_pgid > 0) mainw->whentostop = NEVER_STOP;
  else if (cfile->clip_type == CLIP_TYPE_GENERATOR || (mainw->aud_rec_fd != -1 &&
           mainw->ascrap_file == -1)) mainw->whentostop = STOP_ON_VID_END;
  else if (mainw->multitrack != NULL && CURRENT_CLIP_HAS_VIDEO) mainw->whentostop = STOP_ON_VID_END;
  else if (!CURRENT_CLIP_IS_NORMAL) mainw->whentostop = NEVER_STOP;
  else if (cfile->opening_only_audio) mainw->whentostop = STOP_ON_AUD_END;
  else if (cfile->opening_audio) mainw->whentostop = STOP_ON_VID_END;
  else if (!mainw->preview && (mainw->loop_cont)) mainw->whentostop = NEVER_STOP;
  else if (!CURRENT_CLIP_HAS_VIDEO || (mainw->loop && cfile->achans > 0 && !mainw->is_rendering && (mainw->audio_end / cfile->fps)
                                       < MAX(cfile->laudio_time, cfile->raudio_time) &&
                                       calc_time_from_frame(mainw->current_file, mainw->play_start) < cfile->laudio_time))
    mainw->whentostop = STOP_ON_AUD_END;
  else mainw->whentostop = STOP_ON_VID_END; // tada...
}


#define ASPECT_ALLOWANCE 0.005

void minimise_aspect_delta(double aspect, int hblock, int vblock, int hsize, int vsize, int *width, int *height) {
  // we will use trigonometry to calculate the smallest difference between a given
  // aspect ratio and the actual frame size. If the delta is smaller than current
  // we set the height and width
  int cw = width[0];
  int ch = height[0];

  int real_width, real_height;
  uint64_t delta, current_delta;

  // minimise d[(x-x1)^2 + (y-y1)^2]/d[x1], to get approximate values
  int calc_width = (int)((vsize + aspect * hsize) * aspect / (aspect * aspect + 1.));

  int i;

  current_delta = (hsize - cw) * (hsize - cw) + (vsize - ch) * (vsize - ch);

#ifdef DEBUG_ASPECT
  lives_printerr("aspect %.8f : width %d height %d is best fit\n", aspect, calc_width, (int)(calc_width / aspect));
#endif
  // use the block size to find the nearest allowed size
  for (i = -1; i < 2; i++) {
    real_width = (int)(calc_width / hblock + i) * hblock;
    real_height = (int)(real_width / aspect / vblock + .5) * vblock;
    delta = (hsize - real_width) * (hsize - real_width) + (vsize - real_height) * (vsize - real_height);

    if (real_width % hblock != 0 || real_height % vblock != 0 || ABS((double)real_width / (double)real_height - aspect) > ASPECT_ALLOWANCE) {
      // encoders can be fussy, so we need to fit both aspect ratio and blocksize
      while (1) {
        real_width = ((int)(real_width / hblock) + 1) * hblock;
        real_height = (int)((double)real_width / aspect + .5);

        if (real_height % vblock == 0) break;

        real_height = ((int)(real_height / vblock) + 1) * vblock;
        real_width = (int)((double)real_height * aspect + .5);

        if (real_width % hblock == 0) break;
      }
    }

#ifdef DEBUG_ASPECT
    lives_printerr("block quantise to %d x %d\n", real_width, real_height);
#endif
    if (delta < current_delta) {
#ifdef DEBUG_ASPECT
      lives_printerr("is better fit\n");
#endif
      current_delta = delta;
      width[0] = real_width;
      height[0] = real_height;
    }
  }
}


void zero_spinbuttons(void) {
  lives_signal_handler_block(mainw->spinbutton_start, mainw->spin_start_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_start), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_start, mainw->spin_start_func);
  lives_signal_handler_block(mainw->spinbutton_end, mainw->spin_end_func);
  lives_spin_button_set_range(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0., 0.);
  lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_end), 0.);
  lives_signal_handler_unblock(mainw->spinbutton_end, mainw->spin_end_func);
}


boolean switch_aud_to_jack(boolean set_in_prefs) {
#ifdef ENABLE_JACK
  if (mainw->is_ready) {
    if (!mainw->jack_inited) lives_jack_init();
    if (mainw->jackd == NULL) {
      jack_audio_init();
      jack_audio_read_init();
      mainw->jackd = jack_get_driver(0, TRUE);
      if (!jack_create_client_writer(mainw->jackd)) {
        mainw->jackd = NULL;
        return FALSE;
      }
      mainw->jackd->whentostop = &mainw->whentostop;
      mainw->jackd->cancelled = &mainw->cancelled;
      mainw->jackd->in_use = FALSE;
      mainw->jackd->play_when_stopped = (prefs->jack_opts & JACK_OPTS_NOPLAY_WHEN_PAUSED) ? FALSE : TRUE;
      jack_write_driver_activate(mainw->jackd);
    }

    mainw->aplayer_broken = FALSE;
    lives_widget_show(mainw->vol_toolitem);
    if (mainw->vol_label != NULL) lives_widget_show(mainw->vol_label);
    lives_widget_show(mainw->recaudio_submenu);
    lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

    if (mainw->vpp != NULL && mainw->vpp->get_audio_fmts != NULL)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

#ifdef HAVE_PULSE_AUDIO
    if (mainw->pulsed_read != NULL) {
      pulse_close_client(mainw->pulsed_read);
      mainw->pulsed_read = NULL;
    }

    if (mainw->pulsed != NULL) {
      pulse_close_client(mainw->pulsed);
      mainw->pulsed = NULL;
      pulse_shutdown();
    }
#endif
  }
  prefs->audio_player = AUD_PLAYER_JACK;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_JACK);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_JACK);

  if (mainw->is_ready && mainw->vpp != NULL && mainw->vpp->get_audio_fmts != NULL)
    mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

  if (prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT) {
    jack_rec_audio_to_clip(-1, -1, RECA_MONITOR);
    mainw->jackd_read->in_use = FALSE;
  }

  lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
  lives_widget_set_sensitive(mainw->mute_audio, TRUE);
  lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
  lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

  return TRUE;
#endif
  return FALSE;
}


boolean switch_aud_to_pulse(boolean set_in_prefs) {
#ifdef HAVE_PULSE_AUDIO
  boolean retval;

  if (mainw->is_ready) {
    if ((retval = lives_pulse_init(-1))) {
      if (mainw->pulsed == NULL) {
        pulse_audio_init();
        pulse_audio_read_init();
        mainw->pulsed = pulse_get_driver(TRUE);
        mainw->pulsed->whentostop = &mainw->whentostop;
        mainw->pulsed->cancelled = &mainw->cancelled;
        mainw->pulsed->in_use = FALSE;
        pulse_driver_activate(mainw->pulsed);
      }
      mainw->aplayer_broken = FALSE;
      lives_widget_show(mainw->vol_toolitem);
      if (mainw->vol_label != NULL) lives_widget_show(mainw->vol_label);
      lives_widget_show(mainw->recaudio_submenu);
      lives_widget_set_sensitive(mainw->vol_toolitem, TRUE);

      prefs->audio_player = AUD_PLAYER_PULSE;
      if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_PULSE);
      lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_PULSE);

      if (mainw->vpp != NULL && mainw->vpp->get_audio_fmts != NULL)
        mainw->vpp->audio_codec = get_best_audio(mainw->vpp);
    }

#ifdef ENABLE_JACK
    if (mainw->jackd_read != NULL) {
      jack_close_device(mainw->jackd_read);
      mainw->jackd_read = NULL;
    }

    if (mainw->jackd != NULL) {
      jack_close_device(mainw->jackd);
      mainw->jackd = NULL;
    }
#endif

    if (prefs->perm_audio_reader && prefs->audio_src == AUDIO_SRC_EXT) {
      pulse_rec_audio_to_clip(-1, -1, RECA_MONITOR);
      mainw->pulsed_read->in_use = FALSE;
    }

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, TRUE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);

    return retval;
  }
#endif
  return FALSE;
}


boolean switch_aud_to_sox(boolean set_in_prefs) {
  if (!capable->has_sox_play) return FALSE; // TODO - show error

  prefs->audio_player = AUD_PLAYER_SOX;
  get_pref_default(PREF_SOX_COMMAND, prefs->audio_play_command, 256);
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_SOX);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_SOX);
  set_string_pref(PREF_AUDIO_PLAY_COMMAND, prefs->audio_play_command);

  if (mainw->is_ready) {
    /* //ubuntu / Unity has a hissy fit if you hide things in the menu !
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label != NULL) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp != NULL && mainw->vpp->get_audio_fmts != NULL)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, TRUE);
    lives_widget_set_sensitive(mainw->m_mutebutton, TRUE);
    lives_widget_set_sensitive(mainw->p_mutebutton, TRUE);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read != NULL) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd != NULL) {
    jack_close_device(mainw->jackd);
    mainw->jackd = NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read != NULL) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed != NULL) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
  return TRUE;
}


void switch_aud_to_none(boolean set_in_prefs) {
  prefs->audio_player = AUD_PLAYER_NONE;
  if (set_in_prefs) set_string_pref(PREF_AUDIO_PLAYER, AUDIO_PLAYER_NONE);
  lives_snprintf(prefs->aplayer, 512, "%s", AUDIO_PLAYER_NONE);

  if (mainw->is_ready) {
    /* //ubuntu has a hissy fit if you hide things in the menu
    lives_widget_hide(mainw->vol_toolitem);
    if (mainw->vol_label != NULL) lives_widget_hide(mainw->vol_label);
    */
    lives_widget_set_sensitive(mainw->vol_toolitem, FALSE);
    // lives_widget_hide(mainw->recaudio_submenu);

    if (mainw->vpp != NULL && mainw->vpp->get_audio_fmts != NULL)
      mainw->vpp->audio_codec = get_best_audio(mainw->vpp);

    pref_factory_bool(PREF_REC_EXT_AUDIO, FALSE, TRUE);

    lives_widget_set_sensitive(mainw->int_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->ext_audio_checkbutton, FALSE);
    lives_widget_set_sensitive(mainw->mute_audio, FALSE);
    lives_widget_set_sensitive(mainw->m_mutebutton, FALSE);
    lives_widget_set_sensitive(mainw->p_mutebutton, FALSE);
  }

#ifdef ENABLE_JACK
  if (mainw->jackd_read != NULL) {
    jack_close_device(mainw->jackd_read);
    mainw->jackd_read = NULL;
  }

  if (mainw->jackd != NULL) {
    jack_close_device(mainw->jackd);
    mainw->jackd = NULL;
  }
#endif

#ifdef HAVE_PULSE_AUDIO
  if (mainw->pulsed_read != NULL) {
    pulse_close_client(mainw->pulsed_read);
    mainw->pulsed_read = NULL;
  }

  if (mainw->pulsed != NULL) {
    pulse_close_client(mainw->pulsed);
    mainw->pulsed = NULL;
    pulse_shutdown();
  }
#endif
}


boolean prepare_to_play_foreign(void) {
  // here we are going to 'play' a captured external window

#ifdef GUI_GTK

#if !GTK_CHECK_VERSION(3, 0, 0)
#ifdef GDK_WINDOWING_X11
  GdkVisual *vissi = NULL;
  register int i;
#endif
#endif
#endif

  int new_file = mainw->first_free_file;

  mainw->foreign_window = NULL;

  // create a new 'file' to play into
  if (!get_new_handle(new_file, NULL)) {
    return FALSE;
  }

  mainw->current_file = new_file;

  if (mainw->rec_achans > 0) {
    cfile->arate = cfile->arps = mainw->rec_arate;
    cfile->achans = mainw->rec_achans;
    cfile->asampsize = mainw->rec_asamps;
    cfile->signed_endian = mainw->rec_signed_endian;
#ifdef HAVE_PULSE_AUDIO
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_PULSE) {
      pulse_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->pulsed_read->in_use = TRUE;
    }
#endif
#ifdef ENABLE_JACK
    if (mainw->rec_achans > 0 && prefs->audio_player == AUD_PLAYER_JACK) {
      jack_rec_audio_to_clip(mainw->current_file, -1, RECA_WINDOW_GRAB);
      mainw->jackd_read->in_use = TRUE;
    }
#endif
  }

  cfile->hsize = mainw->foreign_width / 2 + 1;
  cfile->vsize = mainw->foreign_height / 2 + 3;

  cfile->fps = cfile->pb_fps = mainw->rec_fps;

  resize(-2);

  lives_widget_show(mainw->playframe);
  lives_widget_show(mainw->playarea);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  // size must be exact, must not be larger than play window or we end up with nothing
  mainw->pwidth = lives_widget_get_allocation_width(mainw->playframe) - H_RESIZE_ADJUST + 2;
  mainw->pheight = lives_widget_get_allocation_height(mainw->playframe) - V_RESIZE_ADJUST + 2;

  cfile->hsize = mainw->pwidth;
  cfile->vsize = mainw->pheight;

  cfile->img_type = IMG_TYPE_BEST; // override the pref

#ifdef GUI_GTK
#if GTK_CHECK_VERSION(3, 0, 0)

#ifdef GDK_WINDOWING_X11
  mainw->foreign_window = gdk_x11_window_foreign_new_for_display
                          (mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].disp,
                           mainw->foreign_id);
#else
#ifdef GDK_WINDOWING_WIN32
  if (mainw->foreign_window == NULL)
    mainw->foreign_window = gdk_win32_window_foreign_new_for_display
                            (mainw->mgeom[prefs->gui_monitor > 0 ? prefs->gui_monitor - 1 : 0].disp,
                             mainw->foreign_id);
#endif

#endif // GDK_WINDOWING

  if (mainw->foreign_window != NULL) lives_xwindow_set_keep_above(mainw->foreign_window, TRUE);

#else // 3, 0, 0
  mainw->foreign_window = gdk_window_foreign_new(mainw->foreign_id);
#endif
#endif // GUI_GTK

#ifdef GUI_GTK
#ifdef GDK_WINDOWING_X11
#if !GTK_CHECK_VERSION(3, 0, 0)

  if (mainw->foreign_visual != NULL) {
    for (i = 0; i < capable->nmonitors; i++) {
      vissi = gdk_x11_screen_lookup_visual(mainw->mgeom[i].screen, hextodec(mainw->foreign_visual));
      if (vissi != NULL) break;
    }
  }

  if (vissi == NULL) vissi = gdk_visual_get_best_with_depth(mainw->foreign_bpp);

  if (vissi == NULL) return FALSE;

  mainw->foreign_cmap = gdk_x11_colormap_foreign_new(vissi,
                        gdk_x11_colormap_get_xcolormap(gdk_colormap_new(vissi, TRUE)));

  if (mainw->foreign_cmap == NULL) return FALSE;

#endif
#endif
#endif

  if (mainw->foreign_window == NULL) return FALSE;

  mainw->play_start = 1;
  if (mainw->rec_vid_frames == -1) mainw->play_end = INT_MAX;
  else mainw->play_end = mainw->rec_vid_frames;

  mainw->rec_samples = -1;

  omute = mainw->mute;
  osepwin = mainw->sep_win;
  ofs = mainw->fs;
  ofaded = mainw->faded;
  odouble = mainw->double_size;

  mainw->mute = TRUE;
  mainw->sep_win = FALSE;
  mainw->fs = FALSE;
  mainw->faded = TRUE;
  mainw->double_size = FALSE;

  lives_widget_hide(mainw->t_double);
  lives_widget_hide(mainw->t_bckground);
  lives_widget_hide(mainw->t_sepwin);
  lives_widget_hide(mainw->t_infobutton);

  return TRUE;
}


boolean after_foreign_play(void) {
  // read details from capture file
  int capture_fd;
  char *capfile = lives_strdup_printf("%s/.capture.%d", prefs->workdir, capable->mainpid);
  char capbuf[256];
  ssize_t length;
  int new_file = -1;
  int new_frames = 0;
  int old_file = mainw->current_file;

  char *com;
  char **array;
  char file_name[PATH_MAX];

  // assume for now we only get one clip passed back
  if ((capture_fd = open(capfile, O_RDONLY)) > -1) {
    memset(capbuf, 0, 256);
    if ((length = read(capture_fd, capbuf, 256))) {
      if (get_token_count(capbuf, '|') > 2) {
        array = lives_strsplit(capbuf, "|", 3);
        new_frames = atoi(array[1]);

        if (new_frames > 0) {
          new_file = mainw->first_free_file;
          mainw->current_file = new_file;
          cfile = (lives_clip_t *)(lives_malloc(sizeof(lives_clip_t)));
          lives_snprintf(cfile->handle, 255, "%s", array[0]);
          lives_strfreev(array);
          create_cfile();
          lives_snprintf(cfile->file_name, 256, "Capture %d", mainw->cap_number);
          lives_snprintf(cfile->name, CLIP_NAME_MAXLEN, "Capture %d", mainw->cap_number++);
          lives_snprintf(cfile->type, 40, "Frames");

          cfile->progress_start = cfile->start = 1;
          cfile->progress_end = cfile->frames = cfile->end = new_frames;
          cfile->pb_fps = cfile->fps = mainw->rec_fps;

          cfile->hsize = CEIL(mainw->foreign_width, 4);
          cfile->vsize = CEIL(mainw->foreign_height, 4);

          if (mainw->rec_achans > 0) {
            cfile->arate = cfile->arps = mainw->rec_arate;
            cfile->achans = mainw->rec_achans;
            cfile->asampsize = mainw->rec_asamps;
            cfile->signed_endian = mainw->rec_signed_endian;
          }

          // TODO - dirsep

          lives_snprintf(file_name, PATH_MAX, "%s/%s/", prefs->workdir, cfile->handle);

          com = lives_strdup_printf("%s fill_and_redo_frames \"%s\" %d %d %d \"%s\" %.4f %d %d %d %d %d",
                                    prefs->backend,
                                    cfile->handle, cfile->frames, cfile->hsize, cfile->vsize,
                                    get_image_ext_for_type(cfile->img_type), cfile->fps, cfile->arate,
                                    cfile->achans, cfile->asampsize, !(cfile->signed_endian & AFORM_UNSIGNED),
                                    !(cfile->signed_endian & AFORM_BIG_ENDIAN));
          lives_rm(cfile->info_file);
          mainw->com_failed = FALSE;
          lives_system(com, FALSE);

          cfile->nopreview = TRUE;
          if (!mainw->com_failed && do_progress_dialog(TRUE, TRUE, _("Cleaning up clip"))) {
            get_next_free_file();
          } else {
            // cancelled cleanup
            new_frames = 0;
            close_current_file(old_file);
          }
          lives_free(com);
        } else lives_strfreev(array);
      }
      close(capture_fd);
      lives_rm(capfile);
    }
  }

  if (new_frames == 0) {
    // nothing captured; or cancelled
    lives_free(capfile);
    return FALSE;
  }

  cfile->nopreview = FALSE;
  lives_free(capfile);

  add_to_clipmenu();
  if (mainw->multitrack == NULL) switch_to_file(old_file, mainw->current_file);

  else {
    mainw->current_file = mainw->multitrack->render_file;
    mt_init_clips(mainw->multitrack, new_file, TRUE);
    mt_clip_select(mainw->multitrack, TRUE);
  }

  cfile->is_loaded = TRUE;
  cfile->changed = TRUE;
  save_clip_values(mainw->current_file);
  if (prefs->crash_recovery) add_to_recovery_file(cfile->handle);
  lives_notify(LIVES_OSC_NOTIFY_CLIP_OPENED, "");
  return TRUE;
}


LIVES_GLOBAL_INLINE boolean int_array_contains_value(int *array, int num_elems, int value) {
  int i;
  for (i = 0; i < num_elems; i++) {
    if (array[i] == value) return TRUE;
  }
  return FALSE;
}


void reset_clipmenu(void) {
  // sometimes the clip menu gets messed up, e.g. after reloading a set.
  // This function will clean up the 'x's and so on.

  if (mainw->current_file > 0 && cfile != NULL && cfile->menuentry != NULL) {
#ifdef GTK_RADIO_MENU_BUG
    register int i;
    for (i = 1; i < MAX_FILES; i++) {
      if (i != mainw->current_file && mainw->files[i] != NULL && mainw->files[i]->menuentry != NULL) {
        lives_signal_handler_block(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
        lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(mainw->files[i]->menuentry), FALSE);
        lives_signal_handler_unblock(mainw->files[i]->menuentry, mainw->files[i]->menuentry_func);
      }
    }
#endif
    lives_signal_handler_block(cfile->menuentry, cfile->menuentry_func);
    lives_check_menu_item_set_active(LIVES_CHECK_MENU_ITEM(cfile->menuentry), TRUE);
    lives_signal_handler_unblock(cfile->menuentry, cfile->menuentry_func);
  }
}


boolean check_file(const char *file_name, boolean check_existing) {
  int check;
  boolean exists = FALSE;
  char *msg;
  // file_name should be in utf8
  char *lfile_name = U82F(file_name);

  mainw->error = FALSE;

  // check if file exists
  if (lives_file_test(lfile_name, LIVES_FILE_TEST_EXISTS)) {
    if (check_existing) {
      msg = lives_strdup_printf(_("\n%s\nalready exists.\n\nOverwrite ?\n"), file_name);
      if (!do_warning_dialog(msg)) {
        lives_free(msg);
        lives_free(lfile_name);
        return FALSE;
      }
      lives_free(msg);
    }
    check = open(lfile_name, O_WRONLY);
    exists = TRUE;
  }
  // if not, check if we can write to it
  else {
    check = open(lfile_name, O_CREAT | O_EXCL | O_WRONLY, DEF_FILE_PERMS);
  }

  if (check < 0) {
    mainw->error = TRUE;
    if (mainw != NULL && mainw->is_ready) {
      if (errno == EACCES)
        do_file_perm_error(lfile_name);
      else
        do_write_failed_error_s(lfile_name, NULL);
    }
    lives_free(lfile_name);
    return FALSE;
  }

  close(check);
  if (!exists) {
    lives_rm(lfile_name);
  }
  lives_free(lfile_name);
  return TRUE;
}


int lives_rmdir(const char *dir, boolean force) {
  // if force is TRUE, removes non-empty dirs, otherwise leaves them
  // may fail
  char *com;
  char *cmd;

  int retval;

  if (force) {
    cmd = lives_strdup_printf("%s -rf", capable->rm_cmd);
  } else {
    cmd = lives_strdup(capable->rmdir_cmd);
  }

  com = lives_strdup_printf("%s \"%s/\" >\"%s\" 2>&1", cmd, dir, prefs->cmd_log);
  retval = lives_system(com, TRUE);
  lives_free(com);
  lives_free(cmd);
  return retval;
}


int lives_rmdir_with_parents(const char *dir) {
  // may fail, will not remove empty dirs
  char *com = lives_strdup_printf("%s -p \"%s\" >\"%s\" 2>&1", capable->rmdir_cmd, dir, prefs->cmd_log);
  int retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_rm(const char *file) {
  // may fail
  char *com;
  int retval;

  com = lives_strdup_printf("%s -f \"%s\" >\"%s\" 2>&1", capable->rm_cmd, file, prefs->cmd_log);
  retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_rmglob(const char *files) {
  // delete files with name "files"*
  // may fail
  char *com;
  int retval;
  com = lives_strdup_printf("%s \"%s\"* >\"%s\" 2>&1", capable->rm_cmd, files, prefs->cmd_log);
  retval = lives_system(com, TRUE);
  lives_free(com);
  return retval;
}


int lives_cp(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cp_recursive(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s -r \"%s\" \"%s\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cp_keep_perms(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s -a \"%s\" \"%s/\" >\"%s\" 2>&1", capable->cp_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_mv(const char *from, const char *to) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" \"%s\" >\"%s\" 2>&1", capable->mv_cmd, from, to, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_touch(const char *tfile) {
  // may not fail
  char *com = lives_strdup_printf("%s \"%s\" >\"%s\" 2>&1", capable->touch_cmd, tfile, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_ln(const char *from, const char *to) {
  // may not fail
  char *com;
  int retval;
  com = lives_strdup_printf("%s -s \"%s\" \"%s\" >\"%s\" 2>&1", capable->ln_cmd, from, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_chmod(const char *target, const char *mode) {
  // may not fail
  char *com = lives_strdup_printf("%s %s \"%s\" >\"%s\" 2>&1", capable->chmod_cmd, mode, target, prefs->cmd_log);
  int retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_cat(const char *from, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" >\"%s\" 2>&1", capable->cat_cmd, from, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


int lives_echo(const char *text, const char *to, boolean append) {
  // may not fail
  char *com;
  char *op;
  int retval;

  if (append) op = ">>";
  else op = ">";

  com = lives_strdup_printf("%s \"%s\" %s \"%s\" 2>\"%s\"", capable->echo_cmd, text, op, to, prefs->cmd_log);
  retval = lives_system(com, FALSE);
  lives_free(com);
  return retval;
}


void lives_kill_subprocesses(const char *dirname, boolean kill_parent) {
  char *com;
  if (kill_parent)
    com = lives_strdup_printf("%s stopsubsub \"%s\" 2>/dev/null", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsubs \"%s\" 2>/dev/null", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);
}


void lives_suspend_resume_process(const char *dirname, boolean suspend) {
  char *com;
  if (!suspend)
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGCONT 2>/dev/null", prefs->backend_sync, dirname);
  else
    com = lives_strdup_printf("%s stopsubsub \"%s\" SIGTSTP 2>/dev/null", prefs->backend_sync, dirname);
  lives_system(com, TRUE);
  lives_free(com);

  com = lives_strdup_printf("%s resume \"%s\"", prefs->backend_sync, dirname);
  lives_system(com, FALSE);
  lives_free(com);
}


boolean check_dir_access(const char *dir) {
  // if a directory exists, make sure it is readable and writable
  // otherwise create it and then check

  // dir is in locale encoding

  // see also is_writeable_dir() which uses statvfs

  // WARNING: may leave some parents around

  boolean exists = lives_file_test(dir, LIVES_FILE_TEST_EXISTS);
  boolean is_OK = FALSE;

  char *testfile;

  if (!exists) {
    lives_mkdir_with_parents(dir, capable->umask);
  }

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) return FALSE;

  testfile = lives_build_filename(dir, "livestst.txt", NULL);
  lives_touch(testfile);
  if ((is_OK = lives_file_test(testfile, LIVES_FILE_TEST_EXISTS))) {
    lives_rm(testfile);
  }
  lives_free(testfile);
  if (!exists) {
    lives_rmdir(dir, FALSE);
  }
  return is_OK;
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


void activate_url_inner(const char *link) {
#if GTK_CHECK_VERSION(2, 14, 0)
  LiVESError *err = NULL;
  gtk_show_uri(NULL, link, GDK_CURRENT_TIME, &err);
#else
  char *com = getenv("BROWSER");
  com = lives_strdup_printf("\"%s\" '%s' &", com ? com : "gnome-open", link);
  lives_system(com, FALSE);
  lives_free(com);
#endif
}


void activate_url(LiVESAboutDialog *about, const char *link, livespointer data) {
  activate_url_inner(link);
}


void show_manual_section(const char *lang, const char *section) {
  char *tmp = NULL, *tmp2 = NULL;
  const char *link;

  link = lives_strdup_printf("%s%s%s%s", LIVES_MANUAL_URL, (lang == NULL ? "" : (tmp2 = lives_strdup_printf("//%s//", lang))),
                             LIVES_MANUAL_FILENAME, (section == NULL ? "" : (tmp = lives_strdup_printf("#%s", section))));

  activate_url_inner(link);

  if (tmp != NULL) lives_free(tmp);
  if (tmp2 != NULL) lives_free(tmp2);
}


uint64_t get_file_size(int fd) {
  // get the size of file fd
  struct stat filestat;
  fstat(fd, &filestat);
  return (uint64_t)(filestat.st_size);
}


uint64_t sget_file_size(const char *name) {
  // get the size of file fd
  struct stat filestat;
  int fd;

  if ((fd = open(name, O_RDONLY)) == -1) {
    return (uint32_t)0;
  }

  fstat(fd, &filestat);
  close(fd);

  return (uint64_t)(filestat.st_size);
}


void wait_for_bg_audio_sync(int fileno) {
  char *afile = lives_get_audio_file_name(fileno);
  lives_alarm_t alarm_handle = lives_alarm_set(LIVES_SHORTEST_TIMEOUT);
  int fd;

  while ((fd = open(afile, O_RDONLY)) < 0 && lives_alarm_check(alarm_handle) > 0) {
    lives_sync(1);
    lives_usleep(prefs->sleep_time);
  }
  lives_alarm_clear(alarm_handle);

  if (fd >= 0) close(fd);
  lives_free(afile);
}


uint64_t reget_afilesize_inner(int fileno) {
  // safe version that just returns the audio file size
  uint64_t filesize;
  char *afile = lives_get_audio_file_name(fileno);
  lives_sync(1);
  filesize = sget_file_size(afile);
  lives_free(afile);
  return filesize;
}


void reget_afilesize(int fileno) {
  // re-get the audio file size
  lives_clip_t *sfile = mainw->files[fileno];
  boolean bad_header = FALSE;

  if (mainw->multitrack != NULL) return; // otherwise achans gets set to 0...

  sfile->afilesize = reget_afilesize_inner(fileno);

  if (sfile->afilesize == 0l) {
    if (!sfile->opening && fileno != mainw->ascrap_file && fileno != mainw->scrap_file) {
      if (sfile->arate != 0 || sfile->achans != 0 || sfile->asampsize != 0 || sfile->arps != 0) {
        sfile->arate = sfile->achans = sfile->asampsize = sfile->arps = 0;
        save_clip_value(fileno, CLIP_DETAILS_ACHANS, &sfile->achans);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ARATE, &sfile->arps);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_PB_ARATE, &sfile->arate);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        save_clip_value(fileno, CLIP_DETAILS_ASAMPS, &sfile->asampsize);
        if (mainw->com_failed || mainw->write_failed) bad_header = TRUE;
        if (bad_header) do_header_write_error(fileno);
      }
    }
  }

  if (mainw->is_ready && fileno > 0 && fileno == mainw->current_file) {
    // force a redraw
    update_play_times();
  }
}


boolean create_event_space(int length) {
  // try to create desired events
  // if we run out of memory, all events requested are freed, and we return FALSE
  // otherwise we return TRUE

  // NOTE: this is the OLD event system, it's only used for reordering in the clip editor

  if (cfile->resample_events != NULL) {
    lives_free(cfile->resample_events);
  }
  if ((cfile->resample_events = (event *)(lives_try_malloc(sizeof(event) * length))) == NULL) {
    // memory overflow
    return FALSE;
  }
  memset(cfile->resample_events, 0, length * sizeof(event));
  return TRUE;
}


int lives_list_strcmp_index(LiVESList *list, livesconstpointer data, boolean case_sensitive) {
  // find data in list, using strcmp
  int i;
  int len;
  if (list == NULL) return -1;

  len = lives_list_length(list);

  if (case_sensitive) {
    for (i = 0; i < len; i++) {
      if (!lives_utf8_strcmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
      if (!lives_utf8_strcmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
    }
  } else {
    for (i = 0; i < len; i++) {
      if (!lives_utf8_strcasecmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
      if (!lives_utf8_strcasecmp((const char *)lives_list_nth_data(list, i), (const char *)data)) return i;
    }
  }
  return -1;
}


void add_to_recent(const char *filename, double start, int frames, const char *extra_params) {
  const char *mtext;
  char *file;

  if (frames > 0) {
    if (extra_params == NULL || (strlen(extra_params) == 0)) file = lives_strdup_printf("%s|%.2f|%d", filename, start, frames);
    else file = lives_strdup_printf("%s|%.2f|%d\n%s", filename, start, frames, extra_params);
  } else {
    if (extra_params == NULL || (strlen(extra_params) == 0)) file = lives_strdup(filename);
    else file = lives_strdup_printf("%s\n%s", filename, extra_params);
  }

  mtext = lives_menu_item_get_text(mainw->recent[0]);
  if (strcmp(file, mtext)) {
    mtext = lives_menu_item_get_text(mainw->recent[1]);
    if (strcmp(file, mtext)) {
      mtext = lives_menu_item_get_text(mainw->recent[2]);
      if (strcmp(file, mtext)) {
        // not in list, or at pos 4
        mtext = lives_menu_item_get_text(mainw->recent[2]);
        lives_menu_item_set_text(mainw->recent[3], mtext, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[3], mtext, FALSE);
        set_utf8_pref(PREF_RECENT4, mtext);

        mtext = lives_menu_item_get_text(mainw->recent[1]);
        lives_menu_item_set_text(mainw->recent[2], mtext, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[2], mtext, FALSE);
        set_utf8_pref(PREF_RECENT3, mtext);

        mtext = lives_menu_item_get_text(mainw->recent[0]);
        lives_menu_item_set_text(mainw->recent[1], mtext, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[1], mtext, FALSE);
        set_utf8_pref(PREF_RECENT2, mtext);

        lives_menu_item_set_text(mainw->recent[0], file, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[0], file, FALSE);
        set_utf8_pref(PREF_RECENT1, file);
      } else {
        // #3 in list
        mtext = lives_menu_item_get_text(mainw->recent[1]);
        lives_menu_item_set_text(mainw->recent[2], mtext, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[2], mtext, FALSE);
        set_utf8_pref(PREF_RECENT3, mtext);

        mtext = lives_menu_item_get_text(mainw->recent[0]);
        lives_menu_item_set_text(mainw->recent[1], mtext, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[1], mtext, FALSE);
        set_utf8_pref(PREF_RECENT2, mtext);

        lives_menu_item_set_text(mainw->recent[0], file, FALSE);
        if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[0], file, FALSE);
        set_utf8_pref(PREF_RECENT1, file);
      }
    } else {
      // #2 in list
      mtext = lives_menu_item_get_text(mainw->recent[0]);
      lives_menu_item_set_text(mainw->recent[1], mtext, FALSE);
      if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[1], mtext, FALSE);
      set_utf8_pref(PREF_RECENT2, mtext);

      lives_menu_item_set_text(mainw->recent[0], file, FALSE);
      if (mainw->multitrack != NULL) lives_menu_item_set_text(mainw->multitrack->recent[0], file, FALSE);
      set_utf8_pref(PREF_RECENT1, file);
    }
  } else {
    // I'm number 1, so why change ;-)
  }

  mtext = lives_menu_item_get_text(mainw->recent[0]);
  if (strlen(mtext)) {
    lives_widget_show(mainw->recent[0]);
  }
  mtext = lives_menu_item_get_text(mainw->recent[1]);
  if (strlen(mtext)) {
    lives_widget_show(mainw->recent[1]);
  }
  mtext = lives_menu_item_get_text(mainw->recent[2]);
  if (strlen(mtext)) {
    lives_widget_show(mainw->recent[2]);
  }
  mtext = lives_menu_item_get_text(mainw->recent[3]);
  if (strlen(mtext)) {
    lives_widget_show(mainw->recent[3]);
  }

  lives_free(file);
}


int verhash(char *xv) {
  char *version;
  char *s;
  int major = 0;
  int minor = 0;
  int micro = 0;

  if (xv == NULL) return 0;

  version = lives_strdup(xv);

  if (!(strlen(version))) {
    lives_free(version);
    return 0;
  }

  s = strtok(version, ".");
  if (!(s == NULL)) {
    major = atoi(s);
    s = strtok(NULL, ".");
    if (!(s == NULL)) {
      minor = atoi(s);
      s = strtok(NULL, ".");
      if (!(s == NULL)) {
        micro = atoi(s);
      }
    }
  }
  lives_free(version);
  return major * 1000000 + minor * 1000 + micro;
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


// TODO - move into undo.c
void set_undoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->redoable = FALSE;
    cfile->undoable = sensitive;
    if (!(what == NULL)) {
      char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
      lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
      lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
      lives_free(what_safe);
    } else {
      cfile->undoable = FALSE;
      cfile->undo_action = UNDO_NONE;
      lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
      lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
    }
    lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
    lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
  }

  lives_widget_hide(mainw->redo);
  lives_widget_show(mainw->undo);
  lives_widget_set_sensitive(mainw->undo, sensitive);

#ifdef PRODUCE_LOG
  lives_log(what);
#endif
}


void set_redoable(const char *what, boolean sensitive) {
  if (mainw->current_file > -1) {
    cfile->undoable = FALSE;
    cfile->redoable = sensitive;
    if (!(what == NULL)) {
      char *what_safe = lives_strdelimit(lives_strdup(what), "_", ' ');
      lives_snprintf(cfile->undo_text, 32, _("_Undo %s"), what_safe);
      lives_snprintf(cfile->redo_text, 32, _("_Redo %s"), what_safe);
      lives_free(what_safe);
    } else {
      cfile->redoable = FALSE;
      cfile->undo_action = UNDO_NONE;
      lives_snprintf(cfile->undo_text, 32, "%s", _("_Undo"));
      lives_snprintf(cfile->redo_text, 32, "%s", _("_Redo"));
    }
    lives_menu_item_set_text(mainw->undo, cfile->undo_text, TRUE);
    lives_menu_item_set_text(mainw->redo, cfile->redo_text, TRUE);
  }

  lives_widget_hide(mainw->undo);
  lives_widget_show(mainw->redo);
  lives_widget_set_sensitive(mainw->redo, sensitive);
}


void set_sel_label(LiVESWidget *sel_label) {
  char *tstr, *frstr, *tmp;
  char *sy, *sz;

  if (mainw->current_file == -1 || !cfile->frames || mainw->multitrack != NULL) {
    lives_label_set_text(LIVES_LABEL(sel_label), _("-------------Selection------------"));
  } else {
    tstr = lives_strdup_printf("%.2f", calc_time_from_frame(mainw->current_file, cfile->end + 1) -
                               calc_time_from_frame(mainw->current_file, cfile->start));
    frstr = lives_strdup_printf("%d", cfile->end - cfile->start + 1);

    // TRANSLATORS: - try to keep the text of the middle part the same length, by deleting "-" if necessary
    lives_label_set_text(LIVES_LABEL(sel_label),
                         (tmp = lives_strconcat("---------- [ ", tstr, (sy = (lives_strdup(_(" sec ] ----------Selection---------- [ ")))),
                                frstr, (sz = lives_strdup(_(" frames ] ----------"))), NULL)));
    lives_free(sy);
    lives_free(sz);

    lives_free(tmp);
    lives_free(frstr);
    lives_free(tstr);
  }
  lives_widget_queue_draw(sel_label);
}


LIVES_GLOBAL_INLINE void lives_list_free_strings(LiVESList *list) {
  register int i;

  if (list == NULL) return;

  for (i = 0; i < lives_list_length(list); i++) {
    if (lives_list_nth_data(list, i) != NULL) {
      //lives_printerr("free %s\n",list->data);
      lives_free((livespointer)lives_list_nth_data(list, i));
    }
  }
}


LIVES_GLOBAL_INLINE void lives_slist_free_all(LiVESSList **list) {
  if (*list == NULL) return;
  lives_list_free_strings((LiVESList *)*list);
  lives_slist_free(*list);
  *list = NULL;
}


LIVES_GLOBAL_INLINE void lives_list_free_all(LiVESList **list) {
  if (*list == NULL) return;
  lives_list_free_strings(*list);
  lives_list_free(*list);
  *list = NULL;
}


boolean cache_file_contents(const char *filename) {
  FILE *hfile;
  char buff[65536];

  lives_list_free_all(&mainw->cached_list);

  if (!(hfile = fopen(filename, "r"))) return FALSE;
  while (fgets(buff, 65536, hfile) != NULL) {
    mainw->cached_list = lives_list_append(mainw->cached_list, lives_strdup(buff));
    threaded_dialog_spin(0.);
  }
  fclose(hfile);
  return TRUE;
}


char *get_val_from_cached_list(const char *key, size_t maxlen) {
  LiVESList *clist = mainw->cached_list;
  char *keystr_start = lives_strdup_printf("<%s>", key);
  char *keystr_end = lives_strdup_printf("</%s>", key);
  size_t kslen = strlen(keystr_start);
  size_t kelen = strlen(keystr_end);
  size_t xs;

  boolean gotit = FALSE;
  char buff[maxlen];

  memset(buff, 0, 1);

  while (clist != NULL) {
    if (gotit) {
      if (!strncmp(keystr_end, (char *)clist->data, kelen)) {
        break;
      }
      if (strncmp((char *)clist->data, "|", 1)) lives_strappend(buff, maxlen, (char *)clist->data);
      else {
        if (clist->prev != NULL) clist->prev->next = clist->next;
      }
    } else if (!strncmp(keystr_start, (char *)clist->data, kslen)) {
      gotit = TRUE;
    }
    clist = clist->next;
  }
  lives_free(keystr_start);
  lives_free(keystr_end);

  if (!gotit) return NULL;

  xs = strlen(buff);
  if (xs > 0 && buff[xs - 1] == '\n') buff[xs - 1] = '\0'; // remove trailing newline

  return lives_strdup(buff);
}


char *clip_detail_to_string(lives_clip_details_t what, size_t *maxlenp) {
  char *key = NULL;

  switch (what) {
  case CLIP_DETAILS_HEADER_VERSION:
    key = lives_strdup("header_version");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_BPP:
    key = lives_strdup("bpp");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_FPS:
    key = lives_strdup("fps");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_PB_FPS:
    key = lives_strdup("pb_fps");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_WIDTH:
    key = lives_strdup("width");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_HEIGHT:
    key = lives_strdup("height");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    key = lives_strdup("unique_id");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_ARATE:
    key = lives_strdup("audio_rate");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_PB_ARATE:
    key = lives_strdup("pb_audio_rate");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_ACHANS:
    key = lives_strdup("audio_channels");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_ASIGNED:
    key = lives_strdup("audio_signed");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_AENDIAN:
    key = lives_strdup("audio_endian");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_ASAMPS:
    key = lives_strdup("audio_sample_size");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_FRAMES:
    key = lives_strdup("frames");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_TITLE:
    key = lives_strdup("title");
    break;
  case CLIP_DETAILS_AUTHOR:
    key = lives_strdup("author");
    break;
  case CLIP_DETAILS_COMMENT:
    key = lives_strdup("comment");
    break;
  case CLIP_DETAILS_KEYWORDS:
    key = lives_strdup("keywords");
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    key = lives_strdup("pb_frameno");
    if (maxlenp != NULL) *maxlenp = 256;
    break;
  case CLIP_DETAILS_CLIPNAME:
    key = lives_strdup("clipname");
    break;
  case CLIP_DETAILS_FILENAME:
    key = lives_strdup("filename");
    break;
  case CLIP_DETAILS_INTERLACE:
    key = lives_strdup("interlace");
    break;
  case CLIP_DETAILS_DECODER_NAME:
    key = lives_strdup("decoder");
    break;
  default:
    break;
  }
  return key;
}


boolean get_clip_value(int which, lives_clip_details_t what, void *retval, size_t maxlen) {
  time_t old_time = 0, new_time = 0;
  struct stat mystat;

  char *lives_header = NULL;
  char *old_header;
  char *val;
  char *key;
  char *tmp;

  int retval2 = LIVES_RESPONSE_NONE;

  if (mainw->cached_list == NULL) {
    lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle, LIVES_CLIP_HEADER, NULL);
    old_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle, LIVES_CLIP_HEADER_OLD, NULL);

    // TODO - remove this some time before 2038
    if (!stat(old_header, &mystat)) old_time = mystat.st_mtime;
    if (!stat(lives_header, &mystat)) new_time = mystat.st_mtime;
    lives_free(old_header);

    if (old_time > new_time) {
      lives_free(lives_header);
      return FALSE; // clip has been edited by an older version of LiVES
    }
  }
  //////////////////////////////////////////////////
  key = clip_detail_to_string(what, &maxlen);

  if (key == NULL) {
    tmp = lives_strdup_printf("Invalid detail %d requested from file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return FALSE;
  }

  mainw->read_failed = FALSE;

  if (mainw->cached_list != NULL) {
    val = get_val_from_cached_list(key, maxlen);
    lives_free(key);
    if (val == NULL) return FALSE;
  } else {
    val = (char *)lives_malloc(maxlen);
    retval2 = get_pref_from_file(lives_header, key, val, maxlen);
    lives_free(lives_header);
    lives_free(key);
  }

  if (retval2 == LIVES_RESPONSE_CANCEL) {
    return FALSE;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
  case CLIP_DETAILS_WIDTH:
  case CLIP_DETAILS_HEIGHT:
  case CLIP_DETAILS_ARATE:
  case CLIP_DETAILS_ACHANS:
  case CLIP_DETAILS_ASAMPS:
  case CLIP_DETAILS_FRAMES:
  case CLIP_DETAILS_HEADER_VERSION:
    *(int *)retval = atoi(val);
    break;
  case CLIP_DETAILS_ASIGNED:
    *(int *)retval = 0;
    if (mainw->files[which]->header_version == 0) *(int *)retval = atoi(val);
    if (*(int *)retval == 0 && (!strcasecmp(val, "false"))) *(int *)retval = 1; // unsigned
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = 1;
    break;
  case CLIP_DETAILS_PB_ARATE:
    *(int *)retval = atoi(val);
    if (retval == 0) *(int *)retval = mainw->files[which]->arps;
    break;
  case CLIP_DETAILS_INTERLACE:
    *(int *)retval = atoi(val);
    break;
  case CLIP_DETAILS_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = prefs->default_fps;
    break;
  case CLIP_DETAILS_PB_FPS:
    *(double *)retval = strtod(val, NULL);
    if (*(double *)retval == 0.) *(double *)retval = mainw->files[which]->fps;
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    if (capable->cpu_bits == 32) {
      *(int64_t *)retval = strtoll(val, NULL, 10);
    } else {
      *(int64_t *)retval = strtol(val, NULL, 10);
    }
    break;
  case CLIP_DETAILS_AENDIAN:
    *(int *)retval = atoi(val) * 2;
    break;
  case CLIP_DETAILS_TITLE:
  case CLIP_DETAILS_AUTHOR:
  case CLIP_DETAILS_COMMENT:
  case CLIP_DETAILS_CLIPNAME:
  case CLIP_DETAILS_KEYWORDS:
    lives_snprintf((char *)retval, maxlen, "%s", val);
    break;
  case CLIP_DETAILS_FILENAME:
  case CLIP_DETAILS_DECODER_NAME:
    lives_snprintf((char *)retval, maxlen, "%s", (tmp = F2U8(val)));
    lives_free(tmp);
    break;
  default:
    break;
  }
  lives_free(val);
  return TRUE;
}


void save_clip_value(int which, lives_clip_details_t what, void *val) {
  char *lives_header;
  char *com, *tmp;
  char *myval;
  char *key;

  boolean needs_sigs = FALSE;

  mainw->write_failed = mainw->com_failed = FALSE;

  if (which == 0 || which == mainw->scrap_file) return;

  lives_header = lives_build_filename(prefs->workdir, mainw->files[which]->handle, LIVES_CLIP_HEADER, NULL);
  key = clip_detail_to_string(what, NULL);

  if (key == NULL) {
    tmp = lives_strdup_printf("Invalid detail %d added for file %s", which, lives_header);
    LIVES_ERROR(tmp);
    lives_free(tmp);
    lives_free(lives_header);
    return;
  }

  switch (what) {
  case CLIP_DETAILS_BPP:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_FPS:
    if (!mainw->files[which]->ratio_fps) myval = lives_strdup_printf("%.3f", *(double *)val);
    else myval = lives_strdup_printf("%.8f", *(double *)val);
    // dont need to block this because it does nothing during non-playback, and we shouldnt be updating clip details during playback
    if (which == mainw->current_file &&
        mainw->is_ready) lives_spin_button_set_value(LIVES_SPIN_BUTTON(mainw->spinbutton_pb_fps), *(double *)val);
    break;
  case CLIP_DETAILS_PB_FPS:
    if (mainw->files[which]->ratio_fps && (mainw->files[which]->pb_fps == mainw->files[which]->fps))
      myval = lives_strdup_printf("%.8f", *(double *)val);
    else myval = lives_strdup_printf("%.3f", *(double *)val);
    break;
  case CLIP_DETAILS_WIDTH:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_HEIGHT:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_UNIQUE_ID:
    myval = lives_strdup_printf("%"PRId64, *(int64_t *)val);
    break;
  case CLIP_DETAILS_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_PB_ARATE:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_ACHANS:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_ASIGNED:
    if ((*(int *)val) == 1) myval = lives_strdup("true");
    else myval = lives_strdup("false");
    break;
  case CLIP_DETAILS_AENDIAN:
    myval = lives_strdup_printf("%d", (*(int *)val) / 2);
    break;
  case CLIP_DETAILS_ASAMPS:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_FRAMES:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_INTERLACE:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_TITLE:
    myval = lives_strdup((char *)val);
    break;
  case CLIP_DETAILS_AUTHOR:
    myval = lives_strdup((char *)val);
    break;
  case CLIP_DETAILS_COMMENT:
    myval = lives_strdup((const char *)val);
    break;
  case CLIP_DETAILS_KEYWORDS:
    myval = lives_strdup((const char *)val);
    break;
  case CLIP_DETAILS_PB_FRAMENO:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  case CLIP_DETAILS_CLIPNAME:
    myval = lives_strdup((char *)val);
    break;
  case CLIP_DETAILS_FILENAME:
    myval = U82F((const char *)val);
    break;
  case CLIP_DETAILS_DECODER_NAME:
    myval = U82F((const char *)val);
    break;
  case CLIP_DETAILS_HEADER_VERSION:
    myval = lives_strdup_printf("%d", *(int *)val);
    break;
  default:
    return;
  }

  if (mainw->clip_header != NULL) {
    char *keystr_start = lives_strdup_printf("<%s>\n", key);
    char *keystr_end = lives_strdup_printf("\n</%s>\n", key);
    lives_fputs(keystr_start, mainw->clip_header);
    lives_fputs(myval, mainw->clip_header);
    lives_fputs(keystr_end, mainw->clip_header);
    lives_free(keystr_start);
    lives_free(keystr_end);
  } else {
    if (!mainw->signals_deferred) {
      set_signal_handlers((SignalHandlerPointer)defer_sigint);
      needs_sigs = TRUE;
    }
    com = lives_strdup_printf("%s set_clip_value \"%s\" \"%s\" \"%s\"", prefs->backend_sync, lives_header, key, myval);
    lives_system(com, FALSE);
    if (mainw->signal_caught) catch_sigint(mainw->signal_caught);
    if (needs_sigs) set_signal_handlers((SignalHandlerPointer)catch_sigint);
    lives_free(com);
  }

  lives_free(lives_header);
  lives_free(myval);
  lives_free(key);

  return;
}


LiVESList *get_set_list(const char *dir, boolean utf8) {
  // get list of sets in top level dir
  // values will be in filename encoding

  LiVESList *setlist = NULL;
  DIR *tldir, *subdir;
  struct dirent *tdirent, *subdirent;
  char *subdirname;

  if (dir == NULL) return NULL;

  tldir = opendir(dir);

  if (tldir == NULL) return NULL;

  lives_set_cursor_style(LIVES_CURSOR_BUSY, NULL);
  lives_widget_process_updates(LIVES_MAIN_WINDOW_WIDGET, TRUE);

  while (1) {
    tdirent = readdir(tldir);

    if (tdirent == NULL) {
      closedir(tldir);
      lives_set_cursor_style(LIVES_CURSOR_NORMAL, NULL);
      return setlist;
    }

    if (!strncmp(tdirent->d_name, "..", strlen(tdirent->d_name))) continue;

    subdirname = lives_build_filename(dir, tdirent->d_name, NULL);

    subdir = opendir(subdirname);

    if (subdir == NULL) {
      lives_free(subdirname);
      continue;
    }

    while (1) {
      subdirent = readdir(subdir);

      if (subdirent == NULL) {
        break;
      }

      if (!strcmp(subdirent->d_name, "order")) {
        if (!utf8)
          setlist = lives_list_append(setlist, lives_strdup(tdirent->d_name));
        else
          setlist = lives_list_append(setlist, F2U8(tdirent->d_name));
        break;
      }
    }
    lives_free(subdirname);
    closedir(subdir);
  }
}


boolean check_for_ratio_fps(double fps) {
  boolean ratio_fps;
  char *test_fps_string1 = lives_strdup_printf("%.3f00000", fps);
  char *test_fps_string2 = lives_strdup_printf("%.8f", fps);

  if (strcmp(test_fps_string1, test_fps_string2)) {
    // got a ratio
    ratio_fps = TRUE;
  } else {
    ratio_fps = FALSE;
  }
  lives_free(test_fps_string1);
  lives_free(test_fps_string2);

  return ratio_fps;
}


double get_ratio_fps(const char *string) {
  // return a ratio (8dp) fps from a string with format num:denom
  double fps;
  char *fps_string;
  char **array = lives_strsplit(string, ":", 2);
  int num = atoi(array[0]);
  int denom = atoi(array[1]);
  lives_strfreev(array);
  fps = (double)num / (double)denom;
  fps_string = lives_strdup_printf("%.8f", fps);
  fps = lives_strtod(fps_string, NULL);
  lives_free(fps_string);
  return fps;
}


char *remove_trailing_zeroes(double val) {
  int i;
  double xval = val;

  if (val == (int)val) return lives_strdup_printf("%d", (int)val);
  for (i = 0; i <= 16; i++) {
    xval *= 10.;
    if (xval == (int)xval) return lives_strdup_printf("%.*f", i, val);
  }
  return lives_strdup_printf("%.*f", i, val);
}


uint32_t get_signed_endian(boolean is_signed, boolean little_endian) {
  // asigned TRUE == signed, FALSE == unsigned

  if (is_signed) {
    if (little_endian) {
      return 0;
    } else {
      return AFORM_BIG_ENDIAN;
    }
  } else {
    if (!is_signed) {
      if (little_endian) {
        return AFORM_UNSIGNED;
      } else {
        return AFORM_UNSIGNED | AFORM_BIG_ENDIAN;
      }
    }
  }
  return AFORM_UNKNOWN;
}


int get_token_count(const char *string, int delim) {
  int pieces = 1;
  if (string == NULL) return 0;
  if (delim <= 0 || delim > 255) return 1;

  while ((string = strchr(string, delim)) != NULL) {
    pieces++;
    string++;
  }
  return pieces;
}


char *get_nth_token(const char *string, const char *delim, int pnumber) {
  char **array;
  char *ret = NULL;
  register int i;
  if (pnumber < 0 || pnumber >= get_token_count(string, (int)delim[0])) return NULL;
  array = lives_strsplit(string, delim, pnumber + 1);
  for (i = 0; i < pnumber; i++) {
    if (i == pnumber) ret = array[i];
    else lives_free(array[i]);
  }
  lives_free(array);
  return ret;
}


int lives_utf8_strcasecmp(const char *s1, const char *s2) {
  // ignore case
  char *s1u = lives_utf8_casefold(s1, -1);
  char *s2u = lives_utf8_casefold(s2, -1);
  int ret = lives_utf8_strcmp(s1u, s2u);
  lives_free(s1u);
  lives_free(s2u);
  return ret;
}


LIVES_GLOBAL_INLINE int lives_utf8_strcmp(const char *s1, const char *s2) {
  return lives_utf8_collate(s1, s2);
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_sort_alpha(LiVESList *list, boolean fwd) {
  return lives_list_sort_with_data(list, lives_utf8_strcmpfunc, LIVES_INT_TO_POINTER(fwd));
}


char *subst(const char *string, const char *from, const char *to) {
  // return a string with all occurrences of from replaced with to
  // return value should be freed after use
  char *ret = lives_strdup(string), *first;
  char *search = ret;

  while ((search = lives_strstr_len(search, -1, from)) != NULL) {
    first = lives_strndup(ret, search - ret);
    search = lives_strdup(search + strlen(from));
    lives_free(ret);
    ret = lives_strconcat(first, to, search, NULL);
    lives_free(search);
    search = ret + strlen(first) + strlen(to);
    lives_free(first);
  }
  return ret;
}


char *insert_newlines(const char *text, int maxwidth) {
  // crude formating of strings, ensure a newline after every run of maxwidth chars
  // does not take into account for example utf8 multi byte chars

  wchar_t utfsym;

  char newline[] = "\n";
  char *retstr;

  size_t runlen = 0;
  size_t req_size = 1; // for the terminating \0
  size_t tlen;
  size_t nlen = strlen(newline);

  int xtoffs;

  boolean needsnl = FALSE;

  register int i;

  if (text == NULL) return NULL;

  if (maxwidth < 1) return lives_strdup("Bad maxwidth, dummy");

  tlen = strlen(text);

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  //pass 1, get the required size
  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (xtoffs == -1) {
      LIVES_WARN("mbtowc returned -1");
      return lives_strdup(text);
    }

    if (!strncmp(text + i, "\n", nlen)) runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) req_size += nlen; ///< we will insert a nl here
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (strncmp(text + i + 1, "\n", nlen))) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    req_size += xtoffs;
  }

  xtoffs = mbtowc(NULL, NULL, 0); // reset read state

  retstr = (char *)lives_malloc(req_size);
  req_size = 0; // reuse as a ptr to offset in retstr
  runlen = 0;
  needsnl = FALSE;

  //pass 2, copy and insert newlines

  for (i = 0; i < tlen; i += xtoffs) {
    xtoffs = mbtowc(&utfsym, &text[i], 4); // get next utf8 wchar
    if (!strncmp(text + i, "\n", nlen)) runlen = 0; // is a newline (in any encoding)
    else {
      runlen++;
      if (needsnl) {
        memcpy(retstr + req_size, newline, nlen);
        req_size += nlen;
      }
    }

    if (runlen == maxwidth) {
      if (i < tlen - 1 && (strncmp(text + i + 1, "\n", nlen))) {
        // needs a newline
        needsnl = TRUE;
        runlen = 0;
      }
    } else needsnl = FALSE;
    memcpy(retstr + req_size, &utfsym, xtoffs);
    req_size += xtoffs;

  }

  memset(retstr + req_size, 0, 1);

  return retstr;
}


int hextodec(char *string) {
  int i;
  int tot = 0;
  char test[2];

  memset(test + 1, 0, 1);

  for (i = 0; i < strlen(string); i++) {
    tot *= 16;
    lives_memcpy(test, (void *)&string[i], 1);
    tot += get_hex_digit(test);
  }
  return tot;
}


int get_hex_digit(const char *c) {
  if (!strcmp(c, "a") || !strcmp(c, "A")) return 10;
  if (!strcmp(c, "b") || !strcmp(c, "B")) return 11;
  if (!strcmp(c, "c") || !strcmp(c, "C")) return 12;
  if (!strcmp(c, "d") || !strcmp(c, "D")) return 13;
  if (!strcmp(c, "e") || !strcmp(c, "E")) return 14;
  if (!strcmp(c, "f") || !strcmp(c, "F")) return 15;
  return (atoi(c));
}


static uint32_t fastrand_val;

LIVES_GLOBAL_INLINE uint32_t fastrand(void) {
#define rand_a 1073741789L
#define rand_c 32749L

  return (fastrand_val = rand_a * fastrand_val + rand_c);
}


void fastsrand(uint32_t seed) {
  fastrand_val = seed;
}


boolean is_writeable_dir(const char *dir) {
  // return FALSE if we cannot create/write to dir

  // dir should be in locale encoding

  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  struct statvfs sbuf;
  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    lives_mkdir_with_parents(dir, capable->umask);
    if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
      return FALSE;
    }
  }

  // use statvfs to get fs details
  if (statvfs(dir, &sbuf) == -1) return FALSE;
  if (sbuf.f_flag & ST_RDONLY) return FALSE;
  return TRUE;
}


boolean lives_make_writeable_dir(const char *newdir) {
  int ret = lives_mkdir_with_parents(newdir, capable->umask);
  int myerrno = errno;
  if (!check_dir_access(newdir)) {
    // abort if we cannot create the new subdir
    if (myerrno == EINVAL) {
      LIVES_ERROR("Could not write to directory");
    } else LIVES_ERROR("Could not create directory");
    LIVES_ERROR(newdir);
    mainw->com_failed = FALSE;
    return FALSE;
  } else {
    if (ret != -1) {
      LIVES_DEBUG("Created directory");
      LIVES_DEBUG(newdir);
    }
  }
  return TRUE;
}


uint64_t get_fs_free(const char *dir) {
  // get free space in bytes for volume containing directory dir
  // return 0 if we cannot create/write to dir

  // caller should test with is_writeable_dir() first before calling this
  // since 0 is a valid return value

  // dir should be in locale encoding

  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  struct statvfs sbuf;

  uint64_t bytes = 0;
  boolean must_delete = FALSE;

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) must_delete = TRUE;
  if (!is_writeable_dir(dir)) goto getfserr;

  // use statvfs to get fs details
  if (statvfs(dir, &sbuf) == -1) goto getfserr;
  if (sbuf.f_flag & ST_RDONLY) goto getfserr;

  // result is block size * blocks available
  bytes = sbuf.f_bsize * sbuf.f_bavail;

getfserr:
  if (must_delete) lives_rmdir(dir, FALSE);

  return bytes;
}


LIVES_GLOBAL_INLINE LiVESInterpType get_interp_value(short quality) {
  if (quality == PB_QUALITY_HIGH) return LIVES_INTERP_BEST;
  else if (quality == PB_QUALITY_MED) return LIVES_INTERP_NORMAL;
  return LIVES_INTERP_FAST;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_move_to_first(LiVESList *list, LiVESList *item) {
  // move item to first in list
  LiVESList *xlist = lives_list_remove_link(list, item); // item becomes standalone list
  return lives_list_concat(item, xlist); // concat rest of list after item
}


LiVESList *lives_list_delete_string(LiVESList *list, const char *string) {
  // remove string from list, using strcmp

  LiVESList *xlist = list;
  while (xlist != NULL) {
    if (!lives_utf8_strcasecmp((char *)xlist->data, string)) {
      lives_free((livespointer)xlist->data);
      list = lives_list_delete_link(list, xlist);
      return list;
    }
    xlist = xlist->next;
  }
  return list;
}


LIVES_GLOBAL_INLINE LiVESList *lives_list_copy_strings(LiVESList *list) {
  // copy a list, copying the strings too

  LiVESList *xlist = NULL, *olist = list;

  while (olist != NULL) {
    xlist = lives_list_append(xlist, lives_strdup((char *)olist->data));
    olist = olist->next;
  }

  return xlist;
}


boolean string_lists_differ(LiVESList *alist, LiVESList *blist) {
  // compare 2 lists of strings and see if they are different (ignoring ordering)
  // for long lists this would be quicker if we sorted the lists first; however this function
  // is designed to deal with short lists only

  LiVESList *plist;

  if (lives_list_length(alist) != lives_list_length(blist)) return TRUE; // check the simple case first

  // run through alist and see if we have a mismatch

  plist = alist;
  while (plist != NULL) {
    LiVESList *qlist = blist;
    boolean matched = FALSE;
    while (qlist != NULL) {
      if (!(lives_utf8_strcasecmp((char *)plist->data, (char *)qlist->data))) {
        matched = TRUE;
        break;
      }
      qlist = qlist->next;
    }
    if (!matched) return TRUE;
    plist = plist->next;
  }

  // since both lists were of the same length, there is no need to check blist

  return FALSE;
}


lives_cancel_t check_for_bad_ffmpeg(void) {
  int i;

  int fcount, ofcount;

  char *fname_next;

  boolean maybeok = FALSE;

  ofcount = cfile->frames;

  get_frame_count(mainw->current_file);

  fcount = cfile->frames;

  for (i = 1; i <= fcount; i++) {
    fname_next = make_image_file_name(cfile, i, get_image_ext_for_type(cfile->img_type));
    if (sget_file_size(fname_next) > 0) {
      lives_free(fname_next);
      maybeok = TRUE;
      break;
    }
    lives_free(fname_next);
  }

  cfile->frames = ofcount;

  if (!maybeok) {
    do_error_dialog(
      _("Your version of mplayer/ffmpeg may be broken !\nSee http://bugzilla.mplayerhq.hu/show_bug.cgi?id=2071\n\n"
        "You can work around this temporarily by switching to jpeg output in Preferences/Decoding.\n\n"
        "Try running Help/Troubleshoot for more information."));
    return CANCEL_ERROR;
  }
  return CANCEL_NONE;
}
