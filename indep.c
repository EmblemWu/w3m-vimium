/* $Id: indep.c,v 1.38 2007/05/23 15:06:05 inu Exp $ */
#include "fm.h"
#include <stdio.h>
#ifndef __MINGW32_VERSION
#include <pwd.h>
#endif /* __MINGW32_VERSION */
#include <sys/param.h>
#include <sys/types.h>
#include <stdlib.h>
#include "indep.h"
#include "Str.h"
#include <gc.h>
#include "myctype.h"
#include "entity.h"

unsigned char QUOTE_MAP[0x100] = {
    /* NUL SOH STX ETX EOT ENQ ACK BEL  BS  HT  LF  VT  FF  CR  SO  SI */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* DLE DC1 DC2 DC3 DC4 NAK SYN ETB CAN  EM SUB ESC  FS  GS  RS  US */
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    /* SPC   !   "   #   $   %   &   '   (   )   *   +   ,   -   .   / */
    24, 72, 76, 40, 8, 40, 41, 77, 72, 72, 72, 40, 72, 8, 0, 64,
    /*   0   1   2   3   4   5   6   7   8   9   :   ;   <   =   >   ? */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 72, 74, 72, 75, 40,
    /*   @   A   B   C   D   E   F   G   H   I   J   K   L   M   N   O */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   P   Q   R   S   T   U   V   W   X   Y   Z   [   \   ]   ^   _ */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 0,
    /*   `   a   b   c   d   e   f   g   h   i   j   k   l   m   n   o */
    72, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /*   p   q   r   s   t   u   v   w   x   y   z   {   |   }   ~ DEL */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 72, 72, 72, 72, 24,

    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
};

char *HTML_QUOTE_MAP[] = {
    NULL,
    "&amp;",
    "&lt;",
    "&gt;",
    "&quot;",
    "&apos;",
    NULL,
    NULL,
};

clen_t
strtoclen(const char *s)
{
#ifdef HAVE_STRTOLL
    return strtoll(s, NULL, 10);
#elif defined(HAVE_STRTOQ)
    return strtoq(s, NULL, 10);
#elif defined(HAVE_ATOLL)
    return atoll(s);
#elif defined(HAVE_ATOQ)
    return atoq(s);
#else
    return atoi(s);
#endif
}

#ifndef HAVE_BCOPY
void
bcopy(const void *src, void *dest, int len)
{
    int i;
    if (src == dest)
	return;
    if (src < dest) {
	for (i = len - 1; i >= 0; i--)
	    ((char *)dest)[i] = ((const char *)src)[i];
    }
    else {			/* src > dest */
	for (i = 0; i < len; i++)
	    ((char *)dest)[i] = ((const char *)src)[i];
    }
}

void
bzero(void *ptr, int len)
{
    int i;
    char *p = ptr;
    for (i = 0; i < len; i++)
	*(p++) = 0;
}
#endif				/* not HAVE_BCOPY */

char *
allocStr(const char *s, int len)
{
    char *ptr;

    if (s == NULL)
	return NULL;
    if (len < 0)
	len = strlen(s);
    if (len < 0 || len >= STR_SIZE_MAX)
	len = STR_SIZE_MAX - 1;
    ptr = NewAtom_N(char, len + 1);
    if (ptr == NULL) {
	fprintf(stderr, "fm: Can't allocate string. Give me more memory!\n");
	exit(-1);
    }
    bcopy(s, ptr, len);
    ptr[len] = '\0';
    return ptr;
}

int
strCmp(const void *s1, const void *s2)
{
    return strcmp(*(const char **)s1, *(const char **)s2);
}

char *
currentdir()
{
    char *path;
#ifdef HAVE_GETCWD
#ifdef MAXPATHLEN
    path = NewAtom_N(char, MAXPATHLEN);
    getcwd(path, MAXPATHLEN);
#else
    path = getcwd(NULL, 0);
#endif
#else				/* not HAVE_GETCWD */
#ifdef HAVE_GETWD
    path = NewAtom_N(char, 1024);
    getwd(path);
#else				/* not HAVE_GETWD */
    FILE *f;
    char *p;
    path = NewAtom_N(char, 1024);
    f = popen("pwd", "r");
    fgets(path, 1024, f);
    pclose(f);
    for (p = path; *p; p++)
	if (*p == '\n') {
	    *p = '\0';
	    break;
	}
#endif				/* not HAVE_GETWD */
#endif				/* not HAVE_GETCWD */
    return path;
}

