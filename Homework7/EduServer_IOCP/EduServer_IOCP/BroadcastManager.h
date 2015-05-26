#pragma once
#include "XTL.h"
#include "FastSpinlock.h"
#include "google/protobuf/message_lite.h"
#include "Player.h"

struct PacketHeader;
class ClientSession;
class BroadcastManager : public std::enable_shared_from_this<BroadcastManager>
{
public:
	BroadcastManager();
	virtual ~BroadcastManager() {}

	void RegisterClient(ClientSession* client);
	void UnregisterClient(ClientSession* client);
	void BroadcastPacket(std::shared_ptr<Player> player, google::protobuf::MessageLite& pkt);
private:
	bool IsCloseEnough(ClientSession* lhs, ClientSession* rhs);

private:
	FastSpinlock mLock;
	xset<ClientSession*>::type mConnectedClientSet;
	int mCurrentConnectedCount;

};

extern std::shared_ptr<BroadcastManager> GBroadcastManager;