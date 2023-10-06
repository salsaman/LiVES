// filesystem.h
// LiVES
// (c) G. Finch 2019 - 2022 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#ifdef IS_FREEBSD
#define DU_BLOCKSIZE 1024
#else
#define DU_BLOCKSIZE 1
#endif

#define DEF_FILE_PERMS (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) ///< non-executable, is modified by umask

/// return filename (no dir, no .ext) (c.f get_filename for inplace version)
#define lives_get_filename(fullname) lives_strstop(lives_path_get_basename((fullname)), '.')

char *filename_from_fd(char *val, int fd);

ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz);

// check if existitng directory is writeable, but do not create
boolean is_writeable_dir(const char *dir);

// check if dir is writeable, creating it if necessary
boolean lives_make_writeable_dir(const char *newdir);

// similar to is_writeable_dir, but the directory does not need to exist
// will also return FALSE if dir is the name of an existing file
boolean check_dir_access(const char *dir);

boolean check_file(const char *file_name, boolean check_exists);  ///< check if file exists

boolean ensure_isdir(char *fname);
boolean dirs_equal(const char *dira, const char *dirb);

char *get_extension(const char *filename);
char *ensure_extension(const char *fname, const char *ext) WARN_UNUSED;

void get_dirname(char *filename);
void get_rel_dirname(char *filename, const char *topdir);

char *get_dir(const char *filename);
char *get_rel_dir(const char *filename, const char *topdir);

void get_basename(char *filename);
void get_filename(char *filename, boolean strip_dir);

char *get_ancestor_dir(const char *pathname, boolean append_first);

off_t get_file_size(int fd, boolean maybe_padded);
off_t sget_file_size(const char *name);

boolean lives_fsync(int fd);
void lives_sync(int times);

int lives_fputs(const char *s, FILE *stream);
char *lives_fgets(char *s, int size, FILE *stream);
size_t lives_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t lives_fread_string(char *buff, size_t stlen, const char *fname);
char *lives_fread_line(const char *fname);

int lives_open3(const char *pathname, int flags, mode_t mode);
int lives_open2(const char *pathname, int flags);
ssize_t lives_write(int fd, livesconstpointer buf, ssize_t count, boolean allow_fail);
ssize_t lives_write_le(int fd, livesconstpointer buf, ssize_t count, boolean allow_fail);
ssize_t lives_read(int fd, void *buf, ssize_t count, boolean allow_less);
ssize_t lives_read_le(int fd, void *buf, ssize_t count, boolean allow_less);

// buffered io
void init_fbuff_sizes(const char *workdir);

/// fixed values only for write buffers (must be multiples of 16)
// selection of buufer / flush size is determined by the chunk of the data being written
// the idea of course it to buffer a few writes and flush them at once
#define BUFFER_FILL_BYTES_SMALL 64   /// 1 -> 8 byte chunks
#define BUFFER_FILL_BYTES_SMALLMED 1024 /// 65 - 256 byte chunks
#define BUFFER_FILL_BYTES_MED 4096  /// 257 -> 4096 bytes  ---> also size of a memory page
#define BUFFER_FILL_BYTES_BIGMED 32768  ///  - 32K bytes
#define BUFFER_FILL_BYTES_LARGE 256 * 1024 /// 256KB
// chunk sizes > LARGE are written directly

// these are the defined type sizes, we attempt to tune things by testing various values in a specified range
// using a variety of binary search to arrive at the empirically 'best' value. The value is rounded to the nearest
// power of 2 in any case, but we may be able to determin for example,
// if 64K reads are drastically better or worse than 128K, and try to use the better values
// an idea for the future would be periodically retest, in case spme temporay condition was occutring
// and going one step further everything would be coordinated by the 'performance manager' task
#define BUFF_SIZE_READ_SLURP -2
#define BUFF_SIZE_READ_CUSTOM -1
#define BUFF_SIZE_READ_SMALL 0
#define BUFF_SIZE_READ_SMALLMED 1
#define BUFF_SIZE_READ_MED 2
#define BUFF_SIZE_READ_LARGE 3

#define TUNABLE_SMALLFILE_FLUSH_RANGEMIN 8
#define TUNABLE_SMALLFILE_FLUSH_RANGEMAX 268

#define TUNABLE_SMEDFILE_FLUSH_RANGEMIN (smbytes * 4)
#define TUNABLE_SMEDFILE_FLUSH_RANGEMAX 16384

#define TUNABLE_MEDFILE_FLUSH_RANGEMIN (smedbytes * 4)
#define TUNABLE_MEDFILE_FLUSH_RANGEMAX 131072

#define TUNABLE_LARGEFILE_FLUSH_RANGEMIN (medbytes * 4)
#define TUNABLE_LARGEFILE_FLUSH_RANGEMAX 838860			// 8MB
// sizes larger than this are written directly

#define BUFF_SIZE_WRITE_CUSTOM 		-1
#define BUFF_SIZE_WRITE_SMALL 		0
#define BUFF_SIZE_WRITE_SMALLMED	1
#define BUFF_SIZE_WRITE_MED		2
#define BUFF_SIZE_WRITE_BIGMED		3
#define BUFF_SIZE_WRITE_LARGE		4

#define FREAD_BUFFSIZE 65536

// options
#define FB_FLAG_RDONLY		(1ull << 0)
#define FB_FLAG_ALLOW_FAIL	(1ull << 1)
#define FB_FLAG_REVERSE		(1ull << 2)
#define FB_FLAG_USE_RINGBUFF   	(1ull << 3)

