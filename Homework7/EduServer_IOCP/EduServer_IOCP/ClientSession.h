#pragma once

#include "MemoryPool.h"
#include "Session.h"
#include "Player.h"
#include "PacketType.h"

class ClientSessionManager;

class ClientSession : public Session, public PooledAllocatable
{
public:
	ClientSession();
	virtual ~ClientSession();

	void SessionReset();

	bool PostAccept();
	void AcceptCompletion();

	bool SendResponse(short packetType, const protobuf::MessageLite& payload);

	virtual void OnRead(size_t len);
	virtual void OnDisconnect(DisconnectReason dr);
	virtual void OnRelease();

public:
	std::shared_ptr<Player> mPlayer;

private:
	
	SOCKADDR_IN		mClientAddr ;

	
	friend class ClientSessionManager;
} ;



