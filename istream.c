/* $Id: istream.c,v 1.27 2010/07/18 13:43:23 htrb Exp $ */
#include "fm.h"
#include "myctype.h"
#include "istream.h"
#include <signal.h>
#ifdef USE_SSL
#include <openssl/x509v3.h>
#endif
#ifdef __MINGW32_VERSION
#include <winsock.h>
#endif

#define	uchar		unsigned char

#define STREAM_BUF_SIZE 65536
#define SSL_BUF_SIZE	32768

#define MUST_BE_UPDATED(bs) ((bs)->stream.cur==(bs)->stream.next)

#define POP_CHAR(bs) ((bs)->iseos?'\0':(bs)->stream.buf[(bs)->stream.cur++])

static void basic_close(struct basic_conn_handle *handle);
static int basic_read(struct basic_conn_handle *handle, char *buf, int len);

static void file_close(struct io_file_handle *handle);
static int file_read(struct io_file_handle *handle, char *buf, int len);

static int str_read(Str handle, char *buf, int len);

#ifdef USE_SSL
static void ssl_close(struct ssl_handle *handle);
static int ssl_read(struct ssl_handle *handle, char *buf, int len);
#endif

static int ens_read(struct ens_handle *handle, char *buf, int len);
static void ens_close(struct ens_handle *handle);

static int zlib_stream_read(struct zlib_handle *handle, char *buf, int len);
static void zlib_stream_close(struct zlib_handle *handle);

static int brotli_stream_read(struct brotli_handle *handle, char *buf, int len);
static void brotli_stream_close(struct brotli_handle *handle);

static int chunked_stream_read(struct chunked_handle *handle, char *buf, int len);
static void chunked_stream_close(struct chunked_handle *handle);

static void memchop(char *p, int *len);

static void
do_update(BaseStream base)
{
    int len;
    base->stream.cur = base->stream.next = 0;
    len = (*base->read) (base->handle, base->stream.buf, base->stream.size);
    if (len <= 0)
	base->iseos = TRUE;
    else
	base->stream.next += len;
}

static int
buffer_read(StreamBuffer sb, char *obuf, int count)
{
    int len = sb->next - sb->cur;
    if (len > 0) {
	if (len > count)
	    len = count;
	bcopy((const void *)&sb->buf[sb->cur], obuf, len);
	sb->cur += len;
    }
    return len;
}

static void
init_buffer(BaseStream base, char *buf, int bufsize)
{
    StreamBuffer sb = &base->stream;
    sb->size = bufsize;
    sb->cur = 0;
    sb->buf = NewWithoutGC_N(uchar, bufsize);
    if (buf) {
	memcpy(sb->buf, buf, bufsize);
	sb->next = bufsize;
    }
    else {
	sb->next = 0;
    }
    base->iseos = FALSE;
}

static void
init_base_stream(BaseStream base, int bufsize)
{
    init_buffer(base, NULL, bufsize);
}

static void
init_str_stream(BaseStream base, Str s)
{
    init_buffer(base, s->ptr, s->length);
}

InputStream
newInputStream(int des)
{
    return newInputStreamWithConn(des, 0, NULL, 0);
}

InputStream
newInputStreamWithConn(int des, int scheme, const char *host, int port)
{
    InputStream stream;
    struct basic_conn_handle *bh;
    if (des < 0)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->base.type = IST_BASIC;
    bh = NewWithoutGC(struct basic_conn_handle);
    bh->fd = des;
    bh->scheme = scheme;
    bh->port = port;
    bh->is_reusable = 1;
    if (host) {
	strncpy(bh->host, host, sizeof(bh->host) - 1);
	bh->host[sizeof(bh->host) - 1] = '\0';
    }
    else {
	bh->host[0] = '\0';
    }
    stream->base.handle = bh;
    stream->base.read = (int (*)())basic_read;
    stream->base.close = (void (*)())basic_close;
    return stream;
}

InputStream
newFileStream(FILE * f, void (*closep) ())
{
    InputStream stream;
    if (f == NULL)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->file.type = IST_FILE;
    stream->file.handle = NewWithoutGC(struct io_file_handle);
    stream->file.handle->f = f;
    if (closep)
	stream->file.handle->close = closep;
    else
	stream->file.handle->close = (void (*)())fclose;
    stream->file.read = (int (*)())file_read;
    stream->file.close = (void (*)())file_close;
    return stream;
}