char *
cleanupName(char *name)
{
    char *buf, *p, *q;

    buf = allocStr(name, -1);
    p = buf;
    q = name;
    while (*q != '\0') {
	if (strncmp(p, "/../", 4) == 0) {	/* foo/bar/../FOO */
	    if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
		/* ../../       */
		p += 3;
		q += 3;
	    }
	    else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
		/* ../../../    */
		p += 3;
		q += 3;
	    }
	    else {
		while (p != buf && *--p != '/') ;	/* ->foo/FOO */
		*p = '\0';
		q += 3;
		strcat(buf, q);
	    }
	}
	else if (strcmp(p, "/..") == 0) {	/* foo/bar/..   */
	    if (p - 2 == buf && strncmp(p - 2, "..", 2) == 0) {
		/* ../..        */
	    }
	    else if (p - 3 >= buf && strncmp(p - 3, "/..", 3) == 0) {
		/* ../../..     */
	    }
	    else {
		while (p != buf && *--p != '/') ;	/* ->foo/ */
		*++p = '\0';
	    }
	    break;
	}
	else if (strncmp(p, "/./", 3) == 0) {	/* foo/./bar */
	    *p = '\0';		/* -> foo/bar           */
	    q += 2;
	    strcat(buf, q);
	}
	else if (strcmp(p, "/.") == 0) {	/* foo/. */
	    *++p = '\0';	/* -> foo/              */
	    break;
	}
	else if (strncmp(p, "//", 2) == 0) {	/* foo//bar */
	    /* -> foo/bar           */
	    *p = '\0';
	    q++;
	    strcat(buf, q);
	}
	else {
	    p++;
	    q++;
	}
    }
    return buf;
}

char *
expandPath(char *name)
{
    char *p;
    struct passwd *passent, *getpwnam(const char *);
    Str extpath = NULL;

    if (name == NULL)
	return NULL;
    p = name;
    if (*p == '~') {
	p++;
#ifndef __MINGW32_VERSION
	if (IS_ALPHA(*p)) {
	    char *q = strchr(p, '/');
	    if (q) {		/* ~user/dir... */
		passent = getpwnam(allocStr(p, q - p));
		p = q;
	    }
	    else {		/* ~user */
		passent = getpwnam(p);
		p = "";
	    }
	    if (!passent)
		goto rest;
	    extpath = Strnew_charp(passent->pw_dir);
	} else
#endif /* __MINGW32_VERSION */
	  if (*p == '/' || *p == '\0') {	/* ~/dir... or ~ */
	    extpath = Strnew_charp(getenv("HOME"));
	}
	else
	    goto rest;
	if (Strcmp_charp(extpath, "/") == 0 && *p == '/')
	    p++;
	Strcat_charp(extpath, p);
	return extpath->ptr;
    }
  rest:
    return name;
}

#ifndef HAVE_STRCHR
char *
strchr(const char *s, int c)
{
    while (*s) {
	if ((unsigned char)*s == c)
	    return (char *)s;
	s++;
    }
    return NULL;
}
#endif				/* not HAVE_STRCHR */

#ifndef HAVE_STRCASECMP
int
strcasecmp(const char *s1, const char *s2)
{
    int x;
    while (*s1) {
	x = TOLOWER(*s1) - TOLOWER(*s2);
	if (x != 0)
	    return x;
	s1++;
	s2++;
    }
    return -TOLOWER(*s2);
}

int
strncasecmp(const char *s1, const char *s2, size_t n)
{
    int x;
    while (*s1 && n) {
	x = TOLOWER(*s1) - TOLOWER(*s2);
	if (x != 0)
	    return x;
	s1++;
	s2++;
	n--;
    }
    return n ? -TOLOWER(*s2) : 0;
}
#endif				/* not HAVE_STRCASECMP */

#ifndef HAVE_STRCASESTR
/* string search using the simplest algorithm */
char *
strcasestr(const char *s1, const char *s2)
{
    int len1, len2;
    if (s2 == NULL)
	return (char *)s1;
    if (*s2 == '\0')
	return (char *)s1;
    len1 = strlen(s1);
    len2 = strlen(s2);
    while (*s1 && len1 >= len2) {
	if (strncasecmp(s1, s2, len2) == 0)
	    return (char *)s1;
	s1++;
	len1--;
    }
    return 0;
}
#endif

