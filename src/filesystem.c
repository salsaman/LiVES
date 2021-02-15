// filesystem.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <sys/statvfs.h>
#include "main.h"

#ifdef HAVE_LIBEXPLAIN
#include <libexplain/system.h>
#include <libexplain/read.h>
#endif


/**
  @brief: return filename from an open fd, freeing val first

  in case of error function returns val

  if fd is a buffered file then the function just returns the known name,
  else the name is procured from /proc

  call like: foo = filename_from_fd(foo,fd); lives_free(foo);
  input param foo can be NULL or some (non-const) string buffer
  if non-NULL the old value will be freed, so e.g

  char *badfile = NULL;
  while (condition) {
  ....
      if (failed) badfile = filename_from_fd(badfile, fd);
    }
    if (badfile != NULL) lives_free(badfile);

    or:

  char *badfile =  NULL;
  badfile = filename_from_fd(badfile, fd);
  if (badfile == NULL) // error getting filename

**/
char *filename_from_fd(char *val, int fd) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (fbuff) {
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

    lives_memset(rfdpath + slen, 0, 1);

    if (stat(rfdpath, &stb1)) return val;
    if (stb0.st_dev != stb1.st_dev) return val;
    if (stb0.st_ino != stb1.st_ino) return val;
    if (val) lives_free(val);
    return lives_strdup(rfdpath);
  }
}


// system calls

LIVES_GLOBAL_INLINE int lives_open3(const char *pathname, int flags, mode_t mode) {
  return open(pathname, flags, mode);
}


LIVES_GLOBAL_INLINE int lives_open2(const char *pathname, int flags) {
  return open(pathname, flags);
}


LIVES_GLOBAL_INLINE ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz) {
  return readlink(path, buf, bufsiz);
}


LIVES_GLOBAL_INLINE boolean lives_fsync(int fd) {
  // ret TRUE on success
  return !fsync(fd);
}


LIVES_GLOBAL_INLINE void lives_sync(int times) {
  for (int i = 0; i < times; i++) sync();
}


boolean is_writeable_dir(const char *dir) {
  // return FALSE if we cannot create / write to dir
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


ssize_t lives_write(int fd, const void *buf, ssize_t count, boolean allow_fail) {
  ssize_t retval;
  if (count <= 0) return 0;

  retval = write(fd, buf, count);

  if (retval < count) {
    char *msg = NULL;
    /// TODO ****: this needs to be threadsafe
    THREADVAR(write_failed) = fd + 1;
    THREADVAR(write_failed_file) = filename_from_fd(THREADVAR(write_failed_file), fd);
    if (retval >= 0)
      msg = lives_strdup_printf("Write failed %"PRId64" of %"PRId64" in: %s", retval,
                                count, THREADVAR(write_failed_file));
    else
      msg = lives_strdup_printf("Write failed with error %"PRId64" in: %s", retval,
                                THREADVAR(write_failed_file));

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
                                  THREADVAR(write_failed_file));
      LIVES_DEBUG(msg);
      lives_free(ffile);
    }
#endif
    if (msg) lives_free(msg);
  }
  return retval;
}


ssize_t lives_write_le(int fd, const void *buf, ssize_t count, boolean allow_fail) {
  if (count <= 0) return 0;
  if (capable->byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
    reverse_bytes((char *)buf, count, count);
  }
  return lives_write(fd, buf, count, allow_fail);
}


int lives_fputs(const char *s, FILE *stream) {
  int retval = fputs(s, stream);
  if (retval == EOF) {
    THREADVAR(write_failed) = fileno(stream) + 1;
  }
  return retval;
}


char *lives_fgets(char *s, int size, FILE *stream) {
  char *retval;
  if (!size) return NULL;
  retval = fgets(s, size, stream);
  if (!retval && ferror(stream)) {
    THREADVAR(read_failed) = fileno(stream) + 1;
  }
  return retval;
}


size_t lives_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
  size_t bytes_read = fread(ptr, size, nmemb, stream);
  if (ferror(stream)) {
    THREADVAR(read_failed) = fileno(stream) + 1;
  }
  return bytes_read;
}


size_t lives_fread_string(char *buff, size_t stlen, const char *fname) {
  size_t bread = 0;
  FILE *infofile;
  if (!stlen) return 0;
  infofile = fopen(fname, "r");
  if (!infofile) return 0;
  bread = lives_fread(buff, 1, stlen - 1, infofile);
  fclose(infofile);
  lives_memset(buff + bread, 0, 1);
  return bread;
}


lives_file_buffer_t *find_in_file_buffers(int fd) {
  lives_file_buffer_t *fbuff = NULL;
  LiVESList *fblist;

  pthread_mutex_lock(&mainw->fbuffer_mutex);

  for (fblist = mainw->file_buffers; fblist; fblist = fblist->next) {
    fbuff = (lives_file_buffer_t *)fblist->data;
    if (fbuff->fd == fd) break;
    fbuff = NULL;
  }

  pthread_mutex_unlock(&mainw->fbuffer_mutex);

  return fbuff;
}


