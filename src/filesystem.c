// filesystem.c
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#include <sys/statvfs.h>
#include "main.h"
#include "callbacks.h"

#include <sys/mman.h>

#ifdef HAVE_LIBEXPLAIN
#include <libexplain/system.h>
#include <libexplain/read.h>
#endif


off_t get_file_size(int fd, boolean is_native) {
  // get the size of file fd
  // is_native should be set in special cases to avoid mutex deadlocks

  struct stat filestat;
  off_t fsize = -1;
  lives_file_buffer_t *fbuff;

  if (fd < 0) return -1;

retry:
  if (is_native) {
    fstat(fd, &filestat);
    return filestat.st_size;
  }

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    is_native = TRUE;
    goto retry;
  }

  if ((fd = fbuff->fd) >= 0) {
    fstat(fd, &filestat);
    fsize = filestat.st_size;
  }

  //g_printerr("fssize for %d is %ld\n", fd, fsize);
  if (fbuff->flags & FB_FLAG_PREALLOC) {
    /// because of padding bytes... !!!!
    off_t f2size;
    if ((f2size = (off_t)(fbuff->offset + fbuff->bytes)) > fsize) return f2size;
  }

  if (fsize == -1) fsize = fbuff->orig_size;

  return fsize;
}


off_t sget_file_size(const char *name) {
  off_t res;
  struct stat xstat;
  if (!name) return 0;
  res = stat(name, &xstat);
  if (res < 0) return res;
  return xstat.st_size;
}


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


boolean check_file(const char *file_name, boolean check_existing) {
  int check;
  boolean exists = FALSE;
  char *msg;
  // file_name should be in utf8
  char *lfile_name = U82F(file_name);

  mainw->error = FALSE;

  while (1) {
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
      LiVESResponseType resp = LIVES_RESPONSE_NONE;
      mainw->error = TRUE;
      if (mainw && mainw->is_ready) {
        if (errno == EACCES)
          resp = do_file_perm_error(lfile_name, TRUE);
        else
          resp = do_write_failed_error_s_with_retry(lfile_name, NULL);
        if (resp == LIVES_RESPONSE_RETRY) {
          continue;
        }
      }
      lives_free(lfile_name);
      return FALSE;
    }

    close(check);
    break;
  }
  if (!exists) lives_rm(lfile_name);
  lives_free(lfile_name);
  return TRUE;
}


