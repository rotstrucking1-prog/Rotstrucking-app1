// AoCFurnaceActor.cpp
// Architect of Creation - Furnace Actor Implementation
// Full tick-based furnace simulation with temperature, fuel, smelting, and visual systems.

#include "AoCFurnaceActor.h"
#include "AoCQualitySystem.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AAoCFurnaceActor::AAoCFurnaceActor()
{
	PrimaryActorTick.bCanEverTick = true;

	// --- Components ---
	FurnaceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FurnaceMesh"));
	RootComponent = FurnaceMesh;

	OrePileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrePileMesh"));
	OrePileMesh->SetupAttachment(FurnaceMesh);
	OrePileMesh->SetVisibility(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(FurnaceMesh);
	GlowMesh->SetVisibility(false);

	// --- Defaults ---
	FurnaceTier = EFurnaceTier::CampfireCrucible;
	FurnaceState = EFurnaceState::Idle;
	CurrentTemp = AmbientTemp;
	MaxTemp = 800.f;
	SustainedMaxTemp = 800.f;
	FuelLevel = 0.f;
	FuelDecayRate = 0.2f;
	Condition = 100.f;
	MaxCondition = 100.f;
	Capacity = 20;
	OreLoaded = 0;
	OreMaterial = static_cast<EVoxelMaterial>(0);
	OreQuality = 0;
	bBellowsActive = false;
	SlagLevel = 0.f;
	SmeltingProgress = 0.f;
	SmeltingDuration = 0.f;
	CurrentOutputType = ESmeltOutput::Lump;
	SpeedMultiplier = 1.f;
	bHasOutput = false;
	TempDecayRate = 80.f;
	HeatingRate = 5.f;
	OverheatQualityLoss = 0.f;
	SmeltConditionSnapshot = 100.f;
	PreviousTempForDelegate = AmbientTemp;
	BellowsBoostTimer = 0.f;
	FuelQualityTempBonus = 0.f;
	GlowMaterialInstance = nullptr;
	OrePileMaterialInstance = nullptr;
}

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void AAoCFurnaceActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeFromTier();

	// Create dynamic material instances for visual effects
	if (GlowMesh && GlowMesh->GetMaterial(0))
	{
		GlowMaterialInstance = UMaterialInstanceDynamic::Create(GlowMesh->GetMaterial(0), this);
		GlowMesh->SetMaterial(0, GlowMaterialInstance);
	}

	if (OrePileMesh && OrePileMesh->GetMaterial(0))
	{
		OrePileMaterialInstance = UMaterialInstanceDynamic::Create(OrePileMesh->GetMaterial(0), this);
		OrePileMesh->SetMaterial(0, OrePileMaterialInstance);
	}
}

void AAoCFurnaceActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (FurnaceState == EFurnaceState::Idle)
	{
		return;
	}

	// Core tick systems
	UpdateFuelConsumption(DeltaTime);
	UpdateTemperature(DeltaTime);
	UpdateSmeltingProgress(DeltaTime);
	UpdateOverheatPenalty(DeltaTime);
	UpdateVisuals();

	// Bellows boost timer decay (Bloomery only)
	if (bBellowsActive && BellowsBoostTimer > 0.f)
	{
		BellowsBoostTimer -= DeltaTime;
		if (BellowsBoostTimer <= 0.f)
		{
			bBellowsActive = false;
			BellowsBoostTimer = 0.f;
		}
	}

	// Fire temperature change delegate when threshold exceeded
	if (FMath::Abs(CurrentTemp - PreviousTempForDelegate) >= TempChangeDelegateThreshold)
	{
		OnTempChanged.Broadcast(this, CurrentTemp);
		PreviousTempForDelegate = CurrentTemp;
	}
}

// ─── Initialization ─────────────────────────────────────────────────────────