lives_file_buffer_t *find_in_file_buffers_by_pathname(const char *pathname) {
  lives_file_buffer_t *fbuff = NULL;
  LiVESList *fblist;

  pthread_mutex_lock(&mainw->fbuffer_mutex);

  for (fblist = mainw->file_buffers; fblist; fblist = fblist->next) {
    fbuff = (lives_file_buffer_t *)fblist->data;
    if (!lives_strcmp(fbuff->pathname, pathname)) break;
    fbuff = NULL;
  }

  pthread_mutex_unlock(&mainw->fbuffer_mutex);

  return fbuff;
}


static void do_file_read_error(int fd, ssize_t errval, void *buff, ssize_t count) {
  char *msg = NULL;
  THREADVAR(read_failed) = fd + 1;
  THREADVAR(read_failed_file) = filename_from_fd(THREADVAR(read_failed_file), fd);

  if (errval >= 0)
    msg = lives_strdup_printf("Read failed %"PRId64" of %"PRId64" in: %s", (int64_t)errval,
                              count, THREADVAR(read_failed_file));
  else {
    msg = lives_strdup_printf("Read failed with error %"PRId64" in: %s (%s)", (int64_t)errval,
                              THREADVAR(read_failed_file),
#ifdef HAVE_LIBEXPLAIN
                              buff ? explain_read(fd, buff, count) : ""
#else
                              ""
#endif
                             );
  }
  LIVES_ERROR(msg);
  lives_free(msg);
}


ssize_t lives_read(int fd, void *buf, ssize_t count, boolean allow_less) {
  ssize_t retval = read(fd, buf, count);
  if (count <= 0) return 0;

  if (retval < count) {
    if (!allow_less || retval < 0) {
      do_file_read_error(fd, retval, buf, count);
      close(fd);
    }
#ifndef LIVES_NO_DEBUG
    else {
      char *msg = NULL;
      char *ffile = filename_from_fd(NULL, fd);
      msg = lives_strdup_printf("Read got %"PRIu64" of %"PRIu64" in: %s (not an error)",
                                (uint64_t)retval,
                                (uint64_t)count, ffile);
      LIVES_DEBUG(msg);
      lives_free(ffile);
      lives_free(msg);
    }
#endif
  }
  return retval;
}


ssize_t lives_read_le(int fd, void *buf, ssize_t count, boolean allow_less) {
  ssize_t retval;
  if (count <= 0) return 0;
  retval = lives_read(fd, buf, count, allow_less);
  if (retval < count) return retval;
  if (capable->byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    reverse_bytes((char *)buf, count, count);
  }
  return retval;
}

//// buffered io ////

// explanation of values

// read:
// fbuff->buffer holds (fbuff->ptr - fbuff->buffer + fbuff->bytes) bytes
// fbuff->offset is the next real read position

// read x bytes : fbuff->ptr increases by x, fbuff->bytes decreases by x
// if fbuff->bytes is < x, then we concat fbuff->bytes, refill buffer from file, concat remaining bytes
// on read: fbuff->ptr = fbuff->buffer. fbuff->offset += bytes read, fbuff->bytes = bytes read
// if fbuff->reversed is set then we seek to a position offset - 3 / 4 buffsize, fbuff->ptr = fbuff->buffer + 3 / 4 buffsz, bytes = 1 / 4 buffsz


// on seek (read only):
// forward: seek by +z: if z < fbuff->bytes : fbuff->ptr += z, fbuff->bytes -= z
// if z > fbuff->bytes: subtract fbuff->bytes from z. Increase fbuff->offset by remainder. Fill buffer.

// backward: if fbuff->ptr - z >= fbuff->buffer : fbuff->ptr -= z, fbuff->bytes += z
// fbuff->ptr - z < fbuff->buffer:  z -= (fbuff->ptr - fbuff->buffer) : fbuff->offset -= (fbuff->bytes + z) : Fill buffer

// seek absolute: current viritual posn is fbuff->offset - fbuff->bytes : subtract this from absolute posn

// return value is: fbuff->offset - fbuff->bytes ?

// when writing we simply fill up the buffer until full, then flush the buffer to file io
// buffer is finally flushed when we close the file (or we call file_buffer_flush)

// in this case fbuff->bytes holds the number of bytes written to fbuff->buffer, fbuff->offset contains the offset in the underlying fil

// in append mode, seek is first the end of the file. In creat mode any existing file is truncated and overwritten.

// in write mode, if we have fallocate, then we preallocate the buffer size on disk.
// When the file is closed we truncate any remaining bytes. Thus CAUTION because the file size as read directly will include the
// padding bytes, and thus appending directly to the file will write after the padding.bytes, and either be overwritten or truncated.
// in this case the correct size can be obtained from

static ssize_t file_buffer_flush(lives_file_buffer_t *fbuff) {
  // returns number of bytes written to file io, or error code
  ssize_t res = 0;

  if (fbuff->buffer) res = lives_write(fbuff->fd, fbuff->buffer, fbuff->bytes, fbuff->allow_fail);
  //g_print("writing %ld bytes to %d\n", fbuff->bytes, fbuff->fd);

  if (!fbuff->allow_fail && res < fbuff->bytes) {
    lives_close_buffered(-fbuff->fd); // use -fd as lives_write will have closed
    return res;
  }

  if (res > 0) {
    fbuff->offset += res;
    fbuff->bytes = 0;
    fbuff->ptr = fbuff->buffer;
  }
  //g_print("writer offs at %ld bytes to %d\n", fbuff->offset, fbuff->fd);

  return res;
}


