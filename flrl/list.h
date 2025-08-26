#ifndef FLRL_LIST_H
#define FLRL_LIST_H

#include "flrl/flrl.h"

struct list;

struct list *list_new(void (*free_func)(void *), void (*freepp_func)(void **));
void list_delete(struct list **plist);

void *list_shift(struct list *list);
void *list_pop(struct list *list);

void list_unshift(struct list *list, void *data);
void list_push(struct list *list, void *data);

void *list_head(struct list *list);
void *list_tail(struct list *list);

size_t list_count(const struct list *list);

void list_foreach(struct list *list, void (*cb)(void *, void *), void *rock);
void list_rforeach(struct list *list, void (*cb)(void *, void *), void *rock);

#endif
