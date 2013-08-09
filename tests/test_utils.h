#ifndef _TEST_UTILS_H_
#define _TEST_UTILS_H_

#include "ttree.h"

enum balance_type {
    TREE_BALANCED,
    TREE_LEFT_HEAVY,
    TREE_RIGHT_HEAVY,
};

struct balance_info {
    enum balance_type balance;
    TtreeNode *tnode;
};

void check_tree_balance(Ttree *ttree, struct balance_info *binfo);
char *balance_name(enum balance_type type);

#endif /* !_TEST_UTILS_H_ */
