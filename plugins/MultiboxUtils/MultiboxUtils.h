#pragma once

#include <ToolboxPlugin.h>
#include <GWCA/GameEntities/Agent.h>



class MultiboxUtils : public ToolboxPlugin {
public:
    const char* Name() const override { return "MultiboxUtils"; }
    const char* Icon() const override { return ICON_FA_LOCK_OPEN; }

    virtual void Update(float) override;

    void CastSkill(uint32_t slot);
    bool IsSkillready(uint32_t slot);
    bool IsAllowedCast();

    void SmartSelectTarget();
    uint32_t TargetNearestEnemyInAggro();
    uint32_t TargetNearestItem();



    void Initialize(ImGuiContext*, ImGuiAllocFns, HMODULE) override;
    void SignalTerminate() override;
    bool CanTerminate() override;
    void Terminate() override;   

    
private:
    bool IsMapExplorable();

    struct PlayerMapping
    {
        uint32_t id;
        uint32_t party_idx;
    };

   

    
};
