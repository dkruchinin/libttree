#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "ttree.h"

struct item {
     int key;
};

static int __cmpfunc(void *key1, void *key2)
{
    return (*(int *)key1 - *(int *)key2);
}

static void usage(const char *appname)
{
     fprintf(stderr, "Usage: %s <positive number of keys>\n", appname);
     exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
     int num_keys, i, ret;
     Ttree ttree;
     TtreeNode *tnode;
     struct item *all_items, *cur_item;

     if (argc != 2)
          usage(argv[0]);

     num_keys = atoi(argv[1]);
     if (num_keys <= 0)
          usage(argv[0]);

     srandom(time(NULL));
     printf("Generating %d random numbers...\n", num_keys);
     all_items = calloc(num_keys, sizeof(all_items));
     printf("{ ");
     for (i = 0; i < num_keys; i++) {
          all_items[i].key = (int)(random() % (4096 - 1));
          printf("%d ", all_items[i].key);
     }

     printf("}\n");
     printf("Inserting keys to the tree...\n");
     ret = ttree_init(&ttree, 8, false, __cmpfunc, struct item, key);
     if (ret < 0) {
          fprintf(stderr, "Failed to initialize T*-tree. [ERR=%d]\n", ret);
          free(all_items);
          exit(EXIT_FAILURE);
     }
     for (i = 0; i < num_keys; i++) {
          ret = ttree_insert(&ttree, &all_items[i]);
          if (ret < 0) {
               fprintf(stderr, "Failed to insert item %d with key %d! [ERR=%d]\n",
                       i, all_items[i].key, ret);
               free(all_items);
               exit(EXIT_FAILURE);
          }
     }

     printf("Sorted keys:\n");
     printf("{ ");
     tnode = ttree_node_leftmost(ttree.root);
     while (tnode) {
          tnode_for_each_index(tnode, i) {
               printf("%d ", *(int *)tnode_key(tnode, i));
          }

          tnode = tnode->successor;
     }

     printf("}\n");
     ttree_destroy(&ttree);
     free(all_items);
     exit(EXIT_SUCCESS);
}