boolean check_dir_access(const char *dir, boolean leaveit) {
  // if a directory exists, make sure it is readable and writable
  // otherwise create it and then check
  // we test here by actually creating a (mkstemp) file and writing to it
  // dir is in locale encoding

  // see also is_writeable_dir() which uses access() to check directory permissions

  // WARNING: may leave some parents around
  char test[5] = "1234";
  char *testfile;
  boolean exists = lives_file_test(dir, LIVES_FILE_TEST_EXISTS);
  int fp;

  if (!exists) lives_mkdir_with_parents(dir, capable->umask);

  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) return FALSE;

  if (!is_writeable_dir(dir)) return FALSE;

  testfile = lives_build_filename(dir, "livestst-XXXXXX", NULL);
  fp = g_mkstemp(testfile);
  if (fp == -1) {
    lives_free(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    return FALSE;
  }
  if (lives_write(fp, test, 4, TRUE) != 4) {
    close(fp);
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  close(fp);
  fp = lives_open2(testfile, O_RDONLY);
  if (fp < 0) {
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  if (lives_read(fp, test, 4, TRUE) != 4) {
    close(fp);
    lives_rm(testfile);
    if (!exists) {
      lives_rmdir(dir, FALSE);
    }
    lives_free(testfile);
    return FALSE;
  }
  close(fp);
  lives_rm(testfile);
  if (!exists && !leaveit) {
    lives_rmdir(dir, FALSE);
  }
  lives_free(testfile);
  return TRUE;
}


boolean lives_make_writeable_dir(const char *newdir) {
  /// create a directory (including parents)
  /// and ensure we can actually write to it
  int ret = lives_mkdir_with_parents(newdir, capable->umask);
  int myerrno = errno;
  if (!check_dir_access(newdir, TRUE)) {
    // abort if we cannot create the new subdir
    if (myerrno == EINVAL) {
      LIVES_ERROR("Could not write to directory");
    } else LIVES_ERROR("Could not create directory");
    LIVES_ERROR(newdir);
    THREADVAR(com_failed) = FALSE;
    return FALSE;
  } else {
    if (ret != -1) {
      LIVES_DEBUG("Created directory");
      LIVES_DEBUG(newdir);
    }
  }
  return TRUE;
}


boolean is_writeable_dir(const char *dir) {
  // return FALSE if we cannot create / write to dir
  // dir should be in locale encoding
  // WARNING: this will actually create the directory (since we dont know if its parents are needed)

  //struct statvfs sbuf;
  if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
    lives_mkdir_with_parents(dir, capable->umask);
    if (!lives_file_test(dir, LIVES_FILE_TEST_IS_DIR)) {
      return FALSE;
    }
  }

  if (!access(dir, R_OK | W_OK | X_OK)) return TRUE;
  return FALSE;
}


void get_dirname(char *filename) {
  char *tmp;
  // get directory name from a file
  // filename should point to char[PATH_MAX]
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


LIVES_GLOBAL_INLINE void get_basename(char *filename) {
  // get basename from a file
  // (filename without directory)
  // filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  char *tmp = lives_path_get_basename(filename);
  lives_snprintf(filename, PATH_MAX, "%s", tmp);
  lives_free(tmp);
}


LIVES_GLOBAL_INLINE void get_filename(char *filename, boolean strip_dir) {
  // get filename (part without extension) of a file
  //filename should point to char[PATH_MAX]
  // WARNING: will change contents of filename
  if (strip_dir) get_basename(filename);
  lives_strstop(filename, '.');
}


LIVES_GLOBAL_INLINE char *get_extension(const char *filename) {
  // return file extension without the "."
  char *tmp = lives_path_get_basename(filename);
  char *ptr = strrchr(tmp, '.');
  if (!ptr) {
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
  // we then check if fname ends with ".ext". If not we append ext to fname.
  // we return a copy of fname, possibly modified. The string returned should be freed after use.
  // NOTE: the original ext is not changed.

  size_t se, sf;
  char *eptr = (char *)ext, *ret;

  if (!fname) return NULL;

  if (!ext || (se = strlen(ext)) == 0) return lives_strdup(fname);

  if (*eptr != '.') {
    eptr = lives_strdup_printf(".%s", ext);
    se++;
  }

  sf = lives_strlen(fname);
  if (sf < se || strcmp(fname + sf - se, eptr)) {
    ret = lives_strdup_printf("%s%s", fname, eptr);
  } else ret = lives_strdup(fname);
  if (eptr != (char *)ext) lives_free(eptr);
  return ret;
}


boolean ensure_isdir(char *fname) {
  // ensure dirname ends in a single dir separator
  // fname should be char[PATH_MAX]

  // returns TRUE if fname was altered

  size_t dslen = strlen(LIVES_DIR_SEP);
  ssize_t offs;
  boolean ret = FALSE;
  char *tmp = lives_strdup_printf("%s%s", LIVES_DIR_SEP, fname), *tmp2;
  size_t tlen = lives_strlen(tmp), slen, tlen2;

  while (1) {
    // recursively remove double DIR_SEP
    tmp2 = subst(tmp, LIVES_DIR_SEP LIVES_DIR_SEP, LIVES_DIR_SEP);
    if ((tlen2 = lives_strlen(tmp2)) < tlen) {
      ret = TRUE;
      lives_free(tmp);
      tmp = tmp2;
      tlen = tlen2;
    } else {
      if (tmp2 && tmp2 != tmp) lives_free(tmp2);
      break;
    }
  }

  if (ret) lives_snprintf(fname, PATH_MAX, "%s", tmp);
  if (tmp) lives_free(tmp);

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
  return (!lives_strcmp(dir1, dir2));
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


static ssize_t lives_write_cb(lives_file_buffer_t *fbuff) {
  ssize_t res = lives_write(fbuff->fd, fbuff->ring_buffer, fbuff->rbf_size, TRUE);
  fbuff->flags &= ~FB_FLAG_BG_OP;
  return res;
}


ssize_t lives_write_le(int fd, const void *buf, ssize_t count, boolean allow_fail) {
  if (count <= 0) return 0;
  if (capable->hw.byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
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


char *lives_fread_line(const char *fname) {
  char buff[FREAD_BUFFSIZE];
  FILE *infofile = fopen(fname, "r");
  if (infofile) {
    char *rets = lives_fgets(buff, FREAD_BUFFSIZE, infofile);
    fclose(infofile);
    if (rets) return lives_strdup(rets);
  }
  return NULL;
}


lives_file_buffer_t *find_in_file_buffers(int fd) {
  lives_file_buffer_t *fbuff = NULL;
  LiVESList *fblist;

  pthread_mutex_lock(&mainw->fbuffer_mutex);

  for (fblist = mainw->file_buffers; fblist; fblist = fblist->next) {
    fbuff = (lives_file_buffer_t *)fblist->data;
    if (fbuff->idx == fd) break;
  }
  pthread_mutex_unlock(&mainw->fbuffer_mutex);

  if (fblist) return fbuff;
  return NULL;
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
  if (capable->hw.byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    reverse_bytes((char *)buf, count, count);
  }
  return retval;
}

//// buffered io ////

// explanation of values

// read:
// fbuff->buffer holds (fbuff->ptr - fbuff->buffer + fbuff->bytes) bytes
// fbuff->offset is the next real read position (offest in source file), subtracting fbuff->bytes from this
// gives the "virtual" read position

// read x bytes : fbuff->ptr increases by x, fbuff->bytes decreases by x
// if fbuff->bytes is < x, then we concat fbuff->bytes, refill buffer from file, concat remaining bytes
// on read: fbuff->ptr = fbuff->buffer. fbuff->offset += bytes read, fbuff->bytes = bytes read
// if (fbuff->flags & FB_FLAG_REVERSE) is set then when filling the buffer,
// we first seek to a position (offset - 3 / 4 * buffsize), fbuff->ptr = fbuff->buffer + 3 / 4 buffsize, bytes = 1 / 4 buffsz


// on seek (read only):
// forward: seek by +z: if z < fbuff->bytes : fbuff->ptr += z, fbuff->bytes -= z
// if z > fbuff->bytes: subtract fbuff->bytes from z, and copy to out_buffer. Fill fbuff->buffer, and copy remainder.

// backward: if fbuff->ptr - z >= fbuff->buffer : fbuff->ptr -= z, fbuff->bytes += z
// fbuff->ptr - z < fbuff->buffer:  z -= (fbuff->ptr - fbuff->buffer), copy to end of out_buffer : Fill fbuff->buffer

// seek absolute: current viritual posn is fbuff->offset - fbuff->bytes : subtract this from absolute posn

// return value is always: fbuff->offset - fbuff->bytes

// when writing we simply fill up the buffer until full, then flush the buffer to file io
// buffer is finally flushed when we close the file (or we call file_buffer_flush)

// in this case fbuff->bytes holds the number of bytes written to fbuff->buffer, fbuff->offset contains the offset in the underlying file

// in append mode, seek is first the end of the file. In creat mode any existing file is truncated and overwritten.

// in write mode, if we have fallocate, then we preallocate the buffer size on disk.
// When the file is closed we truncate any remaining bytes. Thus CAUTION because the file size as read directly will include the
// padding bytes, and thus appending directly to the file will write after the padding.bytes, and either be overwritten or truncated.
// in this case the correct size can be obtained from

static ssize_t file_buffer_flush(lives_file_buffer_t *fbuff) {
  // returns number of bytes written to file io, or error code
  ssize_t res = 0;

  if (fbuff->buffer) {
    if (fbuff->flags & FB_FLAG_USE_RINGBUFF) {
      uint8_t *tmp_ptr = fbuff->ring_buffer;
      size_t buffsize;
      res = fbuff->bytes;
      lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);

      fbuff->flags |= FB_FLAG_BG_OP;
      fbuff->ring_buffer = fbuff->buffer;
      fbuff->rbf_size = fbuff->bytes;
      lives_proc_thread_create(LIVES_THRDATTR_NONE,
                               (lives_funcptr_t)lives_write_cb, 0, "V", fbuff);
      if (fbuff->bufsztype == BUFF_SIZE_WRITE_CUSTOM)
        buffsize = fbuff->custom_size;
      else buffsize = get_write_buff_size(fbuff->bufsztype);
      if (tmp_ptr) fbuff->buffer = tmp_ptr;
      else fbuff->buffer = (uint8_t *)lives_calloc(buffsize >> 4, 16);
    } else if (fbuff->buffer) {
      res = lives_write(fbuff->fd, fbuff->buffer, fbuff->bytes,
                        (fbuff->flags & FB_FLAG_ALLOW_FAIL) == FB_FLAG_ALLOW_FAIL);
    }
  }

  //g_print("writing %ld bytes to %d\n", fbuff->bytes, fbuff->fd);

  if (!(fbuff->flags & FB_FLAG_ALLOW_FAIL) && res < fbuff->bytes) {
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
    if (!(fbuff->flags & FB_FLAG_RDONLY) && mainw->memok) {
      file_buffer_flush(fbuff);
      fbuff->buffer = NULL;
    } else {
      fbuff->flags |= FB_FLAG_INVALID;
    }
  }

  pthread_mutex_unlock(&mainw->fbuffer_mutex);
}


static pthread_mutex_t nxtidx_mutex = PTHREAD_MUTEX_INITIALIZER;
static int fbuff_idx_base = 1000000;
static int next_fbuff_idx(void) {
  pthread_mutex_lock(&nxtidx_mutex);
  if (fbuff_idx_base < 2000000000) {
    int nxt = ++fbuff_idx_base;
    pthread_mutex_unlock(&nxtidx_mutex);
    return nxt;
  }
  pthread_mutex_unlock(&nxtidx_mutex);
  return -1;
}


static int lives_open_real_buffered(const char *pathname, int flags, int mode, boolean isread) {
  lives_file_buffer_t *fbuff, *xbuff;
  boolean is_append = FALSE;
  int fd, idx;

  if (flags & O_APPEND) {
    is_append = TRUE;
    flags &= ~O_APPEND;
  }

  fd = lives_open3(pathname, flags, mode);
  if (fd >= 0) {
    fbuff = (lives_file_buffer_t *)lives_calloc(sizeof(lives_file_buffer_t) >> 2, 4);
    fbuff->idx = idx = next_fbuff_idx();
    fbuff->fd = fd;
    if (isread) fbuff->flags |= FB_FLAG_RDONLY;
    fbuff->pathname = lives_strdup(pathname);
    fbuff->bufsztype = isread ? BUFF_SIZE_READ_SMALL : BUFF_SIZE_WRITE_SMALL;

    if ((xbuff = find_in_file_buffers(fd)) != NULL) {
      char *msg = lives_strdup_printf("Duplicate fd (%d) in file buffers !\n%s was not removed, and\n%s will be added.", fd,
                                      xbuff->pathname, fbuff->pathname);
      break_me("dupe fd in fbuffs");
      LIVES_ERROR(msg);
      lives_free(msg);
      lives_close_buffered(idx);
    } else {
      if (!isread && !(flags & O_TRUNC)) {
        if (is_append) fbuff->offset = fbuff->orig_size = lseek(fd, 0, SEEK_END);
        else fbuff->orig_size = (size_t)get_file_size(fd, TRUE);
        /// TODO - handle fsize < 0
      }
    }
    pthread_mutex_init(&fbuff->sync_mutex, NULL);
    pthread_mutex_lock(&mainw->fbuffer_mutex);
    mainw->file_buffers = lives_list_prepend(mainw->file_buffers, (livespointer)fbuff);
    pthread_mutex_unlock(&mainw->fbuffer_mutex);
  } else return fd;

  return idx;
}

static volatile size_t bigbytes = BUFFER_FILL_BYTES_LARGE;
static volatile size_t medbytes = BUFFER_FILL_BYTES_MED;
static volatile size_t smedbytes = BUFFER_FILL_BYTES_SMALLMED;
static volatile size_t smbytes = BUFFER_FILL_BYTES_SMALL;

#if AUTOTUNE_FILEBUFF_SIZES
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


static boolean _lives_buffered_rdonly_slurp(lives_file_buffer_t *fbuff, off_t skip) {
  // slurped files: start reading in from posn skip (bytes), done in a dedicated worker thread
  // we keep reading until entire file is loaded
  // fbuff->bytes increases as we read in from file, and for this type of file, fbuff->offest points to the 'virtual'
  // read position
  // FB_FLAG_REVERSE can be set, in this case, reading n bytes will reduce offset by n, in other cases it will increase by n
  // fbuff->skip is set to 'skip', fbuff->orig_size is set to the full size (ignoring skip)
  // fbuff->bufsztypeis set to BUFF_SIZE_READ_SLURP;
  // FB_FLAG_BG_OP is TRUE while the input file is being read in
  // since the buffer is locked in memory, this should only be used for small / medium files, with short lived lifecycle
  //
  // seeking within the buffer is supported, provided we don't seek before 'skip'
  // attempts to read part of the file not yet loaded will block until all data is loaded
  // tests with mmap actually proved to be slower than simply reading in the file in chunks
  // changing the read block size did not appear to make much difference, however
  // we do read a smaller chunk to start with, so that small requests can be served more rapidly

  lives_proc_thread_t self = THREADVAR(proc_thread);
  int fd = fbuff->fd;
  off_t fsize, bufsize = smedbytes, res;
  boolean retval = TRUE;

  if (lives_proc_thread_get_cancel_requested(self)) {
    // if caller gets cancelled, then it will send a cancel_request to this thread
    // then wait for us to return with retval
    lives_proc_thread_cancel(self);
    retval = FALSE;
    goto finished;
  }

  fbuff->orig_size = get_file_size(fd, TRUE);
  fsize = fbuff->orig_size - ABS(skip);

  if (fsize > 0) {
    // caller will wait until this thread goes to WAITING state, then do a sny_ready() and continue
    lives_free_if_non_null(lives_proc_thread_sync_with(lives_proc_thread_get_dispatcher(), NULL));
    // TODO - skip < 0 should truncate end bytes
#if defined HAVE_POSIX_FADVISE
    posix_fadvise(fd, skip, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fd, skip, 0, POSIX_FADV_NOREUSE);
    posix_fadvise(fd, skip, 0, POSIX_FADV_WILLNEED);
#endif
    fbuff->skip = skip;
    if (1) {
      off_t offs = skip > 0 ? skip : 0;
      fbuff->ptr = fbuff->buffer = lives_calloc_align(fsize);
      lseek(fd, offs, SEEK_SET);
      mlock(fbuff->buffer, fsize);
      //fbuff->buffer = fbuff->ptr = lives_calloc(1, fsize);
      //g_printerr("slurp for %d, %s with size %ld\n", fd, fbuff->pathname, fsize);
      while (fsize > 0) {
        if (fbuff->flags & FB_FLAG_INVALID) {
          fbuff->flags &= ~FB_FLAG_INVALID;
          break; // file was closed
        }
        if (bufsize > fsize) bufsize = fsize;
        res = lives_read(fd, fbuff->buffer + fbuff->bytes, bufsize, TRUE);
        //g_printerr("slurp for %d, %s with "
        // "size %ld, read %lu bytes, %lu remain\n", fd, fbuff->pathname, fbuff->orig_size, bufsize, fsize)
        if (res < 0) {
          retval = FALSE;
          break;
        }
        if (res > fsize) res = fsize;
        fbuff->bytes += res;
        fsize -= res;
        if (fsize >= bigbytes && bufsize >= medbytes) bufsize = bigbytes;
        else if (fsize >= medbytes && bufsize >= smedbytes) bufsize = medbytes;
        else if (fsize >= smedbytes) bufsize = smedbytes;
        //g_printerr("slurp %d oof %ld %ld remain %lu  \n", fd, fbuff->offset, fsize, ofsize);
        //if (mainw->disk_pressure > 0.) mainw->disk_pressure = check_disk_pressure(0.);
#ifdef _IS_LINUX_GNU
        readahead(fd, fbuff->bytes + skip, bufsize * 4);
#endif
      }
    }
    if (retval)
      lives_hooks_trigger(lives_proc_thread_get_hook_stacks(self), DATA_PREVIEW_HOOK);
  } else {
    // if there is not enough data to even try reading, we set EOF
    fbuff->flags |= FB_FLAG_EOF;
    lives_free_if_non_null(lives_proc_thread_sync_with(lives_proc_thread_get_dispatcher(), NULL));
  }

finished:
  fbuff->fd = -1;
  IGN_RET(close(fd));
  pthread_mutex_lock(&fbuff->sync_mutex);
  if (!retval) fbuff->flags |= FB_FLAG_INVALID;
  fbuff->flags &= ~FB_FLAG_BG_OP;
  pthread_mutex_unlock(&fbuff->sync_mutex);
  return retval;
}


boolean lives_buffered_rdonly_is_slurping(int fd) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (!fbuff || fbuff->bufsztype != BUFF_SIZE_READ_SLURP) return FALSE;
  return !!(fbuff->flags & FB_FLAG_BG_OP);
}


LIVES_GLOBAL_INLINE lives_proc_thread_t lives_buffered_rdonly_slurp_prep(int fd, off_t skip) {
  lives_proc_thread_t lpt;
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (!fbuff || fbuff->bufsztype == BUFF_SIZE_READ_SLURP) return NULL;
  lpt = lives_proc_thread_create(LIVES_THRDATTR_START_UNQUEUED,
                                 (lives_funcptr_t)_lives_buffered_rdonly_slurp, 0, "vI", fbuff, skip);
  if (lpt) {
    weed_set_voidptr_value(lpt, "_filebuff", (void *)fbuff);
    lives_proc_thread_set_cancellable(lpt);
  }
  return lpt;
}


boolean lives_buffered_rdonly_slurp_ready(lives_proc_thread_t lpt) {
  if (lpt) {
    lives_file_buffer_t *fbuff =
      (lives_file_buffer_t *)weed_get_voidptr_value(lpt, "_filebuff", NULL);
    pthread_mutex_lock(&fbuff->sync_mutex);
    fbuff->bufsztype = BUFF_SIZE_READ_SLURP;
    fbuff->flags |= FB_FLAG_BG_OP;
    fbuff->bytes = fbuff->offset = 0;
    pthread_mutex_unlock(&fbuff->sync_mutex);
    lives_proc_thread_queue(lpt, 0);
    lives_free_if_non_null(lives_proc_thread_sync_with(lpt, NULL));
    return TRUE;
  }
  return FALSE;
}


LIVES_GLOBAL_INLINE void lives_buffered_rdonly_slurp(int fd, off_t skip) {
  lives_buffered_rdonly_slurp_ready(lives_buffered_rdonly_slurp_prep(fd, skip));
}


LIVES_GLOBAL_INLINE boolean lives_buffered_rdonly_set_reversed(int fd, boolean val) {
  lives_file_buffer_t *fbuff = find_in_file_buffers(fd);
  if (!fbuff) {
    // normal non-buffered file
    LIVES_DEBUG("lives_buffered_rdonly_set_reversed: no file buffer found");
    return FALSE;
  }
  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_buffered_rdonly_set_reversed: wrong buffer type");
    return 0;
  }
  if (val) fbuff->flags |= FB_FLAG_REVERSE;
  else fbuff->flags &= ~FB_FLAG_REVERSE;
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

LIVES_GLOBAL_INLINE int lives_open_buffered_writer(const char *pathname, int mode, boolean append) {
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

  /* if (IS_VALID_CLIP(mainw->scrap_file) && mainw->files[mainw->scrap_file]->primary_src && */
  /*     fd == LIVES_POINTER_TO_INT(mainw->files[mainw->scrap_file]->primary_src)); */

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

  if (!(fbuff->flags & FB_FLAG_RDONLY) && should_close) {
    boolean allow_fail = ((fbuff->flags & FB_FLAG_ALLOW_FAIL) == FB_FLAG_ALLOW_FAIL);
    ssize_t bytes = fbuff->bytes;

    lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);

    if (bytes > 0) {
      ret = file_buffer_flush(fbuff);
      // this is correct, as flush will have called close again with should_close=FALSE;
      if (!allow_fail && ret < bytes) return ret;
    }
#ifdef HAVE_POSIX_FALLOCATE
    if (fbuff->flags & FB_FLAG_PREALLOC) {
      IGN_RET(ftruncate(fbuff->fd, MAX(fbuff->offset, fbuff->orig_size)));
      fbuff->flags &= ~FB_FLAG_PREALLOC;
      /* //g_print("truncated  at %ld bytes in %d\n", MAX(fbuff->offset, fbuff->orig_size), fbuff->fd); */
    }
#endif
  }

  if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    if (fbuff->flags & FB_FLAG_BG_OP) {
      pthread_mutex_lock(&fbuff->sync_mutex);
      fbuff->flags |= FB_FLAG_INVALID;
      pthread_mutex_unlock(&fbuff->sync_mutex);
      lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);
    }
    should_close = FALSE;
    munlock(fbuff->buffer, fbuff->bytes);
  }

  lives_free(fbuff->pathname);

  lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);

  pthread_mutex_lock(&mainw->fbuffer_mutex);
  if (should_close && fbuff->fd >= 0) ret = close(fbuff->fd);
  mainw->file_buffers = lives_list_remove(mainw->file_buffers, (livesconstpointer)fbuff);

  if (fbuff->buffer) {
#ifdef TEST_MMAP
    if (fbuff->flags & FB_FLAG_MMAP)
      munmap(fbuff->buffer, fbuff->bytes);
    else {
#endif
      lives_free(fbuff->buffer);
#ifdef TEST_MMAP
    }
#endif
  }

  if (fbuff->ring_buffer) {
    lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);
    lives_free(fbuff->ring_buffer);
  }

  lives_free(fbuff);
  pthread_mutex_unlock(&mainw->fbuffer_mutex);

  return ret;
}


