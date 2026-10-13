#ifndef DIGIT_LOGGING_H
#define DIGIT_LOGGING_H

#define DIGIT_INSTALL_PATH "/opt/digit"
#define DIGIT_LOG_DIRECTORY "./logs"
#define DIGIT_LOG_PATH "./logs/digit.log"
#define DIGIT_MODULE_LOG_PATH "./logs/modules.log"

int digit_log_open(void);
int digit_log_open_path(const char *absolute_path); /* tests only */
int digit_log_event(const char *component, const char *event, const char *detail);
int digit_log_sync(void);
void digit_log_close(void);
int digit_log_is_open(void);

#endif