void lives_invalidate_all_file_buffers(void) {
  lives_file_buffer_t *fbuff = NULL;
  LiVESList *fblist;

  pthread_mutex_lock(&mainw->fbuffer_mutex);

  for (fblist = mainw->file_buffers; fblist; fblist = fblist->next) {
    fbuff = (lives_file_buffer_t *)fblist->data;
    // if a writer, flush
    if (!fbuff->read && mainw->memok) {
      file_buffer_flush(fbuff);
      fbuff->buffer = NULL;
    } else {
      fbuff->invalid = TRUE;
    }
  }

  pthread_mutex_unlock(&mainw->fbuffer_mutex);
}


static int lives_open_real_buffered(const char *pathname, int flags, int mode, boolean isread) {
  lives_file_buffer_t *fbuff, *xbuff;
  boolean is_append = FALSE;
  int fd;

  if (flags & O_APPEND) {
    is_append = TRUE;
    flags &= ~O_APPEND;
  }

  fd = lives_open3(pathname, flags, mode);
  if (fd >= 0) {
    fbuff = (lives_file_buffer_t *)lives_calloc(sizeof(lives_file_buffer_t) >> 2, 4);
    fbuff->fd = fd;
    fbuff->read = isread;
    fbuff->pathname = lives_strdup(pathname);
    fbuff->bufsztype = isread ? BUFF_SIZE_READ_SMALL : BUFF_SIZE_WRITE_SMALL;

    if ((xbuff = find_in_file_buffers(fd)) != NULL) {
      char *msg = lives_strdup_printf("Duplicate fd (%d) in file buffers !\n%s was not removed, and\n%s will be added.", fd,
                                      xbuff->pathname,
                                      fbuff->pathname);
      break_me("dupe fd in fbuffs");
      LIVES_ERROR(msg);
      lives_free(msg);
      lives_close_buffered(fd);
    } else {
      if (!isread && !(flags & O_TRUNC)) {
        if (is_append) fbuff->offset = fbuff->orig_size = lseek(fd, 0, SEEK_END);
        else fbuff->orig_size = (size_t)get_file_size(fd);
        /// TODO - handle fsize < 0
      }
    }
    pthread_mutex_lock(&mainw->fbuffer_mutex);
    mainw->file_buffers = lives_list_prepend(mainw->file_buffers, (livespointer)fbuff);
    pthread_mutex_unlock(&mainw->fbuffer_mutex);
  }

  return fd;
}

static size_t bigbytes = BUFFER_FILL_BYTES_LARGE;
static size_t medbytes = BUFFER_FILL_BYTES_MED;
static size_t smedbytes = BUFFER_FILL_BYTES_SMALLMED;
static size_t smbytes = BUFFER_FILL_BYTES_SMALL;
#define AUTOTUNE
#ifdef AUTOTUNE
static weed_plant_t *tunerl = NULL;
static boolean tunedl = FALSE;
static weed_plant_t *tunerm = NULL;
static boolean tunedm = FALSE;
static weed_plant_t *tunersm = NULL;
static boolean tunedsm = FALSE;
static weed_plant_t *tuners = NULL;
static boolean tuneds = FALSE;
#endif


LIVES_GLOBAL_INLINE int lives_open_buffered_rdonly(const char *pathname) {
  return lives_open_real_buffered(pathname, O_RDONLY, 0, TRUE);
}


boolean _lives_buffered_rdonly_slurp(int fd, off_t skip) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  off_t fsize = get_file_size(fd) - skip, bufsize = smbytes, res;
  if (fsize > 0) {
    fbuff->orig_size = fsize + skip;
    lseek(fd, skip, SEEK_SET);
    fbuff->buffer = fbuff->ptr = lives_calloc(1, fsize);
    //g_printerr("slurp for %d, %s with size %ld\n", fd, fbuff->pathname, fsize);
    while (fsize > 0) {
      if (!fbuff->slurping) break; // file was closed
      if (bufsize > fsize) bufsize = fsize;
      res = lives_read(fbuff->fd, fbuff->buffer + fbuff->offset, bufsize, TRUE);
      //g_printerr("slurp for %d, %s with size %ld, read %lu bytes, remain\n", fd, fbuff->pathname, res, fsize);
      if (res < 0) {
        fbuff->eof = TRUE;
        return FALSE;
      }
      if (res > fsize) res = fsize;
      fbuff->offset += res;
      fsize -= res;
      if (fsize >= bigbytes && bufsize >= medbytes) bufsize = bigbytes;
      else if (fsize >= medbytes && bufsize >= smedbytes) bufsize = medbytes;
      else if (fsize >= smedbytes) bufsize = smedbytes;
      //g_printerr("slurp %d oof %ld %ld remain %lu  \n", fd, fbuff->offset, fsize, ofsize);
      if (mainw->disk_pressure > 0.) mainw->disk_pressure = check_disk_pressure(0.);
    }
  }
  fbuff->eof = TRUE;
  return TRUE;
}


void lives_buffered_rdonly_slurp(int fd, off_t skip) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (!fbuff || fbuff->slurping) return;
  fbuff->slurping = TRUE;
  fbuff->bytes = fbuff->offset = 0;
  lives_proc_thread_create(LIVES_THRDATTR_NONE, (lives_funcptr_t)_lives_buffered_rdonly_slurp, 0, "iI", fd, skip);
  lives_nanosleep_until_nonzero(fbuff->orig_size | fbuff->eof);
}