size_t get_read_buff_size(int sztype) {
  switch (sztype) {
  case BUFF_SIZE_READ_SMALL: return smbytes;
  case BUFF_SIZE_READ_SMALLMED: return smedbytes;
  case BUFF_SIZE_READ_MED: return medbytes;
  case BUFF_SIZE_READ_LARGE: return bigbytes;
  default: break;
  }
  return 0;
}

size_t get_write_buff_size(int sztype) {
  switch (sztype) {
  case BUFF_SIZE_WRITE_SMALL: return BUFFER_FILL_BYTES_SMALL;
  case BUFF_SIZE_WRITE_SMALLMED: return BUFFER_FILL_BYTES_SMALLMED;
  case BUFF_SIZE_WRITE_MED: return BUFFER_FILL_BYTES_MED;
  case BUFF_SIZE_WRITE_BIGMED: return BUFFER_FILL_BYTES_BIGMED;
  case BUFF_SIZE_WRITE_LARGE: return BUFFER_FILL_BYTES_LARGE;
  default: break;
  }
  return 0; // custom perhaps
}


static ssize_t file_buffer_fill(lives_file_buffer_t *fbuff, ssize_t min) {
  ssize_t res;
  ssize_t delta = 0, bufsize;
  boolean reversed = (fbuff->flags & FB_FLAG_REVERSE);
  boolean autotune = FALSE;

#if AUTOTUNE_FILEBUFF_SIZES
  if ((fbuff->bufsztype != BUFF_SIZE_READ_CUSTOM && fbuff->bufsztype != BUFF_SIZE_READ_SLURP)
      && ((fbuff->bufsztype == BUFF_SIZE_READ_SMALL && !tuneds && (!tuners || (tunedsm && !tunedm)))
          || (fbuff->bufsztype == BUFF_SIZE_READ_SMALLMED && tuneds && !tunedsm && !tunersm)
          || (fbuff->bufsztype == BUFF_SIZE_READ_MED && tunedsm && !tunedm && !tunerm)
          || (fbuff->bufsztype == BUFF_SIZE_READ_LARGE && tunedm && !tunedl))) autotune = TRUE;
#endif

  if (min < 0) min = 0;

  if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) {
    if (fbuff->buffer) bufsize = fbuff->ptr - fbuff->buffer + fbuff->bytes;
    else {
      bufsize = fbuff->bytes;
      fbuff->bytes = 0;
    }
  } else bufsize = get_read_buff_size(fbuff->bufsztype);

  if (reversed) {
    if (min > bufsize) reversed = FALSE;
    else {
      delta = (bufsize >> 2);
      if (min <= delta) {
        delta *= 3;
        if (delta > fbuff->offset) delta = fbuff->offset;
      } else delta = 0;
    }
  }
  if (fbuff->buffer && bufsize > fbuff->ptr - fbuff->buffer + fbuff->bytes) {
    lives_freep((void **)&fbuff->buffer);
  }
  if (!fbuff->buffer || !fbuff->ptr) {
    fbuff->buffer = (uint8_t *)lives_calloc_safety(bufsize >> 1, 2);
  }
  fbuff->offset -= delta;
  fbuff->offset = lseek(fbuff->fd, fbuff->offset, SEEK_SET);

