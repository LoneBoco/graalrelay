#include <time.h>
#include <math.h>
#include "ICommon.h"
#include "IEnums.h"
#include "IPlayerProps.h"
#include "CLog.h"
#include "CSocket.h"
#include "CEncryption.h"
#include "TConnection.h"
#include "CProgramSettings.h"
#include "md5.h"

#if defined(_WIN32) || defined(_WIN64)
	#include <direct.h>
	#define mkdir _mkdir
	#define rmdir _rmdir
#else
	#include <sys/stat.h>
	#include <unistd.h>
#endif

// Extern from main.cpp.
extern CLog console;
extern CLog packet_dump;
extern CProgramSettings settings;
extern std::map<CString, CString> chat_commands;
extern CString remote_ip;
extern CString remote_port;
extern CString remote_server_name;

// Packet functions.
bool TConnection::created = false;
typedef bool (TConnection::*TCSock)(CString&);
std::vector<TCSock> TCFunc(256, &TConnection::msgPLI_NULL);
std::vector<TCSock> TSFunc(256, &TConnection::msgPLO_NULL);


void TConnection::createFunctions()
{
	if (TConnection::created)
		return;

	// Client functions.
	//TCFunc[PLI_LOGIN] = &TConnection::msgCL_LOGIN;
	TCFunc[PLI_PLAYERPROPS] = &TConnection::msgPLI_PLAYERPROPS;
	TCFunc[PLI_PACKETCOUNT] = &TConnection::msgPLI_PACKETCOUNT;
	TCFunc[PLI_LANGUAGE] = &TConnection::msgPLI_LANGUAGE;
	TCFunc[PLI_TRIGGERACTION] = &TConnection::msgPLI_TRIGGERACTION;
	TCFunc[PLI_RAWDATA] = &TConnection::msgPLI_RAWDATA;
	TCFunc[PLI_UPDATESCRIPT] = &TConnection::msgPLI_UPDATESCRIPT;

	// Server functions.
	TSFunc[PLO_SIGNATURE] = &TConnection::msgPLO_SIGNATURE;
	TSFunc[PLO_NPCWEAPONADD] = &TConnection::msgPLO_NPCWEAPONADD;
	TSFunc[PLO_SERVERTEXT] = &TConnection::msgPLO_SERVERTEXT;
	TSFunc[PLO_RAWDATA] = &TConnection::msgPLO_RAWDATA;
	TSFunc[PLO_BOARDPACKET] = &TConnection::msgPLO_BOARDPACKET;
	TSFunc[PLO_FILE] = &TConnection::msgPLO_FILE;
	TSFunc[PLO_NPCBYTECODE] = &TConnection::msgPLO_NPCBYTECODE;
	TSFunc[PLO_UNKNOWN134] = &TConnection::msgPLO_UNKNOWN134;
	TSFunc[PLO_NPCWEAPONSCRIPT] = &TConnection::msgPLO_NPCWEAPONSCRIPT;
	TSFunc[PLO_SERVERWARP] = &TConnection::msgPLO_SERVERWARP;
	TSFunc[PLO_FULLSTOP] = &TConnection::msgPLO_FULLSTOP;

	// Finished
	TConnection::created = true;
}


// Constructor
TConnection::TConnection(CSocketManager* manager, CSocket* pSocket, CString remote_ip, CString remote_port)
: client_login(false),
server_ip(remote_ip), server_port(remote_port), socket_manager(manager), client_sock(0), server_sock(0),
client_key(0), server_key(0),
client_nextIsRaw(false), client_rawPacketSize(0),
server_nextIsRaw(false), server_rawPacketSize(0),
client_was_raw_data(false), server_was_raw_data(false), number_of_raw_packets(0),
client_packetCount(0), server_packetCount(0),
manual_serverwarp(false)
{
	srand((unsigned int)time(0));

	// Create Functions
	if (!TConnection::created)
		TConnection::createFunctions();

	// Create our stubs.
	client_sock = new TConnectionStub(this, pSocket);
	CSocket* sock = new CSocket();
	sock->init(remote_ip.text(), remote_port.text());
	sock->connect();
	server_sock = new TConnectionStub(this, sock);
	
	// Bind our stubs.
	client_sock->register_canRecv(&TConnection::client_canRecv);
	client_sock->register_canSend(&TConnection::client_canSend);
	client_sock->register_onRecv(&TConnection::client_onRecv);
	client_sock->register_onSend(&TConnection::client_onSend);
	client_sock->register_onUnregister(&TConnection::client_onUnregister);
	server_sock->register_canRecv(&TConnection::server_canRecv);
	server_sock->register_canSend(&TConnection::server_canSend);
	server_sock->register_onRecv(&TConnection::server_onRecv);
	server_sock->register_onSend(&TConnection::server_onSend);
	server_sock->register_onUnregister(&TConnection::server_onUnregister);

	// Register them now.
	socket_manager->registerSocket(client_sock);
	socket_manager->registerSocket(server_sock);

	// Save the server name.
	server_name = remote_server_name;

	// Inform.
	if (settings.get_bool("Debug", "dump_weapons"))
		console.out(":: Dumping weapons to: weapon_dump/" + server_ip + "_" + server_port + "/\n");
}

TConnection::~TConnection()
{
	client_sock->sock->disconnect();
	server_sock->sock->disconnect();
	socket_manager->unregisterSocket(client_sock);
	socket_manager->unregisterSocket(server_sock);
	delete client_sock;
	delete server_sock;
}

