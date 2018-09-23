/*htm sockets*/

void *OpenHTMSocket(const char *host, int portnumber, boolean sender);

boolean lives_stream_out(void *htmsendhandle, size_t length_in_bytes, void *buffer);

ssize_t lives_stream_in(void *htmrecvhandle, size_t length, void *buffer, int bfsize);

void CloseHTMSocket(void *htmsendhandle);

