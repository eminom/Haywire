

#include "http_server.h"

//This the impls.
void (*http_stream_on_read)(uv_stream_t*, ssize_t, const uv_buf_t*);
int (*http_server_write_response)(hw_write_context*, hw_string*);
