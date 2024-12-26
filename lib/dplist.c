#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"

struct dplist_node
{
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist
{
    dplist_node_t *head;

    void *(*element_copy)(void *src_element);

    void (*element_free)(void **element);

    int (*element_compare)(void *x, void *y);
};

dplist_t *dpl_create( // callback functions
    void *(*element_copy)(void *src_element),
    void (*element_free)(void **element),
    int (*element_compare)(void *x, void *y))
{
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element)
{
    if (list == NULL || *list == NULL)
        return;
    dplist_node_t *current = (*list)->head;
    while (current != NULL)
    {
        dplist_node_t *temp = current;
        current = current->next;
        if (free_element && (*list)->element_free != NULL)
        {
            (*list)->element_free(&(temp->element));
        }
        free(temp);
    }
    free(*list);
    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy)
{
    if (list == NULL)
        return NULL;

    dplist_node_t *new_node = malloc(sizeof(dplist_node_t));
    if (new_node == NULL)
        return NULL;

    if (insert_copy && list->element_copy != NULL)
    {
        new_node->element = list->element_copy(element);
    }
    else
    {
        new_node->element = element;
    }

    if (list->head == NULL)
    {
        new_node->prev = NULL;
        new_node->next = NULL;
        list->head = new_node;
    }
    else if (index <= 0)
    {
        new_node->prev = NULL;
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
    }
    else
    {
        dplist_node_t *current = list->head;
        int i = 0;
        while (current->next != NULL && i < index)
        {
            current = current->next;
            i++;
        }
        if (i < index)
        {
            new_node->prev = current;
            new_node->next = NULL;
            current->next = new_node;
        }
        else
        {
            new_node->prev = current->prev;
            new_node->next = current;
            if (current->prev != NULL)
            {
                current->prev->next = new_node;
            }
            else
            {
                list->head = new_node;
            }
            current->prev = new_node;
        }
    }
    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element)
{
    if (list == NULL || list->head == NULL)
        return list;

    dplist_node_t *current = list->head;

    if (index <= 0)
    {
        list->head = current->next;
        if (list->head != NULL)
        {
            list->head->prev = NULL;
        }
    }
    else
    {
        int i = 0;
        while (current->next != NULL && i < index)
        {
            current = current->next;
            i++;
        }
        if (current->prev != NULL)
        {
            current->prev->next = current->next;
        }
        if (current->next != NULL)
        {
            current->next->prev = current->prev;
        }
    }

    if (free_element && list->element_free != NULL)
    {
        list->element_free(&(current->element));
    }
    free(current);
    return list;
}

int dpl_size(dplist_t *list)
{
    if (list == NULL)
        return -1;
    int count = 0;
    dplist_node_t *current = list->head;
    while (current != NULL)
    {
        count++;
        current = current->next;
    }
    return count;
}

void *dpl_get_element_at_index(dplist_t *list, int index)
{
    if (list == NULL || list->head == NULL)
        return NULL;
    dplist_node_t *current = list->head;
    if (index <= 0)
    {
        return current->element;
    }
    int i = 0;
    while (current->next != NULL && i < index)
    {
        current = current->next;
        i++;
    }
    return current->element;
}

int dpl_get_index_of_element(dplist_t *list, void *element)
{
    if (list == NULL || list->head == NULL)
        return -1;
    int index = 0;
    dplist_node_t *current = list->head;
    while (current != NULL)
    {
        if (list->element_compare != NULL)
        {
            if (list->element_compare(current->element, element) == 0)
            {
                return index;
            }
        }
        else
        {
            if (current->element == element)
            {
                return index;
            }
        }
        current = current->next;
        index++;
    }
    return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index)
{
    if (list == NULL || list->head == NULL)
        return NULL;
    dplist_node_t *current = list->head;
    if (index <= 0)
    {
        return current;
    }
    int i = 0;
    while (current->next != NULL && i < index)
    {
        current = current->next;
        i++;
    }
    return current;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference)
{
    if (list == NULL || list->head == NULL || reference == NULL)
        return NULL;
    dplist_node_t *current = list->head;
    while (current != NULL)
    {
        if (current == reference)
        {
            return current->element;
        }
        current = current->next;
    }
    return NULL;
}