void AAoCFurnaceActor::InitializeFromTier()
{
	MaxTemp = GetMaxTempForTier(FurnaceTier);
	SustainedMaxTemp = GetSustainedMaxTempForTier(FurnaceTier);
	Capacity = GetCapacityForTier(FurnaceTier);
	SpeedMultiplier = GetSpeedMultiplierForTier(FurnaceTier);
	TempDecayRate = GetTempDecayRateForTier(FurnaceTier);
	HeatingRate = GetHeatingRateForTier(FurnaceTier);
}

// ─── Public Functions ────────────────────────────────────────────────────────

bool AAoCFurnaceActor::Ignite()
{
	if (FuelLevel <= 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot ignite — no fuel loaded."));
		return false;
	}

	if (FurnaceState != EFurnaceState::Idle && FurnaceState != EFurnaceState::NeedsFuel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot ignite — furnace is already active."));
		return false;
	}

	SetFurnaceState(EFurnaceState::Heating);
	OnIgnited.Broadcast(this);

	if (GlowMesh)
	{
		GlowMesh->SetVisibility(true);
	}

	UE_LOG(LogTemp, Log, TEXT("Furnace: Ignited. Tier=%d, MaxTemp=%.0f°C"), static_cast<int32>(FurnaceTier), MaxTemp);
	return true;
}

void AAoCFurnaceActor::AddFuel(int32 FuelAmount, float FuelQuality)
{
	const float FuelToAdd = static_cast<float>(FMath::Max(FuelAmount, 0));
	FuelLevel = FMath::Clamp(FuelLevel + FuelToAdd, 0.f, 100.f);

	// High quality fuel provides a small max-temp bonus (+50°C at Q100)
	FuelQualityTempBonus = FMath::Max(FuelQualityTempBonus, FuelQuality * 0.5f);

	UE_LOG(LogTemp, Log, TEXT("Furnace: Added %d fuel (Q%.0f). Fuel level: %.1f/100"), FuelAmount, FuelQuality, FuelLevel);
}

bool AAoCFurnaceActor::PumpBellows()
{
	// Bellows only work on Bloomery tier
	if (FurnaceTier != EFurnaceTier::Bloomery)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Bellows only available on Bloomery."));
		return false;
	}

	// Must be actively heating or smelting
	if (FurnaceState != EFurnaceState::Heating && FurnaceState != EFurnaceState::Ready
		&& FurnaceState != EFurnaceState::Smelting)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot pump bellows — furnace is not active."));
		return false;
	}

	// Add temperature boost (capped at MaxTemp)
	CurrentTemp = FMath::Min(CurrentTemp + BellowsTempBoost, MaxTemp + FuelQualityTempBonus);
	bBellowsActive = true;
	BellowsBoostTimer = BellowsBoostDuration;

	UE_LOG(LogTemp, Log, TEXT("Furnace: Bellows pumped. Temp now %.0f°C"), CurrentTemp);
	return true;
}

bool AAoCFurnaceActor::LoadOre(EVoxelMaterial Material, int32 Amount, uint8 Quality)
{
	if (Amount <= 0)
	{
		return false;
	}

	// Cannot load different ore when ore is already loaded
	if (OreLoaded > 0 && OreMaterial != Material)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot mix ore types. Current ore must be smelted or removed first."));
		return false;
	}

	// Cannot load during smelting or when output is pending
	if (FurnaceState == EFurnaceState::Smelting || bHasOutput)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot load ore during smelting or while output is pending."));
		return false;
	}

	const int32 SpaceAvailable = Capacity - OreLoaded;
	const int32 AmountToLoad = FMath::Min(Amount, SpaceAvailable);

	if (AmountToLoad <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: No capacity available. Capacity=%d, Loaded=%d"), Capacity, OreLoaded);
		return false;
	}

	// If adding to existing ore, merge quality (weighted average with integer rounding → quality loss)
	if (OreLoaded > 0)
	{
		OreQuality = UAoCQualitySystem::MergeQuality(OreQuality, OreLoaded, Quality, AmountToLoad);
	}
	else
	{
		OreMaterial = Material;
		OreQuality = Quality;
	}

	OreLoaded += AmountToLoad;

	// Show ore pile mesh, scale based on fill percentage
	if (OrePileMesh)
	{
		OrePileMesh->SetVisibility(true);
		const float FillRatio = static_cast<float>(OreLoaded) / static_cast<float>(Capacity);
		OrePileMesh->SetRelativeScale3D(FVector(FillRatio, FillRatio, FillRatio));
	}

	UE_LOG(LogTemp, Log, TEXT("Furnace: Loaded %d ore (Q%d). Total: %d/%d"), AmountToLoad, Quality, OreLoaded, Capacity);
	return true;
}

