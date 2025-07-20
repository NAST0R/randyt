#include "http_client.h"
#include "utils.h"
#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#define MAX_RESPONSE_SIZE (1024 * 1024 * 2)
#define MAX_TAG_RESULTS 200  // Space for 100 tags (2 ptrs per tag)
#define MAX_CHANNEL_ID_LEN 128

static void open_url_in_browser(const char *url) {
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: randyt <channel>\n"); return 1; }

    srand((unsigned int)time(NULL));

    const char *username = argv[1];
    static uint8_t buf[MAX_RESPONSE_SIZE];
    http_response_t res;

    // --- Step 1: get HTML of channel homepage ---
    char homepage_url[256];
    snprintf(homepage_url, sizeof(homepage_url), "https://www.youtube.com/%s", username);

    if (http_get(homepage_url, buf, sizeof(buf), &res) != 0) return 10;
    if (res.was_truncated) return 11;
    buf[(res.body_length < sizeof(buf)) ? res.body_length : sizeof(buf) - 1] = 0;

    // --- Step 2: parse channel ID ---
    char channel_id[MAX_CHANNEL_ID_LEN];
    if (parse_channel_id((char *)buf, channel_id, sizeof(channel_id)) == 0) return 12;

    // --- Step 3: get RSS feed using channel ID ---
    snprintf(homepage_url, sizeof(homepage_url),
             "https://www.youtube.com/feeds/videos.xml?channel_id=%s", channel_id);

    if (http_get(homepage_url, buf, sizeof(buf), &res) != 0) return 13;
    if (res.was_truncated) return 14;
    buf[(res.body_length < sizeof(buf)) ? res.body_length : sizeof(buf) - 1] = 0;

    // --- Step 4: extract video IDs from feed ---
    const char *results[MAX_TAG_RESULTS];
    int found = find_tags_content("<yt:videoId>", (const char*)buf, results, MAX_TAG_RESULTS);
    if (found < 2 || (found & 1)) return 15;

    int num_tags = found / 2;
    int random_index = rand() % num_tags;

    const char *start = results[random_index * 2];
    const char *end = results[random_index * 2 + 1];
    int len = (int)(end - start);
    if (len >= 128) return 16;

    char video_id[128];
    memcpy(video_id, start, len);
    video_id[len] = '\0';

    char video_url[256];
    snprintf(video_url, sizeof(video_url), "https://www.youtube.com/watch?v=%s", video_id);

    open_url_in_browser(video_url);

    return 0;
}
