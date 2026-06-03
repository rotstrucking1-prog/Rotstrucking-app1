// Source/AOC/Spellcraft/AoCSpellCastingComponent.cpp
// v29 — Spell casting with two-layer sound system + screen effects.
// Layer 1: School cast sound (plays on cast begin)
// Layer 2: Spell effect sound (plays on spell fire, pitch varies by tier)

#include "AoCSpellCastingComponent.h"
#include "AoCSpellVFXManager.h"
#include "AoCSpellScreenEffects.h"
#include "AoCSpellData.h"
#include "../Combat/AoCProjectile.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"

// ─── Constructor ────────────────────────────────────────────────────────────

UAoCSpellCastingComponent::UAoCSpellCastingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	MaxSpellBarSlots = 10;
	bIsCasting = false;
	bIsChanneling = false;
	CastTimeRemaining = 0.f;
	ChannelTimeRemaining = 0.f;
	ChannelTickInterval = 0.5f;
	ChannelTickAccumulator = 0.f;

	CurrentMana = 100.f;
	MaxMana = 100.f;
	ManaRegenRate = 5.f;

	CurrentTarget = nullptr;
	TargetLocation = FVector::ZeroVector;
	VFXManager = nullptr;
	ScreenEffects = nullptr;
	ActiveCastSound = nullptr;
}

// ─── BeginPlay ──────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::BeginPlay()
{
	Super::BeginPlay();

	SpellBar.SetNum(MaxSpellBarSlots);

	AActor* Owner = GetOwner();
	if (Owner)
	{
		// Find or create VFX manager
		VFXManager = Owner->FindComponentByClass<UAoCSpellVFXManager>();
		if (!VFXManager)
		{
			VFXManager = NewObject<UAoCSpellVFXManager>(Owner, TEXT("SpellVFXManager"));
			if (VFXManager) VFXManager->RegisterComponent();
		}

		// Find or create Screen Effects
		ScreenEffects = Owner->FindComponentByClass<UAoCSpellScreenEffects>();
		if (!ScreenEffects)
		{
			ScreenEffects = NewObject<UAoCSpellScreenEffects>(Owner, TEXT("SpellScreenEffects"));
			if (ScreenEffects) ScreenEffects->RegisterComponent();
		}
	}

	// Load all school cast sounds
	LoadSchoolCastSounds();
}

// ─── Sound Loading ──────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::LoadSchoolCastSounds()
{
	// School cast sounds live at /Game/AoC/Sounds/Cast/cast_{school}.cast_{school}
	struct FSchoolSoundPath
	{
		EAoCMagicSchool School;
		const TCHAR* Path;
	};

	static const FSchoolSoundPath Paths[] = {
		{ EAoCMagicSchool::Arcana,       TEXT("/Game/AoC/Sounds/Cast/cast_arcana.cast_arcana") },
		{ EAoCMagicSchool::Pyromancy,    TEXT("/Game/AoC/Sounds/Cast/cast_pyromancy.cast_pyromancy") },
		{ EAoCMagicSchool::Cryomancy,    TEXT("/Game/AoC/Sounds/Cast/cast_cryomancy.cast_cryomancy") },
		{ EAoCMagicSchool::Stormcalling, TEXT("/Game/AoC/Sounds/Cast/cast_stormcalling.cast_stormcalling") },
		{ EAoCMagicSchool::Necromancy,   TEXT("/Game/AoC/Sounds/Cast/cast_necromancy.cast_necromancy") },
		{ EAoCMagicSchool::Verdancy,     TEXT("/Game/AoC/Sounds/Cast/cast_verdancy.cast_verdancy") },
		{ EAoCMagicSchool::Umbramancy,   TEXT("/Game/AoC/Sounds/Cast/cast_umbramancy.cast_umbramancy") },
		{ EAoCMagicSchool::Radiance,     TEXT("/Game/AoC/Sounds/Cast/cast_radiance.cast_radiance") },
		{ EAoCMagicSchool::Sangromancy,  TEXT("/Game/AoC/Sounds/Cast/cast_sangromancy.cast_sangromancy") },
		{ EAoCMagicSchool::Dominion,     TEXT("/Game/AoC/Sounds/Cast/cast_dominion.cast_dominion") },
	};

	int32 LoadedCount = 0;
	for (const auto& Entry : Paths)
	{
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, Entry.Path);
		if (Sound)
		{
			SchoolCastSounds.Add(Entry.School, Sound);
			LoadedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[SpellCasting] Loaded %d/%d school cast sounds"), LoadedCount, 10);
}

// ─── Sound Playback ─────────────────────────────────────────────────────────

float UAoCSpellCastingComponent::GetTierPitchMultiplier(int32 SchoolLevel) const
{
	// Tier-based pitch variation:
	// T1-3  → 1.15x (quick, snappy — apprentice)
	// T4-6  → 1.0x  (normal)
	// T7-9  → 0.9x  (heavier)
	// T10-12 → 0.8x (deep, devastating)
	// T13-16 → 0.7x (ultra-deep, ultimate tier)
	if (SchoolLevel <= 3)  return 1.15f;
	if (SchoolLevel <= 6)  return 1.0f;
	if (SchoolLevel <= 9)  return 0.9f;
	if (SchoolLevel <= 12) return 0.8f;
	return 0.7f;
}

void UAoCSpellCastingComponent::PlayCastSound(EAoCMagicSchool School)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	USoundBase** SoundPtr = SchoolCastSounds.Find(School);
	if (SoundPtr && *SoundPtr)
	{
		// Stop previous cast sound if any
		if (ActiveCastSound && ActiveCastSound->IsPlaying())
		{
			ActiveCastSound->Stop();
		}

		ActiveCastSound = UGameplayStatics::SpawnSoundAttached(
			*SoundPtr,
			Owner->GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::KeepRelativeOffset,
			false, 1.0f, 1.0f, 0.0f
		);
	}
}