bool AAoCFurnaceActor::CanSmelt(EVoxelMaterial Material) const
{
	const float ReductionTemp = GetReductionTemperature(Material);
	if (ReductionTemp <= 0.f)
	{
		return false;
	}
	return CurrentTemp >= ReductionTemp;
}

bool AAoCFurnaceActor::StartSmelting(ESmeltOutput OutputType)
{
	// Validate state
	if (FurnaceState != EFurnaceState::Heating && FurnaceState != EFurnaceState::Ready)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot start smelting — not in Heating or Ready state."));
		return false;
	}

	if (bHasOutput)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Cannot start smelting — pending output must be extracted first."));
		return false;
	}

	// Campfire Crucible can only produce lumps
	if (FurnaceTier == EFurnaceTier::CampfireCrucible && OutputType != ESmeltOutput::Lump)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Campfire Crucible can only produce lumps."));
		return false;
	}

	// Check ore requirements
	const int32 OreRequired = GetOreRequiredForOutput(OutputType);
	if (OreLoaded < OreRequired)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Not enough ore. Need %d, have %d."), OreRequired, OreLoaded);
		return false;
	}

	// Check temperature
	if (!CanSmelt(OreMaterial))
	{
		const float ReductionTemp = GetReductionTemperature(OreMaterial);
		UE_LOG(LogTemp, Warning, TEXT("Furnace: Temperature too low. Need %.0f°C, current %.0f°C."),
			ReductionTemp, CurrentTemp);
		return false;
	}

	// Calculate smelting duration (base time / speed multiplier, increased by slag)
	const float BaseTime = GetBaseSmeltTime(OutputType);
	const float SlagPenalty = (SlagLevel >= 80.f) ? 1.5f : (SlagLevel >= 50.f) ? 1.25f : 1.0f;
	SmeltingDuration = (BaseTime / SpeedMultiplier) * SlagPenalty;
	SmeltingProgress = 0.f;
	CurrentOutputType = OutputType;

	// Snapshot condition at smelt start
	SmeltConditionSnapshot = Condition;

	// Reset overheating accumulator
	OverheatQualityLoss = 0.f;

	SetFurnaceState(EFurnaceState::Smelting);

	UE_LOG(LogTemp, Log, TEXT("Furnace: Smelting started. Output=%d, Duration=%.1fs, Speed=%.1fx"),
		static_cast<int32>(OutputType), SmeltingDuration, SpeedMultiplier);
	return true;
}

FSmeltingResult AAoCFurnaceActor::ExtractOutput()
{
	FSmeltingResult Result;

	if (!bHasOutput)
	{
		UE_LOG(LogTemp, Warning, TEXT("Furnace: No output available for extraction."));
		return Result;
	}

	Result = PendingOutput;
	bHasOutput = false;
	PendingOutput = FSmeltingResult();

	// Hide ore pile if empty
	if (OreLoaded <= 0 && OrePileMesh)
	{
		OrePileMesh->SetVisibility(false);
	}

	UE_LOG(LogTemp, Log, TEXT("Furnace: Output extracted. Material=%d, Form=%d, Q=%d, Temp=%.0f°C"),
		static_cast<int32>(Result.Material), static_cast<int32>(Result.Form),
		Result.Quality, Result.Temperature);
	return Result;
}

