#ifndef DIGIT_INTEL_DELIVERY_H
#define DIGIT_INTEL_DELIVERY_H
#include <stddef.h>
#define DIGIT_INTEL_SLACK_SERVICE "slack.intel.publish"
#define DIGIT_INTEL_IRC_SERVICE "irc.intel.publish"
#define DIGIT_INTEL_DELIVERY_VERSION 1U
#define DIGIT_INTEL_MESSAGE_MAX 4096
typedef struct {
 unsigned int version;
 char message[DIGIT_INTEL_MESSAGE_MAX];
} digit_intel_delivery_t;
#endif