void UAoCSpellCastingComponent::PlaySpellEffectSound(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Effect sounds live at /Game/AoC/Sounds/Effects/{SpellID}.{SpellID}
	FString SoundPath = FString::Printf(TEXT("/Game/AoC/Sounds/Effects/%s.%s"),
		*Info.SpellID.ToString(), *Info.SpellID.ToString());

	USoundBase* EffectSound = LoadObject<USoundBase>(nullptr, *SoundPath);
	if (EffectSound)
	{
		float Pitch = GetTierPitchMultiplier(Info.SchoolLevel);

		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(),
			EffectSound,
			Owner->GetActorLocation(),
			1.0f,  // Volume
			Pitch, // Tier-based pitch
			0.0f   // Start time
		);
	}
}

// ─── Screen Effects ─────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::TriggerScreenEffects(EAoCMagicSchool School, int32 Tier)
{
	if (!ScreenEffects) return;

	// Screen effects scale with tier
	float Intensity = FMath::Clamp((float)Tier / 16.0f, 0.1f, 1.0f);
	ScreenEffects->TriggerSpellEffect(School, Intensity);
}

// ─── Tick ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCooldowns(DeltaTime);
	UpdateManaRegen(DeltaTime);
	UpdateCasting(DeltaTime);
	UpdateChanneling(DeltaTime);
}

// ─── Spell bar management ───────────────────────────────────────────────────

void UAoCSpellCastingComponent::AssignSpellToSlot(int32 SlotIndex, FName SpellID)
{
	if (SlotIndex >= 0 && SlotIndex < SpellBar.Num())
	{
		SpellBar[SlotIndex].SpellID = SpellID;
		SpellBar[SlotIndex].CooldownRemaining = 0.f;
		SpellBar[SlotIndex].bOnCooldown = false;
	}
}

void UAoCSpellCastingComponent::CastSpellInSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= SpellBar.Num()) return;

	const FAoCSpellBarSlot& Slot = SpellBar[SlotIndex];
	if (Slot.SpellID.IsNone() || Slot.bOnCooldown) return;

	ExecuteSpell(Slot.SpellID);
}

float UAoCSpellCastingComponent::GetCooldownFraction(int32 SlotIndex) const
{
	if (SlotIndex < 0 || SlotIndex >= SpellBar.Num()) return 0.f;
	const FAoCSpellBarSlot& Slot = SpellBar[SlotIndex];
	if (!Slot.bOnCooldown || Slot.CooldownTotal <= 0.f) return 0.f;
	return FMath::Clamp(Slot.CooldownRemaining / Slot.CooldownTotal, 0.f, 1.f);
}

