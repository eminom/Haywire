

#ifndef _HTTP_SERVER_EXTENSION_DEF__
#define _HTTP_SERVER_EXTENSION_DEF__

#include "http_server.h"

#ifdef __cplusplus
extern "C"{
#endif
		

int http_server_write_file(hw_write_context* write_context, hw_string* response_header, const char* filepath);

#ifdef __cplusplus
}
#endif

#endif