#if AUTOTUNE_FILEBUFF_SIZES
  if (autotune) {
    if (fbuff->bufsztype == BUFF_SIZE_READ_SMALL) {
      if (!tuneds && (!tuners || (tunedsm && !tunedm))) tuners = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 3);
      autotune_u64_start(tuners, TUNABLE_SMALLFILE_FLUSH_RANGEMIN, TUNABLE_SMALLFILE_FLUSH_RANGEMAX, 64);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_SMALLMED) {
      if (tuneds && !tunedsm && !tunersm) tunersm = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 4);
      autotune_u64_start(tunersm, TUNABLE_SMEDFILE_FLUSH_RANGEMIN, TUNABLE_SMEDFILE_FLUSH_RANGEMAX, 32);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_MED) {
      if (tunedsm && !tunedm && !tunerm) tunerm = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 5);
      autotune_u64_start(tunerm, TUNABLE_MEDFILE_FLUSH_RANGEMIN, TUNABLE_MEDFILE_FLUSH_RANGEMAX, 32);
    } else if (fbuff->bufsztype == BUFF_SIZE_READ_LARGE) {
      if (tunedm && !tunedl && !tunerl) tunerl = lives_plant_new_with_index(LIVES_WEED_SUBTYPE_TUNABLE, 6);
      autotune_u64_start(tunerl, TUNABLE_LARGEFILE_FLUSH_RANGEMIN, TUNABLE_LARGEFILE_FLUSH_RANGEMAX, 32);
    }
  }
