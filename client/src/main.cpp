#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <map>
#include <string>
#include <fstream>
#include "ICommon.h"
#include "main.h"
#include "CLog.h"
#include "CSocket.h"
#include "CProgramSettings.h"
#include "TConnection.h"
#include "md5.h"

// Linux specific stuff.
#if !(defined(_WIN32) || defined(_WIN64))
	#ifndef SIGBREAK
		#define SIGBREAK SIGQUIT
	#endif
#endif

// Directory stuff.
#if defined(_WIN32) || defined(_WIN64)
	#include <direct.h>
	#define mkdir _mkdir
	#define rmdir _rmdir
#else
	#include <sys/stat.h>
	#include <unistd.h>
#endif

// Function pointer for signal handling.
typedef void (*sighandler_t)(int);

// Home path of the gserver.
CString homepath;
static void getBasePath();

// Log.
CLog console;
CLog packet_dump;

// Stuff.
bool running;
CProgramSettings settings;
CSocket sock_listen;
CSocketManager sockets;
std::map<CString, CString> chat_commands;

// 
CString remote_ip;
CString remote_port;
CString remote_server_name;

///
void create_packet_logging_folder(const CString& server_ip, const CString& server_port);

///

int main(int argc, char* argv[])
{
	// Shut down the server if we get a kill signal.
	signal(SIGINT, (sighandler_t) shutdownServer);
	signal(SIGTERM, (sighandler_t) shutdownServer);
	signal(SIGBREAK, (sighandler_t) shutdownServer);
	signal(SIGABRT, (sighandler_t) shutdownServer);

	// Grab the base path to the server executable.
	getBasePath();

	// Configure our logger.
	console.setFilename("consolelog.txt");
	console.setEnabled(true);

	// Program announcements.
	console.out("\n"
		"--------------------------------------------------------------------------------\n"
		"Graal Relay server version %s.\n"
		"Programmed by Nalin.\n"
#ifndef DISABLE_FINGERPRINT
		"Do not bother using the relay for hacking purposes.  It can be traced."
#endif
		"\n", RELAY_VERSION);

	// Load the settings.
	console.out(":: Loading settings... ");
	settings.load_from_file(homepath + "settings.ini");
	console.out("done.\n");

	// Make sure our connection section exists!
	if (settings.get("Connection") == 0)
	{
		console.out("** Could not find the [Connection] category in settings.ini!  Aborting.\n");
		return 0;
	}

	// Load the default chat commands.
	chat_commands["SERVERWARP"] = "/serverwarp ";
	chat_commands["INJECTWEAPON"] = "/iw ";
	chat_commands["INJECTFLAG"] = "/if ";
	chat_commands["INJECTFLAGCLIENT"] = "/if_client ";
	chat_commands["INJECTTRIGGERACTION"] = "/it ";
	chat_commands["INJECTTRIGGERSERVER"] = "/it_serverside ";
	chat_commands["GANI"] = "/gani ";
	chat_commands["INJECTWEAPONGS1"] = "/iwgs1 ";

	// Load chat commands from file.
	console.out(":: Loading chat commands... ");
	{
		std::ifstream input((homepath + "chat_commands.cfg").text(), std::ios_base::in | std::ios_base::binary);
		if (input.is_open())
		{
			while (input.good())
			{
				// Read the string in.
				std::string sline;
				std::getline(input, sline);
				CString line = sline.c_str();

				// Find where the key ends.
				int loc = -1;
				loc = line.find(" ");
				if (loc == -1) loc = line.find("\t");
				if (loc == -1)
					continue;

				// Grab the key and value.
				CString key = line.subString(0, loc);
				CString value = line.subString(loc);

				// Trim.
				key.trimI();
				value.trimI();

				// Save.
				chat_commands[key] = value + " ";
			}
			input.close();
		}
	}

	// Inform of some enabled settings.
	if (settings.get_bool("Debug", "dump_weapons"))
		console.out(":: Weapon dumping enabled.\n");
	if (settings.get_bool("Debug", "dump_packets"))
		console.out(":: Packet dumping enabled.\n");

	// Save the remote ip and port.
	remote_ip = settings.get("Connection", "remote_ip");
	remote_port = settings.get("Connection", "remote_port");
	remote_server_name = settings.get("Connection", "initial_login_server");
	if (remote_server_name.isEmpty())
		remote_server_name = "Login";

	// Set up our listening socket.
	CString local_ip = settings.get("Connection", "local_ip");
	sock_listen.setDescription("listen");
	sock_listen.setType(SOCKET_TYPE_SERVER);
	sock_listen.init((local_ip.isEmpty() ? 0 : local_ip.text()), settings.get("Connection", "relay_port").text());
	sock_listen.connect();

	running = true;
	console.out("\n");

	while (running)
	{
		// Wait for a connection now.
		console.out(":: Waiting for client connection... ");
		CSocket* newsock = 0;
		while (newsock == 0 && running)
		{
			newsock = sock_listen.accept();
			sleep(50);
		}
		if (!running) return 0;
		console.out("connected!\n");
		console.out(":: Logging in to %s.\n\n", remote_server_name.text());

		// Create our packet logging folder if we are logging packets.
		if (settings.get_bool("Debug", "dump_packets"))
			create_packet_logging_folder(remote_ip, remote_port);

		// Create our new connection.
		TConnection* conn = new TConnection(&sockets, newsock, remote_ip, remote_port);
		if (!conn) return 0;

		// Get the remote IP and port again.
		remote_ip = settings.get("Connection", "remote_ip");
		remote_port = settings.get("Connection", "remote_port");
		remote_server_name = settings.get("Connection", "initial_login_server");
		if (remote_server_name.isEmpty())
			remote_server_name = "Login";

		// Run.
		while (running && conn->is_connected())
		{
			sockets.update(0, 5000);
		}

		console.out(":: Connection terminated.\n");

		delete conn;
	}

	return 0;
}

