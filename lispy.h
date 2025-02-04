#ifndef _LISPY_H
#define _LISPY_H

#include <unistd.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "lispy_magic.h"

#define HASH_MAGIC 0x26FA947ACCD562BB

#define foreach(cursor, m_node)					\
	for(cursor = m_node; cursor; cursor = cursor->next)

#define with_open_file(fd, filename, flags, mode)			\
	for(int fd = open(filename, flags, mode), _stop = 1; _stop; _stop = 0, close(fd))

typedef struct nodestr {
	void *data;
	struct nodestr *next;
}node_t;

typedef struct{
	node_t **array;
	uint32_t size;
	uint32_t length;
}hash_t;

typedef struct {
	void *key;
	void *value;
	uint8_t allocated;
	uint32_t hash_key;
}hash_entry_t;


static node_t * cons(void *car, node_t *cdr)
{
	node_t *node = malloc(sizeof(node));

	node->data = car;
	node->next = cdr;

	return node;
}
static void ll_push(node_t **n, void *data)
{
	*n = cons(data, *n);
}

static void * ll_pop(node_t **n)
{
	void *data = (*n)->data;
	node_t *next = (*n)->next;

	free(*n);
	*n = next;

	return data;
}
static int ll_rm(node_t **n, node_t *del_n)
{
	while(*n != NULL) {
		if(*n == del_n) {
			*n = del_n->next;
			free(del_n);
			return 0;
		} else {
			n = &(*n)->next;
		}
	}
	return -1;
}

static void ll_free(node_t **n)
{
	node_t *next;

	if(!*n)
		return;

	do {
		next = (*n)->next;
		free(*n);
		*n=next;
	} while (next);
	free(*n);
	*n = NULL;

	return;
}

static inline void * car(node_t *n)
{
	return n->data;
}
static inline void * carw(node_t *n, void *data)
{
	return n->data = data;
}
static inline node_t * cdr(node_t *n)
{
	return n->next;
}
static inline node_t * cdrw(node_t *n, node_t *next)
{
	return n->next = next;
}
static inline void * cadr(node_t *n)
{
	return cdr(n)->data;
}
static inline void * cadrw(node_t *n, void *data)
{
	return cdr(n)->data = data;
}
static inline void * nth(node_t *n, int nelem)
{
	node_t *cursor;

	foreach(cursor, n) {
		if(!nelem--)
			return cursor->data;
	}
	return 0;
}
static inline void nthw(node_t *n, int nelem, void *data)
{
	node_t *cursor;

	foreach(cursor, n) {
		if(!nelem--) {
			cursor->data = data;
			return;
		}
	}
}

static inline void map(node_t *n, void *(*f)())
{
	node_t *cursor;

	foreach(cursor, n) {
		f(car(n));
		n = cdr(n);
	}
}

#define mmapa(argtypename, statement, array, size) do {			\
		for(int i = 0; i < size; ++i) {				\
			argtypename = &array[i];			\
			statement;					\
		}							\
	} while (0);

static inline void mapa(void *(*f)(), void *array, uint32_t size, uint8_t elem_size)
{
	for(int i=0; i < size; ++i)
		f(array + i*elem_size);
}

static inline node_t * mapcar(node_t *n, node_t *(*f)())
{
	node_t *cursor;
	node_t *list = NULL;
	node_t **head = &list;
	foreach(cursor, n) {
		*head = f(car(n));
	        head = &(*head)->next;
	}
	return list;
}


static hash_t * new_hash(uint8_t n)
{
	hash_t *hash = malloc(sizeof(hash_t));

	hash->size = 1 << n;
	hash->array = malloc(hash->size * sizeof(uint64_t));
	memset(hash->array, 0, hash->size * sizeof(uint64_t));
	return hash;
}

static uint32_t fhash_raw(char *key)
{
	uint32_t hash_key = 0;
	uint8_t status = 0x5a;
	uint32_t i = 0;

	while (*key) {
		hash_key ^= ((uint64_t *)hash_magic)[(*(key++) ^ ++i ^ status) & 255];
		status = ((uint8_t *)&hash_key)[0] ^ ((uint8_t *)&hash_key)[3];
	}

	return hash_key;
}

static inline hash_entry_t ** hash_get(hash_t *hash, char *key)
{
	uint32_t hash_key = fhash_raw(key);
	uint32_t bucket = hash_key & (hash->size -1);
	node_t *p = hash->array[bucket];

	while (p) {
		if (((hash_entry_t *)(p->data))->hash_key == hash_key)
			return ((hash_entry_t **) &(p->data));
		else
			p = p->next;
	}
	return NULL;

}

/* Setting value_len saves value as a pointer without copying the contents */
static void hash_put(hash_t *hash, char *key, void *value, uint32_t value_len)
{
	uint32_t hash_32 = fhash_raw(key);
	uint32_t bucket = hash_32 & (hash->size -1);

	hash_entry_t *entry = malloc(sizeof(hash_entry_t));
	hash_entry_t **h;

	entry->key = malloc(strlen(key) + 1);
	entry->hash_key = hash_32;
        strcpy(entry->key, key);

	if(value_len) {
		entry->value = malloc(value_len);
		memcpy(entry->value, value, value_len);
		entry->allocated = 1;
	} else {
		entry->value = value;
	}

	if (NULL == (h = hash_get(hash, key))) {
		ll_push(&(hash->array[bucket]), entry);
	} else {
		free((*h)->key);
		free(*h);
		*h = entry;
	}
}

static inline void * hash_get_value(hash_t *hash, char *key)
{
	hash_entry_t **ret = hash_get(hash, key);
	if (ret)
		return (*ret)->value;
	else
		return NULL;
}
static void * hash_key_free(hash_t *hash, char *key)
{
	uint32_t hash_key = fhash_raw(key);
	uint32_t bucket = hash_key & (hash->size - 1);

	node_t **p = &hash->array[bucket];
	while (*p) {
		if (((hash_entry_t *)((*p)->data))->hash_key == hash_key &&
		    0 == strcmp(((hash_entry_t *)((*p)->data))->key, key)) {
			free(((hash_entry_t *)((*p)->data))->key);
			if(((hash_entry_t *)((*p)->data))->allocated)
				free(((hash_entry_t *)((*p)->data))->value);
			free((*p)->data);
			*p = (*p)->next;
		} else {
			p = &(*p)->next;
		}
	}
	return NULL;
}
#endif
