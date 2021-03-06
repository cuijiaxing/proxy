/**
*This is the header file for cache
*andrew id: jiaxingc
*author: Jiaxing Cui
***/
#ifndef __CACHE_H__
#define __CACHE_H__
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct node{
	long time;
	char* content;
	size_t size;
	char* uri;
	struct node* next;
}Node, *LNode;

void init_node(LNode node);
long get_time();
int set_node(LNode node, char* uri, char* content);
int init_cache();
int cache_it(char* uri, char* content, size_t n);
LNode visit(char* uri);
void cache(char*uri, char* content, size_t n);
LNode find_cache(char* uri);
void evict();
void free_node(LNode node);
void insert_node(LNode node);
void decrease_size(size_t t);
void increase_size(size_t t);
size_t get_remaining_size();
#endif
