#include "BoostLog.h"

#include "ApplyOpenSSL.h"

#include <iostream>

// 서버측 SSL 컨텍스트 생성 및 초기화
SSL_CTX* createSSLContextForServer()
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());    // 서버 측 SSL 컨텍스트 생성
    if (!ctx) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_new";
        return NULL;
    }

    // ECDH 자동 설정
    if (SSL_CTX_set_ecdh_auto(ctx, 1) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_ecdh_auto";
    }

    // 최소한의 프로토콜 버전 설정
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_min_proto_version";
    }

    // 인증서 및 개인 키 로드
    if (SSL_CTX_use_certificate_file(ctx, "keyandcert/certificate.crt", SSL_FILETYPE_PEM) <= 0) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_use_certificate_file";
        SSL_CTX_free(ctx);
        return NULL;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, "keyandcert/private.key", SSL_FILETYPE_PEM) <= 0) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_use_PrivateKey_file";
        SSL_CTX_free(ctx);
        return NULL;
    }

    // 개인 키가 사용 가능한 것인지 확인
    if (!SSL_CTX_check_private_key(ctx)) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_check_private_key";
        return NULL;
    }

    //if (SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3) != 1)
    //{
    //    BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_options";
    //}

    // SSL 세션 캐시 비활성화
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    return ctx;
}

// 클라이언트측 SSL 컨텍스트 생성 및 초기화
SSL_CTX* createSSLContextForClient()
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());    // 클라이언트 측 SSL 컨텍스트 생성
    if (!ctx) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_new";
        return NULL;
    }

    // ECDH 자동 설정
    if (SSL_CTX_set_ecdh_auto(ctx, 1) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_ecdh_auto";
    }

    // 최소한의 프로토콜 버전 설정
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_min_proto_version";
    }

/*
    // CA 인증서 로드 (선택적)
    if (SSL_CTX_load_verify_locations(ctx, "ca.crt", NULL) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    // 클라이언트 인증서 및 개인 키 로드 (선택적)
    if (SSL_CTX_use_certificate_file(ctx, "client.crt", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
    if (SSL_CTX_use_PrivateKey_file(ctx, "client.key", SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }
*/
    //if (SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3) != 1)
    //{
    //    BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_options";
    //}

    // SSL 세션 캐시 비활성화
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    return ctx;
}

// SSL 소켓 생성
SSL* createSSLSocket(SSL_CTX* ctx, int socket)
{
    if (socket == -1) {
        BOOST_LOG_TRIVIAL(debug) << "Error: socket is -1";
        return NULL;
    }

    // SSL 소켓 생성
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_new";
        return NULL;
    }

    // SSL 소켓에 일반 소켓 연결
    if (SSL_set_fd(ssl, socket) != 1)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_set_fd";
        SSL_free(ssl);
        return NULL;
    }

    // SSL/TLS 핸드셰이크 수행
    int ret = SSL_accept(ssl);
    if (ret == 1)
    {
        BOOST_LOG_TRIVIAL(debug) << "Success: SSL_accept";
    }
    else
    {
        int error = SSL_get_error(ssl, ret);
        while (error == SSL_ERROR_WANT_READ)
        {
            ret = SSL_accept(ssl);
            if (ret == 1)
            {
                BOOST_LOG_TRIVIAL(debug) << "Success: Retry SSL_accept";
                break;
            }
            error = SSL_get_error(ssl, ret);
        }

        if (ret <= 0)
        {
            BOOST_LOG_TRIVIAL(debug) << "Error: SSL_accept ret is " << ret;
            handleConnectionError(ssl, ret);
            SSL_free(ssl);
            return NULL;
        }
    }

    return ssl;
}

// OpenSSL 초기화
void initializeSSL()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
}

// 오류 출력
void printOpenSSLErrors() 
{
    char errorBuffer[256];
    ERR_error_string_n(ERR_get_error(), errorBuffer, sizeof(errorBuffer));
    BOOST_LOG_TRIVIAL(debug) << "OpenSSL Error: " << errorBuffer;
}

// 소켓 연결 종료 및 에러 처리
void handleConnectionError(SSL* ssl, int ret)
{
    int error = SSL_get_error(ssl, ret);

    switch (error) {
    case SSL_ERROR_ZERO_RETURN:
        // 연결 종료
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_ZERO_RETURN";
        break;
    case SSL_ERROR_WANT_READ:
        // 읽기 대기 상태
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_WANT_READ";
        break;
    case SSL_ERROR_WANT_WRITE:
        // 쓰기 대기 상태
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_WANT_WRITE";
        break;
    case SSL_ERROR_SYSCALL:
        if (errno != 0) {
            // 시스템 호출 에러
            BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_SYSCALL (Error system call)";
        }
        else {
            // 소켓 연결 종료
            BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_SYSCALL (Terminate socket connection)";
        }
        break;
    case SSL_ERROR_SSL:
        // SSL 오류
        printOpenSSLErrors();
        break;
    default:
        // 기타 오류 처리
        BOOST_LOG_TRIVIAL(debug) << "Error: Etc.....";
        break;
    }
}
