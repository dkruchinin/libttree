/*
 * Copyright (c) 2008, 2009 Dan Kruchinin <dkruchinin@acm.org>
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
 * For more information about T- and T*-trees see:
 * 1) Tobin J. Lehman , Michael J. Carey,
 *    A Study of Index Structures for Main Memory Database Management Systems
 * 2) Kong-Rim Choi , Kyung-Chang Kim,
 *    T*-tree: a main memory database index structure for real time applications
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>

#include "ttree.h"

#ifndef DEBUG_TTREE
#define SET_ERRNO(err) errno = (err)
#else /* !DEBUG_TTREE */
#define SET_ERRNO(err)                                                  \
    do {                                                                \
        if ((err) != 0) {                                               \
            fprintf(stderr, "[TTREE] setting errno = %d. "              \
                    "(%s:%s:%d)\n", __FILE__, __FUNCTION__, __LINE__);  \
        }                                                               \
                                                                        \
        errno = (err);                                                  \
    } while (0)
#endif /* DEBUG_TTREE */

/* Index number of first key in a T*-tree node when a node has only one key. */
#define first_tnode_idx(ttree)                  \
    (((ttree)->keys_per_tnode >> 1) - 1)

/*
 * Minimum allowed number of used rooms in a T*-tree node.
 * By default it's a quoter of total number of key rooms in a node.
 */
#define min_tnode_entries(ttree)                                \
    ((ttree)->keys_per_tnode - ((ttree)->keys_per_tnode >> 2))

/*
 * T*-tree has three types of node:
 * 1. Node that hasn't left and right child is called "leaf node".
 * 2. Node that has only one child is called "half-leaf node"
 * 3. Finally, node that has both left and right childs is called "internal node"
 */
#define is_leaf_node(node)                      \
    (!(node)->left && !(node)->right)
#define is_internal_node(node)                  \
    ((node)->left && (node)->right)
#define is_half_leaf(tnode)                                             \
    ((!(tnode)->left || !(tnode)->right) && !((tnode)->left && (tnode)->right))

/* Translate node side to balance factor */
#define side2bfc(side)                          \
    __balance_factors[side]
#define get_bfc_delta(node)                     \
    (side2bfc(tnode_get_side(node)))
#define subtree_is_unbalanced(node)             \
    (((node)->bfc < -1) || ((node)->bfc > 1))
#define opposite_side(side)                     \
    (!(side))
#define left_heavy(node)                        \
    ((node)->bfc < 0)
#define right_heavy(node)                       \
    ((node)->bfc > 0)

struct tnode_lookup {
    void *key;
    int low_bound;
    int high_bound;
};

static int __balance_factors[] = { -1, 1 };

static TtreeNode *allocate_ttree_node(Ttree *ttree)
{
    TtreeNode *tnode = malloc(tnode_size(ttree));

    if (tnode) {
        memset(tnode, 0, sizeof(*tnode) - TNODE_ITEMS_MIN * sizeof(uintptr_t));
    }

    return tnode;
}

/*
 * T*-tree node contains keys in a sorted order. Thus binary search
 * is used for internal lookup.
 */
static void *lookup_inside_tnode(Ttree *ttree, TtreeNode *tnode,
                                 struct tnode_lookup *tnl, int *out_idx)
{
    int floor, ceil, mid, cmp_res;

    floor = tnl->low_bound;
    ceil = tnl->high_bound;
    TTREE_ASSERT((floor >= 0) && (ceil < ttree->keys_per_tnode));
    while (floor <= ceil) {
        mid = (floor + ceil) >> 1;
        if ((cmp_res = ttree->cmp_func(tnl->key, tnode->keys[mid])) < 0)
            ceil = mid - 1;
        else if (cmp_res > 0)
            floor = mid + 1;
        else {
            *out_idx = mid;
            return ttree_key2item(ttree, tnode->keys[mid]);
        }
    }

    /*
     * If a key position is not found, save an index of the position
     * where key may be placed to.
     */
    *out_idx = floor;
    return NULL;
}

static __inline void increase_tnode_window(Ttree *ttree,
                                           TtreeNode *tnode, int *idx)
{
    register int i;

    /*
     * If the right side of an array has more free rooms than the left one,
     * the window will grow to the right. Otherwise it'll grow to the left.
     */
    if ((ttree->keys_per_tnode - 1 - tnode->max_idx) > tnode->min_idx) {
        for (i = ++tnode->max_idx; i > *idx - 1; i--)
            tnode->keys[i] = tnode->keys[i - 1];
    }
    else {
        *idx -= 1;
        for (i = --tnode->min_idx; i < *idx; i++) {
            tnode->keys[i] = tnode->keys[i + 1];
        }
    }
}

