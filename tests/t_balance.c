#include <stdio.h>
#include <stdlib.h>
#include "utest.h"
#include "test_utils.h"
#include "ttree.h"

struct item {
    int key;
};

static int __cmpfunc(void *key1, void *key2)
{
    return (*(int *)key1 - *(int *)key2);
}

static struct item *alloc_item(int val)
{
    struct item *item = malloc(sizeof(*item));

    if (!item) {
        utest_error("Failed to allocate %zd bytes!", sizeof(*item));
    }

    item->key = val;
    return item;
}

static bool tree_is_balanced(Ttree *tree)
{
    struct balance_info binfo;

    check_tree_balance(tree, &binfo);
    if (binfo.balance != TREE_BALANCED) {
        utest_warning("Got unbalanced tree (%s) on node %p "
                      "with BFC = %d!", balance_name(binfo.balance),
                      binfo.tnode, binfo.tnode->bfc);
        return false;
    }

    return true;
}

UTEST_FUNCTION(ut_double_rotation, args)
{
    Ttree tree;
    TtreeNode *tnode;
    int num_keys, ret, i, middle;
    struct item *item;

    num_keys = utest_get_arg(args, 0, INT);
    ret = ttree_init(&tree, num_keys, true, __cmpfunc, struct item, key);
    UTEST_ASSERT(ret >= 0);

    middle = 100000;
    /*
     * The following three for cycles should generate
     * double left-right rotation in order to validate it.
     */
    for (i = 0; i < num_keys; i++) {
        item = alloc_item(i + middle);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
    }

    for (i = 0; i < num_keys; i++) {
        item = alloc_item(middle / 2 - i - 1);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
    }

    UTEST_ASSERT(tree_is_balanced(&tree));

    /*
     * The special rotation case for T-tree occurs after double
     * rotation: if a leaf containing only one key becomes a new
     * root node of a subtree and both its left and right childs
     * were half-leafs before rotation, the new root node have to
     * be filled with the keys taken from its new left or right leaf.
     * (the desition is made by comparing total number of keys in both child
     * nodes. The node that contains more keys will be selected)
     * Because of current implementation of a T-tree, taking keys
     * from left leaf is a quite rare case. We emulate it here by deleting
     * one key from future left child. So that after rotation left child
     * of new root node will contain more keys than the right one.
     */
    ret = *(int *)tnode_key_max(tree.root);
    UTEST_ASSERT(ttree_delete(&tree, &ret) != NULL);
    for (i = 0; i < num_keys; i++) {
        item = alloc_item(middle / 2 + i);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
    }

    /* The following two for's gain right-left double rotation */
    UTEST_ASSERT(tree_is_balanced(&tree));
    for (i = 0; i < num_keys; i++) {
        item = alloc_item(middle * 2 + i);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
    }

    UTEST_ASSERT(tree_is_balanced(&tree));
    for (i = 0; i < num_keys; i++) {
        item = alloc_item(middle * 2 - i - 1);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
    }

    UTEST_ASSERT(tree_is_balanced(&tree));

    /*
     * Then we just check how good tree would be balanced
     * after several typical cases. The following two
     * for's should gain both single and double rotations in a tree.
     */
    for (i = 0; i < (middle / 2 - num_keys - 1); i++) {
        item = alloc_item(i);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
        UTEST_ASSERT(tree_is_balanced(&tree));
    }

    for (i = middle / 2 + num_keys; i < middle; i++) {
        item = alloc_item(i);
        UTEST_ASSERT(ttree_insert(&tree, item) == 0);
        UTEST_ASSERT(tree_is_balanced(&tree));
    }

    /*
     * Here we check tree balance after deleting keys from
     * its root node step by step. So that after some time
     * root node is destroyed and replaced by its left or
     * right child. This code should validate tree balance
     * after each deletion.
     */
    ret = 1;
    while (!ttree_is_empty(&tree)) {
        if (!(ret % 2)) {
            i = *(int *)tnode_key_min(tree.root);
        }
        else {
            i = *(int *)tnode_key_max(tree.root);
        }

        UTEST_ASSERT(ttree_delete(&tree, &i) != NULL);
        UTEST_ASSERT(tree_is_balanced(&tree));
        ret++;
    }

    UTEST_PASSED();
}

/*
 * ut_insert_dec inserts items into the tree in decreasing order.
 * The main purpose of the function is to check if single left rotation
 * works as expected.
 * On each step of the main insertion cycle the function checks balance of
 * each tree node.
 */
