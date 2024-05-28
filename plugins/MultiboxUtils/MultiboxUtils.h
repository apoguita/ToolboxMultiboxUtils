#pragma once

//#include <ToolboxPlugin.h>
#include <ToolboxUIPlugin.h>
#include <GWCA/GameEntities/Agent.h>



class MultiboxUtils : public ToolboxUIPlugin {
public:
    MultiboxUtils()
    {
        can_show_in_main_window = true;
        show_title = true;
        can_collapse = true;
        can_close = false;
    }

    const char* Name() const override { return "MultiboxUtils"; }
    const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    virtual void Update(float) override;

    void CastSkill(uint32_t slot);
    bool IsSkillready(uint32_t slot);
    bool IsAllowedCast();
    bool ThereIsSpaceInInventory();
    GW::AgentID PartyTargetIDChanged();

    void SmartSelectTarget();
    uint32_t TargetNearestEnemyInAggro();
    uint32_t TargetNearestEnemyInBigAggro();
    uint32_t TargetNearestItem();
    int GetPartyNumber();


    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;   

    void Draw(IDirect3DDevice9* pDevice) override;
    void DrawSettings() override;

    
private:
    bool IsMapExplorable();

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
