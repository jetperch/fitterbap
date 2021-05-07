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

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "fitterbap/cli.h"
#include "fitterbap.h"
#include "fitterbap/memory/buffer.h"
#include <string.h>


fbp_cli_t cli;
FBP_BUFFER_STATIC_DEFINE(myprint_str, 256);


static void myprint(void * cookie, const char * s) {
    (void) cookie;
    fbp_buffer_write(&myprint_str, s, strlen(s));
}

static int myexec(void * cookie, const char * cmdline) {
    (void) cookie;
    check_expected_ptr(cmdline);
    return mock_type(int32_t);
}

static int exec_args1(void * cookie, int argc, char *argv[]) {
    (void) cookie;
    check_expected(argc);
    check_expected_ptr(argv[0]);
    return mock_type(int32_t);
}

static int exec_args4(void * cookie, int argc, char *argv[]) {
    (void) cookie;
    check_expected(argc);
    check_expected_ptr(argv[0]);
    check_expected_ptr(argv[1]);
    check_expected_ptr(argv[2]);
    check_expected_ptr(argv[3]);
    return mock_type(int32_t);
}

int setup(void ** state) {
    (void) state;
    memset(&cli, 0, sizeof(cli));
    cli.print = myprint;
    cli.execute_line = myexec;
    cli.response_success = "OK\n";
    cli.response_error = "ERROR\n";
    fbp_buffer_clear(&myprint_str);
    fbp_cli_initialize(&cli);
    fbp_buffer_clear(&myprint_str);
    return 0;
}

void insert_str(const char * str) {
    while (str && *str) {
        fbp_cli_insert_char(&cli, *str++);
    }
}

void initialize(void ** state) {
    (void) state;
    assert_int_equal(FBP_CLI_ECHO_OFF, cli.echo_mode);
    assert_int_equal('\0', cli.echo_user_char);
    assert_string_equal("", cli.prompt);
    assert_int_equal(0, cli.cmdlen);
}

void execute_single_char(void ** state) {
    (void) state;
    fbp_cli_insert_char(&cli, 'h');
    expect_string(myexec, cmdline, "h");
    will_return(myexec, FBP_CLI_SUCCESS);
    fbp_cli_insert_char(&cli, '\r');
    assert_string_equal("\nOK\n", myprint_str.data);
}

void execute_FBP_CLI_SUCCESS(void ** state) {
    (void) state;
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello world!\r");
    assert_string_equal("\nOK\n", myprint_str.data);
}

void execute_backspace(void ** state) {
    (void) state;
    expect_string(myexec, cmdline, "hello world");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello world!\b\r");
    assert_string_equal("\nOK\n", myprint_str.data);
}

void execute_echo_on(void ** state) {
    (void) state;
    fbp_cli_set_echo(&cli, FBP_CLI_ECHO_ON, 0);
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello world!\r");
    assert_string_equal("hello world!\nOK\n", myprint_str.data);
}

void execute_FBP_CLI_SUCCESS_with_prompt_and_echo(void ** state) {
    (void) state;
    strcpy(cli.prompt, "PROMPT> ");
    cli.echo_mode = FBP_CLI_ECHO_ON;
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello world!\r");
    assert_string_equal("hello world!\nOK\nPROMPT> ", myprint_str.data);
}

void execute_echo_user_char(void ** state) {
    (void) state;
    fbp_cli_set_echo(&cli, FBP_CLI_ECHO_USER_CHAR, '*');
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello world!\r");
    assert_string_equal("************\nOK\n", myprint_str.data);
}

void execute_error(void ** state) {
    (void) state;
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_ERROR_PARAMETER_VALUE);
    insert_str("hello world!\r");
    assert_string_equal("\nERROR\n", myprint_str.data);
}

void execute_whitespace(void ** state) {
    (void) state;
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("    hello    \t world!   \r");
    assert_string_equal("\nOK\n", myprint_str.data);
}

void execute_whitespace_verbose(void ** state) {
    (void) state;
    fbp_cli_set_verbose(&cli, FBP_CLI_VERBOSE_FULL);
    expect_string(myexec, cmdline, "hello world!");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("    hello    \t world!   \r");
    assert_string_equal("\nhello world!\nOK\n", myprint_str.data);
}