InputStream
newStrStream(Str s)
{
    InputStream stream;
    if (s == NULL)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_str_stream(&stream->base, s);
    stream->str.type = IST_STR;
    stream->str.handle = NULL;
    stream->str.read = (int (*)())str_read;
    stream->str.close = NULL;
    return stream;
}

#ifdef USE_SSL
InputStream
newSSLStream(SSL * ssl, int sock)
{
    return newSSLStreamWithConn(ssl, sock, 0, NULL, 0, NULL);
}

InputStream
newSSLStreamWithConn(SSL * ssl, int sock, int scheme, const char *host, int port, const char *cert)
{
    InputStream stream;
    struct ssl_handle *sh;
    if (sock < 0)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, SSL_BUF_SIZE);
    stream->ssl.type = IST_SSL;
    sh = NewWithoutGC(struct ssl_handle);
    sh->ssl = ssl;
    sh->sock = sock;
    sh->scheme = scheme;
    sh->port = port;
    sh->is_reusable = 1;
    if (cert)
	sh->ssl_cert = allocStr((char *)cert, -1);
    else
	sh->ssl_cert = NULL;
    if (host) {
	strncpy(sh->host, host, sizeof(sh->host) - 1);
	sh->host[sizeof(sh->host) - 1] = '\0';
    }
    else {
	sh->host[0] = '\0';
    }
    stream->ssl.handle = sh;
    stream->ssl.read = (int (*)())ssl_read;
    stream->ssl.close = (void (*)())ssl_close;
    return stream;
}
#endif

InputStream
newEncodedStream(InputStream is, char encoding)
{
    InputStream stream;
    if (is == NULL || (encoding != ENC_QUOTE && encoding != ENC_BASE64 &&
		       encoding != ENC_UUENCODE))
	return is;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->ens.type = IST_ENCODED;
    stream->ens.handle = NewWithoutGC(struct ens_handle);
    stream->ens.handle->is = is;
    stream->ens.handle->pos = 0;
    stream->ens.handle->encoding = encoding;
    growbuf_init_without_GC(&stream->ens.handle->gb);
    stream->ens.read = (int (*)())ens_read;
    stream->ens.close = (void (*)())ens_close;
    return stream;
}

InputStream
newZlibStream(InputStream is, int is_gzip)
{
    InputStream stream;
    int ret;
    int windowBits;

    if (is == NULL)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->zlib.type = IST_ZLIB;
    stream->zlib.handle = NewWithoutGC(struct zlib_handle);
    memset(stream->zlib.handle, 0, sizeof(struct zlib_handle));
    stream->zlib.handle->is = is;

    /* windowBits: 15 for deflate/zlib, 15 + 16 (31) for gzip, 15 + 32 (47) for automatic gzip/zlib header detection */
    windowBits = 15 + 32;

    ret = inflateInit2(&stream->zlib.handle->z, windowBits);
    if (ret != Z_OK) {
	/* Fallback to standard inflateInit if inflateInit2 fails */
	ret = inflateInit(&stream->zlib.handle->z);
    }
    if (ret != Z_OK) {
	xfree(stream->zlib.handle);
	xfree(stream->base.stream.buf);
	xfree(stream);
	return is;
    }

    stream->zlib.handle->initialized = 1;
    stream->zlib.read = (int (*)())zlib_stream_read;
    stream->zlib.close = (void (*)())zlib_stream_close;
    return stream;
}

InputStream
newBrotliStream(InputStream is)
{
    InputStream stream;
    BrotliDecoderState *state;

    if (is == NULL)
	return NULL;

    state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
    if (!state)
	return is;

    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->brotli.type = IST_BROTLI;
    stream->brotli.handle = NewWithoutGC(struct brotli_handle);
    memset(stream->brotli.handle, 0, sizeof(struct brotli_handle));
    stream->brotli.handle->is = is;
    stream->brotli.handle->state = state;
    stream->brotli.handle->next_in = stream->brotli.handle->in_buf;
    stream->brotli.handle->avail_in = 0;
    stream->brotli.handle->eof_in = 0;
    stream->brotli.handle->finished = 0;
    stream->brotli.handle->initialized = 1;

    stream->brotli.read = (int (*)())brotli_stream_read;
    stream->brotli.close = (void (*)())brotli_stream_close;
    return stream;
}

