#pragma once

#include "Headers.h"

#ifndef HEROAIUTILS_H
#define HEROAIUTILS_H

float DegToRad(float degree) {
    return (degree * 3.14159 / 180);
}

struct ScreenData {
    float width;
    float height;
};

void wcs_tolower(wchar_t* s)
{
    for (size_t i = 0; s[i]; i++)
        s[i] = towlower(s[i]);
}

char* WCharToChar(const wchar_t* wstr)
{
    if (!wstr) return nullptr;

    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    char* strTo = new char[size_needed];
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, strTo, size_needed, NULL, NULL);
    return strTo;
}

std::string WStringToString(const std::wstring& s)
{
    // @Cleanup: ASSERT used incorrectly here; value passed could be from anywhere!
    if (s.empty()) {
        return "Error In Name";
    }
    // NB: GW uses code page 0 (CP_ACP)
    const auto size_needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    PLUGIN_ASSERT(size_needed != 0);
    std::string strTo(size_needed, 0);
    PLUGIN_ASSERT(WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), strTo.data(), size_needed, NULL, NULL));
    return strTo;
}


ScreenData GetInjectedWindowSize() {
    //HWND hwnd = GetForegroundWindow(); // Get the handle of the current foreground window
    HWND hwnd = GW::MemoryMgr::GetGWWindowHandle();
    //HWND hwnd gw_window_handle = GW::MemoryMgr::GetGWWindowHandle();
    RECT rect;

    ScreenData screenSize = { 0, 0 };

    if (hwnd && GetClientRect(hwnd, &rect)) {
        screenSize.width = static_cast<float>(rect.right - rect.left);
        screenSize.height = static_cast<float>(rect.bottom - rect.top);
    }

    return screenSize;
}


int HeroAI::GetPartyNumber() {
    if (GameVars.Map.IsPartyLoaded) {
        for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
        {
            const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
            if (playerId == GameVars.Target.Self.ID) { return i + 1; }
        }
    }
    return 0;
}

bool HeroAI::existsInParty(uint32_t player_id) {

    if (player_id <= 0) { return false; }
    for (int i = 0; i < (GW::PartyMgr::GetPartyPlayerCount()); i++)
    {
        const auto playerId = GW::Agents::GetAgentIdByLoginNumber(GameVars.Party.PartyInfo->players[i].login_number);
        if (!playerId) { continue; }
        const auto agent = GW::Agents::GetAgentByID(playerId);
        if (!agent) { continue; }
        const auto living = agent->GetAsAgentLiving();
        if (!living) { continue; }
        if (living->player_number == player_id) {
            return true;
        }
    }
    return false;
}


float HeroAI::AngleChange() {
    const auto angle = GameVars.Party.LeaderLiving->rotation_angle;
    if (!angle) {
        oldAngle = 0.0f;
        return 0.0f;
    }

    if (oldAngle != angle) {
        oldAngle = angle;
        return oldAngle;
    }
    return 0.0f;
}


bool HeroAI::IsMelee(GW::AgentID AgentID) {

    const auto tempAgent = GW::Agents::GetAgentByID(AgentID);
    const auto tLiving = tempAgent->GetAsAgentLiving();
    //enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; 

    switch (tLiving->weapon_type) {
        case axe:
        case hammer:
        case daggers:
        case scythe:
        case sword:
            return true;
            break;
        default:
            return false;
            break;
    }
    return false;

}

bool HeroAI::IsMartial(GW::AgentID AgentID) {

    const auto tempAgent = GW::Agents::GetAgentByID(AgentID);
    const auto tLiving = tempAgent->GetAsAgentLiving();
    //enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; 

    switch (tLiving->weapon_type) {
    case bow:
    case axe:
    case hammer:
    case daggers:
    case scythe:
    case spear:
    case sword:
        return true;
        break;
    default:
        return false;
        break;
    }
    return false;

}

bool HeroAI::ThereIsSpaceInInventory()
{
    GW::Bag** bags = GW::Items::GetBagArray();
    if (!bags) {
        return false;
    }
    size_t end_bag = static_cast<size_t>(GW::Constants::Bag::Bag_2);

    for (size_t bag_idx = static_cast<size_t>(GW::Constants::Bag::Backpack); bag_idx <= end_bag; bag_idx++) {
        GW::Bag* bag = bags[bag_idx];
        if (!bag || !bag->items.valid()) {
            continue;
        }
        for (size_t slot = 0; slot < bag->items.size(); slot++) {
            if (!bag->items[slot]) {
                return true;
            }
        }
    }
    return false;
}


int HeroAI::GetMemPosByID(GW::AgentID agentID) {
    for (int i = 0; i < 8; i++) {
        MemMgr.MutexAquire();
        if ((MemMgr.MemData->MemPlayers[i].IsActive) && (MemMgr.MemData->MemPlayers[i].ID == agentID)) {
            MemMgr.MutexRelease();
            return i;
        }
        MemMgr.MutexRelease();
    }
    return -1;
}



#endif // COMBAT_H
