# thttpd security review update — 27 November 2026

## Scope and limits
This is a further targeted hardening pass on the previously patched thttpd tree, not an exhaustive security certification. Review focused on the HTTP request parser, informed by prior local penetration testing. Existing fixes from SECURITY_AUDIT.md are retained. A complete audit of every code path, configuration, platform, CGI script, dependency, and runtime deployment has **not** been performed. No claim of zero vulnerabilities is made.

## Newly addressed
- Reject NUL bytes in raw HTTP request bytes rather than silently truncating header fields.
- Reject unsupported Transfer-Encoding request framing (including chunked), instead of accepting it without decoding.
- Reject duplicate Content-Length fields, negative/non-numeric/overflowing values and trailing junk; use checked unsigned parsing rather than atol().
- Reject obsolete folded headers and malformed header field names.

## Tests run
- `./configure && make -j2` on Linux/GCC: PASS.
- Local loopback tests: normal GET 200; duplicate Content-Length 400; chunked Transfer-Encoding 400; embedded NUL 400; negative Content-Length 400; folded header 400; encoded traversal did not expose a secret (404). See `audit-tests/test_http.py`.

## Outstanding risks and release blockers
- The server is an old HTTP implementation; a comprehensive source audit and independent security assessment are still necessary.
- HTTP parsing still needs protocol conformance review (e.g., request-line strictness, header limits, line endings, conflicting Host headers, proxy interactions).
- Full sanitizer-enabled tests, coverage-guided fuzzing, sustained concurrency/slow-client testing, CGI execution tests, authentication and authorization boundary tests, symlink race tests, and platform matrix have not been completed.
- Existing static analyzer warnings noted in SECURITY_AUDIT.md need triage.
- Reverse-proxy request smuggling and production configuration cannot be ruled out by these standalone loopback tests.
- Do not treat this release as safe for internet exposure without further independent review and a hardened deployment.

## Coding approach
Changes are small, localized and deliberately follow the existing C style. They are ordinary maintainable security fixes, with comments where the reason is not obvious. No claim is made about human authorship.
