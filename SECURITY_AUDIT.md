# thttpd security review — 27 November 2026

## Scope and assurance
Reviewed the supplied 2015-era thttpd C source, including core request parsing, authentication, path handling, directory listing and CGI helper programs. Performed source inspection, targeted Clang static analysis, compilation, and limited HTTP/CGI smoke tests. **This is not a certified or exhaustive audit; security cannot be guaranteed.** No internet-facing deployment, sustained fuzzing, formal verification, third-party dependency/CVE inventory, or production configuration assessment was performed.

## Changes incorporated (including earlier hardening pass)
- Replace overlapping `strcpy` operations with `memmove` in several request/path manipulation sites (undefined-behavior avoidance).
- Reject percent-encoded NUL bytes in URL processing, including before path interpretation.
- Guard percent decoder against incomplete escape sequences.
- Reject malformed decoded Basic credentials containing embedded NULs or invalid decoded lengths.
- Check password hashing failures and compute `crypt()` only once per authentication check.
- Guard against empty password-file lines before checking their final character.
- Bound redirect configuration parsing with width-limited `sscanf`; prevent redirect destination buffer overflow on wildcard suffix concatenation.
- Reject CR/LF in wildcard suffixes and configured redirect targets before emitting CGI Location headers.
- Avoid passing a null directory-name pointer to `qsort` for an empty directory.
- Free allocated `dirname` when a later allocation fails in the SSI helper.
- Prior pass includes additional string and build compatibility edits; see `security-full.patch` for exact changes.

## Verification performed
- GCC `make -j2` main server: PASS.
- GCC CGI targets `redirect` and `ssi`: PASS.
- Clang static analyzer invoked on 9 C translation units. It flagged possible issues in `libhttpd.c` stream handling and allocation error paths; these require further investigation. Some static analyzer findings were fixed; the analyzer is not proof of absence of bugs.
- HTTP smoke tests on localhost: `/` -> 200, `/%00` -> 400, `/%2e%2e/etc/passwd` -> 404, `/missing` -> 404.
- Redirect CGI smoke tests: exact-match and wildcard redirects produced expected Location headers.

## Open risks / release blockers
1. **Unverified request parsing and filesystem safety:** Need systematic traversal, symlink, virtual-host, CGI, authentication, header, HTTP pipelining and large-request tests.
2. **CGI helpers and SSI:** Further scrutiny needed for HTML escaping, filesystem permissions, untrusted environment variables and script execution boundaries.
3. **Resource exhaustion:** Need large-input, high-concurrency and slow-client fuzz/load tests; memory limits and integer bounds need additional verification.
4. **Remaining Clang findings:** Potential stream-state issues in `libhttpd.c` and allocation-path cleanup; triage and reproduce before marking fixed.
5. **TLS and architecture:** thttpd is an old, minimalist HTTP server. HTTPS termination, process sandboxing, privilege dropping, least-privilege file access and modern HTTP request smuggling protections require deployment-level review.
6. **Build/test matrix:** Only the available Linux toolchain was used. Sanitizer runs, multiple architectures, CI regression suite and coverage-guided fuzzing remain undone.

## Recommended next steps
Do not deploy this build on the public internet based on this review alone. Run AddressSanitizer/UndefinedBehaviorSanitizer and libFuzzer/AFL++ harnesses on HTTP parsing and CGI; add regression tests for auth, encoded traversal, redirects, SSI and concurrency; review deployment config and update/replace outdated components where feasible.
