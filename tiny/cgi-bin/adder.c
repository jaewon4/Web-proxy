#include "csapp.h"

// 현재 파일은 폴더가 cgi-bin안에 adder.c라는 프로그램을 실행하므로 
// /cgi-bin/adder?100&200 이렇게 인자를 2개 넘겨줄수 있다.
// adder같은 프로그램은 종종 cgi프로그램이라고 부르는데 그 이유는 이들이 cgi표준의 규칙을 준수하기 떄문이다.
// 해당 코드는 클라이언트로부터 전달받은 쿼리 문자열을 처리하고, 
// 덧셈 연산을 수행한 결과를 클라이언트에 반환하는 동적인 웹 페이지를 생성하는 역할을 합니다.
int main(){
    char *buf, *p;
    char arg1[MAXLINE], arg2[MAXLINE], content[MAXLINE];
    int n1=0, n2=0;

    /* 서버 프로세스가 만들어준 QUERY_STRING 환경변수를 getenv로 가져와 buf에 넣음 */
    // http://example.com/cgi-bin/adder?100&200
    // 위 URL에서 QUERY_STRING 환경 변수에 저장되는 값은 "100&200" 입니다. 
    // CGI 스크립트는 이 정보를 파싱하여 원하는 동적인 콘텐츠를 생성하는 데 사용할 수 있습니다.
    if((buf = getenv("QUERY_STRING")) != NULL){
        p = strchr(buf, '&'); // 포인터 p는 &가 있는 곳의 주소
        *p = '\0';      // &를 \0로 바꿔주고
        strcpy(arg1, buf);  // & 앞에 있었던 인자
        strcpy(arg2, p+1);  // & 뒤에 있었던 인자
        n1 = atoi(arg1); // 문자열을 정수형으로 변환하는 데 사용되는 함수
        n2 = atoi(arg2);
    }

    /* content라는 string에 응답 본체를 담는다. */
    sprintf(content, "QUERY_STRING=%s\r\n<p>", buf);  // ""안의 string이 content라는 string에 쓰인다.
    sprintf(content, "%sWelcome to add.com: ", content);
    sprintf(content, "%sTHE Internet addition portal. \r\n<p>", content);
    sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", content, n1, n2, n1+n2);  // 인자를 받아 처리를 해줬다! -> 동적 컨텐츠
    sprintf(content, "%sThanks for visiting!\r\n", content);

    /*
        출력 결과
        QUERY_STRING=100
        Welcome to add.com: THE Internet addition portal.
        The answer is: 100 + 200 = 300
        Thanks for visiting!
    */

    // 네트워크탭에서 응답헤더에서 볼수있음(새로고침)
    /* CGI 프로세스가 부모인 서버 프로세스 대신 채워 주어야 하는 응답 헤더값*/
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);  // 응답 본체 출력!
    fflush(stdout);

    exit(0);
}