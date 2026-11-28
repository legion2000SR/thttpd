# Compile-time abuse mitigation (L2K)

Edit `ddos_config.h` **before building**. `DDOS_ENABLED` defaults to `1`; set to `0` to compile without the new admission checks. All settings are C preprocessor constants. Rebuild after changes (`make clean && make`).

| Define | Default | Meaning |
| --- | ---: | --- |
| DDOS_MAX_CONNECTIONS_PER_IP | 20 | Concurrent TCP connections per source IP |
| DDOS_MAX_REQUESTS_PER_MINUTE | 60 | Accepted connections per IP per fixed 60-second window (one request per connection for legacy HTTP) |
| DDOS_MAX_TOTAL_CONNECTIONS | 512 | Overall HTTP active connections / TLS child ceiling (TLS frontend also retains its internal MAX_CHILDREN=128 cap) |
| DDOS_HEADER_TIMEOUT_SECONDS | 10 | HTTP reading inactivity timeout |
| DDOS_TLS_HANDSHAKE_TIMEOUT_SECONDS | 5 | TLS handshake deadline |
| DDOS_MAX_HEADER_BYTES | 16384 | Maximum HTTP read buffer bytes during header reception |
| DDOS_TEMPORARY_BLOCK_SECONDS | 60 | Cooldown after per-IP admission violation |
| DDOS_TRACKED_IPS | 1024 | Maximum per-process tracked IPs; new IPs are rejected when table is full |

The plain HTTP server and the optional TLS frontend maintain **separate** in-memory counters. The TLS frontend counts incoming connections rather than parsing encrypted HTTP requests. Both use the peer socket address, not `X-Forwarded-For`. IPv4-mapped IPv6 addresses may be represented differently than IPv4 addresses; normalize upstream if this matters. Rejections close the socket without a 429 response. Counters reset on restart; fixed windows are not distributed across replicas. In the HTTP server, the read timeout is inactivity-based, not a strict whole-header deadline. These are basic resource-abuse controls, not protection against a saturated network link or distributed botnet.

Build HTTP-only: `./configure --without-openssl && make`

Build optional HTTPS: `./configure --with-openssl && make`

Do not expose the TLS backend port publicly; bind the backend to loopback. Test against real traffic and tune limits for NAT/shared-IP clients before production deployment. No exhaustive stress/fuzz/security testing is claimed.
