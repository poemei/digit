#ifndef DIGIT_IRC_INTERNAL_H
#define DIGIT_IRC_INTERNAL_H

#include <stddef.h>

#define DIGIT_IRC_ENV_PATH "/opt/digit/auth/irc/irc.env"
#define DIGIT_IRC_SERVER_MAX 256
#define DIGIT_IRC_NICK_MAX 64
#define DIGIT_IRC_CHANNEL_MAX 128
#define DIGIT_IRC_ACCOUNT_MAX 128
#define DIGIT_IRC_PASSWORD_MAX 256
#define DIGIT_IRC_LINE_MAX 4096

typedef struct {
    char server[DIGIT_IRC_SERVER_MAX];
    unsigned short port;
    char nick[DIGIT_IRC_NICK_MAX];
    char channel[DIGIT_IRC_CHANNEL_MAX];
    char account[DIGIT_IRC_ACCOUNT_MAX];
    char password[DIGIT_IRC_PASSWORD_MAX];
} digit_irc_config_t;

int digit_irc_config_load(const char *path, digit_irc_config_t *config);
void digit_irc_config_clear(digit_irc_config_t *config);

int digit_irc_line_command(const char *line, char *command, size_t command_size,
                            const char **params_out);
int digit_irc_word_present(const char *text, const char *word);
int digit_irc_build_plain(const char *account, const char *password,
                           char *output, size_t output_size);
int digit_irc_parse_privmsg(const char *line,
                             char *sender, size_t sender_size,
                             char *target, size_t target_size,
                             char *message, size_t message_size);
int digit_irc_run_selftests(unsigned int *executed, unsigned int *passed,
                             int *negative_executed, int *negative_passed);

#endif
