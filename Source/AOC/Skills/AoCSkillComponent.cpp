#include "AoCSkillComponent.h"

UAoCSkillComponent::UAoCSkillComponent()
{
}

void UAoCSkillComponent::BeginPlay()
{
    Super::BeginPlay();
    // Initialize all skills to level 1
    for (uint8 i = 0; i < static_cast<uint8>(EAoCSkillType::MAX); ++i)
    {
        EAoCSkillType Skill = static_cast<EAoCSkillType>(i);
        if (!Skills.Contains(Skill))
        {
            FAoCSkillData Data;
            Data.Level = 1;
            Data.CurrentXP = 0.f;
            Data.XPToNextLevel = FAoCSkillData::GetXPForLevel(2);
            Skills.Add(Skill, Data);
        }
    }
}

void UAoCSkillComponent::AddXP(EAoCSkillType Skill, float Amount)
{
    if (!Skills.Contains(Skill)) return;
    
    FAoCSkillData& Data = Skills[Skill];
    Data.CurrentXP += Amount;
    OnSkillXPGained.Broadcast(Skill, Amount);
    
    // Check for level up
    while (Data.CurrentXP >= Data.XPToNextLevel && Data.Level < 200)
    {
        int32 OldLevel = Data.Level;
        Data.Level++;
        Data.CurrentXP -= Data.XPToNextLevel;
        Data.XPToNextLevel = FAoCSkillData::GetXPForLevel(Data.Level + 1) - FAoCSkillData::GetXPForLevel(Data.Level);
        OnSkillLevelUp.Broadcast(Skill, OldLevel, Data.Level);
    }
}

int32 UAoCSkillComponent::GetSkillLevel(EAoCSkillType Skill) const
{
    const FAoCSkillData* Data = Skills.Find(Skill);
    return Data ? Data->Level : 1;
}

float UAoCSkillComponent::GetSkillXP(EAoCSkillType Skill) const
{
    const FAoCSkillData* Data = Skills.Find(Skill);
    return Data ? Data->CurrentXP : 0.f;
}

float UAoCSkillComponent::GetXPToNextLevel(EAoCSkillType Skill) const
{
    const FAoCSkillData* Data = Skills.Find(Skill);
    return Data ? Data->XPToNextLevel : 100.f;
}

int32 UAoCSkillComponent::GetTotalLevel() const
{
    int32 Total = 0;
    for (const auto& Pair : Skills) Total += Pair.Value.Level;
    return Total;
}
