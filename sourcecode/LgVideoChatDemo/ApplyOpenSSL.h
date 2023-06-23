#pragma once

#include <openssl/ssl.h>
#include <openssl/err.h>

SSL_CTX* createSSLContextForServer();				// 서버측 SSL 컨텍스트 생성 및 초기화
SSL_CTX* createSSLContextForClient();				// 클라이언트측 SSL 컨텍스트 생성 및 초기화
SSL* createSSLSocket(SSL_CTX* ctx, int socket);		// SSL 소켓 생성
void initializeSSL();								// SSL 초기화
void printOpenSSLErrors();							// 오류 출력
void handleConnectionError(SSL* ssl, int ret);		// 소켓 연결 종료 및 에러 처리

//-----------------------------------------------------------------
// END of File
//-----------------------------------------------------------------