static __inline void decrease_tnode_window(Ttree *ttree,
                                         TtreeNode *tnode, int *idx)
{
    register int i;

    /* Shrink the window to the longer side by given index. */
    if ((ttree->keys_per_tnode - 1 - tnode->max_idx) <= tnode->min_idx) {
        tnode->max_idx--;
        for (i = *idx; i <= tnode->max_idx; i++)
            tnode->keys[i] = tnode->keys[i + 1];
    }
    else {
        tnode->min_idx++;
        for (i = *idx; i >= tnode->min_idx; i--)
            tnode->keys[i] = tnode->keys[i - 1];

        *idx = *idx + 1;
    }
}

/*
 * generic single rotation procedrue.
 * side = TNODE_LEFT  - Right rotation
 * side = TNODE_RIGHT - Left rotation.
 * "target" will be set to the new root of rotated subtree.
 */
static void __rotate_single(TtreeNode **target, int side)
{
    TtreeNode *p, *s;
    int opside = opposite_side(side);

    p = *target;
    TTREE_ASSERT(p != NULL);
    s = p->sides[side];
    TTREE_ASSERT(s != NULL);
    tnode_set_side(s, tnode_get_side(p));
    p->sides[side] = s->sides[opside];
    s->sides[opside] = p;
    tnode_set_side(p, opside);
    s->parent = p->parent;
    p->parent = s;
    if (p->sides[side]) {
        p->sides[side]->parent = p;
        tnode_set_side(p->sides[side], side);
    }
    if (s->parent) {
        if (s->parent->sides[side] == p)
            s->parent->sides[side] = s;
        else
            s->parent->sides[opside] = s;
    }

    *target = s;
}

/*
 * There are two cases of single rotation possible:
 * 1) Right rotation (side = TNODE_LEFT)
 *         [P]             [L]
 *        /  \            /  \
 *      [L]  x1    =>   x2   [P]
 *     /  \                 /  \
 *    x2  x3               x3  x1
 *
 * 2) Left rotation (side = TNODE_RIHGT)
 *      [P]                [R]
 *     /  \               /  \
 *    x1  [R]      =>   [P]   x2
 *       /  \          /  \
 *     x3   x2        x1  x3
 */
static void rotate_single(TtreeNode **target, int side)
{
    TtreeNode *n;

    __rotate_single(target, side);
    n = (*target)->sides[opposite_side(side)];

    /*
     * Recalculate balance factors of nodes after rotation.
     * Let X was a root node of rotated subtree and Y was its
     * child. After single rotation Y is new root of subtree and X is its child.
     * Y node may become either balanced or overweighted to the
     * same side it was but 1 level less.
     * X node scales at 1 level down and possibly it has new child, so
     * its balance should be recalculated too. If it still internal node and
     * its new parent was not overwaighted to the opposite to X side,
     * X is overweighted to the opposite to its new parent side,
     * otherwise it's balanced. If X is either half-leaf or leaf,
     * balance racalculation is obvious.
     */
    if (is_internal_node(n)) {
        n->bfc = (n->parent->bfc != side2bfc(side)) ? side2bfc(side) : 0;
    }
    else {
        n->bfc = !!(n->right) - !!(n->left);
    }

    (*target)->bfc += side2bfc(opposite_side(side));
    TTREE_ASSERT((abs(n->bfc < 2) && (abs((*target)->bfc) < 2)));
}

/*
 * There are two possible cases of double rotation:
 * 1) Left-right rotation: (side == TNODE_LEFT)
 *      [P]                     [r]
 *     /  \                    /  \
 *   [L]  x1                [L]   [P]
 *  /  \          =>       / \    / \
 * x2  [r]                x2 x4  x3 x1
 *    /  \
 *  x4   x3
 *
 * 2) Right-left rotation: (side == TNODE_RIGHT)
 *      [P]                     [l]
 *     /  \                    /  \
 *    x1  [R]               [P]   [R]
 *       /  \     =>        / \   / \
 *      [l] x2             x1 x3 x4 x2
 *     /  \
 *    x3  x4
 */
static void rotate_double(TtreeNode **target, int side)
{
    int opside = opposite_side(side);
    TtreeNode *n = (*target)->sides[side];

    __rotate_single(&n, opside);

    /*
     * Balance recalculation is very similar to recalculation after
     * simple single rotation.
     */
    if (is_internal_node(n->sides[side])) {
        n->sides[side]->bfc = (n->bfc == side2bfc(opside)) ? side2bfc(side) : 0;
    }
    else {
        n->sides[side]->bfc =
            !!(n->sides[side]->right) - !!(n->sides[side]->left);
    }

    TTREE_ASSERT(abs(n->sides[side]->bfc) < 2);
    n = n->parent;
    __rotate_single(target, side);
    if (is_internal_node(n)) {
        n->bfc = ((*target)->bfc == side2bfc(side)) ? side2bfc(opside) : 0;
    }
    else {
        n->bfc = !!(n->right) - !!(n->left);
    }

    /*
     * new root node of subtree is always ideally balanced
     * after double rotation.
     */
    TTREE_ASSERT(abs(n->bfc) < 2);
    (*target)->bfc = 0;
}

