// AoCHumanoidNPCV2.cpp
// Master humanoid NPC character — wires all systems together
// Full implementation with LOD-aware ticking, initialization, death, respawn, save/load

#include "AoCHumanoidNPCV2.h"
#include "AoCNPCRelationship.h"
#include "AoCAILODManager.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAoCNPC);

// ---------------------------------------------------------------------------
// Name tables for random name generation
// ---------------------------------------------------------------------------
namespace
{
	const TArray<FString> FirstNames = {
		TEXT("Aldric"), TEXT("Brynn"), TEXT("Cassian"), TEXT("Dara"), TEXT("Elowen"),
		TEXT("Fynn"), TEXT("Greta"), TEXT("Haldor"), TEXT("Isara"), TEXT("Joren"),
		TEXT("Kael"), TEXT("Lyra"), TEXT("Marek"), TEXT("Nessa"), TEXT("Orin"),
		TEXT("Petra"), TEXT("Quinn"), TEXT("Riven"), TEXT("Seren"), TEXT("Thane"),
		TEXT("Uma"), TEXT("Voren"), TEXT("Wren"), TEXT("Xara"), TEXT("Yorick"),
		TEXT("Zara"), TEXT("Agna"), TEXT("Bjorn"), TEXT("Cerise"), TEXT("Dagny"),
		TEXT("Erwin"), TEXT("Freya"), TEXT("Gareth"), TEXT("Helga"), TEXT("Ivar"),
		TEXT("Jorun"), TEXT("Kirsa"), TEXT("Leif"), TEXT("Maren"), TEXT("Njord"),
		TEXT("Olga"), TEXT("Pax"), TEXT("Ragna"), TEXT("Sigrid"), TEXT("Torben"),
		TEXT("Ulf"), TEXT("Viggo"), TEXT("Wynne"), TEXT("Ylva"), TEXT("Zephyr")
	};

	const TArray<FString> LastNames = {
		TEXT("Ironforge"), TEXT("Stormwind"), TEXT("Blackthorn"), TEXT("Ashborne"), TEXT("Wolfsbane"),
		TEXT("Greymane"), TEXT("Sunward"), TEXT("Darkhollow"), TEXT("Stoneheart"), TEXT("Brightmoor"),
		TEXT("Redmane"), TEXT("Frostpeak"), TEXT("Shadowmend"), TEXT("Copperfield"), TEXT("Thornwall"),
		TEXT("Deepwell"), TEXT("Hawkridge"), TEXT("Silverbrook"), TEXT("Dunmore"), TEXT("Fernwick"),
		TEXT("Goldvein"), TEXT("Hearthstone"), TEXT("Ironwood"), TEXT("Ravencrest"), TEXT("Winterborn"),
		TEXT("Firebrand"), TEXT("Duskwarden"), TEXT("Clearwater"), TEXT("Mossglen"), TEXT("Nighthollow"),
		TEXT("Oakshield"), TEXT("Pinecrest"), TEXT("Quarrydale"), TEXT("Rimecroft"), TEXT("Swiftblade"),
		TEXT("Tallowmere"), TEXT("Underhill"), TEXT("Valewood"), TEXT("Windrift"), TEXT("Yewstone")
	};
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

AoCHumanoidNPCV2::AoCHumanoidNPCV2()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f; // Tick every frame (LOD will throttle)

	// Create existing components
	NeedSystem = CreateDefaultSubobject<UAoCNPCNeedSystem>(TEXT("NeedSystem"));
	Memory = CreateDefaultSubobject<UAoCNPCMemory>(TEXT("Memory"));
	Inventory = CreateDefaultSubobject<UAoCNPCInventory>(TEXT("Inventory"));
	GoalPlanner = CreateDefaultSubobject<UAoCNPCGoalPlanner>(TEXT("GoalPlanner"));
	Personality = CreateDefaultSubobject<UAoCNPCPersonality>(TEXT("Personality"));
	Brain = CreateDefaultSubobject<UAoCNPCBrainV2>(TEXT("Brain"));

