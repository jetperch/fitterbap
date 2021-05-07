/*
 * Copyright 2014-2021 Jetperch LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * References:
 *
 * VT-100 terminal (not currently supported)
 *     - http://vt100.net/docs/vt100-ug/
 *     - http://www.termsys.demon.co.uk/vtansi.htm
 *     - http://misc.flogisoft.com/bash/tip_colors_and_formatting
 *     - https://developer.mbed.org/cookbook/VT100-Terminal
 */

#include "fitterbap/cli.h"
#include "fitterbap/dbc.h"
#include "fitterbap/platform.h"


const char LINE_TOO_LONG[] = "Maximum command line length reached";


/* http://www.asciitable.com/ */
#define KEY_NUL  0
#define KEY_BS   8
#define KEY_DEL  127
#define KEY_TAB  9
#define KEY_LF  10
#define KEY_CR  13
#define KEY_ESC 27


static inline void fbp_cli_print(fbp_cli_t * self, char const * str) {
    if (self->print) {
        self->print(self->print_cookie, str);
    }
}

static inline void print_prompt(fbp_cli_t * self) {
    fbp_cli_print(self, self->prompt);
}

static inline void print_char(fbp_cli_t * self, char ch) {
    char s[2];
    s[0] = ch;
    s[1] = 0;
    fbp_cli_print(self, s);
}

static inline void print_newline(fbp_cli_t * self) {
    print_char(self, KEY_LF);
}

static void fbp_cli_backspace(fbp_cli_t * self) {
    if (self->cmdlen <= 0) {
        return;
    }
    if (self->cmdlen >= FBP_CLI_LINE_LENGTH) {
        self->cmdlen--;
    } else if (self->cmdlen > 0) {
        self->cmdline[self->cmdlen - 1] = '\0';
        self->cmdlen--;
    }
    if (self->echo_mode != FBP_CLI_ECHO_OFF) {
        fbp_cli_print(self, "\b \b");
    }
}

static void fbp_cli_process_char(fbp_cli_t * self, char ch) {
    if (self->cmdlen >= FBP_CLI_LINE_LENGTH) {
        self->cmdlen++;
    } else {
        self->cmdline[self->cmdlen] = ch;
        self->cmdlen++;
        self->cmdline[self->cmdlen] = '\0';
    }
    switch (self->echo_mode) {
        case FBP_CLI_ECHO_OFF: break;
        case FBP_CLI_ECHO_ON: print_char(self, ch); break;
        case FBP_CLI_ECHO_USER_CHAR: print_char(self, self->echo_user_char); break;
        default: break;
    }
}

static bool isWhiteSpace(char ch) {
    return ((ch == ' ') || (ch == KEY_TAB));
}

static bool isCommentStart(char const * s) {
    if (NULL == s) {
        return false;
    }
    if ((*s == '#') || (*s == '@') || (*s == '%')) {
        return true;
    }
    return false;
}

static void fbp_cli_compact(fbp_cli_t * self) {
    fbp_size_t i = 0;
    int offset = 0;
    bool isWhite = true;
    for (i = 0; i < self->cmdlen; ++i) {
        if (i >= FBP_CLI_LINE_LENGTH) {
            // line too long - do not compact.
            break;
        }
        char ch = self->cmdline[i];
        if (isCommentStart(&ch)) { /* start of a comment */
            break; /* ignore the rest of the line. */
        } else if (isWhiteSpace(ch)) {
            if (!isWhite) {
                self->cmdline[offset] = ' ';
                ++offset;
            }
            isWhite = true;
        } else {
            self->cmdline[offset] = ch;
            ++offset;
            isWhite = false;
        }
    }
    if (offset && isWhiteSpace(self->cmdline[offset - 1])) {
        self->cmdline[offset - 1] = '\0';
        --offset;
    }
    self->cmdlen = offset;
    self->cmdline[self->cmdlen] = '\0';
}

