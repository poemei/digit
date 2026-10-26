#ifndef RICTUS_TRANSPORT_H
#define RICTUS_TRANSPORT_H

#define RICTUS_TRANSPORT_CHANNEL_SEND_SERVICE "rictus.transport.channel.send"
#define RICTUS_TRANSPORT_PRIVATE_SEND_SERVICE "rictus.transport.private.send"

#define RICTUS_TRANSPORT_TARGET_MAX 128
#define RICTUS_TRANSPORT_MESSAGE_MAX 512

typedef struct {
    char message[RICTUS_TRANSPORT_MESSAGE_MAX];
} rictus_transport_channel_request_t;

typedef struct {
    char target[RICTUS_TRANSPORT_TARGET_MAX];
    char message[RICTUS_TRANSPORT_MESSAGE_MAX];
} rictus_transport_private_request_t;

#endif
