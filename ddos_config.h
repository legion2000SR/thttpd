/* Compile-time DDoS mitigation settings. Edit before ./configure && make.
 * This is application-level abuse mitigation, not volumetric DDoS filtering.
 */
#ifndef THTTPD_DDOS_CONFIG_H
#define THTTPD_DDOS_CONFIG_H
#define DDOS_ENABLED 1
#define DDOS_MAX_CONNECTIONS_PER_IP 20
#define DDOS_MAX_REQUESTS_PER_MINUTE 60
#define DDOS_MAX_TOTAL_CONNECTIONS 512
#define DDOS_HEADER_TIMEOUT_SECONDS 10
#define DDOS_TLS_HANDSHAKE_TIMEOUT_SECONDS 5
#define DDOS_MAX_HEADER_BYTES 16384
#define DDOS_TEMPORARY_BLOCK_SECONDS 60
#define DDOS_TRACKED_IPS 1024
#endif