InputStream
newChunkedStream(InputStream is)
{
    InputStream stream;

    if (is == NULL)
	return NULL;
    stream = NewWithoutGC(union input_stream);
    init_base_stream(&stream->base, STREAM_BUF_SIZE);
    stream->chunked.type = IST_CHUNKED;
    stream->chunked.handle = NewWithoutGC(struct chunked_handle);
    memset(stream->chunked.handle, 0, sizeof(struct chunked_handle));
    stream->chunked.handle->is = is;
    stream->chunked.handle->state = CHUNK_STATE_HEADER;
    stream->chunked.handle->chunk_remaining = 0;
    stream->chunked.read = (int (*)())chunked_stream_read;
    stream->chunked.close = (void (*)())chunked_stream_close;
    return stream;
}

int
ISclose(InputStream stream)
{
    MySignalHandler(*prevtrap) ();
    if (stream == NULL)
        return -1;
    if (stream->base.close != NULL) {
        if (stream->base.type & IST_UNCLOSE) {
            return -1;
        }
        prevtrap = mySignal(SIGINT, SIG_IGN);
        stream->base.close (stream->base.handle);
        mySignal(SIGINT, prevtrap);
    }
    xfree(stream->base.stream.buf);
    xfree(stream);
    return 0;
}

int
ISgetc(InputStream stream)
{
    BaseStream base;
    if (stream == NULL)
	return '\0';
    base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
	do_update(base);
    return POP_CHAR(base);
}

int
ISundogetc(InputStream stream)
{
    StreamBuffer sb;
    if (stream == NULL)
	return -1;
    sb = &stream->base.stream;
    if (sb->cur > 0) {
	sb->cur--;
	return 0;
    }
    return -1;
}

Str
StrISgets2(InputStream stream, char crnl)
{
    struct growbuf gb;

    if (stream == NULL)
	return NULL;
    growbuf_init(&gb);
    ISgets_to_growbuf(stream, &gb, crnl);
    return growbuf_to_Str(&gb);
}

void
ISgets_to_growbuf(InputStream stream, struct growbuf *gb, char crnl)
{
    BaseStream base = &stream->base;
    StreamBuffer sb = &base->stream;
    int i;

    gb->length = 0;

    while (!base->iseos) {
	if (MUST_BE_UPDATED(base)) {
	    do_update(base);
	    continue;
	}
	if (crnl && gb->length > 0  && gb->ptr[gb->length - 1] == '\r') {
	    if (sb->buf[sb->cur] == '\n') {
		GROWBUF_ADD_CHAR(gb, '\n');
		++sb->cur;
	    }
	    break;
	}
	for (i = sb->cur; i < sb->next; ++i) {
	    if (sb->buf[i] == '\n' || (crnl && sb->buf[i] == '\r')) {
		++i;
		break;
	    }
	}
	growbuf_append(gb, &sb->buf[sb->cur], i - sb->cur);
	sb->cur = i;
	if (gb->length > 0 && gb->ptr[gb->length - 1] == '\n')
	    break;
    }

    growbuf_reserve(gb, gb->length + 1);
    gb->ptr[gb->length] = '\0';
    return;
}

#ifdef unused
int
ISread(InputStream stream, Str buf, int count)
{
    int len;

    if (count + 1 > buf->area_size) {
	char *newptr = GC_MALLOC_ATOMIC(count + 1);
	memcpy(newptr, buf->ptr, buf->length);
	newptr[buf->length] = '\0';
	buf->ptr = newptr;
	buf->area_size = count + 1;
    }
    len = ISread_n(stream, buf->ptr, count);
    buf->length = (len > 0) ? len : 0;
    buf->ptr[buf->length] = '\0';
    return (len > 0) ? 1 : 0;
}
#endif

int
ISread_n(InputStream stream, char *dst, int count)
{
    int len, l;
    BaseStream base;

    if (stream == NULL || count <= 0)
	return -1;
    if ((base = &stream->base)->iseos)
	return 0;

    len = buffer_read(&base->stream, dst, count);
    if (MUST_BE_UPDATED(base)) {
	l = (*base->read) (base->handle, &dst[len], count - len);
	if (l <= 0) {
	    base->iseos = TRUE;
	} else {
	    len += l;
	}
    }
    return len;
}

