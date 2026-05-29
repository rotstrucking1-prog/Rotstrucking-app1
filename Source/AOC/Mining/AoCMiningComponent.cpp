// AoCMiningComponent.cpp
// Architect of Creation - Mining Component Implementation

#include "AoCMiningComponent.h"
#include "AoCVoxelWorld.h"
#include "AoCVoxelChunk.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAoCMiningComponent::UAoCMiningComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	MiningSkill = 0.f;
	CurrentStamina = 100.f;
	MaxStamina = 100.f;
	BaseStaminaCostPerHit = 10.f;
	ToolQuality = 50.f;
	BaseMiningTime = 3.f;
	XPPerHit = 5.f;
	XPPerOreExtracted = 10.f;
	TunnelWidth = 200.f;   // 2m
	TunnelHeight = 250.f;  // 2.5m

	bIsMining = false;
	CurrentAction = EMiningAction::None;
	TargetLocation = FVector::ZeroVector;
	TargetMaterial = EVoxelMaterial::Air;
	MiningProgress = 0.f;
	VoxelWorld = nullptr;
	MiningXP = 0.f;
}

void UAoCMiningComponent::BeginPlay()
{
	Super::BeginPlay();

	// Try to find the voxel world in the level
	FindVoxelWorld();
}

void UAoCMiningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update mining progress for UI display
	if (bIsMining)
	{
		const float MiningTime = GetEffectiveMiningTime();
		if (MiningTime > 0.f)
		{
			const UWorld* World = GetWorld();
			if (World && World->GetTimerManager().IsTimerActive(MiningTimerHandle))
			{
				const float Elapsed = World->GetTimerManager().GetTimerElapsed(MiningTimerHandle);
				MiningProgress = FMath::Clamp(Elapsed / MiningTime, 0.f, 1.f);
			}
		}
	}
}

// ─── Core Mining Actions ─────────────────────────────────────────────────────

bool UAoCMiningComponent::StartMining(FVector InTargetLocation, EVoxelMaterial InTargetMaterial)
{
	if (bIsMining)
	{
		return false;
	}

	if (!HasPickaxeEquipped())
	{
		return false;
	}

	if (!HasEnoughStamina())
	{
		return false;
	}

	if (!VoxelWorld)
	{
		FindVoxelWorld();
		if (!VoxelWorld)
		{
			return false;
		}
	}

	// Verify the target voxel exists and is solid
	const FVoxelData TargetVoxel = VoxelWorld->GetVoxelAt(InTargetLocation);
	if (TargetVoxel.IsAir())
	{
		return false;
	}

	TargetLocation = InTargetLocation;
	TargetMaterial = InTargetMaterial;
	bIsMining = true;
	CurrentAction = TargetVoxel.IsOre() ? EMiningAction::MineOre : EMiningAction::TunnelForward;
	MiningProgress = 0.f;

	// Start mining timer
	const float MiningTime = GetEffectiveMiningTime();
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			MiningTimerHandle,
			this,
			&UAoCMiningComponent::OnMiningTick,
			MiningTime,
			false // Not looping — each hit is a separate trigger
		);
	}

	return true;
}

void UAoCMiningComponent::StopMining()
{
	if (!bIsMining)
	{
		return;
	}

	bIsMining = false;
	CurrentAction = EMiningAction::None;
	MiningProgress = 0.f;
	TargetMaterial = EVoxelMaterial::Air;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(MiningTimerHandle);
	}
}

void UAoCMiningComponent::OnMiningTick()
{
	if (!bIsMining || !VoxelWorld)
	{
		StopMining();
		return;
	}

	// Consume stamina
	if (!ConsumeStamina(GetStaminaCost()))
	{
		StopMining();
		return;
	}

	// Get current voxel at target
	const FVoxelData TargetVoxel = VoxelWorld->GetVoxelAt(TargetLocation);

	// Fire hit delegate
	OnMiningHit.Broadcast(TargetLocation, TargetVoxel.Material);

	if (TargetVoxel.IsOre())
	{
		// Mine ore: extract and report
		FMiningResult Result = MineOre(TargetLocation);
		OnMiningComplete.Broadcast(Result);

		if (Result.bSuccess)
		{
			OnOreExtracted.Broadcast(Result.MinedMaterial, Result.Quantity, Result.Quality);
		}
	}
	else if (TargetVoxel.IsSolid())
	{
		// Regular mining: carve at target
		VoxelWorld->CarveAt(TargetLocation, VOXEL_SIZE * 0.6f);

		FMiningResult Result;
		Result.MinedMaterial = TargetVoxel.Material;
		Result.Quality = TargetVoxel.Quality;
		Result.Quantity = 1;
		Result.XPGained = XPPerHit;
		Result.bSuccess = true;

		GainMiningXP(XPPerHit);
		OnMiningComplete.Broadcast(Result);
	}
	else
	{
		// Target is air — nothing to mine
		StopMining();
		return;
	}

	// Check if target is now depleted
	const FVoxelData PostMineVoxel = VoxelWorld->GetVoxelAt(TargetLocation);
	if (PostMineVoxel.IsAir())
	{
		StopMining();
		return;
	}

	// Continue mining with next hit timer
	MiningProgress = 0.f;
	const float MiningTime = GetEffectiveMiningTime();
	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().SetTimer(
			MiningTimerHandle,
			this,
			&UAoCMiningComponent::OnMiningTick,
			MiningTime,
			false
		);
	}
}

