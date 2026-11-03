#ifndef DIGIT_WEB_TRANSPORT_H
#define DIGIT_WEB_TRANSPORT_H
/* Outbound HTTP transport contract for the ChAoS MVC /chat module.
 * HTTP POST /chat/transport {"type":"poll"} => 204 idle OR
 * 200 {"id":"...","message":"..."} pending.
 * HTTP POST /chat/transport {"type":"reply","id":"...",
 * "status":"ok"|"error","message":"..."} => 200 acknowledged.
 * The web frontend must not expose this transport operation to visitors.
 */
#define DIGIT_WEB_TRANSPORT_VERSION "1.0.1"
#endif