// internal values
#define FB_FLAG_BG_OP		(1ull << 16)
#define FB_FLAG_PREALLOC	(1ull << 17)
#define FB_FLAG_TUNING		(1ull << 18)
#define FB_FLAG_MMAP		(1ull << 19)

#define FB_FLAG_SLURPING	(1ull << 20)

// status bits
#define FB_FLAG_EOF		(1ull << 32)
#define FB_FLAG_INVALID		(1ull << 33)

typedef struct {
  int idx;  ///< identifier in list
  int fd; ///< number of underlying file (maybe become -1 if detached)
  volatile ssize_t bytes;  ///< bytes written in buffer / bytes left to read
  uint8_t *ptr;   ///< read / write point in buffer
  uint8_t *buffer;   ///< address of buffer start : (ptr - buffer + bytes) gives the read size
  uint8_t *ring_buffer;   /// alt for bg flushing
  size_t rbf_size; ///< ring buffer bytes filled
  volatile off_t offset; ///< offset in bytes of END of block, relative to start of file
  off_t skip; ///< count of bytes skipped at start
  int bufsztype;
  size_t custom_size;
  int nseqreads; ///< count of sequential reads since last buffer fill
  int totops; ///< count of operations peformed on buffer
  int64_t totbytes; ///< total bytes read / written to / from buffer
  size_t orig_size; ///< size in bytes of underlying file
  char *pathname; ///< path to underlying file
  pthread_mutex_t sync_mutex;
  volatile uint64_t flags;
} lives_file_buffer_t;

lives_file_buffer_t *find_in_file_buffers(int fd);
lives_file_buffer_t *find_in_file_buffers_by_pathname(const char *pathname);

/// standard buffer sizes - for CUSTOM sizes, use custom_size instead
/// for read buffers, the size may adsjust depending on tuning
size_t get_read_buff_size(int sztype);
size_t get_write_buff_size(int sztype);

int lives_open_buffered_rdonly(const char *pathname);
int lives_open_buffered_writer(const char *pathname, int mode, boolean append);
int lives_create_buffered(const char *pathname, int mode);
int lives_create_buffered_nosync(const char *pathname, int mode);
ssize_t lives_close_buffered(int fd);
off_t lives_lseek_buffered_writer(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t offset);
uint8_t *lives_buffered_get_data(int fd);
off_t lives_buffered_offset(int fd);
size_t lives_buffered_orig_size(int fd);
boolean lives_read_buffered_eof(int fd);
boolean lives_buffered_rdonly_set_reversed(int fd, boolean val);
boolean lives_write_buffered_set_ringmode(int fd);
ssize_t lives_write_buffered_set_custom_size(int fd, size_t count);
ssize_t lives_write_buffered(int fd, const char *buf, ssize_t count, boolean allow_fail);
ssize_t lives_buffered_write_printf(int fd, boolean allow_fail, const char *fmt, ...);
ssize_t lives_write_le_buffered(int fd, livesconstpointer buf, ssize_t count, boolean allow_fail);
ssize_t lives_read_buffered(int fd, void *buf, ssize_t count, boolean allow_less);
ssize_t lives_read_buffered_line(int fd, void *buf, const char sep, size_t maxlen);
ssize_t lives_read_le_buffered(int fd, void *buf, ssize_t count, boolean allow_less);
void lives_buffered_rdonly_slurp(int fd, off_t skip);
lives_proc_thread_t lives_buffered_rdonly_slurp_prep(int fd, off_t skip);
boolean lives_buffered_rdonly_slurp_ready(lives_proc_thread_t lpt);
boolean lives_buffered_rdonly_is_slurping(int fd);

off_t lives_buffered_flush(int fd);

// diskspace checks etc

/// disk/storage status values
typedef enum {
  LIVES_STORAGE_STATUS_UNKNOWN = 0,
  LIVES_STORAGE_STATUS_NORMAL,
  LIVES_STORAGE_STATUS_WARNING,
  LIVES_STORAGE_STATUS_CRITICAL,
  LIVES_STORAGE_STATUS_OVERFLOW,
  LIVES_STORAGE_STATUS_OVER_QUOTA,
  LIVES_STORAGE_STATUS_OFFLINE
} lives_storage_status_t;

/// TODO - do we really need 3 different functions ?
boolean check_storage_space(int clipno, boolean is_processing);
boolean check_for_disk_space(boolean fullcheck);
lives_storage_status_t get_storage_status(const char *dir, uint64_t warn_level, int64_t *dsval, int64_t resvd);

char *lives_format_storage_space_string(uint64_t space);
uint64_t get_ds_free(const char *dir);

const char *get_shmdir(void);
uint64_t get_fs_blocksize(const char *dir);

lives_proc_thread_t disk_monitor_start(const char *dir);
boolean disk_monitor_running(const char *dir);
int64_t disk_monitor_check_result(const char *dir);
int64_t disk_monitor_wait_result(const char *dir, ticks_t timeout);
void disk_monitor_forget(void);

//////////////////////////// prefs file caching ////

LiVESList *cache_file_contents(const char *filename);
char *get_val_from_cached_list(const char *key, size_t maxlen, LiVESList *cache);
void cached_list_free(LiVESList **list);

void print_cache(LiVESList *cache);

#endif
