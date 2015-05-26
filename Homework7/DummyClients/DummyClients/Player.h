#pragma once
#include "SyncExecutable.h"

class DummyClientSession;

class Player : public SyncExecutable
{
public:
	Player(DummyClientSession* session);
	~Player();

	bool IsValid()
	{
		return mPlayerId > 0;
	}
	int  GetPlayerId()
	{
		return mPlayerId;
	}
	std::string GetPlayerName()
	{
		return mPlayerName;
	}

	void Move();
	void RandomChat();

	void RequestLogin();
	void ResponseLogin(bool success, int pid, float x, float y, float z, const char* name);

	void RequestSignIn(const std::string& playerName);
	void ResponseSignIn(bool success, int pid);

	void RequestMove(float x, float y, float z);
	void ResponseMove(bool success, float x, float y, float z);

	void RequestChat(const std::string& message);
	void ResponseChat(bool success, const std::string& name, const std::string& message);

	void RequestLogout();
	void ResponseLogout(bool success);

	void OnTick();

private:
	void PlayerReset(); 

private:
	int	mPlayerId = -1;
	float mPosX = 0.f;
	float mPosY = 0.f;
	float mPosZ = 0.f;

	float mVecX = 0.f;
	float mVecY = 0.f;
	float mVecZ = 0.f;

	bool mIsValid = false;
	unsigned int mChatCount = 0;
	std::string mPlayerName;
	std::string mComment;

	DummyClientSession* const mSession;
	friend class DummyClientSession;
};