#endif

  res = lives_read(fbuff->fd, fbuff->buffer, bufsize, TRUE);

  if (res < 0) {
    lives_close_buffered(-fbuff->fd); // use -fd as lives_read will have closed
    return res;
  }

#if AUTOTUNE_FILEBUFF_SIZES
  if (fbuff->bufsztype == BUFF_SIZE_READ_SMALL) {
    if (tuners) {
      smbytes = autotune_u64_end(&tuners, smbytes, 1. / (double)min);
      if (!tuners) {
        tuneds = TRUE;
      }
    }
  } else if (fbuff->bufsztype == BUFF_SIZE_READ_SMALLMED) {
    if (tunersm) {
      smedbytes = autotune_u64_end(&tunersm, smedbytes, 1. / (double)min);
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
      medbytes = autotune_u64_end(&tunerm, medbytes, 1. / (double)min);
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
      bigbytes = autotune_u64_end(&tunerl, bigbytes, 1. / (double)min);
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

  fbuff->bytes = res - delta;
  fbuff->ptr = fbuff->buffer + delta;
  fbuff->offset += res;
  if (res < bufsize) fbuff->flags |= FB_FLAG_EOF;
  else fbuff->flags &= ~FB_FLAG_EOF;

#if defined HAVE_POSIX_FADVISE || (defined _GNU_SOURCE && defined IS_LINUX_GNU)
  if (reversed) {
#if defined HAVE_POSIX_FADVISE
    posix_fadvise(fbuff->fd, 0, fbuff->offset - (bufsize >> 2) * 3, POSIX_FADV_RANDOM);
    posix_fadvise(fbuff->fd, fbuff->offset - (bufsize >> 2) * 3, bufsize, POSIX_FADV_WILLNEED);
#endif
#ifdef IS_LINUX_GNU
    readahead(fbuff->fd, fbuff->offset - (bufsize >> 2) * 3, bufsize);
#endif
  } else {
#if defined HAVE_POSIX_FADVISE
    posix_fadvise(fbuff->fd, fbuff->offset, 0, POSIX_FADV_SEQUENTIAL);
    posix_fadvise(fbuff->fd, fbuff->offset, bufsize, POSIX_FADV_WILLNEED);
#endif
#ifdef IS_LINUX_GNU
    readahead(fbuff->fd, fbuff->offset, bufsize);
#endif
  }
#endif

  return res - delta;
}


static off_t _lives_lseek_buffered_rdonly_relative(lives_file_buffer_t *fbuff, off_t offset) {
  off_t newoffs = 0;

  pthread_mutex_lock(&fbuff->sync_mutex);
  fbuff->flags &= ~FB_FLAG_EOF;
  pthread_mutex_unlock(&fbuff->sync_mutex);

  if (offset == 0) {
    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
      if (fbuff->offset + offset < fbuff->orig_size)
        return fbuff->offset;
    }
    return fbuff->offset - fbuff->bytes;
  }
  fbuff->nseqreads = 0;

  if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    fbuff->offset += offset;
    fbuff->ptr += offset;
    // exclude "skip" here, butinclude it in get_offset fn.
    return fbuff->offset;
  }

  if (offset > 0) {
    // seek forwards
    if (offset < fbuff->bytes) {
      // we can fill with bytes in the buffer
      fbuff->ptr += offset;
      fbuff->bytes -= offset;
      newoffs =  fbuff->offset - fbuff->bytes;
    } else {
      // otherwise, use up remaining bytes, and we will refill the buffer
      // on the next read
      offset -= fbuff->bytes;
      fbuff->offset += offset;
      fbuff->bytes = 0;
      newoffs = fbuff->offset;
    }
  } else {
    // seek backwards
    offset = -offset;
    if (offset <= fbuff->ptr - fbuff->buffer) {
      // we can use bytes in buffer
      fbuff->ptr -= offset;
      fbuff->bytes += offset;
      newoffs = fbuff->offset - fbuff->bytes;
      //g_print("inbuff\n");
    } else {
      // otherwise, use up remaining bytes, and we will refill the buffer
      // on the next read
      size_t bufsize;
      if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) {
        if (fbuff->buffer) bufsize = fbuff->ptr - fbuff->buffer + fbuff->bytes;
        else {
          bufsize = fbuff->bytes;
        }
      } else bufsize = get_read_buff_size(fbuff->bufsztype);

      if (fbuff->bytes > 0) {
        // rewind virtual offset to start
        offset -= fbuff->ptr - fbuff->buffer;

        // rewind physical offset to start
        fbuff->offset -= bufsize;
      }

      while (offset >= bufsize) {
        // continue stepping back if necessary
        offset -= bufsize;
        fbuff->offset -= bufsize;
      }

      if (fbuff->offset < 0) fbuff->offset = 0;
      newoffs = fbuff->offset - offset;
      if (newoffs < 0) newoffs = 0;
      fbuff->bytes = 0;
      fbuff->ptr = fbuff->buffer;
      fbuff->offset = newoffs;
    }
  }
#ifdef HAVE_POSIX_FADVISE
  if (fbuff->flags & FB_FLAG_REVERSE)
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

  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_lseek_buffered_rdonly: wrong buffer type");
    return 0;
  }

  return _lives_lseek_buffered_rdonly_relative(fbuff, offset);
}


