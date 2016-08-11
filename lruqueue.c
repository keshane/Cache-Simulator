#include <stdlib.h>
#include "lruqueue.h"

unsigned int enqueue(Set *set, Node *node) {
    Node *tail;
    if ((tail = set->tail) != NULL) {
        tail->next = node;
        node->prev = tail;
        node->next = NULL;
        set->tail = node;
    }
    else {
        set->head = node;
        set->tail = node;
    }

    return 0;
}

Node *dequeue(Set *set) {
    if (set->head == set->tail) // return null if only one node in queue
        return NULL;
    Node *head = set->head;
    // Set second node to be head
    if (head->next != NULL) {
        (head->next)->prev = NULL;
        set->head = head->next;
    }
    else { // means we are dequeueing only node in queue
        set->head = NULL;
        set->tail = NULL;
    }

    head->next = NULL;
    head->prev = NULL;
    return head;
}

Node *rmvqueue(Set *set, unsigned int tag) {
    Node *node = set->head;
    while (node != NULL && node->tag != tag)
        node = node->next;

    if (node == NULL) return NULL; // tag doesn't exist

    if (node->prev == NULL) { // means node is head
        node = dequeue(set);
    }
    else if (node->next == NULL) { // means node is tail
        (node->prev)->next = NULL;
        node->prev = NULL;
        node->next = NULL;
        set->tail = node->prev;
    }
    else { // means node is in middle of list
        (node->prev)->next = node->next;
        (node->next)->prev = node->prev;
        node->prev = NULL;
        node->next = NULL;
    }

    return node;
}

unsigned int enqueue_head(Set *set, Node *node) {
    Node *head;
    if ((head = set->head) != NULL) {
        head->prev = node;
        node->next = head;
        node->prev = NULL;
        set->head = node;
    }
    else {
        set->head = node;
        set->tail = node;
    }

    return 0;
}

