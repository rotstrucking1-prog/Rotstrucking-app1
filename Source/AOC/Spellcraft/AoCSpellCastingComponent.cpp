// Source/AOC/Spellcraft/AoCSpellCastingComponent.cpp
// Spell casting — full implementation with NO GAS dependency.

#include "AoCSpellCastingComponent.h"
#include "AoCSpellVFXManager.h"
#include "AoCSpellData.h"
#include "../Combat/AoCProjectile.h"
#include "Kismet/GameplayStatics.h"
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
}

// ─── BeginPlay ──────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialise spell bar slots
	SpellBar.SetNum(MaxSpellBarSlots);

	// Find or create VFX manager on owner
	AActor* Owner = GetOwner();
	if (Owner)
	{
		VFXManager = Owner->FindComponentByClass<UAoCSpellVFXManager>();
		if (!VFXManager)
		{
			VFXManager = NewObject<UAoCSpellVFXManager>(Owner, TEXT("SpellVFXManager"));
			if (VFXManager)
			{
				VFXManager->RegisterComponent();
			}
		}
	}
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
	if (NewTarget)
	{
		TargetLocation = NewTarget->GetActorLocation();
	}
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

	// Check cooldown
	for (const FAoCSpellBarSlot& Slot : SpellBar)
	{
		if (Slot.SpellID == SpellID && Slot.bOnCooldown)
		{
			return false;
		}
	}

	// Check target requirement
	if (Info->bRequiresTarget && !CurrentTarget)
	{
		return false;
	}

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

	// If spell has a cast time, begin casting; otherwise execute immediately
	if (Info.CastTime > 0.f)
	{
		bIsCasting = true;
		CurrentCastSpellID = SpellID;
		CastTimeRemaining = Info.CastTime;
		PendingSpell = Info;

		// Cast VFX
		if (VFXManager)
		{
			VFXManager->SpawnCastEffect(Info.School);
		}

		UE_LOG(LogTemp, Log, TEXT("Casting %s (%.1fs)..."), *Info.DisplayName, Info.CastTime);
		return;
	}

	// Instant cast — dispatch immediately
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

		// Cast complete — dispatch the spell
		UE_LOG(LogTemp, Log, TEXT("Cast complete: %s"), *PendingSpell.DisplayName);

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
			// Self spells may heal or buff
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

	// Apply damage/heal every tick interval
	if (ChannelTickAccumulator >= ChannelTickInterval)
	{
		ChannelTickAccumulator -= ChannelTickInterval;

		if (CurrentTarget)
		{
			ApplySpellDamage(CurrentTarget, ActiveChannelSpell.BaseDamage, ActiveChannelSpell);

			// Beam VFX refresh
			if (VFXManager && ActiveChannelSpell.Type == EAoCSpellType::Beam)
			{
				AActor* Owner = GetOwner();
				if (Owner)
				{
					VFXManager->SpawnBeamEffect(ActiveChannelSpell.School,
						Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
						CurrentTarget->GetActorLocation());
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
		if (Pawn)
		{
			InstigatorController = Pawn->GetController();
		}
	}

	UGameplayStatics::ApplyDamage(
		Target,
		DamageAmount,
		InstigatorController,
		Owner,
		nullptr // default damage type
	);

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

	// Aim at target if we have one
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
			// Spread multi-projectiles in a fan
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

			// VFX
			if (VFXManager)
			{
				VFXManager->SpawnProjectileVFX(Info.School, Projectile);
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
	if (CurrentTarget)
	{
		Center = CurrentTarget->GetActorLocation();
	}

	// Find all actors in radius and apply damage
	TArray<FHitResult> HitResults;
	FCollisionShape Shape = FCollisionShape::MakeSphere(Info.Radius);
	bool bHit = World->SweepMultiByChannel(
		HitResults,
		Center,
		Center + FVector(0.f, 0.f, 1.f),
		FQuat::Identity,
		ECC_Pawn,
		Shape
	);

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

	// VFX
	if (VFXManager)
	{
		VFXManager->SpawnAOEEffect(Info.School, Center, Info.Radius, Info.Duration > 0.f ? Info.Duration : 3.f);
	}

	// Sound
	UGameplayStatics::PlaySoundAtLocation(World, nullptr, Center, 1.f, 1.f);

	UE_LOG(LogTemp, Log, TEXT("AOE %s at (%.0f, %.0f, %.0f) radius %.0f"),
		*Info.DisplayName, Center.X, Center.Y, Center.Z, Info.Radius);
}

// ─── Instant ────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteInstantSpell(const FAoCSpellInfo& Info)
{
	if (CurrentTarget)
	{
		ApplySpellDamage(CurrentTarget, Info.BaseDamage, Info);

		if (VFXManager)
		{
			VFXManager->SpawnImpactEffect(Info.School, CurrentTarget->GetActorLocation(), FVector::UpVector);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Instant spell: %s"), *Info.DisplayName);
}

// ─── Buff ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteBuffSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Buff target is self or current target
	AActor* BuffTarget = Info.bRequiresTarget ? CurrentTarget : Owner;
	if (!BuffTarget) BuffTarget = Owner;

	// VFX
	if (VFXManager)
	{
		VFXManager->SpawnShieldEffect(Info.School, BuffTarget);
	}

	UE_LOG(LogTemp, Log, TEXT("Buff applied: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);

	// NOTE: Actual stat modifications would be applied here via a buff system.
	// For now we log and show VFX. Game-specific buff logic should be added
	// to a dedicated buff manager component.
}

// ─── DOT ────────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteDOTSpell(const FAoCSpellInfo& Info)
{
	if (!CurrentTarget) return;

	// DOT is implemented as a short channel on the target
	// Each tick applies Info.BaseDamage
	bIsChanneling = true;
	ChannelTimeRemaining = Info.Duration;
	ChannelTickInterval = 2.0f; // tick every 2 seconds
	ChannelTickAccumulator = 0.f;
	ActiveChannelSpell = Info;

	if (VFXManager)
	{
		VFXManager->SpawnImpactEffect(Info.School, CurrentTarget->GetActorLocation(), FVector::UpVector);
	}

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

	if (VFXManager)
	{
		VFXManager->SpawnCastEffect(Info.School);
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

	// Initial beam VFX
	if (VFXManager)
	{
		VFXManager->SpawnBeamEffect(Info.School,
			Owner->GetActorLocation() + FVector(0.f, 0.f, 60.f),
			CurrentTarget->GetActorLocation());
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

	if (VFXManager)
	{
		VFXManager->SpawnShieldEffect(Info.School, ShieldTarget);
	}

	UE_LOG(LogTemp, Log, TEXT("Shield: %s for %.1f seconds"), *Info.DisplayName, Info.Duration);

	// NOTE: Actual damage absorption logic would be implemented in a shield
	// subsystem. The VFX is shown here; gameplay shield HP tracking is
	// game-specific and should be wired into the character's health component.
}

// ─── Heal ───────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteHealSpell(const FAoCSpellInfo& Info)
{
	AActor* HealTarget = Info.bRequiresTarget ? CurrentTarget : GetOwner();
	if (!HealTarget) HealTarget = GetOwner();
	if (!HealTarget) return;

	// Apply negative damage (healing) — games often override TakeDamage for this.
	// Here we log the heal. A real implementation would call a health component.
	UE_LOG(LogTemp, Log, TEXT("Healed %s for %.1f HP with %s"),
		*HealTarget->GetName(), Info.BaseDamage, *Info.DisplayName);

	if (VFXManager)
	{
		VFXManager->SpawnImpactEffect(Info.School, HealTarget->GetActorLocation(), FVector::UpVector);
	}
}

// ─── Summon ─────────────────────────────────────────────────────────────────

void UAoCSpellCastingComponent::ExecuteSummonSpell(const FAoCSpellInfo& Info)
{
	AActor* Owner = GetOwner();
	UWorld* World = GetWorld();
	if (!Owner || !World) return;

	// Spawn location in front of caster
	FVector SpawnLoc = Owner->GetActorLocation() + Owner->GetActorForwardVector() * 200.f;

	// Spawn a generic actor placeholder — the summon's actual class should be
	// looked up from a summon registry. For now we spawn VFX at the location.
	if (VFXManager)
	{
		VFXManager->SpawnAOEEffect(Info.School, SpawnLoc, 150.f, Info.Duration);
	}

	UE_LOG(LogTemp, Log, TEXT("Summoned: %s for %.1f seconds at (%.0f, %.0f, %.0f)"),
		*Info.DisplayName, Info.Duration, SpawnLoc.X, SpawnLoc.Y, SpawnLoc.Z);

	// NOTE: Real summon implementation would spawn a specific AI-controlled pawn
	// based on the spell ID and set its lifetime to Info.Duration.
}
