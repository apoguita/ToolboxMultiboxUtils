#pragma once

#include "Headers.h"

enum eState { Idle, Looting, Following, Scatter };
enum combatState { cIdle, cInCastingRoutine };



struct ExperimentalFlagsClass {
    bool LeaderPing;
    bool DumbFollow;
    bool Debug;
    bool AreaRings;
    bool Energy;
    bool SkillData;
    bool MemMon;
    bool SinStrikeMon;
    bool PartyCandidates;
    bool CreateParty;
    bool Pcons;
    bool Title;
    bool resign;
    bool TakeQuest;
    bool Openchest;
    bool ForceFollow;
    bool FormationA;
    bool FormationB;
    bool FormationC;
    bool FormationD;
    bool FormationE;
};

class GameStateClass {
private:
    struct StateMachineStruct {
    public:
        eState State= Idle;
        clock_t LastActivity;
    
        bool Following=true;
        bool Looting = true;
        bool Combat = true;
        bool Targetting = true;
        bool Scatter = true;
        bool HeroFlag = false;
        bool isFollowingEnabled() const { return Following; }
        bool isLootingEnabled() const { return Looting; }
        bool isCombatEnabled() const { return Combat; }
        bool isTargettingEnabled() const { return Targetting; }
        bool isScatterEnabled() const { return Scatter; }
        bool isHeroFlagEnabled() const { return HeroFlag; }

        void toggleFollowing() { Following = !Following; }
        void toggleLooting() { Looting = !Looting; }
        void toggleCombat() { Combat = !Combat; }
        void toggleTargetting() { Targetting = !Targetting; }
        void toggleScatter() { Scatter = !Scatter; }
        void toggleHeroFlag() { HeroFlag = !HeroFlag; }
        float RangedRangeValue = GW::Constants::Range::Area;
        float MeleeRangeValue = GW::Constants::Range::Spellcast;
     
    };

    struct CombatStateMachine {
        combatState State;
        Timer LastActivity;
        Timer StayAlert;
        uint32_t castingptr = 0;
        uint32_t currentCastingSlot;
        bool IsSkillCasting = false;
        float IntervalSkillCasting;
    };
public:
    StateMachineStruct state;
    CombatStateMachine combat;
    bool CombatSkillState[8] = { true,true,true,true,true,true,true,true };

};
