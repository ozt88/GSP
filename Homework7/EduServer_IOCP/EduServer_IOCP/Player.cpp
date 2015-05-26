#include "stdafx.h"
#include "Timer.h"
#include "ClientSession.h"
#include "Player.h"
#include "PlayerDBContext.h"
#include "DBManager.h"
#include "GrandCentralExecuter.h"

#include "PacketType.h"
#include "BroadcastManager.h"
#include "MyPacket.pb.h"
#include "Log.h"

using namespace MyPacket;

Player::Player(ClientSession* session) : mSession(session)
{
	PlayerReset();
}

Player::~Player()
{
}

void Player::PlayerReset()
{
	memset(mPlayerName, 0, sizeof(mPlayerName));
	memset(mComment, 0, sizeof(mComment));
	mPlayerId = -1;
	mIsValid = false;
	mPosX = mPosY = mPosZ = 0;
}

void Player::RequestCreate(const wchar_t* name)
{
	CreatePlayerDataContext* context = new CreatePlayerDataContext(mSession);
	context->SetNewName(name);
	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::ResponseCreate(bool success, int pid, wchar_t* name)
{
	CreateResponse outPacket;
	if(success)
	{
		mPlayerId = pid;
		outPacket.set_playerid(pid);
		wcscpy_s(mPlayerName, name);
	}
	else
	{
		outPacket.set_playerid(-1);
	}

	std::stringstream reslog;
	reslog << "LOG: ResponseCreate : PID[" << pid << "], NAME[" << name << "], SUCCESS[" << success << "]" << std::endl;
	std::string logString = reslog.str();
	EVENT_LOG(logString.c_str(), 0);
	std::cout << logString;

	mSession->SendResponse(PKT_SC_CREATE, outPacket);
}

void Player::RequestLoad(int pid)
{
 	LoadPlayerDataContext* context = new LoadPlayerDataContext(mSession, pid);
 	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::ResponseLoad(bool success, int pid, float x, float y, float z, bool valid, wchar_t* name, wchar_t* comment)
{
	LoginResult outPacket;
	Position* pos = new Position();
	std::string outName;

	if(success)
	{
		mPlayerId = pid;
		mPosX = x;
		mPosY = y;
		mPosZ = z;
		mIsValid = valid;

		wcscpy_s(mPlayerName, name);
		wcscpy_s(mComment, comment);

		outPacket.set_playerid(mPlayerId);
		pos->set_x(mPosX);
		pos->set_y(mPosY);
		pos->set_z(mPosZ);

		std::wstring inName = mPlayerName;
		outName.assign(inName.begin(), inName.end());

		//Login Validation false;
		RequestUpdateValidation(false);

		DoSyncAfter(HEART_BEAT, GetSharedFromThis<Player>(), &Player::OnTick);
	}
	else
	{
		outPacket.set_playerid(-1);
		pos->set_x(0.f);
		pos->set_y(0.f);
		pos->set_z(0.f);
		outName = "";
	}
	std::stringstream reslog;
	reslog << "LOG: ResponseLoad : PID["<< mPlayerId << "], X["<< mPosX << "], Y[" << mPosY << "], Z["<<mPosZ << "], NAME["<<mPlayerName<< "], COMMENT["<<mComment << "]"<< std::endl;
	std::string logString = reslog.str();

	EVENT_LOG(logString.c_str(), 0);
	std::cout << logString;

	outPacket.set_allocated_playerpos(pos);
	outPacket.set_playername(outName);
	mSession->SendResponse(PKT_SC_LOGIN, outPacket);

}

void Player::RequestUpdatePosition(float x, float y, float z)
{
	UpdatePlayerPositionContext* context = new UpdatePlayerPositionContext(mSession, mPlayerId);
	context->mPosX = x;
	context->mPosY = y;
	context->mPosZ = z;
	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::ResponseUpdatePosition(bool success, float x, float y, float z)
{
	MoveResult outPacket;
	Position* pos = new Position();

	if(success)
	{
		mPosX = x;
		mPosY = y;
		mPosZ = z;

		outPacket.set_playerid(mPlayerId);
		pos->set_x(mPosX);
		pos->set_y(mPosY);
		pos->set_z(mPosZ);
	}
	else
	{
		pos->set_x(0.f);
		pos->set_y(0.f);
		pos->set_z(0.f);
		outPacket.set_playerid(-1);
	}

	outPacket.set_allocated_playerpos(pos);
	mSession->SendResponse(PKT_SC_MOVE, outPacket);
}

void Player::RequestUpdateComment(const wchar_t* comment)
{
	UpdatePlayerCommentContext* context = new UpdatePlayerCommentContext(mSession, mPlayerId);
	context->SetNewComment(comment);
	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::ResponseUpdateComment(bool success, const wchar_t* comment)
{
	ChatResult outPacket;
	std::wstring inComment;
	std::wstring inName;
	std::string outComment;
	std::string outName;
	if(success)
	{
		inComment = comment;
		inName = mPlayerName;
		wcscpy_s(mComment, comment);
	}
	else
	{
		inComment = L"";
		inName = L"";
	}

	outComment.assign(inComment.begin(), inComment.end());
	outName.assign(inName.begin(), inName.end());

	outPacket.set_playermessage(outComment.c_str());
	outPacket.set_playername(outName.c_str());
	mSession->SendResponse(PKT_SC_CHAT, outPacket);
}

void Player::RequestUpdateValidation(bool isValid)
{
	UpdatePlayerValidContext* context = new UpdatePlayerValidContext(mSession, mPlayerId);
	context->mIsValid = isValid;
	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::ResponseUpdateValidation(bool success, bool isValid)
{
	if(isValid == true)
	{
		ResponseLogout(success);
	}
}


void Player::RequestLogout()
{
	RequestUpdateValidation(true);
}

void Player::ResponseLogout(bool success)
{
	LogoutResult outPacket;
	if(success)
	{
		outPacket.set_playerid(mPlayerId);
	}
	else
	{
		outPacket.set_playerid(-1);
	}
	mSession->SendResponse(PKT_SC_LOGOUT, outPacket);
}

void Player::TestCreatePlayerData(const wchar_t* newName)
{
	CreatePlayerDataContext* context = new CreatePlayerDataContext(mSession);
	context->SetNewName(newName);
	GDatabaseManager->PostDatabsaseRequest(context);
}

void Player::TestDeletePlayerData(int playerId)
{
	DeletePlayerDataContext* context = new DeletePlayerDataContext(mSession, playerId);
	GDatabaseManager->PostDatabsaseRequest(context);
}


void Player::OnTick()
{
	if (!IsValid())
		return;

// lock order Å×½ºÆ®
	//static FastSpinlock testlock(LO_BUSINESS_CLASS);
	//testlock.EnterReadLock();
	//printf("tick: lockflag %d, use count %d\n", testlock.GetLockFlag(), GetSharedFromThis<Player>().use_count());
	//testlock.LeaveReadLock();

	DoSyncAfter(HEART_BEAT, GetSharedFromThis<Player>(), &Player::OnTick);
}



bool Player::IsCloseEnough(std::shared_ptr<Player> other) const
{
	float distanceSquare = pow(mPosX - other->mPosX, 2) + pow(mPosY - other->mPosY, 2) + pow(mPosZ - other->mPosZ, 2);
	return distanceSquare > 10.f;
}




// 	class PlayerJob : public std::enable_shared_from_this<PlayerJob>
// 	{
// 	public:
// 		PlayerJob(int pid) : mPID(pid)
// 		{
// 			printf("ctor %d\n ", mPID);
// 		}
// 		virtual ~PlayerJob()
// 		{
// 			printf("dtor %d\n ", mPID);
// 		}
// 
// 		void TestFunc(int a)
// 		{
// 			printf("     Hello %d %d\n", mPID, a);
// 		}
// 
// 	private:
// 		int mPID;
// 	};
// 
// 	auto p = std::make_shared<PlayerJob>(mPlayerId);
// 
// 	GCEDispatch(p, &PlayerJob::TestFunc, mPlayerId+1);