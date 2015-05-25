#pragma once

#define MAX_IOTHREAD	8

enum THREAD_TYPE
{
	THREAD_MAIN,
	THREAD_IO_WORKER
};

class Timer;
class ThreadCallHistory;

extern __declspec(thread) int LThreadType;
extern __declspec(thread) int LWorkerThreadId;
extern __declspec(thread) ThreadCallHistory* LThreadCallHistory;
extern __declspec(thread) void* LRecentThisPointer;

extern ThreadCallHistory* GThreadCallHistory[MAX_IOTHREAD];

extern __declspec( thread ) Timer* LTimer;
extern __declspec( thread ) int64_t LTickCount;