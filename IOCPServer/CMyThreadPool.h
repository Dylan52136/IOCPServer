#pragma once
//#include "afx.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <iostream>

class ThreadFuncBase
{
public:
	virtual int Func() = 0;
};

//Test
//class CMyFunc : public ThreadFuncBase
//{
//public:
//	CMyFunc(int iNumber)
//	{
//		m_iNumber = iNumber;
//	}
//	int Func() override
//	{
//		return m_iNumber;
//	}
//private:
//	int m_iNumber;
//};

typedef int(ThreadFuncBase::* FUNCTYPE)();

class ThreadWorker
{
public:
	ThreadWorker()
	{
		thiz = NULL;
		func = NULL;
	}
	
	ThreadWorker(ThreadFuncBase* obj, FUNCTYPE f) : thiz(obj), func(f)
	{

	}
	
	ThreadWorker(const ThreadWorker& worker)
	{
		thiz = worker.thiz;
		func = worker.func;
	}
	
	ThreadWorker& operator=(const ThreadWorker& worker)
	{
		if (this != (&worker))
		{
			thiz = worker.thiz;
			func = worker.func;
		}
		return *this;
	}

	bool IsValid() const
	{
		return (thiz != NULL) && (func != NULL);
	}

	int operator()()
	{
		if (IsValid())
		{
			return (thiz->*func)();
		}
		return -1;
	}

private:
	ThreadFuncBase* thiz;
	FUNCTYPE func;
};


class CMyThread
{
public:
	CMyThread()
	{
		m_hThread = NULL;
	}

	~CMyThread()
	{
		Stop();
	}

	CMyThread(const CMyThread& thread)
	{
		m_hThread = thread.m_hThread;
		m_bStatus = thread.m_bStatus;
		//m_worker = thread.m_worker;
	}

	bool Start(int iThreadIndex)
	{
		std::cout << "Start Thread :" << iThreadIndex << std::endl;
		m_bStatus = true;
		m_iThreadIndex = iThreadIndex;
		m_hThread = (HANDLE)_beginthread(&CMyThread::threadEntry, 0, this);
		if (!IsValid())
		{
			m_bStatus = false;
		}
		return m_bStatus;
	}

	bool Stop()
	{
		if (m_bStatus)
		{
			m_bStatus = false;
		}
		bool ret = WaitForSingleObject(m_hThread, INFINITE) == WAIT_OBJECT_0;
		if (m_worker.load() != NULL)
		{
			ThreadWorker* pWorker = m_worker.load();
			m_worker.store(NULL);
			delete pWorker;
		}
		return ret;
	}

	bool IsValid() //返回true表示线程有效,返回false表示线程异常或者已经终止
	{
		if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE)	return false;
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
	}

	void UpdateWorker(const ThreadWorker& worker)
	{
		if (!worker.IsValid())
		{
			m_worker.store(NULL);
			return;
		}
		if (m_worker.load() != NULL)
		{
			ThreadWorker* pWorker = m_worker.load();
			m_worker.store(NULL);
			delete pWorker;
		}
		m_worker.store(new ThreadWorker(worker));
	}

	bool IsIdle()
	{
		if (NULL == m_worker)
		{
			return true;
		}
		return !m_worker.load()->IsValid();
	}

	virtual void threadWorker()
	{
		while (m_bStatus)
		{
			if (m_worker)
			{
				ThreadWorker* worker = m_worker.load();
				if (worker->IsValid())
				{
					int ret = (*worker)();
					std::cout << "ThreadIndex:" << m_iThreadIndex << "\tResult:" << ret << std::endl;
					if (ret != 0)
					{
						
					}
					if (ret < 0)
					{
						m_worker.store(NULL);
					}
					Sleep(2000);
				}
			}
		}
	}

	static void threadEntry(void* arg)
	{
		CMyThread* thiz = (CMyThread*)arg;
		if (thiz)
		{
			thiz->threadWorker();
		}
		_endthread();
	}

private:
	HANDLE m_hThread;
	bool m_bStatus;  //false表示线程将要关闭，true表示线程正在运行
	std::atomic<ThreadWorker*> m_worker;
	int m_iThreadIndex;
};


class CMyThreadPool
{
public:
	CMyThreadPool()
	{

	}
	CMyThreadPool(size_t size)
	{
		m_vctThread.resize(size);
	}
	~CMyThreadPool()
	{
		Stop();
		m_vctThread.clear();
	}
	bool Invoke()
	{
		bool ret = true;
		for (size_t i = 0; i < m_vctThread.size(); ++i)
		{
			if (!m_vctThread[i].Start(i))
			{
				ret = false;
				break;
			}
		}
		if (!ret)
		{
			for (size_t i = 0; i < m_vctThread.size(); ++i)
			{
				m_vctThread[i].Stop();
			}
		}
		return ret;
	}

	void Stop()
	{
		for (size_t i = 0; i < m_vctThread.size(); ++i)
		{
			m_vctThread[i].Stop();
		}
	}

	//返回-1表示分配失败,返回 >0的整数为线程的index
	int DispatchWorker(ThreadWorker& worker)
	{
		int index = -1;
		m_lock.lock();
		for (size_t i = 0; i < m_vctThread.size(); i++)
		{
			if (m_vctThread[i].IsIdle())
			{
				m_vctThread[i].UpdateWorker(worker);
				index = i;
				break;
			}
		}
		m_lock.unlock();
		return index;
	}

	bool CheckThreadValid(size_t index)
	{
		if (index < m_vctThread.size())
		{
			return m_vctThread[index].IsValid();
		}
		return false;
	}

private:
	std::mutex m_lock;
	std::vector<CMyThread> m_vctThread;
};