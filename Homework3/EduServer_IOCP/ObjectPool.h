#pragma once


#include "Exception.h"
#include "FastSpinlock.h"


template <class TOBJECT, int ALLOC_COUNT = 100>
class ObjectPool : public ClassTypeLock<TOBJECT>  ///# 이거 왜 빼먹음?
{
public:

	static void* operator new(size_t objSize)
	{
		//TODO: TOBJECT 타입 단위로 lock 잠금
		//ClassTypeLock<TOBJECT>::LockGuard(); ///# 이렇게 하면 소멸자는 언제 콜되나? 이게 lock이 제대로 걸리는지?
		ClassTypeLock<TOBJECT>::LockGuard lock; //옥상에 가서 수정했습니다.
		
		/// 즉 아래 코드가 thread-safe한가?
		
		//freeList 체크
		if (mFreeList == nullptr)
		{
			//필요한 바이트 개수만큼 오브젝트 freeList의 chunk들을 할당
			mFreeList = new uint8_t[sizeof(TOBJECT)*ALLOC_COUNT];

			//다음번 오브젝트의 주소
			uint8_t* pNext = mFreeList;
			//FLink와 같이 다음번 주소를 메모리 공간에 저장한다.
			uint8_t** ppCurr = reinterpret_cast<uint8_t**>(mFreeList);

			for (int i = 0; i < ALLOC_COUNT - 1; ++i)
			{
				//오브젝트 할당 공간 맨 앞(unint8_t사이즈만큼)에 
				//FLink를 저장한다.
				pNext += sizeof(TOBJECT);
				*ppCurr = pNext;
				ppCurr = reinterpret_cast<uint8_t**>(pNext);
			}
			//할당할 수 있는 토탈 카운트 ++
			mTotalAllocCount += ALLOC_COUNT;
		}
		
		//pAvailable은 사용가능한 공간의 포인터
		//현재 사용가능한 공간은 mFreeList(맨 앞)에 있는 오브젝트 공간
		uint8_t* pAvailable = mFreeList;
		//mFreeList는 FLink를 따라 이동
		mFreeList = *reinterpret_cast<uint8_t**>(pAvailable);
		
		//현재 사용하는 카운트 ++
		//다썼을 때 처리없는 버그
		//++mCurrentUseCount;
		if(++mCurrentUseCount > mTotalAllocCount)
		{
			mFreeList = nullptr;
		}

		return pAvailable;
	}

	static void	operator delete(void* obj)
	{
		//TODO: TOBJECT 타입 단위로 lock 잠금
		ClassTypeLock<TOBJECT>::LockGuard lock; //옥상에 가서 수정했습니다.

	
		CRASH_ASSERT(mCurrentUseCount > 0);

		//반환하기 전에 카운트 --
		--mCurrentUseCount;

		//반환된 free chunk에 FLink입력
		*reinterpret_cast<uint8_t**>(obj) = mFreeList;
		//새로운 freeList의 최초 공간은 반환된 오브젝트
		mFreeList = static_cast<uint8_t*>(obj);
	}


private:
	static uint8_t*	mFreeList;
	static int		mTotalAllocCount; ///< for tracing
	static int		mCurrentUseCount; ///< for tracing

};


template <class TOBJECT, int ALLOC_COUNT>
uint8_t* ObjectPool<TOBJECT, ALLOC_COUNT>::mFreeList = nullptr;

template <class TOBJECT, int ALLOC_COUNT>
int ObjectPool<TOBJECT, ALLOC_COUNT>::mTotalAllocCount = 0;

template <class TOBJECT, int ALLOC_COUNT>
int ObjectPool<TOBJECT, ALLOC_COUNT>::mCurrentUseCount = 0;

