// AoCSmeltingComponent.cpp
// Architect of Creation - Smelting Component Implementation
// Player smelting skill, furnace interaction, hot metal handling, and recycling.

#include "AoCSmeltingComponent.h"
#include "AoCFurnaceActor.h"
#include "AoCQualitySystem.h"
#include "GameFramework/Actor.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

UAoCSmeltingComponent::UAoCSmeltingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	SmeltingSkill = 0.f;
	SmeltingXP = 0.f;
	XPPerLevel = 100.f;
	BoundFurnace = nullptr;
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void UAoCSmeltingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAoCSmeltingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Cool held hot metal over time
	UpdateHotMetalCooling(DeltaTime);
}

// ─── Furnace Interaction ─────────────────────────────────────────────────────

bool UAoCSmeltingComponent::InteractWithFurnace(AAoCFurnaceActor* Furnace)
{
	if (!Furnace)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot interact — null furnace."));
		return false;
	}

	if (!IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot interact — furnace is pending kill."));
		return false;
	}

	// Bind to furnace smelting-complete delegate for skill-based quality capping
	if (BoundFurnace != Furnace)
	{
		// Unbind from previous furnace
		if (BoundFurnace && IsValid(BoundFurnace))
		{
			BoundFurnace->OnSmeltingComplete.RemoveDynamic(this, &UAoCSmeltingComponent::OnBoundFurnaceSmeltComplete);
		}

		BoundFurnace = Furnace;
		BoundFurnace->OnSmeltingComplete.AddDynamic(this, &UAoCSmeltingComponent::OnBoundFurnaceSmeltComplete);
	}

	UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Interacting with furnace (Tier=%d, State=%d, Temp=%.0f°C)"),
		static_cast<int32>(Furnace->FurnaceTier),
		static_cast<int32>(Furnace->FurnaceState),
		Furnace->CurrentTemp);

	// UI opening is handled by the UI system — we just validate and expose data.
	return true;
}

bool UAoCSmeltingComponent::LoadOreIntoFurnace(AAoCFurnaceActor* Furnace, EVoxelMaterial Ore, int32 Amount, uint8 Quality)
{
	if (!Furnace || !IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot load ore — invalid furnace."));
		return false;
	}

	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot load ore — invalid amount."));
		return false;
	}

	const bool bSuccess = Furnace->LoadOre(Ore, Amount, Quality);
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Loaded %d ore (Q%d) into furnace."), Amount, Quality);
	}

	return bSuccess;
}

bool UAoCSmeltingComponent::StartSmeltInFurnace(AAoCFurnaceActor* Furnace, ESmeltOutput OutputType)
{
	if (!Furnace || !IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot start smelting — invalid furnace."));
		return false;
	}

	// Ensure we're bound to this furnace
	InteractWithFurnace(Furnace);

	const bool bSuccess = Furnace->StartSmelting(OutputType);
	if (bSuccess)
	{
		OnSmeltStarted.Broadcast(this, Furnace);
		GainSmeltingXP(XPPerSmelt);

		UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Smelting started (Skill=%.0f, Output=%d)."),
			SmeltingSkill, static_cast<int32>(OutputType));
	}

	return bSuccess;
}

bool UAoCSmeltingComponent::PumpBellows(AAoCFurnaceActor* Furnace)
{
	if (!Furnace || !IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot pump bellows — invalid furnace."));
		return false;
	}

	const bool bSuccess = Furnace->PumpBellows();
	if (bSuccess)
	{
		GainSmeltingXP(XPPerBellowsPump);
		OnBellowsPumped.Broadcast(this);

		UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Bellows pumped. Temp now %.0f°C."), Furnace->CurrentTemp);
	}

	return bSuccess;
}

