#include "stdafx.h"
#include "Exception.h"
#include "ThreadLocal.h"
#include "DBHelper.h"

//DONE: DbHelper의 static 멤버변수 초기화
SQLHENV DbHelper::mSqlHenv = nullptr;
SQL_CONN* DbHelper::mSqlConnPool = nullptr; 
int DbHelper::mDbWorkerThreadCount = 0;

DbHelper::DbHelper()
{
	CRASH_ASSERT(mSqlConnPool[LWorkerThreadId].mUsingNow == false);

	mCurrentSqlHstmt = mSqlConnPool[LWorkerThreadId].mSqlHstmt;
	mCurrentResultCol = 1;
	mCurrentBindParam = 1;
	CRASH_ASSERT(mCurrentSqlHstmt != nullptr);

	mSqlConnPool[LWorkerThreadId].mUsingNow = true;
}

DbHelper::~DbHelper()
{
	//DONE: SQLFreeStmt를 이용하여 현재 SQLHSTMT 해제(unbind, 파라미터리셋, close 순서로)

	SQLFreeStmt(mSqlHenv, SQL_UNBIND);
	SQLFreeStmt(mSqlHenv, SQL_RESET_PARAMS);
	SQLFreeStmt(mSqlHenv, SQL_CLOSE);

	mSqlConnPool[LWorkerThreadId].mUsingNow = false;
}

bool DbHelper::Initialize(const wchar_t* connInfoStr, int workerThreadCount)
{
	//DONE: mSqlConnPool, mDbWorkerThreadCount를 워커스레스 수에 맞추어 초기화
	mSqlConnPool = new SQL_CONN[workerThreadCount];
	mDbWorkerThreadCount = workerThreadCount;
	

	//환경 핸들 할당
	if (SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &mSqlHenv))
	{
		printf_s("DbHelper Initialize SQLAllocHandle failed\n");
		return false;
	}

	//환경 속성 초기화
	if (SQL_SUCCESS != SQLSetEnvAttr(mSqlHenv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, SQL_IS_INTEGER))
	{
		printf_s("DbHelper Initialize SQLSetEnvAttr failed\n");
		return false;
	}
		

	/// 스레드별로 SQL connection을 풀링하는 방식. 즉, 스레드마다 SQL서버로의 연결을 갖는다.
	for (int i = 0; i < mDbWorkerThreadCount; ++i)
	{
		//DONE: SQLAllocHandle을 이용하여 SQL_CONN의 mSqlHdbc 핸들 사용가능하도록 처리
		//Connection 핸들
		if(SQL_SUCCESS != SQLAllocHandle(SQL_HANDLE_DBC, mSqlHenv, &mSqlConnPool[i].mSqlHdbc))
		{
			printf_s("DbHelper Initialize SQLAllocHandle SQL_HANDLE_DBC failed\n");
			return false;
		}

		SQLSMALLINT resultLen = 0;
		
		//DONE: SQLDriverConnect를 이용하여 SQL서버에 연결하고 그 핸들을 SQL_CONN의 mSqlHdbc에 할당
		SQLRETURN ret = SQLDriverConnect(
			mSqlConnPool[i].mSqlHdbc, //Connection Handle
			NULL, //WindowHandle 거의안씀
			(SQLWCHAR *)connInfoStr,//InConnectionString
			SQL_NTS, //StringLength, NULL종료
			NULL, //OutConnectionString
			0, //StringLength
			&resultLen, //StringLength2Ptr
			SQL_DRIVER_NOPROMPT //DriverCompletion
			);

		///# SQLRETURN ret = SQLDriverConnect(mSqlConnPool[i].mSqlHdbc, NULL, (SQLWCHAR*)connInfoStr, (SQLSMALLINT)wcslen(connInfoStr), NULL, 0, &resultLen, SQL_DRIVER_NOPROMPT);


		if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
		{
			SQLWCHAR sqlState[1024] = { 0, } ;
			SQLINTEGER nativeError = 0;
			SQLWCHAR msgText[1024] = { 0, } ;
			SQLSMALLINT textLen = 0 ;

			SQLGetDiagRec(SQL_HANDLE_DBC, mSqlConnPool[i].mSqlHdbc, 1, sqlState, &nativeError, msgText, 1024, &textLen);

			wprintf_s(L"DbHelper Initialize SQLDriverConnect failed: %s \n", msgText);

			return false;
		}

		//DONE: SQLAllocHandle를 이용하여 SQL_CONN의 mSqlHstmt 핸들 사용가능하도록 처리
	
		if(SQL_SUCCESS != SQLAllocHandle(
			SQL_HANDLE_STMT, 
			mSqlConnPool[i].mSqlHdbc, 
			&mSqlConnPool[i].mSqlHstmt))
		{
			printf_s("DbHelper Initialize SQLAllocHandle SQL_HANDLE_STMT failed\n");
			return false;
		}
	}

	return true;
}


void DbHelper::Finalize()
{
	for (int i = 0; i < mDbWorkerThreadCount; ++i)
	{
		SQL_CONN* currConn = &mSqlConnPool[i];
		if (currConn->mSqlHstmt)
			SQLFreeHandle(SQL_HANDLE_STMT, currConn->mSqlHstmt);

		if (currConn->mSqlHdbc)
			SQLFreeHandle(SQL_HANDLE_DBC, currConn->mSqlHdbc);
	}

	delete[] mSqlConnPool;


}

