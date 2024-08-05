#pragma once

#include "Headers.h"

#ifndef TARGETTING_H
#define TARGETTING_H

GW::AgentID HeroAI::PartyTargetIDChanged() {
    const auto target = GameVars.Party.PartyInfo->players[0].calledTargetId;
    if (!target) {
        oldCalledTarget = 0;
        return 0;
    }

    if (oldCalledTarget != target) {
        oldCalledTarget = target;
        return oldCalledTarget;
    }
    return 0;
}

//////////////////// ENEMY TARGETTING ///////////////////////////
uint32_t  HeroAI::TargetNearestEnemy(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID = 0;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        EnemyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }
 
        EnemyID = tEnemyLiving->agent_id;
 
        if (!EnemyID) { continue; }
        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }
      
        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  HeroAI::TargetNearestEnemyCaster(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID = 0;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        EnemyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }
        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (IsMartial(EnemyID)) {continue;}

        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  HeroAI::TargetNearestEnemyMartial(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID = 0;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        EnemyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }
        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (!IsMartial(EnemyID)) { continue; }

        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  HeroAI::TargetNearestEnemyMelee(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID = 0;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        EnemyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }
        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (!IsMelee(EnemyID)) { continue; }

        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  HeroAI::TargetNearestEnemyRanged(bool IsBigAggro) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;
    float maxdistance = GW::Constants::Range::Earshot;
    if ((!GameState.combat.StayAlert.hasElapsed(3000)) || (IsBigAggro)) {
        distance = GW::Constants::Range::Spellcast;
        maxdistance = GW::Constants::Range::Spellcast;
    }

    static GW::AgentID EnemyID = 0;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        EnemyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Enemy) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }
        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (IsMelee(EnemyID)) { continue; }

        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != maxdistance) { return tStoredEnemyID; }
    else {
        return 0;
    }
}
//////////////////// END ENEMY TARGETTING ///////////////////////////
//////////////////// ALLY TARGETTING ////////////////////////////////


uint32_t  HeroAI::TargetLowestAlly(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        const float vlife = tAllyLiving->hp;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        //IsAllyMelee = IsMelee(AllyID);

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }  

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyEnergy(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyenergyID = 0;
    float StoredEnergy = 2.0f;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;

        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GW::Constants::SkillID::Blood_is_Power, AllyID)) { continue; }
        if (HasEffect(GW::Constants::SkillID::Blood_Ritual, AllyID)) { continue; }
        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        //ENERGY CHECKS
        MemMgr.MutexAquire();
        int PIndex = GetMemPosByID(AllyID);

        if (PIndex >= 0) {
            if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
                (MemMgr.MemData->MemPlayers[PIndex].Energy < StoredEnergy)
                ) {
                StoredEnergy = MemMgr.MemData->MemPlayers[PIndex].Energy;
                tStoredAllyenergyID = AllyID;
            }
        }
        MemMgr.MutexRelease();
 

    }

    if (StoredEnergy != 2.0f) { return tStoredAllyenergyID; }

    return 0;

}

uint32_t  HeroAI::TargetLowestAllyCaster(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        const float vlife = tAllyLiving->hp;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        if (IsMartial(AllyID)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyMartial(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        const float vlife = tAllyLiving->hp;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        if (!IsMartial(AllyID)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyMelee(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        const float vlife = tAllyLiving->hp;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        if (!IsMelee(AllyID)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

uint32_t  HeroAI::TargetLowestAllyRanged(bool OtherAlly) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    auto slot = GameState.combat.currentCastingSlot;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, AllyID)) { continue; }

        const float vlife = tAllyLiving->hp;

        if ((OtherAlly) && (AllyID == GameVars.Target.Self.ID)) { continue; }

        if (IsMelee(AllyID)) { continue; }

        if (vlife < StoredLife)
        {
            StoredLife = vlife;
            tStoredAllyID = AllyID;
        }

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;
}

//////////////////// END ALLY TARGETTING ///////////////////////////

uint32_t  HeroAI::TargetDeadAllyInAggro() {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Ally_NonAttackable) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsDead()) { continue; }

        return AllyID;
    }
    return 0;

}

uint32_t  HeroAI::TargetNearestCorpse() {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    for (const GW::Agent* agent : *agents)
    {
        AllyID = 0;
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        switch (tAllyLiving->allegiance) {
            case GW::Constants::Allegiance::Ally_NonAttackable:
            case GW::Constants::Allegiance::Neutral:
            case GW::Constants::Allegiance::Enemy:
            case GW::Constants::Allegiance::Npc_Minipet:
                break;
            default:
                continue;
                break;
        }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsDead()) { continue; }

        return AllyID;
    }
    return 0;

}

uint32_t HeroAI::TargetNearestItem()
{
    const auto agents = GW::Agents::GetAgentArray();
    float distance = GW::Constants::Range::Spellcast;

    static GW::AgentID ItemID;
    static GW::AgentID tStoredItemID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsItemType()) { continue; }

        const auto AgentItem = agent->GetAsAgentItem();

        if (!AgentItem) { continue; }

        uint32_t ItemID = AgentItem->agent_id;
        const auto tAssignedTo = AgentItem->owner;

        if (!ItemID) { continue; }
        if ((tAssignedTo) && (tAssignedTo != GameVars.Target.Self.ID)) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(ItemID);

        if (!tempAgent) { continue; }


        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredItemID = ItemID;
        }
    }
    if (distance != GW::Constants::Range::Spellcast) { return tStoredItemID; }
    else {
        return 0;
    }
}

