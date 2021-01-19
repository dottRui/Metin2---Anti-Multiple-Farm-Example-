/*
	Developed by .Rui
	Discord: .Rui#3825
*/

#include "stdafx.h"
#include "char.h"
#include "config.h"
#include "char_manager.h"
#include "p2p.h"
#include "buffer_manager.h"
#include "desc.h"
#include "utils.h"
#include "desc_manager.h"
#include "HAntiMultipleFarm.h"

CAntiMultipleFarm::CAntiMultipleFarm() {};
CAntiMultipleFarm::~CAntiMultipleFarm() {};

/*
	Quando dá login, vem aqui diretamente por chamada do core atual e passa aqui nas P2P communications;
	Dá build ao packet de info, elimando todos os registos antigos e trocando pelos novos ao pessoal do core
	que chama a função
*/
auto CAntiMultipleFarm::Login(std::string sMAIf, DWORD dwPID, int8_t forceState) -> void
{
	auto it = m_map_BlockDrops.find(sMAIf);
	
	TBlockDrop s_blockDrop;
	s_blockDrop.dwPID = dwPID;
	s_blockDrop.bDropState = (forceState != AF_GENERATE_NEW_STATE ? forceState : __GetPlayerStartupDropState(sMAIf));

	if (it == m_map_BlockDrops.end()) {
		std::vector<TBlockDrop> makeVector = {s_blockDrop};
		m_map_BlockDrops.insert(std::make_pair(sMAIf, makeVector));
	} else {
		auto it_check_pid = std::find_if(it->second.begin(), it->second.end(),
			[&](const TBlockDrop & sR) { return sR.dwPID == s_blockDrop.dwPID; });
		
		if (it_check_pid == it->second.end()) it->second.emplace_back(s_blockDrop);
	}
	
	__ReloadBlockDropStates(sMAIf);
	__BuildBlockDropsReloadPacakage(sMAIf);
}

/*
	Usado para inicializar o estado dos drops de uma personagem que dá login.
	Basicamente verifica se pode ou não inicializar os drops.
*/
auto CAntiMultipleFarm::__GetPlayerStartupDropState(std::string sMAIf) -> bool
{
	auto it = m_map_BlockDrops.find(sMAIf);
	if (it == m_map_BlockDrops.end()) return false;
	
	if (it->second.size() > MULTIPLE_FARM_MAX_ACCOUNT) return true;
	
	uint8_t u8TotalDropCount = 0;
	auto it_check_state = it->second.begin();
	for (; it_check_state != it->second.end(); ++it_check_state)
		u8TotalDropCount += (int)(!it_check_state->bDropState);
	
	return (u8TotalDropCount >= MULTIPLE_FARM_MAX_ACCOUNT);
}

/*
	Quando dá logout, vem aqui diretamente por chamada do core atual e passa aqui nas P2P communications.
	Retira o jogador da lista de jogadores ligados no mesmo pc, e dá build ao packet que limpa todos os registos
	da parte do cliente e manda os novos, sem o jogador.
*/
auto CAntiMultipleFarm::Logout(std::string sMAIf, DWORD dwPID, bool is_warping) -> void
{
	// /*fix quando um jogador se teleporta*/
	if (is_warping) return;
	
	auto it = m_map_BlockDrops.find(sMAIf);
	if (it == m_map_BlockDrops.end()) return;
	
	{
		auto it_check_pid = std::find_if(it->second.begin(), it->second.end(), 
			[&](const TBlockDrop & sR) { return sR.dwPID == dwPID; });
		
		if (it_check_pid == it->second.end()) return;
		it->second.erase(it_check_pid);
	}
	
	__ReloadBlockDropStates(sMAIf);
	__BuildBlockDropsReloadPacakage(sMAIf);
}

/*
	Retorna o estado dos drops do jogador de PID -> dwPID, esta função é chamada sempre que se quer
	verificar se o jogador pode ou não dropar alguma coisa
*/
auto CAntiMultipleFarm::GetPlayerDropState(std::string sMAIf, DWORD dwPID) -> bool
{
	auto iterate_map = m_map_BlockDrops.find(sMAIf);
	if (iterate_map == m_map_BlockDrops.end()) return false;
	
	{
		auto it = std::find_if(iterate_map->second.begin(), iterate_map->second.end(), 
			[&](const TBlockDrop & sR) { return sR.dwPID == dwPID; });
		
		if (it != iterate_map->second.end()) return it->bDropState;
	}
	
	return false;
}

