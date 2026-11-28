/* Optional OpenSSL HTTPS frontend for thttpd 2.26-L2K.
 * Separate process and loopback-only HTTP backend keep the legacy HTTP/CGI
 * socket paths unchanged.  BSD-style license, see COPYING.
 */
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ddos_config.h"
#include <time.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define BUFSZ 32768
#define TIMEOUT_MS 30000
static int io_timeout_ms = TIMEOUT_MS;
#define MAX_CHILDREN 128
static volatile sig_atomic_t children = 0;
#if DDOS_ENABLED
typedef struct { pid_t pid; char ip[64]; } child_entry;
static child_entry active_children[MAX_CHILDREN];
typedef struct { char ip[64]; time_t window, blocked_until; unsigned int count; } ip_entry;
static ip_entry ip_table[DDOS_TRACKED_IPS];
static int allow_ip(const char* ip, time_t now) {
    int i, empty = -1, active = 0; ip_entry* e = NULL;
    for (i = 0; i < MAX_CHILDREN; ++i)
        if (active_children[i].pid && strcmp(active_children[i].ip, ip) == 0) ++active;
    for (i = 0; i < DDOS_TRACKED_IPS; ++i) {
        if (strcmp(ip_table[i].ip, ip) == 0) { e = &ip_table[i]; break; }
        if (empty < 0 && (ip_table[i].ip[0] == 0 ||
            (ip_table[i].blocked_until <= now && ip_table[i].window + 120 < now))) empty = i;
    }
    if (!e && empty >= 0) {
        e = &ip_table[empty]; memset(e, 0, sizeof *e);
        snprintf(e->ip, sizeof e->ip, "%s", ip); e->window = now;
    }
    if (!e || e->blocked_until > now) return 0;
    if (now < e->window || now - e->window >= 60) { e->window = now; e->count = 0; }
    if (active >= DDOS_MAX_CONNECTIONS_PER_IP || e->count >= DDOS_MAX_REQUESTS_PER_MINUTE) {
        e->blocked_until = now + DDOS_TEMPORARY_BLOCK_SECONDS; return 0;
    }
    ++e->count; return 1;
}
#endif
static void reap(int unused) {
    int saved = errno, status;
    (void)unused;
    { pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (children > 0) --children;
#if DDOS_ENABLED
        { int i; for (i = 0; i < MAX_CHILDREN; ++i)
            if (active_children[i].pid == pid) { active_children[i].pid = 0; break; } }
#endif
    }
    }
    errno = saved;
}
static int nonblock(int fd) {
    int f = fcntl(fd, F_GETFL, 0);
    return f >= 0 && fcntl(fd, F_SETFL, f | O_NONBLOCK) == 0;
}
static int wait_for(int fd, short events) {
    struct pollfd p = {fd, events, 0};
    int r;
    do { r = poll(&p, 1, io_timeout_ms); } while (r < 0 && errno == EINTR);
    return r > 0 && (p.revents & (events | POLLERR | POLLHUP));
}
static int dial_backend(const char* port) {
    struct addrinfo hints, *res, *a;
    int fd = -1, err;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET; hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
    if (getaddrinfo("127.0.0.1", port, &hints, &res) != 0) return -1;
    for (a = res; a; a = a->ai_next) {
        int code; socklen_t len = sizeof code;
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        if (!nonblock(fd)) { close(fd); fd = -1; continue; }
        err = connect(fd, a->ai_addr, a->ai_addrlen);
        if (err == 0 || (err < 0 && errno == EINPROGRESS &&
            wait_for(fd, POLLOUT) && getsockopt(fd, SOL_SOCKET, SO_ERROR, &code, &len) == 0 && code == 0))
            break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}
static int handshake(SSL* ssl, int fd) {
#if DDOS_ENABLED
    time_t deadline = time(NULL) + DDOS_TLS_HANDSHAKE_TIMEOUT_SECONDS;
#endif
    for (;;) {
#if DDOS_ENABLED
        time_t remaining = deadline - time(NULL);
        if (remaining <= 0) return 0;
        io_timeout_ms = (int)(remaining * 1000);
#endif
        int r = SSL_accept(ssl), e;
        if (r == 1) return 1;
        e = SSL_get_error(ssl, r);
        if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE) return 0;
        if (!wait_for(fd, e == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT)) return 0;
    }
}
/* Full duplex with bounded queues, nonblocking SSL and socket operations.
 * No attempt to parse or rewrite HTTP: the backend retains responsibility
 * for framing, CGI, authentication, and responses. */
