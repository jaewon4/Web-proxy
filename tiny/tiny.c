/* Tiny 웹 서버! */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

// main() : port 번호를 인자로 받아 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 함수를 호출한다.
int main(int argc, char **argv)
// argc는 인수(argument)의 개수를 나타내며, char **argv는 프로그램에 전달된 인수를 가리키는 포인터 배열입니다.
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr; // 클라이언트에서 연결 요청을 보내면 알 수 있는 클라이언트 연결 소켓 주소

  /* Check command line args */
  if (argc != 2) {
    // stderr : 표준 오류 출력 스트림을 나타내는 파일 포인터
    // argv[0] : 포트번호를 입력받는다.
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  /* 해당 포트 번호에 해당하는 듣기 소켓 식별자를 열어준다. */
  // getaddrinfo, socket, bind, listen까지의 과정을 진행한다.
  listenfd = Open_listenfd(argv[1]);

  /* 클라이언트의 요청이 올 때마다 새로 연결 소켓을 만들어 doit() 호출*/
  while (1) {
    /* 클라이언트에게서 받은 연결 요청을 accept한다. connfd = 서버 연결 식별자 */
    // clientaddr에는 클라이언트의 IP 주소와 포트 정보가 저장되어 있다.
    clientlen = sizeof(clientaddr);

    // accept 함수는 1. 듣기 식별자, 2. 소켓주소구조체의 주소, 3. 주소(소켓구조체)의 길이를 인자로 받는다.
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* 연결이 성공했다는 메세지를 위해. Getnameinfo를 호출하면서 hostname과 port가 채워진다. */
    // Getnameinfo() : 주어진 clientaddr 구조체에서 IP 주소와 포트 번호를 추출하여 
    // 해당 정보를 이용하여 호스트 이름과 서비스 이름을 검색하고, 
    // 검색 결과를 hostname과 port 변수에 저장하는 함수
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port); // Accepted connection from (143.248.220.176, 49541)
    
    // 인자로 서버 연결식별자를 넘겨준다.
	  doit(connfd);
    
    // 서버 연결 식별자를 닫아준다.
	  Close(connfd);                                            
  }
}
/* $end tinymain */

/*
 * doit - handle one HTTP request/response transaction
 */