bool TConnection::client_onRecv()
{
	// If our socket is gone, delete ourself.
	if (client_sock->sock == 0 || client_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// Grab the data from the socket and put it into our receive buffer.
	unsigned int size = 0;
	char* data = client_sock->sock->getData(&size);
	if (size != 0)
		client_rBuffer.write(data, size);
	else if (client_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// Do the main function.
	if (client_doMain() == false)
		return false;

	return true;
}

bool TConnection::client_onSend()
{
	if (client_sock->sock == 0 || client_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// compress buffer
	switch (client_out_codec.getGen())
	{
		case ENCRYPT_GEN_1:
		case ENCRYPT_GEN_2:
			printf("** Generations 1 and 2 are not supported!\n");
			break;

		case ENCRYPT_GEN_3:
		{
			// Compress the packet.
			client_sBuffer.zcompressI();

			// Sanity check.
			if (client_sBuffer.length() > 0xFFFD)
			{
				printf("** [ERROR] Trying to send a GEN_3 packet over 65533 bytes!  Tossing data.\n");
				return true;
			}

			// Add the packet to the out buffer.
			CString data = CString() << (short)client_sBuffer.length() << client_sBuffer;
			client_oBuffer << data;
			unsigned int dsize = client_oBuffer.length();
			client_oBuffer.removeI(0, client_sock->sock->sendData(client_oBuffer.text(), &dsize));
			break;
		}

		case ENCRYPT_GEN_4:
		{
			client_sBuffer.bzcompressI();

			// Sanity check.
			if (client_sBuffer.length() > 0xFFFD)
			{
				printf("** [ERROR] Trying to send a GEN_4 packet over 65533 bytes!  Tossing data.\n");
				return true;
			}

			// Encrypt the packet and add it to the out buffer.
			client_out_codec.limitFromType(COMPRESS_BZ2);
			client_sBuffer = client_out_codec.encrypt(client_sBuffer);
			CString data = CString() << (short)client_sBuffer.length() << client_sBuffer;
			client_oBuffer << data;
			unsigned int dsize = client_oBuffer.length();
			client_oBuffer.removeI(0, client_sock->sock->sendData(client_oBuffer.text(), &dsize));
			break;
		}

		case ENCRYPT_GEN_5:
		{
			//unsigned int oldSize = pSend.length();

			// Choose which compression to use and apply it.
			int compressionType = COMPRESS_UNCOMPRESSED;
			if (client_sBuffer.length() > 0x2000)	// 8KB
			{
				compressionType = COMPRESS_BZ2;
				client_sBuffer.bzcompressI();
			}
			else if (client_sBuffer.length() > 55)
			{
				compressionType = COMPRESS_ZLIB;
				client_sBuffer.zcompressI();
			}

			// Sanity check.
			if (client_sBuffer.length() > 0xFFFC)
			{
				printf("** [ERROR] Trying to send a GEN_5 packet over 65532 bytes!  Tossing data.\n");
				return true;
			}

			// Encrypt the packet and add it to the out buffer.
			client_out_codec.limitFromType(compressionType);
			client_sBuffer = client_out_codec.encrypt(client_sBuffer);
			CString data = CString() << (short)(client_sBuffer.length() + 1) << (char)compressionType << client_sBuffer;
			client_oBuffer << data;
			unsigned int dsize = client_oBuffer.length();
			client_oBuffer.removeI(0, client_sock->sock->sendData(client_oBuffer.text(), &dsize));
			break;
		}
	}

	client_sBuffer.clear();

	return true;
}

bool TConnection::client_canRecv()
{
	if (client_sock->sock->getState() == SOCKET_STATE_DISCONNECTED) return false;
	return true;
}

bool TConnection::client_canSend()
{
	if (!client_sBuffer.isEmpty())
		return true;
	return false;
}

void TConnection::client_onUnregister()
{
}


bool TConnection::server_onRecv()
{
	// If our socket is gone, delete ourself.
	if (server_sock->sock == 0 || server_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// Grab the data from the socket and put it into our receive buffer.
	unsigned int size = 0;
	char* data = server_sock->sock->getData(&size);
	if (size != 0)
		server_rBuffer.write(data, size);
	else if (server_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	// Do the main function.
	if (server_doMain() == false)
		return false;

	return true;
}

bool TConnection::server_onSend()
{
	if (server_sock->sock == 0 || server_sock->sock->getState() == SOCKET_STATE_DISCONNECTED)
		return false;

	int compressionType = COMPRESS_UNCOMPRESSED;
	if (server_sBuffer.length() > 0x2000)	// 8KB
	{
		compressionType = COMPRESS_BZ2;
		server_sBuffer.bzcompressI();
	}
	else if (server_sBuffer.length() > 55)
	{
		compressionType = COMPRESS_ZLIB;
		server_sBuffer.zcompressI();
	}

	// Sanity check.
	if (server_sBuffer.length() > 0xFFFC)
		return false;

	// Encrypt the packet and add it to the out buffer.
	server_out_codec.limitFromType(compressionType);
	server_sBuffer = server_out_codec.encrypt(server_sBuffer);
	CString data = CString() << (short)(server_sBuffer.length() + 1) << (char)compressionType << server_sBuffer;
	server_oBuffer << data;
	unsigned int dsize = server_oBuffer.length();
	int size = server_sock->sock->sendData(server_oBuffer.text(), &dsize);
	server_oBuffer.removeI(0, size);
	server_sBuffer.clear();

	return true;
}

bool TConnection::server_canRecv()
{
	if (server_sock->sock->getState() == SOCKET_STATE_DISCONNECTED) return false;
	return true;
}

bool TConnection::server_canSend()
{
	if (!server_sBuffer.isEmpty())
		return true;
	return false;
}

void TConnection::server_onUnregister()
{

}

// Main functions.
bool TConnection::client_doMain()
{
	// definitions
	CString unBuffer;

	// parse data
	client_rBuffer.setRead(0);
	while (client_rBuffer.length() > 1)
	{
		// packet length
		unsigned short len = (unsigned short)client_rBuffer.readShort();
		if ((unsigned int)len > (unsigned int)client_rBuffer.length()-2)
			break;

		// get packet
		unBuffer = client_rBuffer.readChars(len);
		client_rBuffer.removeI(0, len+2);

		// decrypt packet
		switch (client_in_codec.getGen())
		{
			case ENCRYPT_GEN_1:		// Gen 1 is not encrypted or compressed.
				break;

			// Gen 2 and 3 are zlib compressed.  Gen 3 encrypts individual packets
			// Uncompress so we can properly decrypt later on.
			case ENCRYPT_GEN_2:
			case ENCRYPT_GEN_3:
				unBuffer.zuncompressI();
				break;

			// Gen 4 and up encrypt the whole combined and compressed packet.
			// Decrypt and decompress.
			default:
				client_decryptPacket(unBuffer);
				break;
		}

		// well theres your buffer
		if (!client_parsePacket(unBuffer))
			return false;
	}
	return true;
}

bool TConnection::server_doMain()
{
	// definitions
	CString unBuffer;

	// parse data
	server_rBuffer.setRead(0);
	while (server_rBuffer.length() > 1)
	{
		// packet length
		unsigned short len = (unsigned short)server_rBuffer.readShort();
		if ((unsigned int)len > (unsigned int)server_rBuffer.length()-2)
			break;

		// get packet
		unBuffer = server_rBuffer.readChars(len);
		server_rBuffer.removeI(0, len+2);

		// decrypt packet
		switch (server_in_codec.getGen())
		{
			case ENCRYPT_GEN_1:		// Gen 1 is not encrypted or compressed.
				break;

			// Gen 2 and 3 are zlib compressed.  Gen 3 encrypts individual packets
			// Uncompress so we can properly decrypt later on.
			case ENCRYPT_GEN_2:
			case ENCRYPT_GEN_3:
				unBuffer.zuncompressI();
				break;

			// Gen 4 and up encrypt the whole combined and compressed packet.
			// Decrypt and decompress.
			default:
				server_decryptPacket(unBuffer);
				break;
		}

		// well theres your buffer
		if (!server_parsePacket(unBuffer))
			return false;
	}
	return true;
}

bool TConnection::is_connected()
{
	bool client = (client_sock->sock->getState() == SOCKET_STATE_DISCONNECTED);
	bool server = (server_sock->sock->getState() == SOCKET_STATE_DISCONNECTED);
	if (client && server) console.out(":: Lost connection to both client and server.\n");
	else if (client) console.out(":: Lost connection to the client.\n");
	else if (server) console.out(":: Lost connection to the server.\n");

	if (client || server)
		return false;
	return true;
}

void TConnection::disconnect()
{
//	server->deletePlayer(this);
}

bool TConnection::client_parsePacket(CString& pPacket)
{
	// If we are dumping packets, inform that we are dumping client packets.
	if (settings.get_bool("Debug", "dump_packets"))
	{
		packet_dump.out("--- Client -> Server ------------\n");
		time_t raw_time;
		time(&raw_time);
		packet_dump.out(": %s\n", ctime(&raw_time));
	}

	// First packet is always unencrypted zlib.  Read it in a special way.
	if (!client_login)
	{
		client_login = true;
		if (msgPLI_LOGIN(CString() << pPacket.readString("\n")) == false)
			return false;
	}

	while (pPacket.bytesLeft() > 0)
	{
		client_was_raw_data = false;

		// Grab a packet out of the input stream.
		CString curPacket;
		if (client_nextIsRaw)
		{
			client_was_raw_data = true;
			client_nextIsRaw = false;
			curPacket = pPacket.readChars(client_rawPacketSize);
		}
		else curPacket = pPacket.readString("\n");

		// Generation 3 encrypts individual packets so decrypt it now.
		if (client_in_codec.getGen() == ENCRYPT_GEN_3)
			client_decryptPacket(curPacket);

		// Get the packet id.
		unsigned char id = curPacket.readGUChar();

		// If we are logging packets, log it now.
		if (settings.get_bool("Debug", "dump_packets"))
		{
			if (client_was_raw_data)
			{
				curPacket.save(getHomePath() + "packet_dump/" + server_ip + "_" + server_port + "/raw_packet_" + CString((int)number_of_raw_packets) + ".txt");
				packet_dump.out("[%u] Saving raw data to raw_packet_%d.txt\n\n", id, number_of_raw_packets++);
			}
			else
			{
				CString p(curPacket);
				if (p[p.length() - 1] == '\n')
					p.removeI(p.length() - 1);

				packet_dump.out("[%u] %s\n", id, p.text());
				for (int i = 0; i < p.length(); ++i)
					packet_dump.out("%x ", (unsigned int)((unsigned char)p[i]));
				packet_dump.out("\n\n");
			}
		}

		// Call the function assigned to the packet id.
		if ((*this.*TCFunc[id])(curPacket))
			++client_packetCount;
	}

	return true;
}

bool TConnection::server_parsePacket(CString& pPacket)
{
	// If we are dumping packets, inform that we are dumping client packets.
	if (settings.get_bool("Debug", "dump_packets"))
	{
		packet_dump.out("--- Server -> Client ------------\n");
		time_t raw_time;
		time(&raw_time);
		packet_dump.out(": %s\n", ctime(&raw_time));
	}

	while (pPacket.bytesLeft() > 0)
	{
		server_was_raw_data = false;

		// Grab a packet out of the input stream.
		CString curPacket;
		if (server_nextIsRaw)
		{
			server_was_raw_data = true;
			server_nextIsRaw = false;
			curPacket = pPacket.readChars(server_rawPacketSize);
		}
		else curPacket = pPacket.readString("\n");

		// Get the packet id.
		unsigned char id = curPacket.readGUChar();

		// If we are logging packets, log it now.
		if (settings.get_bool("Debug", "dump_packets"))
		{
			if (server_was_raw_data)
			{
				curPacket.save(getHomePath() + "packet_dump/" + server_ip + "_" + server_port + "/raw_packet_" + CString((int)number_of_raw_packets) + ".txt");
				packet_dump.out("[%u] Saving raw data to raw_packet_%d.txt\n\n", id, number_of_raw_packets++);
			}
			else
			{
				CString p(curPacket);
				if (p[p.length() - 1] == '\n')
					p.removeI(p.length() - 1);

				packet_dump.out("[%u] %s\n", id, p.text());
				for (int i = 0; i < p.length(); ++i)
					packet_dump.out("%x ", (unsigned int)((unsigned char)p[i]));
				packet_dump.out("\n\n");
			}
		}

		// Call the function assigned to the packet id.
		if (!(*this.*TSFunc[id])(curPacket))
			return false;
	}

	return true;
}

void TConnection::client_decryptPacket(CString& pPacket)
{
	// Version 1.41 - 2.18 encryption
	// Was already decompressed so just decrypt the packet.
	if (client_in_codec.getGen() == ENCRYPT_GEN_3)
	{
		if (!isClient())
			return;

		client_in_codec.decrypt(pPacket);
	}

	// Version 2.19+ encryption.
	// Encryption happens before compression and depends on the compression used so
	// first decrypt and then decompress.
	if (client_in_codec.getGen() == ENCRYPT_GEN_4)
	{
		// Decrypt the packet.
		client_in_codec.limitFromType(COMPRESS_BZ2);
		client_in_codec.decrypt(pPacket);

		// Uncompress packet.
		pPacket.bzuncompressI();
	}
	else if (client_in_codec.getGen() >= ENCRYPT_GEN_5)
	{
		// Find the compression type and remove it.
		int pType = pPacket.readChar();
		pPacket.removeI(0, 1);

		// Decrypt the packet.
		client_in_codec.limitFromType(pType);		// Encryption is partially related to compression.
		client_in_codec.decrypt(pPacket);

		// Uncompress packet
		if (pType == COMPRESS_ZLIB)
			pPacket.zuncompressI();
		else if (pType == COMPRESS_BZ2)
			pPacket.bzuncompressI();
	}
}

void TConnection::server_decryptPacket(CString& pPacket)
{
	// Version 1.41 - 2.18 encryption
	// Was already decompressed so just decrypt the packet.
	if (server_in_codec.getGen() == ENCRYPT_GEN_3)
	{
		if (!isClient())
			return;

		server_in_codec.decrypt(pPacket);
	}

	// Version 2.19+ encryption.
	// Encryption happens before compression and depends on the compression used so
	// first decrypt and then decompress.
	if (server_in_codec.getGen() == ENCRYPT_GEN_4)
	{
		// Decrypt the packet.
		server_in_codec.limitFromType(COMPRESS_BZ2);
		server_in_codec.decrypt(pPacket);

		// Uncompress packet.
		pPacket.bzuncompressI();
	}
	else if (server_in_codec.getGen() >= ENCRYPT_GEN_5)
	{
		// Find the compression type and remove it.
		int pType = pPacket.readChar();
		pPacket.removeI(0, 1);

		// Decrypt the packet.
		server_in_codec.limitFromType(pType);		// Encryption is partially related to compression.
		server_in_codec.decrypt(pPacket);

		// Uncompress packet
		if (pType == COMPRESS_ZLIB)
			pPacket.zuncompressI();
		else if (pType == COMPRESS_BZ2)
			pPacket.bzuncompressI();
	}
}

void TConnection::client_sendPacket(CString pPacket, bool appendNL)
{
	// Is Data?
	if (pPacket.isEmpty())
		return;

	// Add to buffer.
	client_sBuffer << pPacket;

	// Append '\n'.
	if (appendNL)
	{
		if (pPacket[pPacket.length()-1] != '\n')
			client_sBuffer << "\n";
	}
}

void TConnection::server_sendPacket(CString pPacket, bool appendNL)
{
	// Is Data?
	if (pPacket.isEmpty())
		return;

	// Increment the number of packets that have been sent to the server.
	++server_packetCount;

	// Add to buffer.
	server_sBuffer << pPacket;

	// Append '\n'.
	if (appendNL)
	{
		if (pPacket[pPacket.length()-1] != '\n')
			server_sBuffer << "\n";
	}
}


///
/// Packet functions
///
bool TConnection::msgPLI_LOGIN(CString& pPacket)
{
	bool dump_packets = settings.get_bool("Debug", "dump_packets");

	// Read Client-Type
	console.out(":: New client login:\t");
	type = (1 << pPacket.readGChar());
	switch (type)
	{
		case PLTYPE_CLIENT:
			console.out("Client\n");
			client_in_codec.setGen(ENCRYPT_GEN_3);
			break;
		case PLTYPE_RC:
			console.out("RC\n");
			client_in_codec.setGen(ENCRYPT_GEN_3);
			break;
		case PLTYPE_CLIENT2:
			console.out("New Client (2.19 - 2.21, 3 - 3.01)\n");
			client_in_codec.setGen(ENCRYPT_GEN_4);
			break;
		case PLTYPE_CLIENT3:
			console.out("New Client (2.22+)\n");
			client_in_codec.setGen(ENCRYPT_GEN_5);
			break;
		case PLTYPE_RC2:
			console.out("New RC (2.22+)\n");
			client_in_codec.setGen(ENCRYPT_GEN_5);
			break;
		default:
			console.out("Unknown (%d)\n", type);
			client_sendPacket(CString() >> (char)PLO_DISCMESSAGE << "Your client type is unknown.  Please inform the Graal Reborn staff.  Type: " << CString((int)type) << ".");
			return false;
			break;
	}

	// Set up the server codecs.
	server_in_codec.setGen(ENCRYPT_GEN_5);
	server_out_codec.setGen(ENCRYPT_GEN_5);

	// Get encryption key.
	// 2.19+ RC and any client should get the key.
	if (isClient() || (isRC() && client_in_codec.getGen() > ENCRYPT_GEN_3))
	{
		client_key = (unsigned char)pPacket.readGChar();
		client_in_codec.reset(client_key);
		client_out_codec.reset(client_key);

#ifndef DISABLE_FINGERPRINT
		// Always use the same key for fingerprinting.
		server_key = 0x55;
#else
		server_key = client_key;
#endif

		server_in_codec.reset(server_key);
		server_out_codec.reset(server_key);
	}

	// Set the output codec gen.
	client_out_codec.setGen(client_in_codec.getGen());

	// Read Client-Version
	CString version = pPacket.readChars(8);

	// Read Account & Password
	CString accountName = pPacket.readChars(pPacket.readGUChar());
	CString password = pPacket.readChars(pPacket.readGUChar());

	// Read the extra login details.
	CString extra = pPacket.readString("").guntokenizeI();
	CString platform = extra.readString("\n");
	CString unknown = extra.readString("\n");
	CString md5_1 = extra.readString("\n");
	CString md5_2 = extra.readString("\n");
	CString os = extra.readString("\n");

	//serverlog.out("Key: %d\n", key);
	console.out("   Version:\t%s (%s)\n", version.text(), getVersionString(version));
	console.out("   Account:\t%s\n", accountName.text());
	console.out("   Password:\t%s\n", password.text());
	console.out("   Platform:\t%s\n", platform.text());
	console.out("   Unknown:\t%s\n", unknown.text());
	console.out("   md5_1:\t%s\n", md5_1.text());
	console.out("   md5_2:\t%s\n", md5_2.text());
	console.out("   os:\t\t%s\n", os.text());

	// Load our settings.
	CString s_version = settings.get("Client", "version");
	CString s_platform = settings.get("Client", "platform");
	CString s_unknown = settings.get("Client", "unknown");
	CString s_md5_1 = settings.get("Client", "md5_1");
	CString s_md5_2 = settings.get("Client", "md5_2");
	CString s_os = settings.get("Client", "os");
	int s_seed_unknown = settings.get_int("Seed", "unknown");
	int s_seed_md5_1 = 0;
	int s_seed_md5_2 = 0;

	// If any of the settings were empty, use the value from the client.
	if (s_version.isEmpty()) s_version = version;
	if (s_platform.isEmpty()) s_platform = platform;
	if (s_unknown.isEmpty()) s_unknown = unknown;
	if (s_md5_1.isEmpty()) s_md5_1 = md5_1;
	if (s_md5_2.isEmpty()) s_md5_2 = md5_2;
	if (s_os.isEmpty()) s_os = os;

	// Check seeds.
	CProgramSettingsCategory* seed = settings.get("Seed");
	if (seed != nullptr)
	{
		srand((unsigned int)time(0));
		if (seed->get("unknown") == "$rand")
			s_seed_unknown = rand();
		else s_seed_unknown = seed->get_int("unknown");

		if (seed->get("md5_1") == "$rand")
			s_seed_md5_1 = rand();
		else s_seed_md5_1 = seed->get_int("md5_1");

		if (seed->get("md5_2") == "$rand")
			s_seed_md5_2 = rand();
		else s_seed_md5_2 = seed->get_int("md5_2");
	}

	// Verify valid MD5 hash.
	CString valid("0123456789abcdef");
	s_md5_1.toLowerI();
	s_md5_2.toLowerI();
	bool invalid_md5 = false;
	if (s_md5_1.length() != 32 || s_md5_2.length() != 32)
		invalid_md5 = true;
	else
	{
		if (!s_md5_1.onlyContains(valid) || !s_md5_2.onlyContains(valid))
			invalid_md5 = true;
	}
	if (invalid_md5)
	{
		srand((unsigned int)time(0));
		s_seed_md5_1 = rand();
		s_seed_md5_2 = rand();
		console.out("\n** Invalid MD5 detected.  Using random MD5 hash.\n");
	}

	// Generate seeds.
	if (s_seed_unknown > 0)
		s_unknown = generate_md5(s_seed_unknown);
	if (s_seed_md5_1 > 0)
		s_md5_1 = generate_md5(s_seed_md5_1);
	if (s_seed_md5_2 > 0)
		s_md5_2 = generate_md5(s_seed_md5_2);

	// Log.
	console.out("\n:: Sending to server:\n");
	console.out("   Version:\t%s (%s)\n", s_version.text(), getVersionString(s_version));
	console.out("   Account:\t%s\n", accountName.text());
	console.out("   Password:\t%s\n", password.text());
	console.out("   Platform:\t%s\n", s_platform.text());
	console.out("   unknown: %s\n", s_unknown.text());
	console.out("   md5_1: %s\n", s_md5_1.text());
	console.out("   md5_2: %s\n", s_md5_2.text());
	console.out("   os: %s\n\n", s_os.text());

	// Packet dump.
	if (dump_packets)
	{
		packet_dump.out("Login information:\n");
		packet_dump.out("   Version: %s (%s)\n", s_version.text(), getVersionString(s_version));
		packet_dump.out("   Account: %s\n", accountName.text());
		packet_dump.out("   Password: %s\n", password.text());
		packet_dump.out("   Platform: %s\n", s_platform.text());
		packet_dump.out("   unknown: %s\n", s_unknown.text());
		packet_dump.out("   md5_1: %s\n", s_md5_1.text());
		packet_dump.out("   md5_2: %s\n", s_md5_2.text());
		packet_dump.out("   os: %s\n\n", s_os.text());
	}

	// Construct the new login packet.
	CString newLogin;
	newLogin >> (char)5 >> (char)server_key;
	newLogin << s_version;
	newLogin >> (char)accountName.length() << accountName >> (char)password.length() << password;
	newLogin << s_platform.gtokenize() << ",";
	newLogin << s_unknown.gtokenize() << ",";
	newLogin << s_md5_1.gtokenize() << "," << s_md5_2.gtokenize() << ",";
	newLogin << s_os.gtokenize();
	newLogin << "\n";
	newLogin.zcompressI();

	// Send it to the server.
	newLogin = CString() << (short)newLogin.length() << newLogin;
	unsigned int s = newLogin.length();
	server_sock->sock->sendData(newLogin.text(), &s);

#ifndef DISABLE_FINGERPRINT
	// Send a blank PLI_PROCESSLIST packet for fingerprint purposes.
	server_sendPacket(CString() >> (char)PLI_PROCESSLIST);
#endif

	return true;
}

CString TConnection::generate_md5(unsigned int seed)
{
	srand(seed);

	// Generate an MD5.
	char md5[33];
	memset((void*)md5, 0, 33);
	int r = rand();
	CalculateMD5(reinterpret_cast<char*>(&r), sizeof(int), md5);
	
	return CString(md5);
}

bool TConnection::msgPLI_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	int id = pPacket.readGUChar();
	//console.out("[Client] id: %d\n", id);

	// Warn if we encounter a packet that is used with raw data.
	if (client_was_raw_data)
	{
		console.out(":: Unhandled client raw data.  Packet: %d\n", id);
		server_sendPacket(CString() >> (char)PLI_RAWDATA >> (int)pPacket.length());
		server_sendPacket(pPacket, false);
	}
	else server_sendPacket(pPacket);
	return true;
}

bool TConnection::msgPLI_PLAYERPROPS(CString& pPacket)
{
	unsigned char propId = pPacket.readGUChar();
	if (propId == PLPROP_CURCHAT)
	{
		CString message = pPacket.readChars(pPacket.readGUChar());
		message.trimI();

		// Server warp.
		if (message.find(chat_commands["SERVERWARP"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["SERVERWARP"]);
			CString server = message.readString("").trimI();
			if (!server.isEmpty())
			{
				manual_serverwarp = true;
				server_sendPacket(CString() >> (char)PLI_SERVERWARP << server);
			}
			return false;
		}

		// Inject weapon.
		if (message.find(chat_commands["INJECTWEAPON"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTWEAPON"]);
			CString weapon = message.readString(" ").trimI();
			CString image = message.readString("").trimI();

			inject_weapon(weapon, image);
			return false;
		}

		// Inject flag.
		if (message.find(chat_commands["INJECTFLAG"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTFLAG"]);
			CString flag = message.readString("").trimI();

			console.out(":: Injecting server-bound flag: %s\n", flag.text());
			client_sendPacket(CString() >> (char)PLO_FLAGSET << flag);
			server_sendPacket(CString() >> (char)PLI_FLAGSET << flag);
			return false;
		}

		// Inject client flag.
		if (message.find(chat_commands["INJECTFLAGCLIENT"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTFLAGCLIENT"]);
			CString flag = message.readString("").trimI();

			console.out(":: Injecting client-bound flag: %s\n", flag.text());
			client_sendPacket(CString() >> (char)PLO_FLAGSET << flag);
			return false;
		}

		// Inject triggeraction.
		if (message.find(chat_commands["INJECTTRIGGERACTION"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTTRIGGERACTION"]);
			CString x = message.readString(" ").trimI();
			CString y = message.readString(" ").trimI();
			CString data = message.readString("").trimI();
			float loc[2] = {strtofloat(x), strtofloat(y)};
			if (x == "playerx") loc[0] = prop_x;
			if (x == "playery") loc[0] = prop_y;
			if (y == "playerx") loc[1] = prop_x;
			if (y == "playery") loc[1] = prop_y;

			console.out(":: Injecting triggeraction: (%.2f, %.2f) %s\n", loc[0], loc[1], data.text());
			server_sendPacket(CString() >> (char)PLI_TRIGGERACTION >> (int)0 >> (char)(loc[0] * 2.0f) >> (char)(loc[1] * 2.0f) << data);
			return false;
		}

		// Inject serverside triggeraction.
		if (message.find(chat_commands["INJECTTRIGGERSERVER"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTTRIGGERSERVER"]);
			CString data = message.readString("").trimI();

			console.out(":: Injecting serverside triggeraction: %s\n", data.text());
			server_sendPacket(CString() >> (char)PLI_TRIGGERACTION >> (int)0 >> (char)0 >> (char)0 << "serverside," << data);
			return false;
		}

		// Modify the current gani.
		if (message.find(chat_commands["GANI"]) == 0)
		{
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["GANI"]);
			CString data = message.readString("").trimI();

			console.out(":: Injecting gani change: %s\n", data.text());
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_GANI >> (char)data.length() << data);
			server_sendPacket(CString() >> (char)PLI_PLAYERPROPS >> (char)PLPROP_GANI >> (char)data.length() << data);
			return false;
		}

		// Inject GS1 weapon.
		if (message.find(chat_commands["INJECTWEAPONGS1"]) == 0)
		{
			// Set the client's chat back to blank.
			client_sendPacket(CString() >> (char)PLO_PLAYERPROPS >> (char)PLPROP_CURCHAT >> (char)0);

			console.out(":: Intercepting command: %s\n", message.text());
			message.readString(chat_commands["INJECTWEAPONGS1"]);
			CString weapon = message.readString("").trimI();

			inject_weapon_gs1(weapon);
			return false;
		}
	}

	pPacket.setRead(1);
	server_sendPacket(setProps(pPacket));
	return true;
}

bool TConnection::msgPLI_PACKETCOUNT(CString& pPacket)
{
	unsigned short count = pPacket.readGUShort();
	
	console.out(":: Intercepting PLI_PACKETCOUNT: Client: %d, Relay client: %d, Relay server: %d\n", count, client_packetCount, server_packetCount);

	// Reset the packet count.
	server_sendPacket(CString() >> (char)PLI_PACKETCOUNT >> (short)server_packetCount);
	client_packetCount = 0;
	server_packetCount = 0;
	return false;
}

bool TConnection::msgPLI_LANGUAGE(CString& pPacket)
{
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		if (cat->exist("language"))
		{
			CString language = cat->get("language");
			server_sendPacket(CString() >> (char)PLI_LANGUAGE << language);
			console.out(":: Intercepting PLI_LANGUAGE: %s -> %s\n", pPacket.readString("").text(), language.text());
			return true;
		}
	}

	server_sendPacket(pPacket);

#ifndef DISABLE_FINGERPRINT
	// Send it twice for fingerprint purposes.
	server_sendPacket(pPacket);
#endif

	return true;
}

bool TConnection::msgPLI_TRIGGERACTION(CString& pPacket)
{
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		// See if we are blocking triggeractions.
		if (cat->exist("blocked_triggerserver"))
		{
			unsigned int npcId = pPacket.readGUInt();
			float loc[2] = {(float)pPacket.readGUChar() / 2.0f, (float)pPacket.readGUChar() / 2.0f};
			CString action = pPacket.readString("").trim();
			std::vector<CString> actions = action.guntokenize().tokenize("\n");

			// Check for a serverside action.
			if (npcId == 0 && loc[0] == 0.0f && loc[1] == 0.0f && actions.size() >= 2 && actions[0] == "serverside")
			{
				// Check all the blocked triggers for this one.
				std::vector<CString> block = cat->get("blocked_triggerserver").guntokenize().tokenize("\n");
				for (std::vector<CString>::iterator i = block.begin(); i != block.end(); ++i)
				{
					std::vector<CString> trigger = (*i).guntokenize().tokenize("\n");
					if (trigger.size() == 0 || trigger[0] != actions[1])
						continue;

					// Check if they are identical.
					bool same = true;
					for (unsigned int j = 1; j < trigger.size(); ++j)
					{
						if (trigger[j] != actions[1 + j])
						{
							same = false;
							break;
						}
					}

					// If they were identical, block!
					if (same)
					{
						console.out(":: Blocking triggerserver: %s.\n", (*i).text());
						return false;
					}
				}
			}
		}
	}

	server_sendPacket(pPacket);
	return true;
}

bool TConnection::msgPLI_RAWDATA(CString& pPacket)
{
	client_nextIsRaw = true;
	client_rawPacketSize = pPacket.readGUInt();
	return true;
}

bool TConnection::msgPLI_UPDATESCRIPT(CString& pPacket)
{
	CString script = pPacket.readString();

	// Determine if we should block this request.
	std::set<CString>::iterator i = injected_weapons.find(script);
	if (i != injected_weapons.end())
	{
		console.out(":: Blocking client request for script: %s\n", script.text());
		injected_weapons.erase(i);
		return false;
	}

	server_sendPacket(pPacket);
	return true;
}

/////////////////////////////////
/////////////////////////////////
/////////////////////////////////

bool TConnection::msgPLO_NULL(CString& pPacket)
{
	pPacket.setRead(0);
	int id = pPacket.readGUChar();
	//console.out("[Server] id: %d\n", id);

	// Warn if we encounter a packet that is used with raw data.
	if (server_was_raw_data)
	{
		console.out(":: Unhandled server raw data.  Packet: %d\n", id);
		client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
		client_sendPacket(pPacket, false);
	}
	else client_sendPacket(pPacket);
	return true;
}

bool TConnection::msgPLO_SIGNATURE(CString& pPacket)
{
	// Send the signature to the client.
	client_sendPacket(pPacket);

	// Inject weapons and flags on login.
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		std::vector<CString> block = cat->get("inject_weapons").guntokenize().tokenize("\n");
		for (std::vector<CString>::iterator i = block.begin(); i != block.end(); ++i)
		{
			CString weapon = i->trim();
			inject_weapon(weapon, "wbomb1.png");
		}

		std::vector<CString> flags = cat->get("login_flags").guntokenize().tokenize("\n");
		for (std::vector<CString>::iterator i = flags.begin(); i != flags.end(); ++i)
		{
			CString flag = i->trim();
			server_sendPacket(CString() >> (char)PLI_FLAGSET << flag);
			console.out(":: Injecting flag: %s\n", flag.text());
		}
	}

	return true;
}

bool TConnection::msgPLO_NPCWEAPONADD(CString& pPacket)
{
	CString name = pPacket.readChars(pPacket.readGUChar());

	// See if this weapon should be blocked.
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		// See if we should block all weapons.
		if (cat->get_bool("block_weapons"))
		{
			console.out(":: Blocking weapon: %s.\n", name.text());
			return true;
		}

		std::vector<CString> block = cat->get("blocked_weapons").guntokenize().tokenize("\n");
		for (std::vector<CString>::iterator i = block.begin(); i != block.end(); ++i)
		{
			(*i).trimI();
			if (name.match(*i))
			{
				console.out(":: Blocking weapon: %s.\n", name.text());
				return true;
			}
		}
	}

	// Send the weapon to the client.
	client_sendPacket(pPacket);

	return true;
}

bool TConnection::msgPLO_SERVERTEXT(CString& pPacket)
{
	CString packet = pPacket.readString("");
	CString data = packet.guntokenize();

	CString weapon = data.readString("\n");
	CString type = data.readString("\n");
	CString option = data.readString("\n");
	CString params = data.readString("");

	if (weapon == "-Serverlist")
	{
		if (type == "lister")
		{
			if (option == "subscriptions" || option == "subscriptions2")
			{
				// Get our subscription status.
				CString sub = settings.get("Client", "subscription");

				// If the option is not set, just pass what the server sent.
				if (sub.isEmpty())
				{
					client_sendPacket(pPacket);
					return true;
				}

				// If we want to be a guest, send a blank subscriptions2 packet.
				if (sub == "guest")
				{
					client_sendPacket(CString() >> (char)PLO_SERVERTEXT << "-Serverlist,lister,subscriptions2");
					return true;
				}

				// Start our subscription packet.
				CString p;
				p << "-Serverlist,lister,subscriptions,";

				// Split up our sub types!
				std::vector<CString> subtypes = sub.tokenize(",");
				CString p2;
				for (std::vector<CString>::iterator i = subtypes.begin(); i != subtypes.end(); ++i)
				{
					CString s = (*i).trim();
					CString s2;

					// Classic.
					if (s == "classic")
						s2 = "classic\nClassic Subscription\n\n";
					// Gold.
					else if (s == "gold")
						s2 = "global\nGold Subscription\n2678400\n";	// 31 days of gold time.
					// Developer gold.
					else if (s == "playerworlds")
						s2 = "playerworlds\nDeveloper Gold Subscription\n2678400\n";

					p2 << s2.gtokenize();
				}
				p << p2.gtokenize();

				client_sendPacket(CString() >> (char)PLO_SERVERTEXT << p);
				return true;
			}
		}
	}

	client_sendPacket(pPacket);
	return true;
}

bool TConnection::msgPLO_RAWDATA(CString& pPacket)
{
	server_nextIsRaw = true;
	server_rawPacketSize = pPacket.readGUInt();
	return true;
}

bool TConnection::msgPLO_BOARDPACKET(CString& pPacket)
{
	client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
	client_sendPacket(pPacket, false);
	return true;
}

bool TConnection::msgPLO_FILE(CString& pPacket)
{
	client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
	client_sendPacket(pPacket, false);
	return true;
}

bool TConnection::msgPLO_NPCBYTECODE(CString& pPacket)
{
	client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
	client_sendPacket(pPacket, false);
	return true;
}

bool TConnection::msgPLO_UNKNOWN134(CString& pPacket)
{
	client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
	client_sendPacket(pPacket, false);
	return true;
}

bool TConnection::msgPLO_NPCWEAPONSCRIPT(CString& pPacket)
{
	// Dump weapon script.
	if (settings.get_bool("Debug", "dump_weapons"))
	{
		// Grab the weapon header.
		CString header = pPacket.readChars(pPacket.readGUShort());
		header.guntokenizeI();

		// Pull out the weapon name.
		CString type = header.readString("\n");
		CString name = header.readString("\n");
		CString unknown = header.readString("\n");
		CString hash = header.readString("\n");

		// Save the weapon now.
		name.replaceAllI("\\", "_");
		name.replaceAllI("/", "_");
		name.replaceAllI("*", "@");
		name.replaceAllI(":", ";");
		name.replaceAllI("?", "!");

		// Make sure our weapon folder exists.
#if defined(_WIN32) || defined(_WIN64)
		mkdir((getHomePath() + "weapon_dump/").text());
		mkdir((getHomePath() + "weapon_dump/" + server_ip + "_" + server_port + "/").text());
#else
		mkdir((getHomePath() + "weapon_dump/").text(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		mkdir((getHomePath() + "weapon_dump/" + server_ip + "_" + server_port + "/").text(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

		// Save the weapon.
		pPacket.save(getHomePath() + "weapon_dump/" + server_ip + "_" + server_port + "/" + name + ".txt");
	}

	client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)pPacket.length());
	client_sendPacket(pPacket, false);
	return true;
}

bool TConnection::msgPLO_SERVERWARP(CString& pPacket)
{
	// Read the packet info.
	CString serverinfo = pPacket.guntokenize();
	CString id = serverinfo.readString("\n");
	CString name = serverinfo.readString("\n");
	CString ip = serverinfo.readString("\n");
	CString port = serverinfo.readString("\n");

	// See if we should block this serverwarp packet.
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		if (manual_serverwarp)
			console.out(":: Overriding blocked PLO_SERVERWARP.\n");
		else if (cat->get_bool("block_serverwarp"))
		{
			console.out(":: Blocking automatic PLO_SERVERWARP to %s.  %s:%s\n", name.trim().text(), ip.text(), port.text());
			return true;
		}
	}
	manual_serverwarp = false;

	// Save the ip and port so we know which server to connect to.
	remote_ip = ip;
	remote_port = port;
	remote_server_name = name.trim();

	// Modify the ip and port so the client connects to the relay again.
	ip = settings.get("Connection", "relay_ip");
	port = settings.get("Connection", "relay_port");

	// Check to see if we need to modify the IP now.
	if (ip.isEmpty() || ip == "auto")
		ip = settings.get("Connection", "local_ip");

	// Send the new serverwarp packet to the client.
	client_sendPacket(CString() >> (char)PLO_SERVERWARP << "\"" << id << "\",\"" << name << "\",\""
		<< ip << "\",\"" << port << "\"");

	console.out(":: Intercepting PLO_SERVERWARP to %s.  %s:%s -> %s:%s\n", remote_server_name.text(), remote_ip.text(), remote_port.text(), ip.text(), port.text());

	return true;
}

bool TConnection::msgPLO_FULLSTOP(CString& pPacket)
{
	CProgramSettingsCategory* cat = settings.get("Server: " + server_name);
	if (cat)
	{
		if (cat->get_bool("block_fullstop"))
		{
			console.out(":: Blocking PLO_FULLSTOP.\n");
			return true;
		}
	}

	client_sendPacket(pPacket);

	return true;
}

///

void TConnection::inject_weapon(const CString& weapon, CString image)
{
	if (weapon.isEmpty())
		return;

	// Check for a valid image.  If one doesn't exist, just use the bomb.
	if (image.isEmpty())
		image = "wbomb1.png";

	// Properly format the weapon for loading.
	CString weapon2 = weapon;
	weapon2.replaceAllI("\\", "_");
	weapon2.replaceAllI("/", "_");
	weapon2.replaceAllI("*", "@");
	weapon2.replaceAllI(":", ";");
	weapon2.replaceAllI("?", "!");

	CString file;
	file.load(getHomePath() + "weapons/" + weapon2 + ".txt");
	if (!file.isEmpty())
	{
		unsigned char id = file.readGUChar();
		CString header = file.readChars(file.readGUShort());
		CString header2 = header.guntokenize();

		CString type = header2.readString("\n");
		CString name = header2.readString("\n");
		CString unknown = header2.readString("\n");
		CString hash = header2.readString("\n");

		// Add the weapon to the set of injected weapons to block.
		injected_weapons.insert(name);

		// Send the weapon add packet to the client.
		client_sendPacket(CString() >> (char)PLO_NPCWEAPONADD
			>> (char)name.length() << name
			>> (char)0 >> (char)image.length() << image
			>> (char)74 >> (short)0);

		// Get the mod time and send packet 197.
		CString smod = CString() >> (long long)time(0);
		smod.gtokenizeI();
		client_sendPacket(CString() >> (char)PLO_UNKNOWN197 << header << "," << smod);

		// Send the weapon now.
		client_sendPacket(CString() >> (char)PLO_RAWDATA >> (int)file.length());
		client_sendPacket(CString() << file, false);

		console.out(":: Injecting weapon: %s (%s)\n", weapon.text(), image.text());
	}
}

void TConnection::inject_weapon_gs1(const CString& weapon)
{
	if (weapon.isEmpty())
		return;

	// Properly format the weapon for loading.
	CString weapon2 = weapon;
	weapon2.replaceAllI("\\", "_");
	weapon2.replaceAllI("/", "_");
	weapon2.replaceAllI("*", "@");
	weapon2.replaceAllI(":", ";");
	weapon2.replaceAllI("?", "!");

	CString file;
	file.load(getHomePath() + "weapons/" + weapon2 + ".txt");
	if (!file.isEmpty())
	{
		file.removeAllI("\r");

		// Grab some information.
		bool has_script = (file.find("SCRIPT") != -1 ? true : false);
		bool has_scriptend = (file.find("SCRIPTEND") != -1 ? true : false);
		bool found_scriptend = false;

		// Parse into lines.
		std::vector<CString> fileLines = file.tokenize("\n");
		if (fileLines.size() == 0 || fileLines[0].trim() != "GRAWP001")
		{
			console.out(":: ERROR: Weapon file %s is not a valid GS1 weapon.\n", weapon2.text());
			console.out("::        The weapon header was not found.\n");
			return;
		}

		// Definitions.
		CString weaponImage, weaponName, weaponScript;

		// Parse File
		std::vector<CString>::iterator i = fileLines.begin();
		while (i != fileLines.end())
		{
			// Find Command
			CString curCommand = i->readString();

			// Parse Line
			if (curCommand == "REALNAME")
				weaponName = i->readString("");
			else if (curCommand == "IMAGE")
				weaponImage = i->readString("");
			else if (curCommand == "SCRIPT")
			{
				++i;
				while (i != fileLines.end())
				{
					if (*i == "SCRIPTEND")
					{
						found_scriptend = true;
						break;
					}
					weaponScript << *i << "\xa7";
					++i;
				}
			}
			if (i != fileLines.end()) ++i;
		}

		// Valid Weapon Name?
		if (weaponName.isEmpty())
		{
			console.out(":: ERROR: Weapon %s is malformed.\n", weaponName.text());
			console.out("::        The weapon name was not found.\n");
			return;
		}

		// Check for a valid image.  If one doesn't exist, just use the bomb.
		if (weaponImage.isEmpty())
			weaponImage = "wbomb1.png";

		// Give a warning if our weapon was malformed.
		if (has_scriptend && !found_scriptend)
		{
			console.out(":: WARNING: Weapon %s is malformed.\n", weaponName.text());
			console.out("::          SCRIPTEND needs to be on its own line.\n");
		}

		// Add the weapon to the set of injected weapons to block.
		injected_weapons.insert(weaponName);

		// Send the weapon add packet to the client.
		client_sendPacket(CString() >> (char)PLO_NPCWEAPONADD
			>> (char)weaponName.length() << weaponName
			>> (char)0 >> (char)weaponImage.length() << weaponImage
			>> (char)1 >> (short)weaponScript.length() << weaponScript);

		console.out(":: Injecting weapon: %s (%s)\n", weaponName.text(), weaponImage.text());
	}
}
