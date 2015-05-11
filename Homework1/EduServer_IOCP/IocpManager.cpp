#include "stdafx.h"
#include "IocpManager.h"
#include "EduServer_IOCP.h"
#include "ClientSession.h"
#include "SessionManager.h"

#define GQCS_TIMEOUT	20

__declspec(thread) int LIoThreadId = 0;
IocpManager* GIocpManager = nullptr;

IocpManager::IocpManager() : mCompletionPort(NULL), mIoThreadCount(2), mListenSocket(NULL)
{
}


IocpManager::~IocpManager()
{
}

bool IocpManager::Initialize()
{
	//TODO: mIoThreadCount = ...;GetSystemInfo사용해서 set num of I/O threads
	SYSTEM_INFO si;
	ZeroMemory( &si , sizeof( si ) );
	GetSystemInfo( &si );
	mIoThreadCount = si.dwNumberOfProcessors * 2;

	/// winsock initializing
	WSADATA wsa;
	if( WSAStartup( MAKEWORD( 2 , 2 ) , &wsa ) != 0 )
		return false;

	/// Create I/O Completion Port
	//TODO: mCompletionPort = CreateIoCompletionPort(...)
	mCompletionPort = CreateIoCompletionPort( 
		INVALID_HANDLE_VALUE , NULL , 0 , 0 );
	if( mCompletionPort == NULL )
	{
		printf_s( "CreateIoCompletionPort error: %d\n" , GetLastError() );
		return false;
	}
	/// create TCP socket
	//TODO: mListenSocket = ...
	mListenSocket = WSASocket( AF_INET , SOCK_STREAM , 
							   IPPROTO_TCP , NULL , 0 , 
							   WSA_FLAG_OVERLAPPED );
	if( mListenSocket == INVALID_SOCKET )
	{
		printf_s( "WSASocket(listenSocket) error: %d\n" , GetLastError() );
		return false;
	}

	int opt = 1;
	if( SOCKET_ERROR == setsockopt( mListenSocket ,
		SOL_SOCKET , SO_REUSEADDR ,
		( const char* )&opt , sizeof( int ) ) )
	{
		printf_s( "setsocketopt error: %d\n" , GetLastError() );
		return false;
	}

	//TODO:  bind
	SOCKADDR_IN serveraddr;
	ZeroMemory( &serveraddr , sizeof( serveraddr ) );
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons( LISTEN_PORT );
	serveraddr.sin_addr.s_addr = htonl( INADDR_ANY );

	if( SOCKET_ERROR == bind( 
		mListenSocket , ( SOCKADDR* )&serveraddr , sizeof( serveraddr ) ) )
	{
		printf_s( "bind error: %d\n" , GetLastError() );
		return false;
	}

	return true;
}


bool IocpManager::StartIoThreads()
{
	/// I/O Thread
	for (int i = 0; i < mIoThreadCount; ++i)
	{
		DWORD dwThreadId;
		//TODO: HANDLE hThread = (HANDLE)_beginthreadex...);
		HANDLE hThread = ( HANDLE )_beginthreadex( NULL , 0 , IoWorkerThread , ( LPVOID )i , 0, ( unsigned int*)&dwThreadId );
		if( hThread == INVALID_HANDLE_VALUE )
		{
			printf_s( "_beginthreadex error: %d\n" , GetLastError() );
			return false;
		}
	}

	return true;
}


bool IocpManager::StartAcceptLoop()
{
	/// listen
	if (SOCKET_ERROR == listen(mListenSocket, SOMAXCONN))
		return false;


	/// accept loop
	while (true)
	{
		SOCKET acceptedSock = accept(mListenSocket, NULL, NULL);
		if (acceptedSock == INVALID_SOCKET)
		{
			printf_s("accept: invalid socket\n");
			continue;
		}

		SOCKADDR_IN clientaddr;
		int addrlen = sizeof(clientaddr);
		getpeername(acceptedSock, (SOCKADDR*)&clientaddr, &addrlen);

		/// 소켓 정보 구조체 할당과 초기화
		ClientSession* client = GSessionManager->CreateClientSession(acceptedSock);

		/// 클라 접속 처리
		if (false == client->OnConnect(&clientaddr))
		{
			client->Disconnect(DR_ONCONNECT_ERROR);
			GSessionManager->DeleteClientSession(client);
		}
	}

	return true;
}

void IocpManager::Finalize()
{
	CloseHandle( mCompletionPort );

	/// winsock finalizing
	WSACleanup();
}


unsigned int WINAPI IocpManager::IoWorkerThread (LPVOID lpParam)
{
	LThreadType = THREAD_IO_WORKER;

	LIoThreadId = reinterpret_cast<int>(lpParam);
	HANDLE hComletionPort = GIocpManager->GetComletionPort();

	while (true)
	{
		DWORD dwTransferred = 0;
		OverlappedIOContext* context = nullptr;
		ClientSession* asCompletionKey = nullptr;

		int ret = GetQueuedCompletionStatus( 
			hComletionPort , &dwTransferred, 
			(PULONG_PTR)&asCompletionKey, (LPOVERLAPPED*)&context, GQCS_TIMEOUT );
		DWORD errorCode = GetLastError();
		/// check time out first 
		if (ret == 0 &&  errorCode == WAIT_TIMEOUT)
			continue;

		if (ret == 0 || dwTransferred == 0)
		{
			/// connection closing
			asCompletionKey->Disconnect( DR_RECV_ZERO );
			GSessionManager->DeleteClientSession( asCompletionKey );
			continue;
		}

		if (nullptr == context)
		{
			/// 사실 이 부분은 끊으면 안되는건데.. 나중에 PQCS시에 context를 null로 세팅해서 넣는 경우도 있음.
			asCompletionKey->Disconnect( DR_RECV_ZERO ); ///< context가 null이 왔다고 0바이트 받았다고 에러 종료하면 안되지 않겠니?
			GSessionManager->DeleteClientSession( asCompletionKey );
			continue;
		}

		bool completionOk = true;
		switch (context->mIoType)
		{
		case IO_SEND:
			completionOk = SendCompletion(asCompletionKey, context, dwTransferred);
			break;

		case IO_RECV:
			completionOk = ReceiveCompletion(asCompletionKey, context, dwTransferred);
			break;

		default:
			printf_s("Unknown I/O Type: %d\n", context->mIoType);
			break;
		}

		if ( !completionOk )
		{
			/// connection closing
			asCompletionKey->Disconnect(DR_COMPLETION_ERROR);
			GSessionManager->DeleteClientSession(asCompletionKey);
		}

	}

	return 0;
}

bool IocpManager::ReceiveCompletion(const ClientSession* client, OverlappedIOContext* context, DWORD dwTransferred)
{
	/// echo back 처리 client->PostSend()사용.
	bool result = true; ///< 요놈은 누구?

	if( !client->PostSend( context->mBuffer , dwTransferred ) )
	{
		printf_s( "PostSend error: %d\n" , GetLastError() );
		delete context;
		return false;
	}

	delete context;
	return client->PostRecv();
}

bool IocpManager::SendCompletion(const ClientSession* client, OverlappedIOContext* context, DWORD dwTransferred)
{
	/// 전송 다 되었는지 확인하는 것 처리..
	//if (context->mWsaBuf.len != dwTransferred) {...}
	if( context->mWsaBuf.len != dwTransferred )
	{
		printf_s( "SendCompletion Error: Send-length isn't Synchronized\n" );
		delete context;
		return false;
	}
	delete context;
	return true;
}
