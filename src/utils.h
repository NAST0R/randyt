#ifndef RSS_H
#define RSS_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

int find_tags_content(const char *tag, const char *xml, const char **res, int max_slots);
int parse_channel_id(const char *html, char *buf, size_t bufsize);

#endif
