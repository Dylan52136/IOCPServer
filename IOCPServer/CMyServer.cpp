#include "CMyServer.h"

CMyServer::CMyServer(const std::string& ip, short port) :
    m_pool(10)
    ,m_strIp(ip)
    ,m_sPort(port)
{
    m_hIOCP = INVALID_HANDLE_VALUE;
    m_sock = INVALID_SOCKET;
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = inet_addr(ip.c_str());
}

bool CMyServer::StartServer()
{
    CreateSocket();
    if (!InitializeAcceptEx(m_sock)) {
        std::cerr << "Failed to initialize AcceptEx" << std::endl;
        return false;
    }

    if (bind(m_sock, (sockaddr*)&m_addr, sizeof(sockaddr)) == -1)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    if (listen(m_sock, 3) == -1)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }
    m_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
    if (m_hIOCP == NULL)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        m_hIOCP = INVALID_HANDLE_VALUE;
        return false;
    }
    CreateIoCompletionPort((HANDLE)m_sock, m_hIOCP, (ULONG_PTR)this, 0);
    m_pool.Invoke();
    ThreadWorker iocpWorker(ThreadWorker(this, (FUNCTYPE)&CMyServer::Func));
    m_pool.DispatchWorker(iocpWorker);
    if (!NewAccept())
    {
        return false;
    }
    return true;
}

bool CMyServer::NewAccept()
{
    PCLIENT pClient(new CMyClient());
    pClient->SetOverlapped(pClient);
    m_client.insert(std::pair<SOCKET, PCLIENT>(*pClient, pClient));
    if (!lpfnAcceptEx(m_sock, *pClient, *pClient, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, *pClient, *pClient))
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        m_hIOCP = INVALID_HANDLE_VALUE;
        return false;
    }
    return true;
}

void CMyServer::CreateSocket()
{
    m_sock = WSASocket(PF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    int opt = 1;
    setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)); //允许在本地地址被绑定的情况下重新绑定套接字
}

int CMyServer::Func()
{
    DWORD transferred = 0;
    ULONG_PTR completionKey = 0;
    OVERLAPPED* lpOverlapped = NULL;
    if (GetQueuedCompletionStatus(m_hIOCP, &transferred, &completionKey, &lpOverlapped, (DWORD)INFINITY))
    {
        if (transferred > 0 && (completionKey != 0))
        {
            CMyOverlapped* pOverlapped = CONTAINING_RECORD(lpOverlapped, CMyOverlapped, m_overlapped);
            CMyOverlapped* pOver = NULL;
            switch (pOverlapped->m_operator)
            {
            case EAccept:
                pOver = (ACCEPTOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
                break;
            case ESend:
                pOver = (SENDOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
                break;
            case ERecv:
                pOver = (RECVOVERLAPPED*)pOverlapped;
                m_pool.DispatchWorker(pOver->m_worker);
                break;
            case EError:
                pOver = (ERROROVERLAPPED*)pOverlapped;
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

CMyClient::CMyClient() : m_bIsBusy(false)
{
    WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_buffer.resize(1024);
    memset(&m_addr, 0, sizeof(sockaddr_in));
    memset(&m_lAddr, 0, sizeof(sockaddr_in));
    memset(&m_rAddr, 0, sizeof(sockaddr_in));
    m_overlapped.m_client.reset(this);
}

CMyClient::~CMyClient()
{
    closesocket(m_sock);
}

void CMyClient::SetOverlapped(PCLIENT& ptr)
{
    m_overlapped.m_client = ptr;
}
