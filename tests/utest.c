/*
 * Copyright (c) 2010 Dan Kruchinin <dan.kruchiniGn@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include "utest.h"

static char *appname = NULL;
static struct test_case *test_cases = NULL;

void utest_error(const char *fmt, ...)
{
    va_list ap;
    int err = errno;

    va_start(ap, fmt);
    fprintf(stderr, "[UTEST ERROR]: ");
    vfprintf(stderr, fmt, ap);
    if (err) {
        fprintf(stderr, "\n\tERRNO: [%s:%d]\n", strerror(err), err);
    }
    else {
        fputc('\n', stderr);
    }

    va_end(ap);
    exit(EXIT_FAILURE);
}

void utest_warning(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    fprintf(stderr, "[UTEST WARNING]: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}

static void utest_show_usage(void)
{
    fprintf(stderr, "USAGE: %s <test name> [arg1 arg2 ... argN]\n", appname);
    exit(EXIT_SUCCESS);
}

static char *describe_arg_type(enum ut_arg_type type)
{
    switch (type) {
        case UT_ARG_STRING:
            return "STRING";
        case UT_ARG_INT:
            return "INTEGER";
        case UT_ARG_DOUBLE:
            return "DOUBLE";
        case UT_ARG_LONG:
            return "LONG";
    }

    return "UNKNONW";
}

static void validate_test_arg(struct test_arg *arg, enum ut_arg_type given)
{
    if (arg->arg_type != given) {
        utest_error("Unexpected type of argument %s (%s). Expected %s\n",
                    arg->arg_name, describe_arg_type(given),
                    describe_arg_type(arg->arg_type));
    }
}

static void show_test_usage(struct test_case *tc)
{
    struct test_arg *arg;
    int num_args = 0;

    fprintf(stderr, "USAGE: %s <%s>\n", appname, tc->test_name);
    fprintf(stderr, "  Arguments:\n");
    for_each_test_arg(tc->test_args, arg) {
        char *arg_type = describe_arg_type(arg->arg_type);

        fprintf(stderr, "   %s - [<%s>] %s\n", arg->arg_name,
                arg_type, arg->arg_descr);
        num_args++;
    }
    if (!num_args) {
        fprintf(stderr, "   NO ARGUMENTS\n");
    }

    fprintf(stderr, "  Description:\n   %s\n", tc->test_descr);
    exit(EXIT_SUCCESS);
}

static void show_all_tests(void)
{
    struct test_case *tc;

    printf("List of registered tests:\n");
    for_each_test_case(test_cases, tc) {
        printf(" - %s\n", tc->test_name);
        printf("   DESCR: %s\n", tc->test_descr);
    }
}

static struct test_case *test_case_by_name(const char *tname)
{
    struct test_case *tc;
    bool found = false;

    for_each_test_case(test_cases, tc) {
        if (!strcmp(tc->test_name, tname)) {
            found = true;
            break;
        }
    };
    if (found) {
        return tc;
    }

    return NULL;
}

static int get_num_args(struct test_case *tc)
{
    struct test_arg *arg;
    int num = 0;

    for_each_test_arg(tc->test_args, arg) {
        num++;
    }

    return num;
}

static void init_test_args(struct test_case *tc, char *argv[])
{
    struct test_arg *arg;
    int num = 1;

    for_each_test_arg(tc->test_args, arg) {
        arg->__val = argv[++num];
    }
}

static void run_test_case(struct test_case *tc)
{
    bool ret;

    printf("Running test %s\n", tc->test_name);
    printf("  == %s ==\n", tc->test_descr);
    ret = tc->test_function(tc->test_args);
    if (ret) {
        exit(TEST_EXIT_FAILURE);
    }

    exit(TEST_EXIT_SUCCESS);
}

void __describe_failure(const char *fn, int line, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    printf("---------------\n");
    printf("Failure reason: [%s:%d] ", fn, line);
    vprintf(fmt, ap);
    printf("\n--------------\n");
    va_end(ap);
}

void utest_main(struct test_case *utests, int argc, char *argv[])
{
    char *tname;
    struct test_case *tc;
    int num_args;

    appname = argv[0];
    if (!utests || IS_UTESTS_LIST_END(&utests[0])) {
        utest_error("Test cases weren't specified!");
    }
    if (argc == 1) {
        utest_show_usage();
    }

    test_cases = utests;
    tname = argv[1];
    tc = test_case_by_name(tname);
    if (!tc) {
        utest_warning("Test case with name %s was not found!", tname);
        show_all_tests();
        exit(EXIT_SUCCESS);
    }

    num_args = get_num_args(tc);
    if (num_args != (argc - 2)) {
        utest_warning("Invalid number of argnuments for test case %s "
                      "(%d expected)!", tc->test_name, num_args);
        show_test_usage(tc);
    }

    init_test_args(tc, argv);
    run_test_case(tc);
}

char *__utest_get_STRING(struct test_arg *arg)
{
    validate_test_arg(arg, UT_ARG_STRING);
    return arg->__val;
}

int __utest_get_INT(struct test_arg *arg)
{
    validate_test_arg(arg, UT_ARG_INT);
    return atoi(arg->__val);
}

long __utest_get_LONG(struct test_arg *arg)
{
    validate_test_arg(arg, UT_ARG_LONG);
    return atol(arg->__val);
}

double __utest_get_DOUBLE(struct test_arg *arg)
{
    validate_test_arg(arg, UT_ARG_DOUBLE);
    return atof(arg->__val);
}