static int
strcasematch(char *s1, char *s2)
{
    int x;
    while (*s1) {
	if (*s2 == '\0')
	    return 1;
	x = TOLOWER(*s1) - TOLOWER(*s2);
	if (x != 0)
	    break;
	s1++;
	s2++;
    }
    return (*s2 == '\0');
}

/* search multiple strings */
int
strcasemstr(char *str, char *srch[], char **ret_ptr)
{
    int i;
    while (*str) {
	for (i = 0; srch[i]; i++) {
	    if (strcasematch(str, srch[i])) {
		if (ret_ptr)
		    *ret_ptr = str;
		return i;
	    }
	}
	str++;
    }
    return -1;
}

int
strmatchlen(const char *s1, const char *s2, int maxlen)
{
    int i;

    /* To allow the maxlen to be negatie (infinity),
     * compare by "!=" instead of "<=". */
    for (i = 0; i != maxlen; ++i) {
	if (!s1[i] || !s2[i] || s1[i] != s2[i])
	    break;
    }
    return i;
}

char *
remove_space(char *str)
{
    char *p, *q;

    for (p = str; *p && IS_SPACE(*p); p++) ;
    for (q = p; *q; q++) ;
    for (; q > p && IS_SPACE(*(q - 1)); q--) ;
    if (*q != '\0')
	return Strnew_charp_n(p, q - p)->ptr;
    return p;
}

int
non_null(char *s)
{
    if (s == NULL)
	return FALSE;
    while (*s) {
	if (!IS_SPACE(*s))
	    return TRUE;
	s++;
    }
    return FALSE;
}

void
cleanup_line(Str s, int mode)
{
    if (s->length >= 2 &&
	s->ptr[s->length - 2] == '\r' && s->ptr[s->length - 1] == '\n') {
	s->ptr[s->length - 2] = '\n';
	s->ptr[s->length - 1] = '\0';
	s->length--;
    }
    else if (Strlastchar(s) == '\r')
	s->ptr[s->length - 1] = '\n';
    else if (Strlastchar(s) != '\n')
	Strcat_char(s, '\n');
    if (mode != PAGER_MODE) {
	int i;
	for (i = 0; i < s->length; i++) {
	    if (s->ptr[i] == '\0')
		s->ptr[i] = ' ';
	}
    }
}