int
ISfileno(InputStream stream)
{
    if (stream == NULL)
	return -1;
    switch (IStype(stream) & ~IST_UNCLOSE) {
    case IST_BASIC:
	return ((struct basic_conn_handle *)stream->base.handle)->fd;
    case IST_FILE:
	return fileno(stream->file.handle->f);
#ifdef USE_SSL
    case IST_SSL:
	return stream->ssl.handle->sock;
#endif
    case IST_ENCODED:
	return ISfileno(stream->ens.handle->is);
    case IST_ZLIB:
	return ISfileno(stream->zlib.handle->is);
    case IST_BROTLI:
	return ISfileno(stream->brotli.handle->is);
    case IST_CHUNKED:
	return ISfileno(stream->chunked.handle->is);
    default:
	return -1;
    }
}

int
ISeos(InputStream stream)
{
    BaseStream base = &stream->base;
    if (!base->iseos && MUST_BE_UPDATED(base))
	do_update(base);
    return base->iseos;
}

#ifdef USE_SSL
static Str accept_this_site;

void
ssl_accept_this_site(char *hostname)
{
    if (hostname)
	accept_this_site = Strnew_charp(hostname);
    else
	accept_this_site = NULL;
}

static int
ssl_match_cert_ident(char *ident, int ilen, char *hostname)
{
    /* RFC2818 3.1.  Server Identity
     * Names may contain the wildcard
     * character * which is considered to match any single domain name
     * component or component fragment. E.g., *.a.com matches foo.a.com but
     * not bar.foo.a.com. f*.com matches foo.com but not bar.com.
     */
    int hlen = strlen(hostname);
    int i, c;

    /* Is this an exact match? */
    if ((ilen == hlen) && strncasecmp(ident, hostname, hlen) == 0)
	return TRUE;

    for (i = 0; i < ilen; i++) {
	if (ident[i] == '*' && ident[i + 1] == '.') {
	    while ((c = *hostname++) != '\0')
		if (c == '.')
		    break;
	    i++;
	}
	else {
	    if (ident[i] != *hostname++)
		return FALSE;
	}
    }
    return *hostname == '\0';
}

static Str
ssl_check_cert_ident(X509 * x, char *hostname)
{
    int i;
    Str ret = NULL;
    int match_ident = FALSE;
    /*
     * All we need to do here is check that the CN matches.
     *
     * From RFC2818 3.1 Server Identity:
     * If a subjectAltName extension of type dNSName is present, that MUST
     * be used as the identity. Otherwise, the (most specific) Common Name
     * field in the Subject field of the certificate MUST be used. Although
     * the use of the Common Name is existing practice, it is deprecated and
     * Certification Authorities are encouraged to use the dNSName instead.
     */
    i = X509_get_ext_by_NID(x, NID_subject_alt_name, -1);
    if (i >= 0) {
	X509_EXTENSION *ex;
	STACK_OF(GENERAL_NAME) * alt;

	ex = X509_get_ext(x, i);
	alt = X509V3_EXT_d2i(ex);
	if (alt) {
	    int n;
	    GENERAL_NAME *gn;
	    Str seen_dnsname = NULL;

	    n = sk_GENERAL_NAME_num(alt);
	    for (i = 0; i < n; i++) {
		gn = sk_GENERAL_NAME_value(alt, i);
		if (gn->type == GEN_DNS) {
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		    unsigned char *sn = ASN1_STRING_data(gn->d.ia5);
#else
		    const unsigned char *sn = ASN1_STRING_get0_data(gn->d.ia5);
#endif
		    int sl = ASN1_STRING_length(gn->d.ia5);

		    /*
		     * sn is a pointer to internal data and not guaranteed to
		     * be null terminated. Ensure we have a null terminated
		     * string that we can modify.
		     */
		    char *asn = GC_MALLOC(sl + 1);
		    if (!asn)
			exit(1);
		    bcopy(sn, asn, sl);
		    asn[sl] = '\0';

		    if (!seen_dnsname)
			seen_dnsname = Strnew();
		    /* replace \0 to make full string visible to user */
		    if (sl != strlen(asn)) {
			int i;
			for (i = 0; i < sl; ++i) {
			    if (!asn[i])
				asn[i] = '!';
			}
		    }
		    Strcat_m_charp(seen_dnsname, asn, " ", NULL);
		    if (sl == strlen(asn) /* catch \0 in SAN */
			&& ssl_match_cert_ident(asn, sl, hostname))
			break;
		}
	    }
	    X509V3_EXT_get(ex);
	    sk_GENERAL_NAME_free(alt);
	    if (i < n)		/* Found a match */
		match_ident = TRUE;
	    else if (seen_dnsname)
		/* FIXME: gettextize? */
		ret = Sprintf("Bad cert ident from %s: dNSName=%s", hostname,
			      seen_dnsname->ptr);
	}
    }

    if (match_ident == FALSE && ret == NULL) {
	X509_NAME *xn;
	char buf[2048];
	int slen;

	xn = X509_get_subject_name(x);

	slen = X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf));
	if ( slen == -1)
	    /* FIXME: gettextize? */
	    ret = Strnew_charp("Unable to get common name from peer cert");
	else if (slen != strlen(buf)
		|| !ssl_match_cert_ident(buf, strlen(buf), hostname)) {
	    /* replace \0 to make full string visible to user */
	    if (slen != strlen(buf)) {
		int i;
		for (i = 0; i < slen; ++i) {
		    if (!buf[i])
			buf[i] = '!';
		}
	    }
	    /* FIXME: gettextize? */
	    ret = Sprintf("Bad cert ident %s from %s", buf, hostname);
	}
    }
    return ret;
}