LIVES_GLOBAL_INLINE boolean lives_buffered_rdonly_set_reversed(int fd, boolean val) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (!fbuff) {
    // normal non-buffered file
    LIVES_DEBUG("lives_buffered_readonly_set_reversed: no file buffer found");
    return FALSE;
  }
  fbuff->reversed = val;
  return TRUE;
}


#ifndef O_DSYNC
#define O_DSYNC O_SYNC
#define NO_O_DSYNC
#endif

LIVES_GLOBAL_INLINE int lives_create_buffered(const char *pathname, int mode) {
  return lives_open_real_buffered(pathname, O_CREAT | O_WRONLY | O_TRUNC | O_DSYNC, mode, FALSE);
}

LIVES_GLOBAL_INLINE int lives_create_buffered_nosync(const char *pathname, int mode) {
  return lives_open_real_buffered(pathname, O_CREAT | O_WRONLY | O_TRUNC, mode, FALSE);
}

int lives_open_buffered_writer(const char *pathname, int mode, boolean append) {
  return lives_open_real_buffered(pathname, O_CREAT | O_WRONLY | O_DSYNC | (append ? O_APPEND : 0), mode, FALSE);
}

#ifdef NO_O_DSYNC
#undef O_DSYNC
#undef NO_O_DSYNC
#endif


ssize_t lives_close_buffered(int fd) {
  lives_file_buffer_t *fbuff;
  ssize_t ret = 0;
  boolean should_close = TRUE;

  /* if (IS_VALID_CLIP(mainw->scrap_file) && mainw->files[mainw->scrap_file]->ext_src && */
  /*     fd == LIVES_POINTER_TO_INT(mainw->files[mainw->scrap_file]->ext_src)); */

  if (fd < 0) {
    should_close = FALSE;
    fd = -fd;
  }

  fbuff = find_in_file_buffers(fd);

  if (!fbuff) {
    // normal non-buffered file
    LIVES_DEBUG("lives_close_buffered: no file buffer found");
    if (should_close) ret = close(fd);
    return ret;
  }

  if (!fbuff->read && should_close) {
    boolean allow_fail = fbuff->allow_fail;
    ssize_t bytes = fbuff->bytes;

    if (bytes > 0) {
      ret = file_buffer_flush(fbuff);
      // this is correct, as flush will have called close again with should_close=FALSE;
      if (!allow_fail && ret < bytes) return ret;
    }
#ifdef HAVE_POSIX_FALLOCATE
    IGN_RET(ftruncate(fbuff->fd, MAX(fbuff->offset, fbuff->orig_size)));
    /* //g_print("truncated  at %ld bytes in %d\n", MAX(fbuff->offset, fbuff->orig_size), fbuff->fd); */
#endif
  }

  if (fbuff->slurping) {
    fbuff->slurping = FALSE;
    lives_nanosleep_until_nonzero(fbuff->eof);
  }

  lives_free(fbuff->pathname);

  pthread_mutex_lock(&mainw->fbuffer_mutex);
  if (should_close && fbuff->fd >= 0) ret = close(fbuff->fd);
  mainw->file_buffers = lives_list_remove(mainw->file_buffers, (livesconstpointer)fbuff);
  pthread_mutex_unlock(&mainw->fbuffer_mutex);

  if (fbuff->buffer && !fbuff->invalid) {
    lives_free(fbuff->buffer);
  }

  lives_free(fbuff);
  return ret;
}


size_t get_read_buff_size(int sztype) {
  switch (sztype) {
  case BUFF_SIZE_READ_SMALLMED: return smedbytes;
  case BUFF_SIZE_READ_MED: return medbytes;
  case BUFF_SIZE_READ_LARGE: return bigbytes;
  default: break;
  }
  return smbytes;
}


static ssize_t file_buffer_fill(lives_file_buffer_t *fbuff, ssize_t min) {
  ssize_t res;
  ssize_t delta = 0;
  size_t bufsize;

  if (min < 0) min = 0;

  if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) {
    if (fbuff->buffer) bufsize = fbuff->ptr - fbuff->buffer + fbuff->bytes;
    else {
      bufsize = fbuff->bytes;
      fbuff->bytes = 0;
    }
  } else bufsize = get_read_buff_size(fbuff->bufsztype);

  if (fbuff->reversed) delta = (bufsize >> 2) * 3;
  if (delta > fbuff->offset) delta = fbuff->offset;
  if (bufsize - delta < min) bufsize = min + delta;
  if (fbuff->buffer && bufsize > fbuff->ptr - fbuff->buffer + fbuff->bytes) {
    lives_freep((void **)&fbuff->buffer);
  }
  if (!fbuff->buffer || !fbuff->ptr) {
    fbuff->buffer = (uint8_t *)lives_calloc_safety(bufsize >> 1, 2);
  }
  fbuff->offset -= delta;
  fbuff->offset = lseek(fbuff->fd, fbuff->offset, SEEK_SET);

  res = lives_read(fbuff->fd, fbuff->buffer, bufsize, TRUE);
  if (res < 0) {
    lives_close_buffered(-fbuff->fd); // use -fd as lives_read will have closed
    return res;
  }

  fbuff->bytes = res - delta;
  fbuff->ptr = fbuff->buffer + delta;
  fbuff->offset += res;
  if (res < bufsize) fbuff->eof = TRUE;
  else fbuff->eof = FALSE;

