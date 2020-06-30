Chat commands:

Warps to a new server.
/serverwarp servername

Injects a weapon to the client.  Use dump_weapons = true to get weapons to inject.
Dumped weapons get placed in the weapon_dump folder.  Move the weapon to the weapons folder to inject.
/iw weapon image

Injects a flag to the client and server.
/if flagname=value

Injects a flag only to the client.
/if_client flagname=value

Injects a triggeraction to the server.
Can use playerx and playery for location.  No math allowed.
/it x y data

Injects a special "serverside" triggeraction.
/it_serverside data

Modifies the player's current gani.
/gani data

Injects a GS1 weapon.
/iwgs1 weapon


//****************************************************************
//   Initial setup.
//****************************************************************

1) Open up Notepad as an administrator.  On Vista/7, you do this by right-clicking notepad
   and selecting "Run as Administrator".
2) File/Open.  Navigate to: C:\Windows\System32\drivers\etc\
3) Open the hosts file.  You will need to switch it from "Text Documents (*.txt)" to "All Files (*.*)"
4) Add this line: 127.0.0.1    loginserver.graalonline.com
5) Save the file.  If you don't want to use the relay, you must remove that line the
   next time you start Graal.


//****************************************************************
//   settings.ini
//****************************************************************

[Connection]

// The local IP address to bind to.
local_ip = localhost

// The relay IP and port.  This is used so the relay can redirect
// the client properly when a serverwarp packet is received.
// If relay_ip is auto, then local_ip is used.
relay_ip = auto
relay_port = 14900

// The remote server to connect to.
remote_ip = loginserver2.graalonline.com
remote_port = 14900

// The relay won't know the initial login server.  This tells the relay what it is
// so it can properly load the configuration for that server.
initial_login_server = Login


[Client]
// Determines what subscription level the client thinks it has.
// Possible values: gold,classic,playerworlds,guest
subscription = gold,classic

// For all of these, a blank value results in the relay passing what the client sent.
// Known platforms: win, iphone, flash, android
version = 
platform = 
unknown = 
md5_1 = 
md5_2 = 
os = 

[Seed]
// If these are given values, the corresponding settings in [Client] are randomized.
// To randomize your pc-id, give seeds to the md5's.  Seeds are numbers.
// If $rand is specified, a random seed will be generated.
unknown = 
md5_1 = 
md5_2 = 

[ClientProps]
// Currently known values: wind (Windows), iphone (iPhone)
ostype = wind

[Debug]
dump_weapons = false  // Dumps weapons.  Weapons can then be injected.
dump_packets = false  // Dumps packets.

[Server: Login]
language = en
block_weapons = true
inject_weapons = -Serverlist, -Playerlist
login_flags = "client.awesome=1","client.flookie=hello"

[Server: Zodiac]
blocked_weapons = +Trial, Typewriter
block_serverwarp = true
block_fullstop = true
block_triggerserver = "npc,triggerdata"