static void rebalance(Ttree *ttree, TtreeNode **node, TtreeCursor *cursor)
{
    int lh = left_heavy(*node);
    int sum = abs((*node)->bfc + (*node)->sides[opposite_side(lh)]->bfc);

    if (sum >= 2) {
        rotate_single(node, opposite_side(lh));
        goto out;
    }

    rotate_double(node, opposite_side(lh));

    /*
     * T-tree rotation rules difference from AVL rules in only one aspect.
     * After double rotation is done and a leaf became a new root node of
     * subtree and both its left and right childs are half-leafs.
     * If the new root node contains only one item, N - 1 items should
     * be moved into it from one of its childs.
     * (N is a number of items in selected child node).
     */
    if ((tnode_num_keys(*node) == 1) &&
        is_half_leaf((*node)->left) && is_half_leaf((*node)->right)) {
        TtreeNode *n;
        int offs, nkeys;

        /*
         * If right child contains more items than left, they will be moved
         * from the right child. Otherwise from the left one.
         */
        if (tnode_num_keys((*node)->right) >= tnode_num_keys((*node)->left)) {
            /*
             * Right child was selected. So first N - 1 items will be copied
             * and inserted after parent's first item.
             */
            n = (*node)->right;
            nkeys = tnode_num_keys(n);
            (*node)->keys[0] = (*node)->keys[(*node)->min_idx];
            offs = 1;
            (*node)->min_idx = 0;
            (*node)->max_idx = nkeys - 1;
            if (!cursor) {
                goto no_cursor;
            }
            else if (cursor->tnode == n) {
                if (cursor->idx < n->max_idx) {
                    cursor->tnode = *node;
                    cursor->idx = (*node)->min_idx +
                        (cursor->idx - n->min_idx + 1);
                }
                else {
                    cursor->idx = first_tnode_idx(ttree);
                }
            }
        }
        else {
            /*
             * Left child was selected. So its N - 1 items
             * (starting after the min one)
             * will be copied and inserted before parent's single item.
             */
            n = (*node)->left;
            nkeys = tnode_num_keys(n);
            (*node)->keys[ttree->keys_per_tnode - 1] =
                (*node)->keys[(*node)->min_idx];
            (*node)->min_idx = offs = ttree->keys_per_tnode - nkeys;
            (*node)->max_idx = ttree->keys_per_tnode - 1;
            if (!cursor) {
                goto no_cursor;
            }
            else if (cursor->tnode == n) {
                if (cursor->idx > n->min_idx) {
                    cursor->tnode = *node;
                    cursor->idx = (*node)->min_idx + (cursor->idx - n->min_idx);
                }
                else {
                    cursor->idx = first_tnode_idx(ttree);
                }
            }

            n->max_idx = n->min_idx++;
        }

no_cursor:
        memcpy((*node)->keys + offs,
               n->keys + n->min_idx, sizeof(void *) * (nkeys - 1));
        n->keys[first_tnode_idx(ttree)] = n->keys[n->max_idx];
        n->min_idx = n->max_idx = first_tnode_idx(ttree);
    }

out:
    if (ttree->root->parent) {
        ttree->root = *node;
    }
}

static __inline void __add_successor(TtreeNode *n)
{
    /*
     * After new leaf node was added, its successor should be
     * fixed. Also it(successor) could became a successor of the node
     * higher than the given one.
     * There are several possible cases of such situation:
     * 1) If new node is added as a right child, it inherites
     *    successor of its parent. And it itself becomes a successor
     *    of its parent.
     * 2) If it is a left child, its parent will be the successor.
     * 2.1) If parent itself is a right child, then newly added node becomes
     *      the successor of parent's parent.
     * 2.2) Otherwise it becomes a successor of one of nodes located higher.
     *      In this case, we should browse up the tree starting from
     *      parent's parent. One of the nodes on the path *may* have a successor
     *      equals to parent of a newly added node. If such node will be found,
     *      its successor should be changed to a newly added node.
     */
    if (tnode_get_side(n) == TNODE_RIGHT) {
        n->successor = n->parent->successor;
        n->parent->successor = n;
    }
    else {
        n->successor = n->parent;
        if (tnode_get_side(n->parent) == TNODE_RIGHT) {
            n->parent->parent->successor = n;
        }
        else if (tnode_get_side(n->parent) == TNODE_LEFT) {
            register TtreeNode *node;

            for (node = n->parent->parent; node; node = node->parent) {
                if (node->successor == n->parent) {
                    node->successor = n;
                    break;
                }
            }
        }
    }
}

