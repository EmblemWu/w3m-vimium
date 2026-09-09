/* $Id: istream.h,v 1.12 2003/10/20 16:41:56 ukai Exp $ */
#ifndef IO_STREAM_H
#define IO_STREAM_H

#include "indep.h"
#include <stdio.h>
#include <zlib.h>
#include <brotli/decode.h>
#ifdef USE_SSL
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/ssl.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define STREAM_BUF_SIZE 65536
#define SSL_BUF_SIZE	65536

struct stream_buffer {
    unsigned char *buf;
    int size, cur, next;
};

typedef struct stream_buffer *StreamBuffer;

struct io_file_handle {
    FILE *f;
    void (*close) ();
};

struct basic_conn_handle {
    int fd;
    int scheme;
    char host[128];
    int port;
    int is_reusable;
};

#ifdef USE_SSL
struct ssl_handle {
    SSL *ssl;
    int sock;
    int scheme;
    char host[128];
    int port;
    char *ssl_cert;
    int is_reusable;
};
#endif

union input_stream;

struct ens_handle {
    union input_stream *is;
    struct growbuf gb;
    int pos;
    char encoding;
};

struct zlib_handle {
    union input_stream *is;
    z_stream z;
    unsigned char in_buf[STREAM_BUF_SIZE];
    int in_pos;
    int in_len;
    int eof_in;
    int z_err;
    int initialized;
};

struct brotli_handle {
    union input_stream *is;
    BrotliDecoderState *state;
    unsigned char in_buf[STREAM_BUF_SIZE];
    const uint8_t *next_in;
    size_t avail_in;
    int eof_in;
    int finished;
    int initialized;
};

#define CHUNK_STATE_HEADER        0
#define CHUNK_STATE_DATA          1
#define CHUNK_STATE_TRAILER_CRLF  2
#define CHUNK_STATE_TRAILERS      3
#define CHUNK_STATE_EOS           4
#define CHUNK_STATE_ERROR         5

struct chunked_handle {
    union input_stream *is;
    long long chunk_remaining;
    int state;
    struct growbuf gb;
};


struct base_stream {
    struct stream_buffer stream;
    void *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct file_stream {
    struct stream_buffer stream;
    struct io_file_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct str_stream {
    struct stream_buffer stream;
    Str handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

#ifdef USE_SSL
struct ssl_stream {
    struct stream_buffer stream;
    struct ssl_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};
#endif				/* USE_SSL */

struct encoded_stream {
    struct stream_buffer stream;
    struct ens_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct zlib_stream {
    struct stream_buffer stream;
    struct zlib_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct brotli_stream {
    struct stream_buffer stream;
    struct brotli_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct chunked_stream {
    struct stream_buffer stream;
    struct chunked_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

struct length_handle {
    union input_stream *is;
    clen_t remaining;
};

struct length_stream {
    struct stream_buffer stream;
    struct length_handle *handle;
    char type;
    char iseos;
    int (*read) ();
    void (*close) ();
};

union input_stream {
    struct base_stream base;
    struct file_stream file;
    struct str_stream str;
#ifdef USE_SSL
    struct ssl_stream ssl;
#endif				/* USE_SSL */
    struct encoded_stream ens;
    struct zlib_stream zlib;
    struct brotli_stream brotli;
    struct chunked_stream chunked;
    struct length_stream length;
};

typedef struct base_stream *BaseStream;
typedef struct file_stream *FileStream;
typedef struct str_stream *StrStream;
#ifdef USE_SSL
typedef struct ssl_stream *SSLStream;
#endif				/* USE_SSL */
typedef struct encoded_stream *EncodedStrStream;
typedef struct zlib_stream *ZlibStrStream;
typedef struct brotli_stream *BrotliStrStream;
typedef struct chunked_stream *ChunkedStrStream;
typedef struct length_stream *LengthStrStream;

typedef union input_stream *InputStream;

extern InputStream newInputStream(int des);
extern InputStream newInputStreamWithConn(int des, int scheme, const char *host, int port);
extern InputStream newFileStream(FILE * f, void (*closep) ());
extern InputStream newStrStream(Str s);
#ifdef USE_SSL
extern InputStream newSSLStream(SSL * ssl, int sock);
extern InputStream newSSLStreamWithConn(SSL * ssl, int sock, int scheme, const char *host, int port, const char *cert);
#endif
extern void checkin_http_connection(int scheme, const char *host, int port, int sock, void *ssl, const char *cert, int keep_alive);
extern InputStream newEncodedStream(InputStream is, char encoding);
extern InputStream newZlibStream(InputStream is, int is_gzip);
extern InputStream newBrotliStream(InputStream is);
extern InputStream newChunkedStream(InputStream is);
extern InputStream newLengthStream(InputStream is, clen_t content_length);
extern int ISclose(InputStream stream);
extern int ISgetc(InputStream stream);
extern int ISundogetc(InputStream stream);
extern Str StrISgets2(InputStream stream, char crnl);
#define StrISgets(stream) StrISgets2(stream, FALSE)
#define StrmyISgets(stream) StrISgets2(stream, TRUE)
void ISgets_to_growbuf(InputStream stream, struct growbuf *gb, char crnl);
#ifdef unused
extern int ISread(InputStream stream, Str buf, int count);
#endif
int ISread_n(InputStream stream, char *dst, int bufsize);
extern int ISfileno(InputStream stream);
extern void ISset_reusable(InputStream stream, int reusable);
extern int ISeos(InputStream stream);
#ifdef USE_SSL
extern void ssl_accept_this_site(char *hostname);
extern Str ssl_get_certificate(SSL * ssl, char *hostname);
#endif

#define IST_BASIC	0
#define IST_FILE	1
#define IST_STR		2
#define IST_SSL		3
#define IST_ENCODED	4
#define IST_ZLIB	5
#define IST_CHUNKED	6
#define IST_BROTLI	7
#define IST_LENGTH	8
#define IST_UNCLOSE	0x10

#define IStype(stream) ((stream)->base.type)
#define is_eos(stream) ISeos(stream)
#define iseos(stream) ((stream)->base.iseos)
#define file_of(stream) ((stream)->file.handle->f)
#define set_close(stream,closep) ((IStype(stream)==IST_FILE)?((stream)->file.handle->close=(closep)):0)
#define str_of(stream) ((stream)->str.handle)
#ifdef USE_SSL
#define ssl_socket_of(stream) ((stream)->ssl.handle->sock)
#define ssl_of(stream) ((stream)->ssl.handle->ssl)
#endif

#ifdef USE_BINMODE_STREAM
#define openIS(path) newInputStream(open((path),O_RDONLY|O_BINARY))
#else
#define openIS(path) newInputStream(open((path),O_RDONLY))
#endif				/* USE_BINMODE_STREAM */
#endif
