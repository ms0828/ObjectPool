#pragma once
#include <iostream>
#include <Windows.h>
#include "Log.h"

template<typename T>
class CObjectPool
{
public:
	struct Node
	{
	public:
		T instance;
		USHORT seed;
		Node* next;
	};

public:

	//------------------------------------------------------------
	// 오브젝트 프리리스트
	//------------------------------------------------------------
	CObjectPool(bool preConstructor)
	{
		poolSeed = rand();
		bPreConstructor = preConstructor;
		top = nullptr;
		poolCnt = 0;
	}

	//------------------------------------------------------------
	// 오브젝트 풀
	// - 멀티 스레드 환경에서는 이 생성자 호출이 끝나고 사용할 것
	//------------------------------------------------------------
	CObjectPool(bool preConstructor, int poolNum)
	{
		poolSeed = rand();
		bPreConstructor = preConstructor;
		top = nullptr;
		for (int i = 0; i < poolNum; i++)
		{
			Node* newNode = (Node*)malloc(sizeof(Node));
			newNode->seed = poolSeed;
			newNode->next = top;
			top = newNode;

			// bPreConstructor가 true인 경우에만 생성자 호출
			if (bPreConstructor)
			{
				T* instance = (T*)newNode;
				new (instance) T();
			}
		}
		poolCnt = poolNum;
	}

	~CObjectPool()
	{
		Node* curNode = top;
		while (curNode != nullptr)
		{
			Node* deleteNode = curNode;
			curNode = curNode->next;
			if (bPreConstructor)
				delete deleteNode;
			else
				free(deleteNode);
		}
	}

	T* allocObject()
	{
		Node* t = nullptr;
		Node* nextTop = nullptr;
		Node* maskedAllocNode = nullptr;
		do
		{
			t = top;

			//----------------------------------------
			// 풀이 비어있을 때 오브젝트를 새로 생성하여 할당
			//----------------------------------------
			if (t == nullptr)
			{
				Node* newNode = (Node*)malloc(sizeof(Node));
				newNode->seed = poolSeed;
				new (newNode) T();
				return &(newNode->instance);
			}

			maskedAllocNode = (Node*)((ULONGLONG)t & nodeMask);
			nextTop = maskedAllocNode->next;
		} while (InterlockedCompareExchangePointer((void* volatile*)&top, nextTop, t) != t);
		InterlockedDecrement(&poolCnt);

		//----------------------------------------
		// bPreConstructor가 꺼져 있는 경우 할당마다 생성자가 호출
		//----------------------------------------
		if (!bPreConstructor)
			new (maskedAllocNode) T();
		
		return &(maskedAllocNode->instance);
	}

	bool freeObject(T* objectPtr)
	{
		Node* freeNode = (Node*)objectPtr;
		if (freeNode->seed != poolSeed)
		{
			_LOG(dfLOG_LEVEL_ERROR, L"Miss match poolSeed / freeObject Node : %016llx / Seed(%hu) != poolSeed(%hu)\n", freeNode, freeNode->seed, poolSeed);
			return false;
		}

		//--------------------------------------------------
		// freeNode(유저 영역의 메모리 주소)의 상위 17비트를 사용하여 노드 ID 부여
		//--------------------------------------------------
		ULONGLONG nodeID = InterlockedIncrement(&nodeSequence) % (1 << 17);
		freeNode = (Node*)((nodeID << 47) | (ULONGLONG)freeNode);

		Node* t;
		do
		{
			t = top;
			Node* maskedFreeNode = (Node*)((ULONGLONG)freeNode & nodeMask);
			maskedFreeNode->next = t;
		} while (InterlockedCompareExchangePointer((void* volatile*)&top, freeNode, t) != t);
		InterlockedIncrement(&poolCnt);

		if (!bPreConstructor)
			objectPtr->~T();

		return true;
	}

	ULONG GetPoolCnt()
	{
		return poolCnt;
	}

private:
	Node* top;
	bool bPreConstructor;
	USHORT poolSeed;
	ULONG poolCnt;

	//--------------------------------------------
	// 노드 생성 시, 노드 포인터 상위17비트에 저장하는 노드의 고유 인덱스
	//--------------------------------------------
	ULONGLONG nodeSequence;

	//--------------------------------------------
	// Node*의 하위 47비트 추출할 마스크
	//--------------------------------------------
	static const ULONGLONG nodeMask = (1ULL << 47) - 1;
};