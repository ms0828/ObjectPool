#include <iostream>
#include <process.h>
#include "ObjectPool.h"
#include <Windows.h>

using namespace std;

#define dfTestNum 4

HANDLE g_TestStartEvent;
HANDLE g_AllocObjectEndEvents[5];
HANDLE g_FreeObjectEndEvents[5];

HANDLE g_PoolCntCheckEvents[5];


int g_cnt = 0;
class TestNode
{
public:
	TestNode()
	{
		a = g_cnt++;
	}

	TestNode(int _a, USHORT _b)
	{
		a = _a;
		a = b;
	}

	~TestNode()
	{
		a = 0;
	}
public:
	USHORT a;
	alignas(32) ULONGLONG b;
};

struct ThreadArg
{
	HANDLE g_AllocObjectEndEvent;
	HANDLE g_FreeObjectEndEvent;
};



thread_local TestNode* gt_TestNodeArr[dfTestNum];

//CObjectPool<TestNode> g_ObjectPool(false, dfTestNum * 5);
CObjectPool<TestNode> g_ObjectPool(true, 0);

//-------------------------------------------------------------
// �׽�Ʈ 1��
// 1. ��� �����尡 �ڽ��� TestNum ������ŭ AllocObject�� ���� �� �ڽ��� AllocObject�� ��ŭ FreeObject ����
//    - ������ �� ������ �ݺ�
//--------------------------------------------------------------
unsigned int AllocAndFreeProc1(void* arg)
{
	srand(time(nullptr));

	WaitForSingleObject(g_TestStartEvent, INFINITE);

	while (1)
	{
		for (int i = 0; i < dfTestNum; ++i)
		{
			TestNode* allocNode = g_ObjectPool.allocObject();

			allocNode->a = 1;
			gt_TestNodeArr[i] = allocNode;
		}

		for (int i = 0; i < dfTestNum; ++i)
		{
			//---------------------------------------------
			// ABA ���� ����
			// - Alloc�� ������Ʈ�� �޸𸮸� �ٸ� �����尡 �Ҵ� �޾Ƽ� �ǵ帮���� Ȯ��
			//---------------------------------------------
			if (gt_TestNodeArr[i]->a != 1)
			{
				_LOG(dfLOG_LEVEL_ERROR, L"gt_TestNodeArr[i] using!");
				exit(1);
			}
			else
				_LOG(dfLOG_LEVEL_SYSTEM, L"OK!");

			g_ObjectPool.freeObject(gt_TestNodeArr[i]);
		}
	}

	return 0;
}


//-------------------------------------------------------------
// �׽�Ʈ 2��
// 1. ��� �����尡 �ڽ��� TestNum ������ŭ AllocObject�� ����
// 2. ��� �������� AllocObject�� ������ FreeObject ����
//--------------------------------------------------------------
unsigned int AllocAndFreeProc2(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	HANDLE allocObjectEndEvent = threadArg->g_AllocObjectEndEvent;
	HANDLE freeObjectEndEvent = threadArg->g_FreeObjectEndEvent;
	WaitForSingleObject(g_TestStartEvent, INFINITE);

	//--------------------------------------------------
	// ������ AllocObject ����
	//--------------------------------------------------
	for (int i = 0; i < dfTestNum; ++i)
	{
		TestNode* allocNode = g_ObjectPool.allocObject();
		gt_TestNodeArr[i] = allocNode;
	}
	_LOG(dfLOG_LEVEL_DEBUG, L"[Check] A Thread Complete AllocObject \n");
	SetEvent(allocObjectEndEvent);



	//---------------------------------------------------
	// ��� ������ AllocObject ������ ��ٸ���
	//---------------------------------------------------
	WaitForMultipleObjects(5, g_AllocObjectEndEvents, true, INFINITE);


	//--------------------------------------------------
	// ������ FreeObject ����
	//--------------------------------------------------
	for (int i = 0; i < dfTestNum; ++i)
	{
		g_ObjectPool.freeObject(gt_TestNodeArr[i]);
	}
	SetEvent(freeObjectEndEvent);
	_LOG(dfLOG_LEVEL_DEBUG, L"[Check] A Thread Complete FreeObject \n");

	//---------------------------------------------------
	// ��� ������ FreeObject ������ ��ٸ���
	//---------------------------------------------------
	WaitForMultipleObjects(5, g_FreeObjectEndEvents, true, INFINITE);


	//---------------------------------------------------
	// ��� �������� PoolCnt ���� 
	//---------------------------------------------------
	if (g_ObjectPool.GetPoolCnt() == 5 * dfTestNum)
	{
		_LOG(dfLOG_LEVEL_DEBUG, L"[Check] After All Thread Alloc / poolCnt(%lu) == expected pool Cnt(%lu) \n", g_ObjectPool.GetPoolCnt(), 5 * dfTestNum);
	}
	else
	{
		_LOG(dfLOG_LEVEL_ERROR, L"[Error] After All Thread Alloc / Miss Match PoolCnt! / poolCnt(%lu) != expected pool Cnt(%lu) \n", g_ObjectPool.GetPoolCnt(), 5 * dfTestNum);
		exit(1);
	}


	return 0;
}