bool DbHelper::Execute(const wchar_t* sqlstmt)
{
	//DONE: mCurrentSqlHstmt핸들 사용하여 sqlstmt를 수행.  
	SQLRETURN ret = SQLExecDirect(
		mCurrentSqlHstmt, 
		(SQLWCHAR *)sqlstmt,
		SQL_NTS  //NULL로 끝나는 문자열
		);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
		return false;
	}

	return true;
}

bool DbHelper::FetchRow()
{
	//DONE: mCurrentSqlHstmt가 들고 있는 내용 fetch
	SQLRETURN ret = SQLFetch(mCurrentSqlHstmt); 

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		if (SQL_NO_DATA != ret)
		{
			PrintSqlStmtError();
		}
		
		return false;
	}

	return true;
}



bool DbHelper::BindParamInt(int* param)
{
	//DONE: int형 파라미터 바인딩
	SQLRETURN ret = SQLBindParameter(
		mCurrentSqlHstmt, 
		mCurrentBindParam++,
		SQL_PARAM_INPUT, //input / output
		SQL_C_LONG, //변수의 c타입
		SQL_IS_INTEGER,//변수의 sql타입
		0, //컬럼의 precision
		0, //컬럼의 scale
		param, //바인딩 버퍼 포인터
		0, //버퍼 사이즈
		NULL
		);

	///#이렇게: SQLRETURN ret = SQLBindParameter(mCurrentSqlHstmt, mCurrentBindParam++, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 10, 0, param, 0, NULL);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
		return false;
	}

	return true;
}

bool DbHelper::BindParamFloat(float* param)
{
	SQLRETURN ret = SQLBindParameter(mCurrentSqlHstmt, mCurrentBindParam++, SQL_PARAM_INPUT,
		SQL_C_FLOAT, SQL_REAL, 15, 0, param, 0, NULL);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
		return false;
	}

	return true;
}

bool DbHelper::BindParamBool(bool* param)
{
	//DONE: bool형 파라미터 바인딩
	SQLRETURN ret = SQLBindParameter(
		mCurrentSqlHstmt,
		mCurrentBindParam++,
		SQL_PARAM_INPUT,
		SQL_C_BIT, SQL_BIT,
		0, 0, param, 0, NULL
		);

	///# 불은 1바이트로 맵핑: SQLRETURN ret = SQLBindParameter(mCurrentSqlHstmt, mCurrentBindParam++, SQL_PARAM_INPUT, SQL_C_TINYINT, SQL_TINYINT, 3, 0, param, 0, NULL);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
		return false;
	}

	return true;
}

bool DbHelper::BindParamText(const wchar_t* text)
{

	//DONE: 유니코드 문자열 바인딩
	size_t len = wcslen(text);
	SQLRETURN ret = SQLBindParameter(
		mCurrentSqlHstmt,
		mCurrentBindParam++,
		SQL_PARAM_INPUT,
		SQL_C_WCHAR, SQL_WVARCHAR,
		len, 0, (SQLPOINTER) text, 0, NULL
		);
									 

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
		return false;
	}

	return true;
}


void DbHelper::BindResultColumnInt(int* r)
{
	SQLLEN len = 0;
	SQLRETURN ret = SQLBindCol(mCurrentSqlHstmt, mCurrentResultCol++, SQL_C_LONG, r, 4, &len);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
	}
}
void DbHelper::BindResultColumnFloat(float* r)
{
	SQLLEN len = 0;
	//DONE: float형 결과 컬럼 바인딩
	SQLRETURN ret = SQLBindCol(
		mCurrentSqlHstmt, 
		mCurrentResultCol++, 
		SQL_C_FLOAT, //CType
		r, //dstPointer
		4, //BufferSize
		&len //결과 length
		);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
	}
}

void DbHelper::BindResultColumnBool(bool* r)
{
	SQLLEN len = 0;
	SQLRETURN ret = SQLBindCol(mCurrentSqlHstmt, mCurrentResultCol++, SQL_C_TINYINT, r, 1, &len);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
	}
}
void DbHelper::BindResultColumnText(wchar_t* text, size_t count)
{
	SQLLEN len = 0;
	//DONE: wchar_t*형 결과 컬럼 바인딩
	SQLRETURN ret = SQLBindCol(
		mCurrentSqlHstmt,
		mCurrentResultCol++,
		SQL_C_WCHAR, //CType
		text, //dstPointer
		count*2, //BufferSize  여기서 wchar_t는 2바이트
		&len //결과 length
		);

	if (SQL_SUCCESS != ret && SQL_SUCCESS_WITH_INFO != ret)
	{
		PrintSqlStmtError();
	}
}


void DbHelper::PrintSqlStmtError()
{
	SQLWCHAR sqlState[1024] = { 0, };
	SQLINTEGER nativeError = 0;
	SQLWCHAR msgText[1024] = { 0, };
	SQLSMALLINT textLen = 0;

	SQLGetDiagRec(SQL_HANDLE_STMT, mCurrentSqlHstmt, 1, sqlState, &nativeError, msgText, 1024, &textLen);

	wprintf_s(L"DbHelper SQL Statement Error: %ls \n", msgText);
}