static __inline void __remove_successor(TtreeNode *n)
{
    /*
     * Node removing could affect the successor of one of nodes
     * with higher level, so it should be fixed.
     * Since T*-tree node deletion algorithm
     * assumes that ony leafs are removed, successor fixing
     * is opposite to successor adding algorithm.
     */
    if (tnode_get_side(n) == TNODE_RIGHT) {
        n->parent->successor = n->successor;
    }
    else if (tnode_get_side(n->parent) == TNODE_RIGHT) {
        n->parent->parent->successor = n->parent;
    }
    else {
        register TtreeNode *node = n;

        while ((node = node->parent)) {
            if (node->successor == n) {
                node->successor = n->parent;
                break;
            }
        }
    }
}

static void fixup_after_insertion(Ttree *ttree, TtreeNode *n,
                                  TtreeCursor *cursor)
{
    int bfc_delta = get_bfc_delta(n);
    TtreeNode *node = n;

    __add_successor(n);
    /* check tree for balance after new node was added. */
    while ((node = node->parent)) {
        node->bfc += bfc_delta;
        /*
         * if node becomes balanced, tree balance is ok,
         * so process may be stopped here
         */
        if (!node->bfc) {
            return;
        }
        if (subtree_is_unbalanced(node)) {
            /*
             * Because of nature of T-tree rebalancing, just inserted item
             * may change its position in its node and even the node itself.
             * Thus if T-tree cursor was specified we have to take care of it.
             */
            rebalance(ttree, &node, cursor);

            /*
             * single or double rotation tree becomes balanced
             * and we can stop here.
             */
            return;
        }

        bfc_delta = get_bfc_delta(node);
    }
}

static void fixup_after_deletion(Ttree *ttree, TtreeNode *n,
                                 TtreeCursor *cursor)
{
    TtreeNode *node = n->parent;
    int bfc_delta = get_bfc_delta(n);

    __remove_successor(n);

    /*
     * Unlike balance fixing after insertion,
     * deletion may require several rotations.
     */
    while (node) {
        node->bfc -= bfc_delta;
        /*
         * If node's balance factor was 0 and becomes 1 or -1, we can stop.
         */
        if (!(node->bfc + bfc_delta))
            break;

        bfc_delta = get_bfc_delta(node);
        if (subtree_is_unbalanced(node)) {
            TtreeNode *tmp = node;

            rebalance(ttree, &tmp, cursor);
            /*
             * If after rotation subtree height is not changed,
             * proccess should be continued.
             */
            if (tmp->bfc)
                break;

            node = tmp;
        }

        node = node->parent;
    }
}

int __ttree_init(Ttree *ttree, int num_keys, bool is_unique,
                 ttree_cmp_func_fn cmpf, size_t key_offs)
{
    TTREE_CT_ASSERT((TTREE_DEFAULT_NUMKEYS >= TNODE_ITEMS_MIN) &&
                    (TTREE_DEFAULT_NUMKEYS <= TNODE_ITEMS_MAX));

    if ((num_keys < TNODE_ITEMS_MIN) ||
        (num_keys > TNODE_ITEMS_MAX) || !ttree || !cmpf) {
        SET_ERRNO(EINVAL);
        return -1;
    }

    ttree->root = NULL;
    ttree->keys_per_tnode = num_keys;
    ttree->cmp_func = cmpf;
    ttree->key_offs = key_offs;
    ttree->keys_are_unique = is_unique;

    return 0;
}

void ttree_destroy(Ttree *ttree)
{
    TtreeNode *tnode, *next;

    if (!ttree->root)
        return;
    for (tnode = next = ttree_node_leftmost(ttree->root); tnode; tnode = next) {
        next = tnode->successor;
        free(tnode);
    }

    ttree->root = NULL;
}