Str
ssl_get_certificate(SSL * ssl, char *hostname)
{
    BIO *bp;
    X509 *x;
    X509_NAME *xn;
    char *p;
    int len;
    Str s;
    char buf[2048];
    Str amsg = NULL;
    Str emsg;
    char *ans;

    if (ssl == NULL)
	return NULL;
    x = SSL_get_peer_certificate(ssl);
    if (x == NULL) {
	if (accept_this_site
	    && strcasecmp(accept_this_site->ptr, hostname) == 0)
	    ans = "y";
	else {
	    /* FIXME: gettextize? */
	    emsg = Strnew_charp("No SSL peer certificate: accept? (y/n)");
	    ans = inputAnswer(emsg->ptr);
	}
	if (ans && TOLOWER(*ans) == 'y')
	    /* FIXME: gettextize? */
	    amsg = Strnew_charp
		("Accept SSL session without any peer certificate");
	else {
	    /* FIXME: gettextize? */
	    char *e = "This SSL session was rejected "
		"to prevent security violation: no peer certificate";
	    disp_err_message(e, FALSE);
	    free_ssl_ctx();
	    return NULL;
	}
	if (amsg)
	    disp_err_message(amsg->ptr, FALSE);
	ssl_accept_this_site(hostname);
	/* FIXME: gettextize? */
	s = amsg ? amsg : Strnew_charp("valid certificate");
	return s;
    }
#ifdef USE_SSL_VERIFY
    /* check the cert chain.
     * The chain length is automatically checked by OpenSSL when we
     * set the verify depth in the ctx.
     */
    if (ssl_verify_server) {
	long verr;
	if ((verr = SSL_get_verify_result(ssl))
	    != X509_V_OK) {
	    const char *em = X509_verify_cert_error_string(verr);
	    if (accept_this_site
		&& strcasecmp(accept_this_site->ptr, hostname) == 0)
		ans = "y";
	    else {
		/* FIXME: gettextize? */
		emsg = Sprintf("%s: accept? (y/n)", em);
		ans = inputAnswer(emsg->ptr);
	    }
	    if (ans && TOLOWER(*ans) == 'y') {
		/* FIXME: gettextize? */
		amsg = Sprintf("Accept unsecure SSL session: "
			       "unverified: %s", em);
	    }
	    else {
		/* FIXME: gettextize? */
		char *e =
		    Sprintf("This SSL session was rejected: %s", em)->ptr;
		disp_err_message(e, FALSE);
		free_ssl_ctx();
		return NULL;
	    }
	}
    }
#endif
    emsg = ssl_check_cert_ident(x, hostname);
    if (emsg != NULL) {
	if (accept_this_site
	    && strcasecmp(accept_this_site->ptr, hostname) == 0)
	    ans = "y";
	else {
	    Str ep = Strdup(emsg);
	    if (ep->length > COLS - 16)
		Strshrink(ep, ep->length - (COLS - 16));
	    Strcat_charp(ep, ": accept? (y/n)");
	    ans = inputAnswer(ep->ptr);
	}
	if (ans && TOLOWER(*ans) == 'y') {
	    /* FIXME: gettextize? */
	    amsg = Strnew_charp("Accept unsecure SSL session:");
	    Strcat(amsg, emsg);
	}
	else {
	    /* FIXME: gettextize? */
	    char *e = "This SSL session was rejected "
		"to prevent security violation";
	    disp_err_message(e, FALSE);
	    free_ssl_ctx();
	    return NULL;
	}
    }
    if (amsg)
	disp_err_message(amsg->ptr, FALSE);
    ssl_accept_this_site(hostname);
    /* FIXME: gettextize? */
    s = amsg ? amsg : Strnew_charp("valid certificate");
    Strcat_charp(s, "\n");
    xn = X509_get_subject_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
	Strcat_charp(s, " subject=<unknown>");
    else
	Strcat_m_charp(s, " subject=", buf, NULL);
    xn = X509_get_issuer_name(x);
    if (X509_NAME_get_text_by_NID(xn, NID_commonName, buf, sizeof(buf)) == -1)
	Strcat_charp(s, ": issuer=<unknown>");
    else
	Strcat_m_charp(s, ": issuer=", buf, NULL);
    Strcat_charp(s, "\n\n");

    bp = BIO_new(BIO_s_mem());
    X509_print(bp, x);
    len = (int)BIO_ctrl(bp, BIO_CTRL_INFO, 0, (char *)&p);
    Strcat_charp_n(s, p, len);
    BIO_free_all(bp);
    X509_free(x);
    return s;
}
#endif

