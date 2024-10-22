#pragma once
#include <MSWSock.h>
#include <map>
#include "CMyThreadPool.h"
//#include "CMyQueue.h"

//#pragma comment(lib, "Ws2_32.lib")

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
};

template<EnumMyOperator>
class AcceptOverlapped : public CMyOverlapped, ThreadFuncBase
{ 
    AcceptOverlapped() : 
        m_worker(this, &AcceptOverlapped::AcceptWorker)
    {
        m_operator = EAccept;
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int AcceptWorker()
    {
        DWORD lLength = 0, rLength = 0;
        if (m_client->m_received > 0)
        {
            GetAcceptExSockaddrs(m_client, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16),
                (sockaddr**) &m_client->m_lAddr,&lLength, //本地地址
                (sockaddr**)&m_client->m_rAddr,&rLength; //远程地址
        }
        if (!m_server->NewAccept())
        {
            return -2;
        }
        return -1;
    }
    PCLIENT m_client;
};
typedef AcceptOverlapped<EAccept> ACCEPTOVERLAPPED;

template<EnumMyOperator>
class SendOverlapped : public CMyOverlapped, ThreadFuncBase
{
    SendOverlapped() : m_operator(ESend),
        m_worker(this, &SendOverlapped::SendWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int SendWorker()
    {

    }

};
typedef SendOverlapped<ESend> SENDOVERLAPPED;

template<EnumMyOperator>
class RecvOverlapped : public CMyOverlapped, ThreadFuncBase
{
    RecvOverlapped() : m_operator(ERecv),
        m_worker(this, &RecvOverlapped::RecvWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int RecvWorker()
    {

    }

};
typedef RecvOverlapped<ERecv> RECVOVERLAPPED;

template<EnumMyOperator>
class ErrorOverlapped : public CMyOverlapped, ThreadFuncBase
{
    ErrorOverlapped() : m_operator(EError),
        m_worker(this, &ErrorOverlapped::ErrorWorker)
    {
        memset(&m_overlapped, 0, sizeof(m_overlapped));
        m_buffer.resize(1024);
    }

    int ErrorWorker()
    {

    }

};
typedef ErrorOverlapped<EError> ERROROVERLAPPED;


class CMyClient
{
public:
    CMyClient() : m_bIsBusy(false)
    {
        WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        m_buffer.resize(1024);
        memset(&m_addr, 0, sizeof(sockaddr_in));
        memset(&m_lAddr, 0, sizeof(sockaddr_in));
        memset(&m_rAddr, 0, sizeof(sockaddr_in));
        m_overlapped.m_client.reset(this);
    }
    ~CMyClient()
    {
        closesocket(m_sock);
    }

    void SetOverlapped(PCLIENT &ptr)
    {
        m_overlapped.m_client = ptr;
    }

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


private:
    SOCKET m_sock;
    std::vector<char> m_buffer;
    DWORD m_received;
    ACCEPTOVERLAPPED m_overlapped;
    sockaddr_in m_addr;
    sockaddr_in m_lAddr;
    sockaddr_in m_rAddr;
    bool m_bIsBusy;
};

typedef std::shared_ptr<CMyClient> PCLIENT;


class CMyServer :
    public ThreadFuncBase
{
public:
    CMyServer(const std::string& ip = "0.0.0.0", short port = 9527):
        m_pool(10)
    {
        m_hIOCP = INVALID_HANDLE_VALUE;
        m_sock = INVALID_SOCKET;
        m_addr.sin_family = AF_INET;
        m_addr.sin_port = htons(port);
        m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
    }

    bool StartServer()
    {
        CreateSocket();

        if (bind(m_sock, (sockaddr*)&m_addr, sizeof(sockaddr)) == -1)
        {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return;
        }
        if (listen(m_sock, 3) == -1)
        {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            return;
        }
        m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
        if (m_hIOCP == NULL)
        {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return;
        }
        CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
        m_pool.Invoke();
        m_pool.DispatchWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::threadIOCP));
        if (!NewAccept())
        {
            return false;
        }
    }

    bool NewAccept()
    {
        PCLIENT pClient(new CMyClient());
        pClient->SetOverlapped(pClient);
        m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
        if (!AcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient))
        {
            closesocket(m_sock);
            m_sock = INVALID_SOCKET;
            m_hIOCP = INVALID_HANDLE_VALUE;
            return;
        }
    }

private:
    void CreateSocket()
    {
        m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
        int opt = 1;
        setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)); //允许在本地地址被绑定的情况下重新绑定套接字
    }


    int threadIOCP()
    {
        DWORD transferred = 0;
        ULONG_PTR completionKey = 0;
        OVERLAPPED* lpOverlapped = NULL;
        if (GetQueuedCompletionStatus(m_hIOCP,&transferred,&completionKey,&lpOverlapped,INFINITY))
        {
            if (transferred > 0 && (completionKey != 0))
            {
                CMyOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, CMyOverlapped, m_overlapped);
                switch (pOverlapped->m_operator)
                {
                case EAccept:
                    ACCEPTOVERLAPPED* pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                    m_pool.DispatchWorker(pOver->m_worker);
                    break;
                case ESend:
                    SENDOVERLAPPED* pOver = (SENDOVERLAPPED*)pOverlapped;
                    m_pool.DispatchWorker(pOver->m_worker);
                    break;
                case ERecv:
                    RECVOVERLAPPED* pOver = (RECVOVERLAPPED*)pOverlapped;
                    m_pool.DispatchWorker(pOver->m_worker);
                    break;
                case EError:
                    ERROROVERLAPPED* pOver = (ERROROVERLAPPED*)pOverlapped;
                    m_pool.DispatchWorker(pOver->m_worker);
                    break;
                default:
                    break;
                }
            }
            else
            {
                return -1;
            }
        }
        return 0;
    }

private:
    CMyThreadPool m_pool;
    HANDLE m_hIOCP;
    SOCKET m_sock;
    std::map<SOCKET, std::shared_ptr<CMyClient>> m_client;
    sockaddr_in m_addr;
    CMyQueue<CMyClient> m_lstClient;
};

