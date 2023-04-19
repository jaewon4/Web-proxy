#include "csapp.h"
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio);
void parse_uri(char *uri, char *host, char* port, char *path);
void doit(int fd);
void *thread(void *vargp);

typedef struct cache_node{
  char *file_path;
  char *content;
  int content_length;
  struct cache_node *next, *prev;
} cache_node;

typedef struct cache{
  cache_node *head;
  cache_node *tail;
  int total_size;
} cache;

// struct cache를 전역으로 선언해준다. -> head, tail이 전역으로 움직임
struct cache *p;

cache_node *insert_cache(cache *p, char *path, char *contents, int content_length){
  cache_node *newNode = (cache_node*)calloc(1, sizeof(cache_node));
  newNode->content = contents;
  newNode->file_path = path;
  newNode->content_length = content_length;
  newNode->prev = NULL;
  newNode->next = NULL;
  if (content_length > MAX_OBJECT_SIZE) return;

  while(p->total_size + newNode->content_length > MAX_CACHE_SIZE){
    delete_cache(p->tail); // 여러개 지울수 있게 while
  }

  // newNode를 헤드랑 연결
  if (p->head == NULL){
    p->head = newNode;
    p->tail = newNode;
  }
  else{
    p->head->prev = newNode;
    newNode->next = p->head;
    p->head = newNode;
  }
  p->total_size += newNode->content_length;
}

// 링크드 리스트에 있는 케시를 조회하여 path를 가진 케시를 찾는다.
cache_node *find_cache(cache *p, char *path){
  cache_node *tmp = p->head;
  for (tmp; tmp != NULL; tmp = tmp->next){
    // 같으면 0 반환
    if (strcmp(tmp->file_path, path) == 0){
      return tmp;
    }
  }
  return NULL;
};

// 링크드리스트로 저장되어있는 케시에서 노드를 찾아서 분기를 해줌
// 찾은 노드를 변수로 받고 뽑은 노드를 맨앞노드로 만들어준다.
void hit_cache(cache_node *node, cache *p){
  // 첫번째 노드의 path가 내가 찾는 path와 같은경우
  if (p->head == node){
    
  }
  // 마지막 노드가 가지고 있는 path가 내가 찾는 path와 같은경우
  else if(p->tail == node){
    p->tail->prev->next = NULL;
    p->tail = node->prev;
    node->next = p->head;
    node->prev = NULL;
    p->head->prev = node;
    p->head = node;
    
  }
  // 링크드 리스트 중간의 노드가 같은경우
  else{
    node->next->prev = node->prev;
    node->prev->next = node->next;
    node->next = p->head;
    p->head->prev = node;
    node->prev = NULL;
    p->head = node;
  }
};

void delete_cache(cache *p){
  // 저장된 링크드리스트의 끝부분을 지워준다.(가장오래됬음)
  cache_node *tmp = NULL;
  tmp = p->tail;
  p->tail = p->tail->prev;
  tmp->prev->next = NULL;
  p->total_size -= tmp->content_length;
  free(tmp);
};

int main(int argc, char **argv) {
  int listenfd, *connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage server_addr;
  struct sockaddr_storage client_addr;
  pthread_t tid;
  
  p = (cache*)calloc(1, sizeof(cache));
  p->head = NULL;
  p->tail = NULL;
  p->total_size = 0;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(client_addr);
    connfd = Malloc(sizeof(int));
    *connfd = Accept(listenfd, (SA *)&client_addr, &clientlen);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    Pthread_create(&tid, NULL, thread, connfd);
  }
  printf("%s", user_agent_hdr);
  return 0;
}

void *thread(void *vargp){
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd);
    Close(connfd);
    return NULL;
}

