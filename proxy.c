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

void doit(int fd);
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







int main(int argc, char** argv)
{
	int listenfd, connfd, port, clientlen;
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
		doit(connfd);
		Close(connfd);
	}
}
char request[MAXLINE];

void sendit(int fd, char* host, char* hdr, char* message){
	char* end  = "\r\n";
	char buffer[MAXLINE];
	sprintf(buffer, "GET /%s HTTP/1.0\r\n", message);
	printf("request: %s", buffer);
	Rio_writen(fd, buffer, strlen(buffer));
	Rio_writen(fd, hdr, strlen(hdr));
	Rio_writen(fd, end, strlen(end));
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
	Rio_readinitb(&rio, fd);
	if(fd < 0){
		printf("connect to server failed\n");
		return;
	}else{
		printf("connect to server succeed\n");
	}
	sendit(fd, server, hdr, message);
	char buffer[MAXLINE];
	while(Rio_readlineb(&rio, buffer, MAXLINE) != 0){
		Rio_writen(client_fd, buffer, strlen(buffer));
	}
	Close(fd);

}


void doit(int fd){
	int is_static;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], revised_hdr[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
	rio_t rio;

	Rio_readinitb(&rio, fd);
	Rio_readlineb(&rio, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);
	if(strcasecmp(method, "GET")){
		clienterror(fd, method, "501", "Not Implemented", 
			"Tiny does not implement this method");
		return;
	}

	is_static = parse_uri(uri, filename, cgiargs);
	char serverName[100];
	char content [100];
	get_server_name_and_content(filename, serverName, content);
	char* newServerName = get_rid_of_http(serverName);
	printf("new server:%s\n", newServerName);

	//collect
	//read init bytes
	memset(revised_hdr, 0, sizeof(revised_hdr));
	int has_agent = 0, has_accept = 0, has_encoding = 0, has_connection = 0, has_proxy = 0, has_host = 0;
	do{
		Rio_readlineb(&rio, buf, MAXLINE);
		if(! has_agent && strstr(buf, "User-Agent") == NULL){
			strcat(revised_hdr, user_agent_hdr);
			has_agent = 1;
		}else
		if(!has_encoding && strstr(buf, "Accept-Encoding") == NULL){
			strcat(revised_hdr, accept_hdr);
			has_encoding = 1;
		}else
		if(!has_accept && strstr(buf, "Accept") == NULL){
			strcat(revised_hdr, accept_encoding_hdr);
			has_accept = 1;
		}else
		if(!has_connection && strstr(buf, "Connection") == NULL){
			strcat(revised_hdr, connection_hdr);
			has_connection = 1;
		}else
		if(!has_proxy && strstr(buf, "Proxy-Connection") == NULL){
			strcat(revised_hdr, proxy_connection_hdr);
			has_proxy = 1;
		}else
		if(!has_host && strstr(buf, "HOST") == NULL){
			strcat(revised_hdr, "HOST: ");
			strcat(revised_hdr, newServerName);
			strcat(revised_hdr, "\r\n");
			has_host = 1;
		}
		else{
			strcat(revised_hdr, buf);
		}
	}while(strcmp(buf, "\r\n"));
	printf("-----------------start-----------------\n");
	printf("%s", revised_hdr);
	printf("------------------end----------------\n");

	send_request_to_server(fd, newServerName, revised_hdr, content, 80);
}

int find_slash(char* fileName){
	int length = strlen(fileName);
	for(int i = 0; i < length; ++i){
		if((i != 0) && (i != length - 1)){
			if((fileName[i] == '/') && (fileName[i - 1] != '/') && (fileName[i + 1] != '/')){
				return i;
			}
		}
	}
	return -1;
}

void get_server_name_and_content(char* fileName, char* serverName, char* content){
	int slash_index = find_slash(fileName);
	if(slash_index < 0){
		raise_error("bad url from get server name");
	}
	fileName[slash_index] = '\0';
	strcpy(serverName, fileName);
	strcpy(content, fileName + slash_index + 1);


	printf("server name = %s\n", serverName);
	printf("content = %s\n", content);
}


void read_requesthdrs(rio_t *rp){
	char buf[MAXLINE];

	Rio_readlineb(rp, buf, MAXLINE);
	printf("header starts\n");
	printf("%s", buf);
	while(strcmp(buf, "\r\n")){
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	printf("header ends\n");
	return;
}


int parse_uri(char* uri, char* filename, char* cgiargs){
	char* ptr;
	if(!strstr(uri, "cgi-bin")){
		strcpy(cgiargs, "");
		strcpy(filename, "");
		strcat(filename, uri);
		if(uri[strlen(uri) - 1] == '/'){
			strcat(filename, "home.html");
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
