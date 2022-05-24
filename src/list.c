#include <stdlib.h>

#include "flrl/list.h"

struct list_item {
    struct list_item *next;
    struct list_item *prev;
    void *data;
};

struct list {
    struct list_item *head;
    struct list_item *tail;
    size_t count;
    void (*free_func)(void *);
    void (*freepp_func)(void **);
};

struct list *list_new(void (*free_func)(void *), void (*freepp_func)(void **))
{
    struct list *list = calloc(1, sizeof *list);
    list->free_func = free_func;
    list->freepp_func = freepp_func;

    return list;
}

void list_delete(struct list **plist)
{
    struct list *list = *plist;
    struct list_item *item, *next;

    for (item = list->head; item; item = next) {
        next = item->next;

        if (list->free_func)
            list->free_func(item->data);
        else if (list->freepp_func)
            list->freepp_func(&item->data);

        free(item);
    }

    free(list);
    *plist = NULL;
}

void *list_shift(struct list *list)
{
    struct list_item *item = list->head;
    void *data;

    if (!item) return NULL;

    list->head = item->next;
    if (list->head)
        list->head->prev = NULL;
    else
        list->tail = NULL;

    list->count--;

    data = item->data;
    free(item);

    return data;
}

void *list_pop(struct list *list)
{
    struct list_item *item = list->tail;
    void *data;

    if (!item) return NULL;

    list->tail = item->prev;
    if (list->tail)
        list->tail->next = NULL;
    else
        list->head = NULL;
    list->count--;

    data = item->data;
    free(item);

    return data;
}

void list_unshift(struct list *list, void *data)
{
    struct list_item *item;

    item = calloc(1, sizeof *item);
    item->next = list->head;
    item->data = data;

    if (list->head)
        list->head->prev = item;
    list->head = item;
    if (!list->tail)
        list->tail = item;

    list->count++;
}

void list_push(struct list *list, void *data)
{
    struct list_item *item;

    item = calloc(1, sizeof *item);
    item->prev = list->tail;
    item->data = data;

    if (list->tail)
        list->tail->next = item;
    list->tail = item;
    if (!list->head)
        list->head = item;

    list->count++;
}

void *list_head(struct list *list)
{
    if (list->head)
        return list->head->data;

    return NULL;
}

void *list_tail(struct list *list)
{
    if (list->tail)
        return list->tail->data;

    return NULL;
}

size_t list_count(const struct list *list)
{
    return list->count;
}

void list_foreach(struct list *list, void (*cb)(void *, void *), void *rock)
{
    struct list_item *item;

    for (item = list->head; item; item = item->next) {
        cb(item->data, rock);
    }
}

void list_rforeach(struct list *list, void (*cb)(void *, void *), void *rock)
{
    struct list_item *item;

    for (item = list->tail; item; item = item->prev) {
        cb(item->data, rock);
    }
}