static void relay(SSL* ssl, int tlsfd, int backend) {
    unsigned char inbound[BUFSZ], outbound[BUFSZ];
    size_t inlen = 0, outlen = 0;
    int tls_eof = 0, backend_eof = 0, in_wait = 0, out_wait = 0;
    for (;;) {
        struct pollfd p[2];
        short te = 0, be = 0;
        int n;
        if (tls_eof && inlen == 0 && backend_eof && outlen == 0) break;
        if (backend_eof && outlen == 0) break;
        if (!tls_eof && !backend_eof && inlen < BUFSZ && !out_wait) te |= in_wait == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN;
        if (outlen && !in_wait) te |= out_wait == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT;
        if (!backend_eof && outlen < BUFSZ && !out_wait) be |= POLLIN;
        if (inlen && !in_wait) be |= POLLOUT;
        p[0].fd = tlsfd; p[0].events = te; p[0].revents = 0;
        p[1].fd = backend; p[1].events = be; p[1].revents = 0;
        do { n = poll(p, 2, (!out_wait && !backend_eof && !tls_eof && inlen < BUFSZ && SSL_pending(ssl) > 0) ? 0 : TIMEOUT_MS); } while (n < 0 && errno == EINTR);
        if (SSL_pending(ssl) > 0 && !out_wait && !backend_eof && !tls_eof && inlen < BUFSZ) { p[0].revents |= POLLIN; if (n == 0) n = 1; }
        if (n <= 0) break;
        if (outlen && !in_wait && (p[0].revents & (out_wait == SSL_ERROR_WANT_READ ? POLLIN : POLLOUT))) {
            int r = SSL_write(ssl, outbound, (int)outlen);
            if (r > 0) { memmove(outbound, outbound + r, outlen - (size_t)r); outlen -= (size_t)r; out_wait = 0; }
            else { int e = SSL_get_error(ssl, r); if (e != SSL_ERROR_WANT_READ && e != SSL_ERROR_WANT_WRITE) break; out_wait = e; }
        }
        if (inlen && !in_wait && (p[1].revents & POLLOUT)) {
            ssize_t r = send(backend, inbound, inlen, MSG_NOSIGNAL);
            if (r > 0) { memmove(inbound, inbound + r, inlen - (size_t)r); inlen -= (size_t)r; }
            else if (r < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
        }
        if (!tls_eof && !backend_eof && !out_wait && inlen < BUFSZ &&
            (p[0].revents & (in_wait == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN))) {
            int r = SSL_read(ssl, inbound + inlen, (int)(BUFSZ - inlen));
            if (r > 0) { inlen += (size_t)r; in_wait = 0; }
            else {
                int e = SSL_get_error(ssl, r);
                if (e == SSL_ERROR_ZERO_RETURN) { tls_eof = 1; if (!inlen) shutdown(backend, SHUT_WR); }
                else if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) in_wait = e;
                else break;
            }
        }
        if (!backend_eof && !out_wait && outlen < BUFSZ && (p[1].revents & (POLLIN | POLLHUP))) {
            ssize_t r = recv(backend, outbound + outlen, BUFSZ - outlen, 0);
            if (r > 0) outlen += (size_t)r;
            else if (r == 0) backend_eof = 1;
            else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
        }
        if (tls_eof && inlen == 0) shutdown(backend, SHUT_WR);
        if (p[0].revents & (POLLERR | POLLNVAL)) break;
        if (p[1].revents & (POLLERR | POLLNVAL)) break;
    }
}
static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s -C cert.pem -K key.pem [-p 8443] [-b 8080] [-l ::] [-u nobody]\n"
            "Run thttpd separately bound to 127.0.0.1 on backend port (-b).\n", prog);
}
static int listener(const char* host, const char* port) {
    struct addrinfo hints, *res, *a;
    int fd = -1, on = 1;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(host, port, &hints, &res) != 0) return -1;
    for (a = res; a; a = a->ai_next) {
        fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (fd < 0) continue;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof on);
#ifdef IPV6_V6ONLY
        if (a->ai_family == AF_INET6) { on = 0; setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof on); on = 1; }
#endif
        if (bind(fd, a->ai_addr, a->ai_addrlen) == 0 && listen(fd, 128) == 0) break;
        close(fd); fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}
