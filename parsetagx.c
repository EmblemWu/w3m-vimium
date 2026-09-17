/* $Id: parsetagx.c,v 1.18 2006/06/07 03:52:03 inu Exp $ */
#include "fm.h"
#include "myctype.h"
#include "indep.h"
#include "Str.h"
#include "parsetagx.h"
#include "hash.h"

#include "html.c"

/* parse HTML tag */

static int noConv(char *, char **);
static int toNumber(char *, int *);
static int toLength(char *, int *);
static int toAlign(char *, int *);
static int toVAlign(char *, int *);

typedef int (*ToValFunc)(char *, void *);

/* *INDENT-OFF* */
static ToValFunc toValFunc[] = {
    (ToValFunc)noConv,		/* VTYPE_NONE    */
    (ToValFunc)noConv,		/* VTYPE_STR     */
    (ToValFunc)toNumber,	/* VTYPE_NUMBER  */
    (ToValFunc)toLength,	/* VTYPE_LENGTH  */
    (ToValFunc)toAlign,		/* VTYPE_ALIGN   */
    (ToValFunc)toVAlign,	/* VTYPE_VALIGN  */
    (ToValFunc)noConv,		/* VTYPE_ACTION  */
    (ToValFunc)noConv,		/* VTYPE_ENCTYPE */
    (ToValFunc)noConv,		/* VTYPE_METHOD  */
    (ToValFunc)noConv,		/* VTYPE_MLENGTH */
    (ToValFunc)noConv,		/* VTYPE_TYPE    */
};
/* *INDENT-ON* */

static int
noConv(char *oval, char **str)
{
    *str = oval;
    return 1;
}

static int
toNumber(char *oval, int *num)
{
    char *ep;
    int x;

    x = strtol(oval, &ep, 10);

    if (ep > oval) {
	*num = x;
	return 1;
    }
    else
	return 0;
}

static int
toLength(char *oval, int *len)
{
    int w;
    if (!IS_DIGIT(oval[0]))
	return 0;
    w = atoi(oval);
    if (w < 0)
	return 0;
    if (w == 0)
	w = 1;
    if (oval[strlen(oval) - 1] == '%')
	*len = -w;
    else
	*len = w;
    return 1;
}

static int
toAlign(char *oval, int *align)
{
    if (strcasecmp(oval, "left") == 0)
	*align = ALIGN_LEFT;
    else if (strcasecmp(oval, "right") == 0)
	*align = ALIGN_RIGHT;
    else if (strcasecmp(oval, "center") == 0)
	*align = ALIGN_CENTER;
    else if (strcasecmp(oval, "top") == 0)
	*align = ALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
	*align = ALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
	*align = ALIGN_MIDDLE;
    else
	return 0;
    return 1;
}

static int
toVAlign(char *oval, int *valign)
{
    if (strcasecmp(oval, "top") == 0 || strcasecmp(oval, "baseline") == 0)
	*valign = VALIGN_TOP;
    else if (strcasecmp(oval, "bottom") == 0)
	*valign = VALIGN_BOTTOM;
    else if (strcasecmp(oval, "middle") == 0)
	*valign = VALIGN_MIDDLE;
    else
	return 0;
    return 1;
}

extern Hash_si tagtable;
#define MAX_TAG_LEN 64

static unsigned char static_tag_map[MAX_HTMLTAG][MAX_TAGATTR];
static int static_tag_map_initialized = 0;

static void
init_static_tag_map(void)
{
    int t, i;
    for (t = 0; t < MAX_HTMLTAG; t++) {
	memset(static_tag_map[t], MAX_TAGATTR, MAX_TAGATTR);
	for (i = 0; i < TagMAP[t].max_attribute; i++) {
	    static_tag_map[t][TagMAP[t].accept_attribute[i]] = (unsigned char)i;
	}
    }
    static_tag_map_initialized = 1;
}

