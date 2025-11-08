#include <iostream>
#include <process.h>

#define PROFILE

#include "ObjectPool.h"
#include <Windows.h>

#include <conio.h> 


using namespace std;

#define dfTestNum 1000000

HANDLE g_TestStartEvent;
HANDLE g_AllocObjectEndEvents[5];
HANDLE g_FreeObjectEndEvents[5];

HANDLE g_PoolCntCheckEvents[5];


int g_cnt = 0;
class TestNode
{
public:

	TestNode(int _a, USHORT _b, ULONG len)
	{
		a = _a;
		b = _b;
		str = (char*)malloc(sizeof(char) * len);
	}

	~TestNode()
	{
		a = 0;
		free(str);
	}
public:
	ULONG a;
	alignas(32) ULONGLONG b;
	char* str;
};

struct ThreadArg
{
	HANDLE g_AllocObjectEndEvent;
	HANDLE g_FreeObjectEndEvent;
};



thread_local TestNode* gt_TestNodeArr[dfTestNum];

TestNode* g_allocNodeArr[dfTestNum];


//CObjectPool_TLS<TestNode> g_ObjectPool_TLS(true, 0, 1);
//CObjectPool_LF<TestNode> g_ObjectPool_LF(true, 0, 1);
//CObjectPool_Lock<TestNode> g_ObjectPool_Lock(true, 0, 1);
//CObjectPool_ST<TestNode> g_ObjectPool_ST(true, 0, 1);

//CObjectPool_TLS<TestNode> g_ObjectPool(true, dfTestNum / dfNumOfChunkObject, 1, 2, 3, 10);
CObjectPool_LF<TestNode> g_ObjectPool(true, dfTestNum, 1, 2, 3, 10);

//-------------------------------------------------------------
// 테스트 1번
// 1. 모든 스레드가 자신의 TestNum 개수만큼 AllocObject을 수행 후 자신이 AllocObject한 만큼 FreeObject 수행
//    - 끝나면 위 과정을 반복
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
			// ABA 문제 검출
			// - Alloc한 오브젝트의 메모리를 다른 스레드가 할당 받아서 건드리는지 확인
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
// 테스트 2번
// 1. 모든 스레드가 자신의 TestNum 개수만큼 AllocObject을 수행
// 2. 모든 스레드의 AllocObject가 끝나면 FreeObject 수행
//--------------------------------------------------------------
unsigned int AllocAndFreeProc2(void* arg)
{
	ThreadArg* threadArg = (ThreadArg*)arg;
	HANDLE allocObjectEndEvent = threadArg->g_AllocObjectEndEvent;
	HANDLE freeObjectEndEvent = threadArg->g_FreeObjectEndEvent;
	WaitForSingleObject(g_TestStartEvent, INFINITE);

	//--------------------------------------------------
	// 스레드 AllocObject 시작
	//--------------------------------------------------
	for (int i = 0; i < dfTestNum; ++i)
	{
		TestNode* allocNode = g_ObjectPool.allocObject();
		gt_TestNodeArr[i] = allocNode;
	}
	_LOG(dfLOG_LEVEL_DEBUG, L"[Check] A Thread Complete AllocObject \n");
	SetEvent(allocObjectEndEvent);



	//---------------------------------------------------
	// 모든 스레드 AllocObject 끝나기 기다리기
	//---------------------------------------------------
	WaitForMultipleObjects(5, g_AllocObjectEndEvents, true, INFINITE);


	//--------------------------------------------------
	// 스레드 FreeObject 시작
	//--------------------------------------------------
	for (int i = 0; i < dfTestNum; ++i)
	{
		g_ObjectPool.freeObject(gt_TestNodeArr[i]);
	}
	SetEvent(freeObjectEndEvent);
	_LOG(dfLOG_LEVEL_DEBUG, L"[Check] A Thread Complete FreeObject \n");

	//---------------------------------------------------
	// 모든 스레드 FreeObject 끝나기 기다리기
	//---------------------------------------------------
	WaitForMultipleObjects(5, g_FreeObjectEndEvents, true, INFINITE);


	//---------------------------------------------------
	// 모든 스레드의 PoolCnt 검증 
	//---------------------------------------------------
	/*if (g_ObjectPool.GetPoolCnt() == 5 * dfTestNum)
	{
		_LOG(dfLOG_LEVEL_DEBUG, L"[Check] After All Thread Alloc / poolCnt(%lu) == expected pool Cnt(%lu) \n", g_ObjectPool.GetPoolCnt(), 5 * dfTestNum);
	}
	else
	{
		_LOG(dfLOG_LEVEL_ERROR, L"[Error] After All Thread Alloc / Miss Match PoolCnt! / poolCnt(%lu) != expected pool Cnt(%lu) \n", g_ObjectPool.GetPoolCnt(), 5 * dfTestNum);
		exit(1);
	}*/


	return 0;
}


