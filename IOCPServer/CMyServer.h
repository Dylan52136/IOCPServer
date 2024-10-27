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
    PCLIENT m_client;
    WSABUF m_wsabuffer;
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
    AcceptOverlapped();

    //加载GetAcceptExSockaddrs 函数指针
    bool InitializeGetAcceptExSockaddrs(SOCKET listenSocket);

    int Func() override;

private:
    LPFN_GETACCEPTEXSOCKADDRS lpfnGetAcceptExSockaddrs = NULL;

};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EnumMyOperator op>
class SendOverlapped : public CMyOverlapped, ThreadFuncBase
{
public:
    SendOverlapped();

    int Func() override;

};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EnumMyOperator op>
class RecvOverlapped : public CMyOverlapped, ThreadFuncBase
{
public:
    RecvOverlapped();
    int Func() override;

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
        return &m_acceptOverlapped->m_overlapped;
    }
    operator LPDWORD()
    {
        return &m_received;
    }
    LPWSABUF RecvWsaBuffer();

    LPWSABUF SendWsaBuffer();
    sockaddr_in* GetLocalAddr();
    sockaddr_in* GetRemoteAddr();
    size_t GetBufferSize() const;
    DWORD& GetFlags();
    int Recv();
private:
    SOCKET m_sock;
    std::vector<char> m_buffer;
    std::shared_ptr<ACCEPTOVERLAPPED> m_acceptOverlapped;
    std::shared_ptr <SENDOVERLAPPED> m_sendOverlapped;
    std::shared_ptr <RECVOVERLAPPED> m_recvOverlapped;
    size_t m_used;
    bool m_bIsBusy;
    sockaddr_in m_addr;
    sockaddr_in m_lAddr;
    sockaddr_in m_rAddr;
    DWORD m_received;
    DWORD m_flags;
};