void AAoCFurnaceActor::Repair(float MaterialQuality)
{
	// Repair amount = 20 * (MaterialQuality / 100)
	// Q100 materials restore 20 condition points
	const float RepairAmount = 20.f * (FMath::Clamp(MaterialQuality, 0.f, 100.f) / 100.f);
	Condition = FMath::Clamp(Condition + RepairAmount, 0.f, MaxCondition);

	UE_LOG(LogTemp, Log, TEXT("Furnace: Repaired by %.1f (Q%.0f materials). Condition: %.1f/%.1f"),
		RepairAmount, MaterialQuality, Condition, MaxCondition);
}

void AAoCFurnaceActor::CleanSlag()
{
	UE_LOG(LogTemp, Log, TEXT("Furnace: Slag cleaned. Was %.1f, now 0."), SlagLevel);
	SlagLevel = 0.f;
}

// ─── Static Helpers ──────────────────────────────────────────────────────────

float AAoCFurnaceActor::GetReductionTemperature(EVoxelMaterial Material)
{
	// Real metallurgy ore reduction temperatures — NOT melting points.
	// A bloomery never fully melts iron (1538°C) — it reduces ore at 1200°C.
	switch (Material)
	{
		// T1 — Bloomery (≤1200°C without bellows)
		case EVoxelMaterial::OreLead:       return 800.f;
		case EVoxelMaterial::OreZinc:       return 950.f;
		case EVoxelMaterial::OreCopper:     return 1100.f;
		case EVoxelMaterial::OreTin:        return 1100.f;

		// T2 — Bloomery with Bellows (≤1500°C)
		case EVoxelMaterial::OreSilver:     return 1000.f;
		case EVoxelMaterial::OreGold:       return 1100.f;
		case EVoxelMaterial::OreIron:       return 1200.f;
		case EVoxelMaterial::OreManganese:  return 1400.f;

		// T3 — Blast Furnace (≤2000°C)
		case EVoxelMaterial::OreCobalt:     return 1400.f;
		case EVoxelMaterial::OreNickel:     return 1450.f;
		case EVoxelMaterial::OreVanadium:   return 1750.f;
		case EVoxelMaterial::OreChromium:   return 1850.f;

		// T4 — Crucible Furnace (≤2500°C)
		case EVoxelMaterial::OrePalladium:  return 1600.f;
		case EVoxelMaterial::OreTitanium:   return 1800.f;
		case EVoxelMaterial::OrePlatinum:   return 1800.f;
		case EVoxelMaterial::OreRhodium:    return 2000.f;
		case EVoxelMaterial::OreIridium:    return 2400.f;

		// T5 — Arcane Forge (≤3500°C, requires magic)
		case EVoxelMaterial::OreNiobium:    return 2400.f;
		case EVoxelMaterial::OreMolybdenum: return 2500.f;
		case EVoxelMaterial::OreTantalum:   return 2900.f;
		case EVoxelMaterial::OreOsmium:     return 3000.f;
		case EVoxelMaterial::OreTungsten:   return 3300.f;

		default:
			UE_LOG(LogTemp, Warning, TEXT("Furnace: Unknown ore material for reduction temperature."));
			return -1.f;
	}
}

float AAoCFurnaceActor::GetMaxTempForTier(EFurnaceTier Tier)
{
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 800.f;
		case EFurnaceTier::Bloomery:         return 1500.f;
		case EFurnaceTier::BlastFurnace:     return 2000.f;
		case EFurnaceTier::CrucibleFurnace:  return 2500.f;
		case EFurnaceTier::ArcaneForge:      return 3500.f;
		default:                             return 800.f;
	}
}