int
getescapechar(char **str)
{
    int dummy = -1;
    char *p = *str, *q;

    if (*p == '&')
	p++;
    if (*p == '#') {
	p++;
	if (*p == 'x' || *p == 'X') {
	    p++;
	    if (!IS_XDIGIT(*p)) {
		*str = p;
		return -1;
	    }
	    for (dummy = GET_MYCDIGIT(*p), p++; IS_XDIGIT(*p); p++)
		dummy = dummy * 0x10 + GET_MYCDIGIT(*p);
	    if (*p == ';')
		p++;
	    *str = p;
	    return dummy;
	}
	else {
	    if (!IS_DIGIT(*p)) {
		*str = p;
		return -1;
	    }
	    for (dummy = GET_MYCDIGIT(*p), p++; IS_DIGIT(*p); p++)
		dummy = dummy * 10 + GET_MYCDIGIT(*p);
	    if (*p == ';')
		p++;
	    *str = p;
	    return dummy;
	}
    }
    if (!IS_ALPHA(*p)) {
	*str = p;
	return -1;
    }
    q = p;
    for (p++; IS_ALNUM(*p); p++) ;
    int len = (int)(p - q);

    /* Fast-path zero-allocation lookup for frequent HTML entities */
    int fast_val = -1;
    int is_common_loose = 0;

    switch (len) {
    case 2:
	if ((q[0] == 'l' || q[0] == 'L') && (q[1] == 't' || q[1] == 'T')) {
	    fast_val = 0x3c; /* < */
	    is_common_loose = 1;
	}
	else if ((q[0] == 'g' || q[0] == 'G') && (q[1] == 't' || q[1] == 'T')) {
	    fast_val = 0x3e; /* > */
	    is_common_loose = 1;
	}
	break;
    case 3:
	if (!strncasecmp(q, "amp", 3)) {
	    fast_val = 0x26; /* & */
	    is_common_loose = 1;
	}
	break;
    case 4:
	if (!strncasecmp(q, "quot", 4)) {
	    fast_val = 0x22; /* " */
	    is_common_loose = 1;
	}
	else if (!strncasecmp(q, "apos", 4)) {
	    fast_val = 0x27; /* ' */
	    is_common_loose = 1;
	}
	else if (!strncasecmp(q, "nbsp", 4)) {
	    fast_val = 0xa0; /* nbsp */
	    is_common_loose = 1;
	}
	else if (!strncasecmp(q, "copy", 4)) {
	    fast_val = 0xa9; /* copyright */
	}
	break;
    case 5:
	if (!strncasecmp(q, "laquo", 5))
	    fast_val = 0xab;
	else if (!strncasecmp(q, "raquo", 5))
	    fast_val = 0xbb;
	else if (!strncasecmp(q, "trade", 5))
	    fast_val = 0x2122;
	else if (!strncasecmp(q, "times", 5))
	    fast_val = 0xd7;
	break;
    case 6:
	if (!strncasecmp(q, "hellip", 6))
	    fast_val = 0x2026;
	else if (!strncasecmp(q, "mdash", 6))
	    fast_val = 0x2014;
	else if (!strncasecmp(q, "ndash", 6))
	    fast_val = 0x2013;
	else if (!strncasecmp(q, "divide", 6))
	    fast_val = 0xf7;
	break;
    default:
	break;
    }

    if (fast_val >= 0) {
	if (*p == ';') {
	    p++;
	    *str = p;
	    return fast_val;
	}
	if (is_common_loose && *p != '=') {
	    *str = p;
	    return fast_val;
	}
	*str = p;
	return -1;
    }

    /* Fallback hash lookup with stack buffer (zero GC heap allocation) */
    char ent_buf[64];
    if (len >= (int)sizeof(ent_buf)) {
	*str = p;
	return -1;
    }
    memcpy(ent_buf, q, len);
    ent_buf[len] = '\0';

    if (*p == ';') {
	p++;
	*str = p;
	return getHash_si(&entity, ent_buf, -1);
    }
    else {
	*str = p;
	return -1;
    }
}

char *
getescapecmd(char **s)
{
    char *save = *s;
    Str tmp;
    int ch = getescapechar(s);

    if (ch >= 0)
	return conv_entity(ch);

    if (*save != '&')
	tmp = Strnew_charp("&");
    else
	tmp = Strnew();
    Strcat_charp_n(tmp, save, *s - save);
    return tmp->ptr;
}

char *
html_quote(char *str)
{
    Str tmp = NULL;
    char *p, *q;

    if (!str)
	return str;

    for (p = str; *p; ) {
	q = html_quote_char(*p);
	if (q) {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(str, (int)(p - str));
	    Strcat_charp(tmp, q);
	    p++;
	}
	else {
	    char *start = p;
	    while (*p && !html_quote_char(*p))
		p++;
	    if (tmp)
		Strcat_charp_n(tmp, start, (int)(p - start));
	}
    }
    if (tmp)
	return tmp->ptr;
    return str;
}

void
html_quote_to_Str(Str s, const char *str)
{
    const char *p, *q;

    if (!s || !str)
	return;

    for (p = str; *p; ) {
	q = html_quote_char(*p);
	if (q) {
	    Strcat_charp(s, q);
	    p++;
	}
	else {
	    const char *start = p;
	    while (*p && !html_quote_char(*p))
		p++;
	    Strcat_charp_n(s, start, (int)(p - start));
	}
    }
}

char *
html_unquote(char *str)
{
    Str tmp = NULL;
    char *p, *q;

    if (!str)
	return str;
    p = strchr(str, '&');
    if (!p)
	return str;

    tmp = Strnew_charp_n(str, (int)(p - str));
    while (*p) {
	if (*p == '&') {
	    q = getescapecmd(&p);
	    Strcat_charp(tmp, q);
	}
	else {
	    char *start = p;
	    while (*p && *p != '&')
		p++;
	    Strcat_charp_n(tmp, start, (int)(p - start));
	}
    }

    return tmp->ptr;
}

static char xdigit[0x10] = "0123456789ABCDEF";

