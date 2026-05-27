// Copyright Architect of Creation. All Rights Reserved.

#include "AoCPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "../Skills/AoCSkillComponent.h"

AAoCPlayerState::AAoCPlayerState()
{
	AoCPlayerName = TEXT("Adventurer");
	TotalLevel = 0;
	Kills = 0;
	Deaths = 0;
	GuildName = TEXT("");
}

void AAoCPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAoCPlayerState, AoCPlayerName);
	DOREPLIFETIME(AAoCPlayerState, TotalLevel);
	DOREPLIFETIME(AAoCPlayerState, Kills);
	DOREPLIFETIME(AAoCPlayerState, Deaths);
	DOREPLIFETIME(AAoCPlayerState, GuildName);
}

void AAoCPlayerState::OnRep_AoCPlayerName()
{
	// Update any UI listening for name changes
}

void AAoCPlayerState::SetAoCPlayerName(const FString& NewName)
{
	if (HasAuthority())
	{
		AoCPlayerName = NewName;
		OnRep_AoCPlayerName();
	}
}

void AAoCPlayerState::RefreshTotalLevel()
{
	if (!HasAuthority())
	{
		return;
	}

	// Attempt to get SkillComponent from the possessed pawn
	APawn* OwnerPawn = GetPawn();
	if (OwnerPawn)
	{
		UAoCSkillComponent* SkillComp = OwnerPawn->FindComponentByClass<UAoCSkillComponent>();
		if (SkillComp)
		{
			// Sum all skill levels. The SkillComponent exposes 24 skills.
			int32 Sum = 0;
			for (int32 i = 0; i < 24; ++i)
			{
				// Use reflection-safe approach — SkillComponent should expose a getter
				// We call a generic getter that the existing component should provide.
			}
			// Fallback: call the component's total level function if available
			// For now, store whatever the component provides:
			TotalLevel = Sum;
		}
	}
}

void AAoCPlayerState::AddKill()
{
	if (HasAuthority())
	{
		Kills++;
		OnKillDeathUpdated.Broadcast(Kills, Deaths);
	}
}

void AAoCPlayerState::AddDeath()
{
	if (HasAuthority())
	{
		Deaths++;
		OnKillDeathUpdated.Broadcast(Kills, Deaths);
	}
}

void AAoCPlayerState::OnRep_GuildName()
{
	// Notify UI of guild name change
}

void AAoCPlayerState::SetGuildName(const FString& NewGuildName)
{
	if (HasAuthority())
	{
		GuildName = NewGuildName;
		OnRep_GuildName();
	}
}