void *ttree_lookup(Ttree *ttree, void *key, TtreeCursor *cursor)
{
    TtreeNode *n, *marked_tn, *target;
    int side = TNODE_BOUND, cmp_res, idx;
    void *item = NULL;
    enum ttree_cursor_state st = CURSOR_PENDING;

    /*
     * Classical T-tree search algorithm is O(log(2N/M) + log(M - 2))
     * Where N is total number of items in the tree and M is a number of
     * items per node. In worst case each node on the path requires 2
     * comparison(with its min and max items) plus binary search in the last
     * node(bound node) excluding its first and last items.
     *
     * Here is used another approach that was suggested in
     * "Tobin J. Lehman , Michael J. Carey, A Study of Index Structures for
     * Main Memory Database Management Systems".
     * It reduces O(log(2N/M) + log(M - 2)) to true O(log(N)).
     * This algorithm compares the search
     * key only with minimum item in each node. If search key is greater,
     * current node is marked for future consideration.
     */
    target = n = ttree->root;
    marked_tn = NULL;
    idx = first_tnode_idx(ttree);
    if (!n) {
        goto out;
    }
    while (n) {
        target = n;
        cmp_res = ttree->cmp_func(key, tnode_key_min(n));
        if (cmp_res < 0)
            side = TNODE_LEFT;
        else if (cmp_res > 0) {
            marked_tn = n; /* mark current node for future consideration. */
            side = TNODE_RIGHT;
        }
        else { /* ok, key is found, search is completed. */
            side = TNODE_BOUND;
            idx = n->min_idx;
            item = ttree_key2item(ttree, tnode_key_min(n));
            st = CURSOR_OPENED;
            goto out;
        }

        n = n->sides[side];
    }
    if (marked_tn) {
        int c = ttree->cmp_func(key, tnode_key_max(marked_tn));

        if (c <= 0) {
            side = TNODE_BOUND;
            target = marked_tn;
            if (!c) {
                item = ttree_key2item(ttree, tnode_key_max(target));
                idx = target->max_idx;
                st = CURSOR_OPENED;
            }
            else { /* make internal binary search */
                struct tnode_lookup tnl;

                tnl.key = key;
                tnl.low_bound = target->min_idx + 1;
                tnl.high_bound = target->max_idx - 1;
                item = lookup_inside_tnode(ttree, target, &tnl, &idx);
                st = (item != NULL) ? CURSOR_OPENED : CURSOR_PENDING;
            }

            goto out;
        }
    }

    /*
     * If we're here, item wasn't found. So the only thing
     * needs to be done is to determine the position where search key
     * may be placed to. If target node is not empty, key may be placed
     * to its min or max positions.
     */
    if (!tnode_is_full(ttree, target)) {
        side = TNODE_BOUND;
        idx = ((marked_tn != target) || (cmp_res < 0)) ?
            target->min_idx : (target->max_idx + 1);
        st = CURSOR_PENDING;
    }

out:
    if (cursor) {
        ttree_cursor_open_on_node(cursor, ttree, target, TNODE_SEEK_START);
        cursor->side = side;
        cursor->idx = idx;
        cursor->state = st;
    }

    return item;
}

int ttree_insert(Ttree *ttree, void *item)
{
    TtreeCursor cursor;

    /*
     * If the tree already contains the same key item has and
     * tree's wasn't allowed to hold duplicate keys, signal an error.
     */
    if (ttree_lookup(ttree, ttree_item2key(ttree, item), &cursor)
        && ttree->keys_are_unique) {
        return -1;
    }

    ttree_insert_at_cursor(&cursor, item);
    return 0;
}

void ttree_insert_at_cursor(TtreeCursor *cursor, void *item)
{
    Ttree *ttree = cursor->ttree;
    TtreeNode *at_node, *n;
    TtreeCursor tmp_cursor;
    void *key;

    TTREE_ASSERT(cursor->ttree != NULL);
    //TTREE_ASSERT(cursor->state == CURSOR_PENDING);
    key = ttree_item2key(ttree, item);
    n = at_node = cursor->tnode;
    if (!ttree->root) { /* The root node has to be created. */
        at_node = allocate_ttree_node(ttree);
        at_node->keys[first_tnode_idx(ttree)] = key;
        at_node->min_idx = at_node->max_idx = first_tnode_idx(ttree);
        ttree->root = at_node;
        tnode_set_side(at_node, TNODE_ROOT);
        ttree_cursor_open_on_node(cursor, ttree, at_node, TNODE_SEEK_START);
        return;
    }
    if (cursor->side == TNODE_BOUND) {
        if (tnode_is_full(ttree, n)) {
            /*
             * If node is full its max item should be removed and
             * new key should be inserted into it. Removed key becomes
             * new insert value that should be put in successor node.
             */
            void *tmp = n->keys[n->max_idx--];

            increase_tnode_window(ttree, n, &cursor->idx);
            n->keys[cursor->idx] = key;
            key = tmp;

            ttree_cursor_copy(&tmp_cursor, cursor);
            cursor = &tmp_cursor;

            /*
             * If current node hasn't successor and right child
             * New node have to be created. It'll become the right child
             * of the current node.
             */
            if (!n->successor || !n->right) {
                cursor->side = TNODE_RIGHT;
                cursor->idx = first_tnode_idx(ttree);
                goto create_new_node;
            }

            at_node = n->successor;
            /*
             * If successor hasn't any free rooms, new value is inserted
             * into newly created node that becomes left child of the current
             * node's successor.
             */
            if (tnode_is_full(ttree, at_node)) {
                cursor->side = TNODE_LEFT;
                cursor->idx = first_tnode_idx(ttree);
                goto create_new_node;
            }

            /*
             * If we're here, then successor has free rooms and key
             * will be inserted to one of them.
             */
            cursor->idx = at_node->min_idx;
            cursor->tnode = at_node;
        }

        increase_tnode_window(ttree, at_node, &cursor->idx);
        at_node->keys[cursor->idx] = key;
        cursor->state = CURSOR_OPENED;
        return;
    }

create_new_node:
    n = allocate_ttree_node(ttree);
    n->keys[cursor->idx] = key;
    n->min_idx = n->max_idx = cursor->idx;
    n->parent = at_node;
    at_node->sides[cursor->side] = n;
    tnode_set_side(n, cursor->side);
    cursor->tnode = n;
    cursor->state = CURSOR_OPENED;
    fixup_after_insertion(ttree, n, cursor);
}