off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t posn) {
  lives_file_buffer_t *fbuff;

  if (!(fbuff = find_in_file_buffers(fd))) {
    LIVES_DEBUG("lives_lseek_buffered_rdonly_absolute: no file buffer found");
    return lseek(fd, posn, SEEK_SET);
  }

  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_lseek_buffered_rdonly_absolute: wrong buffer type");
    return 0;
  }

  if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    posn -= fbuff->skip;
    if (posn < 0) posn = 0;
    if (posn > fbuff->orig_size) posn = fbuff->orig_size;
    posn -= fbuff->offset;
  } else {
    if (!fbuff->ptr || !fbuff->buffer) {
      fbuff->offset = posn;
      return fbuff->offset;
    }

    // this calculation gives us the relative read posn
    posn -= fbuff->offset - fbuff->bytes;
  }

  return _lives_lseek_buffered_rdonly_relative(fbuff, posn);
}


#if 0
ssize_t lives_buffered_readline(int fd, void *buf, const char sep, size_t maxlen,
                                boolean binmode) {
  ssize_t count = 0;
  int i;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered: no file buffer found");
    return lives_read(fd, buf, count, allow_less);
  }

  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_read_buffered: wrong buffer type");
    return 0;
  }
  lives_buffered_rdonly_set_reversed(fd, FALSE);
  while (1) {
    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
      nbytes = fbuff->bytes - fbuff->offset;
    } else nbytes = fbuff->bytes;
    if (!nbytes) {
      if (fbuff->bufsztype != BUFF_SIZE_READ_SLURP) {
        ssize_t ret = file_buffer_fill(fbuff, fbuff->bytes);
        if (ret < 0) return ret;
      } else {
        if (!(fbuff->flags & FB_FLAG_BG_OP)) break;
        else lives_nanosleep(1000);
        continue;
      }
    }
    for (i = 0; i < nbytes; i++) {
      c = fbuff->ptr[i];
      if (maxlen && i >= maxlen) break;
      if (!binmode && !c) break;
      if (c == sep) break;
    }
    if (!i) {
      if (!binmode) lives_memset(buf + count, 0, 1);
      break;
    }
    if (i < nbytes || (maxlen <= i)) {
      if (i > maxlen) i = maxlen;
      lives_memcpy(buf. fbuff->ptr, i);
      count += i;
      break;
    }
    count += i;
    maxlen -= i;
  }
  lives_lseek_buffered_rdonly_relative(fd, count);
  return count;
}
#endif

ssize_t lives_read_buffered(int fd, void *buf, ssize_t count, boolean allow_less) {
  lives_file_buffer_t *fbuff;
  ssize_t retval = 0, res = 0;
  ssize_t ocount = count;
  uint8_t *ptr = (uint8_t *)buf;
  int bufsztype;
  boolean reversed = FALSE;

  if (count <= 0) return retval;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered: no file buffer found");
    return lives_read(fd, buf, count, allow_less);
  }

  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_read_buffered: wrong buffer type");
    return 0;
  }

  reversed = (fbuff->flags & FB_FLAG_REVERSE) == FB_FLAG_REVERSE;
  bufsztype = fbuff->bufsztype;


  if (!buf) {
    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) return count;
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
  if (fbuff->bytes > 0 || fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    ssize_t nbytes;
    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
      if (fbuff->flags & FB_FLAG_REVERSE) {
        if (ocount > fbuff->offset) ocount = fbuff->offset;
        fbuff->offset -= ocount;
        fbuff->ptr -= ocount;
      }
      lives_nanosleep_while_true((nbytes = fbuff->bytes - fbuff->offset) < count
                                 && (fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);
      if (fbuff->bytes - fbuff->offset <= count) {
        pthread_mutex_lock(&fbuff->sync_mutex);
        fbuff->flags |= FB_FLAG_EOF;
        pthread_mutex_unlock(&fbuff->sync_mutex);
        count -= fbuff->bytes - fbuff->offset;
        if (count <= 0) goto rd_exit;
      }
    } else nbytes = fbuff->bytes;
    if (nbytes > count) nbytes = count;

    // use up buffer

    if (fbuff->flags & FB_FLAG_INVALID) {
      if (mainw->is_exiting) {
        return retval;
      }

      if (fbuff->bufsztype != BUFF_SIZE_READ_SLURP) {
        fbuff->offset -= (fbuff->ptr - fbuff->buffer + fbuff->bytes);
        if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM)
          fbuff->bytes = (fbuff->ptr - fbuff->buffer + fbuff->bytes);
        fbuff->buffer = NULL;
        file_buffer_fill(fbuff, fbuff->bytes);
      }
      fbuff->flags &= ~FB_FLAG_INVALID;
    }

    lives_memcpy(ptr, fbuff->ptr, nbytes);

    retval += nbytes;
    count -= nbytes;
    ptr += nbytes;
    fbuff->totbytes += nbytes;

    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
      if (!(fbuff->flags & FB_FLAG_REVERSE)) {
        fbuff->offset += nbytes;
        fbuff->ptr += nbytes;
      }
      pthread_mutex_lock(&fbuff->sync_mutex);
      if (count > 0) fbuff->flags |= FB_FLAG_EOF;
      else fbuff->flags &= ~FB_FLAG_EOF;
      pthread_mutex_unlock(&fbuff->sync_mutex);
    } else {
      fbuff->bytes -= nbytes;
      fbuff->ptr += nbytes;
    }

    fbuff->nseqreads++;
    if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) goto rd_exit;
    if (count == 0) goto rd_done;
    if ((fbuff->flags & FB_FLAG_EOF) && !(fbuff->flags & FB_FLAG_REVERSE)) goto rd_done;
    fbuff->nseqreads--;
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
    if (fbuff->flags & FB_FLAG_INVALID) {
      if (mainw->is_exiting) {
        return retval;
      }
      fbuff->offset -= (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      if (fbuff->bufsztype == BUFF_SIZE_READ_CUSTOM) fbuff->bytes = (fbuff->ptr - fbuff->buffer + fbuff->bytes);
      fbuff->buffer = NULL;
      file_buffer_fill(fbuff, fbuff->bytes);
      fbuff->flags &= ~FB_FLAG_INVALID;
    } else {
      if (fbuff->buffer && fbuff->bufsztype != bufsztype) {
        lives_freep((void **)&fbuff->buffer);
      }
    }

    if (fbuff->bufsztype != bufsztype) fbuff->nseqreads = 0;

    while (count) {
      fbuff->flags &= ~FB_FLAG_EOF;
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
      if (fbuff->flags & FB_FLAG_EOF) break;
      if ((fbuff->flags & FB_FLAG_REVERSE) && count > 0) fbuff->flags &= ~FB_FLAG_REVERSE;
      ptr += res;
    }
  } else {
    // larger size -> direct read
    if (fbuff->bufsztype != bufsztype) {
      if (fbuff->flags & FB_FLAG_INVALID) {
        fbuff->buffer = NULL;
        fbuff->flags &= ~FB_FLAG_INVALID;
      } else {
        lives_freep((void **)&fbuff->buffer);
      }
    } else {
      lives_freep((void **)&fbuff->buffer);
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
  }

  if (res < count) fbuff->flags |= FB_FLAG_EOF;

rd_done:

  if (reversed) fbuff->flags |= FB_FLAG_REVERSE;

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
  if (capable->hw.byte_order == LIVES_BIG_ENDIAN && !prefs->bigendbug) {
    reverse_bytes((char *)buf, count, count);
  }
  return retval;
}


