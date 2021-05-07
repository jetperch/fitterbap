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

/**
 * @file
 *
 * @brief Provide a simple command line interface.
 */

#ifndef FBP_FBP_CLI_H_
#define FBP_FBP_CLI_H_

#include "fitterbap/platform.h"

/**
 * @ingroup fbp_core
 * @defgroup fbp_cli Command line interface
 *
 * @brief A simple command line interface.
 *
 * @{
 */

FBP_CPP_GUARD_START

/**
 * @brief The supported character echo modes.
 */
enum fbp_cli_echo_mode_e {
    FBP_CLI_ECHO_OFF,           ///< No echo.
    FBP_CLI_ECHO_ON,            ///< Echo each character.
    FBP_CLI_ECHO_USER_CHAR      ///< Echo a user-specified character.
};

/**
 * @brief The supported verbose levels.
 */
enum fbp_cli_verbose_mode_e {
    FBP_CLI_VERBOSE_NORMAL,     ///< Display details and results.
    FBP_CLI_VERBOSE_FULL,       ///< Echo command just before result.
};

/**
 * @brief The command line status.
 */
enum fbp_cli_status_e {
    FBP_CLI_SUCCESS_PROMPT_ONLY = -1,
    FBP_CLI_SUCCESS = 0,
    FBP_CLI_ERROR = 1,
    FBP_CLI_ERROR_PARAMETER_COUNT = 2,
    FBP_CLI_ERROR_PARAMETER_VALUE = 3
};

/**
 * @brief The maximum line length.
 */
#define FBP_CLI_LINE_LENGTH 64

/**
 * @brief The maximum number of characters in the prompt.
 */
#define FBP_CLI_PROMPT_LENGTH 16

/**
 * @brief The maximum number of command-line arguments.
 */
#define FBP_CLI_MAX_ARGS 16

/**
 * @brief The command to execute for each line.
 *
 * @param cookie The execute cookie.
 * @param cmdline The command line to handle.
 * @return FBP_CLI_SUCCESS, FBP_CLI_SUCCESS_PROMPT_ONLY or error code.
 */
typedef int (*fbp_cli_execute_line)(void * cookie, const char * cmdline);

/**
 * @brief The command to execute for each line.
 *
 * @param cookie The execute cookie.
 * @param argc The number of arguments including the command.
 * @param argv The list of arguments.  argv[0] is the command.
 * @return FBP_CLI_SUCCESS, FBP_CLI_SUCCESS_PROMPT_ONLY or error code.
 */
typedef int (*fbp_cli_execute_args)(void * cookie, int argc, char *argv[]);

/**
 * @brief The CLI instance structure.
 */
typedef struct fbp_cli_s {
    /**
     * @brief The command line echo mode.
     */
    enum fbp_cli_echo_mode_e echo_mode;

    /**
     * @brief The echo character for echo_mode FBP_CLI_ECHO_USER_CHAR.
     */
    char echo_user_char;

    /**
     * @brief The text to display following a command execution that
     *      returned FBP_CLI_SUCCESS.
     */
    char const * response_success;

    /**
     * @brief The text to display following a command execution that
     *      returned an error code.
     */
    char const * response_error;

    /**
     * @brief The prompt to display after each command.
     */
    char prompt[FBP_CLI_PROMPT_LENGTH];

    /**
     * @brief The command line buffer for the current command.
     */
    char cmdline[FBP_CLI_LINE_LENGTH + 2];

    /**
     * @brief The current size of cmdline in total_bytes.
     */
    fbp_size_t cmdlen;

    /**
     * @brief The command to execute for each line.
     *
     * If execute_args is specified, set execute_line to NULL.
     */
    fbp_cli_execute_line execute_line;

    /**
     * @brief The command to execute for parsed arguments.
     *
     * If execute_line is specified, set execute_args to NULL.  When execute_args
     * is not NULL, any value provided to execute_line will be ignored.
     */
    fbp_cli_execute_args execute_args;

    /**
     * @brief The user data passed to the execute function.
     */
    void * execute_cookie;

    /**
     * @brief The function called to print to the console.
     *
     * @param[in] cookie The user data provided at configuration time.
     * @param[in] str The string to print.
     */
    void (*print)(void * cookie, const char * str);

    /**
     * @brief The user data provided to each print call.
     */
    void * print_cookie;

    /**
     * @brief The command line verboseness level.
     */
    enum fbp_cli_verbose_mode_e verbose;

    /**
     * @brief Hold internal state for managing line endings.
     */
    char last_char;
} fbp_cli_t;

/**
 * @brief Initialize a CLI instance.
 *
 * @param self The instance to initialize.  Be sure to initialize all fields
 *      or memset the structure to zero.
 *
 * This function may be called repeatedly on the same instance.
 */
FBP_API void fbp_cli_initialize(fbp_cli_t * self);

/**
 * @brief Set the echo mode.
 *
 * @param self The CLI instance.
 * @param mode The echo mode.
 * @param ch The user echo character which is only necessary for
 *      FBP_CLI_ECHO_USER_CHAR.  All other modes should pass 0.
 */
FBP_API void fbp_cli_set_echo(fbp_cli_t * self, enum fbp_cli_echo_mode_e mode, char ch);

/**
 * @brief Set the verbose level.
 *
 * @param self The CLI instance.
 * @param mode The verbose level.
 */
FBP_API void fbp_cli_set_verbose(fbp_cli_t * self, enum fbp_cli_verbose_mode_e mode);

/**
 * @brief Insert the next character in the CLI.
 *
 * @param self The CLI instance.
 * @param ch The next character entered by the user.
 */
FBP_API void fbp_cli_insert_char(fbp_cli_t * self, char ch);

/**
 * @brief The default command line parser used to parse a command line into
 *      arguments.
 *
 * @param self The CLI instance passed as void to be compatible with the
 *      execute_line function pointer.
 * @param cmdline The command line string to parse.
 * @return FBP_CLI_SUCCESS, FBP_CLI_SUCCESS_PROMPT_ONLY or error code.
 *
 * Calling this function will call execute_args on success.
 */
FBP_API int fbp_cli_line_parser(void * self, const char * cmdline);

FBP_CPP_GUARD_END

/** @} */

#endif /* FBP_FBP_CLI_H_ */
