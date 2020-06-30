#ifndef TCONNECTION_H
#define TCONNECTION_H

#include <time.h>
#include <vector>
#include <set>
#include "ICommon.h"
#include "IEnums.h"
#include "IUtil.h"
#include "CSocket.h"
#include "CEncryption.h"

class TConnection;
class TConnectionStub : public CSocketStub
{
	public:
		TConnectionStub(TConnection* pConnection, CSocket* pSocket)
			: connection(pConnection), sock(pSocket)
		{}

		// Required by CSocketStub.
		bool onRecv()
		{
			if (connection != 0 && _onRecv != 0)
				return (connection->*_onRecv)();
			return true;
		}

		bool onSend()
		{
			if (connection != 0 && _onSend != 0)
				return (connection->*_onSend)();
			return true;
		}

		bool onRegister()			{ return true; }

		void onUnregister()
		{
			if (connection != 0 && _onUnregister != 0)
				return (connection->*_onUnregister)();
		}

		SOCKET getSocketHandle()	{ return sock->getHandle(); }

		bool canRecv()
		{
			if (connection != 0 && _canRecv != 0)
				return (connection->*_canRecv)();
			return false;
		}

		bool canSend()
		{
			if (connection != 0 && _canSend != 0)
				return (connection->*_canSend)();
			return false;
		}

		void register_onRecv(bool (TConnection::*func)())	{ _onRecv = func; }
		void register_onSend(bool (TConnection::*func)())	{ _onSend = func; }
		void register_onUnregister(void (TConnection::*func)())	{ _onUnregister = func; }
		void register_canRecv(bool (TConnection::*func)())	{ _canRecv = func; }
		void register_canSend(bool (TConnection::*func)())	{ _canSend = func; }

		CSocket* sock;

	protected:
		TConnection* connection;
		bool (TConnection::*_onRecv)();
		bool (TConnection::*_onSend)();
		void (TConnection::*_onUnregister)();
		bool (TConnection::*_canRecv)();
		bool (TConnection::*_canSend)();
};

class TConnection
{
	public:
		// Required by CSocketStub.
		bool client_onRecv();
		bool client_onSend();
		void client_onUnregister();
		bool client_canRecv();
		bool client_canSend();

		bool server_onRecv();
		bool server_onSend();
		void server_onUnregister();
		bool server_canRecv();
		bool server_canSend();

		// Constructor - Deconstructor
		TConnection(CSocketManager* manager, CSocket* pSocket, CString remote_ip, CString remote_port);
		~TConnection();
		void operator()();

		// Manage Account
		bool sendLogin();

		// Socket-Functions
		bool client_doMain();
		bool server_doMain();
		void client_sendPacket(CString pPacket, bool appendNL = true);
		void server_sendPacket(CString pPacket, bool appendNL = true);

		// Misc functions.
		bool is_connected();
		void disconnect();
		bool isRC()				{ return (type & PLTYPE_ANYRC) ? true : false; }
		bool isNPCServer()		{ return (type & PLTYPE_NPCSERVER) ? true : false; }
		bool isClient()			{ return (type & PLTYPE_ANYCLIENT) ? true : false; }
		
		// Packet-Functions
		static bool created;
		static void createFunctions();

		// Client packets.
		bool msgPLI_LOGIN(CString& pPacket);
		bool msgPLI_NULL(CString& pPacket);
		bool msgPLI_PLAYERPROPS(CString& pPacket);
		bool msgPLI_PACKETCOUNT(CString& pPacket);
		bool msgPLI_LANGUAGE(CString& pPacket);
		bool msgPLI_TRIGGERACTION(CString& pPacket);
		bool msgPLI_RAWDATA(CString& pPacket);
		bool msgPLI_UPDATESCRIPT(CString& pPacket);

		// Server packets.
		bool msgPLO_NULL(CString& pPacket);
		bool msgPLO_SIGNATURE(CString& pPacket);
		bool msgPLO_NPCWEAPONADD(CString& pPacket);
		bool msgPLO_SERVERTEXT(CString& pPacket);
		bool msgPLO_RAWDATA(CString& pPacket);
		bool msgPLO_BOARDPACKET(CString& pPacket);
		bool msgPLO_FILE(CString& pPacket);
		bool msgPLO_NPCBYTECODE(CString& pPacket);
		bool msgPLO_UNKNOWN134(CString& pPacket);
		bool msgPLO_NPCWEAPONSCRIPT(CString& pPacket);
		bool msgPLO_SERVERWARP(CString& pPacket);
		bool msgPLO_FULLSTOP(CString& pPacket);

	private:
		// Helper functions.
		CString generate_md5(unsigned int seed);

		// Packet functions.
		bool client_parsePacket(CString& pPacket);
		void client_decryptPacket(CString& pPacket);
		bool server_parsePacket(CString& pPacket);
		void server_decryptPacket(CString& pPacket);

		// Client login flag.
		bool client_login;

		// Connection.
		CString server_ip;
		CString server_port;
		CString server_name;
		CSocketManager* socket_manager;
		TConnectionStub* client_sock;
		TConnectionStub* server_sock;

		// Socket Variables
		CString client_rBuffer;
		CString client_sBuffer;
		CString client_oBuffer;
		CString server_rBuffer;
		CString server_sBuffer;
		CString server_oBuffer;

		// Encryption
		unsigned char client_key;
		unsigned char server_key;
		CEncryption client_in_codec;
		CEncryption client_out_codec;
		CEncryption server_in_codec;
		CEncryption server_out_codec;

		// Raw packet variables.
		bool client_nextIsRaw;
		int client_rawPacketSize;
		bool server_nextIsRaw;
		int server_rawPacketSize;
		bool client_was_raw_data;
		bool server_was_raw_data;
		int number_of_raw_packets;	// Used with packet dumping.

		// Packet count variables.
		int client_packetCount;
		int server_packetCount;

		// Injected weapons to block.
		std::set<CString> injected_weapons;
		void inject_weapon(const CString& weapon, CString image);
		void inject_weapon_gs1(const CString& weapon);

		// Serverwarp blocking bypass via the /serverwarp command.
		bool manual_serverwarp;

		// Props management.
		CString setProps(CString& pPacket);
		float prop_x;
		float prop_y;
		unsigned short prop_id;
		
		// Other.
		int type;
};

#endif // TCONNECTION_H
