#pragma comment(lib,"ntdll.lib")
#pragma warning(disable : 26451) // Bug in VS according to Stackoverflow.

#include "kernelinterface.hpp"
#include "offsets.hpp"
#include "data.hpp"
#include <iostream>
#include <TlHelp32.h>
#include "Aimbot.hpp"
#include "AntiAim.hpp"
#include "BSPParser.hpp"
#include "ClientMode.h"
#include "config.hpp"
#include "Engine.hpp"
#include "Entity.hpp"
#include <windows.h>

using namespace std;

// hazedumper namespace
using namespace hazedumper::netvars;
using namespace hazedumper::signatures;

Entity CreateEntity(int Address)
{
	if (Address > 0)
	{
		Entity entity(Address);
		return entity;
	}
	
	Entity dummy(0);
	return dummy;
}

vector<string> Split(string s, string delimiter)
{
	size_t pos_start = 0, pos_end, delim_len = delimiter.length();
	string token;
	vector<string> res;

	while ((pos_end = s.find(delimiter, pos_start)) != string::npos) 
	{
		token = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	res.push_back(s.substr(pos_start));
	return res;
}


// TODO: Implement https://guidedhacking.com/threads/external-silent-aim-proof-of-concept-no-shellcode.13595/
// https://www.unknowncheats.me/forum/counterstrike-global-offensive/144597-matchmaking-colors.html

// TODO: On close free all global values from the memory.
inline Engine engine;
inline Aimbot aim = NULL;

void TriggerBotThread()
{
	while (true)
	{
		if (!engine.IsInGame())
		{
			Sleep(500);
			continue;
		}

		if (!aim.localPlayer.isValidPlayer())
		{
			continue;
		}

		//uint32_t LocalPlayer = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + dwLocalPlayer, sizeof(uint32_t));
		Entity LocalPlayerEnt = aim.localPlayer;
			
		if (TriggerBotKey == 0 || (GetAsyncKeyState(TriggerBotKey) & KEY_DOWN))
		{
			bool usable = false;
			uint16_t weaponid = LocalPlayerEnt.GetCurrentWeaponID();
			for (uint16_t wep : WeaponIDs)
			{
				if (wep == 0 || (weaponid > 0 && wep == weaponid))
				{
					usable = true;
					break;
				}
			}

			if (usable)
			{
				aim.TriggerBot();
			}
		}

		Sleep(2);
	}
}

int main(int argc, char* argv[], char* envp[])
{
	Driver = KeInterface("\\\\.\\garhalop");
	SetConsoleTitle(L"GarHal is the best fish ever, by DreTaX");
	
	// Get address of client.dll, engine.dll, and PID.
	ProcessId = Driver.GetTargetPid();
	ClientAddress = Driver.GetClientModule();
	EngineAddress = Driver.GetEngineModule();
	ClientSize = Driver.GetClientModuleSize();
	EngineSize = Driver.GetEngineModuleSize();

	if (ProcessId == 0 || ClientAddress == 0 || EngineAddress == 0)
	{
		std::cout << "Addresses are 0. Start driver & Start CSGO & restart. " << ProcessId << std::endl;
		system("pause");
		return 0;
	}

	hazedumper::BSPParser bspParser;

	// Read config values.
	Config config("garhal.cfg", envp);
	AimbotS = config.pInt("AimbotS");
	AimbotKey = config.pHex("AimbotKey");
	AimbotTarget = config.pInt("AimbotTarget");
	AimbotBullets = config.pInt("AimbotBullets");
	Bhop = config.pBool("Bhop");
	AntiAimS = config.pBool("AntiAim");
	Wallhack = config.pBool("Wallhack");
	NoFlash = config.pBool("NoFlash");
	TriggerBot = config.pBool("TriggerBot");
	TriggerBotKey = config.pHex("TriggerBotKey");
	Radar = config.pBool("Radar");

	std::string weapons = config.pString("TriggerBotAllowed");
	vector<string> dweapons = Split(weapons, ",");
	for (string w : dweapons)
	{
		char cstr[10];
		strcpy_s(cstr, w.c_str());
		WeaponIDs.push_back(atoi(cstr));
	}

	std::cout << "GarHal made by DreTaX" << std::endl;

	std::cout << "==== Memory Addresses ====" << std::endl;
	std::cout << "ProcessID: " << ProcessId << std::endl;
	std::cout << "ClientAddress: " << ClientAddress << std::endl;
	std::cout << "EngineAddress: " << EngineAddress << std::endl;
	std::cout << "ClientSize: " << ClientSize << std::endl;

	// Store address of localplayer
	uint32_t LocalPlayer = 0;

	uint32_t GlowObject = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + dwGlowObjectManager, sizeof(uint32_t));

	std::cout << "GlowObject: " << GlowObject << std::endl;

	std::cout << "==== Config Values ====" << std::endl;
	std::cout << "AimbotS: " << unsigned(AimbotS) << std::endl;
	std::cout << "AimbotKey: " << unsigned(AimbotKey) << std::endl;
	std::cout << "AimbotTarget: " << unsigned(AimbotTarget) << std::endl;
	std::cout << "AimbotBullets: " << unsigned(AimbotBullets) << std::endl;
	std::cout << "AntiAim: " << std::boolalpha << AntiAimS << std::endl;
	std::cout << "Bhop: " << std::boolalpha << Bhop << std::endl;
	std::cout << "Wallhack: " << std::boolalpha << Wallhack << std::endl;
	std::cout << "NoFlash: " << std::boolalpha << NoFlash << std::endl;
	std::cout << "TriggerBot: " << std::boolalpha << TriggerBot << std::endl;
	std::cout << "TriggerBotKey: " << unsigned(TriggerBotKey) << std::endl;
	std::cout << "Radar: " << std::boolalpha << Radar << std::endl;

	aim = Aimbot(&bspParser);
	AntiAim antiaim = AntiAim();

	// Do not use this until I drop handle usage.
	if (AntiAimS)
	{
		ClientMode* clientMode;
		clientMode = **(ClientMode***)((*(uintptr_t**)ClientAddress)[10] + 0x5);
		IClientMode = (DWORD**)clientMode;
		
		antiaim.Enable();
		antiaim.HookCreateMove();
		std::cout << "~AntiAim Create Move hooked!" << std::endl;
	}

	if (TriggerBot) 
	{
		std::thread TriggerBotT(TriggerBotThread);
	}

	while (true)
	{
		if (!engine.IsInGame())
		{
			Sleep(500);
			continue;
		}

		if (Wallhack)
		{
			GlowObject = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + dwGlowObjectManager, sizeof(uint32_t));
		}

		LocalPlayer = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + dwLocalPlayer, sizeof(uint32_t));
		Entity LocalPlayerEnt = Entity(LocalPlayer);

		if (aim.localPlayer.GetEntityAddress() != LocalPlayerEnt.GetEntityAddress())
		{
			aim.localPlayer = LocalPlayerEnt;
		}
		
		uint8_t OurTeam = LocalPlayerEnt.getTeam();
		if (NoFlash) 
		{
			LocalPlayerEnt.SetFlashAlpha(0.0f);
		}

		for (short int i = 0; i < 64; i++)
		{
			uint32_t EntityAddr = Driver.ReadVirtualMemory<uint32_t>(ProcessId, ClientAddress + dwEntityList + i * 0x10, sizeof(uint32_t));

			if (EntityAddr == NULL)
			{
				continue;
			}
			
			if (Wallhack)
			{
				Entity ent = CreateEntity(EntityAddr);
				if (ent.isValidPlayer() && !ent.IsDormant())
				{
					ent.SetCorrectGlowStruct(OurTeam, GlowObject);
				}
			}
			
			if (Radar)
			{
				Entity ent = CreateEntity(EntityAddr);
				if (ent.isValidPlayer() && !ent.IsDormant()) 
				{
					Driver.WriteVirtualMemory<bool>(ProcessId, EntityAddr + m_bSpotted, true, sizeof(bool));
				}
			}
		}


		if (Bhop) 
		{
			if (GetAsyncKeyState(VK_SPACE) & KEY_DOWN)
			{
				if (LocalPlayerEnt.isValidPlayer())
				{
					if (!LocalPlayerEnt.isInAir())
					{
						LocalPlayerEnt.SetForceJump(6);
					}
				}
			}
		}

		if (AntiAimS)
		{
			antiaim.DoAntiAim();
		}

		if (AimbotS == 1) 
		{
			if (GetAsyncKeyState(AimbotKey) & KEY_DOWN)
			{
				aim.aimAssist();
			}
			else
			{
				aim.resetSensitivity();
			}
		}
		else if (AimbotS == 2)
		{
			if (GetAsyncKeyState(AimbotKey) & KEY_DOWN)
			{
				aim.aimBot();
			}
		}

		Sleep(3);
	}

	return 0;
}