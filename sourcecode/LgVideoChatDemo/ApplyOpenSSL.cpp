#include "BoostLog.h"

#include "ApplyOpenSSL.h"

#include <iostream>

// ������ SSL ���ؽ�Ʈ ���� �� �ʱ�ȭ
SSL_CTX* createSSLContextForServer()
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());    // ���� �� SSL ���ؽ�Ʈ ����
    if (!ctx) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_new";
        return NULL;
    }

    // ECDH �ڵ� ����
    if (SSL_CTX_set_ecdh_auto(ctx, 1) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_ecdh_auto";
    }

    // �ּ����� �������� ���� ����
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_min_proto_version";
    }

    // ������ �� ���� Ű �ε�
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

    // ���� Ű�� ��� ������ ������ Ȯ��
    if (!SSL_CTX_check_private_key(ctx)) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_check_private_key";
        return NULL;
    }

    //if (SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3) != 1)
    //{
    //    BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_options";
    //}

    // SSL ���� ĳ�� ��Ȱ��ȭ
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    return ctx;
}

// Ŭ���̾�Ʈ�� SSL ���ؽ�Ʈ ���� �� �ʱ�ȭ
SSL_CTX* createSSLContextForClient()
{
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());    // Ŭ���̾�Ʈ �� SSL ���ؽ�Ʈ ����
    if (!ctx) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_new";
        return NULL;
    }

    // ECDH �ڵ� ����
    if (SSL_CTX_set_ecdh_auto(ctx, 1) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_ecdh_auto";
    }

    // �ּ����� �������� ���� ����
    if (SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION) == 0)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_CTX_set_min_proto_version";
    }

/*
    // CA ������ �ε� (������)
    if (SSL_CTX_load_verify_locations(ctx, "ca.crt", NULL) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_CTX_free(ctx);
        return NULL;
    }

    // Ŭ���̾�Ʈ ������ �� ���� Ű �ε� (������)
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

    // SSL ���� ĳ�� ��Ȱ��ȭ
    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_OFF);

    return ctx;
}

// SSL ���� ����
SSL* createSSLSocket(SSL_CTX* ctx, int socket)
{
    if (socket == -1) {
        BOOST_LOG_TRIVIAL(debug) << "Error: socket is -1";
        return NULL;
    }

    // SSL ���� ����
    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_new";
        return NULL;
    }

    // SSL ���Ͽ� �Ϲ� ���� ����
    if (SSL_set_fd(ssl, socket) != 1)
    {
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_set_fd";
        SSL_free(ssl);
        return NULL;
    }

    // SSL/TLS �ڵ����ũ ����
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

// OpenSSL �ʱ�ȭ
void initializeSSL()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
}

// ���� ���
void printOpenSSLErrors() 
{
    char errorBuffer[256];
    ERR_error_string_n(ERR_get_error(), errorBuffer, sizeof(errorBuffer));
    BOOST_LOG_TRIVIAL(debug) << "OpenSSL Error: " << errorBuffer;
}

// ���� ���� ���� �� ���� ó��
void handleConnectionError(SSL* ssl, int ret)
{
    int error = SSL_get_error(ssl, ret);

    switch (error) {
    case SSL_ERROR_ZERO_RETURN:
        // ���� ����
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_ZERO_RETURN";
        break;
    case SSL_ERROR_WANT_READ:
        // �б� ��� ����
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_WANT_READ";
        break;
    case SSL_ERROR_WANT_WRITE:
        // ���� ��� ����
        BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_WANT_WRITE";
        break;
    case SSL_ERROR_SYSCALL:
        if (errno != 0) {
            // �ý��� ȣ�� ����
            BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_SYSCALL (Error system call)";
        }
        else {
            // ���� ���� ����
            BOOST_LOG_TRIVIAL(debug) << "Error: SSL_ERROR_SYSCALL (Terminate socket connection)";
        }
        break;
    case SSL_ERROR_SSL:
        // SSL ����
        printOpenSSLErrors();
        break;
    default:
        // ��Ÿ ���� ó��
        BOOST_LOG_TRIVIAL(debug) << "Error: Etc.....";
        break;
    }
}
