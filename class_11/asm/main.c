#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"
#include "error.h"

static bool parity(int64_t number);
static error_status extract_odds(const node_list *src_list, node_list *dest_list);
static void print_int(int64_t number);
static void print_list_forward(const node_list *list);
static void print_list_backward(const node_list *list);

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
    print_list_forward(list);

    node_list *odd_list = list_create();
    if (odd_list == NULL) {
        PRINT_ERROR("Cannot create a new list.");
        return EXIT_FAILURE;
    }

    if (extract_odds(list, odd_list) == OK) {
        print_list_backward(odd_list);
    }

    list_destroy(&list);
    list_destroy(&odd_list);

    return EXIT_SUCCESS;
}

static void print_list_forward(const node_list *list) {
    if (list == NULL || list->head) {
        return;
    }

    const node *n = list->head->next_node;
    while (n) {
        if (n->value) {
            print_int(*(int64_t*)n->value);
        }
        n = n->next_node;
    }
    printf("\n");
}

static void print_list_backward(const node_list *list) {
    if (list == NULL || list->head) {
        return;
    }

    const node *n = list->tail->prev_node;
    while (n) {
        if (n->value) {
            print_int(*(int64_t*)n->value);
        }
        n = n->prev_node;
    }
    printf("\n");
}

static bool parity(const int64_t number) {
    return number & 1;
}

static void print_int(const int64_t number) {
    printf("%" PRId64 " ", number);
    fflush(NULL);
}

static error_status extract_odds(const node_list * const src_list, node_list *dest_list) {
    if (src_list == NULL) {
        PRINT_ERROR("Source list is null");
        return LIST_ERR_INVALID_ARG;
    }

    if (dest_list == NULL) {
        PRINT_ERROR("Destination list is null");
        return LIST_ERR_INVALID_ARG;
    }

    if (src_list->head == NULL || src_list->head->next_node == NULL) {
        PRINT_ERROR("Source list has an invalid head node.");
        return LIST_ERR_INVALID_NODE;
    }

    const node *src_node = src_list->head->next_node;
    while (src_node && src_node != src_list->tail) {
        if (src_node->value && parity(*(int64_t*)src_node->value)) {
            int64_t *v = malloc(sizeof(*v));
            if (v == NULL) {
                PRINT_ERROR("Allocation memory for a new node value failed.");
                return MEMORY_ALLOCATION_ERROR;
            }
            *v = *(int64_t*)src_node->value;
            list_push(dest_list, v);
        }
        src_node = src_node->next_node;
    }

    return OK;
}