void doit(int fd) {
  int socket_fd; // proxy 서버가 서버에게 보내는 소켓 식별자
  char client_buf[MAXLINE], server_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[MAXLINE], path[MAXLINE];
  rio_t client_rio, server_rio;
  // 캐시선언
  cache_node *node;
  char* client_path = (char*)malloc(MAXLINE);
  char *contents_buf = (char *)malloc(MAX_OBJECT_SIZE);

  // 1. 클라이언트로부터 요청을 수신한다 : client - proxy(server 역할)
  Rio_readinitb(&client_rio, fd); // client에게서 받은 연결 식별자를 변수로 넣고 client_rio를 초기화한다.
  Rio_readlineb(&client_rio, client_buf, MAXLINE);      // 클라이언트 요청 읽고 파싱
  sscanf(client_buf, "%s %s %s", method, uri, version);
  printf("===FROM CLIENT===\n");
  printf("Request headers:\n");
  printf("%s", client_buf);

  if (strcasecmp(method, "GET")) {  // GET 요청만 처리한다
    printf("Proxy does not implement the method\n");
    return;
  }

  // 2. 요청에서 목적지 서버의 호스트 및 포트 정보를 추출한다
  parse_uri(uri, host, port, path);

  strcpy(client_path, path);
  node = find_cache(p, client_path);
  if (node != NULL){
    //케시에 패스가 존재
    hit_cache(node, p);
    Rio_writen(fd, node->content, node->content_length);
    return;
  }

  // 3. 추출한 호스트 및 포트 정보를 사용하여 목적지 서버로 요청을 날린다
  socket_fd = Open_clientfd(host, port); // 소켓 생성과 동시에 연결
  sprintf(server_buf, "%s %s %s\r\n", method, path, version);
  printf("===TO SERVER===\n");
  printf("%s\n", server_buf);
  
  Rio_readinitb(&server_rio, socket_fd);
  modify_http_header(server_buf, host, port, path, &client_rio);  // 클라이언트로부터 받은 요청의 헤더를 수정하여 보냄
  Rio_writen(socket_fd, server_buf, strlen(server_buf));

  // 4. 서버로부터 응답을 읽어 클라이언트에 반환한다
  size_t n;
  while((n = Rio_readlineb(&server_rio, server_buf, MAXLINE)) != 0) {
    // printf("Proxy received %d bytes from server and sent to client\n", n);
    strcat(contents_buf, server_buf);
    Rio_writen(fd, server_buf, n);
  }

  insert_cache(p, client_path, contents_buf, MAX_OBJECT_SIZE);
  
  Close(socket_fd);
}

// 목적지 서버에 보낼 HTTP 요청 헤더로 수정하기
void modify_http_header(char *http_header, char *hostname, int port, char *path, rio_t *server_rio) {
  char buf[MAXLINE], other_hdr[MAXLINE], host_hdr[MAXLINE];
  sprintf(http_header, "GET %s HTTP/1.0\r\n", path);
 
  while(Rio_readlineb(server_rio, buf, MAXLINE) > 0) {
    if (strcmp(buf, "\r\n") == 0) break;

    if (!strncasecmp(buf, "Host", strlen("Host")))  // Host: 
    {
      strcpy(host_hdr, buf);
      continue;
    }

    if (strncasecmp(buf, "Connection", strlen("Connection"))
        && strncasecmp(buf, "Proxy-Connection", strlen("Proxy-Connection"))
        && strncasecmp(buf, "User-Agent", strlen("User-Agent")))
    {
      strcat(other_hdr, buf);
    }
  }

  if (strlen(host_hdr) == 0) {
    sprintf(host_hdr, "Host: %s:%d\r\n", hostname, port);
  }

  sprintf(http_header, "%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  printf("%s%s%s%s%s%s%s", http_header, host_hdr, "Connection: close\r\n", "Proxy-Connection: close\r\n", user_agent_hdr, other_hdr, "\r\n");
  return;
}

void parse_uri(char *uri, char *host, char* port, char *path) {
  // http://hostname:port/path 형태
  char *ptr = strstr(uri, "//");
  ptr = ptr != NULL ? ptr + 2 : uri;  // http:// 생략
  char *host_ptr = ptr;               // host 부분 찾기
  char *port_ptr = strchr(ptr, ':');  // port 부분 찾기
  char *path_ptr = strchr(ptr, '/');  // path 부분 찾기
  
  // 포트가 있는 경우
  if (port_ptr != NULL && (path_ptr == NULL || port_ptr < path_ptr)) {
    strncpy(host, host_ptr, port_ptr - host_ptr); // 버퍼, 복사할 문자열, 복사할 길이
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
  }
  // 포트가 없는 경우 
  else {
    strcpy(port, "80"); // 기본값
    strncpy(host, host_ptr, path_ptr - host_ptr);
  }
  strcpy(path, path_ptr);
  return;
}