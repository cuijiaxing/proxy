#include "csapp.h"
#include<time.h>

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
void init_cache();
void cache_it(char* uri, char* content, size_t n);
LNode visit(char* uri);
void cache(char*uri, char* content, size_t n);
LNode find_cache(char* uri);
void evict();
void free_node(LNode node);
void insert_node(LNode node);
void decrease_size(size_t t);
void increase_size(size_t t);
size_t get_remaining_size();

