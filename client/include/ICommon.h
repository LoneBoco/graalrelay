#ifndef ICOMMON_H
#define ICOMMON_H

#if defined(_WIN32) || defined(_WIN64)
	#ifndef WIN32_LEAN_AND_MEAN
		#define WIN32_LEAN_AND_MEAN
	#endif

	#ifndef WINVER
		#define WINVER 0x0501
	#endif
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <windows.h>
#endif

#include "main.h"
#undef wait

// Relay version.
#define RELAY_VERSION		"0.2.9"

// Uncomment to disable fingerprinting.
//#define DISABLE_FINGERPRINT

#endif
