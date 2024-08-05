#ifndef HeroAI_H
#define HeroAI_H

#include <ToolboxUIPlugin.h>

#include "Headers.h"

GameStateClass GameState;
GameStateClass ControlAll;


static GW::AgentID oldCalledTarget;

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
    bool IsReadyToCast(uint32_t slot);
    bool IsReadyToCast(uint32_t slot, GW::AgentID vTarget); //fulfill all requirements to castx
    bool CastSkill(uint32_t slot);
    bool IsSkillready(uint32_t slot); // is recharged
    bool IsOOCSkill(uint32_t slot);
    //bool IsAllowedCast(); //time on casting+aftercast
    bool InCastingRoutine();
    bool ThereIsSpaceInInventory();
    GW::AgentID PartyTargetIDChanged();
    float AngleChange();


    void ChooseTarget();
    //uint32_t TargetNearestEnemyInAggro(bool IsBigAggro = false, SkillTarget Target = Enemy);
    uint32_t TargetNearestEnemy(bool IsBigAggro = false);
    uint32_t TargetNearestEnemyCaster(bool IsBigAggro = false);
    uint32_t TargetNearestEnemyMartial(bool IsBigAggro = false);
    uint32_t TargetNearestEnemyMelee(bool IsBigAggro = false);
    uint32_t TargetNearestEnemyRanged(bool IsBigAggro = false);
    //uint32_t TargetLowestAllyInAggro(bool OtherAlly = false, bool CheckEnergy = false, SkillTarget Target = Ally);
    uint32_t TargetLowestAlly(bool OtherAlly = false);
    uint32_t TargetLowestAllyEnergy(bool OtherAlly = false);
    uint32_t TargetLowestAllyCaster(bool OtherAlly = false);
    uint32_t TargetLowestAllyMartial(bool OtherAlly = false);
    uint32_t TargetLowestAllyMelee(bool OtherAlly = false);
    uint32_t TargetLowestAllyRanged(bool OtherAlly = false);
    uint32_t TargetDeadAllyInAggro();
    uint32_t TargetNearestItem();
    uint32_t TargetNearestNpc();
    uint32_t TargetNearestSpirit();
    uint32_t TargetLowestMinion();
    uint32_t TargetNearestCorpse();
    int GetPartyNumber();
    bool existsInParty(uint32_t plyer_id);

    float CalculatePathDistance(float targetX, float targetY, ImDrawList* drawList = 0, bool draw = false);
    bool IsPositionOccupied(float x, float y);

    void MoveAway();

    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettings() override;

    GlobalMouseClass GlobalMouse;
    bool IsFlagged(int hero);

    /////////////////////////// PATHING//////////////////////
    float GridSize = 80.0f;
    GW::Vec3f GetSquareCenter(int q, int r);
    void DrawSquare(ImDrawList* drawList, int q, int r, ImU32 color, float thickness);
    bool IsSquareValid(int q, int r);
    bool IsSquareOccupied(int q, int r);
    void RenderGridAroundPlayer(ImDrawList* drawList);
    bool IsColliding(const GW::GamePos& position, const GW::GamePos& agentpos);


private:
    struct MapStruct {
        bool IsExplorable = false;
        bool IsLoading = false;
        bool IsOutpost = false;
        bool IsPartyLoaded = false;
    };
    struct AgentTargetStruct {
        GW::Agent* Agent = 0;
        GW::AgentID ID = 0;
        GW::AgentLiving* Living = 0;
    };

    struct TargetStruct {
        AgentTargetStruct Self;
        AgentTargetStruct Called;
        AgentTargetStruct Nearest;
        AgentTargetStruct NearestMartial;
        AgentTargetStruct NearestMartialMelee;
        AgentTargetStruct NearestMartialRanged;
        AgentTargetStruct NearestCaster;
        AgentTargetStruct LowestAlly;
        AgentTargetStruct LowestAllyMartial;
        AgentTargetStruct LowestAllyMartialMelee;
        AgentTargetStruct LowestAllyMartialRanged;
        AgentTargetStruct LowestAllyCaster;
        AgentTargetStruct DeadAlly;
        AgentTargetStruct LowestOtherAlly;
        AgentTargetStruct LowestEnergyOtherAlly;
        AgentTargetStruct NearestSpirit;
        AgentTargetStruct LowestMinion;
        AgentTargetStruct NearestCorpse;
    };
    struct PartyStruct {
        GW::PartyInfo* PartyInfo = 0;
        uint32_t LeaderID = 0;
        GW::Agent* Leader = 0;
        GW::AgentLiving* LeaderLiving = 0;
        int SelfPartyNumber = -1;
        float DistanceFromLeader = 0.0f;
        bool InAggro = false;
        bool Flags[8];
        bool IsFlagged(int hero) { return Flags[hero]; }
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

    void DrawTransparentPanel();
    void UnflagAllHeroes();
    void Flag(int hero);
    bool IsHeroFlagged(int hero);
    bool IsMelee(GW::AgentID AgentID);
    bool IsMartial(GW::AgentID AgentID);
};

#endif // HeroAI_H