#if defined HAVE_POSIX_FADVISE || (defined _GNU_SOURCE && defined __linux__)
  if (fbuff->reversed) {
#if defined HAVE_POSIX_FADVISE
    posix_fadvise(fbuff->fd, 0, fbuff->offset - (bufsize >> 2) * 3, POSIX_FADV_RANDOM);
    posix_fadvise(fbuff->fd, fbuff->offset - (bufsize >> 2) * 3, bufsize, POSIX_FADV_WILLNEED);
#endif
#ifdef __linux__
    readahead(fbuff->fd, fbuff->offset - (bufsize >> 2) * 3, bufsize);
#endif
  } else {
#if defined HAVE_POSIX_FADVISE
    posix_fadvise(fbuff->fd, fbuff->offset, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fbuff->fd, fbuff->offset, bufsize, POSIX_FADV_WILLNEED);
#endif
#ifdef __linux__
    readahead(fbuff->fd, fbuff->offset, bufsize);
#endif
  }
#endif

  return res - delta;
}


static off_t _lives_lseek_buffered_rdonly_relative(lives_file_buffer_t *fbuff, off_t offset) {
  off_t newoffs;
  if (offset == 0) return fbuff->offset - fbuff->bytes;
  fbuff->nseqreads = 0;

  if (offset > 0) {
    // seek forwards
    if (offset < fbuff->bytes) {
      fbuff->ptr += offset;
      fbuff->bytes -= offset;
      newoffs =  fbuff->offset - fbuff->bytes;
    } else {
      offset -= fbuff->bytes;
      fbuff->offset += offset;
      fbuff->bytes = 0;
      newoffs = fbuff->offset;
    }
  } else {
    // seek backwards
    offset = -offset;
    if (offset <= fbuff->ptr - fbuff->buffer) {
      fbuff->ptr -= offset;
      fbuff->bytes += offset;
      newoffs = fbuff->offset - fbuff->bytes;
    } else {
      offset -= fbuff->ptr - fbuff->buffer;

      fbuff->offset = fbuff->offset - (fbuff->ptr - fbuff->buffer + fbuff->bytes) - offset;
      if (fbuff->offset < 0) fbuff->offset = 0;

      fbuff->bytes = 0;

      fbuff->eof = FALSE;
      newoffs = fbuff->offset;
    }
  }

#ifdef HAVE_POSIX_FADVISE
  if (fbuff->reversed)
    posix_fadvise(fbuff->fd, 0, fbuff->offset - fbuff->bytes, POSIX_FADV_RANDOM);
  else
    posix_fadvise(fbuff->fd, fbuff->offset, 0, POSIX_FADV_SEQUENTIAL);
#endif

  lseek(fbuff->fd, fbuff->offset, SEEK_SET);

  return newoffs;
}


off_t lives_lseek_buffered_rdonly(int fd, off_t offset) {
  // seek relative
  lives_file_buffer_t *fbuff;
  if (!(fbuff = find_in_file_buffers(fd))) {
    LIVES_DEBUG("lives_lseek_buffered_rdonly: no file buffer found");
    return lseek(fd, offset, SEEK_CUR);
  }

  return _lives_lseek_buffered_rdonly_relative(fbuff, offset);
}


off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t offset) {
  lives_file_buffer_t *fbuff;

  if (!(fbuff = find_in_file_buffers(fd))) {
    LIVES_DEBUG("lives_lseek_buffered_rdonly_absolute: no file buffer found");
    return lseek(fd, offset, SEEK_SET);
  }

  if (!fbuff->ptr || !fbuff->buffer) {
    fbuff->offset = offset;
    return fbuff->offset;
  }
  offset -= fbuff->offset - fbuff->bytes;
  return _lives_lseek_buffered_rdonly_relative(fbuff, offset);
}


ssize_t lives_read_buffered(int fd, void *buf, ssize_t count, boolean allow_less) {
  lives_file_buffer_t *fbuff;
  ssize_t retval = 0, res = 0;
  size_t ocount = count;
  uint8_t *ptr = (uint8_t *)buf;
  int bufsztype;
#ifdef AUTOTUNE
  double cost;
#endif

  if (count <= 0) return retval;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered: no file buffer found");
    return lives_read(fd, buf, count, allow_less);
  }

  if (!fbuff->read) {
    LIVES_ERROR("lives_read_buffered: wrong buffer type");
    return 0;
  }

  bufsztype = fbuff->bufsztype;

#ifdef AUTOTUNE
  if (!fbuff->slurping && fbuff->bufsztype != BUFF_SIZE_READ_CUSTOM) {
    cost = 1. / (1. + (double)fbuff->nseqreads);
    if (fbuff->bufsztype == BUFF_SIZE_READ_SMALL) {
      if (!tuneds && (!tuners || (tunedsm && !tunedm))) tuners = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 3);
      autotune_u64(tuners, 8, smedbytes / 4, 16, cost);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_SMALLMED) {
      if (tuneds && !tunedsm && !tunersm) tunersm = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 4);
      autotune_u64(tunersm, smbytes * 4, 32768, 16, cost);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_MED) {
      if (tunedsm && !tunedm && !tunerm) tunerm = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 5);
      autotune_u64(tunerm, smedbytes * 4, 65536 * 2, 32, cost);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_LARGE) {
      if (tunedm && !tunedl && !tunerl) tunerl = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 6);
      autotune_u64(tunerl, medbytes * 4, 8192 * 1024, 256, cost);
    }
  }