int main(int argc, char** argv) {
    const char *cert = NULL, *key = NULL, *port = "8443", *backend_port = "8080", *host = "::", *user = "nobody";
    int opt, fd, rc = 1;
    SSL_CTX* ctx;
    struct sigaction sa;
    while ((opt = getopt(argc, argv, "C:K:p:b:l:u:h")) != -1) {
        switch (opt) {
        case 'C': cert = optarg; break;
        case 'K': key = optarg; break;
        case 'p': port = optarg; break;
        case 'b': backend_port = optarg; break;
        case 'l': host = optarg; break;
        case 'u': user = optarg; break;
        default: usage(argv[0]); return 2;
        }
    }
    if (!cert || !key || optind != argc) { usage(argv[0]); return 2; }
    signal(SIGPIPE, SIG_IGN);
    ctx = SSL_CTX_new(TLS_server_method());
    if (!ctx) { ERR_print_errors_fp(stderr); return 1; }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION);
    if (SSL_CTX_use_certificate_chain_file(ctx, cert) != 1 ||
        SSL_CTX_use_PrivateKey_file(ctx, key, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(ctx) != 1) {
        ERR_print_errors_fp(stderr); goto done;
    }
    fd = listener(host, port);
    if (fd < 0) { perror("TLS listen"); goto done; }
    if (geteuid() == 0) {
        struct passwd *pw = getpwnam(user);
        if (!pw || initgroups(user, pw->pw_gid) != 0 || setgid(pw->pw_gid) != 0 ||
            setuid(pw->pw_uid) != 0 || geteuid() != pw->pw_uid || getegid() != pw->pw_gid) {
            fprintf(stderr, "TLS frontend: unable to drop privileges to %s\n", user);
            close(fd); goto done;
        }
    } else if (user && strcmp(user, "nobody") != 0 &&
               (!getpwuid(geteuid()) || strcmp(getpwuid(geteuid())->pw_name, user) != 0)) {
        fprintf(stderr, "TLS frontend: cannot switch users without root privileges\n");
        close(fd); goto done;
    }
    memset(&sa, 0, sizeof sa); sa.sa_handler = reap; sigemptyset(&sa.sa_mask); sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
    fprintf(stderr, "TLS frontend listening on %s:%s, HTTP backend 127.0.0.1:%s\n", host, port, backend_port);
    for (;;) {
        struct sockaddr_storage peer; socklen_t peer_len = sizeof peer;
        char ip[64] = "";
        int client = accept(fd, (struct sockaddr*)&peer, &peer_len);
        pid_t pid;
        if (client < 0) { if (errno == EINTR) continue; perror("accept"); break; }
#if DDOS_ENABLED
        if (peer.ss_family == AF_INET)
            inet_ntop(AF_INET, &((struct sockaddr_in*)&peer)->sin_addr, ip, sizeof ip);
        else if (peer.ss_family == AF_INET6)
            inet_ntop(AF_INET6, &((struct sockaddr_in6*)&peer)->sin6_addr, ip, sizeof ip);
        if (!ip[0] || !allow_ip(ip, time(NULL))) {
            fprintf(stderr, "TLS frontend: rate-limited connection from %s\n", ip);
            close(client); continue;
        }
#endif
        if (children >= MAX_CHILDREN
#if DDOS_ENABLED
            || children >= DDOS_MAX_TOTAL_CONNECTIONS
#endif
            ) { close(client); continue; }
        pid = fork();
        if (pid < 0) { perror("fork"); close(client); continue; }
        if (pid == 0) {
            SSL* ssl;
            int backend;
            close(fd);
            signal(SIGCHLD, SIG_DFL);
            if (!nonblock(client)) _exit(1);
            ssl = SSL_new(ctx);
            if (!ssl) _exit(1);
            SSL_set_fd(ssl, client);
#if DDOS_ENABLED
            io_timeout_ms = DDOS_TLS_HANDSHAKE_TIMEOUT_SECONDS * 1000;
#endif
            if (handshake(ssl, client)) {
                io_timeout_ms = TIMEOUT_MS;
                backend = dial_backend(backend_port);
                if (backend >= 0) { relay(ssl, client, backend); close(backend); }
            }
            SSL_free(ssl); close(client); _exit(0);
        }
        ++children;
#if DDOS_ENABLED
        { int i; for (i = 0; i < MAX_CHILDREN; ++i)
            if (!active_children[i].pid) { active_children[i].pid = pid;
                snprintf(active_children[i].ip, sizeof active_children[i].ip, "%s", ip); break; } }
#endif
        close(client);
    }
    close(fd);
done:
    SSL_CTX_free(ctx);
    return rc;
}
