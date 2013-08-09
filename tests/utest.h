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

#ifndef _UTEST_H_
#define _UTEST_H_

#include <stdarg.h>
#include <stdbool.h>

#define TEST_EXIT_FAILURE 255
#define TEST_EXIT_SUCCESS 254

enum ut_arg_type {
    UT_ARG_STRING = 1,
    UT_ARG_INT,
    UT_ARG_LONG,
    UT_ARG_DOUBLE,
};

struct test_arg {
    char *arg_name;
    enum ut_arg_type arg_type;
    char *arg_descr;
    char *__val;
};

typedef bool (*utest_fn)(struct test_arg *);

struct test_case {
    char *test_name;
    char *test_descr;
    utest_fn test_function;
    struct test_arg *test_args;
};

#define DEFINE_UTESTS_LIST(name)                \
    struct test_case (name)[]

#define UTEST_FUNCTION(fname, args)             \
    bool (fname)(struct test_arg *(args))

#define UTESTS_LIST_END     { .test_name = NULL, }
#define UTEST_ARGS_LIST     (struct test_arg [])
#define UTEST_ARGS_LIST_END { .arg_name = NULL, }

#define IS_UTESTS_LIST_END(ut) ((ut)->test_name == NULL)
#define IS_UTEST_ARGS_LIST_END(al) ((al)->arg_name == NULL)

#define for_each_test_case(tests_list, iter)                            \
    for ((iter) = &(tests_list)[0]; !IS_UTESTS_LIST_END(iter); (iter)++)

#define for_each_test_arg(args_list, iter)                              \
    for ((iter) = (args_list); !IS_UTEST_ARGS_LIST_END(iter); (iter)++)

#define UTEST_ASSERT(cond)                                      \
    do {                                                        \
        if (!(cond)) {                                          \
            __describe_failure(__FUNCTION__, __LINE__, #cond);  \
            printf("::: [FAILED]\n");                           \
            return true;                                        \
        }                                                       \
    } while (0)

#define UTEST_PASSED()                          \
    do {                                        \
        printf("::: [PASSED]\n");               \
        return false;                           \
    } while (0)

#define UTEST_FAILED(descr, args...)                                \
    do {                                                            \
        __describe_failure(__FUNCTION__, __LINE__, descr, ##args);  \
        printf("::: [FAILED]\n");                                   \
        return true;                                                \
    } while (0)

#define utest_get_arg(args, idx, type)          \
    __utest_get_##type(&(args)[idx])

void utest_main(struct test_case *utests, int argc, char *argv[]);
void utest_error(const char *fmt, ...);
void utest_warning(const char *fmt, ...);
void __describe_failure(const char *fn, int line, const char *fmt, ...);
int __utest_get_INT(struct test_arg *arg);
char *__utest_get_STRING(struct test_arg *arg);
long __utest_get_LONG(struct test_arg *arg);
double __utest_get_DOUBLE(struct test_arg *arg);

#endif /* !_UTEST_H_ */