#endif
  if (!buf) {
    /// function can be called with buf == NULL to preload a buffer with at least (count) bytes
    lives_freep((void **)&fbuff->buffer);
    if (allow_less) {
      bufsztype = BUFF_SIZE_READ_SMALL;
      if (ocount >= (smbytes >> 2) || count > smbytes) bufsztype = BUFF_SIZE_READ_SMALLMED;
      if (ocount >= (smedbytes >> 2) || count > smedbytes) bufsztype = BUFF_SIZE_READ_MED;
      if (ocount >= (medbytes >> 1) || count > medbytes) bufsztype = BUFF_SIZE_READ_LARGE;
      fbuff->bufsztype = bufsztype;
    } else {
      fbuff->bufsztype = BUFF_SIZE_READ_CUSTOM;
      fbuff->bytes = count;
    }
    return file_buffer_fill(fbuff, count);
  }

  fbuff->totops++;

  // read bytes from fbuff
  if (fbuff->bytes > 0 || fbuff->slurping) {
    size_t nbytes;
    if (fbuff->slurping) {
      if (fbuff->orig_size - fbuff->bytes < count) {
        count = fbuff->orig_size - fbuff->bytes;
        if (!count) goto rd_exit;
      }
      while ((nbytes = fbuff->offset - fbuff->bytes) < count  && !fbuff->eof) {
        lives_nanosleep(1000);
      }
    } else nbytes = fbuff->bytes;
    if (nbytes > count) nbytes = count;

    // use up buffer

    if (fbuff->invalid) {
      if (mainw->is_exiting) {
        return retval;
      }
      fbuff->offset -= (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) fbuff->bytes = (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      fbuff->buffer = NULL;
      file_buffer_fill(fbuff, fbuff->bytes);
      fbuff->invalid = FALSE;
    }

    lives_memcpy(ptr, fbuff->ptr, nbytes);

    retval += nbytes;
    count -= nbytes;
    fbuff->ptr += nbytes;
    ptr += nbytes;
    fbuff->totbytes += nbytes;

    if (fbuff->slurping) fbuff->bytes += nbytes;
    else fbuff->bytes -= nbytes;

    fbuff->nseqreads++;
    if (fbuff->slurping) goto rd_exit;
    if (count == 0) goto rd_done;
    if (fbuff->eof && !fbuff->reversed) goto rd_done;
    fbuff->nseqreads--;
    if (fbuff->reversed) {
      fbuff->offset -= (fbuff->ptr - fbuff->buffer) + count;
    }
  }

  /// buffer used up

  if (count <= bigbytes || fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) {
    if (fbuff->bufsztype != BUFF_SIZE_READ_CUSTOM) {
      bufsztype = BUFF_SIZE_READ_SMALL;
      if (ocount >= (smbytes >> 2) || count > smbytes) bufsztype = BUFF_SIZE_READ_SMALLMED;
      if (ocount >= (smedbytes >> 2) || count > smedbytes) bufsztype = BUFF_SIZE_READ_MED;
      if (ocount >= (medbytes >> 1) || count > medbytes) bufsztype = BUFF_SIZE_READ_LARGE;
      if (fbuff->bufsztype < bufsztype) fbuff->bufsztype = bufsztype;
    } else bufsztype = BUFF_SIZE_READ_CUSTOM;
    if (fbuff->invalid) {
      if (mainw->is_exiting) {
        return retval;
      }
      fbuff->offset -= (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) fbuff->bytes = (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      fbuff->buffer = NULL;
      file_buffer_fill(fbuff, fbuff->bytes);
      fbuff->invalid = FALSE;
    } else {
      if (fbuff->bufsztype != bufsztype) {
        lives_freep((void **)&fbuff->buffer);
      }
    }

    if (fbuff->bufsztype != bufsztype) fbuff->nseqreads = 0;
    res = file_buffer_fill(fbuff, count);
    if (res < 0)  {
      retval = res;
      goto rd_done;
    }

    // buffer is sufficient (or eof hit)
    if (res > count) res = count;
    lives_memcpy(ptr, fbuff->ptr, res);
    retval += res;
    fbuff->ptr += res;
    fbuff->bytes -= res;
    count -= res;
    fbuff->totbytes += res;
  } else {
    // larger size -> direct read
    if (fbuff->bufsztype != bufsztype) {
      if (fbuff->invalid) {
        fbuff->buffer = NULL;
        fbuff->invalid = FALSE;
      } else {
        lives_freep((void **)&fbuff->buffer);
      }
    }

    fbuff->offset = lseek(fbuff->fd, fbuff->offset, SEEK_SET);

    res = lives_read(fbuff->fd, ptr, count, TRUE);
    if (res < 0) {
      retval = res;
      goto rd_done;
    }
    fbuff->offset += res;
    count -= res;
    retval += res;
    fbuff->totbytes += res;
    if (res < count) fbuff->eof = TRUE;
  }

rd_done:
#ifdef AUTOTUNE
  if (fbuff->bufsztype == BUFF_SIZE_READ_SMALL) {
    if (tuners) {
      smbytes = autotune_u64_end(&tuners, smbytes);
      if (!tuners) {
        tuneds = TRUE;
      }
    }
  } else if (fbuff->bufsztype == BUFF_SIZE_READ_SMALLMED) {
    if (tunersm) {
      smedbytes = autotune_u64_end(&tunersm, smedbytes);
      if (!tunersm) {
        tunedsm = TRUE;
        smedbytes = get_near2pow(smedbytes);
        if (prefs->show_dev_opts) {
          char *tmp;
          g_printerr("value rounded to %s\n", (tmp = lives_format_storage_space_string(smedbytes)));
          lives_free(tmp);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
  else if (fbuff->bufsztype == BUFF_SIZE_READ_MED) {
    if (tunerm) {
      medbytes = autotune_u64_end(&tunerm, medbytes);
      if (!tunerm) {
        tunedm = TRUE;
        medbytes = get_near2pow(medbytes);
        if (prefs->show_dev_opts) {
          char *tmp;
          g_printerr("value rounded to %s\n", (tmp = lives_format_storage_space_string(medbytes)));
          lives_free(tmp);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*
  else {
    if (tunerl) {
      bigbytes = autotune_u64_end(&tunerl, bigbytes);
      if (!tunerl) {
        tunedl = TRUE;
        bigbytes = get_near2pow(bigbytes);
        if (prefs->show_dev_opts) {
          char *tmp;
          g_printerr("value rounded to %s\n", (tmp = lives_format_storage_space_string(bigbytes)));
          lives_free(tmp);
	  // *INDENT-OFF*
        }}}}
  // *INDENT-ON*

#endif
rd_exit:
  if (!allow_less && count > 0) {
    do_file_read_error(fd, retval, NULL, ocount);
    lives_close_buffered(fd);
  }

  return retval;
}


ssize_t lives_read_le_buffered(int fd, void *buf, ssize_t count, boolean allow_less) {
  ssize_t retval;
  if (count <= 0) return 0;
  retval = lives_read_buffered(fd, buf, count, allow_less);
  if (retval < count) return retval;
  if (capable->byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    reverse_bytes((char *)buf, count, count);
  }
  return retval;
}


boolean lives_read_buffered_eof(int fd) {
  lives_file_buffer_t *fbuff;
  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered: no file buffer found");
    return TRUE;
  }

  if (!fbuff->read) {
    LIVES_ERROR("lives_read_buffered_eof: wrong buffer type");
    return FALSE;
  }
  return (fbuff->eof && ((!fbuff->reversed && !fbuff->bytes)
                         || (fbuff->reversed && fbuff->ptr == fbuff->buffer)));
}


static ssize_t lives_write_buffered_direct(lives_file_buffer_t *fbuff, const char *buf, ssize_t count, boolean allow_fail) {
  ssize_t res = 0;
  ssize_t bytes = fbuff->bytes;

  if (count <= 0) return 0;

  if (bytes > 0) {
    res = file_buffer_flush(fbuff);
    // this is correct, as flush will have called close again with should_close=FALSE;
    if (!allow_fail && res < bytes) return 0;
  }
  res = 0;

  while (count > 0) {
    size_t bytes;
#define WRITE_ALL
#ifdef WRITE_ALL
    size_t bigbsize = count;
#else
    size_t bigbsize = BUFFER_FILL_BYTES_LARGE;
#endif
    if (bigbsize > count) bigbsize = count;
    bytes = lives_write(fbuff->fd, buf + res, bigbsize, allow_fail);
    if (bytes == bigbsize) {
      fbuff->offset += bytes;
      count -= bytes;
      res += bytes;
    } else {
      LIVES_ERROR("lives_write_buffered: error in bigblock writer");
      if (!fbuff->allow_fail) {
        lives_close_buffered(-fbuff->fd); // use -fd as lives_write will have closed
        return res;
      }
      break;
    }
  }
  return res;
}


ssize_t lives_write_buffered(int fd, const char *buf, ssize_t count, boolean allow_fail) {
  lives_file_buffer_t *fbuff;
  ssize_t retval = 0, res;
  size_t space_left;
  int bufsztype = BUFF_SIZE_WRITE_SMALL;
  ssize_t buffsize;

  if (!(fbuff = find_in_file_buffers(fd))) {
    LIVES_DEBUG("lives_write_buffered: no file buffer found");
    return lives_write(fd, buf, count, allow_fail);
  }

  if (fbuff->read) {
    LIVES_ERROR("lives_write_buffered: wrong buffer type");
    return 0;
  }

  if (count <= 0) return 0;

  if (count > BUFFER_FILL_BYTES_LARGE) return lives_write_buffered_direct(fbuff, buf, count, allow_fail);

  fbuff->totops++;
  fbuff->totbytes += count;

  if (count >= BUFFER_FILL_BYTES_BIGMED >> 1)
    bufsztype = BUFF_SIZE_WRITE_LARGE;
  else if (count >= BUFFER_FILL_BYTES_MED >> 1)
    bufsztype = BUFF_SIZE_WRITE_BIGMED;
  else if (fbuff->totbytes >= BUFFER_FILL_BYTES_SMALLMED)
    bufsztype = BUFF_SIZE_WRITE_MED;
  else if (fbuff->totbytes >= BUFFER_FILL_BYTES_SMALL)
    bufsztype = BUFF_SIZE_WRITE_SMALLMED;

  if (bufsztype < fbuff->bufsztype) bufsztype = fbuff->bufsztype;

  fbuff->allow_fail = allow_fail;

  // write bytes to fbuff
  while (count) {
    if (!fbuff->buffer) fbuff->bufsztype = bufsztype;

    if (fbuff->bufsztype == BUFF_SIZE_WRITE_SMALL)
      buffsize = BUFFER_FILL_BYTES_SMALL;
    else if (fbuff->bufsztype == BUFF_SIZE_WRITE_SMALLMED)
      buffsize = BUFFER_FILL_BYTES_SMALLMED;
    else if (fbuff->bufsztype == BUFF_SIZE_WRITE_MED)
      buffsize = BUFFER_FILL_BYTES_MED;
    else if (fbuff->bufsztype == BUFF_SIZE_WRITE_BIGMED)
      buffsize = BUFFER_FILL_BYTES_BIGMED;
    else
      buffsize = BUFFER_FILL_BYTES_LARGE;

    if (!fbuff->buffer) {
      fbuff->buffer = (uint8_t *)lives_calloc(buffsize >> 4, 16);
      fbuff->ptr = fbuff->buffer;
      fbuff->bytes = 0;

#ifdef HAVE_POSIX_FALLOCATE
      // pre-allocate space for next buffer, we need to ftruncate this when closing the file
      //g_print("alloc space in %d from %ld to %ld\n", fbuff->fd, fbuff->offset, fbuff->offset + buffsize);
      posix_fallocate(fbuff->fd, fbuff->offset, buffsize);
      /* lseek(fbuff->fd, fbuff->offset, SEEK_SET); */
#endif
    }

    space_left = buffsize - fbuff->bytes;
    if (space_left > count) {
      lives_memcpy(fbuff->ptr, buf, count);
      retval += count;
      fbuff->ptr += count;
      fbuff->bytes += count;
      count = 0;
    } else {
      lives_memcpy(fbuff->ptr, buf, space_left);
      fbuff->bytes = buffsize;
      res = file_buffer_flush(fbuff);
      retval += space_left;
      if (res < buffsize) return (res < 0 ? res : retval);
      count -= space_left;
      buf += space_left;
      if (fbuff->bufsztype != bufsztype) {
        lives_free(fbuff->buffer);
        fbuff->buffer = NULL;
      }
    }
  }
  return retval;
}


ssize_t lives_buffered_write_printf(int fd, boolean allow_fail, const char *fmt, ...) {
  ssize_t ret;
  va_list xargs;
  char *text;
  va_start(xargs, fmt);
  text = lives_strdup_vprintf(fmt, xargs);
  va_end(xargs);
  ret = lives_write_buffered(fd, text, lives_strlen(text), allow_fail);
  lives_free(text);
  return ret;
}


ssize_t lives_write_le_buffered(int fd, const void *buf, ssize_t count, boolean allow_fail) {
  if (count <= 0) return 0;
  if (capable->byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
    reverse_bytes((char *)buf, count, count);
  }
  return lives_write_buffered(fd, (char *)buf, count, allow_fail);
}


off_t lives_lseek_buffered_writer(int fd, off_t offset) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_lseek_buffered_writer: no file buffer found");
    return lseek(fd, offset, SEEK_SET);
  }

  if (fbuff->read) {
    LIVES_ERROR("lives_lseek_buffered_writer: wrong buffer type");
    return 0;
  }

  if (fbuff->bytes > 0) {
    ssize_t res = file_buffer_flush(fbuff);
    if (res < 0) return res;
    if (res < fbuff->bytes && !fbuff->allow_fail) {
      fbuff->eof = TRUE;
      return fbuff->offset;
    }
  }
  fbuff->offset = lseek(fbuff->fd, offset, SEEK_SET);
  return fbuff->offset;
}


off_t lives_buffered_offset(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_buffered_offset: no file buffer found");
    return lseek(fd, 0, SEEK_CUR);
  }

  if (fbuff->read) return fbuff->offset - fbuff->bytes;
  return fbuff->offset + fbuff->bytes;
}


size_t lives_buffered_orig_size(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_buffered_offset: no file buffer found");
    return lseek(fd, 0, SEEK_CUR);
  }

  if (!fbuff->read) return fbuff->orig_size;
  if (fbuff->orig_size == 0) fbuff->orig_size = (size_t)get_file_size(fd);
  return fbuff->orig_size;
}


off_t lives_buffered_flush(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_lseek_buffered_writer: no file buffer found");
    return 0;
  }

  if (fbuff->read) {
    LIVES_ERROR("lives_lseek_buffered_writer: wrong buffer type");
    return 0;
  }

  if (fbuff->bytes > 0) {
    ssize_t res = file_buffer_flush(fbuff);
    if (res < 0) return res;
    if (res < fbuff->bytes && !fbuff->allow_fail) {
      fbuff->eof = TRUE;
    }
  }
  return fbuff->offset;
}
