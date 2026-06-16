// AoCNPCCombatBrain.cpp
// Deep combat AI — full implementation
// Pattern recognition, adaptive rotations, weapon switching, consumables, group tactics

#include "AoCNPCCombatBrain.h"
#include "AoCHumanoidNPCV2.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// Forward-declare component headers we reference
// These exist in the project already
// #include "AoCNPCInventory.h"
// #include "AoCNPCPersonality.h"
// #include "AoCNPCMemory.h"

DEFINE_LOG_CATEGORY(LogAoCCombat);

// ---------------------------------------------------------------------------
// Ability Database (built-in abilities keyed by skill+level)
// ---------------------------------------------------------------------------

namespace AbilityDB
{
	// Helper to create an ability entry quickly
	static FCombatAbility Make(FName Name, ESkillID Req, float MinLvl, float Dmg, float Mana,
		float Stam, float CD, float Range, EAbilitySlot Slot,
		bool bAoE = false, bool bHeal = false, bool bInterrupt = false,
		bool bRanged = false, bool bMagic = false, bool bBow = false)
	{
		FCombatAbility A;
		A.AbilityName = Name;
		A.RequiredSkill = Req;
		A.MinSkillLevel = MinLvl;
		A.BaseDamage = Dmg;
		A.ManaCost = Mana;
		A.StaminaCost = Stam;
		A.CooldownDuration = CD;
		A.Range = Range;
		A.PreferredSlot = Slot;
		A.bIsAoE = bAoE;
		A.bIsHeal = bHeal;
		A.bIsInterrupt = bInterrupt;
		A.bIsRanged = bRanged;
		A.bIsMagic = bMagic;
		A.bRequiresBow = bBow;
		return A;
	}