uint32_t HeroAI::TargetNearestNpc()
{
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;

    static GW::AgentID EnemyID;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Npc_Minipet) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }


        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if ((EnemyID == GameVars.Target.Self.ID)) { continue; }


        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != GW::Constants::Range::Earshot) { return tStoredEnemyID; }
    else {
        return 0;
    }

}

uint32_t HeroAI::TargetNearestSpirit()
{
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Earshot;

    static GW::AgentID EnemyID;
    static GW::AgentID tStoredEnemyID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tEnemyLiving = agent->GetAsAgentLiving();
        if (!tEnemyLiving) { continue; }
        if (tEnemyLiving->allegiance != GW::Constants::Allegiance::Spirit_Pet) { continue; }

        EnemyID = tEnemyLiving->agent_id;

        if (!EnemyID) { continue; }


        const auto tempAgent = GW::Agents::GetAgentByID(EnemyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        if ((EnemyID == GameVars.Target.Self.ID)) { continue; }


        const auto dist = GW::GetDistance(GameVars.Target.Self.Living->pos, tempAgent->pos);

        if (dist < distance) {
            distance = dist;
            tStoredEnemyID = EnemyID;
        }
    }
    if (distance != GW::Constants::Range::Earshot) { return tStoredEnemyID; }
    else {
        return 0;
    }
}

uint32_t  HeroAI::TargetLowestMinion() {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spellcast;
    static GW::AgentID AllyID = 0;
    static GW::AgentID tStoredAllyID = 0;
    float StoredLife = 2.0f;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tAllyLiving = agent->GetAsAgentLiving();
        if (!tAllyLiving) { continue; }
        if (tAllyLiving->allegiance != GW::Constants::Allegiance::Minion) { continue; }

        AllyID = tAllyLiving->agent_id;

        if (!AllyID) { continue; }

        const auto tempAgent = GW::Agents::GetAgentByID(AllyID);

        if (!tempAgent) { continue; }
        if (!tempAgent->GetAsAgentLiving()->GetIsAlive()) { continue; }

        const float vlife = tAllyLiving->hp;

            if (vlife < StoredLife)
            {
                StoredLife = vlife;
                tStoredAllyID = AllyID;
            }    

    }
    if (StoredLife != 2.0f) { return tStoredAllyID; }

    return 0;

}

GW::AgentID HeroAI::GetAppropiateTarget(uint32_t slot) {
    GW::AgentID vTarget = 0;
    /* --------- TARGET SELECTING PER SKILL --------------*/
    if (GameState.state.isTargettingEnabled()) {
        if (GameVars.SkillBar.Skills[slot].HasCustomData) {
            bool Strict = GameVars.SkillBar.Skills[slot].CustomData.Conditions.TargettingStrict;
            Strict = true;
            switch (GameVars.SkillBar.Skills[slot].CustomData.TargetAllegiance) {
            case Enemy:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyCaster:
                vTarget = GameVars.Target.NearestCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartial:
                vTarget = GameVars.Target.NearestMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialMelee:
                vTarget = GameVars.Target.NearestMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case EnemyMartialRanged:
                vTarget = GameVars.Target.NearestMartialRanged.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.Nearest.ID; }
                break;
            case Ally:
                vTarget = GameVars.Target.LowestAlly.ID;
                break;
            case AllyCaster:
                vTarget = GameVars.Target.LowestAllyCaster.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartial:
                vTarget = GameVars.Target.LowestAllyMartial.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialMelee:
                vTarget = GameVars.Target.LowestAllyMartialMelee.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case AllyMartialRanged:
                vTarget = GameVars.Target.LowestAllyMartialRanged.ID;
                if ((!vTarget) && (!Strict)) { vTarget = GameVars.Target.LowestAlly.ID; }
                break;
            case OtherAlly:
                if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
                    (GameVars.SkillBar.Skills[slot].CustomData.Nature == EnergyBuff)) {
                    vTarget = GameVars.Target.LowestEnergyOtherAlly.ID;
                }
                else { vTarget = GameVars.Target.LowestOtherAlly.ID; }
                break;
            case Self:
                vTarget = GameVars.Target.Self.ID;
                break;
            case DeadAlly:
                vTarget = GameVars.Target.DeadAlly.ID;
                break;
            case Spirit:
                vTarget = GameVars.Target.NearestSpirit.ID;
                break;
            case Minion:
                vTarget = GameVars.Target.LowestMinion.ID;
                break;
            case Corpse:
                vTarget = GameVars.Target.NearestCorpse.ID;
                break;
            default:
                vTarget = GameVars.Target.Called.ID;
                if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
                break;
                //  enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly,DeadAlly, Self, Corpse, Minion, Spirit, Pet };
            }
        }
        else {
            vTarget = GameVars.Target.Called.ID;
            if (!vTarget) { vTarget = GameVars.Target.Nearest.ID; }
        }
    }
    else
    {
        vTarget = GW::Agents::GetTargetId();
    }
    return vTarget;
}

void HeroAI::ChooseTarget() {
    if (GameState.state.isTargettingEnabled()) {
        const auto target = GW::Agents::GetTargetId();
        if (!target) {
            if (GameVars.Target.Called.ID) {
                GW::Agents::ChangeTarget(GameVars.Target.Called.Agent);
                if (GameVars.Target.Self.Living->weapon_type != 0) {
                    GW::Agents::InteractAgent(GameVars.Target.Called.Agent, false);
                }

            }
            else {
                GW::Agents::ChangeTarget(GameVars.Target.Nearest.Agent);
                if (GameVars.Target.Self.Living->weapon_type != 0) {
                    GW::Agents::InteractAgent(GameVars.Target.Nearest.Agent, false);
                }
            }
        }
    }
}
#endif // TARGETTING_H
