#pragma once

#include <ToolboxUIPlugin.h>

#include "Headers.h"


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
    bool OnMouseUp(const UINT, const WPARAM, const LPARAM lParam);
    virtual void Update(float) override;

    /**************SKILL BEAHVIOUR DECLARATION ***********************************/
    float oldAngle;

   
    CustomSkillClass CustomSkillData;

    bool AreCastConditionsMet(uint32_t slot, GW::AgentID agent);

    
    static uint32_t GetPing() { return 750.0f; }
 
    int GetMemPosByID(GW::AgentID agentID);
    
    bool doesSpiritBuffExists(GW::Constants::SkillID skillID);
    bool HasEffect(const GW::Constants::SkillID skillID, GW::AgentID agentID);

    GW::AgentID GetAppropiateTarget(uint32_t slot);
    bool IsReadyToCast(uint32_t slot); //fulfill all requirements to castx
    bool CastSkill(uint32_t slot);
    bool IsSkillready(uint32_t slot); // is recharged
    //bool IsAllowedCast(); //time on casting+aftercast
    bool InCastingRoutine();
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
        float DistanceFromLeader = 0.0f;
        bool InAggro = false;
        bool FlaggingStatus[8];
        GW::Vec2f FlagCoords[8];

    };
    struct SkillStruct {
        GW::SkillbarSkill Skill;
        GW::Skill Data;
        CustomSkillClass::CustomSkillDatatype CustomData;
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
        SkillBarStruct SkillBar;
        bool IsMemoryLoaded = false;
        //float LastTimeSubscribed = 0.0f;
        //bool RecentlySubscribed() { return ((clock() - LastTimeSubscribed) < 1000) ? true : false;}
    };

    
    GameVarsStruct GameVars;
    MemMgrClass MemMgr;
    
    bool UpdateGameVars();

   
    int fav_count = 0;
    std::vector<int> fav_index{};

    void do_all_mouse_move(); //no longer used click to world pos
};
