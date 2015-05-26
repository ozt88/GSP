#include "stdafx.h"
#include "Player.h"
#include "DummyClientSession.h"
#include "MyPacket.pb.h"
#include "Log.h"
#include "ContentsConfig.h"

Player::Player(DummyClientSession* session)
	:mSession(session)
{
}


Player::~Player()
{
}


void Player::RequestLogin()
{
	static int id = 300;
	if(mPlayerId == -1)
	{
		mPlayerId = id++;
	}
	mPlayerName = "USA" + std::to_string(mPlayerId);
	MyPacket::LoginRequest loginRequest;
	loginRequest.set_playerid(mPlayerId);

	mSession->SendRequest(MyPacket::PKT_CS_LOGIN, loginRequest);
}

void Player::ResponseLogin(bool success, int pid, float x, float y, float z, const char* name)
{
	if(success)
	{
		mPlayerId = pid;
		mPosX = x;
		mPosY = y;
		mPosZ = z;
		
		mVecX = rand() + pid*1 % 100 - 50;
		mVecY = rand() + pid*2 % 100 - 50;
		mVecZ = rand() + pid*3 % 100 - 50;

		mPlayerName = name;

		std::stringstream reslog;
		reslog << "LOG: Player Login : PID[" << mPlayerId << "], X[" << mPosX << "], Y[" << mPosY << "], Z[" << mPosZ << "], NAME[" << mPlayerName << "]" << std::endl;
		std::string logString = reslog.str();

		EVENT_LOG(logString.c_str(), 0);
		std::cout << logString;

		DoSyncAfter(HEART_BEAT, GetSharedFromThis<Player>(), &Player::OnTick);
	}
	else
	{
		//이름 넣어서 새로가입
		RequestSignIn(mPlayerName);
	}
}


void Player::RequestSignIn(const std::string& playerName)
{
	mPlayerName = playerName;
	MyPacket::CreateRequest createRequest;
	createRequest.set_playername(playerName);

	mSession->SendRequest(MyPacket::PKT_CS_CREATE, createRequest);
}

void Player::ResponseSignIn(bool success, int pid)
{
	if(success)
	{
		mPlayerId = pid;
		RequestLogin();
	}
	else
	{
		std::string logString = "SignIn Fail With Name : " + mPlayerName;
		EVENT_LOG(logString.c_str(), 0);
		std::cout << logString << std::endl;

		RequestSignIn(mPlayerName + "1");
	}
}

void Player::OnTick()
{
	if(!IsValid())
		return;

	Move();
	RandomChat();

	DoSyncAfter(HEART_BEAT, GetSharedFromThis<Player>(), &Player::OnTick);
}

void Player::PlayerReset()
{
	mPlayerName.clear();
	mComment.clear();
	mPlayerId = -1;
	mIsValid = false;
	mPosX = mPosY = mPosZ = 0;
	mChatCount = 0;
}

void Player::RequestMove(float x, float y, float z)
{
	MyPacket::MoveRequest moveRequest;
	
	moveRequest.set_playerid(mPlayerId);
	MyPacket::Position* newPos = new MyPacket::Position();
	newPos->set_x(x);
	newPos->set_y(y);
	newPos->set_z(z);
	moveRequest.set_allocated_playerpos(newPos);

	mSession->SendRequest(MyPacket::PKT_CS_MOVE, moveRequest);
}

void Player::ResponseMove(bool success, float x, float y, float z)
{
	std::stringstream logStream;
	if(success)
	{
		mPosX = x;
		mPosY = y;
		mPosZ = z;
		logStream << "Player [" << mPlayerId << "]"
			<< " move to (" << mPosX << ", " << mPosY << ", " << mPosZ << ")" << std::endl;
		
	}
	else
	{
		logStream << "Player [" << mPlayerId << "]"
			<< " fail to move."<< std::endl;
	}
	std::string logString = logStream.str();
	EVENT_LOG(logString.c_str(), 0);
	std::cout << logString;
}

void Player::RequestChat(const std::string& message)
{
	MyPacket::ChatRequest chatRequest;
	chatRequest.set_playermessage(message);
	chatRequest.set_playerid(mPlayerId);
	mSession->SendRequest(MyPacket::PKT_CS_CHAT, chatRequest);
}

void Player::ResponseChat(bool success, const std::string& name, const std::string& message)
{
	std::stringstream logStream;
	std::string logString;
	if(success)
	{
		logStream << "[" << name << "]: " << message << std::endl;
	}
	else
	{
		logStream << "[" << name << "]: " << "message Failed" << std::endl;
	}

	logString = logStream.str();
	std::cout << logString;
}


void Player::Move()
{
	if(mPosX > 500.f || mPosX < -500.f)
	{
		mVecX *= -1;
	}
	if(mPosY > 500.f || mPosY < -500.f)
	{
		mVecY *= -1;
	}
	if(mPosZ > 500.f || mPosZ < -500.f)
	{
		mVecZ *= -1;
	}

	float newX = mPosX + mVecX;
	float newY = mPosY + mVecY;
	float newZ = mPosZ + mVecZ;

	RequestMove(newX, newY, newZ);
}

void Player::RandomChat()
{
	if(mChatCount++ < 100)
	{
		std::stringstream messageStream;
		messageStream << "I said " << mChatCount << " words.";
		std::string message = messageStream.str();
		RequestChat(message);
	}
	else
	{
		RequestLogout();
	}
}

void Player::RequestLogout()
{
	MyPacket::LogoutRequest logoutRequest;
	logoutRequest.set_playerid(mPlayerId);
	mPlayerId = -1;
	mSession->SendRequest(MyPacket::PKT_CS_LOGOUT, logoutRequest);
}

void Player::ResponseLogout(bool success)
{
	mSession->DisconnectRequest(DR_NONE);
}