void *ttree_delete(Ttree *ttree, void *key)
{
    TtreeCursor cursor;
    void *ret;

    ret = ttree_lookup(ttree, key, &cursor);
    if (!ret) {
        return ret;
    }

    ttree_delete_at_cursor(&cursor);
    return ret;
}

void *ttree_delete_at_cursor(TtreeCursor *cursor)
{
    Ttree *ttree = cursor->ttree;
    TtreeNode *tnode, *n;
    void *ret;

    TTREE_ASSERT(cursor->ttree != NULL);
    TTREE_ASSERT(cursor->state == CURSOR_OPENED);
    tnode = cursor->tnode;
    ret = ttree_key2item(ttree, tnode->keys[cursor->idx]);
    decrease_tnode_window(ttree, tnode, &cursor->idx);
    cursor->state = CURSOR_CLOSED;
    if (UNLIKELY(cursor->idx > tnode->max_idx)) {
        cursor->idx = tnode->max_idx;
    }

    /*
     * If after a key was removed, T*-tree node contains more than
     * minimum allowed number of items, the proccess is completed.
     */
    if (tnode_num_keys(tnode) > min_tnode_entries(ttree)) {
        return ret;
    }
    if (is_internal_node(tnode)) {
        int idx;

        /*
         * If it is an internal node, we have to recover number
         * of items from it by moving one item from its successor.
         */
        n = tnode->successor;
        idx = tnode->max_idx + 1;
        increase_tnode_window(ttree, tnode, &idx);
        tnode->keys[idx] = n->keys[n->min_idx++];
        if (UNLIKELY(cursor->idx > tnode->max_idx)) {
            cursor->idx = tnode->max_idx;
        }
        if (!tnode_is_empty(n) && is_leaf_node(n)) {
            return ret;
        }

        /*
         * If we're here, then successor is either a half-leaf
         * or an empty leaf.
         */
        tnode = n;
    }
    if (!is_leaf_node(tnode)) {
        int items, diff;

        n = tnode->left ? tnode->left : tnode->right;
        items = tnode_num_keys(n);

        /*
         * If half-leaf can not be merged with a leaf,
         * the proccess is completed.
         */
        if (items > (ttree->keys_per_tnode - tnode_num_keys(tnode))) {
            return ret;
        }

        if (tnode_get_side(n) == TNODE_RIGHT) {
            /*
             * Merge current node with its right leaf. Items from the leaf
             * are placed after the maximum item in a node.
             */
            diff = (ttree->keys_per_tnode - tnode->max_idx - items) - 1;
            if (diff < 0) {
                memcpy(tnode->keys + tnode->min_idx + diff,
                       tnode->keys + tnode->min_idx, sizeof(void *) *
                       tnode_num_keys(tnode));
                tnode->min_idx += diff;
                tnode->max_idx += diff;
                if (cursor->tnode == tnode) {
                    cursor->idx += diff;
                }
            }
            memcpy(tnode->keys + tnode->max_idx + 1, n->keys + n->min_idx,
                   sizeof(void *) * items);
            tnode->max_idx += items;
        }
        else {
            /*
             * Merge current node with its left leaf. Items the leaf
             * are placed before the minimum item in a node.
             */
            diff = tnode->min_idx - items;
            if (diff < 0) {
                register int i;

                for (i = tnode->max_idx; i >= tnode->min_idx; i--) {
                    tnode->keys[i - diff] = tnode->keys[i];
                }

                tnode->min_idx -= diff;
                tnode->max_idx -= diff;
                if (cursor->tnode == tnode) {
                    cursor->idx -= diff;
                }
            }

            memcpy(tnode->keys + tnode->min_idx - items, n->keys + n->min_idx,
                   sizeof(void *) * items);
            tnode->min_idx -= items;
        }

        n->min_idx = 1;
        n->max_idx = 0;
        tnode = n;
    }
    if (!tnode_is_empty(tnode)) {
        return ret;
    }

    /* if we're here, then current node will be removed from the tree. */
    n = tnode->parent;
    if (!n) {
        ttree->root = NULL;
        free(tnode);
        return ret;
    }

    n->sides[tnode_get_side(tnode)] = NULL;
    fixup_after_deletion(ttree, tnode, NULL);
    free(tnode);
    return ret;
}

