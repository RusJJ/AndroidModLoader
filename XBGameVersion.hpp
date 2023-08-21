#include <stdio.h>
#include <string.h>
#include <android/log.h>
#include <dlfcn.h>
#include <ARMPatch/armpatch_src/ARMPatch.h>

// A separate header file for the game version, so that it can be used independently in other projects.  
// Use XB just to deal with name collisions. (XMDS)

namespace XB
{
	enum GAME_ID : char
	{
		UN_GAME,
		GTA3,
		GTAVC,
		GTASA,
		GTALCS,
		GTACTW
	};

	enum GAME_VER : char
	{
		UN_VER,
		// 32
		III_1_8,
		VC_1_09,
		SA_2_00,
		LCS_2_4,
		CTW_1_04,
		// hign ver
		III_1_9,
		VC_1_12,
		SA_2_10,
		// 64
		III_1_9_64,
		VC_1_12_64,
		SA_2_10_64
	};

	static const char* game_name[] = { "NONE", "GTA3", "GTAVC", "GTASA", "GTALCS", "GTACTW" };
	static const char* game_lib[] = { "NONE", "libR1.so", "libGTAVC.so", "libGTASA.so", "LibGTALcs.so", "LibCTW.so" };

	static const char* GetGameProcessName()
	{
		static char name[255];
		memset(name, 0, sizeof(name));
		FILE* cmd_file = fopen("/proc/self/cmdline", "r");
		if (cmd_file) {
			if (fgets(name, sizeof(name), cmd_file))
				return name;
		}
		return NULL;
	}


	static GAME_ID GetGameId()
	{
		const char* p_name = GetGameProcessName();
		if (p_name == NULL) {
			__android_log_write(ANDROID_LOG_ERROR, "GAME", "GetGameProcessName fail.");
			goto fail;
		}

		if (strcasestr(p_name, game_name[GAME_ID::GTA3])) {
			return GAME_ID::GTA3;
		}
		else if (strcasestr(p_name, game_name[GAME_ID::GTAVC])) {
			return GAME_ID::GTAVC;
		}
		else if (strcasestr(p_name, game_name[GAME_ID::GTASA])) {
			return GAME_ID::GTASA;
		}
		else if (strcasestr(p_name, game_name[GAME_ID::GTALCS])) {
			return GAME_ID::GTALCS;
		}
		else if (strcasestr(p_name, game_name[GAME_ID::GTACTW])) {
			return GAME_ID::GTACTW;
		}

	fail:
		__android_log_write(ANDROID_LOG_ERROR, "GAME", "GetGameId fail, Unknown Game.");
		return GAME_ID::UN_GAME;
	}

	static inline const char* GetGameName()
	{
		return game_name[GetGameId()];
	}

	static inline const char* GetGameLibName()
	{
		return game_lib[GetGameId()];
	}
	
	static GAME_VER GetGameVersion()
	{
		const char* lib_name = GetGameLibName();
		if (lib_name) {
			uintptr_t plt_addr = ARMPatch::GetAddressFromPattern(NULL, lib_name, ".plt");
			uintptr_t re_addr = plt_addr - ARMPatch::GetLib(lib_name);
			if (re_addr && re_addr != plt_addr) {
				switch (re_addr)
				{
				case 0x00189D44:
					return GAME_VER::SA_2_00;
				case 0x000AFBA0:
					return GAME_VER::VC_1_09;
				case 0x000970B8:
					return GAME_VER::III_1_8;
				case 0x00109784:
					return GAME_VER::LCS_2_4;
				case 0x002987F8:
					return GAME_VER::CTW_1_04;
				case 0x00189CCC:
					return GAME_VER::SA_2_10;
				case 0x000BAE98:
					return GAME_VER::VC_1_12;
				case 0x00097140:
					return GAME_VER::III_1_9;
				case 0x0000000000218E90:
					return GAME_VER::SA_2_10_64;
				case 0x00000000000FFD90:
					return GAME_VER::VC_1_12_64;
				case 0x00000000000CE100:
					return GAME_VER::III_1_9_64;
				default:
					break;
				}
			}
		}
		__android_log_write(ANDROID_LOG_ERROR, "GAME", "GetGameVersion fail, Unknown Game version.");
		return GAME_VER::UN_VER;
	}
}






