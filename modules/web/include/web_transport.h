#ifndef DIGIT_WEB_TRANSPORT_H
#define DIGIT_WEB_TRANSPORT_H
/* Native C <-> PHP local socket protocol v1.
 * Request: BE32 payload length + UTF-8 request text.
 * Response: U8 status (0=success,1=error) + BE32 length + UTF-8 text.
 * Socket path and peer authorization are server-only details.
 * The existing digit.router.dispatch service caps replies at 511 bytes.
 */
#define DIGIT_WEB_TRANSPORT_VERSION "1.0.0"
#endif
