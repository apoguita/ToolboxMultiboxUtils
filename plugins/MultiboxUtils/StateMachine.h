#pragma once

#include "Headers.h"

enum eState { Idle, Looting, Following, Scatter };
enum combatState { cIdle, cInCastingRoutine };





class GameStateClass {
private:
    struct StateMachineStruct {
    public:
        eState State= Idle;
        clock_t LastActivity;
    
        bool Following=false;
        bool Looting = false;
        bool Combat = false;
        bool Targetting = false;
        bool Scatter = false;
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
     
    };

    struct CombatStateMachine {
        combatState State;
        clock_t LastActivity;
        clock_t StayAlert;
        uint32_t castingptr = 0;
        uint32_t currentCastingSlot;
        bool IsSkillCasting = false;
        float IntervalSkillCasting;
    };
public:
    StateMachineStruct state;
    CombatStateMachine combat;
    bool CombatSkillState[8];

};
