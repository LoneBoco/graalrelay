#include "ICommon.h"
#include <vector>

#include "TConnection.h"
#include "IEnums.h"
#include "IPlayerProps.h"
#include "IUtil.h"
#include "CProgramSettings.h"
#include "CLog.h"

// Extern from main.cpp.
extern CLog console;
extern CLog packet_dump;
extern CProgramSettings settings;
extern CString remote_ip;
extern CString remote_port;
extern CString remote_server_name;


CString TConnection::setProps(CString& pPacket)
{
	CString out(pPacket.subString(0, pPacket.readPos()));
	CString attr;
	int len = 0;

	while (pPacket.bytesLeft() > 0)
	{
		unsigned char propId = pPacket.readGUChar();
		
		switch (propId)
		{
			case PLPROP_NICKNAME:
			{
				CString nick = pPacket.readChars(pPacket.readGUChar());
				out >> (char)propId >> (char)nick.length() << nick;
				break;
			}

			case PLPROP_MAXPOWER:
			{
				unsigned char maxPower = pPacket.readGUChar();
				out >> (char)propId >> (char)maxPower;
				break;
			}

			case PLPROP_CURPOWER:
			{
				float power = (float)pPacket.readGUChar() / 2.0f;
				out >> (char)propId >> (char)(power * 2.0f);
				break;
			}

			case PLPROP_RUPEESCOUNT:
			{
				unsigned int gralats = pPacket.readGUInt();
				out >> (char)propId >> (int)gralats;
				break;
			}

			case PLPROP_ARROWSCOUNT:
			{
				unsigned char arrows = pPacket.readGUChar();
				out >> (char)propId >> (char)arrows;
				break;
			}

			case PLPROP_BOMBSCOUNT:
			{
				unsigned char bombs = pPacket.readGUChar();
				out >> (char)propId >> (char)bombs;
				break;
			}

			case PLPROP_GLOVEPOWER:
			{
				unsigned char glovePower = pPacket.readGUChar();
				out >> (char)propId >> (char)glovePower;
				break;
			}

			case PLPROP_BOMBPOWER:
			{
				unsigned char bombPower = pPacket.readGUChar();
				out >> (char)propId >> (char)bombPower;
				break;
			}

			case PLPROP_SWORDPOWER:
			{
				CString swordImg;
				int sp = pPacket.readGUChar();
				out >> (char)propId >> (char)sp;
				if (sp <= 4)
					swordImg = CString() << "sword" << CString(sp) << ".png";
				else
				{
					sp -= 30;
					len = pPacket.readGUChar();
					if (len > 0)
						swordImg = pPacket.readChars(len);
					else swordImg = "";
					out >> (char)len << swordImg;
				}
				break;
			}

			case PLPROP_SHIELDPOWER:
			{
				CString shieldImg;
				int sp = pPacket.readGUChar();
				out >> (char)propId >> (char)sp;
				if (sp <= 3)
					shieldImg = CString() << "shield" << CString(sp) << ".png";
				else
				{
					// This fixes an odd bug with the 1.41 client.
					if (pPacket.bytesLeft() == 0) continue;

					sp -= 10;
					if (sp < 0) break;
					len = pPacket.readGUChar();
					if (len > 0)
						shieldImg = pPacket.readChars(len);
					else shieldImg = "";
					out >> (char)len << shieldImg;
				}
				break;
			}

			case PLPROP_GANI:
			{
				CString gani = pPacket.readChars(pPacket.readGUChar());
				out >> (char)propId >> (char)gani.length() << gani;
				break;
			}

			case PLPROP_HEADGIF:
			{
				CString headImg;
				len = pPacket.readGUChar();
				out >> (char)propId >> (char)len;
				if (len < 100)
				{
					headImg = CString() << "head" << CString(len) << ".png";
				}
				else if (len > 100)
				{
					headImg = pPacket.readChars(len-100);
					out << headImg;
				}
				break;
			}

			case PLPROP_CURCHAT:
			{
				CString chatMsg = pPacket.readChars(pPacket.readGUChar());
				out >> (char)propId >> (char)chatMsg.length() << chatMsg;
				break;
			}

			case PLPROP_COLORS:
			{
				char colors[5];
				for (unsigned int i = 0; i < sizeof(colors) / sizeof(unsigned char); ++i)
					colors[i] = pPacket.readGUChar();
				out >> (char)propId;
				for (unsigned int i = 0; i < sizeof(colors) / sizeof(unsigned char); ++i)
					out >> (char)colors[i];
				break;
			}

			case PLPROP_ID:
			{
				unsigned short id = pPacket.readGUShort();
				out >> (char)propId >> (short)id;
				prop_id = id;
				break;
			}

			case PLPROP_X:
			{
				float x = (float)(pPacket.readGUChar() / 2.0f);
				out >> (char)propId >> (char)(x * 2.0f);
				prop_x = x;
				break;
			}

			case PLPROP_Y:
			{
				float y = (float)(pPacket.readGUChar() / 2.0f);
				out >> (char)propId >> (char)(y * 2.0f);
				prop_y = y;
				break;
			}

			case PLPROP_Z:
			{
				float z = (float)pPacket.readGUChar();
				out >> (char)propId >> (char)z;
				break;
			}

			case PLPROP_SPRITE:
			{
				unsigned char sprite = pPacket.readGUChar();
				out >> (char)propId >> (char)sprite;
				break;
			}

			case PLPROP_STATUS:
			{
				unsigned char status = pPacket.readGUChar();
				out >> (char)propId >> (char)status;
				break;
			}

			case PLPROP_CARRYSPRITE:
			{
				unsigned char carrySprite = pPacket.readGUChar();
				out >> (char)propId >> (char)carrySprite;
				break;
			}

			case PLPROP_CURLEVEL:
			{
				len = pPacket.readGUChar();
				CString levelName = pPacket.readChars(len);
				out >> (char)propId >> (char)levelName.length() << levelName;
				break;
			}

			case PLPROP_HORSEGIF:
			{
				len = pPacket.readGUChar();
				CString horseImg = pPacket.readChars(len);
				out >> (char)propId >> (char)horseImg.length() << horseImg;
				break;
			}

			case PLPROP_HORSEBUSHES:
			{
				unsigned char horsebushes = pPacket.readGUChar();
				out >> (char)propId >> (char)horsebushes;
				break;
			}

			case PLPROP_EFFECTCOLORS:
			{
				len = pPacket.readGUChar();
				int colors = 0;
				if (len > 0) colors = pPacket.readGInt4();

				out >> (char)propId >> (char)len;
				if (len > 0) out.writeGInt4(colors);
				break;
			}

			case PLPROP_CARRYNPC:
			{
				unsigned int carryNpcId = pPacket.readGUInt();
				out >> (char)propId >> (int)carryNpcId;
				break;
			}

			case PLPROP_APCOUNTER:
			{
				unsigned short apCounter = pPacket.readGUShort();
				out >> (char)propId >> (short)apCounter;
				break;
			}

			case PLPROP_MAGICPOINTS:
			{
				unsigned char mp = pPacket.readGUChar();
				out >> (char)propId >> (char)mp;
				break;
			}

			case PLPROP_KILLSCOUNT:
			{
				int kills = pPacket.readGInt();
				out >> (char)propId >> (int)kills;
				break;
			}

			case PLPROP_DEATHSCOUNT:
			{
				int deaths = pPacket.readGInt();
				out >> (char)propId >> (int)deaths;
				break;
			}

			case PLPROP_ONLINESECS:
			{
				int onlinesecs = pPacket.readGInt();
				out >> (char)propId >> (int)onlinesecs;
				break;
			}

			case PLPROP_IPADDR:
			{
				int ip = pPacket.readGInt5();
				out >> (char)propId >> (long long)ip;
				break;
			}

			case PLPROP_UDPPORT:
			{
				int udpport = pPacket.readGInt();
				out >> (char)propId >> (int)udpport;
				break;
			}

			case PLPROP_ALIGNMENT:
			{
				unsigned char ap = pPacket.readGUChar();
				out >> (char)propId >> (char)ap;
				break;
			}

			case PLPROP_ADDITFLAGS:
			{
				unsigned char additionalFlags = pPacket.readGUChar();
				out >> (char)propId >> (char)additionalFlags;
				break;
			}

			case PLPROP_ACCOUNTNAME:
			{
				len = pPacket.readGUChar();
				CString account = pPacket.readChars(len);
				out >> (char)propId >> (char)account.length() << account;
				break;
			}

			case PLPROP_BODYIMG:
			{
				len = pPacket.readGUChar();
				CString bodyImg = pPacket.readChars(len);
				out >> (char)propId >> (char)bodyImg.length() << bodyImg;
				break;
			}

			case PLPROP_RATING:
			{
				int rating = pPacket.readGInt();
				out >> (char)propId >> (int)rating;
				break;
			}

			case PLPROP_ATTACHNPC:
			{
				// Only supports object_type 0 (NPC).
				unsigned char object_type = pPacket.readGUChar();
				unsigned int npcID = pPacket.readGUInt();
				out >> (char)propId >> (char)object_type >> (int)npcID;
				break;
			}
/*
			case PLPROP_UNKNOWN50:
				break;
*/
			case PLPROP_PCONNECTED:
			break;

			case PLPROP_PLANGUAGE:
			{
				len = pPacket.readGUChar();
				CString language = pPacket.readChars(len);
				out >> (char)propId >> (char)language.length() << language;
				break;
			}

			case PLPROP_PSTATUSMSG:
			{
				unsigned char statusMsg = pPacket.readGUChar();
				out >> (char)propId >> (char)statusMsg;
				break;
			}

			case PLPROP_GATTRIB1:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB2:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB3:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB4:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB5:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB6:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB7:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB8:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB9:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB10:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB11:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB12:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB13:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB14:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB15:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB16:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB17:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB18:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB19:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB20:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB21:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB22:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB23:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB24:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB25:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB26:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB27:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB28:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB29:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;
			case PLPROP_GATTRIB30:  attr = pPacket.readChars(pPacket.readGUChar()); out >> (char)propId >> (char)attr.length() << attr; break;

			// OS type.
			// Windows: wind
			case PLPROP_OSTYPE:
			{
				CString os = pPacket.readChars(pPacket.readGUChar());

				CProgramSettingsCategory* props = settings.get("ClientProps");
				if (props)
				{
					if (props->exist("ostype"))
						os = props->get("ostype");
				}
				
				out >> (char)propId >> (char)os.length() << os;
				break;
			}

			// Text codepage.
			// Example: 1252
			case PLPROP_TEXTCODEPAGE:
			{
				int codepage = pPacket.readGInt();
				out >> (char)propId >> (int)codepage;
				break;
			}

			// Location, in pixels, of the player on the level in 2.30+ clients.
			// Bit 0x0001 controls if it is negative or not.
			// Bits 0xFFFE are the actual value.
			case PLPROP_X2:
			{
				unsigned short x2 = len = pPacket.readGUShort();
				out >> (char)propId >> (short)x2;

				// If the first bit is 1, our position is negative.
				x2 >>= 1;
				if ((short)len & 0x0001) x2 = -x2;
				
				prop_x = (float)x2 / 16.0f;

				break;
			}

			case PLPROP_Y2:
			{
				unsigned short y2 = len = pPacket.readGUShort();
				out >> (char)propId >> (short)y2;

				// If the first bit is 1, our position is negative.
				y2 >>= 1;
				if ((short)len & 0x0001) y2 = -y2;

				prop_y = (float)y2 / 16.0f;

				break;
			}

			case PLPROP_Z2:
			{
				unsigned short z2 = len = pPacket.readGUShort();
				out >> (char)propId >> (short)z2;

				// If the first bit is 1, our position is negative.
				z2 >>= 1;
				if ((short)len & 0x0001) z2 = -z2;

				break;
			}

			case PLPROP_GMAPLEVELX:
			{
				unsigned char gmaplevelx = pPacket.readGUChar();
				out >> (char)propId >> (char)gmaplevelx;
				break;
			}

			case PLPROP_GMAPLEVELY:
			{
				unsigned char gmaplevely = pPacket.readGUChar();
				out >> (char)propId >> (char)gmaplevely;
				break;
			}

			case PLPROP_COMMUNITYNAME:
			{
				CString name = pPacket.readChars(pPacket.readGUChar());
				out >> (char)propId >> (char)name.length() << name;
				break;
			}

			default:
			{
#ifdef DEBUG
				printf("Unidentified PLPROP: %i, readPos: %d\n", propId, pPacket.readPos());
				for (int i = 0; i < pPacket.length(); ++i)
					printf("%02x ", (unsigned char)pPacket[i]);
				printf("\n");
#endif
			}
			return out;
		}
	}

	return out;
}