float AAoCFurnaceActor::GetSustainedMaxTempForTier(EFurnaceTier Tier)
{
	// Temperature achievable by fuel alone — Bloomery needs bellows to exceed 1200°C
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 800.f;
		case EFurnaceTier::Bloomery:         return 1200.f;    // Bellows push to 1500
		case EFurnaceTier::BlastFurnace:     return 2000.f;    // Built-in forced air
		case EFurnaceTier::CrucibleFurnace:  return 2500.f;
		case EFurnaceTier::ArcaneForge:      return 3500.f;
		default:                             return 800.f;
	}
}

int32 AAoCFurnaceActor::GetCapacityForTier(EFurnaceTier Tier)
{
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 20;
		case EFurnaceTier::Bloomery:         return 400;
		case EFurnaceTier::BlastFurnace:     return 800;
		case EFurnaceTier::CrucibleFurnace:  return 400;
		case EFurnaceTier::ArcaneForge:      return 200;
		default:                             return 20;
	}
}

float AAoCFurnaceActor::GetSpeedMultiplierForTier(EFurnaceTier Tier)
{
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 1.0f;
		case EFurnaceTier::Bloomery:         return 1.0f;
		case EFurnaceTier::BlastFurnace:     return 3.0f;
		case EFurnaceTier::CrucibleFurnace:  return 2.0f;
		case EFurnaceTier::ArcaneForge:      return 5.0f;
		default:                             return 1.0f;
	}
}

float AAoCFurnaceActor::GetTempDecayRateForTier(EFurnaceTier Tier)
{
	// °C per minute — converted to per-second in UpdateTemperature
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 80.f;
		case EFurnaceTier::Bloomery:         return 50.f;     // Without bellows
		case EFurnaceTier::BlastFurnace:     return 30.f;
		case EFurnaceTier::CrucibleFurnace:  return 40.f;
		case EFurnaceTier::ArcaneForge:      return 20.f;     // Well-insulated + magic
		default:                             return 80.f;
	}
}

float AAoCFurnaceActor::GetHeatingRateForTier(EFurnaceTier Tier)
{
	// °C per second while fuel is burning
	switch (Tier)
	{
		case EFurnaceTier::CampfireCrucible: return 5.f;      // ~2.5 min to 800°C
		case EFurnaceTier::Bloomery:         return 4.f;       // ~5 min to 1200°C
		case EFurnaceTier::BlastFurnace:     return 8.f;       // ~4 min to 2000°C
		case EFurnaceTier::CrucibleFurnace:  return 10.f;      // ~4 min to 2500°C
		case EFurnaceTier::ArcaneForge:      return 15.f;      // ~4 min to 3500°C
		default:                             return 5.f;
	}
}

int32 AAoCFurnaceActor::GetOreRequiredForOutput(ESmeltOutput OutputType)
{
	switch (OutputType)
	{
		case ESmeltOutput::Lump:  return 1;
		case ESmeltOutput::Bar:   return 4;
		case ESmeltOutput::Ingot: return 20;
		default:                  return 1;
	}
}

float AAoCFurnaceActor::GetBaseSmeltTime(ESmeltOutput OutputType)
{
	// Base smelting time in seconds (before speed multiplier)
	switch (OutputType)
	{
		case ESmeltOutput::Lump:  return 15.f;
		case ESmeltOutput::Bar:   return 45.f;
		case ESmeltOutput::Ingot: return 150.f;
		default:                  return 15.f;
	}
}

// ─── Internal Tick Systems ───────────────────────────────────────────────────

