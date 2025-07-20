#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdint.h>

typedef struct {
    int status_code;
    size_t body_length;
    int was_truncated;  // 1 if truncated, else 0
} http_response_t;

int http_get(const char *url,
             uint8_t *response_buffer,
             size_t response_buffer_size,
             http_response_t *response_out);

int http_post(const char *url,
              const uint8_t *request_body,
              size_t request_body_size,
              const char *content_type,
              uint8_t *response_buffer,
              size_t response_buffer_size,
              http_response_t *response_out);

#endif // HTTP_CLIENT_H
