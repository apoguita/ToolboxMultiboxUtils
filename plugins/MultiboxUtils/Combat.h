#pragma once

#include "Headers.h"

#ifndef COMBAT_H
#define COMBAT_H

bool HeroAI::IsSkillready(uint32_t slot) {
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { return false; }
    return true;
}

bool HeroAI::IsOOCSkill(uint32_t slot) {

    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsOutOfCombat) { return true; }

    switch (GameVars.SkillBar.Skills[slot].CustomData.SkillType) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }

    switch (GameVars.SkillBar.Skills[slot].Data.type) {
    case GW::Constants::SkillType::Form:
    case GW::Constants::SkillType::Preparation:
        return true;
        break;
    }

    switch (GameVars.SkillBar.Skills[slot].CustomData.Nature) {
    case Healing:
    case Hex_Removal:
    case Condi_Cleanse:
    case EnergyBuff:
    case Resurrection:
        return true;
        break;
    default:
        return false;
    }

    return false;
}

bool HeroAI::IsReadyToCast(uint32_t slot) {
    if (GameVars.Target.Self.Living->GetIsCasting()) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { GameState.combat.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((GameVars.Target.Self.Living->energy * GameVars.Target.Self.Living->max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { GameState.combat.IsSkillCasting = false; return false; }

    float hp_target = GameVars.SkillBar.Skills[slot].CustomData.Conditions.SacrificeHealth;
    float current_life = GameVars.Target.Self.Living->hp;
    if ((current_life < hp_target) && (GameVars.SkillBar.Skills[slot].Data.health_cost > 0.0f)) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    GW::AgentID vTarget = GetAppropiateTarget(slot);
    if (!vTarget) { return false; }

    //assassin strike chains
    const auto tempAgent = GW::Agents::GetAgentByID(vTarget);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }


    if ((GameVars.SkillBar.Skills[slot].Data.combo == 1) && ((tempAgentLiving->dagger_status != 0) && (tempAgentLiving->dagger_status != 3))) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 2) && (tempAgentLiving->dagger_status != 1)) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 3) && (tempAgentLiving->dagger_status != 2)) { return false; }


    if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
        (!AreCastConditionsMet(slot, vTarget))) {
        GameState.combat.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { GameState.combat.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, vTarget)) { GameState.combat.IsSkillCasting = false; return false; }

    if ((GameVars.Target.Self.Living->GetIsCasting()) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { GameState.combat.IsSkillCasting = false; return false; }

    return true;
}

bool HeroAI::IsReadyToCast(uint32_t slot , GW::AgentID vTarget) {
    if (GameVars.Target.Self.Living->GetIsCasting()) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.skill_id == GW::Constants::SkillID::No_Skill) { GameState.combat.IsSkillCasting = false; return false; }
    if (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0) { GameState.combat.IsSkillCasting = false; return false; }

    const auto current_energy = static_cast<uint32_t>((GameVars.Target.Self.Living->energy * GameVars.Target.Self.Living->max_energy));
    if (current_energy < GameVars.SkillBar.Skills[slot].Data.energy_cost) { GameState.combat.IsSkillCasting = false; return false; }

    float hp_target = GameVars.SkillBar.Skills[slot].CustomData.Conditions.SacrificeHealth;
    float current_life = GameVars.Target.Self.Living->hp;
    if ((current_life < hp_target) && (GameVars.SkillBar.Skills[slot].Data.health_cost > 0.0f)) { GameState.combat.IsSkillCasting = false; return false; }

    const auto enough_adrenaline =
        (GameVars.SkillBar.Skills[slot].Data.adrenaline == 0) ||
        (GameVars.SkillBar.Skills[slot].Data.adrenaline > 0 &&
            GameVars.SkillBar.Skills[slot].Skill.adrenaline_a >= GameVars.SkillBar.Skills[slot].Data.adrenaline);

    if (!enough_adrenaline) { GameState.combat.IsSkillCasting = false; return false; }

    //GW::AgentID vTarget = GetAppropiateTarget(slot);
    //if (!vTarget) { return false; }

    //assassin strike chains
    const auto tempAgent = GW::Agents::GetAgentByID(vTarget);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }


    if ((GameVars.SkillBar.Skills[slot].Data.combo == 1) && ((tempAgentLiving->dagger_status != 0) && (tempAgentLiving->dagger_status != 3))) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 2) && (tempAgentLiving->dagger_status != 1)) { return false; }
    if ((GameVars.SkillBar.Skills[slot].Data.combo == 3) && (tempAgentLiving->dagger_status != 2)) { return false; }


    if ((GameVars.SkillBar.Skills[slot].HasCustomData) &&
        (!AreCastConditionsMet(slot, vTarget))) {
        GameState.combat.IsSkillCasting = false; return false;
    }
    if (doesSpiritBuffExists(GameVars.SkillBar.Skills[slot].Skill.skill_id)) { GameState.combat.IsSkillCasting = false; return false; }

    if (HasEffect(GameVars.SkillBar.Skills[slot].Skill.skill_id, vTarget)) { GameState.combat.IsSkillCasting = false; return false; }

    if ((GameVars.Target.Self.Living->GetIsCasting()) && (GameVars.SkillBar.Skills[slot].Skill.GetRecharge() != 0)) { GameState.combat.IsSkillCasting = false; return false; }

    return true;
}