	static TArray<FCombatAbility> GetAllAbilities()
	{
		TArray<FCombatAbility> All;

		// --- Melee Weapon Abilities ---
		// Sword
		All.Add(Make(TEXT("SlashCombo"),     ESkillID::Sword, 1.f, 15.f, 0, 15, 1.5f, 250, EAbilitySlot::Core));
		All.Add(Make(TEXT("ThrustStrike"),   ESkillID::Sword, 10.f, 25.f, 0, 20, 3.0f, 250, EAbilitySlot::Core));
		All.Add(Make(TEXT("RisingSlash"),    ESkillID::Sword, 20.f, 35.f, 0, 25, 5.0f, 250, EAbilitySlot::Opener));
		All.Add(Make(TEXT("ExecuteSlash"),   ESkillID::Sword, 40.f, 60.f, 0, 35, 10.f, 250, EAbilitySlot::Finisher));
		All.Add(Make(TEXT("BladeDance"),     ESkillID::Sword, 60.f, 45.f, 0, 30, 8.f, 300, EAbilitySlot::Core, true));
		All.Add(Make(TEXT("PommelStrike"),   ESkillID::Sword, 25.f, 10.f, 0, 15, 6.f, 200, EAbilitySlot::Interrupt, false, false, true));

		// Greatsword
		All.Add(Make(TEXT("CleaveSwing"),    ESkillID::Greatsword, 1.f, 22.f, 0, 20, 2.0f, 300, EAbilitySlot::Core, true));
		All.Add(Make(TEXT("OverheadSmash"),  ESkillID::Greatsword, 15.f, 40.f, 0, 30, 4.0f, 280, EAbilitySlot::Opener));
		All.Add(Make(TEXT("WhirlwindSlash"), ESkillID::Greatsword, 35.f, 55.f, 0, 40, 8.0f, 350, EAbilitySlot::Core, true));
		All.Add(Make(TEXT("DecapitateSwing"),ESkillID::Greatsword, 50.f, 80.f, 0, 50, 15.f, 280, EAbilitySlot::Finisher));

		// Dagger
		All.Add(Make(TEXT("QuickStab"),      ESkillID::Dagger, 1.f, 10.f, 0, 8, 0.8f, 180, EAbilitySlot::Core));
		All.Add(Make(TEXT("Backstab"),       ESkillID::Dagger, 15.f, 45.f, 0, 20, 6.f, 180, EAbilitySlot::Opener));
		All.Add(Make(TEXT("PoisonStrike"),   ESkillID::Dagger, 25.f, 20.f, 0, 15, 8.f, 180, EAbilitySlot::Core));
		All.Add(Make(TEXT("FatalStrike"),    ESkillID::Dagger, 45.f, 70.f, 0, 35, 12.f, 180, EAbilitySlot::Finisher));
		All.Add(Make(TEXT("TwinStab"),       ESkillID::Dagger, 30.f, 30.f, 0, 18, 3.f, 180, EAbilitySlot::Core));

		// Axe
		All.Add(Make(TEXT("ChopSwing"),      ESkillID::Axe, 1.f, 18.f, 0, 18, 1.8f, 230, EAbilitySlot::Core));
		All.Add(Make(TEXT("RendingChop"),    ESkillID::Axe, 20.f, 35.f, 0, 25, 5.f, 230, EAbilitySlot::Core));
		All.Add(Make(TEXT("SkullSplitter"),  ESkillID::Axe, 40.f, 55.f, 0, 35, 10.f, 230, EAbilitySlot::Finisher));

		// Great Axe
		All.Add(Make(TEXT("MightySwing"),    ESkillID::GreatAxe, 1.f, 28.f, 0, 25, 2.5f, 300, EAbilitySlot::Core));
		All.Add(Make(TEXT("GroundSplitter"), ESkillID::GreatAxe, 25.f, 50.f, 0, 40, 7.f, 350, EAbilitySlot::Core, true));
		All.Add(Make(TEXT("BerserkerChop"),  ESkillID::GreatAxe, 50.f, 90.f, 0, 55, 15.f, 300, EAbilitySlot::Finisher));

		// Hammer/GreatHammer
		All.Add(Make(TEXT("CrushBlow"),      ESkillID::Hammer, 1.f, 20.f, 0, 20, 2.0f, 220, EAbilitySlot::Core));
		All.Add(Make(TEXT("StunBash"),       ESkillID::Hammer, 20.f, 15.f, 0, 25, 8.f, 220, EAbilitySlot::Opener, false, false, true));
		All.Add(Make(TEXT("EarthShatter"),   ESkillID::GreatHammer, 1.f, 30.f, 0, 30, 2.5f, 280, EAbilitySlot::Core, true));
		All.Add(Make(TEXT("JudgmentBlow"),   ESkillID::GreatHammer, 40.f, 70.f, 0, 50, 12.f, 280, EAbilitySlot::Finisher));

		// Mace
		All.Add(Make(TEXT("CrackBlow"),      ESkillID::Mace, 1.f, 16.f, 0, 15, 1.5f, 220, EAbilitySlot::Core));
		All.Add(Make(TEXT("ArmorCrush"),     ESkillID::Mace, 25.f, 30.f, 0, 25, 5.f, 220, EAbilitySlot::Core));
		All.Add(Make(TEXT("GreatSmite"),     ESkillID::GreatMace, 1.f, 25.f, 0, 25, 2.f, 280, EAbilitySlot::Core));

		// Spear
		All.Add(Make(TEXT("PierceThrust"),   ESkillID::Spear, 1.f, 18.f, 0, 12, 1.3f, 350, EAbilitySlot::Core));
		All.Add(Make(TEXT("LungingStrike"),  ESkillID::Spear, 15.f, 30.f, 0, 20, 4.f, 400, EAbilitySlot::Opener));
		All.Add(Make(TEXT("ImpaleStrike"),   ESkillID::Spear, 40.f, 55.f, 0, 35, 10.f, 350, EAbilitySlot::Finisher));
		All.Add(Make(TEXT("SpearSweep"),     ESkillID::Spear, 25.f, 25.f, 0, 20, 5.f, 350, EAbilitySlot::Core, true));

		// Scythe
		All.Add(Make(TEXT("ReapSwing"),      ESkillID::Scythe, 1.f, 20.f, 0, 18, 1.8f, 320, EAbilitySlot::Core));
		All.Add(Make(TEXT("HarvestSweep"),   ESkillID::Scythe, 30.f, 40.f, 0, 30, 6.f, 350, EAbilitySlot::Core, true));

		// Claws
		All.Add(Make(TEXT("RakeClaw"),       ESkillID::Claws, 1.f, 12.f, 0, 8, 0.7f, 180, EAbilitySlot::Core));
		All.Add(Make(TEXT("FrenzySwipes"),   ESkillID::Claws, 20.f, 35.f, 0, 25, 4.f, 200, EAbilitySlot::Core));
		All.Add(Make(TEXT("DisembowelClaw"), ESkillID::Claws, 45.f, 65.f, 0, 40, 12.f, 180, EAbilitySlot::Finisher));

		// Bow
		All.Add(Make(TEXT("QuickShot"),      ESkillID::Bow, 1.f, 15.f, 0, 10, 1.5f, 5000, EAbilitySlot::Core, false, false, false, true, false, true));
		All.Add(Make(TEXT("PowerShot"),      ESkillID::Bow, 15.f, 30.f, 0, 20, 3.f, 5000, EAbilitySlot::Core, false, false, false, true, false, true));
		All.Add(Make(TEXT("PinShot"),        ESkillID::Bow, 25.f, 20.f, 0, 25, 8.f, 5000, EAbilitySlot::Opener, false, false, false, true, false, true));
		All.Add(Make(TEXT("ArrowVolley"),    ESkillID::Bow, 35.f, 45.f, 0, 35, 10.f, 4000, EAbilitySlot::Core, true, false, false, true, false, true));
		All.Add(Make(TEXT("HeadshotArrow"),  ESkillID::Bow, 50.f, 80.f, 0, 40, 15.f, 5000, EAbilitySlot::Finisher, false, false, false, true, false, true));

		// Shield
		All.Add(Make(TEXT("ShieldBash"),     ESkillID::Shield, 10.f, 10.f, 0, 15, 5.f, 200, EAbilitySlot::Interrupt, false, false, true));
		All.Add(Make(TEXT("ShieldBlock"),    ESkillID::Shield, 1.f, 0.f, 0, 10, 2.f, 0, EAbilitySlot::Defensive));

		// Staff
		All.Add(Make(TEXT("StaffSmack"),     ESkillID::Staff1H, 1.f, 12.f, 0, 10, 1.5f, 250, EAbilitySlot::Core));
		All.Add(Make(TEXT("StaffSweep"),     ESkillID::Staff2H, 1.f, 18.f, 0, 15, 1.8f, 300, EAbilitySlot::Core));

		// Unarmed
		All.Add(Make(TEXT("Punch"),          ESkillID::Unarmed, 1.f, 8.f, 0, 8, 0.8f, 180, EAbilitySlot::Core));
		All.Add(Make(TEXT("KickStrike"),     ESkillID::Unarmed, 10.f, 15.f, 0, 12, 2.f, 200, EAbilitySlot::Core));
		All.Add(Make(TEXT("FlurryBlows"),    ESkillID::Unarmed, 30.f, 25.f, 0, 20, 5.f, 180, EAbilitySlot::Core));

		// --- Magic Abilities ---
		// Pyromancy
		All.Add(Make(TEXT("Firebolt"),       ESkillID::Pyromancy, 1.f, 20.f, 10, 0, 2.f, 4000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("FlameBlast"),     ESkillID::Pyromancy, 15.f, 35.f, 20, 0, 4.f, 3500, EAbilitySlot::Core, true, false, false, true, true));
		All.Add(Make(TEXT("Fireball"),       ESkillID::Pyromancy, 25.f, 50.f, 30, 0, 6.f, 4000, EAbilitySlot::Opener, true, false, false, true, true));
		All.Add(Make(TEXT("MeteorStrike"),   ESkillID::Pyromancy, 50.f, 100.f, 60, 0, 20.f, 4000, EAbilitySlot::Finisher, true, false, false, true, true));
		All.Add(Make(TEXT("FlameShield"),    ESkillID::Pyromancy, 20.f, 0.f, 25, 0, 15.f, 0, EAbilitySlot::Defensive, false, false, false, false, true));

		// Cryomancy
		All.Add(Make(TEXT("IceShard"),       ESkillID::Cryomancy, 1.f, 18.f, 10, 0, 2.f, 4000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("FrostNova"),      ESkillID::Cryomancy, 20.f, 30.f, 25, 0, 8.f, 600, EAbilitySlot::Defensive, true, false, false, false, true));
		All.Add(Make(TEXT("FreezeRoot"),     ESkillID::Cryomancy, 15.f, 5.f, 20, 0, 10.f, 3500, EAbilitySlot::Opener, false, false, false, true, true));
		All.Add(Make(TEXT("IceSpike"),       ESkillID::Cryomancy, 30.f, 45.f, 30, 0, 5.f, 4000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("Blizzard"),       ESkillID::Cryomancy, 50.f, 80.f, 55, 0, 18.f, 3500, EAbilitySlot::Finisher, true, false, false, true, true));

		// Stormcalling
		All.Add(Make(TEXT("LightningBolt"), ESkillID::Stormcalling, 1.f, 22.f, 12, 0, 2.5f, 4500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("ThunderStrike"), ESkillID::Stormcalling, 20.f, 40.f, 25, 0, 6.f, 4000, EAbilitySlot::Core, false, false, true, true, true));
		All.Add(Make(TEXT("ChainLightning"),ESkillID::Stormcalling, 35.f, 55.f, 35, 0, 10.f, 4000, EAbilitySlot::Core, true, false, false, true, true));
		All.Add(Make(TEXT("Tempest_Storm"), ESkillID::Stormcalling, 50.f, 90.f, 55, 0, 20.f, 3500, EAbilitySlot::Panic, true, false, false, true, true));

		// Verdancy (healing/nature)
		All.Add(Make(TEXT("NaturalMend"),   ESkillID::Verdancy, 1.f, 0.f, 15, 0, 3.f, 0, EAbilitySlot::Defensive, false, true, false, false, true));
		All.Add(Make(TEXT("ThornWhip"),     ESkillID::Verdancy, 10.f, 20.f, 12, 0, 3.f, 3000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("EntangleRoots"), ESkillID::Verdancy, 20.f, 5.f, 20, 0, 12.f, 3000, EAbilitySlot::Opener, false, false, false, true, true));
		All.Add(Make(TEXT("GreatHeal"),     ESkillID::Verdancy, 35.f, 0.f, 40, 0, 10.f, 0, EAbilitySlot::Panic, false, true, false, false, true));

		// Radiance (light/holy)
		All.Add(Make(TEXT("HolyLight"),     ESkillID::Radiance, 1.f, 15.f, 10, 0, 2.f, 3500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("Smite"),         ESkillID::Radiance, 20.f, 35.f, 20, 0, 5.f, 3500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("DivineShield"),  ESkillID::Radiance, 25.f, 0.f, 30, 0, 20.f, 0, EAbilitySlot::Panic, false, false, false, false, true));
		All.Add(Make(TEXT("Purify"),        ESkillID::Radiance, 15.f, 0.f, 15, 0, 8.f, 0, EAbilitySlot::Defensive, false, true, false, false, true));

		// Umbramancy (shadow/dark)
		All.Add(Make(TEXT("ShadowBolt"),    ESkillID::Umbramancy, 1.f, 20.f, 10, 0, 2.f, 4000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("LifeDrain"),     ESkillID::Umbramancy, 20.f, 25.f, 20, 0, 6.f, 3000, EAbilitySlot::Core, false, true, false, true, true));
		All.Add(Make(TEXT("DarkPulse"),     ESkillID::Umbramancy, 35.f, 50.f, 35, 0, 10.f, 800, EAbilitySlot::Core, true, false, false, false, true));
		All.Add(Make(TEXT("VoidEruption"),  ESkillID::Umbramancy, 50.f, 85.f, 55, 0, 18.f, 3500, EAbilitySlot::Finisher, true, false, false, true, true));

		// Sangromancy (blood magic)
		All.Add(Make(TEXT("BloodBolt"),     ESkillID::Sangromancy, 1.f, 22.f, 8, 5, 2.f, 3500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("BloodRitual"),   ESkillID::Sangromancy, 25.f, 0.f, 5, 10, 12.f, 0, EAbilitySlot::Defensive, false, true, false, false, true));
		All.Add(Make(TEXT("Hemorrhage"),    ESkillID::Sangromancy, 35.f, 40.f, 15, 8, 8.f, 3000, EAbilitySlot::Core, false, false, false, true, true));

		// Dominion (control/mind)
		All.Add(Make(TEXT("PsychicLash"),   ESkillID::Dominion, 1.f, 15.f, 12, 0, 2.5f, 3500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("MindBlast"),     ESkillID::Dominion, 20.f, 10.f, 20, 0, 10.f, 3000, EAbilitySlot::Interrupt, false, false, true, true, true));
		All.Add(Make(TEXT("Dominate"),      ESkillID::Dominion, 45.f, 0.f, 45, 0, 25.f, 2500, EAbilitySlot::Opener, false, false, false, true, true));

		// Arcana (general/utility magic)
		All.Add(Make(TEXT("ArcaneMissile"), ESkillID::Arcana, 1.f, 16.f, 8, 0, 1.8f, 4000, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("ArcaneBarrier"), ESkillID::Arcana, 15.f, 0.f, 20, 0, 12.f, 0, EAbilitySlot::Defensive, false, false, false, false, true));
		All.Add(Make(TEXT("ArcaneBlast"),   ESkillID::Arcana, 30.f, 45.f, 30, 0, 8.f, 3500, EAbilitySlot::Core, true, false, false, true, true));

		// Tempest (wind/force)
		All.Add(Make(TEXT("WindSlash"),     ESkillID::Tempest, 1.f, 18.f, 10, 0, 2.f, 3500, EAbilitySlot::Core, false, false, false, true, true));
		All.Add(Make(TEXT("Gust"),          ESkillID::Tempest, 15.f, 8.f, 15, 0, 6.f, 2500, EAbilitySlot::Interrupt, false, false, true, true, true));
		All.Add(Make(TEXT("Cyclone"),       ESkillID::Tempest, 35.f, 55.f, 40, 0, 12.f, 3000, EAbilitySlot::Core, true, false, false, true, true));

		return All;
	}
}

