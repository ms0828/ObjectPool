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

		for (int i = 0; i < poolNum; i++)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));
			newNode->headFence = dfFenceValue;
			newNode->tailFence = dfFenceValue;
			newNode->seed = poolSeed;
			newNode->next = top;
			top = PackingNode(newNode, GetNodeStamp(top) + 1);

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
		while (1)
		{
			Node* t;
			Node* maskedT;
			Node* nextTop;
			do
			{
				t = top;
				maskedT = UnpackingNode(t);
				if (maskedT == nullptr)
					return;
				nextTop = PackingNode(maskedT->next, GetNodeStamp(t) + 1);
			} while (InterlockedCompareExchangePointer((void* volatile*)&top, nextTop, t) != t);
			//------------------------------------------------------------------------------
			// bPreConstructor�� true�� ���, ObjectPool�� �Ҹ�� �� instance�� �Ҹ��� ȣ��
			//------------------------------------------------------------------------------
			if (bPreConstructor)
				delete maskedT;
			else
				free(maskedT);
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
		Node* t = nullptr;
		Node* nextTop = nullptr;
		Node* maskedT = nullptr;

		do
		{
			t = top;
			maskedT = UnpackingNode(t);
			if (maskedT == nullptr)
			{
				Node* newNode = (Node*)malloc(sizeof(Node));
				newNode->headFence = dfFenceValue;
				newNode->tailFence = dfFenceValue;
				newNode->seed = poolSeed;
				newNode->next = nullptr;
				T* instance = &newNode->instance;
				new (instance) T(std::forward<Args>(args)...);
			#ifdef dfDebugObjectPool
				InterlockedIncrement(&allocCnt);
			#endif
				return instance;
			}

			nextTop = PackingNode(maskedT->next, GetNodeStamp(t) + 1);
		} while (InterlockedCompareExchangePointer((void* volatile*)&top, nextTop, t) != t);

		T* instance = &maskedT->instance;
		if (!bPreConstructor)
			new (instance) T(std::forward<Args>(args)...);

#ifdef dfDebugObjectPool
		InterlockedDecrement(&poolCnt);
		InterlockedIncrement(&allocCnt);
#endif

		return instance;
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

		Node* t;
		Node* nextTop;
		Node* maskedT;
		do
		{
			t = top;
			maskedT = UnpackingNode(t);
			freeNode->next = maskedT;
			nextTop = PackingNode(freeNode, GetNodeStamp(t) + 1);
		} while (InterlockedCompareExchangePointer((void* volatile*)&top, nextTop, t) != t);


		//------------------------------------------------------------------------------------------------
		// �� ���� ������ �̿��� ������Ʈ Ǯ ������ �Ѱ�
		// - ���� ���� �ʾ����� �Ҹ��� ȣ�� ������ �̹� �ٸ� �����忡�� �ش� ������Ʈ�� �Ҵ� �޾��� �� �ִ�.
		// - �� ���� ���� ������Ʈ Ǯ�� �ݳ� �� �Ҹ��� ȣ�� �Ұ� (������ �� ���� ���� ������Ʈ Ǯ�� �� ���� �ڷᱸ���� ��� Ǯ������ ����ϱ� ������ �Ҹ��� ȣ���� �ʿ� ����)
		//------------------------------------------------------------------------------------------------
		//if (!bPreConstructor)
			//objectPtr->~T();

#ifdef dfDebugObjectPool
		InterlockedIncrement(&poolCnt);
		InterlockedDecrement(&allocCnt);
#endif

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


	inline Node* PackingNode(Node* ptr, ULONGLONG stamp)
	{
		return (Node*)((ULONGLONG)ptr | (stamp << stampShift));
	}
	inline Node* UnpackingNode(Node* ptr)
	{
		return (Node*)((ULONGLONG)ptr & nodeMask);
	}
	inline ULONGLONG GetNodeStamp(Node* ptr)
	{
		return (ULONGLONG)ptr >> stampShift;
	}

private:
	Node* top;
	bool bPreConstructor;
	USHORT poolSeed;
	ULONG allocCnt;
	ULONG poolCnt;
	

	//--------------------------------------------
	// Node*�� ���� 47��Ʈ ������ ����ũ
	//--------------------------------------------
	static const ULONGLONG nodeMask = (1ULL << 47) - 1;
	static const ULONG stampShift = 47;
};
