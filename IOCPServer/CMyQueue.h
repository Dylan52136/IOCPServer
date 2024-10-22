#pragma once
#include <list>
#include <atomic>
#include <string>
#include <Windows.h>
#include <process.h>

enum
{
	EqNone,
	EQPush,
	EQPop,
	EQSize,
	EQClear
};

template <typename T>
class CMyQueue
{
	//线程安全的队列,利用IOCP实现
public:
	typedef struct IocpParam
	{
		int nOperator; //操作
		T data; //数据
		HANDLE hEvent; //pop操作需要

		IocpParam(int op, const T& _data, HANDLE hEv = nullptr)
		{
			nOperator = op;
			data = _data;
			hEvent = hEv;
		}
		IocpParam()
		{
			nOperator = EqNone;
		}
	}IOCP_PARAM;


	CMyQueue():
		m_lock(false)
	{
		m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,1);
		m_thread = INVALID_HANDLE_VALUE;
		if (m_hCompeletionPort != nullptr)
		{
			m_thread = (HANDLE)_beginthread(&CMyQueue<T>::threadEntry, 0, m_hCompeletionPort);
		}
	}

	~CMyQueue()
	{
		m_lock = true;
		HANDLE hTemp = m_hCompeletionPort;
		PostQueuedCompletionStatus(m_hCompeletionPort, 0, NULL, NULL);
		WaitForSingleObject(m_thread, INFINITE);
		m_hCompeletionPort = NULL;
		CloseHandle(hTemp);
	}

	bool PushBack(const T& data)
	{
		IOCP_PARAM* pParam = new IOCP_PARAM(EQPush, data);
		if (m_lock)	return false;
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(IOCP_PARAM), (ULONG_PTR)pParam, NULL);
		if (!ret)
		{
			delete pParam;
		}
		return ret;
	}
	bool PopFront(T& data)
	{
		HANDLE hEvent = CreateEvent(NULL,true,false,NULL);
		IOCP_PARAM* pParam = new IOCP_PARAM(EQPop, data, hEvent);
		if (m_lock)	return false;
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(IOCP_PARAM), (ULONG_PTR)pParam, NULL);
		if (!ret)
		{
			CloseHandle(hEvent);
			delete pParam;
			return ret;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret)
		{
			delete pParam->data;
			delete pParam;
		}
		return ret;
	}
	size_t Size()
	{
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		IOCP_PARAM param(EQSize, T(), hEvent);
		if (m_lock)
		{
			if(hEvent)	CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(IOCP_PARAM), (ULONG_PTR)&param, NULL);
		if (!ret)
		{
			if (hEvent)	CloseHandle(hEvent);
			return -1;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		if (ret)
		{
			return param.nOperator;
		}
		return -1;
	}

	void Clear()
	{
		if (m_lock)	return false;
		IOCP_PARAM param(EQSize, T());
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(IOCP_PARAM), (ULONG_PTR)&param, NULL);
		return ret;
	}

private:
	static void threadEntry(void* arg)
	{
		CMyQueue<T>* thiz = (CMyQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}
	void threadMain()
	{
		DWORD dwTransferred = 0;
		ULONG_PTR CompletionKey = 0;
		OVERLAPPED* pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE))
		{
			if ((dwTransferred == 0) && (CompletionKey == NULL))
			{
				printf("thread is prepare to exit!\r\n");
				break;
			}
			IOCP_PARAM* pParam = (IOCP_PARAM*)CompletionKey;
			switch (pParam->nOperator)
			{
			case EQPush:
				printf("IocpListPush\r\n");
				m_lstData.push_back(pParam->data);
				delete pParam;
				break;
			case EQPop:
				if (m_lstData.size() > 0)
				{
					pParam->data = m_lstData.front();
					m_lstData.pop_front();
				}
				if (pParam->hEvent != NULL)
				{
					SetEvent(pParam->hEvent);
				}
				break;
			case EQSize:
				pParam->nOperator = m_lstData.size();
				if (pParam->hEvent != NULL)
				{
					SetEvent(pParam->hEvent);
				}
				break;
			case EQClear:
				m_lstData.clear();
				break;	
			default:
				break;
			}
		}
	}

private:
	std::atomic<bool> m_lock;
	std::list<T> m_lstData;
	HANDLE m_hCompeletionPort;
	HANDLE m_thread; 

};