bool UAoCSmeltingComponent::ExtractFromFurnace(AAoCFurnaceActor* Furnace)
{
	if (!Furnace || !IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot extract — invalid furnace."));
		return false;
	}

	if (!Furnace->bHasOutput)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot extract — no output available."));
		return false;
	}

	if (HeldHotMetal.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot extract — already holding hot metal."));
		return false;
	}

	// Extract from furnace
	FSmeltingResult RawOutput = Furnace->ExtractOutput();
	if (!RawOutput.bIsValid)
	{
		return false;
	}

	// Apply player skill cap: final_Q = min(skill, furnace_output_Q)
	const uint8 FinalQuality = UAoCQualitySystem::CalculateSmeltingQuality(
		SmeltingSkill,
		static_cast<float>(RawOutput.Quality),
		100.f // Condition already factored in by furnace
	);
	RawOutput.Quality = FinalQuality;

	// Store as held hot metal
	HeldHotMetal.SmeltData = RawOutput;
	HeldHotMetal.CurrentTemperature = RawOutput.Temperature;
	HeldHotMetal.bIsQuenched = false;
	HeldHotMetal.bIsValid = true;

	// Fire completion delegate
	OnSmeltComplete.Broadcast(this, Furnace, FinalQuality);

	UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Extracted output. Final Q=%d (skill cap=%.0f), Temp=%.0f°C"),
		FinalQuality, SmeltingSkill, RawOutput.Temperature);

	return true;
}

// ─── Hot Metal Functions ─────────────────────────────────────────────────────

bool UAoCSmeltingComponent::QuenchMetal(FVector WaterBarrelLocation)
{
	if (!HeldHotMetal.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot quench — not holding hot metal."));
		return false;
	}

	if (HeldHotMetal.bIsQuenched)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Metal already quenched."));
		return false;
	}

	if (HeldHotMetal.CurrentTemperature <= AmbientTemp + 10.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Metal is already cool — quenching has no effect."));
		return false;
	}

	// Check distance to water barrel
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	const float DistToBarrel = FVector::Dist(Owner->GetActorLocation(), WaterBarrelLocation);
	if (DistToBarrel > QuenchMaxDistance)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Too far from water barrel (%.0f cm, max %.0f cm)."),
			DistToBarrel, QuenchMaxDistance);
		return false;
	}

	// Instant cool — quenching affects hardness (tracked by bIsQuenched flag for downstream systems)
	HeldHotMetal.CurrentTemperature = AmbientTemp;
	HeldHotMetal.bIsQuenched = true;

	UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Metal quenched at water barrel. Hardness modified."));
	return true;
}

bool UAoCSmeltingComponent::IsHoldingHotMetal() const
{
	return HeldHotMetal.bIsValid;
}

FHotMetalItem UAoCSmeltingComponent::GetHeldHotMetal() const
{
	return HeldHotMetal;
}

// ─── Recycling ───────────────────────────────────────────────────────────────

bool UAoCSmeltingComponent::RecycleItem(AAoCFurnaceActor* Furnace, FName ItemID, uint8 ItemQuality, EVoxelMaterial BaseMaterial)
{
	if (!Furnace || !IsValid(Furnace))
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot recycle — invalid furnace."));
		return false;
	}

	// Recycling requires Smelting skill 90+
	if (SmeltingSkill < RecyclingMinSkill)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot recycle — requires Smelting skill %.0f (current: %.0f)."),
			RecyclingMinSkill, SmeltingSkill);
		return false;
	}

	// Furnace must be at reduction temperature for the base material
	if (!Furnace->CanSmelt(BaseMaterial))
	{
		const float ReductionTemp = AAoCFurnaceActor::GetReductionTemperature(BaseMaterial);
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot recycle — furnace temp (%.0f°C) below reduction temp (%.0f°C)."),
			Furnace->CurrentTemp, ReductionTemp);
		return false;
	}

	// Furnace must not already be smelting or have pending output
	if (Furnace->FurnaceState == EFurnaceState::Smelting || Furnace->bHasOutput)
	{
		UE_LOG(LogTemp, Warning, TEXT("SmeltingComponent: Cannot recycle — furnace is busy."));
		return false;
	}

	// Calculate recycled quality: 80% of original item quality
	const uint8 RecycledQuality = static_cast<uint8>(FMath::Clamp(
		FMath::FloorToInt(static_cast<float>(ItemQuality) * RecyclingQualityRetention),
		0, 100
	));

	// Load recycled material as ore (1 lump output)
	const bool bLoaded = Furnace->LoadOre(BaseMaterial, 1, RecycledQuality);
	if (!bLoaded)
	{
		return false;
	}

	// Auto-start smelting as a lump
	const bool bStarted = Furnace->StartSmelting(ESmeltOutput::Lump);
	if (bStarted)
	{
		GainSmeltingXP(XPPerRecycle);
		UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Recycling item '%s' (Q%d → Q%d) into %s lump."),
			*ItemID.ToString(), ItemQuality, RecycledQuality,
			*UEnum::GetValueAsString(BaseMaterial));
	}

	return bStarted;
}

