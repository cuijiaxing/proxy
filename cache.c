#include "cache.h"



LNode cache_head = NULL;

void init_node(LNode node){
	node->content = NULL;
	node->size = 0;
	node->uri = NULL;
	node->next = NULL;
}
static long count = 0;
static size_t total_length = 1049000;
static size_t max_single_length = 102400;
sem_t q_mutex, v_mutex, r_mutex, time_mutex;
int read_count = 0;

long get_time(){
	P(&time_mutex);
	++count;
	V(&time_mutex);
	return count;
}
int set_node(LNode node, char* uri, char* content){
	size_t length = strlen(content);
	if(length > total_length){
		return -1;
	}
	node->time = get_time();
	node->uri = (char*)malloc(strlen(uri));
	node->content = (char*)malloc(length);
	strcpy(node->uri, uri);
	strcpy(node->content, content);
	total_length -= length;
	return 0;
}
void init_cache(){
	//alread cached
	if(cache_head != NULL){
		return;
	}
	cache_head = (LNode)malloc(sizeof(Node));
	init_node(cache_head);
	Sem_init(&q_mutex, 1, 1);
	Sem_init(&v_mutex, 1, 1);
	Sem_init(&r_mutex, 1, 1);
	Sem_init(&time_mutex, 1, 1);
}

void cache(char* uri, char* content, size_t n){
	P(&q_mutex);
	P(&v_mutex);
	if(strlen(content) <= max_single_length){
		while(strlen(content) > get_remaining_size()){
			evict();
		}
		if(strlen(content) <= get_remaining_size()){
			cache_it(uri, content, n);
		}
	}
	V(&v_mutex);
	V(&q_mutex);
	return;
}


char* visit(char* uri){
	char* result = NULL;
	P(&q_mutex);
	if(read_count == 0){
		P(&v_mutex);
	}
	++read_count;
	V(&q_mutex);
	result = find_cache(uri);

	P(&r_mutex);
	--read_count;
	if(read_count == 0){
		V(&v_mutex);
	}
	V(&r_mutex);
	return result;
}


void insert_node(LNode node){
	node->next = cache_head->next;
	cache_head->next = node;
}


void cache_it(char* uri, char* content, size_t n){
	LNode node = (LNode)malloc(sizeof(Node));
	init_node(node);
	node->uri = (char*)malloc(sizeof(char) * strlen(uri));
	node->content = (char*)malloc(sizeof(char) * n);
	strcpy(node->uri, uri);
	strncpy(node->content, content, n);
	insert_node(node);
	decrease_size(n);
}

char* find_cache(char* uri){
	LNode temp_head = cache_head->next;
	while(temp_head){
		if(!strcmp(temp_head->uri, uri)){
			break;
		}
		temp_head = temp_head->next;
	}
	if(temp_head){
		temp_head->time = get_time();
		return temp_head->content;
	}
	return NULL;
}

void decrease_size(size_t t){
	total_length -= t;
}
void increase_size(size_t t){
	total_length += t;
}

size_t get_remaining_size(){
	return total_length;
}

void evict(){
	LNode prev_head = cache_head;
	LNode current_node = cache_head->next;
	LNode current_prev = cache_head;
	LNode result_node = NULL;
	long min_time = -1;
	while(current_node){
		if(current_node->time < min_time || min_time == -1){
			result_node = current_node;
			current_prev = prev_head;
			min_time = current_node->time;
		}
		prev_head = current_node;
		current_node = current_node->next;
	}
	if(result_node == NULL){
		return;
	}
	current_prev->next = result_node->next;
	increase_size(strlen(result_node->content));
	free_node(result_node);
}

void free_node(LNode node){
	free(node->content);
	free(node->uri);
	free(node);
}
