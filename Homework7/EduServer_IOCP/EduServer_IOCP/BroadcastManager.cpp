#include "stdafx.h"
#include "Exception.h"
#include "BroadcastManager.h"
#include "ClientSession.h"

#include "PacketType.h"
#include "Player.h"

std::shared_ptr<BroadcastManager> GBroadcastManager;

BroadcastManager::BroadcastManager() : mCurrentConnectedCount(0), mLock(LO_ECONOMLY_CLASS)
{

}

void BroadcastManager::RegisterClient(ClientSession* client)
{
	FastSpinlockGuard criticalSection(mLock);

	auto result = mConnectedClientSet.insert(client);

	if (result.second) 
		++mCurrentConnectedCount;
	else
		CRASH_ASSERT(false); ///< 이미 존재하는 경우
	
}

void BroadcastManager::UnregisterClient(ClientSession* client)
{
	FastSpinlockGuard criticalSection(mLock);

	if (mConnectedClientSet.erase(client) > 0)
		--mCurrentConnectedCount;
	else
		CRASH_ASSERT(false); ///< 지울게 없는 경우로 쌍이 맞지 않는 경우다
}

void BroadcastManager::BroadcastPacket(std::shared_ptr<Player> player, google::protobuf::MessageLite& pkt)
{
	FastSpinlockGuard criticalSection(mLock);
	for(auto it : mConnectedClientSet)
	{
		if(!(it->mPlayer->IsCloseEnough(player)))
		{
			continue;
		}
		if(false == it->SendResponse(MyPacket::PKT_SC_CHAT, pkt))
		{
			it->DisconnectRequest(DR_ACTIVE);
		}
	}
}
