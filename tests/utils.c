#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "ttree.h"
#include "test_utils.h"

static int __check_tree_balance(TtreeNode *tnode, struct balance_info *binfo)
{
    int r, l, diff;

    if (!tnode || (binfo->balance != TREE_BALANCED)) {
        return 0;
    }

    l = __check_tree_balance(tnode->left, binfo);
    r = __check_tree_balance(tnode->right, binfo);
    if (tnode->left) {
        l++;
    }
    if (tnode->right) {
        r++;
    }

    diff = r - l;
    if (diff && (binfo->balance == TREE_BALANCED)) {
        binfo->tnode = tnode;
        if (diff > 1) {
            binfo->balance = TREE_RIGHT_HEAVY;
        }
        else if (diff < -1) {
            binfo->balance = TREE_LEFT_HEAVY;
        }
    }

    return ((r > l) ? r : l);
}

void check_tree_balance(Ttree *ttree, struct balance_info *binfo)
{
    binfo->tnode = NULL;
    binfo->balance = TREE_BALANCED;
    __check_tree_balance(ttree->root, binfo);
}

char *balance_name(enum balance_type type)
{
    switch (type) {
        case TREE_LEFT_HEAVY:
            return "Left-heavy";
        case TREE_RIGHT_HEAVY:
            return "Right-heavy";
    }

    return "Balanced";
}
