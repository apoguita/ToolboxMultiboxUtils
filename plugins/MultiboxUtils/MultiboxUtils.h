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

    bool WndProc(const UINT Message, const WPARAM wParam, const LPARAM lParam);
    bool OnMouseMove(const UINT, const WPARAM, const LPARAM lParam);

    /**************SKILL BEAHVIOUR DECLARATION ***********************************/

    enum SkillNature { Offensive, OffensiveCaster, OffensiveMartial, Enchantment_Removal, Healing, Hex_Removal, Condi_Cleanse, Buff,EnergyBuff, Neutral, SelfTargetted, Resurrection, Interrupt }; 
    enum SkillTarget { Enemy, EnemyCaster, EnemyMartial, Ally, AllyCaster, AllyMartial, OtherAlly,DeadAlly, Self, Corpse, Minion, Spirit, Pet }; 
    enum WeaponType { bow = 1, axe, hammer, daggers, scythe, spear, sword, wand = 10, staff1 = 12, staff2 = 14 }; // 1=bow, 2=axe, 3=hammer, 4=daggers, 5=scythe, 6=spear, 7=sWORD, 10=wand, 12=staff, 14=staff};

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
    uint32_t TargetNearestEnemyInAggro(bool IsBigAggro = false, SkillTarget Target = Enemy);
    uint32_t TargetLowestAllyInAggro(bool OtherAlly = false, bool CheckEnergy = false, SkillTarget Target = Ally);
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
        bool IsExplorable=false;
        bool IsLoading=false;
        bool IsOutpost = false;
        bool IsPartyLoaded = false;
    };
    struct AgentTargetStruct {
        GW::Agent* Agent =0;
        GW::AgentID ID =0;
        GW::AgentLiving* Living =0;
    };

    struct TargetStruct {
        AgentTargetStruct Self;
        AgentTargetStruct Called;
        AgentTargetStruct Nearest;
        AgentTargetStruct NearestMartial;
        AgentTargetStruct NearestCaster;
        AgentTargetStruct LowestAlly;
        AgentTargetStruct LowestAllyMartial;
        AgentTargetStruct LowestAllyCaster;
        AgentTargetStruct DeadAlly;
        AgentTargetStruct LowestOtherAlly;
        AgentTargetStruct LowestEnergyOtherAlly;
    };
    struct PartyStruct {
        GW::PartyInfo* PartyInfo = 0;
        uint32_t LeaderID = 0;
        GW::Agent* Leader = 0;
        GW::AgentLiving* LeaderLiving = 0;
        int SelfPartyNumber = -1;
        bool IsLeaderAttacking = false;
        bool IsLeaderCasting = false;

    };
    struct CombatFieldStruct {
        bool InAggro = false;
        float DistanceFromLeader = 0.0f;
        bool IsSelfMoving = false;
        bool IsSelfCasting = false;
        bool IsSelfKnockedDwon = false;

    };
    struct SkillStruct {
        GW::SkillbarSkill Skill;
        GW::Skill Data;
        CustomSkillData CustomData;
        bool HasCustomData = false;
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
        bool RecentlySubscribed() { return ((clock() - LastTimeSubscribed) < 1000) ? true : false;}
    };

    
    GameVarsStruct GameVars;

    struct MemSinglePlayer {
        GW::AgentID ID = 0;
        float Energy = 0.0f;
        float energyRegen = 0.0f;
        bool IsActive = false;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct MemPlayerStruct {
        MemSinglePlayer MemPlayers[8]; 
    };

    MemPlayerStruct MemCopy;
    
    MemPlayerStruct* MemData;
    
    //GameVarsStruct MemPlayers[8];

    struct ClockStruct {
        float LastTimeAccesed = 0.0f;
        bool CanAccess() { return ((clock() - LastTimeAccesed) > 100) ? true : false; }
    };
    ClockStruct FileAccessClock;

    //Memory ver 3
    HANDLE hMapFile;
    void InitializeSharedMemory(MemPlayerStruct*& pData);
    //End Memory Ver 3

    bool UpdateGameVars();

    //--------- mouse click coords implementation
    uintptr_t GetBasePointer();
    float* GetClickCoordsX();
    float* GetClickCoordsY();
    void SendCoords();
    static void CallSendCoords();

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
