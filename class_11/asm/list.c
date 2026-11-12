//
// Created by admn on 11/12/26.
//

#include "list.h"
#include "error.h"

#include <stdlib.h>


node_list * list_create(void) {
    node_list *l = malloc(sizeof(node_list));
    if (l == NULL) {
        PRINT_ERROR("Memory allocation for a list failed.");
        return NULL;
    }
    l->head = malloc(sizeof(node));
    l->tail = malloc(sizeof(node));
    if (l->head == NULL || l->tail == NULL) {
        PRINT_ERROR("Memory allocation for a new head/tail failed.");
        return NULL;
    }

    l->head->value = NULL;
    l->head->prev_node = NULL;
    l->tail->value = NULL;
    l->tail->next_node = NULL;
    l->head->next_node = l->tail;
    l->tail->prev_node = l->head;
    l->size = 0;

    return l;
}

void list_destroy(node_list **l) {
    if (l == NULL || *l == NULL) {
        return;
    }

    node *n = (*l)->head->next_node;
    while (n != (*l)->tail) {
        node *del = n;
        n = n->next_node;
        free(del->value);
        free(del);
    }
    free((*l)->head);
    free((*l)->tail);
    free(*l);
    *l = NULL;
}

int list_push(node_list *l, void *value) {
    node *left_node = l->tail->prev_node;
    node *middle_node = malloc(sizeof(node));
    if (middle_node == NULL) {
        PRINT_ERROR("Allocation memory failed.");
        return LIST_ERR_ADDING_NEW_NODE;
    }

    middle_node->value = value;
    left_node->next_node = middle_node;
    middle_node->next_node = l->tail;
    l->tail->prev_node = middle_node;
    middle_node->prev_node = left_node;
    l->size++;

    return OK;
}