boolean lives_read_buffered_eof(int fd) {
  lives_file_buffer_t *fbuff;
  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_read_buffered_eof: no file buffer found");
    return TRUE;
  }

  if (!(fbuff->flags & FB_FLAG_RDONLY)) {
    LIVES_ERROR("lives_read_buffered_eof: wrong buffer type");
    return FALSE;
  }
  return ((fbuff->flags & FB_FLAG_EOF) && ((!(fbuff->flags & FB_FLAG_REVERSE) && !fbuff->bytes)
          || ((fbuff->flags & FB_FLAG_REVERSE) && fbuff->ptr == fbuff->buffer)));
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
      LIVES_ERROR("lives_write_buffered_direct: error in bigblock writer");
      if (!(fbuff->flags & FB_FLAG_ALLOW_FAIL)) {
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

  if (fbuff->flags & FB_FLAG_RDONLY) {
    LIVES_ERROR("lives_write_buffered: wrong buffer type");
    return 0;
  }

  if (count <= 0) return 0;

  if (fbuff->bufsztype != BUFF_SIZE_WRITE_CUSTOM) {
    if (count > BUFFER_FILL_BYTES_LARGE) return lives_write_buffered_direct(fbuff, buf, count, allow_fail);

    if (count >= BUFFER_FILL_BYTES_BIGMED >> 1)
      bufsztype = BUFF_SIZE_WRITE_LARGE;
    else if (count >= BUFFER_FILL_BYTES_MED >> 1)
      bufsztype = BUFF_SIZE_WRITE_BIGMED;
    else if (fbuff->totbytes >= BUFFER_FILL_BYTES_SMALLMED)
      bufsztype = BUFF_SIZE_WRITE_MED;
    else if (fbuff->totbytes >= BUFFER_FILL_BYTES_SMALL)
      bufsztype = BUFF_SIZE_WRITE_SMALLMED;

    if (bufsztype < fbuff->bufsztype) bufsztype = fbuff->bufsztype;
  } else bufsztype = BUFF_SIZE_WRITE_CUSTOM;

  fbuff->totops++;
  fbuff->totbytes += count;
  if (allow_fail) fbuff->flags |= FB_FLAG_ALLOW_FAIL;
  else fbuff->flags &= ~FB_FLAG_ALLOW_FAIL;

  // write bytes to fbuff
  while (count) {
    if (!fbuff->buffer) fbuff->bufsztype = bufsztype;

    if (fbuff->bufsztype == BUFF_SIZE_WRITE_CUSTOM)
      buffsize = fbuff->custom_size;
    else buffsize = get_write_buff_size(fbuff->bufsztype);
    if (!fbuff->buffer) {
      fbuff->buffer = (uint8_t *)lives_calloc(buffsize >> 4, 16);
      fbuff->ptr = fbuff->buffer;
      fbuff->bytes = 0;

#ifdef HAVE_POSIX_FALLOCATE
      // pre-allocate space for next buffer, we need to ftruncate this when closing the file
      //g_print("alloc space in %d from %ld to %ld\n", fbuff->fd, fbuff->offset, fbuff->offset + buffsize);
      posix_fallocate(fbuff->fd, fbuff->offset, buffsize);
      fbuff->flags |= FB_FLAG_PREALLOC;
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
        if (fbuff->ring_buffer) {
          lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);
          lives_free(fbuff->ring_buffer);
          fbuff->ring_buffer = NULL;
        }
      }
    }
  }
  return retval;
}


boolean lives_write_buffered_set_ringmode(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_write_buffered_set_ringmode: no file buffer found");
    return FALSE;
  }

  if (fbuff->flags & FB_FLAG_RDONLY) {
    LIVES_ERROR("lives_write_buffered_set_ringmode: wrong buffer type");
    return FALSE;
  }

  fbuff->flags |= FB_FLAG_USE_RINGBUFF;
  return TRUE;
}


ssize_t lives_write_buffered_set_custom_size(int fd, size_t count) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_write_buffered_set_custom_size: no file buffer found");
    return -1;
  }

  if (fbuff->flags & FB_FLAG_RDONLY) {
    LIVES_ERROR("lives_write_buffered_set_custom_size: wrong buffer type");
    return -1;
  }

  if (fbuff->bytes > 0) {
    file_buffer_flush(fbuff);
  }

  count = (count >> 4) << 4;
  if (!count) return 0;

  if (fbuff->buffer) {
    lives_free(fbuff->buffer);
    fbuff->buffer = NULL;
  }

  if (fbuff->ring_buffer) {
    lives_nanosleep_while_true((fbuff->flags & FB_FLAG_BG_OP) == FB_FLAG_BG_OP);
    lives_free(fbuff->ring_buffer);
    fbuff->ring_buffer = NULL;
  }

  fbuff->bufsztype = BUFF_SIZE_WRITE_CUSTOM;
  fbuff->custom_size = count;
  return count;
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
  if (capable->hw.byte_order == LIVES_BIG_ENDIAN && (prefs->bigendbug != 1)) {
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

  if (fbuff->flags & FB_FLAG_RDONLY) {
    LIVES_ERROR("lives_lseek_buffered_writer: wrong buffer type");
    return 0;
  }

  if (fbuff->bytes > 0) {
    ssize_t bytes = fbuff->bytes;
    ssize_t res = file_buffer_flush(fbuff);
    if (res < 0) return res;
    if (res < bytes && !(fbuff->flags & FB_FLAG_ALLOW_FAIL)) {
      fbuff->flags |= FB_FLAG_EOF;
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

  if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    return fbuff->offset + fbuff->skip;
  }

  if (fbuff->flags & FB_FLAG_RDONLY) return fbuff->offset - fbuff->bytes;
  return fbuff->offset + fbuff->bytes;
}


size_t lives_buffered_orig_size(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_buffered_orig_size: no file buffer found");
    return lseek(fd, 0, SEEK_CUR);
  }

  if (fbuff->bufsztype == BUFF_SIZE_READ_SLURP) {
    return fbuff->orig_size;// + fbuff->skip;
  }

  if (!(fbuff->flags & FB_FLAG_RDONLY)) return fbuff->orig_size;
  if (fbuff->orig_size == 0) fbuff->orig_size = (size_t)get_file_size(fd, FALSE);
  return fbuff->orig_size;
}


uint8_t *lives_buffered_get_data(int fd) {
  lives_file_buffer_t *fbuff;
  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_buffered_get_data: no file buffer found");
    return NULL;
  }
  return fbuff->buffer;
}


off_t lives_buffered_flush(int fd) {
  lives_file_buffer_t *fbuff;

  if ((fbuff = find_in_file_buffers(fd)) == NULL) {
    LIVES_DEBUG("lives_buffered_flush: no file buffer found");
    return 0;
  }

  if (fbuff->flags & FB_FLAG_RDONLY) {
    LIVES_ERROR("lives_buffered_flush: wrong buffer type");
    return 0;
  }

  if (fbuff->bytes > 0) {
    ssize_t bytes = fbuff->bytes;
    ssize_t res = file_buffer_flush(fbuff);
    if (res < 0) return res;
    if (res < bytes && !(fbuff->flags & FB_FLAG_ALLOW_FAIL)) {
      fbuff->flags |= FB_FLAG_EOF;
    }
  }
  return fbuff->offset;
}

//////////////////////////////////////////////////////////

