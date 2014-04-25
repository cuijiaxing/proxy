#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including these long lines in your code */
static char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static char *connection_hdr = "Connection: close\r\n";
static char *proxy_connection_hdr = "Proxy-connection: close\r\n";

void* doit(void* param);
void read_requesthdrs(rio_t *rp);
int parse_uri(char* uri, char* filename, char* cgiargs);
void serve_static(int fd, char* filename, int filesize);
void get_filetype(char* filename, char* filetype);
void server_dynamic(int fd, char* cause, char* errnum, 
			char* shortmsg, char* longmsg);
void clienterror(int fd, char* cause, char* errnum, 
		char* shortmsg, char* longmsg);
void serve_dynamic(int fd, char* filename, char* cgiargs);
void get_server_name_and_content(char* fileName, char* serverName, char* content);
void raise_error(const char* error);
int get_port(char* server);







int main(int argc, char** argv)
{
	int listenfd, connfd, port, clientlen;
	pthread_t thread;
	struct sockaddr_in clientaddr;

	if(argc != 2){
		fprintf(stderr, "useage: %s<port>\n", argv[0]);
		exit(1);
	}
	port = atoi(argv[1]);
	
	listenfd = Open_listenfd(port);
	while(1){
		clientlen = sizeof(clientaddr);
		connfd = Accept(listenfd, (SA*)&clientaddr, (socklen_t*)&clientlen);
		int* param = (int*)malloc(sizeof(int));
		*param = connfd;
		printf("prev fd = %d\n", connfd);
		Pthread_create(&thread, NULL, doit, (void*)param);
	}
}
char request[MAXLINE];

void sendit(int fd, char* host, char* hdr, char* message){
	char* end  = "\r\n";
	char buffer[MAXLINE];
	sprintf(buffer, "GET /%s HTTP/1.0\r\n", message);
	//printf("request: %s", buffer);
	Rio_writen(fd, buffer, strlen(buffer));
	Rio_writen(fd, hdr, strlen(hdr));
	Rio_writen(fd, end, strlen(end));
	//printf("%s", hdr);
	printf("send message success\n");
}

char* get_rid_of_http(char* hostname){
	char* head = "http://";
	if(strlen(hostname) >= 7){
		for(int i = 0; i < 7; ++i){
			if(hostname[i] != head[i]){
				return hostname;
			}
		}
		return hostname + 7;
	}
	return hostname;
}


void send_request_to_server(int client_fd, char* server, char* hdr, char* message, int port){
	rio_t rio;
	int fd = Open_clientfd(server, port);
	if(fd < 0){
		printf("connect to server failed\n");
		return;
	}else{
		printf("connect to server succeed\n");
	}
	sendit(fd, server, hdr, message);
	Rio_readinitb(&rio, fd);
	char buffer[MAXLINE];
	size_t n;
	while((n = Rio_readnb(&rio, buffer, MAXLINE)) > 0){
		Rio_writen(client_fd, buffer, n);
		//printf("%s", buffer);
		memset(buffer, 0, sizeof(buffer));
	}
	if(strlen(buffer) > 0){
		Rio_writen(client_fd, buffer, strlen(buffer));
	}
	Close(fd);
}


