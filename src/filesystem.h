// filesystem.h
// LiVES
// (c) G. Finch 2019 - 2020 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING or www.gnu.org for licensing details

#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

char *filename_from_fd(char *val, int fd);

ssize_t lives_readlink(const char *path, char *buf, size_t bufsiz);

boolean is_writeable_dir(const char *dir);
boolean check_dir_access(const char *dir, boolean leaveit);
boolean lives_make_writeable_dir(const char *newdir);
boolean check_file(const char *file_name, boolean check_exists);  ///< check if file exists

boolean ensure_isdir(char *fname);
boolean dirs_equal(const char *dira, const char *dirb);

char *get_extension(const char *filename);
char *ensure_extension(const char *fname, const char *ext) WARN_UNUSED;

void get_dirname(char *filename);
char *get_dir(const char *filename);
void get_basename(char *filename);
void get_filename(char *filename, boolean strip_dir);

off_t get_file_size(int fd);
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

/// fixed values only for write buffers (must be multiples of 16)
#define BUFFER_FILL_BYTES_SMALL 64   /// 1 -> 16 bytes
#define BUFFER_FILL_BYTES_SMALLMED 1024 /// 17 - 256 bytes
#define BUFFER_FILL_BYTES_MED 4096  /// 257 -> 2048 bytes
#define BUFFER_FILL_BYTES_BIGMED 16386  /// 2049 - 8192 bytes
#define BUFFER_FILL_BYTES_LARGE 65536

#define BUFF_SIZE_READ_SMALL 0
#define BUFF_SIZE_READ_SMALLMED 1
#define BUFF_SIZE_READ_MED 2
#define BUFF_SIZE_READ_LARGE 3
#define BUFF_SIZE_READ_CUSTOM -1
#define BUFF_SIZE_READ_SLURP -2

#define BUFF_SIZE_WRITE_SMALL 0
#define BUFF_SIZE_WRITE_SMALLMED 1
#define BUFF_SIZE_WRITE_MED 2
#define BUFF_SIZE_WRITE_BIGMED 3
#define BUFF_SIZE_WRITE_LARGE 4

#define DEF_BUFFSIZE 65536

typedef struct {
  volatile ssize_t bytes;  /// buffer size for write, bytes left to read in case of read
  uint8_t *ptr;   /// read point in buffer
  uint8_t *buffer;   /// ptr to data  (ptr - buffer + bytes) gives the read size
  off_t offset; // file offs (of END of block)
  off_t skip;
  int fd;
  int bufsztype;
  volatile boolean eof;
  boolean read;
  boolean reversed;
  volatile boolean slurping;
  int nseqreads;
  int totops;
  int64_t totbytes;
  boolean allow_fail;
  volatile boolean invalid;
  size_t orig_size;
  char *pathname;
} lives_file_buffer_t;

lives_file_buffer_t *find_in_file_buffers(int fd);
lives_file_buffer_t *find_in_file_buffers_by_pathname(const char *pathname);

size_t get_read_buff_size(int sztype);

int lives_open_buffered_rdonly(const char *pathname);
int lives_open_buffered_writer(const char *pathname, int mode, boolean append);
int lives_create_buffered(const char *pathname, int mode);
int lives_create_buffered_nosync(const char *pathname, int mode);
ssize_t lives_close_buffered(int fd);
off_t lives_lseek_buffered_writer(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly(int fd, off_t offset);
off_t lives_lseek_buffered_rdonly_absolute(int fd, off_t offset);
off_t lives_buffered_offset(int fd);
size_t lives_buffered_orig_size(int fd);
boolean lives_buffered_rdonly_set_reversed(int fd, boolean val);
ssize_t lives_write_buffered(int fd, const char *buf, ssize_t count, boolean allow_fail);
ssize_t lives_buffered_write_printf(int fd, boolean allow_fail, const char *fmt, ...);
ssize_t lives_write_le_buffered(int fd, livesconstpointer buf, ssize_t count, boolean allow_fail);
ssize_t lives_read_buffered(int fd, void *buf, ssize_t count, boolean allow_less);
ssize_t lives_read_le_buffered(int fd, void *buf, ssize_t count, boolean allow_less);
boolean lives_read_buffered_eof(int fd);
lives_file_buffer_t *get_file_buffer(int fd);
void lives_buffered_rdonly_slurp(int fd, off_t skip);

off_t lives_buffered_flush(int fd);

#endif