#define url_unquote_char(pstr) \
  ((IS_XDIGIT((*(pstr))[1]) && IS_XDIGIT((*(pstr))[2])) ? \
    (*(pstr) += 3, (GET_MYCDIGIT((*(pstr))[-2]) << 4) | GET_MYCDIGIT((*(pstr))[-1])) : \
   -1)

char *
url_quote(char *str)
{
    Str tmp = NULL;
    char *p;

    for (p = str; *p; p++) {
	if (is_url_quote(*p)) {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(str, (int)(p - str));
	    Strcat_char(tmp, '%');
	    Strcat_char(tmp, xdigit[((unsigned char)*p >> 4) & 0xF]);
	    Strcat_char(tmp, xdigit[(unsigned char)*p & 0xF]);
	}
	else {
	    if (tmp)
		Strcat_char(tmp, *p);
	}
    }
    if (tmp)
	return tmp->ptr;
    return str;
}

char *
file_quote(char *str)
{
    Str tmp = NULL;
    char *p;
    char buf[4];

    for (p = str; *p; p++) {
	if (is_file_quote(*p)) {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(str, (int)(p - str));
	    sprintf(buf, "%%%02X", (unsigned char)*p);
	    Strcat_charp(tmp, buf);
	}
	else {
	    if (tmp)
		Strcat_char(tmp, *p);
	}
    }
    if (tmp)
	return tmp->ptr;
    return str;
}

char *
file_unquote(char *str)
{
    Str tmp = NULL;
    char *p, *q;
    int c;

    for (p = str; *p;) {
	if (*p == '%') {
	    q = p;
	    c = url_unquote_char(&q);
	    if (c >= 0) {
		if (tmp == NULL)
		    tmp = Strnew_charp_n(str, (int)(p - str));
		if (c != '\0' && c != '\n' && c != '\r')
		    Strcat_char(tmp, (char)c);
		p = q;
		continue;
	    }
	}
	if (tmp)
	    Strcat_char(tmp, *p);
	p++;
    }
    if (tmp)
	return tmp->ptr;
    return str;
}

Str
Str_form_quote(Str x)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length;
    char buf[4];

    for (; p < ep; p++) {
	if (*p == ' ') {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
	    Strcat_char(tmp, '+');
	}
	else if (is_url_unsafe(*p)) {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
	    sprintf(buf, "%%%02X", (unsigned char)*p);
	    Strcat_charp(tmp, buf);
	}
	else {
	    if (tmp)
		Strcat_char(tmp, *p);
	}
    }
    if (tmp)
	return tmp;
    return x;
}


Str
Str_url_unquote(Str x, int is_form, int safe)
{
    Str tmp = NULL;
    char *p = x->ptr, *ep = x->ptr + x->length, *q;
    int c;

    for (; p < ep;) {
	if (is_form && *p == '+') {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
	    Strcat_char(tmp, ' ');
	    p++;
	    continue;
	}
	else if (*p == '%') {
	    q = p;
	    c = url_unquote_char(&q);
	    if (c >= 0 && (!safe || !IS_ASCII(c) || !is_file_quote(c))) {
		if (tmp == NULL)
		    tmp = Strnew_charp_n(x->ptr, (int)(p - x->ptr));
		Strcat_char(tmp, (char)c);
		p = q;
		continue;
	    }
	}
	if (tmp)
	    Strcat_char(tmp, *p);
	p++;
    }
    if (tmp)
	return tmp;
    return x;
}

char *
shell_quote(char *str)
{
    Str tmp = NULL;
    char *p;

    for (p = str; *p; p++) {
	if (is_shell_unsafe(*p)) {
	    if (tmp == NULL)
		tmp = Strnew_charp_n(str, (int)(p - str));
	    Strcat_char(tmp, '\\');
	    Strcat_char(tmp, *p);
	}
	else {
	    if (tmp)
		Strcat_char(tmp, *p);
	}
    }
    if (tmp)
	return tmp->ptr;
    return str;
}

void *
xrealloc(void *ptr, size_t size)
{
    void *newptr = realloc(ptr, size);
    if (newptr == NULL) {
	fprintf(stderr, "Out of memory\n");
	exit(-1);
    }
    return newptr;
}

/* Define this as a separate function in case the free() has
 * an incompatible prototype. */
void
xfree(void *ptr)
{
    free(ptr);
}