int ttree_replace(Ttree *ttree, void *key, void *new_item)
{
    TtreeCursor cursor;

    if (!ttree_lookup(ttree, &cursor, key))
        return -1;

    cursor.tnode->keys[cursor.idx] = ttree_item2key(ttree, new_item);
    return 0;
}

int ttree_cursor_open_on_node(TtreeCursor *cursor, Ttree *tree,
                              TtreeNode *tnode, enum tnode_seek seek)
{
    TTREE_ASSERT(cursor != NULL);
    TTREE_ASSERT(tree != NULL);

    memset(cursor, 0, sizeof(*cursor));
    cursor->ttree = tree;
    cursor->tnode = tnode;

    /*
     * If T*-tree node was specified, the cursor becomes
     * ready for iteration. Otherwise we suppose that T*-tree
     * is completely empty, so it becomes ready for insertion.
     * In second case seek argument is ignored.
     */
    if (tnode) {
        switch (seek) {
            case TNODE_SEEK_START:
                cursor->idx = tnode->min_idx;
                break;
            case TNODE_SEEK_END:
                cursor->idx = tnode->max_idx;
                break;
            default:
                SET_ERRNO(EINVAL);
                return -1;
        }

        cursor->state = CURSOR_OPENED;
    }
    else {
        TTREE_ASSERT(cursor->ttree->root == NULL);
        cursor->idx = first_tnode_idx(cursor->ttree);
        cursor->state = CURSOR_PENDING;
    }

    cursor->side = TNODE_BOUND;
    return 0;
}

int ttree_cursor_open(TtreeCursor *cursor, Ttree *ttree)
{
    return ttree_cursor_open_on_node(cursor, ttree, ttree->root,
                                     TNODE_SEEK_START);
}

int ttree_cursor_first(TtreeCursor *cursor)
{
    TtreeNode *tnode;
    int ret = 0;

    TTREE_ASSERT(cursor != NULL);
    TTREE_ASSERT(cursor->ttree != NULL);
    cursor->side = TNODE_BOUND;
    cursor->state = CURSOR_OPENED;
    tnode = ttree_node_leftmost(cursor->ttree->root);
    if (UNLIKELY(tnode == NULL)) {
        if (LIKELY(cursor->ttree->root != NULL)) {
            cursor->idx = cursor->ttree->root->min_idx;
            cursor->tnode = cursor->ttree->root;
        }
        else {
            cursor->idx = first_tnode_idx(cursor->ttree);
            cursor->state = CURSOR_PENDING;
            cursor->tnode = NULL;
            ret = -1;
        }
    }
    else {
        cursor->tnode = tnode;
        cursor->idx = tnode->min_idx;
    }

    return ret;
}

int ttree_cursor_last(TtreeCursor *cursor)
{
    TtreeNode *tnode;
    int ret = 0;

    TTREE_ASSERT(cursor != NULL);
    TTREE_ASSERT(cursor->ttree != NULL);

    cursor->state = CURSOR_OPENED;
    cursor->side = TNODE_BOUND;
    tnode = ttree_node_rightmost(cursor->ttree->root);
    if (UNLIKELY(tnode == NULL)) {
        if (LIKELY(cursor->ttree->root != NULL)) {
            cursor->tnode = cursor->ttree->root;
            cursor->idx = cursor->tnode->max_idx;
        }
        else {
            cursor->idx = first_tnode_idx(cursor->ttree);
            cursor->state = CURSOR_PENDING;
            cursor->tnode = NULL;
            ret = -1;
        }
    }
    else {
        cursor->tnode = tnode;
        cursor->idx = tnode->max_idx;
    }

    return ret;
}