/*
  doit(serverfd) : 클라이언트의 요청 라인을 확인해 정적, 동적 컨텐츠를 구분하고 각각의 서버에 보낸다.
*/
void doit(int fd) 
{
  int is_static;
  // stat() 함수 : 파일의 메타데이터, 즉 파일의 상태 정보를 구조체 변수에 저장합니다. 
  // 구체적으로는 파일의 종류, 권한, 생성일자, 크기 등의 정보를 가져올 수 있습니다. 
  // 이 함수는 unistd.h 라이브러리에 선언되어 있으며, 파일 이름을 인자로 받습니다.
  struct stat sbuf; // 파일의 메타데이터를 stat() 함수를 통해 가져와서 sbuf 변수에 저장
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE]; // 클라이언트에게서 받은 요청(rio)으로 채워진다.
  char filename[MAXLINE], cgiargs[MAXLINE]; // parse_uri를 통해 채워진다.
  rio_t rio; // 클라이언트가 보낸 요청을 읽고, 서버가 보낼 응답을 쓰는 등 데이터의 입출력 작업을 쉽게 처리할 수 있도록 돕는 것

  /* Read request line and headers */
  // 클라이언트가 rio로 보낸 request 라인과 헤더를 읽고 분석한다.
  Rio_readinitb(&rio, fd); // RIO 버퍼를 생성하고 초기화하는 역할
  if (!Rio_readlineb(&rio, buf, MAXLINE)) // RIO 버퍼에서 한 줄의 데이터를 읽어와서 buf에 저장하는 역할
      return;
  printf("%s", buf); // 요청 라인 buf = "GET /cat.gif HTTP/1.1\0"을 표준 출력만 해줌!
  sscanf(buf, "%s %s %s", method, uri, version); // buf에서 문자열 3개를 읽어와 method, uri, version이라는 문자열에 저장한다.

  // 요청 method가 GET 아니면 종료. main으로 가서 연결 닫고 다음 요청 기다림.      
  if (strcasecmp(method, "GET")) { // 문자열 비교 결과가 같으면 0을 반환하고, 다르면 0이 아닌 값을 반환합니다.
    clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
    return;
  }

  // 요청 라인을 뺀 나머지 요청 헤더들을 무시(그냥 프린트)한다.                                          
  read_requesthdrs(&rio);                              

  /* Parse URI from GET request */
  /* parse_uri : 클라이언트 요청 라인에서 받아온 uri를 이용해 정적/동적 컨텐츠를 구분한다. */
  is_static = parse_uri(uri, filename, cgiargs);
  // *** 출력 예제 ***
  // is_static : 1 / uri : /cat.jpg / filename :./cat.jpg / cgiargs :
  // is_static : 0 / uri : /cgi-bin/adder / filename :./cgi-bin/adder / cgiargs :100&200

  /* stat(filename, *sbuf) : filename을 sbuf에 넘긴다. */
  if (stat(filename, &sbuf) < 0) { // 못 넘기면 fail. 파일이 없다. 404.             
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }                                               

  // 컨텐츠의 유형(정적, 동적)을 파악한 후 각각의 서버에 보낸다. 
  if (is_static) { /* Serve static content */
    // -rwxrwxrwx 형태
    // !(일반 파일이다-txt, binary data file) or !(읽기 권한이 있다)   
	  if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { 
	    clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
	    return;
	  }
    // 정적 서버에 파일의 사이즈와 메서드를 같이 보낸다. -> Response header에 Content-length 위해!
	  serve_static(fd, filename, sbuf.st_size);        
  }
  else { /* Serve dynamic content */
    // !(일반 파일이다) or !(실행 권한이 있다)
	  if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { 
	    clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
	    return;
	  }
    // 동적 서버에 인자와 메서드를 같이 보낸다.
	  serve_dynamic(fd, filename, cgiargs);
  }
}
/* $end doit */

/*
 * read_requesthdrs - read HTTP request headers
 */
// 클라이언트가 버퍼 rp에 보낸 나머지 요청 헤더들을 무시한다(그냥 프린트한다).
void read_requesthdrs(rio_t *rp) 
{
  char buf[MAXLINE];

  // rp에서 최대 MAXLINE바이트만큼의 데이터를 읽어와서 버퍼에 저장하고, 
  // 그 중에서 개행 문자가 나오기 전까지의 데이터만을 가져온다.
  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);

  /* 버퍼 rp의 마지막 끝을 만날 때까지("Content-length: %d\r\n\r\n에서 마지막 \r\n) */
  /* 계속 출력해줘서 없앤다. */
  while(strcmp(buf, "\r\n")) { // buf 문자열이 "\r\n" 문자열과 같지 않은 동안에 계속 반복
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}
/* $end read_requesthdrs */

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
/* $begin parse_uri */
// uri를 받아 filename과 cgiarg를 채워준다.
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
  char *ptr;

  /* uri에 cgi-bin이 없다면, 즉 정적 컨텐츠를 요청한다면 1을 리턴한다.*/
  // 일반적으로 웹 서버는 정적인 콘텐츠(HTML, CSS, 이미지 파일 등)를 제공하는 역할을 합니다. 
  // 하지만 CGI를 이용하면 서버는 외부 프로그램을 호출하여 동적으로 생성된 콘텐츠를 반환할 수 있습니다. 
  // 예시 : GET /godzilla.jpg HTTP/1.1 -> uri에 cgi-bin이 없다
  // uri안에 "cgi-bin"과 일치하는 문자열이 있는지 확인
  if (!strstr(uri, "cgi-bin")) {  /* Static content */ 
    strcpy(cgiargs, ""); // 정적이니까 cgiargs는 아무것도 없다.                      
    strcpy(filename, "."); // 현재경로에서부터 시작 ./path ~~                    
    strcat(filename, uri); // filename 스트링에 uri 스트링을 이어붙인다.

    // 만약 uri뒤에 '/'이 있다면 그 뒤에 home.html을 붙인다.
    // 내가 브라우저에 http://localhost:8000만 입력하면 바로 뒤에 '/'이 생기는데,
    // '/' 뒤에 home.html을 붙여 해당 위치 해당 이름의 정적 컨텐츠가 출력된다.               
    if (uri[strlen(uri)-1] == '/')                   
        strcat(filename, "home.html");
    /* 예시
      uri : /godzilla.jpg
      ->
      cgiargs : 
      filename : ./godzilla.jpg
    */
    
    // 정적 컨텐츠면 1 리턴
    return 1;
  }
  else {  /* Dynamic content */                        
	  ptr = index(uri, '?');

    // '?'가 있으면 cgiargs를 '?' 뒤 인자들과 값으로 채워주고 ?를 NULL로 만든다.                       
    if (ptr) {
        strcpy(cgiargs, ptr+1);
        *ptr = '\0';
    }
    else  // '?'가 없으면 그냥 아무것도 안 넣어준다.
      strcpy(cgiargs, "");
                        
    strcpy(filename, "."); // 현재 디렉토리에서 시작
    strcat(filename, uri); // uri 넣어준다.

    /* 예시
      uri : /cgi-bin/adder?123&123
      ->
      cgiargs : 123&123
      filename : ./cgi-bin/adder
    */
    return 0;
  }
}
/* $end parse_uri */