void *
w3m_GC_realloc_atomic(void *ptr, size_t size)
{
    return ptr ? GC_REALLOC(ptr, size) : GC_MALLOC_ATOMIC(size);
}

void
w3m_GC_free(void *ptr)
{
    GC_FREE(ptr);
}

void
growbuf_init(struct growbuf *gb)
{
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    gb->realloc_proc = &w3m_GC_realloc_atomic;
    gb->free_proc = &w3m_GC_free;
}

void
growbuf_init_without_GC(struct growbuf *gb)
{
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    gb->realloc_proc = &xrealloc;
    gb->free_proc = &xfree;
}

void
growbuf_clear(struct growbuf *gb)
{
    (*gb->free_proc) (gb->ptr);
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
}

Str
growbuf_to_Str(struct growbuf *gb)
{
    Str s;

    if (gb->free_proc == &w3m_GC_free) {
	growbuf_reserve(gb, gb->length + 1);
	gb->ptr[gb->length] = '\0';
	s = New(struct _Str);
	s->ptr = gb->ptr;
	s->length = gb->length;
	s->area_size = gb->area_size;
    } else {
	s = Strnew_charp_n(gb->ptr, gb->length);
	(*gb->free_proc) (gb->ptr);
    }
    gb->ptr = NULL;
    gb->length = 0;
    gb->area_size = 0;
    return s;
}

void
growbuf_reserve(struct growbuf *gb, int leastarea)
{
    int newarea;

    if (gb->area_size < leastarea) {
	newarea = gb->area_size * 3 / 2;
	if (newarea < leastarea)
	    newarea = leastarea;
	newarea += 16;
	gb->ptr = (*gb->realloc_proc) (gb->ptr, newarea);
	gb->area_size = newarea;
    }
}

void
growbuf_append(struct growbuf *gb, const unsigned char *src, int len)
{
    growbuf_reserve(gb, gb->length + len);
    memcpy(&gb->ptr[gb->length], src, len);
    gb->length += len;
}

static char *
w3m_dir(const char *name, char *dft)
{
#ifdef USE_PATH_ENVVAR
    char *value = getenv(name);
    if (value && *value)
	return value;
#endif
#if defined(__APPLE__) || defined(__MACH__)
    {
	struct stat st;
	if (stat(dft, &st) != 0) {
	    if (strcmp(name, "W3M_AUXBIN_DIR") == 0) {
		if (stat("/opt/homebrew/libexec/w3m", &st) == 0)
		    return "/opt/homebrew/libexec/w3m";
		if (stat("/opt/homebrew/opt/w3m/libexec/w3m", &st) == 0)
		    return "/opt/homebrew/opt/w3m/libexec/w3m";
	    }
	    else if (strcmp(name, "W3M_LIB_DIR") == 0) {
		if (stat("/opt/homebrew/libexec/w3m/cgi-bin", &st) == 0)
		    return "/opt/homebrew/libexec/w3m/cgi-bin";
	    }
	    else if (strcmp(name, "W3M_HELP_DIR") == 0) {
		if (stat("/opt/homebrew/share/w3m", &st) == 0)
		    return "/opt/homebrew/share/w3m";
	    }
	    else if (strcmp(name, "W3M_ETC_DIR") == 0 || strcmp(name, "W3M_CONF_DIR") == 0) {
		if (stat("/opt/homebrew/etc/w3m", &st) == 0)
		    return "/opt/homebrew/etc/w3m";
	    }
	}
    }
#endif
    return dft;
}

char *
w3m_auxbin_dir(void)
{
    return w3m_dir("W3M_AUXBIN_DIR", AUXBIN_DIR);
}

char *
w3m_lib_dir(void)
{
    /* FIXME: use W3M_CGIBIN_DIR? */
    return w3m_dir("W3M_LIB_DIR", CGIBIN_DIR);
}

char *
w3m_etc_dir(void)
{
    return w3m_dir("W3M_ETC_DIR", ETC_DIR);
}

char *
w3m_conf_dir(void)
{
    return w3m_dir("W3M_CONF_DIR", CONF_DIR);
}

char *
w3m_help_dir(void)
{
    return w3m_dir("W3M_HELP_DIR", HELP_DIR);
}
/* Local Variables:    */
/* c-basic-offset: 4   */
/* tab-width: 8        */
/* End:                */