///

void create_packet_logging_folder(const CString& server_ip, const CString& server_port)
{
	// If we are logging packets, make sure our server directory exists.
#if defined(_WIN32) || defined(_WIN64)
	mkdir((getHomePath() + "packet_dump/").text());
	mkdir((getHomePath() + "packet_dump/" + server_ip + "_" + server_port + "/").text());
#else
	mkdir((getHomePath() + "packet_dump/").text(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	mkdir((getHomePath() + "packet_dump/" + server_ip + "_" + server_port + "/").text(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif

	// Set the log folder location.
	packet_dump.setFilename("packet_dump/" + server_ip + "_" + server_port + "/packets.txt");
	packet_dump.setEnabled(true);
	packet_dump.setWriteToConsole(false);
	packet_dump.clear();

	// Inform.
	console.out(":: Dumping packets to: packet_dump/" + server_ip + "_" + server_port + "/\n");
}

///

const CString getHomePath()
{
	return homepath;
}

void shutdownServer(int signal)
{
	console.out("\n:: Server is shutting down...\n");
	running = false;
}

void getBasePath()
{
	#if defined(_WIN32) || defined(_WIN64)
	// Get the path.
	char path[ MAX_PATH ];
	GetModuleFileNameA(0, path, MAX_PATH);

	// Find the program exe and remove it from the path.
	// Assign the path to homepath.
	homepath = path;
	int pos = homepath.findl('\\');
	if (pos == -1) homepath.clear();
	else if (pos != (homepath.length() - 1))
		homepath.removeI(++pos, homepath.length());
#else
	// Get the path to the program.
	char path[260];
	memset((void*)path, 0, 260);
	readlink("/proc/self/exe", path, sizeof(path));

	// Assign the path to homepath.
	char* end = strrchr(path, '/');
	if (end != 0)
	{
		end++;
		if (end != 0) *end = '\0';
		homepath = path;
	}
#endif
}
