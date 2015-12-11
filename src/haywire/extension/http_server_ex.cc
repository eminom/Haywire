

// Recommended song:
// Civil War by Gun N Rose
// The House of The Rising Sun

extern "C" {

#include "http_server_ex.h"
#include "uv.h"

}

#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define READ_BUFF_LENGTH	4

class _FreeAtEnd
{
public:
	_FreeAtEnd(void *p):ptr(p){}
	~_FreeAtEnd(){free(ptr);}
private:
	void *const ptr;
};

#define _CREATE_SP(lname)\
	int __cb = \
		sizeof(uv_fs_t) * 3 \
		+ sizeof(uv_buf_t) \
		+ sizeof(hw_write_context**)\
		+ sizeof(uv_write_t)\
		+ sizeof(uv_buf_t)\
		+ READ_BUFF_LENGTH\
		+ sizeof(*lname) * (1 + strlen(lname));\
	void *ptr = malloc(__cb);\
	memset(ptr, 0, __cb);

#define _DEFS_C(_PTR)	\
	uv_fs_t *open_req = (uv_fs_t*)(_PTR); \
	uv_fs_t *read_req = (uv_fs_t*)(open_req+1); \
	uv_fs_t *close_req= (uv_fs_t*)(read_req+1); \
	uv_buf_t *read_buf = (uv_buf_t*)(close_req+1); \
	hw_write_context **pwrite_ctx = (hw_write_context**)(read_buf+1); \
	uv_write_t *fs_write_req = (uv_write_t*)(pwrite_ctx+1);\
	uv_buf_t *fs_write_buf = (uv_buf_t*)(fs_write_req+1);\
	char *data_buffer = (char*)(fs_write_buf + 1); \
	char *mstr = (char*)(data_buffer + READ_BUFF_LENGTH);

#define _CLEANUP()	\
	free(open_req);

#define _FINISH_WEB_REQ(WRITE_CTX, WRIT_REQ)\
	_FinishWebReq(WRITE_CTX, WRIT_REQ);

void http_server_after_file_write(uv_write_t *req, int status);

inline void _FinishWebReq(hw_write_context* writ_ctx, uv_write_t *write_req)
{
	if( ! writ_ctx->connection->keep_alive && write_req && write_req->handle)
	{
		//As a matter of fact, the handle below is from context's connection stream.
		// Which is loaded as a new write request is issued.
		uv_close((uv_handle_t*)(write_req->handle), http_stream_on_close);
	}
	if( writ_ctx->callback )
	{
		writ_ctx->callback(writ_ctx->user_data);
	}
	free(writ_ctx);
}

static void on_fs_open(uv_fs_t *req);
static void on_fs_read(uv_fs_t *req);
static void on_fs_close(uv_fs_t *req);
static void on_response_write(uv_write_t *req, int status);

int http_server_write_file(hw_write_context* write_context, hw_string* response_header, const char* filepath)
{
	int length = strlen(filepath) + 1;
	int cb = sizeof(uv_write_t) + sizeof(uv_buf_t) + sizeof(filepath[0]) * length;
	void *ptr = malloc(cb);
	memset(ptr, 0, cb);

	uv_write_t* write_req = (uv_write_t*)ptr;
	uv_buf_t* resbuf = (uv_buf_t*)(write_req+1);
	char *lstr = (char*)(resbuf+1);
	strcpy(lstr, filepath);

	write_req->data = write_context;
	*resbuf = uv_buf_init(response_header->value, response_header->length);

	uv_write(write_req
		, (uv_stream_t*)&(write_context->connection->stream)
		, resbuf
		, 1
		, http_server_after_file_write
	);
	return 0;	//~ Started
}

void http_server_after_file_write(uv_write_t *req, int status)
{
	hw_write_context* write_context = (hw_write_context*)req->data;
	uv_buf_t *resbuf = (uv_buf_t*)(req+1);
	char *lstr = (char*)(resbuf+1);	
	_FreeAtEnd _1(req), _2(resbuf->base);
	if (0 != status)
	{
		_FINISH_WEB_REQ(write_context, req)
	}
	else
	{
		//Go on:(filename and req follows)
		_CREATE_SP(lstr)
		_DEFS_C(ptr)
		*pwrite_ctx = write_context;
		fs_write_req->data = open_req;
		strcpy(mstr, lstr);

		// Mode ??
		uv_fs_open(uv_loop, open_req, mstr, O_RDONLY, 0, on_fs_open);
	}
}

void on_fs_open(uv_fs_t *req) {
	uv_fs_req_cleanup(req);
	void *ptr = req;
	_DEFS_C(ptr)
	if (req->result>=0) {
		fs_write_req->data = ptr;
		*read_buf = uv_buf_init(data_buffer, READ_BUFF_LENGTH);
		uv_fs_read(uv_loop, read_req, req->result, read_buf, 1, -1, on_fs_read);
	} else {
		_FINISH_WEB_REQ(*pwrite_ctx, fs_write_req)
		_CLEANUP()
	}
}

void on_fs_read(uv_fs_t *req) {
	uv_fs_req_cleanup(req);
	void *ptr = req - 1;
	_DEFS_C(ptr)
	if(req->result > 0 ){
		fs_write_buf->len = req->result;
		fs_write_buf->base= read_buf->base;
		uv_write(fs_write_req, (uv_stream_t*)(&(*pwrite_ctx)->connection->stream), fs_write_buf, 1, on_response_write);
	} else {
		//~ Reaching end or reading non.  equals 0 or less than 0
		uv_fs_close(uv_loop, close_req, open_req->result, on_fs_close);
	}
}

void on_response_write(uv_write_t *req, int status) {
	void *ptr = req->data;
	_DEFS_C(ptr);
	*read_buf = uv_buf_init(data_buffer, READ_BUFF_LENGTH);
	//Offset -1. Yes.
	uv_fs_read(uv_loop, read_req, open_req->result, read_buf, 1, -1, on_fs_read);
}

void on_fs_close(uv_fs_t *req) {
	uv_fs_req_cleanup(req);
	void *ptr = req - 2;
	_DEFS_C(ptr);
	_FINISH_WEB_REQ(*pwrite_ctx, fs_write_req)
	_CLEANUP()
}