UTEST_FUNCTION(ut_insert_dec, args)
{
    Ttree tree;
    int num_keys, num_items, items, ret, i;
    void *res;
    struct item *item;
    struct balance_info binfo;

    num_keys = utest_get_arg(args, 0, INT);
    num_items = utest_get_arg(args, 1, INT);
    UTEST_ASSERT(num_items >= 1);

    ret = ttree_init(&tree, num_keys, true, __cmpfunc, struct item, key);
    UTEST_ASSERT(ret >= 0);

    i = 0;
    items = num_items;
    while (items >= 0) {
        item = alloc_item(items);
        item->key = items;
        ret = ttree_insert(&tree, item);
        if (ret != 0) {
            UTEST_FAILED("Failed to insert key %d into the tree on step %d!",
                         item->key, i);
        }

        check_tree_balance(&tree, &binfo);
        if (binfo.balance != TREE_BALANCED) {
            UTEST_FAILED("Step [%d]. Got unbalanced tree (%s) on node %p "
                         "with BFC = %d!", i, balance_name(binfo.balance),
                         binfo.tnode, binfo.tnode->bfc);
        }

        i++;
        items--;
    }
    for (i = 0; i < num_items; i++) {
        res = ttree_delete(&tree, &i);
        if (res == NULL) {
            UTEST_FAILED("Failed to delete item %d on step %d\n",
                         i, i);
        }

        UTEST_ASSERT(tree_is_balanced(&tree));
    }

    UTEST_PASSED();
}

/*
 * ut_insert_inc inserts keys into the tree in increasing order.
 * It should test the simplest case of signle right rotation.
 * After each successful insertion the function checks the balance
 * of each tree node.
 */
UTEST_FUNCTION(ut_insert_inc, args)
{
    Ttree tree;
    int num_keys, num_items, ret, i;
    void *res;
    struct item *item;
    struct balance_info binfo;

    num_keys = utest_get_arg(args, 0, INT);
    num_items = utest_get_arg(args, 1, INT);
    UTEST_ASSERT(num_items >= 1);
    ret = ttree_init(&tree, num_keys, true, __cmpfunc, struct item, key);
    UTEST_ASSERT(ret >= 0);

    for (i = 0; i < num_items; i++) {
        item = alloc_item(i);
        item->key = i;
        ret = ttree_insert(&tree, item);
        if (ret != 0) {
            UTEST_FAILED("Failed to insert item %d to the tree!", item->key);
        }

        check_tree_balance(&tree, &binfo);
        if (binfo.balance != TREE_BALANCED) {
            UTEST_FAILED("Step [%d]. Got unbalanced tree (%s) on node %p "
                         "with BFC =  %d!", i, balance_name(binfo.balance),
                         binfo.tnode, binfo.tnode->bfc);
        }
    }
    for (i = num_items - 1; i >= 0; i--) {
        res = ttree_delete(&tree, &i);
        if (res == NULL) {
            UTEST_FAILED("Failed to delete item %d on step %d",
                         i, num_items - 1);
        }

        UTEST_ASSERT(tree_is_balanced(&tree));
    }

    UTEST_PASSED();
}

DEFINE_UTESTS_LIST(tests) = {
    {
        "UT_INSERT_INC",
        "Insert items into a tree in increasing order",
        ut_insert_inc,
        UTEST_ARGS_LIST {
            { "keys",  UT_ARG_INT, "Number of keys per T*-tree node" },
            {
                "total_items", UT_ARG_INT,
                "Total number of items that will be inserted into the tree.",
            },

            UTEST_ARGS_LIST_END,
        }
    },
    {
        "UT_INSERT_DEC",
        "Insert items into a tree in decreasing order",
        ut_insert_dec,
        UTEST_ARGS_LIST {
            { "keys", UT_ARG_INT, "Number of keys per T*-tree node" },
            {
                "total_items", UT_ARG_INT,
                "Total number of items that will be inserted into the tree.",
            },

            UTEST_ARGS_LIST_END,
        },
    },
    {
        "UT_DOUBLE_ROTATION",
        "Check if left-right and rihgt-left double rotations work as expected",
        ut_double_rotation,
        UTEST_ARGS_LIST {
            { "keys", UT_ARG_INT, "Number of keys per T*-tree node" },
            UTEST_ARGS_LIST_END,
        },
    },

    UTESTS_LIST_END,
};

int main(int argc, char *argv[])
{
    utest_main(tests, argc, argv);
    return 0;
}