// ---------------------------------------------------------------------------
// Constructor / BeginPlay
// ---------------------------------------------------------------------------

UAoCNPCCombatBrain::UAoCNPCCombatBrain()
{
	PrimaryComponentTick.bCanEverTick = false;
	CombatRand.Initialize(FMath::Rand());
}

void UAoCNPCCombatBrain::BeginPlay()
{
	Super::BeginPlay();

	OwnerNPC = Cast<AoCHumanoidNPCV2>(GetOwner());
	if (OwnerNPC.IsValid())
	{
		SkillSystem = OwnerNPC->Skills;
		Inventory = OwnerNPC->Inventory;
		Personality = OwnerNPC->Personality;
		Memory = OwnerNPC->Memory;
	}

	CombatRand.Initialize(GetOwner() ? GetOwner()->GetUniqueID() : FMath::Rand());

	// Build initial ability pool and rotation from skills
	BuildAbilityPool();
	BuildRotation();
	RefreshConsumableCount();

	// Calculate max HP/Mana from attributes
	if (SkillSystem)
	{
		float END = SkillSystem->GetAttribute(EAttributeID::END);
		float STR = SkillSystem->GetAttribute(EAttributeID::STR);
		float WIS = SkillSystem->GetAttribute(EAttributeID::WIS);
		float INT = SkillSystem->GetAttribute(EAttributeID::INT);

		MaxHP = 50.f + END * 3.f + STR * 1.f;
		CurrentHP = MaxHP;
		MaxMana = 20.f + INT * 2.f + WIS * 1.5f;
		CurrentMana = MaxMana;
		MaxStamina = 50.f + END * 2.f + STR * 1.f;
		CurrentStamina = MaxStamina;
	}
}

// ---------------------------------------------------------------------------
// Ability Pool & Rotation Building
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::BuildAbilityPool()
{
	KnownAbilities.Reset();

	if (!SkillSystem) return;

	TArray<FCombatAbility> AllAbilities = AbilityDB::GetAllAbilities();

	for (const FCombatAbility& Ability : AllAbilities)
	{
		if (Ability.RequiredSkill == ESkillID::MAX) continue;

		float Level = SkillSystem->GetSkillLevel(Ability.RequiredSkill);
		if (Level >= Ability.MinSkillLevel)
		{
			KnownAbilities.Add(Ability);
		}
	}

	// Determine combat style
	CurrentStyle = DetermineCombatStyle();

	// Check available weapons
	bHasBow = SkillSystem->GetSkillLevel(ESkillID::Bow) >= 5.f;
	bHasShield = SkillSystem->GetSkillLevel(ESkillID::Shield) >= 5.f;

	// Set active weapon to best weapon skill
	ActiveWeaponSkill = SkillSystem->GetBestWeaponSkill();

	UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: Built pool of %d abilities. Style: %d. Active weapon: %s"),
		KnownAbilities.Num(), static_cast<int32>(CurrentStyle),
		*UAoCNPCSkillSystem::GetSkillName(ActiveWeaponSkill).ToString());
}

void UAoCNPCCombatBrain::BuildRotation()
{
	CurrentRotation = FCombatRotation();

	if (KnownAbilities.Num() == 0) return;

	// Find best ability for each rotation slot
	// Sort abilities by damage/utility for each slot type

	// Opener: highest damage opener available
	FCombatAbility* BestOpener = GetBestAbilityForSlot(EAbilitySlot::Opener);
	CurrentRotation.Opener = BestOpener ? BestOpener->AbilityName : NAME_None;

	// Core rotation: top 3-5 core abilities sorted by DPS
	TArray<FCombatAbility*> CoreCandidates;
	for (FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.PreferredSlot == EAbilitySlot::Core)
		{
			CoreCandidates.Add(&Ab);
		}
	}
	CoreCandidates.Sort([](const FCombatAbility* A, const FCombatAbility* B)
	{
		float DpsA = A->BaseDamage / FMath::Max(A->CooldownDuration, 0.5f);
		float DpsB = B->BaseDamage / FMath::Max(B->CooldownDuration, 0.5f);
		return DpsA > DpsB;
	});

	int32 CoreCount = FMath::Min(CoreCandidates.Num(), 5);
	for (int32 i = 0; i < CoreCount; ++i)
	{
		CurrentRotation.CoreRotation.Add(CoreCandidates[i]->AbilityName);
	}

	// If no core abilities, add basic attack
	if (CurrentRotation.CoreRotation.Num() == 0)
	{
		CurrentRotation.CoreRotation.Add(TEXT("Punch")); // Fallback
	}

	// Finisher
	FCombatAbility* BestFinisher = GetBestAbilityForSlot(EAbilitySlot::Finisher);
	CurrentRotation.Finisher = BestFinisher ? BestFinisher->AbilityName : NAME_None;

	// Defensive
	FCombatAbility* BestDef = GetBestAbilityForSlot(EAbilitySlot::Defensive);
	CurrentRotation.DefensiveAbility = BestDef ? BestDef->AbilityName : NAME_None;

	// Panic
	FCombatAbility* BestPanic = GetBestAbilityForSlot(EAbilitySlot::Panic);
	CurrentRotation.PanicAbility = BestPanic ? BestPanic->AbilityName : NAME_None;

	// Interrupt
	FCombatAbility* BestInterrupt = GetBestAbilityForSlot(EAbilitySlot::Interrupt);
	CurrentRotation.InterruptAbility = BestInterrupt ? BestInterrupt->AbilityName : NAME_None;

	UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: Rotation built — Opener: %s, Core[%d], Finisher: %s, Def: %s"),
		*CurrentRotation.Opener.ToString(), CurrentRotation.CoreRotation.Num(),
		*CurrentRotation.Finisher.ToString(), *CurrentRotation.DefensiveAbility.ToString());
}

FCombatAbility* UAoCNPCCombatBrain::GetBestAbilityForSlot(EAbilitySlot Slot)
{
	FCombatAbility* Best = nullptr;
	float BestScore = -1.f;

	for (FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.PreferredSlot != Slot) continue;

		// Score = damage scaled by skill proficiency
		float SkillLevel = SkillSystem ? SkillSystem->GetSkillLevel(Ab.RequiredSkill) : 0.f;
		float Score = Ab.BaseDamage * (1.f + SkillLevel * 0.01f);

		// Heals scored differently
		if (Ab.bIsHeal)
		{
			Score = 50.f + SkillLevel;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			Best = &Ab;
		}
	}

	return Best;
}

// ---------------------------------------------------------------------------
// Combat Style Determination
// ---------------------------------------------------------------------------

ECombatStyle UAoCNPCCombatBrain::DetermineCombatStyle() const
{
	if (!SkillSystem) return ECombatStyle::Unarmed;

	TArray<ESkillID> TopSkills = SkillSystem->GetTopSkills(3);
	if (TopSkills.Num() == 0) return ECombatStyle::Unarmed;

	ESkillID Primary = TopSkills[0];
	float PrimaryLevel = SkillSystem->GetSkillLevel(Primary);

	// Check for hybrid (battlemage)
	bool bHasMelee = false;
	bool bHasMagic = false;
	float TopMeleeLevel = 0.f;
	float TopMagicLevel = 0.f;

	for (ESkillID S : TopSkills)
	{
		if (UAoCNPCSkillSystem::IsWeaponSkill(S) && S != ESkillID::Bow)
		{
			bHasMelee = true;
			TopMeleeLevel = FMath::Max(TopMeleeLevel, SkillSystem->GetSkillLevel(S));
		}
		if (UAoCNPCSkillSystem::IsMagicSkill(S))
		{
			bHasMagic = true;
			TopMagicLevel = FMath::Max(TopMagicLevel, SkillSystem->GetSkillLevel(S));
		}
	}

	// Battlemage: both melee and magic in top skills with decent levels
	if (bHasMelee && bHasMagic && TopMeleeLevel > 20.f && TopMagicLevel > 20.f)
	{
		return ECombatStyle::Battlemage;
	}

	// Pure mage: magic is dominant
	if (bHasMagic && TopMagicLevel > TopMeleeLevel * 1.5f)
	{
		return ECombatStyle::PureMage;
	}

	// Ranged kiter: bow is primary
	if (Primary == ESkillID::Bow)
	{
		return ECombatStyle::RangedKiter;
	}

	// Tank: shield is in top skills
	if (SkillSystem->GetSkillLevel(ESkillID::Shield) > 25.f)
	{
		return ECombatStyle::Tank;
	}

	// Assassin: dagger or claws primary
	if (Primary == ESkillID::Dagger || Primary == ESkillID::Claws)
	{
		return ECombatStyle::Assassin;
	}

	// Berserker: 2H weapon primary
	if (Primary == ESkillID::Greatsword || Primary == ESkillID::GreatAxe ||
		Primary == ESkillID::GreatHammer || Primary == ESkillID::GreatMace)
	{
		return ECombatStyle::Berserker;
	}

	// Default: melee brawler
	return ECombatStyle::MeleeBrawler;
}

