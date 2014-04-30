/*
 *This is the cache for proxy
 *I use a linked list to store the cached values
 *I will inserted the newly cached web object at the 
 *beginning of the list
 *I use LRU to evict the node according to their time stamp
 *andrew id: jiaxingc
 *author: Jiaxing Cui
 * */
#include "cache.h"


LNode cache_head = NULL;
static long count = 0;
static size_t total_length = MAX_CACHE_SIZE;
static size_t max_single_length = MAX_OBJECT_SIZE;
pthread_mutex_t q_mutex, v_mutex, r_mutex, time_mutex, revise_time_mutex;
int read_count = 0;


//get current time
long get_time(){
	int temp = 0;
	pthread_mutex_lock(&time_mutex);
	++count;
	temp = count;
	pthread_mutex_unlock(&time_mutex);
	return temp;
}

//init a node to fault values
void init_node(LNode node){
	node->content = NULL;
	node->size = 0;
	node->uri = NULL;
	node->next = NULL;
	node->time = get_time();
}

//decrease the size of available memory
void decrease_size(size_t t){
	total_length -= t;
}

//increase the size of available memory
void increase_size(size_t t){
	total_length += t;
}

//init mutex at the very beginning
void very_beginning_init_mutex(){
	//waiting queue 
	pthread_mutex_init(&q_mutex, NULL);
	//writer has to wait on this
	pthread_mutex_init(&v_mutex, NULL);
	//update read count mutex
	pthread_mutex_init(&r_mutex, NULL);
	//update time
	pthread_mutex_init(&time_mutex, NULL);
	//revise time
	pthread_mutex_init(&revise_time_mutex, NULL);
}

//init the cache
//return 0 if success
//return -1 if failure
int init_cache(){
	//alread initialized
	if(cache_head != NULL){
		return 0;
	}
	very_beginning_init_mutex();
	cache_head = (LNode)malloc(sizeof(Node));
	if(cache_head == NULL){
		return -1;
	}
	init_node(cache_head);
	return 0;
}


//cache an object with lock
void cache(char* uri, char* content, size_t n){
	if(n <= 0){
		return;
	}
	pthread_mutex_lock(&q_mutex);
	pthread_mutex_lock(&v_mutex);
	if(n <= max_single_length){
		while(n > get_remaining_size()){
			evict();
		}
		if(n <= get_remaining_size()){
			cache_it(uri, content, n);
		}
	}
	pthread_mutex_unlock(&v_mutex);
	pthread_mutex_unlock(&q_mutex);
	return;
}

//try to find if a uri is in the cache
//return NULL if not found
LNode  visit(char* uri){
	LNode result = NULL;
	pthread_mutex_lock(&q_mutex);
	if(read_count == 0){
		pthread_mutex_lock(&v_mutex);
	}
	++read_count;
	pthread_mutex_unlock(&q_mutex);
	//parallel find 
	result = find_cache(uri);
	pthread_mutex_lock(&r_mutex);
	--read_count;
	if(read_count == 0){
		pthread_mutex_unlock(&v_mutex);
	}
	pthread_mutex_unlock(&r_mutex);
	return result;
}

//insert a node into the cache list
void insert_node(LNode node){
	node->next = cache_head->next;
	cache_head->next = node;
}

//cache a web object
//return negavive number on failuer
//retur 0 if success
int cache_it(char* uri, char* content, size_t n){
	LNode node = (LNode)malloc(sizeof(Node));
	if(node == NULL){
		return -1;
	}
	init_node(node);
	//padding 1 for '\0'
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
	return 0;
}

//copy a node to return to server
//to avoid race condition such as when 
//it is returned, it is evicted before the 
//server access it
//return NULL on failure
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
	result_node->uri = (char*)malloc((strlen(node->uri) + 1) 
						* sizeof(char));
	if(result_node->uri == NULL){
		return NULL;
	}
	strcpy(result_node->uri, node->uri);
	memcpy(result_node->content, node->content, node->size);
	return result_node;
}


//find if the uri is cache
//return NULL if not found
LNode find_cache(char* uri){
	LNode temp_head = cache_head->next;
	LNode result_node = NULL;
	while(temp_head){
		if(!strcmp(temp_head->uri, uri)){
			break;
		}
		temp_head = temp_head->next;
	}
	if(temp_head){
		//try to avoid race condition
		pthread_mutex_lock(&revise_time_mutex);
		temp_head->time = get_time();
		result_node = copy_node(temp_head);
		pthread_mutex_unlock(&revise_time_mutex);
		return result_node;
	}
	return NULL;
}



//get available cache size
size_t get_remaining_size(){
	return total_length;
}


//evict node from the cache
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


//release memory
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
}
