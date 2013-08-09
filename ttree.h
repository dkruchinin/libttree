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
 */

/**
 * @file ttree.h
 * @author Dan Kruchinin
 * @brief T*-tree API defenitions and constants
 *
 * For more information about T- and T*-trees see:
 * 1) Tobin J. Lehman , Michael J. Carey,
 *    A Study of Index Structures for Main Memory Database Management Systems
 * 2) Kong-Rim Choi , Kyung-Chang Kim,
 *    T*-tree: a main memory database index structure for real time applications
 */

#ifndef __TTREE_H__
#define __TTREE_H__

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "ttree_defs.h"

#define TCSR_END -1
#define TCSR_OK   0

enum {
    TNODE_UNDEF = -1, /**< T*-tree node side is undefined */
    TNODE_LEFT,       /**< Left side */
    TNODE_RIGHT,      /**< Right side */
};

enum tnode_seek {
    TNODE_SEEK_START,
    TNODE_SEEK_END,
};

enum ttree_cursor_state {
    CURSOR_CLOSED = 0,
    CURSOR_OPENED,
    CURSOR_PENDING,
};

#define TNODE_ROOT  TNODE_UNDEF /**< T*-tree node is root */
#define TNODE_BOUND TNODE_UNDEF /**< T*-tree node bounds searhing value */

/**
 * @brief T*-tree node structure.
 *
 * T*-tree node has an array of items, link to its parent
 * node and two links to its childs: left and right one.
 * A node with N keys can be drawn as following:
 * <pre>
 *          [^]        <- pointer to parent
 *     [k1, ... kN]    <- Array of items
 *         /  \
 *       (L)  (R)      <- Pointers to left and right childs.
 * </pre>
 *
 * T*-tree node also has a pointer to its successor, i.e.
 * another node following it in sorted order. This feature allows
 * easily represent whole tree as a sorted list by taking the lefmost
 * tree's node and traveling through its sucessors list.
 *
 * T*-tree defines three type of nodes:
 * 1) Node that hasn't left and right child is called "leaf node".
 * 2) Node that has only one child is called "half-leaf node".
 * 3) Finally, node that has both left and right childs
 *    is called "internal node"
 *
 * @see Ttree
 */
typedef struct ttree_node {
    struct ttree_node *parent;     /**< Pointer to node's parent */
    struct ttree_node *successor;  /**< Pointer to node's soccussor */
    union {
        struct ttree_node *sides[2];
        struct  {
            struct ttree_node *left;   /**< Pointer to node's left child  */
            struct ttree_node *right;  /**< Pointer to node's right child */
        };
    };
    union {
        uint32_t pad;
        struct {
            signed min_idx     :12;  /**< Index of minimum item in node's array */
            signed max_idx     :12;  /**< Index of maximum item in node's array */
            signed bfc         :4;   /**< Node's balance factor */
            unsigned node_side :4;  /**< Node's side(TNODE_LEFT, TNODE_RIGHT or TNODE_ROOT) */
        };
    };

    /**
     * First two items of T*-tree node keys array
     */
    void *keys[TNODE_ITEMS_MIN];
} TtreeNode;

typedef int (*ttree_cmp_func_fn)(void *key1, void *key2);
typedef void (*ttree_callback_fn)(TtreeNode *tnode, void *arg);

/**
 * @brief T*-tree structure
 */
typedef struct ttree {
    TtreeNode *root;            /**< A pointer to T*-tree root node */
    ttree_cmp_func_fn cmp_func; /**< User-defined key comparing function */
    size_t key_offs;            /**< Offset from item to its key(may be 0) */
    int keys_per_tnode;         /**< Number of keys per each T*-tree node */

    /**
     * The field is true if keys in a tree supposed to be unique
     */
    bool keys_are_unique;
} Ttree;

typedef struct ttree_cursor {
    Ttree *ttree;
    TtreeNode *tnode;     /**< A pointer to T*-tree node */
    int idx;              /**< Particular index in a T*-tree node array */
    int side;             /**< T*-tree node side. Used when item is inserted. */
    enum ttree_cursor_state state;
} TtreeCursor;

/**
 * @brief Get size of T*-tree node in bytes.
 * @param ttree - a pointer to Ttree.
 * @return size of TtreeNode in a tree in bytes,
 */
#define tnode_size(ttree)                                               \
    (sizeof(TtreeNode) + (ttree->keys_per_tnode - \
                          TNODE_ITEMS_MIN) * sizeof(uintptr_t))

#define tnode_num_keys(tnode)                   \
    (((tnode)->max_idx - (tnode)->min_idx) + 1)

#define tnode_is_empty(tnode)                   \
    (!tnode_num_keys(tnode))