// ─── Furnace Construction ────────────────────────────────────────────────────

bool UAoCSmeltingComponent::CanBuildFurnace(EFurnaceTier Tier) const
{
	return SmeltingSkill >= GetRequiredSkillForFurnace(Tier);
}

float UAoCSmeltingComponent::GetRequiredSkillForFurnace(EFurnaceTier Tier)
{
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 0.f;
		case EFurnaceTier::Bloomery:         return 30.f;
		case EFurnaceTier::BlastFurnace:     return 60.f;
		case EFurnaceTier::CrucibleFurnace:  return 80.f;
		case EFurnaceTier::ArcaneForge:      return 100.f;
		default:                             return 0.f;
	}
}

// ─── Internal Systems ────────────────────────────────────────────────────────

void UAoCSmeltingComponent::GainSmeltingXP(float Amount)
{
	if (SmeltingSkill >= 100.f)
	{
		return; // Already maxed
	}

	SmeltingXP += Amount;

	// Level up: XP threshold increases per level
	// Each level requires XPPerLevel * (1 + currentLevel * 0.1) XP
	const float LevelThreshold = XPPerLevel * (1.f + SmeltingSkill * 0.1f);

	while (SmeltingXP >= LevelThreshold && SmeltingSkill < 100.f)
	{
		SmeltingXP -= LevelThreshold;
		SmeltingSkill = FMath::Min(SmeltingSkill + 1.f, 100.f);

		OnSmeltingSkillUp.Broadcast(this, SmeltingSkill);

		UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Smelting skill increased to %.0f!"), SmeltingSkill);
	}
}

void UAoCSmeltingComponent::UpdateHotMetalCooling(float DeltaTime)
{
	if (!HeldHotMetal.bIsValid)
	{
		return;
	}

	if (HeldHotMetal.bIsQuenched)
	{
		return; // Already quenched — no further cooling needed
	}

	if (HeldHotMetal.CurrentTemperature <= AmbientTemp)
	{
		HeldHotMetal.CurrentTemperature = AmbientTemp;
		return;
	}

	// Cool linearly from extraction temperature to ambient over 60 seconds
	// CoolingRate = (ExtractTemp - AmbientTemp) / CooldownDuration
	const float ExtractTemp = HeldHotMetal.SmeltData.Temperature;
	const float TempRange = FMath::Max(ExtractTemp - AmbientTemp, 1.f);
	const float CoolingRate = TempRange / HotMetalCooldownDuration;

	HeldHotMetal.CurrentTemperature -= CoolingRate * DeltaTime;
	HeldHotMetal.CurrentTemperature = FMath::Max(HeldHotMetal.CurrentTemperature, AmbientTemp);
}

void UAoCSmeltingComponent::OnBoundFurnaceSmeltComplete(AAoCFurnaceActor* Furnace, FSmeltingResult Output)
{
	if (!Furnace || Furnace != BoundFurnace)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("SmeltingComponent: Bound furnace completed smelting. Output Q=%d, ready for extraction."),
		Output.Quality);
}
