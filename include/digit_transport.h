#ifndef DIGIT_TRANSPORT_H
#define DIGIT_TRANSPORT_H

#define DIGIT_TRANSPORT_CHANNEL_SEND_SERVICE "digit.transport.channel.send"
#define DIGIT_TRANSPORT_PRIVATE_SEND_SERVICE "digit.transport.private.send"

#define DIGIT_TRANSPORT_TARGET_MAX 128
#define DIGIT_TRANSPORT_MESSAGE_MAX 512

typedef struct {
    char message[DIGIT_TRANSPORT_MESSAGE_MAX];
} digit_transport_channel_request_t;

typedef struct {
    char target[DIGIT_TRANSPORT_TARGET_MAX];
    char message[DIGIT_TRANSPORT_MESSAGE_MAX];
} digit_transport_private_request_t;

#endif