#define tnode_is_full(ttree, tnode)                     \
    (tnode_num_keys(tnode) == (ttree)->keys_per_tnode)

#define ttree_key2item(ttree, key)                  \
    ((void *)((char *)(key) - (ttree)->key_offs))

#define ttree_item2key(ttree, item)                 \
    ((void *)((char *)(item) + (ttree)->key_offs))

#define tnode_key(tnode, idx)                   \
    ((tnode)->keys[(idx)])

#define tnode_key_min(tnode) tnode_key(tnode, (tnode)->min_idx)

#define tnode_key_max(tnode) tnode_key(tnode, (tnode)->max_idx)

#define ttree_node_glb(tnode)                    \
    __tnode_get_bound(tnode, TNODE_LEFT)

#define ttree_node_lub(tnode)                    \
    __tnode_get_bound(tnode, TNODE_RIGHT)

#define ttree_node_leftmost(tnode)               \
    __tnode_sidemost(tnode, TNODE_LEFT)

#define ttree_node_rightmost(tnode)              \
    __tnode_sidemost(tnode, TNODE_RIGHT)

#define tnode_for_each_index(tnode, iter)                               \
    for ((iter) = (tnode)->min_idx; (iter) <= (tnode)->max_idx; (iter)++)

static __inline void tnode_set_side(TtreeNode *tnode, int side)
{
    tnode->node_side &= ~0x3;
    tnode->node_side |= (side + 1);
}

static __inline int tnode_get_side(TtreeNode *tnode)
{
    return ((tnode->node_side & 0x03) - 1);
}

#define ttree_is_empty(ttree)                   \
    (!(ttree)->root)

/**
 * @brief Initialize new T*-tree.
 * @param ttree[out]  - A pointer to T*-tree structure for initialization
 * @param num_keys    - A number of keys per T*-tree node.
 * @param is_unique   - A boolean to determine whether keys must be unique.
 * @param cmpf        - A pointer to user-defined comparison function
 * @param data_struct - Structure containing an item that will be
 *                      used by T*-tree as a key.
 * @param key_field   - Name of a key field in a @a data_struct.
 * @return 0 on success, -1 on error.
 * @see __ttree_init
 */
#define ttree_init(ttree, num_keys, is_unique, cmpf, data_struct, key_field) \
    __ttree_init(ttree, num_keys, is_unique, cmpf,                      \
                 offsetof(data_struct, key_field))

/**
 * @brief More detailed T*-tree initialization.
 * @param ttree[out] - A pointer to T*-tree to initialize
 * @param num_keys   - A number of keys per T*-tree node.
 * @param is_unique  - A boolean to determine whether keys must be unique.
 * @param cmpf       - User defined comparison function
 * @param key_offs   - Offset from item structure start to its key field.
 * @return 0 on success, -1 on error.
 * @see ttree_init
 */
int __ttree_init(Ttree *ttree, int num_keys, bool is_unique,
                 ttree_cmp_func_fn cmpf, size_t key_offs);

/**
 * @brief Destroy whole T*-tree
 * @param ttree - A pointer to tree to destroy.
 * @see ttree_init
 */
void ttree_destroy(Ttree *ttree);

/**
 * @fn void *ttree_lookup(Ttree *ttree, void *key, TtreeCursor *cursor)
 * @brief Find an item by its key in a tree.
 *
 * This function allows to find an item in a tree by item's key.
 * Also it is used for searching a place where new item with a key @a key
 * should be inserted. All necessary information are stored in a @a tnode_meta
 * if meta is specified.
 *
 * @param ttree           - A pointer to T*-tree where to search.
 * @param key             - A pointer to search key.
 * @param tnode_meta[out] - A pointer to T*-tree node meta, where results meta is searched.(may be NULL)
 * @return A pointer to found item or NULL if item wasn't found.
 * @see tnode_meta_t
 */
void *ttree_lookup(Ttree *ttree, void *key, TtreeCursor *cursor);

/**
 * @brief Insert an item @a item in the T*-tree @ttree
 *
 * ttree_insert function inserts given item @a item in the T*-tree.
 * If tree already contains a key euqual to the key of inserting item,
 * error is returned.
 *
 * @param ttree - A pointer to a tree.
 * @param item  - A pointer to item that will be inserted.
 * @return 0 if all is ok, negative value if item's key is duplicated.
 */
int ttree_insert(Ttree *ttree, void *item);

/**
 * @brief Delete an item from a T*-tree by item's key.
 * @param ttree - A pointer to tree.
 * @param key   - A pointer to item's key.
 * @return A pointer to removed item or NULL item with key @a key wasn't found.
 */
void *ttree_delete(Ttree *ttree, void *key);

