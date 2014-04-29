/*
 *This the multi-thread proxy
 *I will create a thread for each incoming request
 *It will first look into the cache, if hit, it will
 * direct fetch from the cache, otherwise it will
 * send request to server and get response
 * if the size of the object is appropriate,
 * then I will cathe it.
 * I will ignore to store the web object like
 * the return status is not 200
 * and the header contains no-cache
 *
 * */

#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


static int verbose = 1;

/* You won't lose style points for including these long lines in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static char *connection_hdr = "Connection: close\r\n";
static char *proxy_connection_hdr = "Proxy-connection: close\r\n";

void* doit(void* param);
void print_errno(int err);
int parse_uri(char* uri, char* filename);
void serve_static(int fd, char* filename, int filesize);
void get_filetype(char* filename, char* filetype);
void clienterror(int fd, char* cause, char* errnum, 
		char* shortmsg, char* longmsg);
int get_server_name_and_content(char* fileName,
	 char* server_name, char* content);

void cc_log(char* message){
	/*
	FILE* file = fopen("log.txt", "a");
	fputs("curl -v --proxy bambooshark.ics.cs.cmu.edu:46854 ", file); 
	fputs(message, file);
	fputs("\r\n", file);
	fclose(file);
	*/
}

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);
	int connfd, port, clientlen, listenfd;
	pthread_t thread;
	struct sockaddr_in clientaddr;

	if(argc != 2){
		fprintf(stderr, "useage: %s<port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);

	//init cache
	init_cache();
	
	listenfd = Open_listenfd(port);
	while(1){
		clientlen = sizeof(clientaddr);
		connfd = accept(listenfd, (SA*)&clientaddr, (socklen_t*)&clientlen);
		if(connfd < 0){
			if(verbose){
				fprintf(stderr, "accept error\n");
				print_errno(errno);
			}
			continue;
		}
		int* param = (int*)malloc(sizeof(int));
		if(param == NULL){
			if(verbose){
				fprintf(stderr, "no memory to start the child process\n");
			}
			close(connfd);
			continue;
		}
		*param = connfd;
		//create child thread
		if(pthread_create(&thread, NULL, doit, (void*)param) != 0){
			if(verbose){
				fprintf(stderr, "create child process failed!\n");
			}
			close(connfd);
		}
	}
	Close(listenfd);
}
char request[MAXLINE];

int sendit(int fd, char* host, char* hdr, char* message){
	char buffer[MAXLINE];
	size_t n;
	sprintf(buffer, "GET %s HTTP/1.0\r\n", message);
	strcat(buffer, hdr);
	//printf("request: %s", buffer);
	if((n = rio_writen(fd, buffer, strlen(buffer))) < 0){
		if(verbose){
			fprintf(stderr, "send get request failed\n");
		}
		return -1;
	}
	return 0;
}

void print_errno(int err){
	switch(err){
		case ENOBUFS:
			printf("no enough buf\n");
			break;
		case ENOMEM:
			printf("no enough mem\n");
			break;
		case EMFILE:
			printf("per process limit reached\n");
			break;
		default:
			printf("no known\n");
			break;
	}
	printf("errno = %d\n", err);
	return;
}


int send_request_to_server(int client_fd,
	 char* server, char* hdr, char* message,
	 	 int port, char* uri, int is_static){
	rio_t rio;
	//we don't want to use Open_clientfd because it will terminate the program
	int fd = open_clientfd(server, port);
	if(fd < 0){
		if(verbose){
			printf("connect to server failed\n");
			clienterror(client_fd, "GET", "404", "Server not Found", 
				"I cannot find it!");
		}
		return -1;
	}
	if(sendit(fd, server, hdr, message) < 0){
		if(verbose){
			printf("send message failed\n");
		}
		close(fd);
		return -1;
	}
	Rio_readinitb(&rio, fd);
	char buffer[MAXLINE];
	size_t n;
	char temp_cache[MAX_OBJECT_SIZE];	
	memset(temp_cache, 0, sizeof(temp_cache));
	size_t response_size = 0;
	char* increase_temp_cache = temp_cache;
	int should_cache = 1;
	while((n = rio_readlineb(&rio, buffer, MAXLINE)) > 0){
		if(rio_writen(client_fd, buffer, n) < 0){
			close(fd);
			return -1;
		}
		response_size += n;
		if(response_size <= MAX_OBJECT_SIZE){
			memcpy(increase_temp_cache, buffer, n);
		}
		increase_temp_cache += n;
		//we cannot cache these kind of web objects
		//according to the protocol
		if(strstr(buffer, "Cache-Control: no-cache")){
			should_cache = 0;
		}
		if(!strcmp(buffer, "\r\n")){
			break;
		}
	}
	if( n < 0){
		close(fd);
		return -1;
	}
	while((n = rio_readnb(&rio, buffer, MAXLINE)) > 0){
		response_size += n;
		if(should_cache && response_size < MAX_OBJECT_SIZE){
			memcpy(increase_temp_cache, buffer, n);
			increase_temp_cache += n;
		}
		if(rio_writen(client_fd, buffer, n) < 0){
			close(fd);
			return -1;
		}
	}
	if(n < 0){
		close(fd);
		return -1;
	}
	if(response_size <= MAX_OBJECT_SIZE &&
		is_static && should_cache
		&& response_size > 0){
		//we don't care about if an object is successfully cached
		cache(uri, temp_cache, response_size);
		printf("cached:%s\n", uri);
		printf("size=%zd\n", response_size);
	}
	printf("******************************\n");
	printf("%s\n", uri);
	printf("total size = %zd\n", response_size);
	printf("******************************\n");
	close(fd);
	return 0;
}


void* doit(void* param){
	//int* a = (int*)100;
	//*a = 100;
	pthread_detach(pthread_self());
	if(param == NULL){
		return NULL;
	}
	int fd = *((int*)param);
	if(param != NULL){
		free(param);
	}
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE],
					 version[MAXLINE], revised_hdr[MAXLINE];
	char filename[MAXLINE];
	rio_t rio;
	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	if(verbose){
		printf("************requested uri***************\n");
		printf("%s\n", buf);
		printf("**************end requested uri**********\n");
	}
	sscanf(buf, "%s %s %s", method, uri, version);
	cc_log(uri);
	if(strcasecmp(method, "GET")){
		clienterror(fd, method, "501", "Not Implemented", 
			"Tiny does not implement this method");
		close(fd);
		return NULL;
	}
	printf("%s\n", uri);
	int is_static = parse_uri(uri, filename);
	LNode cache_node = NULL;
	if(is_static){
		cache_node = visit(uri);
	}
	//find cached object
	if(cache_node != NULL){
		printf("send from cache, size = %zd\n", cache_node->size);
		if(rio_writen(fd, cache_node->content, cache_node->size) < 0){
			free_node(cache_node);
			close(fd);
			return NULL;
		}
		close(fd);
		printf("%s\n", cache_node->uri);
		free_node(cache_node);
		printf("end send from cache\n");
		return NULL;
	}
	char server_name[MAXLINE];
	char content[MAXLINE];
	int port = 80;
	if((port = get_server_name_and_content(filename,
							 			server_name, content)) < 0){
		if(verbose){
			fprintf(stderr, "get server name failed\n");
		}
		close(fd);
		return NULL;
	}
	//collect
	//read init bytes
	memset(revised_hdr, 0, sizeof(revised_hdr));
	int n;
	int has_agent = 0, has_accept = 0, has_encoding = 0,
			 has_connection = 0, has_proxy = 0, has_host = 0;
	while((n = rio_readlineb(&rio, buf, MAXLINE)) > 0 && strcmp(buf, "\r\n")){
		if(strstr(buf, "User-Agent") != NULL){
			if(!has_agent){
				strcat(revised_hdr, user_agent_hdr);
				has_agent = 1;
			}
		}else
		if(strstr(buf, "Accept-Encoding") != NULL){
			if(!has_encoding){
				strcat(revised_hdr, accept_hdr);
				has_encoding = 1;
			}
		}else
		if(strstr(buf, "Accept") == NULL){
			if(!has_accept){
				strcat(revised_hdr, accept_encoding_hdr);
				has_accept = 1;
			}
		}else
		if(strstr(buf, "Connection") != NULL){
			if(!has_connection){
				strcat(revised_hdr, connection_hdr);
				has_connection = 1;
			}
		}else
		if(strstr(buf, "Proxy-Connection") != NULL){
			if(!has_proxy){
				strcat(revised_hdr, proxy_connection_hdr);
				has_proxy = 1;
			}
		}else
		if(strstr(buf, "HOST") != NULL){
			has_host = 1;
		}
		else{
			strcat(revised_hdr, buf);
		}
	}
	if(n < 0){
		if(verbose){
				fprintf(stderr, "read request hdr failed\n");
			}
			close(fd);
			return NULL;
	}
	if(!has_agent){
		strcat(revised_hdr, user_agent_hdr);
	}
	if(!has_encoding){
		strcat(revised_hdr, accept_hdr);
	}
	if(!has_accept){
		strcat(revised_hdr, accept_encoding_hdr);
	}
	if(!has_connection){
		strcat(revised_hdr, connection_hdr);
	}
	if(!has_proxy){
		strcat(revised_hdr, proxy_connection_hdr);
	}
	if(!has_host){
		strcat(revised_hdr, "HOST: ");
		strcat(revised_hdr, server_name);
		strcat(revised_hdr, "\r\n");
	}
	strcat(revised_hdr, "\r\n");
	send_request_to_server(fd, server_name, 
					revised_hdr, content, port, uri, is_static);
	close(fd);
	return NULL;
}


