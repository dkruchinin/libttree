#include <stdio.h>
#include <errno.h>
#include "utest.h"
#include "ttree.h"

struct test_struct {
    int key;
};

int mycmpfunc(void *k1, void *k2)
{
    return 0;
}

#define TTREE_INIT_CHECK(ttreep, numk, func, st, key)           \
    do {                                                        \
        ret = ttree_init(ttreep, numk, true, func, st, key);    \
        UTEST_ASSERT(ret == 0);                                 \
    } while (0)

#define TTREE_INIT_CHECK_ERR(ttreep, numk, func, st, key, err)  \
    do {                                                        \
        ret = ttree_init(ttreep, numk, true, func, st, key);    \
        UTEST_ASSERT(ret < 0);                                  \
        UTEST_ASSERT(errno == (err));                           \
        errno = 0;                                              \
    } while (0)

UTEST_FUNCTION(ut_init, unused)
{
    Ttree ttree;
    int ret;

    TTREE_INIT_CHECK(&ttree, TTREE_DEFAULT_NUMKEYS, mycmpfunc,
                     struct test_struct, key);
    TTREE_INIT_CHECK_ERR(&ttree, 0, mycmpfunc,
                         struct test_struct, key, EINVAL);
    TTREE_INIT_CHECK_ERR(&ttree, TNODE_ITEMS_MAX + 1, mycmpfunc,
                         struct test_struct, key, EINVAL);
    TTREE_INIT_CHECK_ERR(NULL, TTREE_DEFAULT_NUMKEYS, mycmpfunc,
                         struct test_struct, key, EINVAL);
    TTREE_INIT_CHECK_ERR(&ttree, TTREE_DEFAULT_NUMKEYS, NULL,
                         struct test_struct, key, EINVAL);

    UTEST_PASSED();
}

DEFINE_UTESTS_LIST(tests) = {
    {
        .test_name = "UT_INIT",
        .test_descr = "Testing ttree_init and __ttree_init functions.",
        .test_function = ut_init,
        UTEST_ARGS_LIST { UTEST_ARGS_LIST_END, },
    },
    UTESTS_LIST_END,
};

int main(int argc, char *argv[])
{
    utest_main(tests, argc, argv);
    return 0;
}
