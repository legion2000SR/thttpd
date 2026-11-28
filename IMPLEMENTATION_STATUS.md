# Implementation status — thttpd 2.26-L2K

This document describes the implementation state in this source snapshot. A checked item means the relevant code was inspected; it does **not** imply comprehensive security or runtime testing.

## Optional OpenSSL HTTPS frontend (new)

- [x] `./configure --with-openssl` conditionally builds the separate `thttpd-tls` executable linked to OpenSSL; the default and `--without-openssl` builds remain OpenSSL-free.
- [x] The TLS frontend supports TLS 1.2+ and PEM certificate/key loading, with bounded nonblocking relay buffers and a 30-second I/O timeout.
- [x] Local Linux smoke tests: HTTP and HTTPS static GET returned HTTP 200; TLS 1.2 and TLS 1.3 handshakes succeeded; IPv6 frontend listener accepted connections.
- [x] The HTTPS frontend drops privileges when started as root; the backend retains its existing `-u` support.
- [ ] Native in-process TLS integration in `thttpd`: **not implemented**. HTTPS is provided by a separate executable, not the legacy event loop.
- [ ] HTTPS-specific CGI environment and redirect correctness: **not implemented**. CGI/SSI are forwarded as HTTP bytes but cannot reliably detect HTTPS. See `TLS_README.md`.
- [ ] Extended TLS security review, load/fuzz testing, portability, and complete IPv6 HTTPS request verification.

## Verified in the bundled source

- [x] **Privilege-drop hardening:** when started as root, `initgroups(user, gid)` is called before `chroot()` and a failure is fatal. `setgid()` and `setuid()` failures are fatal, and the resulting effective UID/GID are checked. The existing `-u` user-selection mechanism remains in place.
- [x] **Directory listings:** the listing loop in `libhttpd.c` skips dot-prefixed entries as documented in `TODO`.
- [x] **Nonblocking file open:** `mmc.c` opens files with `O_RDONLY | O_NONBLOCK`.
- [x] **TCP_NODELAY:** `thttpd.c` applies `TCP_NODELAY` after `httpd_start_request()`.
- [x] **Basic authentication cache metadata:** the single-entry cache compares password-file change time, size, inode, device, and modification time. This is a partial hardening measure, not a complete cache/race redesign.

## Not verified as complete / outstanding

- [ ] IPv6: dual-stack end-to-end testing and edge cases.
- [ ] Virtual hosts: host mapping, numeric-address/no-Host cases, and symlink boundary validation.
- [ ] CGI and PATH_INFO: authorization and execution review, including the ACME News issue.
- [ ] Authentication: end-to-end tests and full cache/race redesign.
- [ ] Permissions: parent-directory traversal, symlink behaviour, and race-condition review.
- [ ] HTTP keep-alive: persistent connection handling and request framing.
- [ ] Stat cache: implementation with safe invalidation.
- [ ] Tasklets: event-driven architecture rewrite.
- [ ] Remaining unchecked entries in `TODO`: no completion claim is made for these.

## Verification limitations

This snapshot includes Linux builds with and without OpenSSL and a basic loopback HTTP/HTTPS smoke test. It does **not** include comprehensive privileged runtime tests, sanitizer runs, fuzzing, TLS load tests, or platform portability testing. Previous build claims should not be interpreted as verification of all outstanding features.

Do not treat this snapshot as independently validated for public-facing production deployment.

## Compile-time abuse mitigation (2026-11-28)

- [x] Added `ddos_config.h` to allow operators to set limits before compiling.
- [x] HTTP admission: bounded per-IP table, connection cap, rolling 60-second fixed-window counter, temporary cooldown, and global connection cap.
- [x] HTTP: configurable header-read inactivity timeout and request-header buffer ceiling.
- [x] Optional OpenSSL frontend: per-IP admission, bounded child-process count, temporary cooldown, and TLS handshake deadline.
- [x] Compiles on Linux with `./configure --with-openssl` and `./configure --without-openssl`.
- [ ] No HTTP 429 response yet: rejected connections are closed.
- [ ] No comprehensive adversarial/load testing; not a substitute for upstream DDoS protection.

The rate setting is per accepted TCP connection (one HTTP request per connection in the legacy server), not a sophisticated per-URL/application-layer limiter. TLS frontend limits apply independently of the backend limits. Defaults are editable and enabled in `ddos_config.h`; setting `DDOS_ENABLED` to `0` removes the added checks at compile time. These features are not claimed to stop large-scale DDoS attacks.