void AAoCFurnaceActor::UpdateTemperature(float DeltaTime)
{
	const float EffectiveMaxTemp = SustainedMaxTemp + FuelQualityTempBonus;
	const float DecayPerSecond = TempDecayRate / 60.f;

	switch (FurnaceState)
	{
		case EFurnaceState::Heating:
		case EFurnaceState::Ready:
		case EFurnaceState::Smelting:
		{
			if (FuelLevel > 0.f)
			{
				// Heat toward sustained max temp (fuel-driven)
				if (CurrentTemp < EffectiveMaxTemp)
				{
					CurrentTemp += HeatingRate * DeltaTime;
					CurrentTemp = FMath::Min(CurrentTemp, EffectiveMaxTemp);
				}

				// If above sustained max (from bellows), decay back toward it
				if (CurrentTemp > EffectiveMaxTemp)
				{
					CurrentTemp -= DecayPerSecond * DeltaTime;
					CurrentTemp = FMath::Max(CurrentTemp, EffectiveMaxTemp);
				}
			}
			else
			{
				// No fuel — temperature decays
				CurrentTemp -= DecayPerSecond * DeltaTime;
				CurrentTemp = FMath::Max(CurrentTemp, AmbientTemp);
			}

			// Check if we've reached reduction temp for loaded ore → Ready
			if (FurnaceState == EFurnaceState::Heating && OreLoaded > 0)
			{
				const float ReductionTemp = GetReductionTemperature(OreMaterial);
				if (ReductionTemp > 0.f && CurrentTemp >= ReductionTemp)
				{
					SetFurnaceState(EFurnaceState::Ready);
				}
			}
			break;
		}

		case EFurnaceState::Cooling:
		{
			// Cooling — temperature decays toward ambient
			CurrentTemp -= DecayPerSecond * DeltaTime;
			if (CurrentTemp <= AmbientTemp)
			{
				CurrentTemp = AmbientTemp;
				SetFurnaceState(EFurnaceState::Idle);

				if (GlowMesh)
				{
					GlowMesh->SetVisibility(false);
				}
			}
			break;
		}

		case EFurnaceState::NeedsFuel:
		{
			// Temperature decays while waiting for fuel
			CurrentTemp -= DecayPerSecond * DeltaTime;
			CurrentTemp = FMath::Max(CurrentTemp, AmbientTemp);

			// If fuel is added, transition back to Heating
			if (FuelLevel > 0.f)
			{
				SetFurnaceState(EFurnaceState::Heating);
			}

			// If temperature drops to ambient, go Idle
			if (CurrentTemp <= AmbientTemp)
			{
				CurrentTemp = AmbientTemp;
				SetFurnaceState(EFurnaceState::Idle);

				if (GlowMesh)
				{
					GlowMesh->SetVisibility(false);
				}
			}
			break;
		}

		default:
			break;
	}
}

void AAoCFurnaceActor::UpdateFuelConsumption(float DeltaTime)
{
	// Only consume fuel when actively operating
	if (FurnaceState != EFurnaceState::Heating && FurnaceState != EFurnaceState::Ready
		&& FurnaceState != EFurnaceState::Smelting)
	{
		return;
	}

	if (FuelLevel <= 0.f)
	{
		return;
	}

	FuelLevel -= FuelDecayRate * DeltaTime;

	if (FuelLevel <= 0.f)
	{
		FuelLevel = 0.f;
		FuelQualityTempBonus = 0.f;

		// Pause smelting if in progress
		if (FurnaceState == EFurnaceState::Smelting)
		{
			// Smelting paused — waiting for fuel. Progress preserved.
			UE_LOG(LogTemp, Warning, TEXT("Furnace: Fuel depleted during smelting! Add fuel to continue."));
		}

		SetFurnaceState(EFurnaceState::NeedsFuel);
		OnFuelDepleted.Broadcast(this);
	}
}