HANDLE allocEndEvents[3];
HANDLE freeEndEvents[3];
HANDLE testEndEvent;

ULONG allocIndex = 0;
ULONG freeIndex = 0;

unsigned int AllocAndFreeProc3(void* arg)
{
	WaitForSingleObject(g_TestStartEvent, INFINITE);

	int threadNum = (int)arg;

	if (threadNum <= 3)
	{
		while (1)
		{
			ULONG index = InterlockedIncrement(&allocIndex);
			if (index >= dfTestNum)
				break;
			g_allocNodeArr[index] = g_ObjectPool.allocObject();
		}	
		SetEvent(allocEndEvents[threadNum - 1]);
	}
	else
	{
		WaitForMultipleObjects(3, allocEndEvents, true, INFINITE);
		while (1)
		{
			ULONG index = InterlockedIncrement(&freeIndex);
			if (index >= dfTestNum)
				break;
			g_ObjectPool.freeObject(g_allocNodeArr[index]);
		}
		SetEvent(freeEndEvents[threadNum - 4]);
	}
	
	while (1)
	{
		int a = 0;
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

	printf("Test1 Start! \n");
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
	// 테스트 2번
	//---------------------------------------------------
	HANDLE testTh1 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[0], 0, nullptr);
	HANDLE testTh2 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[1], 0, nullptr);
	HANDLE testTh3 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[2], 0, nullptr);
	HANDLE testTh4 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[3], 0, nullptr);
	HANDLE testTh5 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc2, (void*)&threadArg[4], 0, nullptr);
	SetEvent(g_TestStartEvent);

	printf("Test2 Start! \n");

}

//--------------------------------------------
// TLS Test
//--------------------------------------------
void Test3()
{
	g_TestStartEvent = CreateEvent(nullptr, true, false, nullptr);
	for (int i = 0; i < 3; i++)
	{
		allocEndEvents[i] = CreateEvent(nullptr, true, false, nullptr);
		freeEndEvents[i] = CreateEvent(nullptr, true, false, nullptr);
	}
	testEndEvent = CreateEvent(nullptr, false, false, nullptr);

	HANDLE testTh1 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)1, 0, nullptr);
	HANDLE testTh2 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)2, 0, nullptr);
	HANDLE testTh3 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)3, 0, nullptr);
	HANDLE testTh4 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)4, 0, nullptr);
	HANDLE testTh5 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)5, 0, nullptr);
	HANDLE testTh6 = (HANDLE)_beginthreadex(nullptr, 0, AllocAndFreeProc3, (void*)6, 0, nullptr);


	Sleep(3000);
	SetEvent(g_TestStartEvent);

	printf("Test3 Start! \n");
	WaitForMultipleObjects(3, freeEndEvents, true, INFINITE);
	printf("Test3 End! \n");


	ProfileDataOutText("v5_LF.txt");
	printf("ProfileDataOutText\n");

	return;
}


int main()
{
	InitLog(dfLOG_LEVEL_SYSTEM, ELogMode::NOLOG);

	Test3();

	DWORD lastTest3Tick = GetTickCount64();
	DWORD sPressedTick = 0;
	BOOL waitingForS = FALSE;
	while (1)
	{
		// 1) r키를 누르면 프로파일링 리셋
		if (_kbhit())
		{
			int ch = _getch();

			if (ch == 's' || ch == 'S')
			{
				ProfileReset();
				printf("ProfileReset And Start Profile\n");
				waitingForS = TRUE;
				sPressedTick = GetTickCount64();
			}
			else if (ch == 27) // ESC 키로 종료
			{
				break;
			}
		}

		// 2) s키 누르고 10초 후 프로파일링 저장
		if (waitingForS)
		{
			DWORD now = GetTickCount64();
			if (now - sPressedTick >= 10000)
			{
				ProfileDataOutText("v6_LF.txt");
				printf("ProfileDataOutText\n");
				waitingForS = FALSE;
			}
		}

		//// 3) 1초마다 출력
		//DWORD now = GetTickCount64();
		//if (now - lastTest3Tick >= 1000)
		//{
		//	//printf("-----------------------------------------\n");
		//	/*printf("access Chunk Pool Cnt = %d\n", g_ObjectPool.accessChunkPoolCnt);
		//	printf("access Empty Pool Cnt = %d\n", g_ObjectPool.accessEmptyPoolCnt);*/
		//	//printf("-----------------------------------------\n");
		//	lastTest3Tick = now;
		//}
	}


	return 0;
}