ECombatStyle UAoCNPCCombatBrain::GetCombatStyle() const
{
	return CurrentStyle;
}

// ---------------------------------------------------------------------------
// Enter / Exit Combat
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::EnterCombat(AActor* Target)
{
	if (!Target || bInCombat) return;

	bInCombat = true;
	CurrentTarget = Target;
	CurrentPhase = ECombatPhase::Evaluating;
	CombatTimer = 0.f;
	TimeSinceCombatStart = 0.f;
	TimeSinceLastAction = 0.f;
	GlobalCooldownTimer = 0.f;
	ReactionDelayTimer = 0.f;
	CurrentRotation.CoreIndex = 0;

	// Refresh abilities and consumables
	BuildAbilityPool();
	BuildRotation();
	RefreshConsumableCount();

	// Reset all cooldowns for fresh fight
	for (FCombatAbility& Ab : KnownAbilities)
	{
		Ab.CooldownRemaining = 0.f;
	}

	UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s entering combat with %s (Style: %d)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*Target->GetName(), static_cast<int32>(CurrentStyle));
}

void UAoCNPCCombatBrain::ExitCombat()
{
	if (!bInCombat) return;

	bInCombat = false;
	CurrentPhase = ECombatPhase::None;
	CurrentTarget = nullptr;
	GroupFocusTarget = nullptr;

	UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s exiting combat"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"));
}

// ---------------------------------------------------------------------------
// Main Combat Tick
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::CombatTick(float DeltaTime)
{
	if (!bInCombat) return;

	CombatTimer += DeltaTime;
	TimeSinceCombatStart += DeltaTime;
	TimeSinceLastAction += DeltaTime;

	// Tick cooldowns
	TickCooldowns(DeltaTime);

	// Tick GCD
	if (GlobalCooldownTimer > 0.f)
	{
		GlobalCooldownTimer -= DeltaTime;
		if (GlobalCooldownTimer > 0.f) return; // Still on GCD
	}

	// Tick reaction delay
	if (ReactionDelayTimer > 0.f)
	{
		ReactionDelayTimer -= DeltaTime;
		if (ReactionDelayTimer > 0.f) return; // Still reacting
	}

	// Validate target
	if (!CurrentTarget.IsValid())
	{
		ExitCombat();
		return;
	}

	// Regenerate stamina slowly during combat
	CurrentStamina = FMath::Min(CurrentStamina + DeltaTime * 5.f, MaxStamina);

	// Regenerate mana slowly during combat
	float WIS = SkillSystem ? SkillSystem->GetAttribute(EAttributeID::WIS) : 10.f;
	CurrentMana = FMath::Min(CurrentMana + DeltaTime * (1.f + WIS * 0.05f), MaxMana);

	// Update combat phase
	UpdateCombatPhase();

	// Check consumables
	CheckConsumables();

	// Manage range (close gap or create distance)
	ManageRange();

	// Check if we should switch weapons
	if (ShouldSwitchWeapons())
	{
		// Determine best weapon for current situation
		float DistToTarget = GetDistanceToTarget();

		if (DistToTarget > MeleeRange * 2.f && bHasBow && ActiveWeaponSkill != ESkillID::Bow)
		{
			SwitchWeapon(ESkillID::Bow);
		}
		else if (DistToTarget <= MeleeRange && ActiveWeaponSkill == ESkillID::Bow)
		{
			ESkillID BestMelee = SkillSystem ? SkillSystem->GetBestWeaponSkill() : ESkillID::Unarmed;
			if (BestMelee == ESkillID::Bow)
			{
				// Bow is best weapon, but we need melee — find second best
				TArray<ESkillID> TopSkills = SkillSystem->GetTopSkills(5);
				for (ESkillID S : TopSkills)
				{
					if (UAoCNPCSkillSystem::IsWeaponSkill(S) && S != ESkillID::Bow)
					{
						BestMelee = S;
						break;
					}
				}
			}
			SwitchWeapon(BestMelee != ESkillID::Bow ? BestMelee : ESkillID::Unarmed);
		}
	}

	// Select and execute action
	FName Action = SelectAction();
	if (!Action.IsNone())
	{
		// Roll for mistake at low skill
		if (RollForMistake())
		{
			// Fumbled! Add extra delay, waste the action
			UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s fumbled action %s!"),
				OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
				*Action.ToString());
			GlobalCooldownTimer = CombatRand.FRandRange(0.5f, 1.0f);
			return;
		}

		ExecuteAttack(Action);
		TimeSinceLastAction = 0.f;

		// Apply GCD with human-like variance
		float GCD = CombatRand.FRandRange(GlobalCooldownMin, GlobalCooldownMax);
		GlobalCooldownTimer = GCD;
	}
}

// ---------------------------------------------------------------------------
// Combat Phase Management
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::UpdateCombatPhase()
{
	float HPPercent = MaxHP > 0.f ? CurrentHP / MaxHP : 0.f;

	// Get target HP estimate (based on observations or known data)
	float TargetHPPercent = 1.0f;

	// Try to read target HP from another NPC's combat brain, or use damage dealt heuristic
	if (AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(CurrentTarget.Get()))
	{
		if (TargetNPC->CombatBrain)
		{
			TargetHPPercent = TargetNPC->CombatBrain->MaxHP > 0.f ?
				TargetNPC->CombatBrain->CurrentHP / TargetNPC->CombatBrain->MaxHP : 0.f;
		}
	}
	else
	{
		// For player targets, estimate from time in combat and damage dealt
		// Rough estimate decreasing over time
		TargetHPPercent = FMath::Max(0.1f, 1.f - TimeSinceCombatStart * 0.02f);
	}

	ECombatPhase OldPhase = CurrentPhase;

	if (CurrentPhase == ECombatPhase::Evaluating && CombatTimer > 0.5f)
	{
		CurrentPhase = ECombatPhase::Opening;
		CombatTimer = 0.f;
	}
	else if (CurrentPhase == ECombatPhase::Opening && CombatTimer > 2.0f)
	{
		CurrentPhase = ECombatPhase::Engaging;
	}
	else if (CurrentPhase == ECombatPhase::Engaging || CurrentPhase == ECombatPhase::Pressing ||
		CurrentPhase == ECombatPhase::Surviving || CurrentPhase == ECombatPhase::Executing)
	{
		// Phase transitions based on HP states
		if (TargetHPPercent < 0.1f)
		{
			CurrentPhase = ECombatPhase::Executing;
		}
		else if (TargetHPPercent < 0.3f)
		{
			CurrentPhase = ECombatPhase::Pressing;
		}
		else if (HPPercent < 0.2f)
		{
			CurrentPhase = ECombatPhase::Surviving;
		}
		else
		{
			CurrentPhase = ECombatPhase::Engaging;
		}
	}

	// Check flee condition (VERY rare — fight to death is default)
	if (HPPercent < 0.1f && ShouldFlee())
	{
		CurrentPhase = ECombatPhase::Disengaging;
	}

	if (OldPhase != CurrentPhase)
	{
		UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s phase %d → %d (HP: %.0f%%, TargetHP: %.0f%%)"),
			OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
			static_cast<int32>(OldPhase), static_cast<int32>(CurrentPhase),
			HPPercent * 100.f, TargetHPPercent * 100.f);
	}
}

// ---------------------------------------------------------------------------
// Action Selection (the brain of combat)
// ---------------------------------------------------------------------------