void AAoCFurnaceActor::UpdateSmeltingProgress(float DeltaTime)
{
	if (FurnaceState != EFurnaceState::Smelting)
	{
		return;
	}

	if (SmeltingDuration <= 0.f)
	{
		return;
	}

	// Check that temperature is still at or above reduction temp
	const float ReductionTemp = GetReductionTemperature(OreMaterial);
	if (CurrentTemp < ReductionTemp)
	{
		// Under-heating: smelting pauses but doesn't reset. Fuel is wasted.
		UE_LOG(LogTemp, Verbose, TEXT("Furnace: Temperature below reduction point. Smelting paused."));
		return;
	}

	// Advance progress
	SmeltingProgress += DeltaTime / SmeltingDuration;

	if (SmeltingProgress >= 1.f)
	{
		SmeltingProgress = 1.f;

		// --- Smelting Complete ---

		// Consume ore
		const int32 OreConsumed = GetOreRequiredForOutput(CurrentOutputType);
		OreLoaded -= OreConsumed;
		OreLoaded = FMath::Max(OreLoaded, 0);

		// Calculate output quality: min(ore_Q, furnace_condition) with overheat penalty
		const float EffectiveOreQ = FMath::Max(static_cast<float>(OreQuality) - FMath::Floor(OverheatQualityLoss), 0.f);
		const float FurnaceQ = FMath::Min(EffectiveOreQ, SmeltConditionSnapshot);
		const uint8 OutputQ = static_cast<uint8>(FMath::Clamp(FMath::FloorToInt(FurnaceQ), 0, 100));

		// Build output result
		PendingOutput.Material = OreMaterial;
		PendingOutput.Form = CurrentOutputType;
		PendingOutput.Quality = OutputQ;
		PendingOutput.Temperature = CurrentTemp;
		PendingOutput.bIsValid = true;
		bHasOutput = true;

		// Degrade furnace condition
		float ConditionLoss = ConditionLossPerLump;
		if (CurrentOutputType == ESmeltOutput::Bar) ConditionLoss = ConditionLossPerBar;
		else if (CurrentOutputType == ESmeltOutput::Ingot) ConditionLoss = ConditionLossPerIngot;
		Condition = FMath::Max(Condition - ConditionLoss, 0.f);

		if (Condition < ConditionLowThreshold)
		{
			OnConditionLow.Broadcast(this, Condition);
		}

		// Accumulate slag
		float SlagGain = SlagPerLump;
		if (CurrentOutputType == ESmeltOutput::Bar) SlagGain = SlagPerBar;
		else if (CurrentOutputType == ESmeltOutput::Ingot) SlagGain = SlagPerIngot;
		SlagLevel = FMath::Min(SlagLevel + SlagGain, 100.f);

		// Update ore pile visual
		if (OrePileMesh)
		{
			if (OreLoaded <= 0)
			{
				OrePileMesh->SetVisibility(false);
			}
			else
			{
				const float FillRatio = static_cast<float>(OreLoaded) / static_cast<float>(Capacity);
				OrePileMesh->SetRelativeScale3D(FVector(FillRatio, FillRatio, FillRatio));
			}
		}

		// Fire delegate
		OnSmeltingComplete.Broadcast(this, PendingOutput);

		// Transition state
		if (OreLoaded > 0 && !bHasOutput)
		{
			SetFurnaceState(EFurnaceState::Ready);
		}
		else
		{
			SetFurnaceState(EFurnaceState::Ready);
		}

		// Reset smelting state
		SmeltingProgress = 0.f;
		SmeltingDuration = 0.f;
		OverheatQualityLoss = 0.f;

		UE_LOG(LogTemp, Log, TEXT("Furnace: Smelting complete! Output Q=%d, Form=%d, Temp=%.0f°C"),
			OutputQ, static_cast<int32>(CurrentOutputType), CurrentTemp);
	}
}

void AAoCFurnaceActor::UpdateOverheatPenalty(float DeltaTime)
{
	// Only apply during active smelting
	if (FurnaceState != EFurnaceState::Smelting || OreLoaded <= 0)
	{
		return;
	}

	const float ReductionTemp = GetReductionTemperature(OreMaterial);
	if (ReductionTemp <= 0.f)
	{
		return;
	}

	// Overheating: Q -= 5 per minute when temp is 50°C+ above reduction temp
	if (CurrentTemp > ReductionTemp + OverheatThreshold)
	{
		OverheatQualityLoss += (OverheatQualityLossPerMinute / 60.f) * DeltaTime;
	}
}