int ttree_cursor_next(TtreeCursor *cursor)
{
    TTREE_ASSERT(cursor != NULL);
    TTREE_ASSERT(cursor->ttree != NULL);
    TTREE_ASSERT(cursor->tnode != NULL);

    if (UNLIKELY(cursor->state == CURSOR_CLOSED)) {
        return TCSR_END;
    }
    if (UNLIKELY(cursor->state == CURSOR_PENDING)) {
        cursor->state = CURSOR_OPENED;
        if ((cursor->side == TNODE_LEFT) ||
            (cursor->idx < cursor->tnode->min_idx)) {
            cursor->side = TNODE_BOUND;
            cursor->idx = cursor->tnode->min_idx;
            return TCSR_OK;
        }
        else if (cursor->side == TNODE_BOUND) {
            return TCSR_OK;
        }
        else if ((cursor->side == TNODE_RIGHT) ||
                 (cursor->idx > cursor->tnode->max_idx)) {
            cursor->idx = cursor->tnode->max_idx;
        }
    }

    /*
     * In case when maximum key of the T*-tree node is reached,
     * the next item will be the very first(minumum) kery of
     * its successor node. Because of nature of T*-tree we always
     * has direct access to successor of each tree node.
     */
    cursor->side = TNODE_BOUND;
    if (cursor->idx == cursor->tnode->max_idx) {
        if (cursor->tnode->successor) {
            cursor->tnode = cursor->tnode->successor;
            cursor->idx = cursor->tnode->min_idx;
            return TCSR_OK;
        }

        return TCSR_END;
    }

    cursor->idx++;
    return TCSR_OK;
}

int ttree_cursor_prev(TtreeCursor *cursor)
{
    TTREE_ASSERT(cursor != NULL);
    TTREE_ASSERT(cursor->ttree != NULL);

    if (UNLIKELY(cursor->state == CURSOR_CLOSED)) {
        return TCSR_END;
    }
    if (UNLIKELY(cursor->state == CURSOR_PENDING)) {
        cursor->state = CURSOR_OPENED;
        if ((cursor->side == TNODE_RIGHT) ||
            (cursor->idx > cursor->tnode->max_idx)) {
            cursor->side = TNODE_BOUND;
            cursor->idx = cursor->tnode->max_idx;
            return TCSR_OK;
        }
        else if ((cursor->side == TNODE_LEFT) ||
                 (cursor->idx < cursor->tnode->min_idx)) {
            cursor->side = TNODE_BOUND;
            cursor->idx = cursor->tnode->min_idx;
        }
    }

    cursor->side = TNODE_BOUND;
    if (cursor->idx == cursor->tnode->min_idx) {
        /*
         * When cursor reaches the minimum index in a T*-tree
         * node, a previous item would be the very last(maximum)
         * key in the greatest lower bound of given node.
         */
        TtreeNode *n = ttree_node_glb(cursor->tnode);

        if (n == NULL) {
            /*
             * If given node has not greatest lower bound(I.e. it hasn't
             * even its left child), we have to determine an accestor
             * of given node such that it's a right child of its parent.
             * The parent of accestor we found will be the previous node
             * of given one.
             */
            for (n = cursor->tnode; n->parent &&
                     n->parent->left == n; n = n->parent);
            if (!n->parent) {
                return TCSR_END;
            }


            n = n->parent;
        }

        cursor->tnode = n;
        cursor->idx = cursor->tnode->max_idx;
        return TCSR_OK;
    }

    cursor->state = CURSOR_OPENED;
    cursor->idx--;
    return TCSR_OK;
}

static void __print_tree(TtreeNode *tnode, int offs,
                         void (*fn)(TtreeNode *tnode))
{
    int i;

    for (i = 0; i < offs; i++)
        printf(" ");
    if (!tnode) {
        printf("(nil)\n");
        return;
    }
    if (tnode_get_side(tnode) == TNODE_LEFT)
        printf("[L] ");
    else if (tnode_get_side(tnode) == TNODE_RIGHT)
        printf("[R] ");
    else
        printf("[*] ");

    printf("\n");
    for (i = 0; i < offs + 1; i++)
        printf(" ");

    printf("<%d> ", tnode_num_keys(tnode));
    if (fn) {
        fn(tnode);
    }

    __print_tree(tnode->left, offs + 1, fn);
    __print_tree(tnode->right, offs + 1, fn);
}

static int __ttree_get_depth(TtreeNode *tnode)
{
   int l, r;

   if (!tnode) {
        return 0;
   }

   l = __ttree_get_depth(tnode->left);
   r = __ttree_get_depth(tnode->right);
   if (tnode->left) {
       l++;
   }
   if (tnode->right) {
       r++;
   }

   return ((r > l) ? r : l);
}

int ttree_get_depth(Ttree *ttree)
{
    return __ttree_get_depth(ttree->root);
}

void ttree_print(Ttree *ttree, void (*fn)(TtreeNode *tnode))
{
    __print_tree(ttree->root, 0, fn);
}

