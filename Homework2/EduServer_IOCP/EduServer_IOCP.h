#pragma once

//에러는 아니지만 포트번호 수정
#define LISTEN_PORT		9001
#define MAX_CONNECTION	10000

enum THREAD_TYPE
{
	THREAD_MAIN,
	THREAD_IO_WORKER
};

extern __declspec(thread) int LThreadType;