/* Raw level input stream functions */

static void
basic_close(struct basic_conn_handle *handle)
{
    if (handle) {
	if (handle->host[0] != '\0') {
	    checkin_http_connection(handle->scheme, handle->host, handle->port,
				    handle->fd, NULL, NULL, handle->is_reusable);
	}
	else {
#ifdef __MINGW32_VERSION
	    closesocket(handle->fd);
#else
	    close(handle->fd);
#endif
	}
	xfree(handle);
    }
}

static int
basic_read(struct basic_conn_handle *handle, char *buf, int len)
{
#ifdef __MINGW32_VERSION
    return recv(handle->fd, buf, len, 0);
#else
    return read(handle->fd, buf, len);
#endif
}

static void
file_close(struct io_file_handle *handle)
{
    handle->close(handle->f);
    xfree(handle);
}

static int
file_read(struct io_file_handle *handle, char *buf, int len)
{
    return fread(buf, 1, len, handle->f);
}

static int
str_read(Str handle, char *buf, int len)
{
    return 0;
}

#ifdef USE_SSL
static void
ssl_close(struct ssl_handle *handle)
{
    if (handle) {
	if (handle->host[0] != '\0') {
	    checkin_http_connection(handle->scheme, handle->host, handle->port,
				    handle->sock, handle->ssl, handle->ssl_cert,
				    handle->is_reusable);
	}
	else {
	    close(handle->sock);
	    if (handle->ssl)
		SSL_free(handle->ssl);
	}
	xfree(handle);
    }
}

static int
ssl_read(struct ssl_handle *handle, char *buf, int len)
{
    int status;
    if (handle->ssl) {
#ifdef USE_SSL_VERIFY
	for (;;) {
	    status = SSL_read(handle->ssl, buf, len);
	    if (status > 0)
		break;
	    switch (SSL_get_error(handle->ssl, status)) {
	    case SSL_ERROR_WANT_READ:
	    case SSL_ERROR_WANT_WRITE:	/* reads can trigger write errors; see SSL_get_error(3) */
		continue;
	    default:
		break;
	    }
	    break;
	}
#else				/* if !defined(USE_SSL_VERIFY) */
	status = SSL_read(handle->ssl, buf, len);
#endif				/* !defined(USE_SSL_VERIFY) */
    }
    else
	status = read(handle->sock, buf, len);
    return status;
}
#endif				/* USE_SSL */

static void
ens_close(struct ens_handle *handle)
{
    ISclose(handle->is);
    growbuf_clear(&handle->gb);
    xfree(handle);
}

