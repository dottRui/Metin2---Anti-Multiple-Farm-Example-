#pragma once

#ifndef __ANTI_FARM_HEADER_LOAD__
#define __ANTI_FARM_HEADER_LOAD__

class CAntiMultipleFarm : public singleton<CAntiMultipleFarm>
{
	typedef struct SBlockDrop{DWORD dwPID; bool bDropState;} TBlockDrop;
	typedef std::unordered_map<std::string, std::vector<TBlockDrop>> TBlockDropsMaps;
	
	public:
		enum ERecvFuncBlockStates {AF_UNBLOCK_STATE, AF_BLOCK_STATE, AF_GENERATE_NEW_STATE};
		
		/*only used for p2p-update status packages*/
		typedef struct SP2PChangeDropStatus
		{
			SP2PChangeDropStatus(BYTE header) : header(header) {}
			BYTE header; DWORD dwPIDs[MULTIPLE_FARM_MAX_ACCOUNT]; char cMAIf[MA_LENGTH + 1];
		} TP2PChangeDropStatus;
		
		CAntiMultipleFarm();
		~CAntiMultipleFarm();
		
		auto	Login(std::string sMAIf, DWORD dwPID, int8_t forceState = AF_GENERATE_NEW_STATE) -> void;
		auto	Logout(std::string sMAIf, DWORD dwPID, bool is_warping) -> void;
		auto	GetPlayerDropState(std::string sMAIf, DWORD dwPID) -> bool;
		auto	SendBlockDropStatusChange(std::string sMAIf, std::vector<DWORD> dwPIDS) -> void;
		auto	PrintPlayerDropState(std::string sMAIf, LPCHARACTER ch) -> void;
		
		inline auto P2PLogin(std::string sMAIf, DWORD dwPID, int8_t forceState) -> void { Login(sMAIf, dwPID, forceState); }
		inline auto P2PLogout(std::string sMAIf, DWORD dwPID, bool is_warping = false) -> void { Logout(sMAIf, dwPID, is_warping); }
		inline auto P2PSendBlockDropStatusChange(std::string sMAIf, std::vector<DWORD> dwPIDS) -> void { SendBlockDropStatusChange(sMAIf, dwPIDS); }
	protected:
		TBlockDropsMaps m_map_BlockDrops;
		
		auto	__BuildBlockDropsReloadPacakage(std::string sMAIf) -> void;
		auto	__GetPlayerStartupDropState(std::string sMAIf) -> bool;
		auto	__ReloadBlockDropStates(std::string sMAIf) -> void;
};

#endif // !__ANTI_FARM_HEADER_LOAD__
