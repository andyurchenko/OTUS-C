//
// Created by admn on 11/12/26.
//

#ifndef ASM_LIST_H
#define ASM_LIST_H
#include <stddef.h>

typedef struct s_node node;
struct s_node {
    void *value;
    node *next_node;
    node *prev_node;
};

typedef struct s_list {
    node *head;
    node *tail;
    size_t size;
} node_list;


node_list *list_create(void);
void  list_destroy(node_list **l);
int  list_push(node_list *l, void *value);

#endif //ASM_LIST_H
