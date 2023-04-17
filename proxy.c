#include <stdio.h>

#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void doit(int fd);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; 

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
	doit(connfd);
	Close(connfd);
  }
}

void doit(int fd)
{
  int socket_fd;
  int is_static;
  char proxy_buf[MAXLINE], server_buf[MAXLINE] ,method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  char *host, *port, *path;
  rio_t rio_client;
  rio_t rio_server;

  host = "localhost";
  port = "4567";
  socket_fd = Open_clientfd(host, port); // 소켓 생성

  Rio_readinitb(&rio_client, fd);
  Rio_readinitb(&rio_server, socket_fd);

  Rio_readlineb(&rio_client, proxy_buf, MAXLINE); // 한줄을 읽어온다.
  printf("Request headers:\n");
  printf("%s", proxy_buf);
  sscanf(proxy_buf, "%s %s %s", method, uri, version);
  Rio_writen(socket_fd, proxy_buf, strlen(proxy_buf));

  while((strcmp(proxy_buf, "\r\n"))){
    Rio_readlineb(&rio_client, proxy_buf, MAXLINE);
    printf("%s", proxy_buf); // \n지워도됨
    Rio_writen(socket_fd, proxy_buf, strlen(proxy_buf));
  }

  while(Rio_readlineb(&rio_server, server_buf, MAXLINE) != 0){
    printf("%s\n", server_buf); // \n지워도됨
    Rio_writen(fd, server_buf, strlen(server_buf));
  }
}
