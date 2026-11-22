#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_json.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

/* Existing Gateway JSON transport, with explicit borrowed owner inputs. */
static long long monotonic_millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int tcp_connect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
        close(fd);
        return -1;
    }
    return fd;
}

static int samosa_send_all(int fd, const void *data, size_t size) {
    const char *cursor=(const char *)data;
    while (size) {
#ifdef MSG_NOSIGNAL
        ssize_t n=send(fd,cursor,size,MSG_NOSIGNAL);
#else
        ssize_t n=send(fd,cursor,size,0);
#endif
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return 0;
        cursor+=n; size-=(size_t)n;
    }
    return 1;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} TextBuffer;

static int text_reserve(TextBuffer *buffer, size_t extra) {
    if (extra > SIZE_MAX - buffer->len - 1) return 0;
    size_t needed = buffer->len + extra + 1;
    if (needed <= buffer->cap) return 1;
    size_t cap = buffer->cap ? buffer->cap : 4096;
    while (cap < needed) {
        if (cap > SIZE_MAX / 2) { cap = needed; break; }
        cap *= 2;
    }
    char *next = realloc(buffer->data, cap);
    if (!next) return 0;
    buffer->data = next; buffer->cap = cap;
    return 1;
}

static int text_add_n(TextBuffer *buffer, const char *text, size_t length) {
    if (!text_reserve(buffer, length)) return 0;
    memcpy(buffer->data + buffer->len, text, length);
    buffer->len += length; buffer->data[buffer->len] = 0;
    return 1;
}

static void backend_receive_timeout(int fd, int seconds) {
    struct timeval timeout = {.tv_sec = seconds, .tv_usec = 0};
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

static void trace_payload(const SamosaBackendJson *r, const char *event,
                          const char *source, const char *data, size_t bytes) {
    if (r->trace_payload) r->trace_payload(r->context,event,source,data,bytes);
}
static void trace_event(const SamosaBackendJson *r, const char *event, const char *fields) {
    if (r->trace_event) r->trace_event(r->context,event,fields);
}

static int complete_body(const TextBuffer *response, const char *separator) {
    if (memchr(response->data, 0, response->len)) return 0;
    size_t bytes = response->len - (size_t)(separator + 4 - response->data);
    const char *line = strchr(response->data, '\n');
    int seen = 0;
    while (line && ++line < separator) {
        const char *end = strstr(line, "\r\n");
        if (!end || end > separator) return 0;
        if (!strncasecmp(line, "Transfer-Encoding:", 18)) return 0;
        if (!strncasecmp(line, "Content-Length:", 15)) {
            if (seen++) return 0;
            const char *value = line + 15;
            while (value < end && (*value == ' ' || *value == '\t')) value++;
            if (value == end || *value < '0' || *value > '9') return 0;
            size_t length = 0;
            for (; value < end && *value >= '0' && *value <= '9'; value++) {
                unsigned digit = (unsigned)(*value - '0');
                if (length > (SIZE_MAX - digit) / 10) return 0;
                length = length * 10 + digit;
            }
            while (value < end && (*value == ' ' || *value == '\t')) value++;
            if (value != end || length != bytes) return 0;
        }
        line = end + 1;
    }
    return 1;
}

char *samosa_backend_json(const SamosaBackendJson *request, const char *payload) {
    if (!request || !payload || !request->probe || !request->gate ||
        !request->mutex || !request->upstream_fd) return NULL;
    int timeout_seconds = request->timeout_seconds;
    long long started = monotonic_millis();
    trace_payload(request, "internal_model_request", "chat_backend",
                            payload ? payload : "", payload ? strlen(payload) : 0);
    if (!request->probe(request->context)) {
        trace_event(request, "internal_model_error",
                              "\"code\":\"backend_loading\"");
        return NULL;
    }
    int fd = tcp_connect(request->port);
    if (fd < 0) {
        trace_event(request, "internal_model_error",
                              "\"code\":\"backend_connect_failed\"");
        return NULL;
    }
    if (!samosa_turn_primary_begin(request->gate)) {
        close(fd);
        trace_event(request, "internal_model_error",
                              "\"code\":\"multimodal_specialist_active\"");
        return NULL;
    }
    /* Planner/ranker/sufficiency calls are internal control turns. They must
       fail closed after a bounded wait instead of holding a web request until
       a broken local backend eventually notices a dead socket. */
    backend_receive_timeout(fd, timeout_seconds);
    pthread_mutex_lock(request->mutex);
    int cancelled = request->cancelled && request->cancelled(request->context);
    if (!cancelled) *request->upstream_fd = fd;
    pthread_mutex_unlock(request->mutex);
    if (cancelled) {
        samosa_turn_primary_end(request->gate);
        close(fd);
        return NULL;
    }
    char header[512];
    int n = snprintf(header, sizeof(header),
        "POST /v1/chat/completions HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
        "Content-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        request->port, strlen(payload));
    if (n <= 0 || (size_t)n >= sizeof(header) ||
        !samosa_send_all(fd, header, (size_t)n) ||
        !samosa_send_all(fd, payload, strlen(payload))) {
        pthread_mutex_lock(request->mutex); if (*request->upstream_fd == fd) *request->upstream_fd = -1; pthread_mutex_unlock(request->mutex);
        samosa_turn_primary_end(request->gate); close(fd);
        trace_event(request, "internal_model_error",
                              "\"code\":\"request_write_failed\"");
        return NULL;
    }
    TextBuffer response = {0}; char chunk[65536];
    int complete = 0;
    long long deadline = started + timeout_seconds * 1000LL;
    while (response.len < SAMOSA_BACKEND_JSON_MAX_BODY + SAMOSA_BACKEND_JSON_MAX_HEADER) {
        long long remaining = deadline - monotonic_millis();
        if (remaining <= 0) { free(response.data); memset(&response, 0, sizeof(response)); break; }
        backend_receive_timeout(fd, (int)((remaining + 999) / 1000));
        ssize_t got = recv(fd, chunk, sizeof(chunk), 0);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { complete = got == 0; break; }
        if (!text_add_n(&response, chunk, (size_t)got)) break;
    }
    pthread_mutex_lock(request->mutex); if (*request->upstream_fd == fd) *request->upstream_fd = -1; pthread_mutex_unlock(request->mutex);
    samosa_turn_primary_end(request->gate); close(fd);
    if (response.data)
        trace_payload(request, "internal_model_response", "chat_backend_http",
                                response.data, response.len);
    if (request->require_complete && (!complete || !response.data ||
        (strncmp(response.data, "HTTP/1.1 200 ", 13) &&
         strncmp(response.data, "HTTP/1.0 200 ", 13)))) {
        free(response.data);
        return NULL;
    }
    if (!response.data || !strstr(response.data, " 200 ")) {
        char fields[128];
        snprintf(fields, sizeof(fields), "\"code\":\"non_200_response\",\"duration_ms\":%lld",
                 monotonic_millis() - started);
        trace_event(request, "internal_model_error", fields);
        free(response.data); return NULL;
    }
    char *body = strstr(response.data, "\r\n\r\n");
    if (!body) { free(response.data); return NULL; }
    if (request->require_complete && !complete_body(&response, body)) {
        free(response.data);
        return NULL;
    }
    body += 4;
    char *copy = strdup(body);
    free(response.data); return copy;
}
