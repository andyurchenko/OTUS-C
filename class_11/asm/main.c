#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "error.h"

static bool parity(int64_t number);
static void extract_odds(const node * src_node, node *dest_node, bool (*callback)(int64_t));
static void print_int(int64_t number);
static void print_list_forward(const node *node, void (*callback)(int64_t));
static void print_list_backward(const node *node, void (*callback)(int64_t));

int main(void) {
    const int64_t arr[6] = {4, 8, 15, 16, 23, 42};

    node_list *list = list_create();
    if (list == NULL) {
        PRINT_ERROR("Cannot create a new list.");
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 6; i++) {
        int64_t *value = malloc(sizeof(*value));
        *value = arr[i];
        if (list_push(list, value) == -1) {
            PRINT_ERROR("Cannot add a new element to the list.");
            return EXIT_FAILURE;
        }
    }
    print_list_forward(list->head, print_int);

    node_list *odd_list = list_create();
    if (odd_list == NULL) {
        PRINT_ERROR("Cannot create a new list.");
        return EXIT_FAILURE;
    }

    extract_odds(list->head->next_node, odd_list->head, parity);
    print_list_backward(odd_list->tail, print_int);

    list_destroy(&list);
    list_destroy(&odd_list);

    return EXIT_SUCCESS;
}

static void print_list_forward(const node *node, void (*callback)(int64_t)) {
    if (node == NULL) {
        printf("\n");
        return;
    }

    if (node->value) {
        callback(*(int64_t*)node->value);
    }

    print_list_forward(node->next_node, callback);
}

static void print_list_backward(const node *node, void (*callback)(int64_t)) {
    if (node == NULL) {
        printf("\n");
        return;
    }

    if (node->value) {
        callback(*(int64_t*)node->value);
    }

    print_list_backward(node->prev_node, callback);
}

static bool parity(const int64_t number) {
    return number & 1;
}

static void print_int(const int64_t number) {
    printf("%" PRId64 " ", number);
    fflush(NULL);
}

static node *add_element(const void * const src_v, node * const n) {
    node *new_node = malloc(sizeof *new_node);
    if (new_node == NULL) {
        PRINT_ERROR("Cannot create a new node for a list.");
        return NULL;
    }

    int64_t *dest_v = malloc(sizeof *dest_v);
    if (new_node == NULL) {
        PRINT_ERROR("Allocation memory for a new node value failed.");
        return NULL;
    }

    *dest_v = *(int64_t*)src_v;
    new_node->value = dest_v;
    node *tail = n->next_node;
    n->next_node = new_node;
    new_node->prev_node = n;
    new_node->next_node = tail;
    tail->prev_node = new_node;

    return new_node;
}

static void extract_odds(const node * const src_node, node *dest_node, bool (*callback)(int64_t)) {
    if (src_node == NULL || src_node->value == NULL) {
        return;
    }

    if (callback(*(int64_t*)src_node->value)) {
        dest_node = add_element(src_node->value, dest_node);
    }

    extract_odds(src_node->next_node, dest_node, callback);
}

