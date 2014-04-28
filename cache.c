#include "cache.h"
#define CC_DEBUG


LNode cache_head = NULL;
#ifdef CC_DEBUG
int output_fd = -1;
#endif
static long count = 0;
static size_t total_length = 1049000;
static size_t max_single_length = 102400;
sem_t q_mutex, v_mutex, r_mutex, time_mutex, revise_time_mutex;
int read_count = 0;

#ifdef CC_DEBUG
void cc_log(char* message){
	write(output_fd, message, strlen(message));
}
#endif 
long get_time(){
	int temp = 0;
	P(&time_mutex);
	++count;
	temp = count;
	V(&time_mutex);
	return temp;
}
void init_node(LNode node){
	node->content = NULL;
	node->size = 0;
	node->uri = NULL;
	node->next = NULL;
	node->time = get_time();
	#ifdef CC_DEBUG
	output_fd = open("log.txt", O_WRONLY);
	#endif
}
void decrease_size(size_t t){
	total_length -= t;
}
void increase_size(size_t t){
	total_length += t;
}


void very_beginning(){
	Sem_init(&q_mutex, 1, 1);
	Sem_init(&v_mutex, 1, 1);
	Sem_init(&r_mutex, 1, 1);
	Sem_init(&time_mutex, 1, 1);
	Sem_init(&revise_time_mutex, 1, 1);
}
int init_cache(){
	//alread cached
	if(cache_head != NULL){
		return 0;
	}
	very_beginning();
	cache_head = (LNode)malloc(sizeof(Node));
	if(cache_head == NULL){
		return -1;
	}
	init_node(cache_head);
	return 0;
}

void cache(char* uri, char* content, size_t n){
	P(&q_mutex);
	P(&v_mutex);
	if(n <= max_single_length){
		while(n > get_remaining_size()){
			evict();
		}
		if(n <= get_remaining_size()){
			cache_it(uri, content, n);
		}
	}
	V(&v_mutex);
	V(&q_mutex);
	return;
}

LNode  visit(char* uri){
	LNode result = NULL;
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


int cache_it(char* uri, char* content, size_t n){
	LNode node = (LNode)malloc(sizeof(Node));
	if(node == NULL){
		return -1;
	}
	init_node(node);
	node->uri = (char*)malloc(sizeof(char) * (strlen(uri) + 1));
	if(node->uri == NULL){
		return -1;
	}
	if(node->uri == NULL){
		return -1;
	}

	node->content = (char*)malloc(sizeof(char) * n);
	if(node->content == NULL){
		return -1;
	}
	node->size = n;
	strcpy(node->uri, uri);
	memcpy(node->content, content, n);
	insert_node(node);
	decrease_size(n);
	printf("*********************cached******************\n");
	printf("%s, size=%zd, remaining size=%zd\n", uri, n, get_remaining_size());
	printf("*********************end***********************\n");
	return 0;
}

//copy a node to return to server
//to avoid race condition such as when 
//it is returned, it is evicted before the 
//server access it
LNode copy_node(LNode node){
	LNode result_node = (LNode)malloc(sizeof(Node));
	if(result_node == NULL){
		return result_node;
	}
	init_node(result_node);
	result_node->time = node->time;
	result_node->size = node->size;
	result_node->content = (char*)malloc(sizeof(char) * node->size);
	if(result_node->content == NULL){
		return NULL;
	}
	result_node->uri = (char*)malloc((strlen(node->uri) + 1) * sizeof(char));
	if(result_node->uri == NULL){
		return NULL;
	}
	strcpy(result_node->uri, node->uri);
	memcpy(result_node->content, node->content, node->size);
	return result_node;
}

LNode find_cache(char* uri){
	LNode temp_head = cache_head->next;
	LNode result_node = NULL;
	//printf("******************look begin*************\n");
	while(temp_head){
		//printf("%s\n", temp_head->uri);
		if(!strcmp(temp_head->uri, uri)){
			break;
		}
		temp_head = temp_head->next;
	}
	//printf("******************look end*************\n");
	if(temp_head){
		//race condition
		P(&revise_time_mutex);
		temp_head->time = get_time();
		result_node = copy_node(temp_head);
		V(&revise_time_mutex);
		return result_node;
	}
	return NULL;
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
	increase_size(result_node->size);
	free_node(result_node);
}

void free_node(LNode node){
	if(node == NULL){
		return ;
	}
	if(node->content != NULL){
		free(node->content);
		node->content = NULL;
	}
	if(node->uri != NULL){
		free(node->uri);
		node->uri = NULL;
	}
	free(node);
	printf("free successfully\n");
}
