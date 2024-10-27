#pragma once
#include <WinSock2.h>
#include <MSWSock.h>
#include <map>
#include <memory>
#include "CMyThreadPool.h"
#include "CMyQueue.h"

#pragma comment(lib, "Ws2_32.lib")

class CMyClient;
class CMyServer;
typedef std::shared_ptr<CMyClient> PCLIENT;
enum EnumMyOperator
{
    ENone,
    EAccept,
    ERecv,
    ESend,
    EError
};


class CMyOverlapped
{
public:
    OVERLAPPED m_overlapped;
    DWORD m_operator; //操作
    std::vector<char> m_buffer; //缓冲区
    ThreadWorker m_worker;  //处理函数
    CMyServer* m_server;    //服务器对象
};



class CMyServer :
    public ThreadFuncBase
{
public:
    CMyServer(const std::string& ip = "0.0.0.0", short port = 9527);
    bool StartServer();
    bool NewAccept();

    bool InitializeAcceptEx(SOCKET listenSocket) {
        GUID guidAcceptEx = WSAID_ACCEPTEX;
        DWORD bytes = 0;

        return WSAIoctl(
            listenSocket,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidAcceptEx,
            sizeof(guidAcceptEx),
            &lpfnAcceptEx,
            sizeof(lpfnAcceptEx),
            &bytes,
            NULL,
            NULL
        ) == 0;
    }

private:
    void CreateSocket();
    int Func() override;
private:
    CMyThreadPool m_pool;
    HANDLE m_hIOCP;
    SOCKET m_sock;
    std::map<SOCKET, std::shared_ptr<CMyClient>> m_client;
    sockaddr_in m_addr;
    CMyQueue<CMyClient> m_lstClient;
    std::string m_strIp;
    short m_sPort;

    LPFN_ACCEPTEX lpfnAcceptEx = NULL;
};



template<EnumMyOperator op>
class AcceptOverlapped : public CMyOverlapped, ThreadFuncBase
{ 
public:
    AcceptOverlapped()
    {
        //m_worker = ThreadWorker(static_cast<ThreadFuncBase*>(this), static_cast<FUNCTYPE>(&AcceptOverlapped::Func));
        m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::Func);
        m_operator = EAccept;
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);

        if (!InitializeGetAcceptExSockaddrs(*m_client)) 
        {
            std::cerr << "Failed to initialize GetAcceptExSockaddrs" << std::endl;
        }
    }

    //加载GetAcceptExSockaddrs 函数指针
    bool InitializeGetAcceptExSockaddrs(SOCKET listenSocket) {
        GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
        DWORD bytes = 0;
        return WSAIoctl(
            listenSocket,
            SIO_GET_EXTENSION_FUNCTION_POINTER,
            &guidGetAcceptExSockaddrs,
            sizeof(guidGetAcceptExSockaddrs),
            &lpfnGetAcceptExSockaddrs,
            sizeof(lpfnGetAcceptExSockaddrs),
            &bytes,
            NULL,
            NULL
        ) == 0;
    }

    int Func() override
    {
        DWORD lLength = 0, rLength = 0;
        if (m_client->m_received > 0)
        {
            lpfnGetAcceptExSockaddrs(
                *m_client,
                0,
                sizeof(sockaddr_in) + 16,
                sizeof(sockaddr_in) + 16,
                (sockaddr**)&m_client->m_lAddr, (LPINT)&lLength, //本地地址
                (sockaddr**)&m_client->m_rAddr, (LPINT)&rLength //远程地址
            );
        }
        if (!m_server->NewAccept())
        {
            return -2;
        }
        return -1;
    }

public:
    PCLIENT m_client;

private:
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;

};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EnumMyOperator op>
class SendOverlapped : public CMyOverlapped, ThreadFuncBase
{
public:
    SendOverlapped() : m_operator(ESend),
        m_worker(this, &SendOverlapped::SendWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int Func() override
    {

    }

};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EnumMyOperator op>
class RecvOverlapped : public CMyOverlapped, ThreadFuncBase
{
public:
    RecvOverlapped() : m_operator(ERecv),
        m_worker(this, &RecvOverlapped::RecvWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int Func() override
    {

    }

};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EnumMyOperator op>
class ErrorOverlapped : public CMyOverlapped, ThreadFuncBase
{
public:
    ErrorOverlapped() : m_operator(EError),
        m_worker(this, &ErrorOverlapped::ErrorWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int Func() override
    {

    }

};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;


class CMyClient
{
public:
    CMyClient();
    ~CMyClient();
    void SetOverlapped(PCLIENT& ptr);
    operator SOCKET()
    {
        return m_sock;
    }
    operator PVOID()
    {
        return &m_buffer[0];
    }
    operator LPOVERLAPPED()
    {
        return &m_overlapped.m_overlapped;
    }
    operator LPDWORD()
    {
        return &m_received;
    }
public:
    sockaddr_in m_addr;
    sockaddr_in m_lAddr;
    sockaddr_in m_rAddr;
    DWORD m_received;
private:
    SOCKET m_sock;
    std::vector<char> m_buffer;
    ACCEPTOVERLAPPED m_overlapped;
    bool m_bIsBusy;
};
