# Implementation status — thttpd 2.26-L2K

This document describes the implementation state in this source snapshot. A checked item means the relevant code was inspected; it does **not** imply comprehensive security or runtime testing.

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

This update reconciles the status document with the bundled `TODO` and inspected C source. It does **not** include a new build, privileged runtime test, sanitizer run, fuzzing campaign, or platform portability test. Previous build claims should not be interpreted as verification of all outstanding features.

Do not treat this snapshot as independently validated for public-facing production deployment.
