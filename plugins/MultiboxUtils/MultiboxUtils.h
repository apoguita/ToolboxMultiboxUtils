#pragma once

//#include <ToolboxPlugin.h>
#include <ToolboxUIPlugin.h>
#include <GWCA/GameEntities/Agent.h>
#include <GWCA/GameEntities/Party.h>
#include <GWCA/Managers/SkillbarMgr.h>
#include <GWCA/GameEntities/Skill.h>
#include "Aclapi.h"



class HeroAI : public ToolboxUIPlugin {
public:
    HeroAI()
    {
        can_show_in_main_window = true;
        show_title = true;
        can_collapse = true;
        can_close = false;
    }

    const char* Name() const override { return "HeroAI"; }
    const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    /**************SKILL BEAHVIOUR DECLARATION ***********************************/

    enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff,EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt }; 
    enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly,DeadAlly, Self, Corpse, Minion, Spirit, Pet }; 

    struct castConditions {
        bool IsAlive = true;
        bool HasCondition = false;
        bool HasDeepWound = false;
        bool HasCrippled = false;
        bool HasBleeding = false;
        bool HasPoison = false;
        bool HasWeaponSpell = false;
        bool HasEnchantment = false;
        bool HasHex = false;
        bool IsCasting = false;
        bool IsNockedDown = false;
        bool IsMoving = false;
        bool IsAttacking = false;
        bool IsHoldingItem = false;
        float LessLife = 0;
        float MoreLife = 0;
        float LessEnergy = 0;
        bool IsPartyWide = false;
        bool UniqueProperty = false;

    };
    castConditions castData[1000];

    struct CustomSkillData {
        GW::Constants::SkillID SkillID = GW::Constants::SkillID::No_Skill;
        GW::Constants::SkillType SkillType;
        SkillTarget  TargetAllegiance;
        SkillNature Nature = Offensive;
        castConditions Conditions;
    };

    static const int MaxSkillData = 1235;
    CustomSkillData SpecialSkills[MaxSkillData];

    // Ally_NonAttackable = 0x1, Neutral = 0x2, Enemy = 0x3, Spirit_Pet = 0x4, Minion = 0x5, Npc_Minipet = 0x6

    
    void serializeSpecialSkills(const std::string& filename);
    void deserializeSpecialSkills(const std::string& filename);

    static uint32_t GetPing();

    virtual void Update(float) override;
    void InitSkillData();
    int GetMemPosByID(GW::AgentID agentID);
    bool AreCastConditionsMet(uint32_t slot, GW::AgentID agent);
    bool doesSpiritBuffExists(GW::Constants::SkillID skillID);
    bool HasEffect(const GW::Constants::SkillID skillID, GW::AgentID agentID);

    GW::AgentID GetAppropiateTarget(uint32_t slot);
    bool IsReadyToCast(uint32_t slot); //fulfill all requirements to castx
    bool CastSkill(uint32_t slot);
    bool IsSkillready(uint32_t slot); // is recharged
    bool IsAllowedCast(); //time on casting+aftercast
    bool ThereIsSpaceInInventory();
    GW::AgentID PartyTargetIDChanged();
    float AngleChange();


    void ChooseTarget();
    uint32_t TargetNearestEnemyInAggro(bool IsBigAggro = false);
    uint32_t TargetLowestAllyInAggro();
    uint32_t TargetDeadAllyInAggro();
    uint32_t TargetNearestItem();
    int GetPartyNumber();


    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;   

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettings() override;
    
    

private:
    struct MapStruct {
        bool IsExplorable;
        bool IsLoading;
        bool IsOutpost;
        bool IsPartyLoaded;
    };
    struct AgentTargetStruct {
        GW::Agent* Agent;
        GW::AgentID ID;
        GW::AgentLiving* Living;
    };

    struct TargetStruct {
        AgentTargetStruct Self;
        AgentTargetStruct Called;
        AgentTargetStruct Nearest;
        AgentTargetStruct NearestAlly;
        AgentTargetStruct DeadAlly;
    };
    struct PartyStruct {
        GW::PartyInfo* PartyInfo;
        uint32_t LeaderID;
        GW::Agent* Leader;
        GW::AgentLiving* LeaderLiving;
        int SelfPartyNumber;
        bool IsLeaderAttacking;
        bool IsLeaderCasting;

    };
    struct CombatFieldStruct {
        bool InAggro;
        float DistanceFromLeader;
        bool IsSelfMoving;
        bool IsSelfCasting;
        bool IsSelfKnockedDwon;

    };
    struct SkillStruct {
        GW::SkillbarSkill Skill;
        GW::Skill Data;
        CustomSkillData CustomData;
        bool HasCustomData;
    };
    struct SkillBarStruct {
        GW::Skillbar* SkillBar;
        SkillStruct Skills[8];
        uint32_t SkillOrder[8];
        uint32_t Skillptr = 0;
    };
    struct GameVarsStruct {
        MapStruct Map;
        TargetStruct Target;
        PartyStruct Party;
        CombatFieldStruct CombatField;
        SkillBarStruct SkillBar;
        bool IsMemoryLoaded = false;
        float LastTimeSubscribed = 0.0f;
        bool RecentlySubscribed() { return ((clock() - LastTimeSubscribed) < 100000) ? true : false;}
    };

    
    GameVarsStruct GameVars;
    GameVarsStruct MemPlayers[8];

    struct ClockStruct {
        float LastTimeAccesed = 0.0f;
        bool CanAccess() { return ((clock() - LastTimeAccesed) > 100) ? true : false; }
    };
    ClockStruct FileAccessClock;

    // MEMORY VER 2
    void setFileSize(HANDLE hFile, DWORD size);
    HANDLE createMemoryMappedFile(HANDLE& hFileMapping);
    bool writeGameVarsToFile(HANDLE hFileMapping, int clientIndex, GameVarsStruct* gameVars);
    bool readGameVarsFromFile(HANDLE hFileMapping, int clientIndex, GameVarsStruct* gameVars);

    // MEMORY MAPPED FILE

    bool UpdateGameVars();

    struct PlayerMapping
    {
        uint32_t id;
        uint32_t party_idx;
    };

    static constexpr uint32_t QuestAcceptDialog(GW::Constants::QuestID quest) { return static_cast<int>(quest) << 8 | 0x800001; }
    static constexpr uint32_t QuestRewardDialog(GW::Constants::QuestID quest) { return static_cast<int>(quest) << 8 | 0x800007; }

    int fav_count = 0;
    std::vector<int> fav_index{};

    
};