void AAoCFurnaceActor::UpdateVisuals()
{
	if (!GlowMaterialInstance)
	{
		return;
	}

	// Temperature-driven visual effects:
	//   Cold (<500°C):       natural color, emissive = 0
	//   500°C:               slight red glow, emissive = 0.3
	//   1000°C:              bright orange, emissive = 0.7
	//   1500°C:              yellow-white, emissive = 1.0
	//   2000°C+:             white-blue, emissive = 1.5

	float EmissiveIntensity = 0.f;
	FLinearColor GlowColor = FLinearColor::Black;

	if (CurrentTemp < 500.f)
	{
		// Below visible glow threshold — interpolate from 0 to slight glow
		const float Alpha = FMath::Clamp((CurrentTemp - AmbientTemp) / (500.f - AmbientTemp), 0.f, 1.f);
		EmissiveIntensity = Alpha * 0.3f;
		GlowColor = FLinearColor::LerpUsingHSV(FLinearColor::Black, FLinearColor(1.0f, 0.1f, 0.0f), Alpha);
	}
	else if (CurrentTemp < 1000.f)
	{
		// Red to orange
		const float Alpha = (CurrentTemp - 500.f) / 500.f;
		EmissiveIntensity = FMath::Lerp(0.3f, 0.7f, Alpha);
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(1.0f, 0.1f, 0.0f),   // Deep red
			FLinearColor(1.0f, 0.5f, 0.0f),   // Bright orange
			Alpha
		);
	}
	else if (CurrentTemp < 1500.f)
	{
		// Orange to yellow-white
		const float Alpha = (CurrentTemp - 1000.f) / 500.f;
		EmissiveIntensity = FMath::Lerp(0.7f, 1.0f, Alpha);
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(1.0f, 0.5f, 0.0f),   // Bright orange
			FLinearColor(1.0f, 0.9f, 0.6f),   // Yellow-white
			Alpha
		);
	}
	else if (CurrentTemp < 2000.f)
	{
		// Yellow-white to white
		const float Alpha = (CurrentTemp - 1500.f) / 500.f;
		EmissiveIntensity = FMath::Lerp(1.0f, 1.5f, Alpha);
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(1.0f, 0.9f, 0.6f),   // Yellow-white
			FLinearColor(1.0f, 1.0f, 1.0f),   // Pure white
			Alpha
		);
	}
	else
	{
		// 2000°C+: white to white-blue
		const float Alpha = FMath::Clamp((CurrentTemp - 2000.f) / 1500.f, 0.f, 1.f);
		EmissiveIntensity = FMath::Lerp(1.5f, 2.0f, Alpha);
		GlowColor = FLinearColor::LerpUsingHSV(
			FLinearColor(1.0f, 1.0f, 1.0f),   // Pure white
			FLinearColor(0.7f, 0.85f, 1.0f),  // White-blue
			Alpha
		);
	}

	// Apply to dynamic material
	GlowMaterialInstance->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity);
	GlowMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), GlowColor);

	// Update ore pile emissive to show heating ore
	if (OrePileMaterialInstance && OreLoaded > 0)
	{
		OrePileMaterialInstance->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity * 0.6f);
		OrePileMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), GlowColor);
	}
}

void AAoCFurnaceActor::SetFurnaceState(EFurnaceState NewState)
{
	if (FurnaceState == NewState)
	{
		return;
	}

	const EFurnaceState OldState = FurnaceState;
	FurnaceState = NewState;

	UE_LOG(LogTemp, Verbose, TEXT("Furnace: State changed from %d to %d"),
		static_cast<int32>(OldState), static_cast<int32>(NewState));
}