// ─── Tunneling ───────────────────────────────────────────────────────────────

bool UAoCMiningComponent::TunnelForward()
{
	return CarveTunnelSection(0.f);
}

bool UAoCMiningComponent::TunnelDown()
{
	return CarveTunnelSection(-50.f); // -0.5m
}

bool UAoCMiningComponent::TunnelUp()
{
	return CarveTunnelSection(50.f); // +0.5m
}

bool UAoCMiningComponent::CarveTunnelSection(float VerticalOffset)
{
	if (bIsMining)
	{
		return false;
	}

	if (!HasPickaxeEquipped() || !HasEnoughStamina())
	{
		return false;
	}

	if (!VoxelWorld)
	{
		FindVoxelWorld();
		if (!VoxelWorld)
		{
			return false;
		}
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Consume stamina for tunnel action (more expensive than single hit)
	const float TunnelStaminaCost = GetStaminaCost() * 3.f;
	if (!ConsumeStamina(TunnelStaminaCost))
	{
		return false;
	}

	// Get owner's facing direction and position
	const FVector OwnerLocation = Owner->GetActorLocation();
	const FRotator OwnerRotation = Owner->GetActorRotation();
	const FVector ForwardDir = OwnerRotation.Vector();

	// Tunnel center is one step forward, with vertical offset
	const float StepDistance = VOXEL_SIZE * 2.f; // One tunnel section = 1m forward
	const FVector TunnelCenter = OwnerLocation + ForwardDir * StepDistance + FVector(0.f, 0.f, VerticalOffset);

	// Carve the tunnel section as an ellipsoid (wider than tall, or rectangular approx)
	// We carve multiple overlapping spheres to form a rectangular-ish tunnel
	const float HalfWidth = TunnelWidth * 0.5f;
	const float HalfHeight = TunnelHeight * 0.5f;
	const float CarveRadius = FMath::Min(HalfWidth, HalfHeight);

	// Carve center
	VoxelWorld->CarveAt(TunnelCenter, CarveRadius);

	// Carve top and bottom to get full height
	if (TunnelHeight > TunnelWidth)
	{
		VoxelWorld->CarveAt(TunnelCenter + FVector(0.f, 0.f, CarveRadius * 0.5f), CarveRadius * 0.7f);
		VoxelWorld->CarveAt(TunnelCenter - FVector(0.f, 0.f, CarveRadius * 0.5f), CarveRadius * 0.7f);
	}

	// Carve sides to get full width
	const FVector RightDir = FVector::CrossProduct(ForwardDir, FVector::UpVector).GetSafeNormal();
	if (TunnelWidth > CarveRadius)
	{
		VoxelWorld->CarveAt(TunnelCenter + RightDir * CarveRadius * 0.4f, CarveRadius * 0.7f);
		VoxelWorld->CarveAt(TunnelCenter - RightDir * CarveRadius * 0.4f, CarveRadius * 0.7f);
	}

	// Gain XP for tunneling
	GainMiningXP(XPPerHit * 2.f);

	// Fire delegates
	FMiningResult Result;
	Result.MinedMaterial = EVoxelMaterial::Rock;
	Result.Quality = 0;
	Result.Quantity = 0; // Tunneling doesn't yield material directly
	Result.XPGained = XPPerHit * 2.f;
	Result.bSuccess = true;

	OnMiningHit.Broadcast(TunnelCenter, EVoxelMaterial::Rock);
	OnMiningComplete.Broadcast(Result);

	return true;
}

// ─── Ore Extraction ──────────────────────────────────────────────────────────

FMiningResult UAoCMiningComponent::MineOre(FVector Location)
{
	FMiningResult Result;
	Result.bSuccess = false;

	if (!VoxelWorld)
	{
		return Result;
	}

	const FVoxelData Voxel = VoxelWorld->GetVoxelAt(Location);
	if (!Voxel.IsOre())
	{
		return Result;
	}

	// Calculate yield based on skill and quality
	// Base yield: 1 ore per mine action
	// Skill bonus: up to 2x at skill 100
	// Quality from source vein — tool quality does NOT affect ore quality (design doc rule)
	const float SkillMultiplier = 1.f + (MiningSkill / 100.f);
	const int32 BaseYield = 1;
	const int32 BonusYield = (FMath::FRand() < (MiningSkill / 200.f)) ? 1 : 0; // Chance of +1 at high skill
	const int32 TotalYield = BaseYield + BonusYield;

	// Remove the ore voxel
	FVoxelData AirVoxel;
	AirVoxel.Material = EVoxelMaterial::Air;
	AirVoxel.Density = 0.f;
	AirVoxel.Quality = 0;
	VoxelWorld->SetVoxelAt(Location, AirVoxel);

	// Update vein remaining ore count
	// Find the closest vein to this location and decrement
	float ClosestDistSq = FLT_MAX;
	int32 ClosestVeinIdx = INDEX_NONE;

	for (int32 i = 0; i < VoxelWorld->OreVeins.Num(); ++i)
	{
		FVeinData& Vein = VoxelWorld->OreVeins[i];
		if (Vein.Material == Voxel.Material && !Vein.IsDepleted())
		{
			const float DistSq = FVector::DistSquared(Location, Vein.Center);
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestVeinIdx = i;
			}
		}
	}

	if (ClosestVeinIdx != INDEX_NONE)
	{
		VoxelWorld->OreVeins[ClosestVeinIdx].RemainingOre -= TotalYield;
	}

	// Build result
	Result.MinedMaterial = Voxel.Material;
	Result.Quality = Voxel.Quality;
	Result.Quantity = TotalYield;
	Result.XPGained = XPPerOreExtracted * TotalYield;
	Result.bSuccess = true;

	// Gain XP
	GainMiningXP(Result.XPGained);

	return Result;
}