bool HeroAI::CastSkill(uint32_t slot) {
    GW::AgentID vTarget = 0;

    vTarget = GetAppropiateTarget(slot);
    if (!vTarget) { return false; } //because of forced targetting

    if (IsReadyToCast(slot, vTarget))
    {
        GameState.combat.IntervalSkillCasting = GameVars.SkillBar.Skills[slot].Data.activation + GameVars.SkillBar.Skills[slot].Data.aftercast + 750.0f;
        GameState.combat.LastActivity.reset();

        GW::SkillbarMgr::UseSkill(slot, vTarget);
        ChooseTarget();
        return true;
    }
    return false;
}

bool HeroAI::InCastingRoutine() {
    return GameState.combat.IsSkillCasting;
}


bool HeroAI::doesSpiritBuffExists(GW::Constants::SkillID skillID) {
    const auto agents = GW::Agents::GetAgentArray();

    float distance = GW::Constants::Range::Spirit;
    static GW::AgentID SpiritID;
    static GW::AgentID tStoredSpiritID = 0;

    for (const GW::Agent* agent : *agents)
    {
        if (!agent) { continue; }
        if (!agent->GetIsLivingType()) { continue; }
        const auto tSpiritLiving = agent->GetAsAgentLiving();
        if (!tSpiritLiving) { continue; }
        if (tSpiritLiving->allegiance != GW::Constants::Allegiance::Spirit_Pet) { continue; }

        SpiritID = tSpiritLiving->agent_id;
        if (!SpiritID) { continue; }

        if (!tSpiritLiving->IsNPC()) { continue; }
        if (!tSpiritLiving->GetIsAlive()) { continue; }

        const auto SpiritModelID = tSpiritLiving->player_number;

        if ((SpiritModelID == 2882) && (skillID == GW::Constants::SkillID::Frozen_Soil)) { return true; }
        if ((SpiritModelID == 4218) && (skillID == GW::Constants::SkillID::Life)) { return true; }
        if ((SpiritModelID == 4227) && (skillID == GW::Constants::SkillID::Bloodsong)) { return true; }
        if ((SpiritModelID == 4229) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //anger
        if ((SpiritModelID == 4230) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //hate
        if ((SpiritModelID == 4231) && (skillID == GW::Constants::SkillID::Signet_of_Spirits)) { return true; } //suffering
        if ((SpiritModelID == 5720) && (skillID == GW::Constants::SkillID::Anguish)) { return true; }
        if ((SpiritModelID == 4225) && (skillID == GW::Constants::SkillID::Disenchantment)) { return true; }
        if ((SpiritModelID == 4221) && (skillID == GW::Constants::SkillID::Dissonance)) { return true; }
        if ((SpiritModelID == 4214) && (skillID == GW::Constants::SkillID::Pain)) { return true; }
        if ((SpiritModelID == 4213) && (skillID == GW::Constants::SkillID::Shadowsong)) { return true; }
        if ((SpiritModelID == 4228) && (skillID == GW::Constants::SkillID::Wanderlust)) { return true; }
        if ((SpiritModelID == 5723) && (skillID == GW::Constants::SkillID::Vampirism)) { return true; }
        if ((SpiritModelID == 5854) && (skillID == GW::Constants::SkillID::Agony)) { return true; }
        if ((SpiritModelID == 4217) && (skillID == GW::Constants::SkillID::Displacement)) { return true; }
        if ((SpiritModelID == 4222) && (skillID == GW::Constants::SkillID::Earthbind)) { return true; }
        if ((SpiritModelID == 5721) && (skillID == GW::Constants::SkillID::Empowerment)) { return true; }
        if ((SpiritModelID == 4219) && (skillID == GW::Constants::SkillID::Preservation)) { return true; }
        if ((SpiritModelID == 5719) && (skillID == GW::Constants::SkillID::Recovery)) { return true; }
        if ((SpiritModelID == 4220) && (skillID == GW::Constants::SkillID::Recuperation)) { return true; }
        if ((SpiritModelID == 5853) && (skillID == GW::Constants::SkillID::Rejuvenation)) { return true; }
        if ((SpiritModelID == 4223) && (skillID == GW::Constants::SkillID::Shelter)) { return true; }
        if ((SpiritModelID == 4216) && (skillID == GW::Constants::SkillID::Soothing)) { return true; }
        if ((SpiritModelID == 4224) && (skillID == GW::Constants::SkillID::Union)) { return true; }
        if ((SpiritModelID == 4215) && (skillID == GW::Constants::SkillID::Destruction)) { return true; }
        if ((SpiritModelID == 4226) && (skillID == GW::Constants::SkillID::Restoration)) { return true; }
        if ((SpiritModelID == 2884) && (skillID == GW::Constants::SkillID::Winds)) { return true; }
        if ((SpiritModelID == 4239) && (skillID == GW::Constants::SkillID::Brambles)) { return true; }
        if ((SpiritModelID == 4237) && (skillID == GW::Constants::SkillID::Conflagration)) { return true; }
        if ((SpiritModelID == 2885) && (skillID == GW::Constants::SkillID::Energizing_Wind)) { return true; }
        if ((SpiritModelID == 4236) && (skillID == GW::Constants::SkillID::Equinox)) { return true; }
        if ((SpiritModelID == 2876) && (skillID == GW::Constants::SkillID::Edge_of_Extinction)) { return true; }
        if ((SpiritModelID == 4238) && (skillID == GW::Constants::SkillID::Famine)) { return true; }
        if ((SpiritModelID == 2883) && (skillID == GW::Constants::SkillID::Favorable_Winds)) { return true; }
        if ((SpiritModelID == 2878) && (skillID == GW::Constants::SkillID::Fertile_Season)) { return true; }
        if ((SpiritModelID == 2877) && (skillID == GW::Constants::SkillID::Greater_Conflagration)) { return true; }
        if ((SpiritModelID == 5715) && (skillID == GW::Constants::SkillID::Infuriating_Heat)) { return true; }
        if ((SpiritModelID == 4232) && (skillID == GW::Constants::SkillID::Lacerate)) { return true; }
        if ((SpiritModelID == 2888) && (skillID == GW::Constants::SkillID::Muddy_Terrain)) { return true; }
        if ((SpiritModelID == 2887) && (skillID == GW::Constants::SkillID::Natures_Renewal)) { return true; }
        if ((SpiritModelID == 4234) && (skillID == GW::Constants::SkillID::Pestilence)) { return true; }
        if ((SpiritModelID == 2881) && (skillID == GW::Constants::SkillID::Predatory_Season)) { return true; }
        if ((SpiritModelID == 2880) && (skillID == GW::Constants::SkillID::Primal_Echoes)) { return true; }
        if ((SpiritModelID == 2886) && (skillID == GW::Constants::SkillID::Quickening_Zephyr)) { return true; }
        if ((SpiritModelID == 5718) && (skillID == GW::Constants::SkillID::Quicksand)) { return true; }
        if ((SpiritModelID == 5717) && (skillID == GW::Constants::SkillID::Roaring_Winds)) { return true; }
        if ((SpiritModelID == 2879) && (skillID == GW::Constants::SkillID::Symbiosis)) { return true; }
        if ((SpiritModelID == 5716) && (skillID == GW::Constants::SkillID::Toxicity)) { return true; }
        if ((SpiritModelID == 4235) && (skillID == GW::Constants::SkillID::Tranquility)) { return true; }
        if ((SpiritModelID == 2874) && (skillID == GW::Constants::SkillID::Winter)) { return true; }
        if ((SpiritModelID == 2875) && (skillID == GW::Constants::SkillID::Winnowing)) { return true; }
    }
    return false;
}

namespace GW {
    namespace Effects {
        Effect* GetAgentEffectBySkillId(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* effects = GetAgentEffects(agent_id);
            if (!effects) return nullptr;
            for (auto& effect : *effects) {
                if (effect.skill_id == skill_id)
                    return &effect;
            }
            return nullptr;
        }

        Buff* GetAgentBuffBySkillId(uint32_t agent_id, Constants::SkillID skill_id) {
            auto* buffs = GetAgentBuffs(agent_id);
            if (!buffs) return nullptr;
            for (auto& buff : *buffs) {
                if (buff.skill_id == skill_id)
                    return &buff;
            }
            return nullptr;
        }
    }
}

bool HeroAI::HasEffect(const GW::Constants::SkillID skillID, GW::AgentID agentID)
{
    const auto* effect = GW::Effects::GetAgentEffectBySkillId(agentID, skillID);
    const auto* buff = GW::Effects::GetAgentBuffBySkillId(agentID, skillID);

    if ((effect) || (buff)) { return true; }

    int Skillptr = 0;

    const auto tempAgent = GW::Agents::GetAgentByID(agentID);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

    Skillptr = CustomSkillData.GetPtrBySkillID(skillID);
    if (!Skillptr) { return false; }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);

    if ((vSkill.SkillType == GW::Constants::SkillType::WeaponSpell) && (tempAgentLiving->GetIsWeaponSpelled())) { return true; }

    GW::Skill skill = *GW::SkillbarMgr::GetSkillConstantData(skillID);

    if ((skill.type == GW::Constants::SkillType::WeaponSpell) && (tempAgentLiving->GetIsWeaponSpelled())) { return true; }

    return false;
}


bool HeroAI::AreCastConditionsMet(uint32_t slot, GW::AgentID agentID) {

    int Skillptr = 0;
    int ptr = 0;
    int NoOfFeatures = 0;
    int featurecount = 0;

    const auto tempAgent = GW::Agents::GetAgentByID(agentID);
    if (!tempAgent) { return false; }
    const auto tempAgentLiving = tempAgent->GetAsAgentLiving();
    if (!tempAgentLiving) { return false; }

    Skillptr = CustomSkillData.GetPtrBySkillID(GameVars.SkillBar.Skills[slot].Skill.skill_id);
    if (!Skillptr) { return true; }

    if (GameVars.SkillBar.Skills[slot].CustomData.Nature == Resurrection) {
        if (tempAgentLiving->GetIsDead()) {
            //if (tempAgentLiving->allegiance == GW::Constants::Allegiance::Ally_NonAttackable) {
            return true;
            //}
        }
        else { return false; }
    }

    CustomSkillClass::CustomSkillDatatype vSkill = CustomSkillData.GetSkillByPtr(Skillptr);
    CustomSkillClass::CustomSkillDatatype vSkillSB = GameVars.SkillBar.Skills[slot].CustomData;

    float SelfEnergy = GameVars.Target.Self.Living->energy;
    float SelfHp = GameVars.Target.Self.Living->hp;


    if (GameVars.SkillBar.Skills[slot].CustomData.Conditions.UniqueProperty) {
        switch (GameVars.SkillBar.Skills[slot].Skill.skill_id) {
        case GW::Constants::SkillID::Energy_Drain:
        case GW::Constants::SkillID::Energy_Tap:
        case GW::Constants::SkillID::Ether_Lord:
            return (SelfEnergy < vSkillSB.Conditions.LessEnergy) ? true : false;
            break;
        case GW::Constants::SkillID::Glowing_Signet:
            return ((SelfEnergy < vSkillSB.Conditions.LessEnergy) && (GameVars.Target.NearestSpirit.ID)) ? true : false;
            break;
        case GW::Constants::SkillID::Essence_Strike:
            return ((SelfEnergy < vSkillSB.Conditions.LessEnergy) && (tempAgentLiving->GetIsConditioned())) ? true : false;
            break;
        case GW::Constants::SkillID::Clamor_of_Souls:
            return ((GameVars.Target.Self.Living->weapon_type == 0) && (SelfEnergy < vSkillSB.Conditions.LessEnergy)) ? true : false;
            break;
        case GW::Constants::SkillID::Waste_Not_Want_Not:
            return (!tempAgentLiving->GetIsCasting() && !tempAgentLiving->GetIsAttacking() && (SelfEnergy < vSkillSB.Conditions.LessEnergy)) ? true : false;
            break;
        case GW::Constants::SkillID::Mend_Body_and_Soul:
            return  ((tempAgentLiving->hp < vSkillSB.Conditions.LessLife) || (tempAgentLiving->GetIsConditioned())) ? true : false;
            break;
        case GW::Constants::SkillID::Grenths_Balance:
            return  ((SelfHp < tempAgentLiving->hp) && (SelfHp < vSkillSB.Conditions.LessLife)) ? true : false;
            break;
        case GW::Constants::SkillID::Deaths_Retreat:
            return  (SelfHp < tempAgentLiving->hp) ? true : false;
            break;
        case GW::Constants::SkillID::Desperate_Strike:
        case GW::Constants::SkillID::Spirit_to_Flesh:
            return  (SelfHp < vSkillSB.Conditions.LessLife) ? true : false;
            break;
        case GW::Constants::SkillID::Plague_Sending:
        case GW::Constants::SkillID::Plague_Signet:
        case GW::Constants::SkillID::Plague_Touch:
            return (GameVars.Target.Self.Living->GetIsConditioned()) ? true : false;
            break;
        case GW::Constants::SkillID::Golden_Fang_Strike:
        case GW::Constants::SkillID::Golden_Fox_Strike:
        case GW::Constants::SkillID::Golden_Lotus_Strike:
        case GW::Constants::SkillID::Golden_Phoenix_Strike:
        case GW::Constants::SkillID::Golden_Skull_Strike:
            return (GameVars.Target.Self.Living->GetIsEnchanted()) ? true : false;
            break;
        case GW::Constants::SkillID::Brutal_Weapon:
            return (!tempAgentLiving->GetIsEnchanted()) ? true : false;
            break;
        case GW::Constants::SkillID::Signet_of_Removal:
            return ((!tempAgentLiving->GetIsEnchanted()) && ((tempAgentLiving->GetIsConditioned() || tempAgentLiving->GetIsHexed()))) ? true : false;
            break;
        case GW::Constants::SkillID::Dwaynas_Kiss:
        case GW::Constants::SkillID::Unnatural_Signet:
        case GW::Constants::SkillID::Toxic_Chill:
            return  ((tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted())) ? true : false;
            break;
        case GW::Constants::SkillID::Discord:
            return (tempAgentLiving->GetIsConditioned() && (tempAgentLiving->GetIsHexed() || tempAgentLiving->GetIsEnchanted())) ? true : false;
            break; 
        case GW::Constants::SkillID::Empathic_Removal:
        case GW::Constants::SkillID::Iron_Palm:
        case GW::Constants::SkillID::Melandrus_Resilience:
        case GW::Constants::SkillID::Necrosis:
        case GW::Constants::SkillID::Peace_and_Harmony: //same conditionsas below
        case GW::Constants::SkillID::Purge_Signet:
        case GW::Constants::SkillID::Resilient_Weapon:
            return (tempAgentLiving->GetIsConditioned() || tempAgentLiving->GetIsHexed()) ? true : false;
            break;
        case GW::Constants::SkillID::Gaze_from_Beyond:
        case GW::Constants::SkillID::Spirit_Burn:
        case GW::Constants::SkillID::Signet_of_Ghostly_Might:
            return (GameVars.Target.NearestSpirit.ID) ? true : false;
            break;
        default:
            return true;
        }
    }

    

    featurecount += (vSkill.Conditions.IsAlive) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCondition) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasDeepWound) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasCrippled) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasBleeding) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasPoison) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasWeaponSpell) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasEnchantment) ? 1 : 0;
    featurecount += (vSkill.Conditions.HasHex) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsCasting) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsNockedDown) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsMoving) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsAttacking) ? 1 : 0;
    featurecount += (vSkill.Conditions.IsHoldingItem) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.MoreLife > 0.0f) ? 1 : 0;
    featurecount += (vSkill.Conditions.LessEnergy > 0.0f) ? 1 : 0;

    if (tempAgentLiving->GetIsAlive() && vSkillSB.Conditions.IsAlive) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsConditioned() && vSkillSB.Conditions.HasCondition) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsDeepWounded() && vSkillSB.Conditions.HasDeepWound) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsCrippled() && vSkillSB.Conditions.HasCrippled) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsBleeding() && vSkillSB.Conditions.HasBleeding) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsPoisoned() && vSkillSB.Conditions.HasPoison) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsWeaponSpelled() && vSkillSB.Conditions.HasWeaponSpell) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsEnchanted() && vSkillSB.Conditions.HasEnchantment) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsHexed() && vSkillSB.Conditions.HasHex) { NoOfFeatures++; }

    if (tempAgentLiving->GetIsKnockedDown() && vSkillSB.Conditions.IsNockedDown) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsMoving() && vSkillSB.Conditions.IsMoving) { NoOfFeatures++; }
    if (tempAgentLiving->GetIsAttacking() && vSkillSB.Conditions.IsAttacking) { NoOfFeatures++; }
    if (tempAgentLiving->weapon_type == 0 && vSkillSB.Conditions.IsHoldingItem) { NoOfFeatures++; }

    if ((vSkillSB.Conditions.LessLife != 0.0f) && (tempAgentLiving->hp < vSkillSB.Conditions.LessLife)) { NoOfFeatures++; }
    if ((vSkillSB.Conditions.MoreLife != 0.0f) && (tempAgentLiving->hp > vSkillSB.Conditions.MoreLife)) { NoOfFeatures++; }


    //ENERGY CHECKS
    MemMgr.MutexAquire();
    int PIndex = GetMemPosByID(agentID);

    if (PIndex >= 0) {
        if ((MemMgr.MemData->MemPlayers[PIndex].IsActive) &&
            (GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy != 0.0f) &&
            (MemMgr.MemData->MemPlayers[PIndex].Energy < GameVars.SkillBar.Skills[slot].CustomData.Conditions.LessEnergy)
            ) {
            NoOfFeatures++;
        }
    }
    MemMgr.MutexRelease();



    const auto TargetSkill = GW::SkillbarMgr::GetSkillConstantData(static_cast<GW::Constants::SkillID>(tempAgentLiving->skill));

    if (tempAgentLiving->GetIsCasting() && GameVars.SkillBar.Skills[slot].CustomData.Conditions.IsCasting && TargetSkill && TargetSkill->activation > 0.250f) { NoOfFeatures++; }

    if (featurecount == NoOfFeatures) { return true; }

    return false;
}


#endif // COMBAT_H
