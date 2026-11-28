# Optional OpenSSL HTTPS for thttpd 2.26-L2K

## Architecture

OpenSSL is **disabled by default**. `--with-openssl` builds a separate
`thttpd-tls` HTTPS frontend. It accepts TLS 1.2/1.3 connections and relays
HTTP bytes to an independently running thttpd process bound to **127.0.0.1**.
This intentionally does **not** modify the old event loop, CGI subprocesses,
SSI, or libhttpd's socket code. HTTPS is provided by the bundled frontend,
not by a native TLS socket inside the thttpd executable.

## Build

```sh
./configure --without-openssl  # default: HTTP only
make
# OR
./configure --with-openssl     # builds thttpd and thttpd-tls
make
```

`--with-openssl` requires OpenSSL development headers and libssl/libcrypto.
Run `make distclean` before switching configurations if needed.

## Run

Create a certificate and private key (for local testing only):

```sh
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -subj '/CN=localhost' -keyout /tmp/thttpd.key -out /tmp/thttpd.crt
```

In one terminal, start the **HTTP backend** on loopback only:

```sh
./thttpd -D -h 127.0.0.1 -p 8080 -d /srv/www -u nobody
```

In another, start the HTTPS frontend:

```sh
./thttpd-tls -C /tmp/thttpd.crt -K /tmp/thttpd.key -p 8443 -b 8080 -l :: -u nobody
curl -k https://localhost:8443/
```

Use a trusted certificate for deployment. Port 443 requires a privileged
bind or suitable service capability. Certificate/key files are loaded before
any root-to-user privilege drop. The frontend defaults to `nobody` when
started as root; non-root invocations retain their existing identity.

## Security and limitations

- Keep the backend on **127.0.0.1**, not a public interface. The TLS frontend
  connects only to IPv4 loopback. The frontend can listen on IPv4 or IPv6.
- Each TLS client uses a forked worker (up to 128 concurrent children),
  nonblocking SSL and bounded 32 KiB buffers per direction. There is a
  30-second inactivity timeout for handshakes and transfers.
- The frontend does not add or trust `X-Forwarded-*` headers. The HTTP backend
  does **not** know that the original client used HTTPS; CGI `HTTPS` variables,
  redirects, and absolute URL generation may therefore reflect HTTP rather
  than HTTPS. CGI/SSI data is transported, but HTTPS-specific CGI semantics
  are **not** implemented. Do not use this mode for applications that require
  a trustworthy HTTPS environment variable or secure-cookie detection.
- This is not HTTP/2, SNI virtual-host certificate selection, mutual TLS,
  OCSP stapling, or a full-featured reverse proxy.
- TLS is limited to TLS 1.2+; actual TLS 1.3 support depends on OpenSSL.
- Audit and load-test before public deployment; this frontend is newly added.