static int
ens_read(struct ens_handle *handle, char *buf, int len)
{
    if (handle->pos == handle->gb.length) {
	char *p;
	struct growbuf gbtmp;

	ISgets_to_growbuf(handle->is, &handle->gb, TRUE);
	if (handle->gb.length == 0)
	    return 0;
	if (handle->encoding == ENC_BASE64)
	    memchop(handle->gb.ptr, &handle->gb.length);
	else if (handle->encoding == ENC_UUENCODE) {
	    if (handle->gb.length >= 5 &&
		!strncmp(handle->gb.ptr, "begin", 5))
		ISgets_to_growbuf(handle->is, &handle->gb, TRUE);
	    memchop(handle->gb.ptr, &handle->gb.length);
	}
	growbuf_init_without_GC(&gbtmp);
	p = handle->gb.ptr;
	if (handle->encoding == ENC_QUOTE)
	    decodeQP_to_growbuf(&gbtmp, &p);
	else if (handle->encoding == ENC_BASE64)
	    decodeB_to_growbuf(&gbtmp, &p);
	else if (handle->encoding == ENC_UUENCODE)
	    decodeU_to_growbuf(&gbtmp, &p);
	growbuf_clear(&handle->gb);
	handle->gb = gbtmp;
	handle->pos = 0;
    }

    if (len > handle->gb.length - handle->pos)
	len = handle->gb.length - handle->pos;

    memcpy(buf, &handle->gb.ptr[handle->pos], len);
    handle->pos += len;
    return len;
}

static void
memchop(char *p, int *len)
{
    char *q;

    for (q = p + *len; q > p; --q) {
	if (q[-1] != '\n' && q[-1] != '\r')
	    break;
    }
    if (q != p + *len)
	*q = '\0';
    *len = q - p;
    return;
}

static void
zlib_stream_close(struct zlib_handle *handle)
{
    if (handle) {
	if (handle->initialized) {
	    inflateEnd(&handle->z);
	    handle->initialized = 0;
	}
	if (handle->is) {
	    ISclose(handle->is);
	    handle->is = NULL;
	}
	xfree(handle);
    }
}

static int
zlib_stream_read(struct zlib_handle *handle, char *buf, int len)
{
    int ret;

    if (!handle || !handle->initialized || len <= 0)
	return 0;

    if (handle->z_err == Z_STREAM_END && handle->z.avail_in == 0)
	return 0;

    handle->z.next_out = (Bytef *)buf;
    handle->z.avail_out = (uInt)len;

    while (handle->z.avail_out > 0) {
	if (handle->z.avail_in == 0 && !handle->eof_in) {
	    int nread = ISread_n(handle->is, (char *)handle->in_buf, sizeof(handle->in_buf));
	    if (nread > 0) {
		handle->z.next_in = handle->in_buf;
		handle->z.avail_in = (uInt)nread;
	    } else {
		handle->eof_in = 1;
		handle->z.next_in = Z_NULL;
		handle->z.avail_in = 0;
	    }
	}

	if (handle->z.avail_in == 0 && handle->eof_in) {
	    /* No more input data */
	    break;
	}

	ret = inflate(&handle->z, Z_NO_FLUSH);
	if (ret == Z_STREAM_END) {
	    handle->z_err = Z_STREAM_END;
	    break;
	} else if (ret == Z_OK) {
	    /* Got some decompressed output or consumed input */
	    if (handle->z.avail_out < (uInt)len) {
		/* Made progress */
		break;
	    }
	} else if (ret == Z_BUF_ERROR) {
	    /* Need more input */
	    if (handle->eof_in)
		break;
	} else {
	    /* Decompression error */
	    handle->z_err = ret;
	    break;
	}
    }

    return (int)(len - handle->z.avail_out);
}

static void
brotli_stream_close(struct brotli_handle *handle)
{
    if (handle) {
	if (handle->state) {
	    BrotliDecoderDestroyInstance(handle->state);
	    handle->state = NULL;
	}
	if (handle->is) {
	    ISclose(handle->is);
	    handle->is = NULL;
	}
	xfree(handle);
    }
}