static void ds_warn(boolean freelow, uint64_t bytes) {
  char *reason, *aorb;
  char *amount = lives_format_storage_space_string(bytes);
  if (freelow) {
    reason = (_("FREE DISK SPACE"));
    aorb = (_("BELOW"));
  } else {
    reason = (_("DISK SPACE USED"));
    aorb = (_("ABOVE"));
  }
  d_print(_("\nRECORDING was PAUSED because %s in %s IS %s %s !\n"
            "Diskspace limits can be set in Preferences / Misc.\n"),
          reason, prefs->workdir, aorb, amount);
  on_record_perf_activate(NULL, NULL);
  d_print_urgency(URGENCY_MSG_TIMEOUT, _("RECORDING WAS PAUSED DUE TO DISKSPACE LIMITS\n"));
  lives_free(reason);
  lives_free(aorb);
}


boolean check_for_disk_space(boolean fullcheck) {
  /// fullcheck == FALSE, we MAY check ds used, and we WILL check free ds using cached value
  /// fullcheck == TRUE, we WILL update free ds
  int64_t free_ds = capable->ds_free;
  int64_t ds_used = capable->ds_used;
  static double xscrap_mb = -1., xascrap_mb = -1.;
  static double xxscrap_mb = -1., xxascrap_mb = -1.;
  static boolean wrtable = FALSE;

  double scrap_mb = 0., ascrap_mb = get_ascrap_mb();

  if (prefs->disk_quota == 0 && prefs->rec_stop_gb <= 0.
      && (!prefs->rec_stop_dwarn || !mainw->next_ds_warn_level)) return TRUE;

  if (IS_VALID_CLIP(mainw->scrap_file)) {
    scrap_mb = (double)mainw->files[mainw->scrap_file]->f_size / (double)ONE_MILLION;
  }

  //capable->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &dsval, 0);
  //capable->ds_free = dsval;

  if (prefs->disk_quota > 0) {
    int64_t ds_used;
    if ((ds_used = disk_monitor_check_result(prefs->workdir)) > -1) {
      capable->ds_used = ds_used;
      xxscrap_mb = scrap_mb;
      xxascrap_mb = ascrap_mb;
    } else {
      if (xxscrap_mb == -1. || xxscrap_mb > scrap_mb) xxscrap_mb = scrap_mb;
      if (xxascrap_mb == -1. || xxascrap_mb > ascrap_mb) xxascrap_mb = ascrap_mb;
    }
    if (ds_used > -1) {
      /// value is in BYTES
      ds_used += (int64_t)(scrap_mb + ascrap_mb - xxscrap_mb - xxascrap_mb)
                 * ONE_MILLION;
      if ((double)ds_used >= (double)(prefs->disk_quota * ONE_BILLION) * prefs->rec_stop_quota / 100.) {
        if (!RECORD_PAUSED && LIVES_IS_RECORDING) {
          ds_warn(FALSE, (uint64_t)ds_used);
        }
        return FALSE;
      }
    }
  }

  // check if we have enough free space left on the volume (return FALSE if not)
  if (prefs->rec_stop_gb > 0. || prefs->rec_stop_dwarn) {
    // check free space again
    if (fullcheck || free_ds == -1 || xscrap_mb == -1 || xascrap_mb == -1
        || scrap_mb < xscrap_mb || ascrap_mb < xascrap_mb) {
      capable->ds_status = get_storage_status(prefs->workdir, mainw->next_ds_warn_level, &ds_used, 0);
      capable->ds_free = free_ds = ds_used;
      xscrap_mb = scrap_mb;
      xascrap_mb = ascrap_mb;
      if (free_ds == 0) wrtable = is_writeable_dir(prefs->workdir);
      else wrtable = TRUE;
    }
    if (wrtable) {
      double free_mb = (double)free_ds / (double)ONE_MILLION;
      double freesp = free_mb - (scrap_mb + ascrap_mb - xscrap_mb - xascrap_mb);
      if ((double)freesp / 1000. < prefs->rec_stop_gb
          || (prefs->rec_stop_dwarn && free_ds <= mainw->next_ds_warn_level)) {
        if (!RECORD_PAUSED && LIVES_IS_RECORDING) {
          ds_warn(TRUE, free_ds);
        }
        return FALSE;
      }
    }
  }
  return TRUE;
}


////////////////// file caching //////

typedef struct {
  uint32_t hash;
  char *key;
  char *data;
} lives_speed_cache_t;


char *get_val_from_cached_list(const char *key, size_t maxlen, LiVESList * cache) {
  // WARNING - contents may be invalid if the underlying file is updated (e.g with set_*_pref())
  LiVESList *list = cache;
  uint32_t khash = fast_hash(key, 0);
  lives_speed_cache_t *speedy;
  for (; list; list = list->next) {
    speedy = (lives_speed_cache_t *)list->data;
    if (khash == speedy->hash && !lives_strcmp(key, speedy->key))
      return lives_strndup(speedy->data, maxlen);
  }
  return NULL;
}


LIVES_GLOBAL_INLINE void cached_list_free(LiVESList **list) {
  lives_speed_cache_t *speedy;
  for (LiVESList *xlist = *list; xlist; xlist = xlist->next) {
    speedy = (lives_speed_cache_t *)(*list)->data;
    if (speedy) {
      if (speedy->key) lives_free(speedy->key);
      if (speedy->data) lives_free(speedy->data);
      lives_free(speedy);
    }
    xlist->data = NULL;
  }
  lives_list_free(*list);
  *list = NULL;
}


void print_cache(LiVESList * cache) {
  /// for debugging
  lives_speed_cache_t *speedy;
  LiVESList *ll = cache;
  g_print("dumping cache %p\n", cache);
  for (; ll; ll = ll->next) {
    speedy = (lives_speed_cache_t *)ll->data;
    g_print("cache dets: %s = %s\n", speedy->key, speedy->data);
  }
}


LiVESList *cache_file_contents(const char *filename) {
  lives_speed_cache_t *speedy;
  LiVESList *list = NULL;
  FILE *hfile;
  size_t kelen;
  char buff[65536];
  char *key = NULL, *keystr_end = NULL, *cptr, *data = NULL;
  if (!(hfile = fopen(filename, "r"))) return NULL;
  while (fgets(buff, 65536, hfile)) {
    if (!*buff) continue;
    if (*buff == '#') continue;
    if (key) {
      if (!lives_strncmp(buff, keystr_end, kelen)) {
        speedy = (lives_speed_cache_t *)lives_calloc(1, sizeof(lives_speed_cache_t));
        speedy->hash = fast_hash(key, 0);
        speedy->key = key;
        speedy->data = data;
        key = data = NULL;
        lives_free(keystr_end);
        keystr_end = NULL;
        list = lives_list_prepend(list, speedy);
        continue;
      }
      cptr = buff;
      if (data) {
        if (*buff != '|') continue;
        cptr++;
      }
      lives_chomp(cptr, FALSE);
      data = lives_strcollate(&data, "\n", cptr);
      continue;
    }
    if (*buff != '<') continue;
    kelen = 0;
    for (cptr = buff; cptr; cptr++) {
      if (*cptr == '>') {
        kelen = cptr - buff;
        if (kelen > 2) {
          *cptr = 0;
          key = lives_strdup(buff + 1);
          keystr_end = lives_strdup_printf("</%s>", key);
          kelen++;
        }
        break;
      }
    }
  }
  fclose(hfile);
  if (key) lives_free(key);
  if (keystr_end) lives_free(keystr_end);
  return lives_list_reverse(list);
}

