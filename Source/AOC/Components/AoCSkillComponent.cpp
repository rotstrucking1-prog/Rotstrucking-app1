// Copyright Architect of Creation. All Rights Reserved.

#include "AoCSkillComponent.h"

UAoCSkillComponent::UAoCSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UAoCSkillComponent::GetSkillLevel(FName SkillName) const
{
	const int32* Val = PlayerSkills.Find(SkillName);
	return Val ? *Val : 0;
}

void UAoCSkillComponent::SetSkillLevel(FName SkillName, int32 Level)
{
	PlayerSkills.Add(SkillName, Level);
}

int32 UAoCSkillComponent::GetTotalLevel() const
{
	int32 Sum = 0;
	for (const auto& Pair : PlayerSkills)
	{
		Sum += Pair.Value;
	}
	return Sum;
}