static int
brotli_stream_read(struct brotli_handle *handle, char *buf, int len)
{
    size_t avail_out;
    uint8_t *next_out;
    BrotliDecoderResult res;

    if (!handle || !handle->initialized || !handle->state || len <= 0)
	return 0;

    if (handle->finished)
	return 0;

    next_out = (uint8_t *)buf;
    avail_out = (size_t)len;

    while (avail_out > 0) {
	if (handle->avail_in == 0 && !handle->eof_in) {
	    int nread = ISread_n(handle->is, (char *)handle->in_buf, sizeof(handle->in_buf));
	    if (nread > 0) {
		handle->next_in = handle->in_buf;
		handle->avail_in = (size_t)nread;
	    } else {
		handle->eof_in = 1;
		handle->next_in = NULL;
		handle->avail_in = 0;
	    }
	}

	res = BrotliDecoderDecompressStream(
	    handle->state,
	    &handle->avail_in,
	    &handle->next_in,
	    &avail_out,
	    &next_out,
	    NULL
	);

	if (res == BROTLI_DECODER_RESULT_SUCCESS) {
	    handle->finished = 1;
	    break;
	} else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT) {
	    /* Output buffer full, return what we have so far */
	    break;
	} else if (res == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
	    if (handle->eof_in) {
		/* Stream ended prematurely */
		handle->finished = 1;
		break;
	    }
	    /* Loop back to read more from underlying stream */
	    continue;
	} else {
	    /* BROTLI_DECODER_RESULT_ERROR */
	    handle->finished = 1;
	    break;
	}
    }

    return (int)(len - (int)avail_out);
}

static void
chunked_stream_close(struct chunked_handle *handle)
{
    if (handle) {
	if (handle->is) {
	    ISclose(handle->is);
	    handle->is = NULL;
	}
	xfree(handle);
    }
}

static int
chunked_stream_read(struct chunked_handle *handle, char *buf, int len)
{
    int total_read = 0;
    struct growbuf gb;

    if (!handle || !handle->is || len <= 0)
	return 0;

    growbuf_init_without_GC(&gb);

    while (total_read < len && handle->state != CHUNK_STATE_EOS) {
	switch (handle->state) {
	case CHUNK_STATE_HEADER: {
	    ISgets_to_growbuf(handle->is, &gb, TRUE);
	    if (gb.length == 0) {
		/* Unexpected EOF or connection closed */
		handle->state = CHUNK_STATE_EOS;
		break;
	    }
	    char *p = gb.ptr;
	    while (*p == ' ' || *p == '\t')
		p++;
	    if (*p == '\r' || *p == '\n' || *p == '\0') {
		/* Blank line before chunk header, skip and read next line */
		break;
	    }
	    char *endptr = NULL;
	    unsigned long long chunk_sz = strtoull(p, &endptr, 16);
	    if (endptr == p) {
		/* Invalid chunk header */
		handle->state = CHUNK_STATE_EOS;
		break;
	    }
	    if (chunk_sz == 0) {
		handle->state = CHUNK_STATE_TRAILERS;
	    } else {
		handle->chunk_remaining = (long long)chunk_sz;
		handle->state = CHUNK_STATE_DATA;
	    }
	    break;
	}
	case CHUNK_STATE_DATA: {
	    int to_read = len - total_read;
	    if ((long long)to_read > handle->chunk_remaining)
		to_read = (int)handle->chunk_remaining;
	    int nread = ISread_n(handle->is, buf + total_read, to_read);
	    if (nread <= 0) {
		handle->state = CHUNK_STATE_EOS;
		break;
	    }
	    total_read += nread;
	    handle->chunk_remaining -= nread;
	    if (handle->chunk_remaining == 0) {
		handle->state = CHUNK_STATE_TRAILER_CRLF;
	    }
	    break;
	}
	case CHUNK_STATE_TRAILER_CRLF: {
	    /* Consume the \r\n immediately following the chunk data */
	    ISgets_to_growbuf(handle->is, &gb, TRUE);
	    handle->state = CHUNK_STATE_HEADER;
	    break;
	}
	case CHUNK_STATE_TRAILERS: {
	    /* Consume any trailer headers until empty line */
	    while (1) {
		ISgets_to_growbuf(handle->is, &gb, TRUE);
		if (gb.length == 0)
		    break;
		char *p = gb.ptr;
		while (*p == ' ' || *p == '\t')
		    p++;
		if (*p == '\r' || *p == '\n' || *p == '\0')
		    break;
	    }
	    handle->state = CHUNK_STATE_EOS;
	    break;
	}
	case CHUNK_STATE_EOS:
	default:
	    break;
	}

	if (total_read > 0)
	    break;
    }

    growbuf_clear(&gb);
    return total_read;
}


