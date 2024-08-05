#pragma once

#include "Headers.h"

#ifndef HEROFUNC_H
#define HEROFUNC_H

enum eClickHandler { eHeroFlag };
struct ClickHandlerStruct {
    bool WaitForClick = false;
    eClickHandler msg;
    int data;
    bool IsPartyWide = false;

} ClickHandler;

//hero stuff
enum FlaggingState : uint32_t {
    FlagState_All = 0,
    FlagState_Hero1,
    FlagState_Hero2,
    FlagState_Hero3,
    FlagState_Hero4,
    FlagState_Hero5,
    FlagState_Hero6,
    FlagState_Hero7,
    FlagState_None
};

enum CaptureMouseClickType : uint32_t {
    CaptureType_None [[maybe_unused]] = 0,
    CaptureType_FlagHero [[maybe_unused]] = 1,
    CaptureType_SalvageWithUpgrades [[maybe_unused]] = 11,
    CaptureType_SalvageMaterials [[maybe_unused]] = 12,
    CaptureType_Idenfify [[maybe_unused]] = 13
};

struct MouseClickCaptureData {
    struct sub1 {
        uint8_t pad0[0x3C];

        struct sub2 {
            uint8_t pad1[0x14];

            struct sub3 {
                uint8_t pad2[0x24];

                struct sub4 {
                    uint8_t pad3[0x2C];

                    struct sub5 {
                        uint8_t pad4[0x4];
                        FlaggingState* flagging_hero;
                    } *sub5;
                } *sub4;
            } *sub3;
        } *sub2;
    } *sub1;
}*MouseClickCaptureDataPtr = nullptr;

uint32_t* GameCursorState = nullptr;
CaptureMouseClickType* CaptureMouseClickTypePtr = nullptr;

FlaggingState GetFlaggingState()
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return FlagState_None;
    }
    if (!CaptureMouseClickTypePtr || *CaptureMouseClickTypePtr != CaptureType_FlagHero || !MouseClickCaptureDataPtr || !MouseClickCaptureDataPtr->sub1) {
        return FlagState_None;
    }
    return *MouseClickCaptureDataPtr->sub1->sub2->sub3->sub4->sub5->flagging_hero;
}


bool HeroAI::IsHeroFlagged(int hero) {
    if (hero <= GW::PartyMgr::GetPartyHeroCount()) {
        if (hero == 0) {
            const GW::Vec3f& allflag = GW::GetGameContext()->world->all_flag;
            if (allflag.x != 0 && allflag.y != 0 && (!std::isinf(allflag.x) || !std::isinf(allflag.y))) {
                return true;
            }
        }
        else {
            const GW::HeroFlagArray& flags = GW::GetGameContext()->world->hero_flags;
            if (!flags.valid() || static_cast<uint32_t>(hero) > flags.size()) {
                return false;
            }

            const GW::HeroFlag& flag = flags[hero - 1];
            if (!std::isinf(flag.flag.x) || !std::isinf(flag.flag.y)) {
                return true;
            }
        }
    }
    else {
        MemMgr.MutexAquire();
        bool ret = ((MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsFlagged) && (MemMgr.MemData->MemPlayers[hero - GW::PartyMgr::GetPartyHeroCount()].IsActive)) ? true : false;
        MemMgr.MutexRelease();
        return ret;

    }


    return false;

}


bool SetFlaggingState(FlaggingState set_state)
{
    if (GW::Map::GetInstanceType() != GW::Constants::InstanceType::Explorable) {
        return false;
    }
    // keep an internal flag for the minimap flagging until StartCaptureMouseClick_Func is working
    // minimap_flagging_state = set_state;
    if (GetFlaggingState() == set_state) { return true; }
    if (set_state == FlagState_None) {
        set_state = GetFlaggingState();
    }
    GW::UI::ControlAction key;
    switch (set_state) {
    case FlagState_Hero1:
        key = GW::UI::ControlAction_CommandHero1;
        break;
    case FlagState_Hero2:
        key = GW::UI::ControlAction_CommandHero2;
        break;
    case FlagState_Hero3:
        key = GW::UI::ControlAction_CommandHero3;
        break;
    case FlagState_Hero4:
        key = GW::UI::ControlAction_CommandHero4;
        break;
    case FlagState_Hero5:
        key = GW::UI::ControlAction_CommandHero5;
        break;
    case FlagState_Hero6:
        key = GW::UI::ControlAction_CommandHero6;
        break;
    case FlagState_Hero7:
        key = GW::UI::ControlAction_CommandHero7;
        break;
    case FlagState_All:
        key = GW::UI::ControlAction_CommandParty;
        break;
    default:
        return false;
    }
    return Keypress(key);
}

bool FlagHero(uint32_t idx)
{
    if (idx > FlagState_None) {
        return false;
    }
    return SetFlaggingState(static_cast<FlaggingState>(idx));
}

void HeroAI::Flag(int hero) {

    if ((hero <= GW::PartyMgr::GetPartyHeroCount()) && (GW::PartyMgr::GetPartyHeroCount() > 0)) {
        FlagHero(hero);
        GameVars.Party.Flags[hero] = true;
    }
    else {
        if (GW::PartyMgr::GetPartyHeroCount() == 0) {
            ClickHandler.IsPartyWide = true;
        }
        ClickHandler.WaitForClick = true;
        ClickHandler.msg = eHeroFlag;
        ClickHandler.data = hero - GW::PartyMgr::GetPartyHeroCount();
    }
}

void HeroAI::UnflagAllHeroes() {

    GW::PartyMgr::UnflagAll();
    MemMgr.MutexAquire();
    MemMgr.MemData->MemPlayers[0].FlagPos.x = GameVars.Party.LeaderLiving->x;
    MemMgr.MemData->MemPlayers[0].FlagPos.y = GameVars.Party.LeaderLiving->y;
    GameVars.Party.Flags[0] = false;
    MemMgr.MutexRelease();
    for (int i = 1; i <= GW::PartyMgr::GetPartyHeroCount(); i++) {
        GW::PartyMgr::UnflagHero(i);
    }


    for (int i = 0; i < GW::PartyMgr::GetPartySize(); i++)
    {
        MemMgr.MutexAquire();
        MemMgr.MemData->MemPlayers[i].IsFlagged = false;
        MemMgr.MemData->MemPlayers[i].IsAllFlag = false;

        MemMgr.MemData->MemPlayers[i].FlagPos.x = 0;
        MemMgr.MemData->MemPlayers[i].FlagPos.y = 0;
        MemMgr.MutexRelease();
    }
}



#endif // HEROFUNC_H
