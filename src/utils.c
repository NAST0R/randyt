#include "utils.h"
#include <string.h>

#define MAX_TAG_LEN 256
#define MIN_TAG_LEN 3
#define ID_OFFSET 22

/*
   Helper to extract inner content of tags from YouTube RSS feed.
   Expects valid C strings for tag and xml.
   Doesn't support nested XML tags with same name or self-closing tags (only <tag></tag> patterns).
   Returns start and end of content pointers for each entry found.
   Uneven number of entries means there was something malformed, check it after called
*/

int find_tags_content(const char *tag, const char *xml, const char **res, int max_slots) {
    size_t taglen = strnlen(tag, MAX_TAG_LEN);
    if (taglen >= MAX_TAG_LEN - 2 || taglen < MIN_TAG_LEN || tag[0] != '<' || tag[taglen - 1] != '>') {
        return 0;
    }

    char tag_closure[MAX_TAG_LEN] = {0};
    tag_closure[0] = '<';
    tag_closure[1] = '/';
    memcpy(tag_closure + 2, tag + 1, taglen - 2);
    tag_closure[taglen] = '>';
    tag_closure[taglen + 1] = '\0';

    size_t closure_len = strlen(tag_closure);

    const char *cursor = xml;
    int count = 0;

    while (cursor && count + 2 <= max_slots) {
        const char *open = strstr(cursor, tag);
        if (!open) break;

        open = strchr(open, '>');
        if (!open) break;
        open++;

        const char *end_tag = strstr(open, tag_closure);
        if (!end_tag) break;

        res[count++] = open;
        res[count++] = end_tag;

        cursor = end_tag + closure_len;
    }

    return count;
}

/* 
   Parses channel_id from YouTube channel homepage raw HTML.
   Expects valid C string in HTML.
   Returns size of channel id string if found and channel id in buffer.
*/

size_t parse_channel_id(const char *html, char *buf, size_t bufsize) {
    char *cursor;
    char *end;
    size_t id_len;

    cursor = strstr(html, "videos.xml?channel_id=");
    if (!cursor) return 0;
    cursor += ID_OFFSET;

    end = strchr(cursor, '\"');
    if (!end) return 0;

    id_len = (int)(end - cursor);
    if (id_len >= bufsize) return 0;

    memcpy(buf, cursor, id_len);
    buf[id_len] = 0;

    return id_len;    
}