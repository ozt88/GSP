#pragma once
#include "ContentsConfig.h"
#include "SyncExecutable.h"

class ClientSession;


class Player : public SyncExecutable
{
public:
	Player(ClientSession* session);
	~Player();

	bool IsValid() const { return mPlayerId > 0; };
	bool IsCloseEnough(std::shared_ptr<Player> other) const;
	int  GetPlayerId() const { return mPlayerId;  }
	std::wstring GetPlayerName() const { return mPlayerName; }

	void RequestCreate(const wchar_t* name);
	void ResponseCreate(bool success, int pid, wchar_t* name);

	void RequestLoad(int pid);
	void ResponseLoad(bool success, int pid, float x, float y, float z, bool valid, wchar_t* name, wchar_t* comment);

	void RequestUpdatePosition(float x, float y, float z);
	void ResponseUpdatePosition(bool success, float x, float y, float z);

	void RequestUpdateComment(const wchar_t* comment);
	void ResponseUpdateComment(bool success, const wchar_t* comment);

	void RequestUpdateValidation(bool isValid);
	void ResponseUpdateValidation(bool success, bool isValid);

	void RequestLogout();
	void ResponseLogout(bool success);

	void OnTick(); ///< 로그인후 1초마다 불리는 기능

private:

	void PlayerReset();

	//TODO: 테스트용, 요놈들은 사실상 외부로 빼야 함
	void TestCreatePlayerData(const wchar_t* newName);
	void TestDeletePlayerData(int playerId);

private:

	int		mPlayerId;
	float	mPosX;
	float	mPosY;
	float	mPosZ;
	bool	mIsValid;
	wchar_t	mPlayerName[MAX_NAME_LEN];
	wchar_t	mComment[MAX_COMMENT_LEN];

	ClientSession* const mSession;
	friend class ClientSession;
};