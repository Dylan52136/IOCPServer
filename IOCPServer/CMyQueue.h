#pragma once
#include <list>
#include <atomic>
#include <string>
#include <Windows.h>
#include <process.h>
#include "CMyThreadPool.h"

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
	//�̰߳�ȫ�Ķ���,����IOCPʵ��
public:
	typedef struct IocpParam
	{
		int nOperator; //����
		T data; //����
		HANDLE hEvent; //pop������Ҫ

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
	}PPARAM;

public:
	CMyQueue():
		m_lock(false)
	{
		m_hCompeletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,1);
		m_thread = INVALID_HANDLE_VALUE;
		if (m_hCompeletionPort != NULL)
		{
			m_thread = (HANDLE)_beginthread(&CMyQueue<T>::threadEntry, 0, this);
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
		PPARAM* pParam = new PPARAM(EQPush, data);
		if (m_lock)	return false;
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (!ret)
		{
			delete pParam;
		}
		return ret;
	}

protected:
	virtual bool PopFront(T& data)
	{
		HANDLE hEvent = CreateEvent(NULL,true,false,NULL);
		PPARAM* pParam = new PPARAM(EQPop, data, hEvent);
		if (m_lock)	return false;
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
		if (!ret)
		{
			CloseHandle(hEvent);
			delete pParam;
			return ret;
		}
		ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
		data = pParam->data;
		if (ret)
		{
			delete pParam;
		}
		return ret;
	}
	size_t Size()
	{
		HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		PPARAM param(EQSize, T(), hEvent);
		if (m_lock)
		{
			if(hEvent)	CloseHandle(hEvent);
			return -1;
		}
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&param, NULL);
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
		PPARAM param(EQSize, T());
		bool ret = PostQueuedCompletionStatus(m_hCompeletionPort, sizeof(PPARAM), (ULONG_PTR)&param, NULL);
		return ret;
	}
protected:
	static void threadEntry(void* arg)
	{
		CMyQueue<T>* thiz = (CMyQueue<T>*)arg;
		thiz->threadMain();
		_endthread();
	}

	virtual void DealParam(PPARAM* pParam)
	{
		switch (pParam->nOperator)
		{
		case EQPush:
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

	void threadMain()
	{
		DWORD dwTransferred = 0;
		ULONG_PTR CompletionKey = 0;
		PPARAM* pParam = NULL;
		LPOVERLAPPED pOverlapped = NULL;
		while (GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, INFINITE))
		{
			if ((dwTransferred == 0) && (CompletionKey == NULL))
			{
				printf("thread is prepare to exit!\r\n");
				break;
			}
			pParam = (PPARAM*)CompletionKey;
			DealParam(pParam);
			Sleep(1);
		}
		GetQueuedCompletionStatus(m_hCompeletionPort, &dwTransferred, &CompletionKey, &pOverlapped, 0);
		CloseHandle(m_hCompeletionPort);
	}

protected:
	std::atomic<bool> m_lock;  //������������
	std::list<T> m_lstData;
	HANDLE m_hCompeletionPort;
	HANDLE m_thread; 

};

template <class T>
class CMySendQueue : public CMyQueue<T>,public ThreadFuncBase
{
public:
	typedef int(ThreadFuncBase::* MYCALLBACK)(T& data);
	CMySendQueue(ThreadFuncBase* obj, MYCALLBACK callback):
		CMyQueue<T>(),
		m_base(obj),
		m_callback(callback)
	{
		m_thread.Start(9999);
		m_thread.UpdateWorker(ThreadWorker((ThreadFuncBase*)this, (FUNCTYPE)&CMySendQueue<T>::threadTick));
	}

protected:
	virtual bool PopFront(T& data) { return false; };

	bool PopFront()
	{
		typename CMyQueue<T>::PPARAM* pParam = new typename CMyQueue<T>::PPARAM(EQPop, T());
		if (this->m_lock)
		{
			delete pParam;
			return false;
		}
		bool ret = PostQueuedCompletionStatus(this->m_hCompeletionPort, sizeof(typename CMyQueue<T>::PPARAM), (ULONG_PTR)pParam, NULL);
		if (!ret)
		{
			delete pParam;
			return ret;
		}
		return ret;
	}

	int threadTick()
	{
		if (this->m_lstData.size() > 0)
		{
			PopFront();
		}
		return 0;
	}




	void DealParam(typename CMyQueue<T>::PPARAM* pParam) override
	{
		switch (pParam->nOperator)
		{
		case EQPush:
			this->m_lstData.push_back(pParam->data);
			delete pParam;
			break;
		case EQPop:
			if (this->m_lstData.size() > 0)
			{
				pParam->data = this->m_lstData.front();
				if(0 == (m_base->*m_callback)(pParam->data))
					this->m_lstData.pop_front();
			}
			delete pParam;
			break;
		case EQSize:
			pParam->nOperator = this->m_lstData.size();
			if (pParam->hEvent != NULL)
			{
				SetEvent(pParam->hEvent);
			}
			break;
		case EQClear:
			this->m_lstData.clear();
			break;
		default:
			break;
		}
	}

private:
	ThreadFuncBase* m_base;
	MYCALLBACK m_callback;
	CMyThread m_thread;
};

typedef CMySendQueue<std::vector<char>>::MYCALLBACK SENDCALLBACK;