#include <iostream>
#include "CMyQueue.h"
#include "CMyThreadPool.h"


int main(int argc, char* argv[])
{
	CMyThreadPool threadPool(20);
	threadPool.Invoke();
	for (size_t i = 0; i < 1000; i++)
	{
		ThreadFuncBase* pBase = new CMyFunc(i % 100);
		ThreadWorker worker(pBase, &ThreadFuncBase::Func);
		threadPool.DispatchWorker(worker);
		//Sleep(1);
	}
	
	

	/*CMyQueue<int> que;
	for (size_t i = 0; i < 20; i++)
	{
		que.PushBack(i);
	}
	while (que.Size() != 0)
	{
		int data;
		que.PopFront(data);
		std::cout << data << std::endl;
	}*/
	system("pause");
}