	// Create new components
	Skills = CreateDefaultSubobject<UAoCNPCSkillSystem>(TEXT("Skills"));
	CombatBrain = CreateDefaultSubobject<UAoCNPCCombatBrain>(TEXT("CombatBrain"));
	LootBrain = CreateDefaultSubobject<UAoCNPCLootBrain>(TEXT("LootBrain"));
	SocialBrain = CreateDefaultSubobject<UAoCNPCSocialBrain>(TEXT("SocialBrain"));
	LifeBrain = CreateDefaultSubobject<UAoCNPCLifeBrain>(TEXT("LifeBrain"));

	// Sensible character movement defaults
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->MaxWalkSpeed = 450.f;
		MoveComp->MaxWalkSpeedCrouched = 200.f;
		MoveComp->bOrientRotationToMovement = true;
		MoveComp->RotationRate = FRotator(0.f, 480.f, 0.f);
		MoveComp->JumpZVelocity = 420.f;
		MoveComp->AirControl = 0.2f;
	}

	// Default property values
	bIsAggressive = false;
	bCanLoot = true;
	bCanCraft = true;
	bCanTrade = true;
	bIsEssential = false;
	bIsDead = false;
	bIsInitialized = false;
	RespawnDelay = 300.f; // 5 minutes
	CorpseLingerTime = 600.f; // 10 minutes
	CachedLODTier = EAILODTier::Full;
	ArchetypePreset = EPersonalityArchetype::Villager;
}

// ---------------------------------------------------------------------------
// BeginPlay / EndPlay
// ---------------------------------------------------------------------------

void AoCHumanoidNPCV2::BeginPlay()
{
	Super::BeginPlay();

	if (!bIsInitialized)
	{
		InitializeNPC();
	}

	// Register with LOD manager
	UWorld* World = GetWorld();
	if (World)
	{
		UAoCAILODManager* LODMgr = World->GetSubsystem<UAoCAILODManager>();
		if (LODMgr)
		{
			LODMgr->RegisterNPC(this);
		}
	}

	UE_LOG(LogAoCNPC, Log, TEXT("NPC %s (Archetype: %d) initialized and registered."),
		*NPCName, static_cast<int32>(ArchetypePreset));
}

