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

CMyClient::CMyClient() : 
    m_bIsBusy(false),
    m_flags(0),
    m_acceptOverlapped(new ACCEPTOVERLAPPED),
    m_sendOverlapped(new SENDOVERLAPPED),
    m_recvOverlapped(new RECVOVERLAPPED)
{
    WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
    m_buffer.resize(1024);
    memset(&m_addr, 0, sizeof(sockaddr_in));
    memset(&m_lAddr, 0, sizeof(sockaddr_in));
    memset(&m_rAddr, 0, sizeof(sockaddr_in));
    m_acceptOverlapped->m_client.reset(this);
}

CMyClient::~CMyClient()
{
    closesocket(m_sock);
}

void CMyClient::SetOverlapped(PCLIENT& ptr)
{
    m_acceptOverlapped->m_client = ptr;
}

LPWSABUF CMyClient::RecvWsaBuffer() 
{
    return &m_recvOverlapped->m_wsabuffer;
}

LPWSABUF CMyClient::SendWsaBuffer()
{
    return &m_sendOverlapped->m_wsabuffer;
}

sockaddr_in* CMyClient::GetLocalAddr() 
{ 
    return &m_lAddr; 
}

sockaddr_in* CMyClient::GetRemoteAddr()
{
    return &m_rAddr;
}

size_t CMyClient::GetBufferSize() const
{
    return m_buffer.size();
}

DWORD& CMyClient::GetFlags()
{
    return m_flags;
}

int CMyClient::Recv()
{
    int ret = recv(m_sock, m_buffer.data() + m_used, m_buffer.size() - m_used, 0);
    if (ret <= 0)   return -1;
    m_used += (size_t)ret;
    //TOPO 解析数据
    return 0;
}


template<EnumMyOperator op>
inline AcceptOverlapped<op>::AcceptOverlapped()
{
    //m_worker = ThreadWorker(static_cast<ThreadFuncBase*>(this), static_cast<FUNCTYPE>(&AcceptOverlapped::Func));
    m_worker = ThreadWorker(this, (FUNCTYPE)&AcceptOverlapped<op>::Func);
    m_operator = op;
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024);

    if (!InitializeGetAcceptExSockaddrs(*m_client))
    {
        std::cerr << "Failed to initialize GetAcceptExSockaddrs" << std::endl;
    }
}

template<EnumMyOperator op>
bool AcceptOverlapped<op>::InitializeGetAcceptExSockaddrs(SOCKET listenSocket)

{
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

template<EnumMyOperator op>
int AcceptOverlapped<op>::Func()
{
    INT lLength = 0, rLength = 0;
    if (*(LPDWORD)*m_client > 0)
    {
        lpfnGetAcceptExSockaddrs(
            *m_client,
            0,
            sizeof(sockaddr_in) + 16,
            sizeof(sockaddr_in) + 16,
            (sockaddr**)m_client->GetLocalAddr(), &lLength, //本地地址
            (sockaddr**)m_client->GetRemoteAddr(), &rLength //远程地址
        );
    }

    int ret = WSARecv((SOCKET)*m_client, m_client->RecvWsaBuffer(), 1, *m_client, &m_client->GetFlags(), *m_client, NULL);
    if (ret == SOCKET_ERROR && (WSAGetLastError() != WSA_IO_PENDING))
    {
        //TODO 报错
    }
    if (!m_server->NewAccept())
    {
        return -2;
    }
    return -1;
}


template<EnumMyOperator op>
inline RecvOverlapped<op>::RecvOverlapped()
{
    m_operator = op;
    m_worker = ThreadWorker(this, (FUNCTYPE)&RecvOverlapped<op>::Func);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024);
}

template<EnumMyOperator op>
inline int RecvOverlapped<op>::Func()
{
    int ret = m_client->Recv();
    return ret;
}


template<EnumMyOperator op>
inline SendOverlapped<op>::SendOverlapped()
{
    m_operator = op; 
    m_worker = ThreadWorker(this, (FUNCTYPE)&SendOverlapped<op>::Func);
    memset(&m_overlapped, 0, sizeof(m_overlapped));
    m_buffer.resize(1024);
}

template<EnumMyOperator op>
inline int SendOverlapped<op>::Func()
{
    return 0;
}
