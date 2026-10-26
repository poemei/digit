#ifndef RICTUS_IRC_INTERNAL_H
#define RICTUS_IRC_INTERNAL_H

#include <stddef.h>

#define RICTUS_IRC_ENV_PATH "/opt/rictus/auth/irc/irc.env"
#define RICTUS_IRC_SERVER_MAX 256
#define RICTUS_IRC_NICK_MAX 64
#define RICTUS_IRC_CHANNEL_MAX 128
#define RICTUS_IRC_ACCOUNT_MAX 128
#define RICTUS_IRC_PASSWORD_MAX 256
#define RICTUS_IRC_LINE_MAX 4096

typedef struct {
    char server[RICTUS_IRC_SERVER_MAX];
    unsigned short port;
    char nick[RICTUS_IRC_NICK_MAX];
    char channel[RICTUS_IRC_CHANNEL_MAX];
    char account[RICTUS_IRC_ACCOUNT_MAX];
    char password[RICTUS_IRC_PASSWORD_MAX];
} rictus_irc_config_t;

int rictus_irc_config_load(const char *path, rictus_irc_config_t *config);
void rictus_irc_config_clear(rictus_irc_config_t *config);

int rictus_irc_line_command(const char *line, char *command, size_t command_size,
                            const char **params_out);
int rictus_irc_word_present(const char *text, const char *word);
int rictus_irc_build_plain(const char *account, const char *password,
                           char *output, size_t output_size);
int rictus_irc_parse_privmsg(const char *line,
                             char *sender, size_t sender_size,
                             char *target, size_t target_size,
                             char *message, size_t message_size);
int rictus_irc_run_selftests(unsigned int *executed, unsigned int *passed,
                             int *negative_executed, int *negative_passed);

#endif
