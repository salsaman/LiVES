// htmsocket.h
// (c) G. Finch 2019 - 2023 <salsaman+lives@gmail.com>
// released under the GNU GPL 3 or later
// see file ../COPYING for licensing details

void *OpenHTMSocket(const char *host, int portnumber, boolean sender);

boolean lives_stream_out(void *htmsendhandle, size_t length_in_bytes, void *buffer);

ssize_t lives_stream_in(void *htmrecvhandle, size_t length, void *buffer, int bfsize);

void CloseHTMSocket(void *htmsendhandle);