FName UAoCNPCCombatBrain::SelectAction()
{
	if (!CurrentTarget.IsValid()) return NAME_None;

	float DistToTarget = GetDistanceToTarget();

	switch (CurrentPhase)
	{
	case ECombatPhase::Evaluating:
	{
		// During evaluation, use buffs or approach
		// Check if we have a pre-fight buff/potion
		return NAME_None; // Just observe and close distance
	}
	case ECombatPhase::Opening:
	{
		return SelectOpener();
	}
	case ECombatPhase::Engaging:
	case ECombatPhase::Pressing:
	{
		// Check for interrupt opportunity
		if (TryInterrupt())
		{
			return CurrentRotation.InterruptAbility;
		}

		// In Pressing phase, prefer high-damage abilities
		if (CurrentPhase == ECombatPhase::Pressing)
		{
			// Use finisher if available and target is low
			if (!CurrentRotation.Finisher.IsNone())
			{
				FCombatAbility* Fin = FindAbility(CurrentRotation.Finisher);
				if (Fin && CanUseAbility(*Fin))
				{
					return CurrentRotation.Finisher;
				}
			}
		}

		return SelectCoreAbility();
	}
	case ECombatPhase::Surviving:
	{
		// Defensive priority
		FName DefAction = SelectDefensiveAction();
		if (!DefAction.IsNone()) return DefAction;

		// Still attack if possible
		return SelectCoreAbility();
	}
	case ECombatPhase::Executing:
	{
		return SelectFinisher();
	}
	case ECombatPhase::Disengaging:
	{
		// Try to use any CC or slow, then run
		if (!CurrentRotation.InterruptAbility.IsNone())
		{
			FCombatAbility* Int = FindAbility(CurrentRotation.InterruptAbility);
			if (Int && CanUseAbility(*Int))
			{
				return CurrentRotation.InterruptAbility;
			}
		}
		// Move away (handled by ManageRange)
		return NAME_None;
	}
	default:
		break;
	}

	return NAME_None;
}

FName UAoCNPCCombatBrain::SelectOpener() const
{
	if (!CurrentRotation.Opener.IsNone())
	{
		// Find and check if usable
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.AbilityName == CurrentRotation.Opener && Ab.IsReady())
			{
				return CurrentRotation.Opener;
			}
		}
	}

	// Fallback to first core ability
	if (CurrentRotation.CoreRotation.Num() > 0)
	{
		return CurrentRotation.CoreRotation[0];
	}

	return NAME_None;
}

FName UAoCNPCCombatBrain::SelectCoreAbility()
{
	if (CurrentRotation.CoreRotation.Num() == 0) return NAME_None;

	// Try abilities in rotation order, skipping ones on cooldown
	int32 Attempts = 0;
	while (Attempts < CurrentRotation.CoreRotation.Num())
	{
		FName CandidateName = CurrentRotation.CoreRotation[CurrentRotation.CoreIndex];
		FCombatAbility* Ab = FindAbility(CandidateName);

		CurrentRotation.CoreIndex = (CurrentRotation.CoreIndex + 1) % CurrentRotation.CoreRotation.Num();
		++Attempts;

		if (Ab && CanUseAbility(*Ab))
		{
			// Check if ability range matches current distance
			float Dist = GetDistanceToTarget();
			if (Ab->bIsRanged || Ab->bIsMagic || Dist <= Ab->Range)
			{
				return CandidateName;
			}
		}
	}

	// All core abilities on cooldown — use basic attack
	// Find any usable ability for active weapon
	for (const FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.RequiredSkill == ActiveWeaponSkill && Ab.IsReady())
		{
			float Dist = GetDistanceToTarget();
			if (Ab.bIsRanged || Ab.bIsMagic || Dist <= Ab.Range)
			{
				return Ab.AbilityName;
			}
		}
	}

	return NAME_None;
}

FName UAoCNPCCombatBrain::SelectFinisher() const
{
	if (!CurrentRotation.Finisher.IsNone())
	{
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.AbilityName == CurrentRotation.Finisher && Ab.IsReady())
			{
				return CurrentRotation.Finisher;
			}
		}
	}

	// No finisher available — use highest damage ability
	FName Best = NAME_None;
	float BestDmg = 0.f;
	for (const FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.IsReady() && Ab.BaseDamage > BestDmg)
		{
			BestDmg = Ab.BaseDamage;
			Best = Ab.AbilityName;
		}
	}
	return Best;
}

FName UAoCNPCCombatBrain::SelectDefensiveAction() const
{
	// Priority: heal > barrier > block > dodge
	for (const FCombatAbility& Ab : KnownAbilities)
	{
		if ((Ab.PreferredSlot == EAbilitySlot::Defensive || Ab.PreferredSlot == EAbilitySlot::Panic) &&
			Ab.bIsHeal && Ab.IsReady())
		{
			return Ab.AbilityName;
		}
	}

	if (!CurrentRotation.DefensiveAbility.IsNone())
	{
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.AbilityName == CurrentRotation.DefensiveAbility && Ab.IsReady())
			{
				return CurrentRotation.DefensiveAbility;
			}
		}
	}

	// Panic button
	if (!CurrentRotation.PanicAbility.IsNone())
	{
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.AbilityName == CurrentRotation.PanicAbility && Ab.IsReady())
			{
				return CurrentRotation.PanicAbility;
			}
		}
	}

	return NAME_None;
}

FName UAoCNPCCombatBrain::SelectPanicAction() const
{
	if (!CurrentRotation.PanicAbility.IsNone())
	{
		FCombatAbility* PanicAb = nullptr;
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.AbilityName == CurrentRotation.PanicAbility && Ab.IsReady())
			{
				return CurrentRotation.PanicAbility;
			}
		}
	}

	// Biggest damage ability as last resort
	FName Best = NAME_None;
	float BestDmg = 0.f;
	for (const FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.IsReady() && Ab.BaseDamage > BestDmg)
		{
			BestDmg = Ab.BaseDamage;
			Best = Ab.AbilityName;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------
// Execute Attack
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::ExecuteAttack(FName AbilityName)
{
	FCombatAbility* Ab = FindAbility(AbilityName);
	if (!Ab) return;

	if (!CanUseAbility(*Ab)) return;

	// Consume resources
	CurrentMana -= Ab->ManaCost;
	CurrentStamina -= Ab->StaminaCost;

	// Put ability on cooldown
	Ab->CooldownRemaining = Ab->CooldownDuration;

	// Calculate damage including skill bonuses
	float Damage = Ab->BaseDamage;
	if (SkillSystem)
	{
		float SkillLevel = SkillSystem->GetSkillLevel(Ab->RequiredSkill);
		float SkillMult = 1.f + SkillLevel * 0.015f; // +1.5% per skill level

		if (Ab->bIsMagic)
		{
			Damage *= SkillMult;
			Damage += SkillSystem->GetSpellPower(Ab->RequiredSkill) * 0.3f;
		}
		else
		{
			Damage *= SkillMult;
			Damage += SkillSystem->GetEffectiveDamage(Ab->RequiredSkill) * 0.2f;
		}

		// Gain skill XP for using this ability
		float DiffMod = 1.0f; // Combat use gives base XP
		SkillSystem->GainSkillXP(Ab->RequiredSkill, 5.f, DiffMod);
	}

	// Apply damage to target
	if (CurrentTarget.IsValid())
	{
		if (AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(CurrentTarget.Get()))
		{
			TargetNPC->TakeDamageFromSource(Damage, OwnerNPC.Get());
		}
		// For player targets, would call the player's damage interface
	}

	UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s used %s for %.1f damage"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*AbilityName.ToString(), Damage);
}

// ---------------------------------------------------------------------------
// Dodge / Block
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::DodgeOrBlock()
{
	if (!OwnerNPC.IsValid()) return;

	// Decision: block or dodge based on equipment and style
	bool bCanBlock = bHasShield && CurrentStyle == ECombatStyle::Tank;

	if (bCanBlock)
	{
		// Shield block
		ExecuteAttack(TEXT("ShieldBlock"));
		UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s blocks!"),
			*OwnerNPC->GetDisplayName());
	}
	else
	{
		// Dodge — choose direction based on pattern recognition
		FVector DodgeDir = FVector::RightVector;

		if (CurrentTarget.IsValid())
		{
			FPlayerPattern& Pat = GetOrCreatePattern(CurrentTarget.Get());
			// Dodge opposite to target's preferred direction
			if (Pat.DodgeDirectionBias > 0.2f)
			{
				DodgeDir = -FVector::RightVector; // Dodge left if they expect right
			}
			else if (Pat.DodgeDirectionBias < -0.2f)
			{
				DodgeDir = FVector::RightVector; // Dodge right if they expect left
			}
			else
			{
				// Random dodge direction
				DodgeDir = CombatRand.FRand() > 0.5f ? FVector::RightVector : -FVector::RightVector;
			}
		}

		// Apply dodge movement
		if (UCharacterMovementComponent* Movement = OwnerNPC->GetCharacterMovement())
		{
			FVector WorldDodge = OwnerNPC->GetActorRotation().RotateVector(DodgeDir * 400.f);
			Movement->AddImpulse(WorldDodge, true);
		}

		// Stamina cost for dodging
		CurrentStamina -= 15.f;

		UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s dodges!"),
			*OwnerNPC->GetDisplayName());
	}

	// Add reaction delay after defensive action
	ReactionDelayTimer = CalculateReactionDelay() * 0.5f;
}