// ─── Targeting ──────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::SetTarget(AActor* NewTarget)
{
	CurrentTarget = NewTarget;
	if (NewTarget) TargetLocation = NewTarget->GetActorLocation();
}

void UAoCSpellCastingComponent::SetTargetLocation(FVector NewLocation)
{
	TargetLocation = NewLocation;
}

// ─── Can cast check ─────────────────────────────────────────────────────────

bool UAoCSpellCastingComponent::CanCastSpell(FName SpellID) const
{
	if (bIsCasting || bIsChanneling) return false;

	FAoCSpellInfo* Info = UAoCSpellDatabase::FindSpell(SpellID);
	if (!Info) return false;

	if (CurrentMana < Info->ManaCost) return false;

	for (const FAoCSpellBarSlot& Slot : SpellBar)
	{
		if (Slot.SpellID == SpellID && Slot.bOnCooldown) return false;
	}

	if (Info->bRequiresTarget && !CurrentTarget) return false;

	return true;
}

// ─── Main execute entry point ───────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteSpell(FName SpellID)
{
	if (!CanCastSpell(SpellID))
	{
		UE_LOG(LogTemp, Warning, TEXT("Cannot cast spell: %s"), *SpellID.ToString());
		return;
	}

	FAoCSpellInfo* InfoPtr = UAoCSpellDatabase::FindSpell(SpellID);
	if (!InfoPtr) return;

	FAoCSpellInfo Info = *InfoPtr;

	// Deduct mana
	CurrentMana = FMath::Max(0.f, CurrentMana - Info.ManaCost);

	// Start cooldown
	StartCooldown(SpellID, Info.Cooldown);

	// ★ LAYER 1: Play school cast sound immediately
	PlayCastSound(Info.School);

	// If spell has a cast time, begin casting; otherwise execute immediately
	if (Info.CastTime > 0.f)
	{
		bIsCasting = true;
		CurrentCastSpellID = SpellID;
		CastTimeRemaining = Info.CastTime;
		PendingSpell = Info;

		// Cast VFX (charging particles)
		if (VFXManager)
		{
			AActor* Owner = GetOwner();
			if (Owner)
			{
				VFXManager->SpawnCastVFX(Info.School,
					Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
					Owner->GetActorRotation());
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Casting %s (%.1fs)..."), *Info.DisplayName, Info.CastTime);
		return;
	}

	// Instant cast — dispatch immediately
	// ★ LAYER 2: Play spell effect sound
	PlaySpellEffectSound(Info);

	// ★ Screen effects
	TriggerScreenEffects(Info.School, Info.SchoolLevel);

	switch (Info.Type)
	{
	case EAoCSpellType::Projectile: ExecuteProjectileSpell(Info); break;
	case EAoCSpellType::AOE:        ExecuteAOESpell(Info);       break;
	case EAoCSpellType::Instant:    ExecuteInstantSpell(Info);   break;
	case EAoCSpellType::Buff:       ExecuteBuffSpell(Info);      break;
	case EAoCSpellType::DOT:        ExecuteDOTSpell(Info);       break;
	case EAoCSpellType::Channeled:  ExecuteChanneledSpell(Info); break;
	case EAoCSpellType::Beam:       ExecuteBeamSpell(Info);      break;
	case EAoCSpellType::Shield:     ExecuteShieldSpell(Info);    break;
	case EAoCSpellType::Summon:     ExecuteSummonSpell(Info);    break;
	case EAoCSpellType::Self:
		if (Info.BaseDamage > 0.f)
			ExecuteHealSpell(Info);
		else
			ExecuteBuffSpell(Info);
		break;
	default: break;
	}
}

// ─── Cancel cast ────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::CancelCast()
{
	if (bIsCasting)
	{
		bIsCasting = false;
		CastTimeRemaining = 0.f;
		CurrentCastSpellID = NAME_None;

		// Stop cast sound
		if (ActiveCastSound && ActiveCastSound->IsPlaying())
		{
			ActiveCastSound->FadeOut(0.2f, 0.0f);
		}
		UE_LOG(LogTemp, Log, TEXT("Cast cancelled."));
	}

	if (bIsChanneling)
	{
		bIsChanneling = false;
		ChannelTimeRemaining = 0.f;
		ChannelTickAccumulator = 0.f;
		UE_LOG(LogTemp, Log, TEXT("Channel cancelled."));
	}
}

// ─── Update loops ───────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::UpdateCooldowns(float DeltaTime)
{
	for (FAoCSpellBarSlot& Slot : SpellBar)
	{
		if (Slot.bOnCooldown)
		{
			Slot.CooldownRemaining -= DeltaTime;
			if (Slot.CooldownRemaining <= 0.f)
			{
				Slot.CooldownRemaining = 0.f;
				Slot.bOnCooldown = false;
			}
		}
	}
}

void UAoCSpellCastingComponent::UpdateManaRegen(float DeltaTime)
{
	if (CurrentMana < MaxMana)
	{
		CurrentMana = FMath::Min(MaxMana, CurrentMana + ManaRegenRate * DeltaTime);
	}
}

void UAoCSpellCastingComponent::UpdateCasting(float DeltaTime)
{
	if (!bIsCasting) return;

	CastTimeRemaining -= DeltaTime;
	if (CastTimeRemaining <= 0.f)
	{
		bIsCasting = false;
		CastTimeRemaining = 0.f;
		CurrentCastSpellID = NAME_None;

		UE_LOG(LogTemp, Log, TEXT("Cast complete: %s"), *PendingSpell.DisplayName);

		// ★ LAYER 2: Play spell effect sound on cast completion
		PlaySpellEffectSound(PendingSpell);

		// ★ Screen effects
		TriggerScreenEffects(PendingSpell.School, PendingSpell.SchoolLevel);

		// Dispatch by type
		switch (PendingSpell.Type)
		{
		case EAoCSpellType::Projectile: ExecuteProjectileSpell(PendingSpell); break;
		case EAoCSpellType::AOE:        ExecuteAOESpell(PendingSpell);       break;
		case EAoCSpellType::Instant:    ExecuteInstantSpell(PendingSpell);   break;
		case EAoCSpellType::Buff:       ExecuteBuffSpell(PendingSpell);      break;
		case EAoCSpellType::DOT:        ExecuteDOTSpell(PendingSpell);       break;
		case EAoCSpellType::Channeled:  ExecuteChanneledSpell(PendingSpell); break;
		case EAoCSpellType::Beam:       ExecuteBeamSpell(PendingSpell);      break;
		case EAoCSpellType::Shield:     ExecuteShieldSpell(PendingSpell);    break;
		case EAoCSpellType::Summon:     ExecuteSummonSpell(PendingSpell);    break;
		case EAoCSpellType::Self:
			if (PendingSpell.BaseDamage > 0.f)
				ExecuteHealSpell(PendingSpell);
			else
				ExecuteBuffSpell(PendingSpell);
			break;
		default: break;
		}
	}
}

void UAoCSpellCastingComponent::UpdateChanneling(float DeltaTime)
{
	if (!bIsChanneling) return;

	ChannelTimeRemaining -= DeltaTime;
	ChannelTickAccumulator += DeltaTime;

	if (ChannelTickAccumulator >= ChannelTickInterval)
	{
		ChannelTickAccumulator -= ChannelTickInterval;

		if (CurrentTarget)
		{
			ApplySpellDamage(CurrentTarget, ActiveChannelSpell.BaseDamage, ActiveChannelSpell);

			if (VFXManager && ActiveChannelSpell.Type == EAoCSpellType::Beam)
			{
				AActor* Owner = GetOwner();
				if (Owner)
				{
					VFXManager->SpawnProjectileVFX(ActiveChannelSpell.School,
						Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
						CurrentTarget->GetActorLocation(), 0.f);
				}
			}
		}
	}

	if (ChannelTimeRemaining <= 0.f)
	{
		bIsChanneling = false;
		ChannelTimeRemaining = 0.f;
		ChannelTickAccumulator = 0.f;
		UE_LOG(LogTemp, Log, TEXT("Channel complete: %s"), *ActiveChannelSpell.DisplayName);
	}
}

// ─── Cooldown ───────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::StartCooldown(FName SpellID, float CooldownDuration)
{
	for (FAoCSpellBarSlot& Slot : SpellBar)
	{
		if (Slot.SpellID == SpellID)
		{
			Slot.CooldownTotal = CooldownDuration;
			Slot.CooldownRemaining = CooldownDuration;
			Slot.bOnCooldown = true;
		}
	}
}

// ─── Damage helper ──────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ApplySpellDamage(AActor* Target, float DamageAmount,
                                                  const FAoCSpellInfo& Info)
{
	if (!Target || DamageAmount <= 0.f) return;

	AActor* Owner = GetOwner();
	AController* InstigatorController = nullptr;

	if (Owner)
	{
		APawn* Pawn = Cast<APawn>(Owner);
		if (Pawn) InstigatorController = Pawn->GetController();
	}

	UGameplayStatics::ApplyDamage(Target, DamageAmount, InstigatorController, Owner, nullptr);

	UE_LOG(LogTemp, Log, TEXT("[%s] dealt %.1f damage to %s"),
		*Info.DisplayName, DamageAmount, *Target->GetName());
}

// ─── Projectile ─────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteProjectileSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;

	FVector SpawnLoc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 100.f + FVector(0.f, 0.f, 60.f);
	FRotator SpawnRot = Owner->GetActorRotation();

	if (CurrentTarget)
	{
		FVector Dir = (CurrentTarget->GetActorLocation() - SpawnLoc).GetSafeNormal();
		SpawnRot = Dir.Rotation();
	}

	for (int32 i = 0; i < Info.NumberOfProjectiles; ++i)
	{
		FRotator ProjRot = SpawnRot;
		if (Info.NumberOfProjectiles > 1)
		{
			float SpreadAngle = 10.f;
			float TotalSpread = SpreadAngle * (Info.NumberOfProjectiles - 1);
			float Offset = -TotalSpread * 0.5f + SpreadAngle * i;
			ProjRot.Yaw += Offset;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Owner;
		SpawnParams.Instigator = Cast<APawn>(Owner);
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AAoCProjectile* Projectile = World->SpawnActor<AAoCProjectile>(
			AAoCProjectile::StaticClass(), SpawnLoc, ProjRot, SpawnParams);

		if (Projectile)
		{
			Projectile->InitializeProjectile(Info, Owner);

			if (VFXManager)
			{
				FVector TargetLoc = CurrentTarget ? CurrentTarget->GetActorLocation() : (SpawnLoc + ProjRot.Vector() * Info.Range);
				VFXManager->SpawnProjectileVFX(Info.School, SpawnLoc, TargetLoc, Info.ProjectileSpeed);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Fired %d projectile(s): %s"), Info.NumberOfProjectiles, *Info.DisplayName);
}

// ─── AOE ────────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteAOESpell(const FAoCSpellInfo& Info)
{
	UWorld* World = GetWorld();
	AActor* Owner = GetOwner();
	if (!World || !Owner) return;

	FVector Center = TargetLocation;
	if (CurrentTarget) Center = CurrentTarget->GetActorLocation();

	TArray<FHitResult> HitResults;
	FCollisionShape Shape = FCollisionShape::MakeSphere(Info.Radius);
	bool bHit = World->SweepMultiByChannel(
		HitResults, Center, Center + FVector(0.f, 0.f, 1.f),
		FQuat::Identity, ECC_Pawn, Shape);

	if (bHit)
	{
		TSet<AActor*> AlreadyHit;
		for (const FHitResult& Hit : HitResults)
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor && HitActor != Owner && !AlreadyHit.Contains(HitActor))
			{
				AlreadyHit.Add(HitActor);
				ApplySpellDamage(HitActor, Info.BaseDamage, Info);
			}
		}
	}

	if (VFXManager)
	{
		VFXManager->SpawnImpactVFX(Info.School, Center);
	}

	UE_LOG(LogTemp, Log, TEXT("AOE %s at (%.0f, %.0f, %.0f) radius %.0f"),
		*Info.DisplayName, Center.X, Center.Y, Center.Z, Info.Radius);
}

// ─── Instant ────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteInstantSpell(const FAoCSpellInfo& Info)
{
	if (CurrentTarget)
	{
		ApplySpellDamage(CurrentTarget, Info.BaseDamage, Info);
		if (VFXManager) VFXManager->SpawnImpactVFX(Info.School, CurrentTarget->GetActorLocation());
	}
	UE_LOG(LogTemp, Log, TEXT("Instant spell: %s"), *Info.DisplayName);
}

// ─── Buff ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteBuffSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	AActor* BuffTarget = Info.bRequiresTarget ? CurrentTarget : Owner;
	if (!BuffTarget) BuffTarget = Owner;

	if (VFXManager) VFXManager->SpawnImpactVFX(Info.School, BuffTarget->GetActorLocation());

	UE_LOG(LogTemp, Log, TEXT("Buff applied: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);
}

// ─── DOT ────────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteDOTSpell(const FAoCSpellInfo& Info)
{
	if (!CurrentTarget) return;

	bIsChanneling = true;
	ChannelTimeRemaining = Info.Duration;
	ChannelTickInterval = 2.0f;
	ChannelTickAccumulator = 0.f;
	ActiveChannelSpell = Info;

	if (VFXManager) VFXManager->SpawnImpactVFX(Info.School, CurrentTarget->GetActorLocation());

	UE_LOG(LogTemp, Log, TEXT("DOT applied: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);
}

// ─── Channeled ──────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteChanneledSpell(const FAoCSpellInfo& Info)
{
	bIsChanneling = true;
	ChannelTimeRemaining = Info.Duration;
	ChannelTickInterval = 0.5f;
	ChannelTickAccumulator = 0.f;
	ActiveChannelSpell = Info;

	AActor* Owner = GetOwner();
	if (VFXManager && Owner)
	{
		VFXManager->SpawnCastVFX(Info.School,
			Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
			Owner->GetActorRotation());
	}

	UE_LOG(LogTemp, Log, TEXT("Channeling: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);
}

// ─── Beam ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteBeamSpell(const FAoCSpellInfo& Info)
{
	if (!CurrentTarget) return;
	AActor* Owner = GetOwner();
	if (!Owner) return;

	bIsChanneling = true;
	ChannelTimeRemaining = Info.Duration;
	ChannelTickInterval = 0.3f;
	ChannelTickAccumulator = 0.f;
	ActiveChannelSpell = Info;

	if (VFXManager)
	{
		VFXManager->SpawnProjectileVFX(Info.School,
			Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
			CurrentTarget->GetActorLocation(), 0.f);
	}

	UE_LOG(LogTemp, Log, TEXT("Beam: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);
}

// ─── Shield ─────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteShieldSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	AActor* ShieldTarget = Info.bRequiresTarget ? CurrentTarget : Owner;
	if (!ShieldTarget) ShieldTarget = Owner;

	if (VFXManager) VFXManager->SpawnImpactVFX(Info.School, ShieldTarget->GetActorLocation());

	UE_LOG(LogTemp, Log, TEXT("Shield: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);
}

// ─── Heal ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteHealSpell(const FAoCSpellInfo& Info)
{
	AActor* HealTarget = Info.bRequiresTarget ? CurrentTarget : GetOwner();
	if (!HealTarget) HealTarget = GetOwner();
	if (!HealTarget) return;

	UE_LOG(LogTemp, Log, TEXT("Healed %s for %.1f HP with %s"),
		*HealTarget->GetName(), Info.BaseDamage, *Info.DisplayName);

	if (VFXManager) VFXManager->SpawnImpactVFX(Info.School, HealTarget->GetActorLocation());
}

// ─── Summon ─────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteSummonSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;

	FVector SpawnLoc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 200.f;

	if (VFXManager)
	{
		VFXManager->SpawnImpactVFX(Info.School, SpawnLoc);
	}

	UE_LOG(LogTemp, Log, TEXT("Summoned: %s for %.1f seconds at (%.0f, %.0f, %.0f)"),
		*Info.DisplayName, Info.Duration, SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);
}
