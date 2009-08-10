/*htm sockets*/

void *OpenHTMSocket(char *host, int portnumber, gboolean sender);

gboolean lives_stream_out(void *htmsendhandle, size_t length_in_bytes, void *buffer);

ssize_t lives_stream_in(void *htmrecvhandle, size_t length, void *buffer, gboolean block);

void CloseHTMSocket(void *htmsendhandle);

