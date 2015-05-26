#include "stdafx.h"
#include "Log.h"
#include "PacketType.h"
#include "Player.h"
#include "ClientSession.h"
#include "BroadcastManager.h"
#include "MyPacket.pb.h"

using namespace MyPacket;
//@{ Handler Helper

typedef void(*HandlerFunc)( ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream );

static HandlerFunc HandlerTable[PKT_MAX];

static void DefaultHandler(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream)
{

	printf_s("Default Handler...PKT ID: %d\n", pktBase.mType);
}

struct InitializeHandlers
{
	InitializeHandlers()
	{
		for(int i = 0; i < PKT_MAX; ++i)
			HandlerTable[i] = DefaultHandler;
	}
} _init_handlers_;

struct RegisterHandler
{
	RegisterHandler(int pktType, HandlerFunc handler)
	{
		HandlerTable[pktType] = handler;
	}
};

#define REGISTER_HANDLER(PKT_TYPE)	\
	static void Handler_##PKT_TYPE(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream); \
	static RegisterHandler _register_##PKT_TYPE(PKT_TYPE, Handler_##PKT_TYPE); \
	static void Handler_##PKT_TYPE(ClientSession* session, PacketHeader& pktBase, protobuf::io::CodedInputStream& payloadStream)


//@}


void ClientSession::OnRead(size_t len)
{
	/// 패킷 파싱하고 처리
	protobuf::io::ArrayInputStream arrayInputStream(mRecvBuffer.GetBufferStart(), mRecvBuffer.GetContiguiousBytes());
	protobuf::io::CodedInputStream codedInputStream(&arrayInputStream);

	PacketHeader packetheader;

	while(codedInputStream.ReadRaw(&packetheader, HEADER_SIZE))
	{
		const void* payloadPos = nullptr;
		int payloadSize = 0;

		codedInputStream.GetDirectBufferPointer(&payloadPos, &payloadSize);

		if(payloadSize < packetheader.mSize) ///< 패킷 본체 사이즈 체크
			break;

		if(packetheader.mType >= PKT_MAX || packetheader.mType <= 0)
		{
			DisconnectRequest(DR_ACTIVE);
			break;
		}

		/// payload 읽기
		protobuf::io::ArrayInputStream payloadArrayStream(payloadPos, packetheader.mSize);
		protobuf::io::CodedInputStream payloadInputStream(&payloadArrayStream);

		/// packet dispatch...
		HandlerTable[packetheader.mType](this, packetheader, payloadInputStream);

		/// 읽은 만큼 전진 및 버퍼에서 제거
		codedInputStream.Skip(packetheader.mSize); ///< readraw에서 헤더 크기만큼 미리 전진했기때문
		mRecvBuffer.Remove(HEADER_SIZE + packetheader.mSize);

	}
}

/////////////////////////////////////////////////////////
REGISTER_HANDLER(PKT_CS_LOGIN)
{
	LoginRequest inPacket;
	if(false == inPacket.ParseFromCodedStream(&payloadStream))
	{
		EVENT_LOG("packet parsing error", PKT_CS_LOGIN);
		return;
	}
	
	/// 테스트를 위해 10ms후에 로딩하도록 ㄱㄱ
	DoSyncAfter(10, session->mPlayer, &Player::RequestLoad, inPacket.playerid());

}

REGISTER_HANDLER(PKT_CS_MOVE)
{
	MoveRequest inPacket;
	if(false == inPacket.ParseFromCodedStream(&payloadStream))
	{
		EVENT_LOG("packet parsing error", PKT_CS_MOVE);
		return;
	}

	if (inPacket.playerid() != session->mPlayer->GetPlayerId())
	{
		EVENT_LOG("PKT_CS_MOVE: invalid player ID", session->mPlayer->GetPlayerId());
		return;
	}

	/// 지금은 성능 테스트를 위해 DB에 업데이트하고 통보하도록 하자.
	session->mPlayer->DoSync(&Player::RequestUpdatePosition, 
							 inPacket.playerpos().x(), inPacket.playerpos().y(), inPacket.playerpos().z());
}

REGISTER_HANDLER(PKT_CS_CHAT)
{
	ChatRequest inPacket;
	if(false == inPacket.ParseFromCodedStream(&payloadStream))
	{
		EVENT_LOG("packet parsing error", PKT_CS_CHAT);
		return;
	}

	if (inPacket.playerid() != session->mPlayer->GetPlayerId())
	{
		EVENT_LOG("PKT_CS_CHAT: invalid player ID", session->mPlayer->GetPlayerId());
		return;
	}
		
	/// chatting의 경우 여기서 바로 방송
	ChatResult outPacket;
	std::wstring& inName = session->mPlayer->GetPlayerName();
	std::string outName;
	outName.assign(inName.begin(), inName.end());
	outPacket.set_playername(outName);
	outPacket.set_playermessage(inPacket.playermessage());

	GBroadcastManager->BroadcastPacket(session->mPlayer, outPacket);
}

REGISTER_HANDLER(PKT_CS_CREATE)
{
	CreateResquest inPacket;
	if(false == inPacket.ParseFromCodedStream(&payloadStream))
	{
		LoggerUtil::EventLog("packet parsing error", PKT_CS_CHAT);
		return;
	}

	std::string inName = inPacket.playername();
	std::wstring outName;
	outName.assign(inName.begin(), inName.end());

	session->mPlayer->DoSync(&Player::RequestCreate,outName.c_str());
}