void comment(void ** state) {
    (void) state;
    cli.prompt[0] = '>';
    fbp_cli_set_verbose(&cli, FBP_CLI_VERBOSE_FULL);
    insert_str("  #  hello    \t world!   \r");
    assert_string_equal("\n>", myprint_str.data);
}

void comment_too_long(void ** state) {
    (void) state;
    cli.prompt[0] = '>';
    insert_str(" # this is a very long comment which exceeds the maximum line length for commands\r");
    assert_string_equal("\n>", myprint_str.data);
}

void command_too_long(void ** state) {
    (void) state;
    cli.prompt[0] = '>';
    insert_str("this is a very long command which exceeds the maximum line length for commands\r");
    assert_string_equal("\nMaximum command line length reached\n>", myprint_str.data);
}

void two_commands(void ** state) {
    (void) state;
    cli.prompt[0] = '>';
    expect_string(myexec, cmdline, "hello");
    will_return(myexec, FBP_CLI_SUCCESS);
    expect_string(myexec, cmdline, "world");
    will_return(myexec, FBP_CLI_SUCCESS);
    insert_str("hello\rworld\r");
    assert_string_equal("\nOK\n>\nOK\n>", myprint_str.data);
}

void line_parser_empty(void ** state) {
    (void) state;
    assert_int_equal(0, fbp_cli_line_parser(&cli, ""));
    assert_int_equal(0, fbp_cli_line_parser(&cli, "    \t  "));
}

void line_parser_command_only(void ** state) {
    (void) state;
    cli.execute_args = exec_args1;
    expect_value(exec_args1, argc, 1);
    expect_string(exec_args1, argv[0], "mycommand");
    will_return(exec_args1, FBP_CLI_SUCCESS);
    assert_int_equal(0, fbp_cli_line_parser(&cli, "mycommand"));
}

void line_parser_command_only_with_whitespace(void ** state) {
    (void) state;
    cli.execute_args = exec_args1;
    expect_value(exec_args1, argc, 1);
    expect_string(exec_args1, argv[0], "mycommand");
    will_return(exec_args1, FBP_CLI_SUCCESS);
    assert_int_equal(0, fbp_cli_line_parser(&cli, "    mycommand   \t "));
}

void line_parser_with_args(void ** state) {
    (void) state;
    cli.execute_args = exec_args4;
    expect_value(exec_args4, argc, 4);
    expect_string(exec_args4, argv[0], "mycommand");
    expect_string(exec_args4, argv[1], "1");
    expect_string(exec_args4, argv[2], "2");
    expect_string(exec_args4, argv[3], "my_string");
    will_return(exec_args4, FBP_CLI_SUCCESS);
    assert_int_equal(0, fbp_cli_line_parser(&cli, "    mycommand\t1,2,my_string"));
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test_setup(initialize, setup),
            cmocka_unit_test_setup(execute_single_char, setup),
            cmocka_unit_test_setup(execute_FBP_CLI_SUCCESS, setup),
            cmocka_unit_test_setup(execute_backspace, setup),
            cmocka_unit_test_setup(execute_echo_on, setup),
            cmocka_unit_test_setup(execute_FBP_CLI_SUCCESS_with_prompt_and_echo, setup),
            cmocka_unit_test_setup(execute_echo_user_char, setup),
            cmocka_unit_test_setup(execute_error, setup),
            cmocka_unit_test_setup(execute_whitespace, setup),
            cmocka_unit_test_setup(execute_whitespace_verbose, setup),
            cmocka_unit_test_setup(comment, setup),
            cmocka_unit_test_setup(comment_too_long, setup),
            cmocka_unit_test_setup(command_too_long, setup),
            cmocka_unit_test_setup(two_commands, setup),
            cmocka_unit_test_setup(line_parser_empty, setup),
            cmocka_unit_test_setup(line_parser_command_only, setup),
            cmocka_unit_test_setup(line_parser_command_only_with_whitespace, setup),
            cmocka_unit_test_setup(line_parser_with_args, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