/*
 * serve_static - copy a file back to the client 
 */
/* $begin serve_static */
/*
  serve_static : 
  클라이언트가 원하는 파일 디렉토리를 받아온다.
  응답 라인과 헤더를 작성하고 서버에게 보낸다.
*/
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  /* Send response headers to client 클라이언트에게 응답 헤더 보내기*/
  /* 응답 라인과 헤더 작성 */
  get_filetype(filename, filetype);    // 파일 타입 찾아오기 
  sprintf(buf, "HTTP/1.1 200 OK\r\n"); // 응답 라인 작성
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n"); // 응답 헤더 작성
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n", filesize);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: %s\r\n\r\n", filetype);
  Rio_writen(fd, buf, strlen(buf));

  // 더알아보기
  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0); // filename의 이름을 갖는 파일을 읽기 권한으로 불러온다.
  srcp = (char*)malloc(filesize);
  // srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0); // 파일 크기만큼의 메모리를 동적할당한다.
  Close(srcfd);
  Rio_writen(fd, srcp, filesize); // 해당 메모리에 있는 파일 내용들을 클라이언트에 보낸다(읽는다).
  free(srcp);
  // Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
/*
  get_filetype - Derive file type from filename
  filename을 조사해 각각의 식별자에 맞는 MIME 타입을 filetype에 입력해준다.
  -> Response header의 Content-type에 필요.
*/
void get_filetype(char *filename, char *filetype) 
{
  if (strstr(filename, ".html")) // filename 스트링에 ".html" 
	  strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
	  strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
	  strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
	  strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".mp4"))
	  strcpy(filetype, "video/mp4");
  else
	  strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
/* $begin serve_dynamic */
/*
  serve_dynamic() : 
  클라이언트가 원하는 동적 컨텐츠 디렉토리를 받아온다. 
  응답 라인과 헤더를 작성하고 서버에게 보낸다. 
  CGI 자식 프로세스를 fork하고 그 프로세스의 표준 출력을 클라이언트 출력과 연결한다.
*/
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.1 200 OK\r\n"); 
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));
  
   if (Fork() == 0) { /* Child */ 
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1); 

    // clientfd 출력을 CGI 프로그램의 표준 출력과 연결한다.
    // CGI 프로그램에서 printf하면 클라이언트에서 출력됨!!
    Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ 
    Execve(filename, emptylist, environ); /* Run CGI program */ 
  }
  Wait(NULL); /* Parent waits for and reaps child */ 
}
/* $end serve_dynamic */

/*
  clienterror : 에러메세지와 응답 본체를 서버 소켓을 통해 클라이언트에 보낸다.
*/
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE];

  /* Print the HTTP response headers */
  sprintf(buf, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /* Print the HTTP response body */
  sprintf(buf, "<html><title>Tiny Error</title>");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
  Rio_writen(fd, buf, strlen(buf));
}
/* $end clienterror */