struct parsed_tag *
parse_tag(char **s, int internal)
{
    struct parsed_tag *tag = NULL;
    int tag_id;
    char tagname[MAX_TAG_LEN], attrname[MAX_TAG_LEN];
    char *p, *q;
    int i, attr_id = 0, nattr;

    if (!static_tag_map_initialized)
	init_static_tag_map();

    /* Parse tag name */
    tagname[0] = '\0';
    q = (*s) + 1;
    p = tagname;
    if (*q == '/') {
	*(p++) = *(q++);
	SKIP_BLANKS(q);
    }
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
	   *q != '>' && p - tagname < MAX_TAG_LEN - 1) {
	*(p++) = TOLOWER(*q);
	q++;
    }
    *p = '\0';
    while (*q && !IS_SPACE(*q) && !(tagname[0] != '/' && *q == '/') &&
	   *q != '>')
	q++;

    tag_id = getHash_si(&tagtable, tagname, HTML_UNKNOWN);

    if (tag_id == HTML_UNKNOWN ||
	(!internal && TagMAP[tag_id].flag & TFLG_INT))
	goto skip_parse_tagarg;

    tag = New(struct parsed_tag);
    bzero(tag, sizeof(struct parsed_tag));
    tag->tagid = tag_id;
    tag->need_reconstruct = FALSE;
    tag->map = static_tag_map[tag_id];

    nattr = TagMAP[tag_id].max_attribute;
    if (nattr > 0) {
	if (nattr > MAX_TAG_ATTR_INLINE)
	    nattr = MAX_TAG_ATTR_INLINE;
	memset(tag->attrid, ATTR_UNKNOWN, nattr);
    }

    /* Parse tag arguments */
    SKIP_BLANKS(q);
    while (1) {
	char *val_str = NULL;
	if (*q == '>' || *q == '\0')
	    goto done_parse_tag;
	p = attrname;
	while (*q && *q != '=' && !IS_SPACE(*q) &&
	       *q != '>' && p - attrname < MAX_TAG_LEN - 1) {
	    *(p++) = TOLOWER(*q);
	    q++;
	}
	*p = '\0';
	while (*q && *q != '=' && !IS_SPACE(*q) && *q != '>')
	    q++;
	SKIP_BLANKS(q);
	int attr_slot = -1;
	char a0 = attrname[0];
	for (i = 0; i < nattr; i++) {
	    if (tag->attrid[i] == ATTR_UNKNOWN) {
		const char *mname = AttrMAP[TagMAP[tag_id].accept_attribute[i]].name;
		if (mname[0] == a0 && strcmp(mname, attrname) == 0) {
		    attr_id = TagMAP[tag_id].accept_attribute[i];
		    attr_slot = i;
		    break;
		}
	    }
	}

	if (*q == '=') {
	    /* get value */
	    q++;
	    SKIP_BLANKS(q);
	    char quote_ch = 0;
	    if (*q == '"' || *q == '\'') {
		quote_ch = *q++;
	    }
	    char *val_start = q;
	    int has_newline = 0;
	    int has_quote = 0;

	    if (quote_ch) {
		while (*q && *q != quote_ch) {
		    if (*q == '\n') has_newline = 1;
		    if (is_html_quote(*q)) has_quote = 1;
		    q++;
		}
	    } else {
		while (*q && !IS_SPACE(*q) && *q != '>') {
		    if (*q == '\n') has_newline = 1;
		    if (is_html_quote(*q)) has_quote = 1;
		    q++;
		}
	    }
	    int val_len = (int)(q - val_start);
	    if (quote_ch && *q == quote_ch)
		q++;

	    if (has_quote && !tag->need_reconstruct)
		tag->need_reconstruct = TRUE;

	    if (attr_slot >= 0) {
		if (!has_newline) {
		    val_str = allocStr(val_start, val_len);
		} else {
		    char *dst = NewAtom_N(char, val_len + 1);
		    char *src = val_start;
		    char *dp = dst;
		    int k;
		    for (k = 0; k < val_len; k++, src++) {
			if (*src != '\n')
			    *dp++ = *src;
		    }
		    *dp = '\0';
		    val_str = dst;
		}
	    }
	}

	if (attr_slot >= 0) {
	    if (!internal &&
		((AttrMAP[attr_id].flag & AFLG_INT) ||
		 (val_str && AttrMAP[attr_id].vtype == VTYPE_METHOD &&
		  !strcasecmp(val_str, "internal")))) {
		tag->need_reconstruct = TRUE;
		continue;
	    }
	    tag->attrid[attr_slot] = attr_id;
	    if (val_str)
		tag->value[attr_slot] = html_unquote(val_str);
	    else
		tag->value[attr_slot] = NULL;
	}
	else {
	    tag->need_reconstruct = TRUE;
	}
    }

  skip_parse_tagarg:
    while (*q != '>' && *q)
	q++;
  done_parse_tag:
    if (*q == '>')
	q++;
    *s = q;
    return tag;
}

int
parsedtag_set_value(struct parsed_tag *tag, int id, char *value)
{
    int i;

    if (!parsedtag_accepts(tag, id))
	return 0;

    i = tag->map[id];
    if (i >= MAX_TAG_ATTR_INLINE)
	return 0;
    tag->attrid[i] = id;
    if (value)
	tag->value[i] = allocStr(value, -1);
    else
	tag->value[i] = NULL;
    tag->need_reconstruct = TRUE;
    return 1;
}

int
parsedtag_get_value(struct parsed_tag *tag, int id, void *value)
{
    int i;
    if (!parsedtag_exists(tag, id))
	return 0;
    i = tag->map[id];
    if (i >= MAX_TAG_ATTR_INLINE || !tag->value[i])
	return 0;
    return toValFunc[AttrMAP[id].vtype] (tag->value[i], value);
}

Str
parsedtag2str(struct parsed_tag *tag)
{
    int i;
    int tag_id = tag->tagid;
    int nattr = TagMAP[tag_id].max_attribute;
    Str tagstr = Strnew();
    if (nattr > MAX_TAG_ATTR_INLINE)
	nattr = MAX_TAG_ATTR_INLINE;
    Strcat_char(tagstr, '<');
    Strcat_charp(tagstr, TagMAP[tag_id].name);
    for (i = 0; i < nattr; i++) {
	if (tag->attrid[i] != ATTR_UNKNOWN) {
	    Strcat_char(tagstr, ' ');
	    Strcat_charp(tagstr, AttrMAP[tag->attrid[i]].name);
	    if (tag->value[i]) {
		Strcat_charp(tagstr, "=\"");
		html_quote_to_Str(tagstr, tag->value[i]);
		Strcat_char(tagstr, '"');
	    }
	}
    }
    Strcat_char(tagstr, '>');
    return tagstr;
}
