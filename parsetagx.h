/* $Id: parsetagx.h,v 1.4 2001/11/24 02:01:26 ukai Exp $ */
#ifndef PARSETAGX_H
#define PARSETAGX_H

#include "html.h"
#include "Str.h"

#define MAX_TAG_ATTR_INLINE 24

/* Parsed Tag structure */

struct parsed_tag {
    unsigned char tagid;
    unsigned char need_reconstruct;
    unsigned char attrid[MAX_TAG_ATTR_INLINE];
    char *value[MAX_TAG_ATTR_INLINE];
    const unsigned char *map;
};

#define parsedtag_accepts(tag, id) ((tag)->map&&(tag)->map[id]!=MAX_TAGATTR)
#define parsedtag_exists(tag, id)  (parsedtag_accepts(tag,id)&&((tag)->attrid[(tag)->map[id]]!=ATTR_UNKNOWN))
#define parsedtag_delete(tag, id)  (parsedtag_accepts(tag,id)&&((tag)->attrid[(tag)->map[id]]=ATTR_UNKNOWN))
#define parsedtag_need_reconstruct(tag) ((tag)->need_reconstruct)
#define parsedtag_attname(tag, i) (AttrMAP[(tag)->attrid[i]].name)

extern struct parsed_tag *parse_tag(char **s, int internal);
extern int parsedtag_get_value(struct parsed_tag *tag, int id, void *value);
extern int parsedtag_set_value(struct parsed_tag *tag, int id, char *value);
extern Str parsedtag2str(struct parsed_tag *tag);
#endif
