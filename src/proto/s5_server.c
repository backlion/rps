
#include "s5.h"
#include "core.h"
#include "util.h"
#include "log.h"
#include "server.h"

#include <stdio.h>
#include <uv.h>



static s5_err_t 
s5_parse(s5_handle_t *handle, uint8_t **data, ssize_t *size) {
    s5_err_t err;
    uint8_t *p;
    uint8_t c;
    size_t i;
    size_t n;

    i = 0;
    n = *size; 
    p = *data;

    while(i < n) {
        c = p[i];
        i++;

        printf("read: %x\n", c);
        
        switch (handle->state) {
            case s5_version:
                if (c != SOCKS5_VERSION) {
                    err = s5_bad_version;
                    goto out;
         
                }
                handle->version = c;
                handle->state = s5_nmethods;
                break;

            case s5_nmethods:
                handle->nmethods = c;
                handle->__n = 0;
                handle->state = s5_methods;
                break;

            case s5_methods:
                if (handle->__n < handle->nmethods) {
                    switch (c) {
                        case 0:
                            handle->methods[handle->__n] = s5_auth_none;
                            break;
                        case 1:
                            handle->methods[handle->__n] = s5_auth_gssapi;
                            break;
                        case 2:
                            handle->methods[handle->__n]= s5_auth_passwd;
                            break;
                        default:
                            /* Unsupport auth method */
                            break;
                    }
                    handle->__n++;
                }
                break;
        }
    }

    err = s5_ok;

out:
    *data = p + i;
    *size = n - i;  
    return err;  
    
}

static uint8_t
s5_select_auth(struct s5_method_request *req) {
    int i;
    uint8_t method;

    ASSERT(req->nmethods <= 255);

    /* Select no authentication required if rps server didn't set user and password */

    for (i=0; i<req->nmethods; i++) {
        method = req->methods[i];
        if (method == s5_auth_none) {
            return s5_auth_none;
        }
    }

    return s5_auth_unacceptable;
}

static uint16_t
s5_do_handshake(struct context *ctx, uint8_t *data) {
    uint16_t new_state;
    struct server *s;
    struct s5_method_request *req;
    struct s5_method_response resp;
    rps_status_t status;

    req = (struct s5_method_request *)data;
    if (req->ver != SOCKS5_VERSION) {
        log_error("s5 handshake error: bad protocol version.");
        return c_kill;
    }

    resp.ver = req->ver;

    s = ctx->sess->server;
    
    if (string_empty(&s->cfg->username) && string_empty(&s->cfg->password)) {
        /* If rps didn't assign username and password, 
         * select auth method dependent on client request 
         * */
        resp.method = s5_select_auth(req);
    } else {
        resp.method = s5_auth_passwd;
    }

    status = server_write(ctx, &resp, sizeof(resp));
    if (status != RPS_OK) {
        return c_kill;
    }

    switch (resp.method) {
        case s5_auth_none:
            new_state = c_requests;
            break;
        case s5_auth_passwd:
            new_state = c_auth;
            break;
        case s5_auth_gssapi:
        case s5_auth_unacceptable:
            new_state = c_kill;
            log_error("s5 handshake error: unacceptable authentication.");
            break;
    }

#ifdef RPS_DEBUG_OPEN
    log_verb("s5 handshake finish.");
#endif

    return new_state;
}

static uint16_t
s5_do_auth(struct context *ctx, uint8_t *data) {
    uint16_t new_state;
    struct server *s;
    struct s5_auth_request *req;
    struct s5_auth_response resp;
    rps_status_t status;

    req = (struct s5_auth_request *)data;
    if (req->ver != SOCKS5_AUTH_PASSWD_VERSION) {
        log_error("s5 handshake error: bad password auth version.");
        return c_kill;
    } 

    /* Reset the req struct memory layout */
    req->plen = req->uname[req->ulen];
    req->uname[req->ulen] = '\0';
    memcpy(req->passwd, &req->uname[req->ulen+1], req->plen);
    req->passwd[req->plen] = '\0';

    ASSERT(strlen((const char *)req->uname) == req->ulen);
    ASSERT(strlen((const char *)req->passwd) == req->plen);

    s = ctx->sess->server;

    resp.ver = SOCKS5_AUTH_PASSWD_VERSION;
    if (rps_strcmp(s->cfg->username.data, req->uname) == 0 && 
        rps_strcmp(s->cfg->password.data, req->passwd) == 0) {
        resp.status = s5_auth_allow;
        new_state = c_requests;
    } else {
        resp.status = s5_auth_deny;
        new_state = c_kill;
    }

    status = server_write(ctx, &resp, sizeof(resp));
    if (status != RPS_OK) {
        return c_kill;
    }

#ifdef RPS_DEBUG_OPEN
    if (resp.status ==  s5_auth_allow) {
        log_verb("s5 username password authentication success.");
    } else {
        log_verb("s5 username password authentication failed.");
    }
#endif

    return new_state;
}

static uint16_t
s5_do_request(struct context *ctx, uint8_t *data) {
    uint16_t new_state;
    uint8_t len;
    struct s5_request *req;
    int err;
    rps_addr_t  remote;

    req = (struct s5_request *)data;
    if (req->ver != SOCKS5_VERSION) {
        log_error("s5 request error: bad protocol version.");
        return c_kill;
    }

    /* tcp bind and udp association are not be supported */
    if (req->cmd != s5_cmd_tcp_connect) {
        log_error("s5 request error: only support tcp connect verify.");
        return c_kill;
    }

    remote = ctx->sess->remote;
	
    switch (req->atyp) {
        case s5_atyp_ipv4:
			memcpy(req->dport, &req->daddr[4], 2);
            rps_addr_in4(&remote, req->daddr, 4, req->dport);
			break;

		case s5_atyp_ipv6:
			memcpy(req->dport, &req->daddr[16], 2);
            rps_addr_in6(&remote, req->daddr, 16, req->dport);
            break;

		case s5_atyp_domain:
            /* First byte is hostname length */
            len = req->daddr[0]; 
            /* Last 2 byte is dport */
			memcpy(req->dport, &req->daddr[len+1], 2);	
            rps_addr_name(&remote, &req->daddr[1], len, req->dport);
			break;

        default:
            /* Address type not supported */
            return c_kill;
    }

    char remoteip[MAX_INET_ADDRSTRLEN];

    err = rps_unresolve_addr(&remote, remoteip);
    if (err < 0) {
        return c_kill;
    }
    
    log_debug("remote %s:%d\n", remoteip, rps_unresolve_port(&remote));

}

static s5_state_t
s5_reply() {

}

void 
s5_server_do_next(struct context *ctx) {
    uint8_t    *data;
    ssize_t size;
    uint16_t new_state; 
    s5_handle_t *handle;

    handle = &ctx->proxy_handle.s5;

    data = (uint8_t *)ctx->buf;
    size = ctx->nread;

#if RPS_DEBUG_OPEN
    int i;
    log_verb("read %zd bytes: ", size);
    for (i=0; i<size; i++) {
        log_verb("\t%x ", data[i]);
    }
#endif

    switch (ctx->state) {
        case c_handshake:
            new_state = s5_do_handshake(ctx, data);
            break;
        case c_auth:
            new_state = s5_do_auth(ctx, data);
            break;
        case c_requests:
            new_state = s5_do_request(ctx, data);
            break;
        default:
            NOT_REACHED();
			new_state = c_kill;
    }
    
    ctx->state = new_state;
}
