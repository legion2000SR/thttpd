# thttpd — consolidated changelog from original source ZIP

**Comparison:** `thttpd-master.zip` (original supplied archive) → `thttpd-auth-cache-incremental.zip` (latest development archive).

**Date:** 27 November 2026. This report was derived from a direct comparison of the two ZIP archives, not just prior conversation summaries. All source changes are included in the accompanying unified patch. The latest archive also contains generated build artifacts; these are not intentional source-code changes.

Original ZIP SHA-256: `0cbea5e126c977d0730e5b38bccd3a388e4be67feddfbab445cab70cf338a236`  
Latest ZIP SHA-256: `6988262f91de6edaeea9f531699be81744e6018948052dcbaefc8366ee536a89`

## Summary

- HTTP request framing validation, encoded-NUL rejection and hostname validation were tightened.
- Basic authentication input checks and password-cache invalidation were strengthened.
- Overlapping string copies were changed to `memmove` in request and configuration paths.
- CGI redirect bounds, header handling and directory redirect behavior were changed.
- Directory listings, file mapping and certain platform compatibility paths were hardened.
- Documentation and generated build files were also changed.

## Detailed source-code changes

### 1. `libhttpd.c` — HTTP parsing and URL processing
- Added `<stdint.h>` for `SIZE_MAX`.
- Reject raw NUL bytes in the request buffer.
- Reject `%00` encoded NULs in URLs before decoding.
- Check that percent escapes have both hexadecimal digits before decoding.
- Reject obsolete folded headers, header lines without a colon, and invalid header-name characters.
- Reject `Transfer-Encoding` requests rather than treating unsupported chunked bodies as ordinary requests.
- Reject duplicate `Content-Length` fields; require a valid unsigned decimal value with range checking rather than using `atol`.
- Reject duplicate `Host` fields. Validate hostname characters, length, dot placement, and optional numeric port suffixes. Validate bracketed IPv6 literals with `inet_pton` when IPv6 is compiled in.
- Apply hostname validation to absolute-form request targets as well as Host header values.
- Replace overlapping `strcpy` calls with `memmove` during CGI pattern normalization, URL/path cleanup, and IPv4-mapped address text normalization.

### 2. `libhttpd.c` — Basic authentication
- Reject invalid or zero-length Base64 decoded credentials and embedded decoded NUL bytes.
- Check `crypt()` results for NULL before comparing password hashes; compute the hash once per check.
- Avoid indexing before the start of an empty password-file line.
- Extend cached-password-file identity checks from modification time to also include change time, file size, inode and device; store these fields on successful authentication.
- **Limit:** This remains a single-entry authentication cache and is not a complete race-resistant authentication redesign.

### 3. `libhttpd.c` — directory listing, CGI and portability
- Omit dot-prefixed names from automatically generated directory listings, except `..`.
- Avoid calling `qsort` unless at least two directory entries exist.
- Restrict the CGI descriptor close-on-exec workaround to Linux.
- Replace a legacy `sigset` call with `signal` in a CGI-related signal reset.
- Replace an unbounded `vsprintf` fallback with `vsnprintf`.
- Guard IPv4-mapped IPv6 address handling with an address-family check.

### 4. `thttpd.c` — main server
- Replace overlapping string copies in log-path handling and throttle-pattern normalization with `memmove`.
- Replace `sigset` calls with `signal` in the relevant compile-time branch for modern Linux compatibility.
- Include `<netinet/tcp.h>` and enable `TCP_NODELAY` on a connection after request dispatch to reduce response latency.
- **Important:** `TCP_NODELAY` is not HTTP keep-alive; persistent connection management has not been implemented.

### 5. `mmc.c` — memory-mapped file handling
- Open files using `O_RDONLY | O_NONBLOCK`.
- After opening, call `fstat` and reject non-regular files or files whose device, inode or size differs from the prior `stat` result.
- Close the descriptor on validation failure.
- **Limit:** This is a targeted race-hardening measure, not a full race-free filesystem permissions architecture.

### 6. `cgi-src/redirect.c` — CGI redirect helper
- Add a `Refresh: 0; url=...` response header alongside the existing redirect header.
- Apply width limits to redirect-map `sscanf` fields.
- Reject CR/LF characters in redirect targets and wildcard suffixes before constructing response headers.
- Bound wildcard suffix concatenation to prevent exceeding the redirect destination buffer.
- Append `index.html` to configured redirect destinations ending in `/` when there is sufficient space.
- **Behavior change:** The redirect helper now sends an additional `Refresh` header and directory targets are rewritten to an index filename.

### 7. `cgi-src/ssi.c` — server-side includes
- Free `dirname` when allocation of `authname` fails, closing an allocation-error memory leak.

### 8. `config.h`, `README`, `INSTALL`
- `config.h`: clarify that `INDEX_NAMES` entries must be simple filenames without slash or `..` components. This is a comment change, not a runtime validator.
- `README`: document Basic authentication behavior for symlinks and resolved target directories.
- `INSTALL`: replaced or regenerated generic Autotools installation instructions; this is documentation/tooling churn rather than a server security fix.

## File-by-file change counts

These counts cover the compared tracked source/document files, not autogenerated build outputs.

| File | Added lines | Removed lines |
|---|---:|---:|
| `libhttpd.c` | 169 | 25 |
| `thttpd.c` | 21 | 11 |
| `mmc.c` | 15 | 1 |
| `cgi-src/redirect.c` | 20 | 3 |
| `cgi-src/ssi.c` | 3 | 1 |
| `config.h` | 2 | 2 |
| `README` | 4 | 0 |
| `INSTALL` | 357 | 360 |

## Build and test evidence from development history

- Earlier development passes reported successful GCC builds of the main server and CGI helpers.
- Earlier targeted HTTP checks covered normal GET, encoded NULs, traversal attempts, malformed framing, and hostname handling.
- The previous local penetration test sent 122 targeted requests and did not observe a secret disclosure or server crash in those tests.
- The latest authentication-cache change was compiled, but its own status report explicitly says it has **not** received dedicated authentication integration testing, sanitizer testing or fuzzing.
- The direct archive comparison for this changelog does not itself establish that the latest binary is secure.

## Requested features still incomplete

| Feature | Status in latest archive |
|---|---|
| IPv6 | Partial validation fixes; no comprehensive dual-stack verification |
| Virtual hosts | Partial Host validation; mapping and symlink boundaries not fully verified |
| HTTP keep-alive | Not implemented |
| CGI | Targeted improvements only; comprehensive authorization/execution review outstanding |
| Authentication | Targeted input/cache improvements; full redesign and integration tests outstanding |
| Permissions redesign | Not implemented |
| Stat caching | Not implemented |
| Tasklet rewrite | Not implemented |

## Generated and non-source differences

The latest ZIP contains build/configuration outputs such as `Makefile`, `Makefile.in`, `configure`, `config.status`, `config.log`, `aclocal.m4`, `.deps`, object files, executables, and `.plist` files. These may reflect regeneration, compiler output or the environment used to build the project. They are **not** evidence that additional runtime features were implemented. The archive also adds security review documents and tests from intermediate work. The original upstream `ChangeLog` is not itself the consolidated record of this development session.

## Release assessment

**Not production certified.** The original-to-latest patch documents the changed source lines exactly. A complete audit still requires extensive sanitizer/fuzzer coverage, CGI and authentication integration testing, IPv6/virtual-host boundary tests, and architectural implementation and validation of the outstanding requested features.