void ttree_insert_at_cursor(TtreeCursor *cursor, void *item);

/**
 * @brief "Placeful" item insertion in a T*-tree.
 *
 * This function allows to insert new intems in a T*-tree specifing
 * they precise places using T*-tree node meta information.
 * T*-tree nodes meta may be known by searching a key position in a tree
 * using ttree_lookup function. Even if key is not found, tnode_meta_t
 * will hold its *possible* place. For example ttree_insert dowsn't allow
 * to insert the duplicates. But using a boundle of ttree_lookup and ttree_insert_placeful
 * this limitation can be wiped out. After ttree_lookup is done, metainformation is
 * saved in tnode_meta and this meta may be used for placeful insertion.
 * Metainformation after insertion metainformation(idx with tnode) may be used for
 * accessing to item or its key in a tree. Note that ttree_insert_placeful *may*
 * change the meta if and only if item position is changed during insertion proccess.
 * @warning @a tnode_meta shouldn't be modified directly.
 *
 * @param ttree           - A pointer to T*-tree where to insert.
 * @param tnode_meta[out] - A pointer to T*-tree node meta information(filled by ttree_lookup)
 * @param item            - An item to insert.
 *
 * @see tnode_meta_t
 * @see ttree_lookup
 * @see ttree_delete_placeful
 */
//void ttree_insert_placeful(TtreeCursor *cursor, void *item);

/**
 * @brief "Placeful" item deletion from a T*-tree.
 *
 * ttree_delete_placeful use similar to ttree_insert_placeful approach. I.e. it uses
 * metainformation structure filled by ttree_lookup for item deletion. Since tnode_meta_t
 * contains target T*-tree node and precise place of item in that node, this information
 * may be used for deletion.
 *
 * @param ttree      - A pointer to a T*-tree item will be removed from.
 * @param tnode_meta - T*-tree node metainformation structure filled by ttree_lookup.
 * @return A pointer to removed item.
 *
 * @see tnode_meta_t
 * @see ttree_lookup
 * @see ttree_delete_placeful
 */
void *ttree_delete_at_cursor(TtreeCursor *cursor);

/**
 * @brief Replace an item saved in a T*-tree by a key @a key.
 * It's an atomic operation that doesn't requires any rebalancing.
 *
 * @param ttree    - A pointer to a T*-tree.
 * @param key      - A pointer to key whose item will be replaced.
 * @param new_item - A pointer to new item that'll replace previous one.
 * @return 0 if all is ok or negative value if @a key wasn't found.
 */
int ttree_replace(Ttree *ttree, void *key, void *new_item);

int ttree_cursor_open_on_node(TtreeCursor *cusrsor, Ttree *tree,
                              TtreeNode *tnode, enum tnode_seek seek);
int ttree_cursor_open(TtreeCursor *cursor, Ttree *ttree);
int ttree_cursor_next(TtreeCursor *cursor);
int ttree_cursor_prev(TtreeCursor *cursor);

#define ttree_cursor_copy(csr_dst, csr_src)         \
    memcpy(csr_dst, csr_src, sizeof(*(csr_src)))

static __inline void *ttree_key_from_cursor(TtreeCursor *cursor)
{
    if (LIKELY(cursor->state == CURSOR_OPENED)) {
        return tnode_key(cursor->tnode, cursor->idx);
    }

    return NULL;
}

static __inline void *ttree_item_from_cursor(TtreeCursor *cursor)
{
    void *key = ttree_key_from_cursor(cursor);

    if (!key) {
        return NULL;
    }

    return ttree_key2item(cursor->ttree, key);
}

/**
 * @brief Display T*-tree structure on a screen.
 * @param ttree - A pointer to a T*-tree.
 * @paran fn    - A pointer to function used for displaing T-tree node items
 * @warning Recursive function.
 */
void ttree_print(Ttree *ttree, void (*fn)(TtreeNode *tnode));

/*
 * Internal T*-tree functions.
 * Not invented for public usage.
 */
static __inline TtreeNode *__tnode_sidemost(TtreeNode *tnode, int side)
{
    if (!tnode) {
        return NULL;
    }
    else {
        TtreeNode *n;

        for (n = tnode; n->sides[side]; n = n->sides[side]);
        return n;
    }
}

static __inline TtreeNode *__tnode_get_bound(TtreeNode *tnode, int side)
{
    if (!tnode)
        return NULL;
    else {
        if (!tnode->sides[side])
            return NULL;
        else {
            TtreeNode *bnode;

            for (bnode = tnode->sides[side]; bnode->sides[!side];
                 bnode = bnode->sides[!side]);
            return bnode;
        }
    }
}

#endif /* !__TTREE_H__ */