// ─── Prospecting ─────────────────────────────────────────────────────────────

bool UAoCMiningComponent::Prospect(FVector SearchCenter, TArray<FVeinData>& OutVeins)
{
	if (!VoxelWorld)
	{
		FindVoxelWorld();
		if (!VoxelWorld)
		{
			return false;
		}
	}

	const float Radius = GetProspectRadius();
	return VoxelWorld->ProspectAt(SearchCenter, Radius, OutVeins);
}

// ─── Calculations ────────────────────────────────────────────────────────────

float UAoCMiningComponent::GetEffectiveMiningTime() const
{
	// Mining time decreases with both skill and tool quality
	// At skill 0, tool Q 0: BaseMiningTime (3s)
	// At skill 100, tool Q 100: ~0.75s
	const float SkillFactor = 1.f - (MiningSkill / 100.f) * 0.5f;    // 1.0 → 0.5
	const float ToolFactor = 1.f - (ToolQuality / 100.f) * 0.5f;     // 1.0 → 0.5
	return FMath::Max(0.5f, BaseMiningTime * SkillFactor * ToolFactor);
}

float UAoCMiningComponent::GetStaminaCost() const
{
	// Stamina cost decreases with skill: skill 100 = half cost
	const float SkillReduction = MiningSkill / 100.f * 0.5f;
	return BaseStaminaCostPerHit * (1.f - SkillReduction);
}

float UAoCMiningComponent::GetProspectRadius() const
{
	// Skill 0 = 1 tile (50cm), Skill 100 = 10 tiles (500cm)
	// Scaled to world units for meaningful search
	const float TilesRadius = 1.f + (MiningSkill / 100.f) * 9.f;
	return TilesRadius * VOXEL_SIZE;
}

bool UAoCMiningComponent::HasPickaxeEquipped() const
{
	// Placeholder — always returns true
	// In production, check equipped tool via inventory system
	return true;
}

bool UAoCMiningComponent::HasEnoughStamina() const
{
	return CurrentStamina >= GetStaminaCost();
}

// ─── Skill & Stamina ─────────────────────────────────────────────────────────

void UAoCMiningComponent::GainMiningXP(float Amount)
{
	if (MiningSkill >= 100.f)
	{
		return;
	}

	MiningXP += Amount;
	while (MiningXP >= XPPerSkillPoint && MiningSkill < 100.f)
	{
		MiningXP -= XPPerSkillPoint;
		MiningSkill = FMath::Min(MiningSkill + 1.f, 100.f);
	}
}

bool UAoCMiningComponent::ConsumeStamina(float Amount)
{
	if (CurrentStamina < Amount)
	{
		return false;
	}
	CurrentStamina -= Amount;
	return true;
}

FName UAoCMiningComponent::GetMiningAnimation() const
{
	switch (CurrentAction)
	{
	case EMiningAction::TunnelForward:
		return FName(TEXT("Anim_Mining_Forward"));
	case EMiningAction::TunnelDown:
		return FName(TEXT("Anim_Mining_Down"));
	case EMiningAction::TunnelUp:
		return FName(TEXT("Anim_Mining_Up"));
	case EMiningAction::MineOre:
		return FName(TEXT("Anim_Mining_Ore"));
	case EMiningAction::Prospect:
		return FName(TEXT("Anim_Prospecting"));
	default:
		return NAME_None;
	}
}

// ─── Utility ─────────────────────────────────────────────────────────────────

void UAoCMiningComponent::FindVoxelWorld()
{
	if (VoxelWorld)
	{
		return;
	}

	// Search the level for an AAoCVoxelWorld actor
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(World, AAoCVoxelWorld::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		VoxelWorld = Cast<AAoCVoxelWorld>(FoundActors[0]);
	}
}
