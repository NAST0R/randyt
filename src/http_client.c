#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <winhttp.h>
#include <wchar.h>
#include <string.h>
#include <stdint.h>
#include "http_client.h"

#define HTTP_TIMEOUT_RESOLVE    3000
#define HTTP_TIMEOUT_CONNECT    3000
#define HTTP_TIMEOUT_SEND       5000
#define HTTP_TIMEOUT_RECEIVE    5000

#define SAFE_CLOSE(h) if ((h) != NULL) { WinHttpCloseHandle(h); (h) = NULL; }

static wchar_t *utf8_to_wide(const char *str, wchar_t *buf, size_t max_wchars) {
    if (!str || !buf || max_wchars == 0) return NULL;
    int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, buf, (int)max_wchars);
    return (len > 0 && len < (int)max_wchars) ? buf : NULL;
}

static int parse_url(const char *url, wchar_t *host, size_t host_len,
                     wchar_t *path, size_t path_len,
                     INTERNET_PORT *port_out, BOOL *is_https_out) {
    if (!url || !host || !path || !port_out || !is_https_out) return -1;
    if (strlen(url) >= 2048) return -1;

    wchar_t wurl[1024];
    if (!utf8_to_wide(url, wurl, sizeof(wurl) / sizeof(wurl[0]))) return -1;

    URL_COMPONENTS parts = {0};
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = (DWORD)(host_len - 1);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = (DWORD)(path_len - 1);

    if (!WinHttpCrackUrl(wurl, 0, 0, &parts)) return -1;

    if (parts.dwHostNameLength < host_len - 1) host[parts.dwHostNameLength] = L'\0';
    if (parts.dwUrlPathLength < path_len - 1) path[parts.dwUrlPathLength] = L'\0';

    *port_out = parts.nPort;
    *is_https_out = (parts.nScheme == INTERNET_SCHEME_HTTPS);
    return 0;
}

static int http_request(const wchar_t *method, const wchar_t *host,
                        INTERNET_PORT port, const wchar_t *path, BOOL is_https,
                        const uint8_t *req_body, size_t req_len,
                        const wchar_t *content_type,
                        uint8_t *resp_buf, size_t resp_buf_size,
                        http_response_t *resp_out) {
    if (!method || !host || !path || !resp_buf || !resp_out) return -1;

    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    DWORD flags = is_https ? WINHTTP_FLAG_SECURE : 0;
    DWORD total_read = 0;

    hSession = WinHttpOpen(L"WinHTTP/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY,
                           WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) goto cleanup;

    hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) goto cleanup;

    hRequest = WinHttpOpenRequest(hConnect, method, path, NULL, WINHTTP_NO_REFERER,
                                  WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) goto cleanup;

    DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY,
                     &redirect_policy, sizeof(redirect_policy));

    if (!WinHttpSetTimeouts(hRequest, HTTP_TIMEOUT_RESOLVE, HTTP_TIMEOUT_CONNECT,
                            HTTP_TIMEOUT_SEND, HTTP_TIMEOUT_RECEIVE)) {
        goto cleanup;
    }

    // --- Add standard browser-like headers ---
    WinHttpAddRequestHeaders(hRequest,
        L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        L"AppleWebKit/537.36 (KHTML, like Gecko) "
        L"Chrome/115.0.0.0 Safari/537.36\r\n"
        L"Accept-Language: en-US,en;q=0.9\r\n"
        L"Cookie: SOCS=CAESEwgDEgk0ODE3Nzk3MjQaAmVuIAEaBgiA_LyaBg",  // Random SOCS for YouTube cookies
        -1, WINHTTP_ADDREQ_FLAG_ADD);

    // --- Add Content-Type if needed ---
    if (req_body && content_type) {
        wchar_t header[128];
        swprintf_s(header, 128, L"Content-Type: %s", content_type);
        WinHttpAddRequestHeaders(hRequest, header, -1, WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            (LPVOID)req_body, (DWORD)req_len, (DWORD)req_len, 0)) {
        goto cleanup;
    }

    if (!WinHttpReceiveResponse(hRequest, NULL)) goto cleanup;

    while (1) {
        DWORD avail = 0, read = 0;

        if (!WinHttpQueryDataAvailable(hRequest, &avail)) goto cleanup;
        if (avail == 0) break;

        if (total_read + avail > resp_buf_size) {
            avail = (DWORD)(resp_buf_size - total_read);
            if (avail == 0) break;
        }

        if (!WinHttpReadData(hRequest, resp_buf + total_read, avail, &read)) goto cleanup;
        if (read == 0) break;

        total_read += read;
    }

    DWORD status_code = 0, len = sizeof(status_code);
    if (!WinHttpQueryHeaders(hRequest,
                             WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                             NULL, &status_code, &len, NULL)) {
        status_code = 500;
    }

    resp_out->status_code = (int)status_code;
    resp_out->body_length = total_read;
    resp_out->was_truncated = (total_read == resp_buf_size);

    SAFE_CLOSE(hRequest);
    SAFE_CLOSE(hConnect);
    SAFE_CLOSE(hSession);
    return 0;

cleanup:
    SAFE_CLOSE(hRequest);
    SAFE_CLOSE(hConnect);
    SAFE_CLOSE(hSession);
    return -1;
}

int http_get(const char *url, uint8_t *response_buffer,
             size_t response_buffer_size, http_response_t *response_out) {
    if (!url || !response_buffer || !response_out) return -1;

    wchar_t host[256], path[1024];
    INTERNET_PORT port;
    BOOL is_https;

    if (parse_url(url, host, 256, path, 1024, &port, &is_https) != 0) return -1;

    return http_request(L"GET", host, port, path, is_https,
                        NULL, 0, NULL,
                        response_buffer, response_buffer_size,
                        response_out);
}

int http_post(const char *url, const uint8_t *request_body,
              size_t request_body_size, const char *content_type,
              uint8_t *response_buffer, size_t response_buffer_size,
              http_response_t *response_out) {
    if (!url || !response_buffer || !response_out) return -1;

    wchar_t host[256], path[1024];
    INTERNET_PORT port;
    BOOL is_https;

    if (parse_url(url, host, 256, path, 1024, &port, &is_https) != 0) return -1;

    wchar_t wctype[64] = {0};
    wchar_t *ctype = NULL;

    if (content_type && utf8_to_wide(content_type, wctype, 64))
        ctype = wctype;

    return http_request(L"POST", host, port, path, is_https,
                        request_body, request_body_size, ctype,
                        response_buffer, response_buffer_size,
                        response_out);
}