void* doit(void* param){
	int fd = *((int*)param);
	printf("fd = %d\n", fd);
	free((int*)param);
	int is_static;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], revised_hdr[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	Pthread_detach(Pthread_self());

	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if(strcasecmp(method, "GET")){
		clienterror(fd, method, "501", "Not Implemented", 
			"Tiny does not implement this method");
		return NULL;
	}

	is_static = parse_uri(uri, filename, cgiargs);
	char serverName[MAXLINE];
	char content [MAXLINE];
	get_server_name_and_content(filename, serverName, content);
	char* newServerName = get_rid_of_http(serverName);
	printf("new server:%s\n", newServerName);

	//collect
	//read init bytes
	memset(revised_hdr, 0, sizeof(revised_hdr));
	int has_agent = 0, has_accept = 0, has_encoding = 0, has_connection = 0, has_proxy = 0, has_host = 0;
	do{
		Rio_readlineb(&rio, buf, MAXLINE);
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
		if(strstr(buf, "Proxy-Connection") == NULL){
			if(!has_proxy){
				strcat(revised_hdr, proxy_connection_hdr);
				has_proxy = 1;
			}
		}else
		if(strstr(buf, "HOST") == NULL){
			if(!has_host){
				strcat(revised_hdr, "HOST: ");
				strcat(revised_hdr, newServerName);
				strcat(revised_hdr, "\r\n");
				has_host = 1;
			}
		}
		else{
			strcat(revised_hdr, buf);
			printf("cat %s\n", buf);
		}
	}while(strcmp(buf, "\r\n"));
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
		strcat(revised_hdr, newServerName);
		strcat(revised_hdr, "\r\n");
	}
	printf("-----------------start-----------------\n");
	printf("%s", revised_hdr);
	printf("------------------end----------------\n");
	
	int port = get_port(newServerName);
	send_request_to_server(fd, newServerName, revised_hdr, content, port);
	return NULL;
}

int find_slash(char* fileName){
	int length = strlen(fileName);
	printf("file name length: %d\n", length);
	for(int i = 0; i < length; ++i){
		if(fileName[i] == '/'){
			if(i < length - 1 && fileName[i + 1] == '/'){
				continue;
			}
			if(i > 0 && fileName[i - 1] == '/'){
				continue;
			}
			return i;
		}
	}
	return -1;
}

void get_server_name_and_content(char* fileName, char* serverName, char* content){
	int slash_index = find_slash(fileName);
	printf("index = %d\n", slash_index);
	if(slash_index < 0){
		raise_error("bad url from get server name");
	}
	fileName[slash_index] = '\0';
	strcpy(serverName, fileName);
	strcpy(content, fileName + slash_index + 1);
	printf("file name = %s\n", fileName);
	printf("server name = %s\n", serverName);
	printf("content = %s\n", content);
}


void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	//printf("header starts\n");
	//printf("%s", buf);
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		//printf("%s", buf);
	}
	//printf("header ends\n");
	return;
}

int get_port(char* server){
	int port;
	char* start_ptr = NULL;
	if((start_ptr = strstr(server, ":")) != NULL){
		start_ptr[0] = '\0';
		start_ptr += 1;
		port = atoi(start_ptr);
	}
	//use the default port
	return 80;
}


int parse_uri(char* uri, char* filename, char* cgiargs){
	char* ptr;
	if(!strstr(uri, "cgi-bin")){
		strcpy(cgiargs, "");
		strcpy(filename, "");
		strcat(filename, uri);
		if(uri[strlen(uri) - 1] == '/'){
			strcat(filename, "");
		}
		return 1;
	}else{
		ptr = index(uri, '?');
		if(ptr){
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		}else
			strcpy(cgiargs, "");
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}

void serve_static(int fd, char* filename, int filesize){
	int srcfd;
	char* srcp, filetype[MAXLINE], buf[MAXBUF];

	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));

	srcfd = Open(filename, O_RDONLY, 0);
	srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	Munmap(srcp, filesize);
}

void get_filetype(char* filename, char* filetype){
	if(strstr(filename, ".html")){
		strcpy(filetype, "text/html");
	}else
	if(strstr(filename, ".gif")){
		strcpy(filetype, "image/gif");
	}else
	if(strstr(filename, ".jpg")){
		strcpy(filetype, "image/jped");
	}else{
		strcpy(filetype, "text/plain");
	}
}



void serve_dynamic(int fd, char* filename, char* cgiargs){
	char buf[MAXLINE], *emptylist[] = {NULL};

	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Server: Tiny Web Server\r\n");
	Rio_writen(fd, buf, strlen(buf));

	if(Fork() == 0){
		setenv("QUERY_STRING", cgiargs, 1);
		Dup2(fd, STDOUT_FILENO);

		Execve(filename, emptylist, environ);
	}
	Wait(NULL);
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
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}



void raise_error(const char* error){
	printf("fuck %s\n", error);
}