int get_server_name_and_content(char* fileName, 
						char* server_name, char* content){
	int port = 80;
	sscanf(fileName, "%*[^:]://%[^/]%s", server_name, content);
	if(strlen(server_name) == 0){
		sscanf(fileName, "%[^/]%s", server_name, content);
	}
	if(strlen(content) == 0){
		strcpy(content, "/");
	}
	if(strstr(server_name, ":")){
		char buf[MAXLINE];
		strcpy(buf, server_name);
		sscanf(buf, "%[^:]:%d", server_name, &port);
	}
	if(verbose){
		printf("file name = %s\n", fileName);
		printf("server name = %s\n", server_name);
		printf("content = %s\n", content);
		printf("port = %d\n", port);
	}
	return port;
}

int parse_uri(char* uri, char* filename){
	filename[0] = '\0';
	strcat(filename, uri);
	if(uri[strlen(uri) - 1] == '/'){
		strcat(filename, "");
	}
	if(strstr(filename, "?")){
		return 0;
	}else{
		return 1;
	}
}



void clienterror(int fd, char* cause, char* errnum, 
		char* shortmsg, char* longmsg){
	char buf[MAXLINE], body[MAXBUF];

	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	if(rio_writen(fd, buf, strlen(buf)) < 0){
		close(fd);
		return;
	}
	sprintf(buf, "Content-type: text/html\r\n");
	if(rio_writen(fd, buf, strlen(buf)) < 0){
		close(fd);
		return;
	}
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	if(rio_writen(fd, buf, strlen(buf)) < 0){
		close(fd);
		return;
	}
	if(rio_writen(fd, body, strlen(body)) < 0){
		close(fd);
		return;
	}
}