// ---------------------------------------------------------------------------
// Consumables
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::UseConsumable(FName ItemName)
{
	if (!OwnerNPC.IsValid()) return;

	// Health potion
	if (ItemName == TEXT("HealthPotion") && HealthPotionsRemaining > 0)
	{
		float HealAmount = MaxHP * 0.4f; // Heal 40% of max HP
		CurrentHP = FMath::Min(CurrentHP + HealAmount, MaxHP);
		--HealthPotionsRemaining;

		UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s used health potion (HP: %.0f/%.0f, %d remaining)"),
			*OwnerNPC->GetDisplayName(), CurrentHP, MaxHP, HealthPotionsRemaining);

		// Apply GCD for potion use
		GlobalCooldownTimer = 1.0f; // Potions have longer GCD
	}
	// Mana potion
	else if (ItemName == TEXT("ManaPotion") && ManaPotionsRemaining > 0)
	{
		float ManaRestore = MaxMana * 0.5f;
		CurrentMana = FMath::Min(CurrentMana + ManaRestore, MaxMana);
		--ManaPotionsRemaining;

		UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s used mana potion (Mana: %.0f/%.0f, %d remaining)"),
			*OwnerNPC->GetDisplayName(), CurrentMana, MaxMana, ManaPotionsRemaining);

		GlobalCooldownTimer = 1.0f;
	}
}

void UAoCNPCCombatBrain::CheckConsumables()
{
	float HPPercent = MaxHP > 0.f ? CurrentHP / MaxHP : 1.f;
	float ManaPercent = MaxMana > 0.f ? CurrentMana / MaxMana : 1.f;

	// Health potion check
	float HPThreshold = GetHealthPotionThreshold();
	if (HPPercent < HPThreshold && HealthPotionsRemaining > 0)
	{
		// Don't waste potions if clearly winning
		float TargetHPPercent = 1.f;
		if (AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(CurrentTarget.Get()))
		{
			if (TargetNPC->CombatBrain)
			{
				TargetHPPercent = TargetNPC->CombatBrain->MaxHP > 0.f ?
					TargetNPC->CombatBrain->CurrentHP / TargetNPC->CombatBrain->MaxHP : 0.f;
			}
		}

		// Use potion if: we're low AND (target is still healthy OR we're really low)
		if (TargetHPPercent > 0.2f || HPPercent < 0.15f)
		{
			UseConsumable(TEXT("HealthPotion"));
			return;
		}
	}

	// Mana potion check
	if (ManaPercent < ManaPotionThreshold && ManaPotionsRemaining > 0)
	{
		// Only use mana potion if we have spells worth casting
		bool bHasUsableSpells = false;
		for (const FCombatAbility& Ab : KnownAbilities)
		{
			if (Ab.bIsMagic && Ab.ManaCost > 0 && Ab.BaseDamage > 20.f)
			{
				bHasUsableSpells = true;
				break;
			}
		}

		if (bHasUsableSpells)
		{
			UseConsumable(TEXT("ManaPotion"));
		}
	}
}

void UAoCNPCCombatBrain::RefreshConsumableCount()
{
	// Count potions in inventory
	// Since we reference the inventory component abstractly, use a reasonable default
	// In full integration, this queries the inventory component
	HealthPotionsRemaining = 3; // Default starting potions
	ManaPotionsRemaining = 2;

	// TODO: Query actual inventory when component is fully linked
	// if (Inventory)
	// {
	//     HealthPotionsRemaining = Inventory->CountItem(TEXT("HealthPotion"));
	//     ManaPotionsRemaining = Inventory->CountItem(TEXT("ManaPotion"));
	// }
}

// ---------------------------------------------------------------------------
// Pattern Recognition
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::UpdatePatternRecognition(AActor* Target, FName ObservedAction)
{
	if (!Target) return;

	FPlayerPattern& Pat = GetOrCreatePattern(Target);
	Pat.RecentActions.Add(ObservedAction);

	// Keep only last 10 actions
	while (Pat.RecentActions.Num() > 10)
	{
		Pat.RecentActions.RemoveAt(0);
	}

	++Pat.TotalObservations;

	// Update dodge direction bias
	if (ObservedAction == TEXT("DodgeLeft"))
	{
		Pat.DodgeDirectionBias = Pat.DodgeDirectionBias * 0.8f + (-1.f) * 0.2f;
	}
	else if (ObservedAction == TEXT("DodgeRight"))
	{
		Pat.DodgeDirectionBias = Pat.DodgeDirectionBias * 0.8f + (1.f) * 0.2f;
	}

	// Update spell frequency
	int32 SpellCount = 0;
	for (const FName& A : Pat.RecentActions)
	{
		// Simple heuristic: actions containing "bolt", "fire", "ice", "lightning", etc.
		FString ActionStr = A.ToString().ToLower();
		if (ActionStr.Contains(TEXT("bolt")) || ActionStr.Contains(TEXT("fire")) ||
			ActionStr.Contains(TEXT("ice")) || ActionStr.Contains(TEXT("storm")) ||
			ActionStr.Contains(TEXT("arcane")) || ActionStr.Contains(TEXT("shadow")) ||
			ActionStr.Contains(TEXT("heal")) || ActionStr.Contains(TEXT("spell")))
		{
			++SpellCount;
		}
	}
	Pat.SpellCastFrequency = Pat.RecentActions.Num() > 0 ?
		static_cast<float>(SpellCount) / Pat.RecentActions.Num() : 0.f;

	// Detect kiting behavior
	float Dist = GetDistanceToTarget();
	if (Dist > MeleeRange * 2.f)
	{
		Pat.bTendsToKite = true;
	}
	else
	{
		Pat.bTendsToKite = Pat.bTendsToKite && (Dist > MeleeRange);
	}

	// Detect aggressive behavior
	int32 AttackCount = 0;
	for (const FName& A : Pat.RecentActions)
	{
		FString ActionStr = A.ToString().ToLower();
		if (!ActionStr.Contains(TEXT("dodge")) && !ActionStr.Contains(TEXT("block")) &&
			!ActionStr.Contains(TEXT("heal")) && !ActionStr.Contains(TEXT("run")))
		{
			++AttackCount;
		}
	}
	Pat.bIsAggressive = Pat.RecentActions.Num() > 3 &&
		(static_cast<float>(AttackCount) / Pat.RecentActions.Num()) > 0.7f;
}

FPlayerPattern& UAoCNPCCombatBrain::GetOrCreatePattern(AActor* Target)
{
	uint32 ID = Target ? Target->GetUniqueID() : 0;
	FPlayerPattern* Found = TargetPatterns.Find(ID);
	if (Found) return *Found;

	TargetPatterns.Add(ID, FPlayerPattern());
	return TargetPatterns[ID];
}

FName UAoCNPCCombatBrain::PredictTargetAction(AActor* Target) const
{
	if (!Target) return NAME_None;

	uint32 ID = Target->GetUniqueID();
	const FPlayerPattern* Pat = TargetPatterns.Find(ID);
	if (!Pat || Pat->TotalObservations < 5) return NAME_None;

	// Simple prediction: most frequent recent action
	TMap<FName, int32> ActionCounts;
	for (const FName& A : Pat->RecentActions)
	{
		ActionCounts.FindOrAdd(A)++;
	}

	FName MostFrequent = NAME_None;
	int32 MaxCount = 0;
	for (const auto& Pair : ActionCounts)
	{
		if (Pair.Value > MaxCount)
		{
			MaxCount = Pair.Value;
			MostFrequent = Pair.Key;
		}
	}

	return MostFrequent;
}

// ---------------------------------------------------------------------------
// Weapon Switching
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::SwitchWeapon(ESkillID NewWeapon)
{
	if (NewWeapon == ActiveWeaponSkill) return;

	ESkillID OldWeapon = ActiveWeaponSkill;
	ActiveWeaponSkill = NewWeapon;

	// Rebuild rotation for new weapon
	BuildAbilityPool();
	BuildRotation();

	// Weapon switch has a delay (like a real player)
	GlobalCooldownTimer = 0.8f;

	UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s switched weapon %s → %s"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		*UAoCNPCSkillSystem::GetSkillName(OldWeapon).ToString(),
		*UAoCNPCSkillSystem::GetSkillName(NewWeapon).ToString());
}

