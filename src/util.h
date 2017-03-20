#ifndef _RPS_UTIL_H
#define _RPS_UTIL_H

#include "log.h"

#include "uv.h"

#include <stdlib.h>
#include <sys/un.h>
#include <sys/socket.h>

#define CRLF    "\x0d\x0a"

#define MAX(_a, _b)     ((_a) > (_b) ? (_a):(_b))
#define MIN(_a, _b)     ((_a) < (_b) ? (_a):(_b))

#define UNUSED(_x) (void)(_x)

#define rps_alloc(_s)                                               \
    _rps_alloc((size_t)(_s), __FILE__, __LINE__)                    \

#define rps_zalloc(_s)                                              \
    _rps_zalloc((size_t)(_s), __FILE__, __LINE__)                   \

#define rps_calloc(_n, _s)                                          \
    _rps_calloc((size_t)(_n), (size_t)(_s), __FILE__, __LINE__)     \

#define rps_realloc(_p, _s)                                         \
    _rps_realloc(_p, (size_t)(_s), __FILE__, __LINE__)              \

#define rps_free(_p)                                                \
    _rps_free(_p, __FILE__, __LINE__)                               \


void  *_rps_alloc(size_t size, const char *name, int line);
void * _rps_zalloc(size_t size, const char *name, int line);
void *_rps_calloc(size_t nmemb, size_t size, const char *name, int line);
void *_rps_realloc(void *ptr, size_t size, const char *name, int line);
void _rps_free(void *ptr, const char *name, int line);

typedef enum { false, true } bool;

#define UV_SHOW_ERROR(err, why) do {                                \
    log_debug("libuv %s:%s", why, uv_strerror(err));                 \
} while(0)

void _rps_assert(const char *cond, const char *file, int line);

#define NOT_REACHED() _rps_assert("not reached", __FILE__, __LINE__);                                      

#define ASSERT(_x) do {                                             \
    if (!(_x)) {                                                    \
        _rps_assert(#_x, __FILE__, __LINE__);                        \
    }                                                               \
} while(0)

#define MAX_HOSTNAME_LEN 255
#define MAX_INET_ADDRSTRLEN MAX_HOSTNAME_LEN
#define AF_DOMAIN 60 /* AF_INET is 2, AF_INET6 is 30, so we get 60 */

struct sockaddr_name {
    uint16_t    family;
    uint16_t    port;
    char        host[255];
};

typedef struct sockinfo {
    uint16_t        family;
    unsigned int    addrlen;
    union {
        struct sockaddr_in      in;
        struct sockaddr_in6     in6;
        struct sockaddr_un      un;
        struct sockaddr_name    name;
    } addr;
} rps_addr_t;

static inline bool 
rps_valid_port(uint16_t port) {
    return (port > 1 && port < UINT16_MAX);  
}

static inline void
rps_addrinfo(struct sockaddr *addr, struct sockinfo *info, unsigned int addrlen) {
    info->family = addr->sa_family;
    info->addrlen = addrlen;
    memcpy(&info->addr, addr, addrlen);
}


int rps_resolve_inet(const char *node, uint16_t port, rps_addr_t *si); 
int rps_unresolve_addr(rps_addr_t *addr, char *name);
uint16_t rps_unresolve_port(rps_addr_t *addr);
void rps_addr_in4(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port);
void rps_addr_in6(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port);
void rps_addr_name(rps_addr_t *addr, uint8_t *_addr, uint8_t len, uint8_t *port);

int rps_random(int max);

#endif

