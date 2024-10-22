#pragma once
#include "afx.h"
#include <atomic>
#include <vector>
#include <mutex>

class ThreadFuncBase
{

};
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

	bool IsValid()
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

	bool Start()
	{
		m_bStatus = true;
		m_hThread = (HANDLE)_beginthread(&CMyThread::threadEntry, 0, this);
		if (!IsValid())
		{
			m_bStatus = false;
		}
		return m_bStatus;
	}

	bool Stop()
	{

	}

	bool IsValid()  //����true��ʾ�߳���Ч,����false��ʾ�߳��쳣�����Ѿ���ֹ
	{
		if (m_hThread == NULL || m_hThread == INVALID_HANDLE_VALUE)	return false;
		return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
	}

	bool SetWorker(const ::ThreadWorker& worker)
	{

	}

	void UpdateWorker(const ThreadWorker& worker)
	{
		m_worker.store(worker);
	}

	bool IsIdle()
	{
		return m_worker.load().IsValid();
	}

protected:
	//����ֵС��0������ֹ�߳�ѭ��������0�򾯸���־������0��ʾ����
	virtual int each_step() = 0;

private:
	virtual void threadWorker()
	{
		while (m_bStatus)
		{
			ThreadWorker worker = m_worker.load();
			if (worker.IsValid())
			{
				int ret = worker();
				if (ret != 0)
				{
					//CString str;
					//str.Format(_T("thread found warning code %d\r\n"),ret);
					//OutputDebugString(str);
				}
				if (ret < 0)
				{
					m_worker.store(::ThreadWorker());
				}
			}
			else
			{
				Sleep(1);
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
	bool m_bStatus;  //false��ʾ�߳̽�Ҫ�رգ�true��ʾ�߳���������
	std::atomic<ThreadWorker> m_worker;
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
			if (!m_vctThread[i].Start())
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

	//����-1��ʾ����ʧ��,���� >0������Ϊ�̵߳�index
	int DispatchWorker(const ThreadWorker& worker)
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