bool UAoCNPCCombatBrain::ShouldSwitchWeapons() const
{
	if (!CurrentTarget.IsValid() || !SkillSystem) return false;

	float Dist = GetDistanceToTarget();

	// Melee NPC and target is far → switch to bow
	if (!UAoCNPCSkillSystem::IsWeaponSkill(ActiveWeaponSkill) || ActiveWeaponSkill == ESkillID::Bow)
	{
		if (Dist <= MeleeRange && ActiveWeaponSkill == ESkillID::Bow)
		{
			return true; // Cornered, switch to melee
		}
	}
	else
	{
		// Melee weapon equipped, target is far
		if (Dist > MeleeRange * 3.f && bHasBow)
		{
			return true; // Switch to bow
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Range Management
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::ManageRange()
{
	if (!CurrentTarget.IsValid() || !OwnerNPC.IsValid()) return;

	float Dist = GetDistanceToTarget();
	UCharacterMovementComponent* Movement = OwnerNPC->GetCharacterMovement();
	if (!Movement) return;

	FVector ToTarget = (CurrentTarget->GetActorLocation() - OwnerNPC->GetActorLocation()).GetSafeNormal();

	float DesiredRange = MeleeRange * 0.8f;

	switch (CurrentStyle)
	{
	case ECombatStyle::MeleeBrawler:
	case ECombatStyle::Tank:
	case ECombatStyle::Berserker:
		DesiredRange = MeleeRange * 0.7f;
		break;
	case ECombatStyle::Assassin:
		DesiredRange = MeleeRange * 0.5f; // Get very close
		break;
	case ECombatStyle::RangedKiter:
		DesiredRange = RangedMaxRange * 0.5f; // Maintain distance
		break;
	case ECombatStyle::PureMage:
		DesiredRange = SpellMaxRange * 0.5f;
		break;
	case ECombatStyle::Battlemage:
		DesiredRange = MeleeRange * 1.5f; // Medium range
		break;
	default:
		break;
	}

	// Disengaging: run away
	if (CurrentPhase == ECombatPhase::Disengaging)
	{
		OwnerNPC->AddMovementInput(-ToTarget, 1.0f);
		return;
	}

	// Flanking check for group combat
	if (ShouldFlank())
	{
		FVector FlankPos = GetFlankPosition();
		FVector ToFlank = (FlankPos - OwnerNPC->GetActorLocation()).GetSafeNormal();
		OwnerNPC->AddMovementInput(ToFlank, 1.0f);
		return;
	}

	// Normal range management
	float RangeTolerance = 100.f;

	if (Dist > DesiredRange + RangeTolerance)
	{
		// Too far — close gap
		OwnerNPC->AddMovementInput(ToTarget, 1.0f);
	}
	else if (Dist < DesiredRange - RangeTolerance)
	{
		// Too close for ranged styles — back up
		if (CurrentStyle == ECombatStyle::RangedKiter || CurrentStyle == ECombatStyle::PureMage)
		{
			OwnerNPC->AddMovementInput(-ToTarget, 0.8f);
		}
		// Melee styles stay close
	}

	// Always face target
	FRotator LookAt = ToTarget.Rotation();
	OwnerNPC->SetActorRotation(FMath::RInterpTo(OwnerNPC->GetActorRotation(), LookAt, GetWorld()->GetDeltaSeconds(), 10.f));
}

// ---------------------------------------------------------------------------
// Interrupt Logic
// ---------------------------------------------------------------------------

bool UAoCNPCCombatBrain::TryInterrupt()
{
	if (CurrentRotation.InterruptAbility.IsNone()) return false;

	FCombatAbility* IntAb = FindAbility(CurrentRotation.InterruptAbility);
	if (!IntAb || !IntAb->IsReady()) return false;

	// Check if target is casting (heuristic: check pattern predictions)
	if (CurrentTarget.IsValid())
	{
		FPlayerPattern& Pat = GetOrCreatePattern(CurrentTarget.Get());

		// If target has high spell frequency and we predict a spell next
		if (Pat.SpellCastFrequency > 0.4f && Pat.TotalObservations >= 5)
		{
			FName Predicted = PredictTargetAction(CurrentTarget.Get());
			FString PredStr = Predicted.ToString().ToLower();
			if (PredStr.Contains(TEXT("bolt")) || PredStr.Contains(TEXT("fire")) ||
				PredStr.Contains(TEXT("heal")) || PredStr.Contains(TEXT("ice")))
			{
				// 60% chance to interrupt (not 100% — feels more human)
				if (CombatRand.FRand() < 0.6f)
				{
					return true;
				}
			}
		}

		// Special: save interrupt for when target heals (predicted at threshold)
		if (Pat.HealThreshold > 0.f)
		{
			AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(CurrentTarget.Get());
			if (TargetNPC && TargetNPC->CombatBrain)
			{
				float TargetHP = TargetNPC->CombatBrain->CurrentHP / FMath::Max(TargetNPC->CombatBrain->MaxHP, 1.f);
				if (FMath::Abs(TargetHP - Pat.HealThreshold) < 0.05f)
				{
					// Target is at their heal threshold — interrupt NOW
					return true;
				}
			}
		}
	}

	return false;
}

// ---------------------------------------------------------------------------
// Event Handlers
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::OnTargetKilled(AActor* DeadTarget)
{
	if (DeadTarget == CurrentTarget.Get())
	{
		UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s killed target %s!"),
			OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
			*DeadTarget->GetName());

		CurrentPhase = ECombatPhase::Looting;

		// Gain skill XP bonus for the kill
		if (SkillSystem)
		{
			SkillSystem->GainSkillXP(ActiveWeaponSkill, 25.f, 1.5f);
		}

		// Record in memory
		if (Memory && OwnerNPC.IsValid())
		{
			// Memory->Remember(TEXT("KilledEnemy"), DeadTarget->GetActorLocation(), 0.5f, DeadTarget);
		}

		ExitCombat();
	}
}

void UAoCNPCCombatBrain::OnDamageTaken(float Damage, AActor* Instigator)
{
	CurrentHP -= Damage;

	UE_LOG(LogAoCCombat, Verbose, TEXT("CombatBrain: %s took %.1f damage from %s (HP: %.0f/%.0f)"),
		OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
		Damage, Instigator ? *Instigator->GetName() : TEXT("???"),
		CurrentHP, MaxHP);

	// If not in combat, enter combat with attacker
	if (!bInCombat && Instigator)
	{
		EnterCombat(Instigator);
	}
	// If in combat but attacked by someone else, consider switching targets
	else if (bInCombat && Instigator && Instigator != CurrentTarget.Get())
	{
		// Switch target if new attacker is closer or more dangerous
		float DistToCurrent = GetDistanceToTarget();
		float DistToNew = OwnerNPC.IsValid() ?
			FVector::Dist(OwnerNPC->GetActorLocation(), Instigator->GetActorLocation()) : MAX_FLT;

		if (DistToNew < DistToCurrent * 0.5f)
		{
			CurrentTarget = Instigator;
			UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s switching target to %s (closer attacker)"),
				OwnerNPC.IsValid() ? *OwnerNPC->GetDisplayName() : TEXT("???"),
				*Instigator->GetName());
		}
	}

	// Reactive dodge if we have stamina
	if (CurrentStamina > 20.f && Damage > MaxHP * 0.15f)
	{
		// 40% chance to dodge-react after being hit (human-like)
		if (CombatRand.FRand() < 0.4f)
		{
			ReactionDelayTimer = CalculateReactionDelay();
			// After delay, will dodge
		}
	}

	// Update pattern for attacker
	if (Instigator)
	{
		UpdatePatternRecognition(Instigator, TEXT("AttackedMe"));
	}

	// Check death
	if (CurrentHP <= 0.f)
	{
		CurrentHP = 0.f;
		if (OwnerNPC.IsValid())
		{
			OwnerNPC->OnDeath(Instigator);
		}
	}
}

// ---------------------------------------------------------------------------
// Flee Logic (VERY RARE — fight to death by default)
// ---------------------------------------------------------------------------

bool UAoCNPCCombatBrain::ShouldFlee() const
{
	// DEFAULT: fight to death
	if (!Personality || !OwnerNPC.IsValid()) return false;

	float HPPercent = MaxHP > 0.f ? CurrentHP / MaxHP : 0.f;
	float Courage = 0.5f;

	// Read courage from personality
	// Courage = Personality->GetTraitValue(TEXT("Courage"));
	// For now, estimate from personality component
	Courage = 0.5f; // Default moderate courage

	// Guards NEVER flee
	if (OwnerNPC->ArchetypePreset == EPersonalityArchetype::Guard)
	{
		return false;
	}

	// Only consider fleeing if: low courage AND very low HP
	if (Courage < 0.15f && HPPercent < 0.1f)
	{
		// Even then, 50% chance they fight anyway (rage/desperation)
		return CombatRand.FRand() > 0.5f;
	}

	// Bandits might flee if ambush goes wrong
	if (OwnerNPC->ArchetypePreset == EPersonalityArchetype::Bandit && Courage < 0.3f && HPPercent < 0.15f)
	{
		return CombatRand.FRand() > 0.5f;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Group Combat
// ---------------------------------------------------------------------------

void UAoCNPCCombatBrain::SetGroupFocusTarget(AActor* Target)
{
	GroupFocusTarget = Target;

	// If we have a group focus target and not in combat, engage it
	if (Target && !bInCombat)
	{
		EnterCombat(Target);
	}
	else if (Target && bInCombat)
	{
		CurrentTarget = Target;
	}
}

void UAoCNPCCombatBrain::RequestHelp(float Radius)
{
	if (!OwnerNPC.IsValid()) return;

	// Find nearby friendly NPCs and ask them to join combat
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == OwnerNPC.Get()) continue;

		float Dist = FVector::Dist(Actor->GetActorLocation(), OwnerNPC->GetActorLocation());
		if (Dist > Radius) continue;

		AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
		if (!OtherNPC || !OtherNPC->SocialBrain) continue;

		// Check if they're friendly to us
		if (OtherNPC->SocialBrain->IsAlly(OwnerNPC.Get()) && !OtherNPC->CombatBrain->IsInCombat())
		{
			// They'll help!
			if (CurrentTarget.IsValid())
			{
				OtherNPC->CombatBrain->EnterCombat(CurrentTarget.Get());
				OtherNPC->CombatBrain->SetGroupFocusTarget(CurrentTarget.Get());
			}

			UE_LOG(LogAoCCombat, Log, TEXT("CombatBrain: %s called for help — %s responds!"),
				*OwnerNPC->GetDisplayName(), *OtherNPC->GetDisplayName());
		}
	}
}

bool UAoCNPCCombatBrain::ShouldFlank() const
{
	if (!CurrentTarget.IsValid() || !OwnerNPC.IsValid()) return false;

	// Flanking: if an ally is fighting the same target from the front, circle behind
	UWorld* World = GetWorld();
	if (!World) return false;

	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(World, AoCHumanoidNPCV2::StaticClass(), NearbyActors);

	for (AActor* Actor : NearbyActors)
	{
		if (Actor == OwnerNPC.Get()) continue;

		AoCHumanoidNPCV2* OtherNPC = Cast<AoCHumanoidNPCV2>(Actor);
		if (!OtherNPC || !OtherNPC->CombatBrain) continue;

		// Is this NPC an ally fighting the same target?
		if (OtherNPC->CombatBrain->IsInCombat() &&
			OtherNPC->CombatBrain->GetCurrentTarget() == CurrentTarget.Get())
		{
			float AllyDist = FVector::Dist(OtherNPC->GetActorLocation(), CurrentTarget->GetActorLocation());
			if (AllyDist < MeleeRange * 1.5f)
			{
				return true; // Ally is in melee with target — we should flank
			}
		}
	}

	return false;
}

FVector UAoCNPCCombatBrain::GetFlankPosition() const
{
	if (!CurrentTarget.IsValid() || !OwnerNPC.IsValid()) return FVector::ZeroVector;

	// Get position behind target relative to the closest ally
	FVector TargetPos = CurrentTarget->GetActorLocation();
	FVector TargetForward = CurrentTarget->GetActorForwardVector();

	// Position behind the target
	return TargetPos - TargetForward * MeleeRange * 0.8f;
}

// ---------------------------------------------------------------------------
// Utility Functions
// ---------------------------------------------------------------------------

float UAoCNPCCombatBrain::GetDistanceToTarget() const
{
	if (!CurrentTarget.IsValid() || !OwnerNPC.IsValid()) return MAX_FLT;
	return FVector::Dist(OwnerNPC->GetActorLocation(), CurrentTarget->GetActorLocation());
}

void UAoCNPCCombatBrain::TickCooldowns(float DeltaTime)
{
	for (FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.CooldownRemaining > 0.f)
		{
			Ab.CooldownRemaining -= DeltaTime;
		}
	}
}