/*
	Verifica se o estado dos drops dos jogadores registados no computador são válidos, caso não forem
	corrige, normalmente esta verificação é usada ANTES de mandar de dar build ao packet das informações
*/
auto CAntiMultipleFarm::__ReloadBlockDropStates(std::string sMAIf) -> void
{
	auto it = m_map_BlockDrops.find(sMAIf);
	if (it == m_map_BlockDrops.end()) return;
	
	if (it->second.size() <= MULTIPLE_FARM_MAX_ACCOUNT)
	{
		auto it_deblock = it->second.begin();
		for (; it_deblock != it->second.end(); ++it_deblock)
			it_deblock->bDropState = false;
		return;
	}
	
	uint8_t u8TotalDropCount, u8DistributeDropCount;
	u8TotalDropCount = u8DistributeDropCount = 0;
	
	auto it_check_state = it->second.begin();
	for (; it_check_state != it->second.end(); ++it_check_state)
		u8TotalDropCount += (int)(!it_check_state->bDropState);
	
	if (u8TotalDropCount < MULTIPLE_FARM_MAX_ACCOUNT)
		u8DistributeDropCount = MAX(0, MULTIPLE_FARM_MAX_ACCOUNT - u8TotalDropCount);
	
	if (!u8DistributeDropCount) return;
	
	auto it_deblock = it->second.begin();
	for (; it_deblock != it->second.end(); ++it_deblock) {
		if (u8DistributeDropCount <= 0) break;
		if (!it_deblock->bDropState) continue;
		
		it_deblock->bDropState = false;
		--u8DistributeDropCount;
	}
}

/*
	Packet dinâmico usado para mandar as informações de jogadores e estados registados
	no computador.
*/
auto CAntiMultipleFarm::__BuildBlockDropsReloadPacakage(std::string sMAIf) -> void
{
	/*fix cross-fire between server stages*/
	if (g_bAuthServer) return;
	
	auto iterate_map = m_map_BlockDrops.find(sMAIf);
	if (iterate_map == m_map_BlockDrops.end()) return;
	
	TEMP_BUFFER buf;
	
	TSendAntiFarmInfo dataPacket(HEADER_GC_ANTI_FARM, 0, AF_SH_SENDING_DATA);
	dataPacket.size = sizeof(dataPacket);
	
	bool canReset = true;
	buf.write(&canReset, sizeof(canReset));
	
	auto it = iterate_map->second.begin();
	for (; it < iterate_map->second.end(); it++) {
		DWORD dwPID = it->dwPID;
		CCI* pkCCI = P2P_MANAGER::instance().FindByPID(dwPID);
		TAntiFarmPlayerInfo playerInfo(dwPID, it->bDropState);
		
		if (!pkCCI) {
			LPCHARACTER pkTarget = nullptr;
			if (!(pkTarget = CHARACTER_MANAGER::instance().FindByPID(dwPID)) || (pkTarget && !pkTarget->GetDesc())) continue;
			
			/*fix cross-fire between phase packages*/
			if (!pkTarget->GetDesc()->IsPhase(PHASE_GAME) && !pkTarget->GetDesc()->IsPhase(PHASE_DEAD)) continue;
			strlcpy(playerInfo.szName, pkTarget->GetName(), sizeof(playerInfo.szName));
		}
		else strlcpy(playerInfo.szName, pkCCI->szName, sizeof(playerInfo.szName));
		
		buf.write(&playerInfo, sizeof(playerInfo));
	}
	dataPacket.size += buf.size();
	
	auto it_send = iterate_map->second.begin();
	for (; it_send < iterate_map->second.end(); it_send++) {
		DWORD dwPID = it_send->dwPID;
		
		LPCHARACTER pkTarget = nullptr;
		if (!(pkTarget = CHARACTER_MANAGER::instance().FindByPID(dwPID)) || (pkTarget && !pkTarget->GetDesc())) continue;

		/*fix cross-fire between phase packages*/
		if (!pkTarget->GetDesc()->IsPhase(PHASE_GAME) && !pkTarget->GetDesc()->IsPhase(PHASE_DEAD)) continue;
	
		if (buf.size()) {
			pkTarget->GetDesc()->BufferedPacket(&dataPacket, sizeof(dataPacket));
			pkTarget->GetDesc()->Packet(buf.read_peek(), buf.size());
		}
		else pkTarget->GetDesc()->Packet(&dataPacket, sizeof(dataPacket));
	}
}

/*
	Usado para alterar os estados dos drops dos jogadores, ____ainda não está estável____ 
*/
auto CAntiMultipleFarm::SendBlockDropStatusChange(std::string sMAIf, std::vector<DWORD> dwPIDS) -> void
{
	{
		auto it = m_map_BlockDrops.find(sMAIf);
		if (it == m_map_BlockDrops.end()) return;
		
		auto it_deblock = it->second.begin();
		for (; it_deblock != it->second.end(); ++it_deblock)
			it_deblock->bDropState = !(std::find(dwPIDS.begin(), dwPIDS.end(), it_deblock->dwPID) != dwPIDS.end());
	}
	
	__ReloadBlockDropStates(sMAIf);
	__BuildBlockDropsReloadPacakage(sMAIf);
}

/*
	Dá print aos jogadores ligados no mesmo computador e o seu estado
*/
auto CAntiMultipleFarm::PrintPlayerDropState(std::string sMAIf, LPCHARACTER ch) -> void
{
	if (!ch) return;
	
	auto iterate_map = m_map_BlockDrops.find(sMAIf);
	if (iterate_map == m_map_BlockDrops.end()) return;
	
	ch->ChatPacket(CHAT_TYPE_INFO, "------------------------------------");
	for (auto &sR : iterate_map->second)
		ch->ChatPacket(CHAT_TYPE_INFO, "PID do jogador: %d || Estado: %d (1 - Bloqueado / 0 - Desbloqueado)", sR.dwPID, (int)sR.bDropState);
	ch->ChatPacket(CHAT_TYPE_INFO, "------------------------------------");
}