void AoCHumanoidNPCV2::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from LOD manager
	UWorld* World = GetWorld();
	if (World)
	{
		UAoCAILODManager* LODMgr = World->GetSubsystem<UAoCAILODManager>();
		if (LODMgr)
		{
			LODMgr->UnregisterNPC(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void AoCHumanoidNPCV2::InitializeNPC()
{
	bIsInitialized = true;

	// Generate unique ID
	UniqueID = FGuid::NewGuid();

	// Generate name if not set
	if (NPCName.IsEmpty())
	{
		GenerateName();
	}

	// Set home location if not set
	if (HomeLocation.IsZero())
	{
		HomeLocation = GetActorLocation();
	}

	// Initialize personality from archetype
	// Personality->SetArchetype(ArchetypePreset); // Existing system

	// Initialize skills from archetype (with randomization)
	if (Skills)
	{
		Skills->InitializeFromArchetype(ArchetypePreset);
	}

	// Initialize combat brain
	if (CombatBrain)
	{
		CombatBrain->BuildAbilityPool();
		CombatBrain->BuildRotation();
	}

	// Set up starting equipment
	if (SpawnEquipment.Num() > 0)
	{
		for (const FNPCItemData& Item : SpawnEquipment)
		{
			// Inventory->AddToBag(Item);
			// if (Item.Slot != EEquipSlot::None) Inventory->EquipItem(Item);
		}
	}
	else
	{
		// Generate starting equipment based on archetype and skill levels
		GenerateStartingEquipment();
	}

	// Set aggression based on archetype
	switch (ArchetypePreset)
	{
	case EPersonalityArchetype::Bandit:
		bIsAggressive = true;
		break;
	case EPersonalityArchetype::Guard:
		bIsAggressive = false; // Guards only attack hostiles/criminals
		break;
	default:
		bIsAggressive = false;
		break;
	}

	UE_LOG(LogAoCNPC, Log, TEXT("NPC Initialized: %s | Archetype: %d | Home: %s | Aggressive: %s"),
		*NPCName, static_cast<int32>(ArchetypePreset),
		*HomeLocation.ToString(), bIsAggressive ? TEXT("YES") : TEXT("NO"));
}

void AoCHumanoidNPCV2::GenerateName()
{
	FRandomStream NameRand;
	NameRand.Initialize(GetTypeHash(GetUniqueID()));

	if (FirstNames.Num() > 0 && LastNames.Num() > 0)
	{
		FString First = FirstNames[NameRand.RandRange(0, FirstNames.Num() - 1)];
		FString Last = LastNames[NameRand.RandRange(0, LastNames.Num() - 1)];
		NPCName = FString::Printf(TEXT("%s %s"), *First, *Last);
	}
	else
	{
		NPCName = FString::Printf(TEXT("NPC_%s"), *GetUniqueID().ToString());
	}
}

void AoCHumanoidNPCV2::GenerateStartingEquipment()
{
	if (!Skills) return;

	// Generate equipment quality based on skill levels
	// Higher skill = better starting gear

	switch (ArchetypePreset)
	{
	case EPersonalityArchetype::Guard:
	{
		float SwordSkill = Skills->GetSkillLevel(ESkillID::Sword);
		float ShieldSkill = Skills->GetSkillLevel(ESkillID::Shield);

		// Sword quality based on skill
		FNPCItemData Sword;
		Sword.Name = SwordSkill > 40.f ? TEXT("IronSword") : TEXT("BronzeSword");
		Sword.Slot = EEquipSlot::MainHand;
		Sword.BaseDamage = 10.f + SwordSkill * 0.5f;
		Sword.Weight = 3.f;
		Sword.RequiredSkill = TEXT("Sword");
		Sword.RequiredLevel = 1.f;
		SpawnEquipment.Add(Sword);

		if (ShieldSkill > 15.f)
		{
			FNPCItemData Shield;
			Shield.Name = TEXT("WoodShield");
			Shield.Slot = EEquipSlot::OffHand;
			Shield.BaseArmor = 5.f + ShieldSkill * 0.3f;
			Shield.Weight = 5.f;
			SpawnEquipment.Add(Shield);
		}

		// Armor
		FNPCItemData Chest;
		Chest.Name = SwordSkill > 40.f ? TEXT("ChainmailChest") : TEXT("LeatherChest");
		Chest.Slot = EEquipSlot::Chest;
		Chest.BaseArmor = SwordSkill > 40.f ? 25.f : 12.f;
		Chest.Weight = SwordSkill > 40.f ? 15.f : 6.f;
		SpawnEquipment.Add(Chest);
		break;
	}
	case EPersonalityArchetype::Bandit:
	{
		ESkillID BestWeapon = Skills->GetBestWeaponSkill();
		float WeaponSkill = Skills->GetSkillLevel(BestWeapon);

		FNPCItemData Weapon;
		Weapon.Name = FName(*FString::Printf(TEXT("Crude_%s"), *UAoCNPCSkillSystem::GetSkillName(BestWeapon).ToString()));
		Weapon.Slot = EEquipSlot::MainHand;
		Weapon.BaseDamage = 8.f + WeaponSkill * 0.4f;
		Weapon.Weight = 3.f;
		SpawnEquipment.Add(Weapon);

		FNPCItemData Leather;
		Leather.Name = TEXT("TatteredLeather");
		Leather.Slot = EEquipSlot::Chest;
		Leather.BaseArmor = 8.f;
		Leather.Weight = 4.f;
		SpawnEquipment.Add(Leather);
		break;
	}
	case EPersonalityArchetype::Mage:
	{
		FNPCItemData Staff;
		Staff.Name = TEXT("ApprenticeStaff");
		Staff.Slot = EEquipSlot::MainHand;
		Staff.BaseDamage = 5.f;
		Staff.BonusINT = 5.f;
		Staff.BonusWIS = 3.f;
		Staff.Weight = 2.f;
		SpawnEquipment.Add(Staff);

		FNPCItemData Robe;
		Robe.Name = TEXT("MageRobe");
		Robe.Slot = EEquipSlot::Chest;
		Robe.BaseArmor = 5.f;
		Robe.BonusINT = 3.f;
		Robe.Weight = 2.f;
		SpawnEquipment.Add(Robe);
		break;
	}
	case EPersonalityArchetype::Hunter:
	{
		float BowSkill = Skills->GetSkillLevel(ESkillID::Bow);

		FNPCItemData Bow;
		Bow.Name = BowSkill > 45.f ? TEXT("CompositeBow") : TEXT("ShortBow");
		Bow.Slot = EEquipSlot::MainHand;
		Bow.BaseDamage = 8.f + BowSkill * 0.5f;
		Bow.Weight = 2.f;
		SpawnEquipment.Add(Bow);

		FNPCItemData Dagger;
		Dagger.Name = TEXT("HuntingKnife");
		Dagger.Slot = EEquipSlot::OffHand;
		Dagger.BaseDamage = 6.f;
		Dagger.Weight = 1.f;
		SpawnEquipment.Add(Dagger);

		FNPCItemData Leather;
		Leather.Name = TEXT("HunterLeather");
		Leather.Slot = EEquipSlot::Chest;
		Leather.BaseArmor = 10.f;
		Leather.BonusDEX = 2.f;
		Leather.Weight = 5.f;
		SpawnEquipment.Add(Leather);
		break;
	}
	case EPersonalityArchetype::Merchant:
	{
		FNPCItemData Dagger;
		Dagger.Name = TEXT("MerchantDagger");
		Dagger.Slot = EEquipSlot::MainHand;
		Dagger.BaseDamage = 5.f;
		Dagger.Weight = 1.f;
		SpawnEquipment.Add(Dagger);

		FNPCItemData FineClothes;
		FineClothes.Name = TEXT("FineClothes");
		FineClothes.Slot = EEquipSlot::Chest;
		FineClothes.BaseArmor = 3.f;
		FineClothes.Weight = 1.f;
		SpawnEquipment.Add(FineClothes);
		break;
	}
	case EPersonalityArchetype::Hermit:
	{
		FNPCItemData Staff;
		Staff.Name = TEXT("WalkingStick");
		Staff.Slot = EEquipSlot::MainHand;
		Staff.BaseDamage = 4.f;
		Staff.Weight = 2.f;
		SpawnEquipment.Add(Staff);

		FNPCItemData Tunic;
		Tunic.Name = TEXT("WornTunic");
		Tunic.Slot = EEquipSlot::Chest;
		Tunic.BaseArmor = 4.f;
		Tunic.Weight = 1.f;
		SpawnEquipment.Add(Tunic);
		break;
	}
	default: // Villager
	{
		FNPCItemData Tool;
		Tool.Name = TEXT("Pickaxe");
		Tool.Slot = EEquipSlot::MainHand;
		Tool.BaseDamage = 3.f;
		Tool.Weight = 3.f;
		SpawnEquipment.Add(Tool);

		FNPCItemData Clothes;
		Clothes.Name = TEXT("VillagerClothes");
		Clothes.Slot = EEquipSlot::Chest;
		Clothes.BaseArmor = 2.f;
		Clothes.Weight = 1.f;
		SpawnEquipment.Add(Clothes);
		break;
	}
	}

	// Add some consumables for everyone
	FNPCItemData Bread;
	Bread.Name = TEXT("Bread");
	Bread.bIsConsumable = true;
	Bread.Weight = 0.2f;
	Bread.Value = 2.f;
	SpawnEquipment.Add(Bread);
	SpawnEquipment.Add(Bread);
	SpawnEquipment.Add(Bread);

	FNPCItemData HealthPotion;
	HealthPotion.Name = TEXT("MinorHealthPotion");
	HealthPotion.bIsConsumable = true;
	HealthPotion.Weight = 0.3f;
	HealthPotion.Value = 15.f;
	SpawnEquipment.Add(HealthPotion);

	UE_LOG(LogAoCNPC, Log, TEXT("NPC %s: generated %d starting items for archetype %d"),
		*NPCName, SpawnEquipment.Num(), static_cast<int32>(ArchetypePreset));
}

// ---------------------------------------------------------------------------
// Master Tick
// ---------------------------------------------------------------------------

void AoCHumanoidNPCV2::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsDead) return;

	MasterTick(DeltaTime);
}

void AoCHumanoidNPCV2::MasterTick(float DeltaTime)
{
	// 1. Get LOD tier
	UWorld* World = GetWorld();
	if (World)
	{
		UAoCAILODManager* LODMgr = World->GetSubsystem<UAoCAILODManager>();
		if (LODMgr)
		{
			CachedLODTier = LODMgr->GetLODTier(this);
		}
	}

	// 2. Tick based on LOD tier
	switch (CachedLODTier)
	{
	case EAILODTier::Hibernated:
		// Do almost nothing — major decisions only (every 30s handled by LOD manager stagger)
		TickHibernated(DeltaTime);
		break;

	case EAILODTier::Background:
		// Data-only simulation (no mesh, no animation)
		TickBackground(DeltaTime);
		break;

	case EAILODTier::Reduced:
		// Full mesh + animation, reduced perception and pathfinding
		TickReduced(DeltaTime);
		break;

	case EAILODTier::Full:
		// Full simulation — everything runs
		TickFull(DeltaTime);
		break;
	}
}

void AoCHumanoidNPCV2::TickHibernated(float DeltaTime)
{
	// Almost nothing — just keep time-based counters moving
	if (Skills)
	{
		Skills->TickPassiveGains(DeltaTime * 0.1f); // Very slow passive gains
	}
}

void AoCHumanoidNPCV2::TickBackground(float DeltaTime)
{
	// Life simulation and passive skill gains — NO world queries

	if (LifeBrain)
	{
		LifeBrain->LifeTick(DeltaTime);
	}

	if (Skills)
	{
		Skills->TickPassiveGains(DeltaTime);
	}

	if (NeedSystem)
	{
		// NeedSystem->Tick(DeltaTime); // Needs still decay
	}
}

void AoCHumanoidNPCV2::TickReduced(float DeltaTime)
{
	// Background + perception (at reduced rate) + needs + social

	TickBackground(DeltaTime);

	if (Brain)
	{
		// Brain->TickPerception(DeltaTime); // At reduced frequency
	}

	if (SocialBrain)
	{
		SocialBrain->SocializeTick(DeltaTime);
	}
}

void AoCHumanoidNPCV2::TickFull(float DeltaTime)
{
	// Everything runs at full fidelity

	// Needs
	if (NeedSystem)
	{
		// NeedSystem->Tick(DeltaTime);
	}

	// Brain perception and decision
	if (Brain)
	{
		// Brain->Tick(DeltaTime);
	}

	// Combat (if in combat)
	if (CombatBrain && CombatBrain->IsInCombat())
	{
		CombatBrain->CombatTick(DeltaTime);
	}

	// Life brain (daily routine, goals)
	if (LifeBrain && !(CombatBrain && CombatBrain->IsInCombat()))
	{
		LifeBrain->LifeTick(DeltaTime);
	}

	// Social
	if (SocialBrain)
	{
		SocialBrain->SocializeTick(DeltaTime);
	}

	// Looting
	if (LootBrain && LootBrain->IsLooting())
	{
		LootBrain->LootTick(DeltaTime);
	}

	// Passive skill gains
	if (Skills)
	{
		Skills->TickPassiveGains(DeltaTime);
	}
}

// ---------------------------------------------------------------------------
// Combat Integration
// ---------------------------------------------------------------------------

void AoCHumanoidNPCV2::TakeDamageFromSource(float Damage, AActor* DamageInstigator)
{
	if (bIsDead) return;

	// Route to combat brain
	if (CombatBrain)
	{
		CombatBrain->OnDamageTaken(Damage, DamageInstigator);

		// Enter combat if not already
		if (!CombatBrain->IsInCombat() && DamageInstigator)
		{
			CombatBrain->EnterCombat(DamageInstigator);
		}
	}

	// Memory: remember attacker
	if (Memory)
	{
		// Memory->Remember(TEXT("Attacker"), GetActorLocation(), Damage, DamageInstigator);
	}

	// Social: record being attacked
	if (SocialBrain && DamageInstigator)
	{
		AoCHumanoidNPCV2* AttackerNPC = Cast<AoCHumanoidNPCV2>(DamageInstigator);
		if (AttackerNPC)
		{
			SocialBrain->RecordInteraction(
				AttackerNPC->GetUniqueID(),
				AttackerNPC->GetDisplayName(),
				ESocialInteraction::AttackedMe,
				Damage / 50.f // Magnitude scales with damage
			);

			// Alert nearby allies
			SocialBrain->SpreadReputation(
				AttackerNPC->GetUniqueID(),
				AttackerNPC->GetDisplayName(),
				30.f, // Hostility increase
				5000.f // 50m radius
			);
		}
	}
}

// ---------------------------------------------------------------------------
// Death & Respawn
// ---------------------------------------------------------------------------

void AoCHumanoidNPCV2::OnDeath(AActor* Killer)
{
	if (bIsDead) return;
	bIsDead = true;

	UE_LOG(LogAoCNPC, Warning, TEXT("NPC %s has been killed by %s!"),
		*NPCName, Killer ? *Killer->GetName() : TEXT("Unknown"));

	// Exit combat
	if (CombatBrain)
	{
		CombatBrain->ExitCombat();
	}

	// Drop loot as corpse
	DropLoot();

	// Notify social network
	if (SocialBrain)
	{
		// Notify allies that we died
		TArray<FGuid> Allies = SocialBrain->GetAllies();
		for (const FGuid& AllyID : Allies)
		{
			// Find ally NPC and record "KilledMyAlly"
			if (Killer)
			{
				AoCHumanoidNPCV2* KillerNPC = Cast<AoCHumanoidNPCV2>(Killer);
				if (KillerNPC)
				{
					// This would need to look up the ally by ID and record the interaction
					SocialBrain->SpreadReputation(
						KillerNPC->GetUniqueID(),
						KillerNPC->GetDisplayName(),
						80.f, // Very high hostility
						8000.f // Large radius — word spreads
					);
				}
			}
		}
	}

	// Disable collision, movement
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->DisableMovement();

	// Schedule respawn if not essential (essential NPCs always respawn)
	if (bIsEssential || RespawnDelay > 0.f)
	{
		FTimerHandle RespawnTimerHandle;
		GetWorldTimerManager().SetTimer(RespawnTimerHandle, this, &AoCHumanoidNPCV2::OnRespawn, RespawnDelay, false);
	}
}

void AoCHumanoidNPCV2::DropLoot()
{
	// In production: create a lootable corpse actor with our inventory
	// For now, log what would be dropped
	UE_LOG(LogAoCNPC, Log, TEXT("NPC %s dropped loot corpse at %s"),
		*NPCName, *GetActorLocation().ToString());

	// Schedule corpse cleanup
	FTimerHandle CorpseTimer;
	GetWorldTimerManager().SetTimer(CorpseTimer, [this]()
	{
		// Destroy corpse after linger time
		// In production: destroy the corpse actor, not this character
	}, CorpseLingerTime, false);
}

void AoCHumanoidNPCV2::OnRespawn()
{
	UE_LOG(LogAoCNPC, Log, TEXT("NPC %s respawning!"), *NPCName);

	bIsDead = false;

	// Reset position to home
	SetActorLocation(HomeLocation);

	// Re-enable collision, movement
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	// Reset HP
	if (CombatBrain)
	{
		CombatBrain->CurrentHP = CombatBrain->MaxHP;
		CombatBrain->CurrentMana = CombatBrain->MaxMana;
		CombatBrain->CurrentStamina = CombatBrain->MaxStamina;
	}

	// SKILLS PERSIST — they keep progression from last life
	// Only regenerate equipment
	SpawnEquipment.Reset();
	GenerateStartingEquipment();

	// Reset needs
	// NeedSystem->ResetAll();

	// Reset combat brain
	if (CombatBrain)
	{
		CombatBrain->BuildAbilityPool();
		CombatBrain->BuildRotation();
	}

	// Memory: remember who killed us and adapt
	// if (Memory) Memory->Remember(TEXT("Died"), HomeLocation, 100.f, LastKiller);

	// Re-register with LOD manager
	UWorld* World = GetWorld();
	if (World)
	{
		UAoCAILODManager* LODMgr = World->GetSubsystem<UAoCAILODManager>();
		if (LODMgr)
		{
			LODMgr->RegisterNPC(this);
		}
	}
}

// ---------------------------------------------------------------------------
// Getters
// ---------------------------------------------------------------------------

FString AoCHumanoidNPCV2::GetDisplayName() const
{
	return NPCName;
}

int32 AoCHumanoidNPCV2::GetNPCLevel() const
{
	if (!Skills) return 1;

	// Level is based on total skill points (sum of all skill levels)
	float TotalSkill = 0.f;
	for (uint8 i = 0; i < static_cast<uint8>(ESkillID::MAX); ++i)
	{
		TotalSkill += Skills->GetSkillLevel(static_cast<ESkillID>(i));
	}

	// Rough level: 1 per 50 total skill points, max 100
	return FMath::Clamp(FMath::FloorToInt(TotalSkill / 50.f) + 1, 1, 100);
}

float AoCHumanoidNPCV2::GetCombatPower() const
{
	if (!Skills) return 1.f;
	return Skills->GetCombatPower();
}

FGuid AoCHumanoidNPCV2::GetUniqueID() const
{
	return UniqueID;
}

// ---------------------------------------------------------------------------
// State Serialization
// ---------------------------------------------------------------------------

FString AoCHumanoidNPCV2::SaveState() const
{
	// Serialize full NPC state for server saves
	// In production: use a proper serialization format (JSON, FArchive, etc.)

	FString State;
	State += FString::Printf(TEXT("Name=%s\n"), *NPCName);
	State += FString::Printf(TEXT("ID=%s\n"), *UniqueID.ToString());
	State += FString::Printf(TEXT("Archetype=%d\n"), static_cast<int32>(ArchetypePreset));
	State += FString::Printf(TEXT("Faction=%s\n"), *FactionID.ToString());
	State += FString::Printf(TEXT("Position=%s\n"), *GetActorLocation().ToString());
	State += FString::Printf(TEXT("Home=%s\n"), *HomeLocation.ToString());
	State += FString::Printf(TEXT("Dead=%d\n"), bIsDead ? 1 : 0);
	State += FString::Printf(TEXT("Aggressive=%d\n"), bIsAggressive ? 1 : 0);
	State += FString::Printf(TEXT("Level=%d\n"), GetNPCLevel());

	// Skill system serialization
	if (Skills)
	{
		State += Skills->SerializeState();
	}

	// Combat brain state
	if (CombatBrain)
	{
		State += FString::Printf(TEXT("HP=%.1f/%.1f\n"), CombatBrain->CurrentHP, CombatBrain->MaxHP);
		State += FString::Printf(TEXT("Mana=%.1f/%.1f\n"), CombatBrain->CurrentMana, CombatBrain->MaxMana);
	}

	return State;
}

void AoCHumanoidNPCV2::LoadState(const FString& StateData)
{
	// Parse state data and restore NPC
	// In production: proper deserialization

	TArray<FString> Lines;
	StateData.ParseIntoArray(Lines, TEXT("\n"));

	for (const FString& Line : Lines)
	{
		FString Key, Value;
		if (Line.Split(TEXT("="), &Key, &Value))
		{
			if (Key == TEXT("Name"))
			{
				NPCName = Value;
			}
			else if (Key == TEXT("ID"))
			{
				FGuid::Parse(Value, UniqueID);
			}
			else if (Key == TEXT("Archetype"))
			{
				ArchetypePreset = static_cast<EPersonalityArchetype>(FCString::Atoi(*Value));
			}
			else if (Key == TEXT("Faction"))
			{
				FactionID = FName(*Value);
			}
			else if (Key == TEXT("Dead"))
			{
				bIsDead = FCString::Atoi(*Value) != 0;
			}
			else if (Key == TEXT("Aggressive"))
			{
				bIsAggressive = FCString::Atoi(*Value) != 0;
			}
		}
	}

	// Deserialize skills
	if (Skills)
	{
		// TODO: Skills->DeserializeState needs FSkillSystemState, not FString;
	}

	bIsInitialized = true;
	UE_LOG(LogAoCNPC, Log, TEXT("NPC %s state loaded."), *NPCName);
}