bool UAoCNPCCombatBrain::CanUseAbility(const FCombatAbility& Ability) const
{
	if (!Ability.IsReady()) return false;
	if (Ability.ManaCost > CurrentMana) return false;
	if (Ability.StaminaCost > CurrentStamina) return false;

	// Range check
	if (!Ability.bIsRanged && !Ability.bIsMagic && Ability.Range > 0)
	{
		float Dist = GetDistanceToTarget();
		if (Dist > Ability.Range * 1.2f) return false; // Small tolerance
	}

	return true;
}

FCombatAbility* UAoCNPCCombatBrain::FindAbility(FName Name)
{
	for (FCombatAbility& Ab : KnownAbilities)
	{
		if (Ab.AbilityName == Name)
		{
			return &Ab;
		}
	}
	return nullptr;
}

float UAoCNPCCombatBrain::GetHealthPotionThreshold() const
{
	float BaseThreshold = HealthPotionBaseThreshold;

	// Personality modifier: cautious NPCs heal earlier, aggressive later
	// Courage > 0.5: heal later (more aggressive)
	// Courage < 0.5: heal earlier (more cautious)
	float CourageMod = 0.5f; // Default
	// if (Personality) CourageMod = Personality->GetTraitValue(TEXT("Courage"));

	// Cautious (low courage): heal at 40%
	// Aggressive (high courage): heal at 20%
	return BaseThreshold + (0.5f - CourageMod) * 0.2f;
}

float UAoCNPCCombatBrain::CalculateReactionDelay() const
{
	float DEX = SkillSystem ? SkillSystem->GetAttribute(EAttributeID::DEX) : 10.f;

	// Higher DEX = faster reactions
	// DEX 10: max delay (0.5s)
	// DEX 50: mid delay (0.3s)
	// DEX 100: min delay (0.15s)
	float NormDEX = FMath::Clamp((DEX - 10.f) / 90.f, 0.f, 1.f);
	float Delay = FMath::Lerp(ReactionDelayMax, ReactionDelayMin, NormDEX);

	// Add some randomness
	Delay += CombatRand.FRandRange(-0.05f, 0.05f);

	return FMath::Max(Delay, ReactionDelayMin);
}

float UAoCNPCCombatBrain::GetMistakeChance() const
{
	if (!SkillSystem) return 0.1f;

	float SkillLevel = SkillSystem->GetSkillLevel(ActiveWeaponSkill);

	// Low skill = more mistakes
	// Skill 10: 15% mistake chance
	// Skill 50: 5% mistake chance
	// Skill 90: 1% mistake chance
	float NormSkill = FMath::Clamp(SkillLevel / 100.f, 0.f, 1.f);
	return FMath::Lerp(0.15f, 0.01f, NormSkill);
}

bool UAoCNPCCombatBrain::RollForMistake()
{
	float Chance = GetMistakeChance();
	return CombatRand.FRand() < Chance;
}

FTargetAssessment UAoCNPCCombatBrain::EvaluateTarget(AActor* Target) const
{
	FTargetAssessment Assessment;
	if (!Target || !SkillSystem) return Assessment;

	// Estimate target's combat power from visible gear and behavior
	AoCHumanoidNPCV2* TargetNPC = Cast<AoCHumanoidNPCV2>(Target);
	if (TargetNPC && TargetNPC->Skills)
	{
		Assessment.EstimatedCombatPower = TargetNPC->Skills->GetCombatPower();
	}
	else
	{
		// For players, estimate from observed behavior
		Assessment.EstimatedCombatPower = 100.f; // Default assume moderate
	}

	float OwnPower = SkillSystem->GetCombatPower();

	// Confidence to win = own power / (own + target power)
	float TotalPower = OwnPower + Assessment.EstimatedCombatPower;
	Assessment.ConfidenceToWin = TotalPower > 0.f ? OwnPower / TotalPower : 0.5f;

	// Threat level based on relative power
	Assessment.ThreatLevel = FMath::Clamp(Assessment.EstimatedCombatPower / FMath::Max(OwnPower, 1.f), 0.f, 1.f);

	// Recommended approach
	if (Assessment.ConfidenceToWin > 0.7f)
	{
		Assessment.RecommendedApproach = ECombatApproach::Aggressive;
	}
	else if (Assessment.ConfidenceToWin > 0.4f)
	{
		Assessment.RecommendedApproach = ECombatApproach::Cautious;
	}
	else if (Assessment.ConfidenceToWin > 0.25f)
	{
		Assessment.RecommendedApproach = ECombatApproach::Ambush;
	}
	else
	{
		Assessment.RecommendedApproach = ECombatApproach::GroupUp;
	}

	return Assessment;
}
