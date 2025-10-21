#pragma once
#include "Log.h"
#include <new>
#include <utility>

#define dfFenceValue 0xdddddddddddddddd

#define dfDebugObjectPool


template<typename T>
class CObjectPool
{
private:
	struct Node
	{
		ULONGLONG headFence;
		T instance;
		ULONGLONG tailFence;
		USHORT seed;
		Node* next;
	};

public:

	template<typename... Args>
	CObjectPool(bool preConstructor, int poolNum = 0, Args&&... args)
	{
		poolSeed = rand();
		bPreConstructor = preConstructor;
		top = nullptr;
		allocCnt = 0;
		poolCnt = 0;
		InitializeSRWLock(&poolLock);

		for (int i = 0; i < poolNum; i++)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));
			newNode->headFence = dfFenceValue;
			newNode->tailFence = dfFenceValue;
			newNode->seed = poolSeed;
			newNode->next = top;
			top = newNode;
			
			//--------------------------------------------------------
			// bPreConstructor�� true�� ���, ó�� ���� �� ������ ȣ��
			//--------------------------------------------------------
			if (bPreConstructor)
			{
				T* instance = &newNode->instance;
				new (instance) T(std::forward<Args>(args)...);
			}

			poolCnt++;
		}
	}

	~CObjectPool()
	{
		while (top != nullptr)
		{
			Node* deleteNode = top;
			top = top->next;
			
			//------------------------------------------------------------------------------
			// bPreConstructor�� true�� ���, ObjectPool�� �Ҹ�� �� instance�� �Ҹ��� ȣ��
			//------------------------------------------------------------------------------
			if (bPreConstructor)
				delete deleteNode;
			else
				free(deleteNode);
		}
	}
	

	//---------------------------------------------------------------
	// �Ҵ� ��å
	// - bPreConstructor�� true�� ���, allocObject���� ������ ȣ�� X
	// - bPreConstructor�� false�� ���, allocObject���� ������ ȣ�� O
	// 
	// Ǯ�� ������� ���� instance�� ���� �����Ͽ� �Ҵ�
	// - ������ ȣ�� O
	//---------------------------------------------------------------
	template<typename... Args>
	T* allocObject(Args&&... args)
	{
		AcquireSRWLockExclusive(&poolLock);
		if (top == nullptr)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));
			newNode->headFence = dfFenceValue;
			newNode->tailFence = dfFenceValue;
			newNode->seed = poolSeed;
			T* instance = &newNode->instance;
			new (instance) T(std::forward<Args>(args)...);
			allocCnt++;
			_LOG(dfLOG_LEVEL_DEBUG, L"[AllocObject] : allocCnt = %d / poolCnt = %d\n", allocCnt, poolCnt);
			ReleaseSRWLockExclusive(&poolLock);
			return instance;
		}
		else
		{	
			Node* allocNode = top;
			top = allocNode->next;
			T* instance = &allocNode->instance;
			if (!bPreConstructor)
				new (instance) T(std::forward<Args>(args)...);
			poolCnt--;
			allocCnt++;
			_LOG(dfLOG_LEVEL_DEBUG, L"[AllocObject] : allocCnt = %d / poolCnt = %d\n", allocCnt, poolCnt);
			ReleaseSRWLockExclusive(&poolLock);
			return instance;
		}
	}

	//---------------------------------------------------------------
	// �ݳ� ��å
	// - bPreConstructor�� true�� ���, freeObject���� �Ҹ��� ȣ�� X
	// - bPreConstructor�� false�� ���, freeObject���� �Ҹ��� ȣ�� O
	// 
	// 
	// ���� ��ġ [ dfDebugObjectPool�� ���ǵǾ� ���� ��� �ݳ� �� ��� ���� ���� ]
	// - Node�� seed�� Pool�� seed�� �������� Ȯ��
	// - Node���� instance �� �ڷ� fence�� �ξ� ���� �����Ǿ��ִ��� Ȯ��
	//---------------------------------------------------------------
	bool freeObject(T* objectPtr)
	{
		Node* freeNode;
		int t1 = alignof(T);
		int t2 = alignof(ULONGLONG);
		if (alignof(T) > alignof(ULONGLONG))
		{
			int remainAlign = alignof(T) - alignof(ULONGLONG);
			freeNode = (Node*)((char*)objectPtr - remainAlign - sizeof(ULONGLONG));
		}
		else
			freeNode = (Node*)((char*)objectPtr - sizeof(ULONGLONG));

#ifdef dfDebugObjectPool
		if (freeNode->seed != poolSeed)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"[ObjectPool Error] : Miss match poolSeed / freeObject Node : %016llx / Seed(%hu) != poolSeed(%hu)\n", freeNode, freeNode->seed, poolSeed);
			__debugbreak();
			return false;
		}
		if (freeNode->headFence != dfFenceValue || freeNode->tailFence != dfFenceValue)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"[ObjectPool Error] : memory access overflow (fence is not dfFenceValue) / freeObject Node : %016llx / Seed = %hu ", freeNode, freeNode->seed);
			__debugbreak();
			return false;
		}
#endif
		AcquireSRWLockExclusive(&poolLock);
		freeNode->next = top;
		top = freeNode;
		if (!bPreConstructor)
			objectPtr->~T();
		poolCnt++;
		allocCnt--;
		_LOG(dfLOG_LEVEL_DEBUG, L"[freeObject] : allocCnt = %d / poolCnt = %d\n", allocCnt, poolCnt);
		ReleaseSRWLockExclusive(&poolLock);
		return true;
	}

	inline ULONG GetPoolCnt()
	{
		return poolCnt;
	}

	inline ULONG GetAllocCnt()
	{
		return allocCnt;
	}

private:
	Node* top;
	bool bPreConstructor;
	USHORT poolSeed;
	ULONG allocCnt;
	ULONG poolCnt;
	
	SRWLOCK poolLock;
};