static void fbp_cli_process_line(fbp_cli_t * self) {
    print_newline(self);
    fbp_cli_compact(self);
    if (self->cmdlen == 0) {
        // empty line or only comment
    } else if (self->cmdlen >= FBP_CLI_LINE_LENGTH) {
        fbp_cli_print(self, LINE_TOO_LONG);
        print_newline(self);
    } else { // valid command
        int rc = FBP_CLI_SUCCESS;
        if (self->execute_line) {
            rc = self->execute_line(self->execute_cookie, self->cmdline);
        }
        if (rc != FBP_CLI_SUCCESS_PROMPT_ONLY) {
            if (self->verbose == FBP_CLI_VERBOSE_FULL) {
                fbp_cli_print(self, self->cmdline);
                print_newline(self);
            }
            fbp_cli_print(self, (FBP_CLI_SUCCESS == rc) ? self->response_success : self->response_error);
        }
    }
    self->cmdline[0] = '\0';
    self->cmdlen = 0;
    print_prompt(self);
}

void fbp_cli_initialize(fbp_cli_t * self) {
    FBP_DBC_NOT_NULL(self);
    fbp_memset(self->cmdline, 0, sizeof(self->cmdline));
    self->cmdlen = 0;
    if (self->execute_args) {
        self->execute_line = fbp_cli_line_parser;
        self->execute_cookie = self;
    }
    print_prompt(self);
}

void fbp_cli_set_echo(fbp_cli_t * self, enum fbp_cli_echo_mode_e mode, char ch) {
    FBP_DBC_NOT_NULL(self);
    self->echo_mode = mode;
    self->echo_user_char = ch;
}

void fbp_cli_set_verbose(fbp_cli_t * self, enum fbp_cli_verbose_mode_e mode) {
    FBP_DBC_NOT_NULL(self);
    self->verbose = mode;
}

void fbp_cli_insert_char(fbp_cli_t * self, char ch) {
    FBP_DBC_NOT_NULL(self);
    switch (ch) {
        case KEY_BS:   fbp_cli_backspace(self); break;
        case KEY_DEL:  fbp_cli_backspace(self); break;
        case KEY_LF:
            if (self->last_char != KEY_CR) {
                fbp_cli_process_line(self);
            }
            break;
        case KEY_CR:   fbp_cli_process_line(self); break;
        default:       fbp_cli_process_char(self, ch); break;
    }
    self->last_char = ch;
}

static bool _is_delimiter(char ch, const char * delim) {
    while (*delim) {
        if (ch == *delim++) {
            return true;
        }
    }
    return false;
}

int fbp_cli_line_parser(void * self, const char * cmdline) {
    FBP_DBC_NOT_NULL(self);
    fbp_cli_t const * p = (fbp_cli_t const *) self;
    const char delimiters[] = " \t,";
    char line[FBP_CLI_LINE_LENGTH + 2];
    int argc = 0;
    char * argv[FBP_CLI_MAX_ARGS];
    char * lineptr = line;
    fbp_memset(&argv, 0, sizeof(argv));
    fbp_memcpy(line, cmdline, sizeof(line));
    line[sizeof(line) - 1] = 0; // just to be sure.
    while (1) {
        // consume multiple delimiters
        while (*lineptr && _is_delimiter(*lineptr, delimiters)) {
            ++lineptr;
        }
        if (*lineptr == 0) {
            break;
        }
        if (argc >= FBP_CLI_MAX_ARGS) {
            return FBP_CLI_ERROR_PARAMETER_VALUE;
        }
        argv[argc++] = lineptr; // start of token
        // consume token
        while (*lineptr && !_is_delimiter(*lineptr, delimiters)) {
            ++lineptr;
        }
        if (*lineptr == 0) {
            break;
        }
        *lineptr++ = 0; // force token break
    }

    if (argc == 0) { // blank line!
        return FBP_CLI_SUCCESS;
    } else if (p->execute_args) {
        return p->execute_args(p->execute_cookie, argc, argv);
    } else { // not really valid...
        return FBP_CLI_ERROR_PARAMETER_VALUE;
    }
}