void Test1()
{
	g_TestStartEvent = CreateEvent(nullptr, true, false, nullptr);
	HANDLE testTh1 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc1, nullptr, 0, nullptr);
	HANDLE testTh2 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc1, nullptr, 0, nullptr);
	HANDLE testTh3 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc1, nullptr, 0, nullptr);
	HANDLE testTh4 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc1, nullptr, 0, nullptr);
	HANDLE testTh5 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc1, nullptr, 0, nullptr);
	SetEvent(g_TestStartEvent);

	printf("Test Start! \n");

	while (1)
	{

	}
}
void Test2()
{
	g_TestStartEvent = CreateEvent(nullptr, true, false, nullptr);
	ThreadArg threadArg[5];
	for (int i = 0; i < 5; i++)
	{
		g_AllocObjectEndEvents[i] = CreateEvent(nullptr, true, false, nullptr);
		g_FreeObjectEndEvents[i] = CreateEvent(nullptr, true, false, nullptr);
		threadArg[i].g_AllocObjectEndEvent = g_AllocObjectEndEvents[i];
		threadArg[i].g_FreeObjectEndEvent = g_FreeObjectEndEvents[i];
	}

	//---------------------------------------------------
	// �׽�Ʈ 2��
	//---------------------------------------------------
	HANDLE testTh1 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[0], 0, nullptr);
	HANDLE testTh2 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[1], 0, nullptr);
	HANDLE testTh3 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[2], 0, nullptr);
	HANDLE testTh4 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[3], 0, nullptr);
	HANDLE testTh5 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[4], 0, nullptr);
	SetEvent(g_TestStartEvent);

	printf("Test Start! \n");


	while (1)
	{

	}
}

int main()
{
	InitLog(dfLOG_LEVEL_DEBUG, ELogMode::NOLOG);

	Test1();

	


	/*CObjectPool<TestNode> *pool = new CObjectPool<TestNode>(true, 500);
	TestNode* p = pool->allocObject();
	pool->freeObject(p);
	delete pool;*/
	
	/*
	TestNode* arr[5];

	for (int j = 0; j < 5; j++)
	{
		for (int i = 0; i < 5; i++)
		{
			TestNode* node = pool.allocObject();
			int poolCnt = pool.GetPoolCnt();
			printf("[alloc] poolCnt = %d / node adr = %016llx / node value = %d / top = %016llx\n", poolCnt, node, node->a, pool.top);

			arr[i] = node;
		}

		for (int i = 0; i < 5; i++)
		{
			TestNode* freeNode = arr[i];
			int value = freeNode->a;
			pool.freeObject(arr[i]);
			int poolCnt = pool.GetPoolCnt();
			printf("[free] poolCnt = %d / node adr = %016llx / node value = %d / top = %016llx\n", poolCnt, freeNode, value, pool.top);
		}
	}*/
	

	return 0;
}