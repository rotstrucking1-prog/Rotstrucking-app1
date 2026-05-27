// AoCNPCSkillSystem.cpp
// Full skill system implementation — 48 skills, 6 attributes, use-based leveling

#include "AoCNPCSkillSystem.h"
#include "AoCHumanoidNPCV2.h"

DEFINE_LOG_CATEGORY(LogAoCSkill);

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

static const FName SkillNames[] =
{
	// Weapon
	TEXT("Sword"), TEXT("Greatsword"), TEXT("Dagger"), TEXT("Axe"), TEXT("GreatAxe"),
	TEXT("Hammer"), TEXT("GreatHammer"), TEXT("Mace"), TEXT("GreatMace"), TEXT("Scythe"),
	TEXT("Spear"), TEXT("Claws"), TEXT("Bow"), TEXT("Shield"), TEXT("Staff1H"),
	TEXT("Staff2H"), TEXT("Unarmed"),
	// Magic
	TEXT("Arcana"), TEXT("Pyromancy"), TEXT("Cryomancy"), TEXT("Stormcalling"),
	TEXT("Tempest"), TEXT("Verdancy"), TEXT("Umbramancy"), TEXT("Radiance"),
	TEXT("Sangromancy"), TEXT("Dominion"),
	// Gathering
	TEXT("Mining"), TEXT("Woodcutting"), TEXT("Fishing"), TEXT("Herbalism"),
	TEXT("Hunting"), TEXT("Farming"), TEXT("Skinning"),
	// Crafting
	TEXT("WeaponSmithing"), TEXT("ArmorSmithing"), TEXT("BowCrafting"), TEXT("StaffCrafting"),
	TEXT("Jewelcrafting"), TEXT("Enchanting"), TEXT("Alchemy"), TEXT("Cooking"),
	TEXT("Brewing"), TEXT("Tailoring"), TEXT("Leatherworking"),
	// Construction
	TEXT("Masonry"), TEXT("Carpentry"), TEXT("Architecture"),
};

// ---------------------------------------------------------------------------
// Constructor / BeginPlay
// ---------------------------------------------------------------------------

UAoCNPCSkillSystem::UAoCNPCSkillSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
	FMemory::Memzero(Attributes, sizeof(Attributes));
	// All attributes start at 10
	for (int32 i = 0; i < static_cast<int32>(EAttributeID::MAX); ++i)
	{
		Attributes[i] = 10.f;
	}
}

void UAoCNPCSkillSystem::BeginPlay()
{
	Super::BeginPlay();

	// Initialize all 48 skills if not already done
	for (uint8 i = 0; i < static_cast<uint8>(ESkillID::MAX); ++i)
	{
		ESkillID ID = static_cast<ESkillID>(i);
		if (!Skills.Contains(i))
		{
			InitSkill(ID, GetSkillName(ID), 0.f);
		}
	}

	RandStream.Initialize(GetOwner() ? GetOwner()->GetUniqueID() : FMath::Rand());
}

void UAoCNPCSkillSystem::InitSkill(ESkillID ID, FName Name, float StartLevel)
{
	FNPCSkillData Data;
	Data.SkillName = Name;
	Data.Level = StartLevel;
	Data.TotalXP = 0.f;
	Data.bMasteryUnlocked = false;
	Data.MasteryLevel = 0.f;
	Data.LastUsedTime = 0.f;
	Data.RecentUseCount = 0.f;
	Data.RecentUseDecayTimer = 0.f;

	// If starting above 0, compute the total XP that corresponds to that level
	if (StartLevel > 0.f)
	{
		float TotalXPNeeded = 0.f;
		for (float L = 0; L < StartLevel; L += 1.f)
		{
			TotalXPNeeded += XPForLevel(L);
		}
		Data.TotalXP = TotalXPNeeded;
	}

	Skills.Add(static_cast<uint8>(ID), Data);
}

// ---------------------------------------------------------------------------
// XP Formulas
// ---------------------------------------------------------------------------

float UAoCNPCSkillSystem::XPForLevel(float Level)
{
	// XP required to go from Level to Level+1
	// Exponential: BaseXP * (Level+1)^1.5
	return BaseXPPerLevel * FMath::Pow(FMath::Max(Level + 1.f, 1.f), LevelExponent);
}

float UAoCNPCSkillSystem::LevelFromXP(float TotalXP)
{
	float Level = 0.f;
	float AccumulatedXP = 0.f;

	while (Level < 100.f)
	{
		float XPNeeded = XPForLevel(Level);
		if (AccumulatedXP + XPNeeded > TotalXP)
		{
			// Fractional level
			float Remainder = TotalXP - AccumulatedXP;
			return Level + (Remainder / XPNeeded);
		}
		AccumulatedXP += XPNeeded;
		Level += 1.f;
	}

	return 100.f;
}

// ---------------------------------------------------------------------------
// Core Skill Functions
// ---------------------------------------------------------------------------

void UAoCNPCSkillSystem::GainSkillXP(ESkillID Skill, float BaseXP, float DifficultyModifier)
{
	uint8 Key = static_cast<uint8>(Skill);
	FNPCSkillData* Data = Skills.Find(Key);
	if (!Data) return;

	// Can't gain XP beyond 100 (or mastery 100 if mastery unlocked)
	if (Data->Level >= 100.f && !Data->bMasteryUnlocked) return;
	if (Data->bMasteryUnlocked && Data->MasteryLevel >= 100.f) return;

	// Difficulty modifier: harder tasks = more XP
	// But if task is WAY below your level, minimal XP
	float LevelPenalty = 1.f;
	if (DifficultyModifier < 0.5f)
	{
		// Easy task relative to skill — reduced XP
		LevelPenalty = DifficultyModifier * 2.f; // 0-1 range
		LevelPenalty = FMath::Max(LevelPenalty, 0.05f); // Minimum 5% XP
	}
	else if (DifficultyModifier > 1.5f)
	{
		// Hard task — bonus XP (capped)
		LevelPenalty = FMath::Min(DifficultyModifier, 3.0f);
	}
	else
	{
		LevelPenalty = DifficultyModifier;
	}

	// Diminishing returns for grinding same action
	float DiminishingMult = GetDiminishingReturns(Skill);

	// Higher levels = slower gains naturally (XP requirement increases)
	float FinalXP = BaseXP * LevelPenalty * DiminishingMult;
	FinalXP = FMath::Max(FinalXP, 0.01f);

	// Track recent use for diminishing returns
	Data->RecentUseCount += 1.f;
	Data->RecentUseDecayTimer = 0.f;
	Data->LastUsedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (Data->bMasteryUnlocked)
	{
		// Mastery XP (separate progression, same formula but slower)
		float MasteryXP = FinalXP * 0.5f; // Mastery gains at half rate
		float CurrentMasteryXP = 0.f;
		for (float L = 0; L < Data->MasteryLevel; L += 1.f)
		{
			CurrentMasteryXP += XPForLevel(L) * 1.5f; // Mastery costs 50% more
		}
		CurrentMasteryXP += MasteryXP;

		// Recalculate mastery level
		float NewMastery = 0.f;
		float Acc = 0.f;
		while (NewMastery < 100.f)
		{
			float Needed = XPForLevel(NewMastery) * 1.5f;
			if (Acc + Needed > CurrentMasteryXP)
			{
				NewMastery += (CurrentMasteryXP - Acc) / Needed;
				break;
			}
			Acc += Needed;
			NewMastery += 1.f;
		}

		float OldMastery = Data->MasteryLevel;
		Data->MasteryLevel = FMath::Clamp(NewMastery, 0.f, 100.f);

		if (FMath::FloorToInt(Data->MasteryLevel) > FMath::FloorToInt(OldMastery))
		{
			UE_LOG(LogAoCSkill, Log, TEXT("NPC skill %s mastery up: %.1f → %.1f"),
				*Data->SkillName.ToString(), OldMastery, Data->MasteryLevel);
		}
	}
	else
	{
		Data->TotalXP += FinalXP;
		float OldLevel = Data->Level;
		Data->Level = FMath::Clamp(LevelFromXP(Data->TotalXP), 0.f, 100.f);

		// Check for mastery unlock at 100
		if (Data->Level >= 100.f && !Data->bMasteryUnlocked)
		{
			Data->bMasteryUnlocked = true;
			Data->MasteryLevel = 0.f;
			UE_LOG(LogAoCSkill, Log, TEXT("NPC skill %s MASTERY UNLOCKED!"), *Data->SkillName.ToString());
		}

		// Log level-ups
		if (FMath::FloorToInt(Data->Level) > FMath::FloorToInt(OldLevel))
		{
			UE_LOG(LogAoCSkill, Log, TEXT("NPC skill %s level up: %.1f → %.1f"),
				*Data->SkillName.ToString(), OldLevel, Data->Level);
		}
	}

	// Apply attribute gains from this skill use
	ApplyAttributeGain(Skill, FinalXP);

	// Mark combat power for recalculation
	bCombatPowerDirty = true;
}

float UAoCNPCSkillSystem::GetDiminishingReturns(ESkillID Skill) const
{
	uint8 Key = static_cast<uint8>(Skill);
	const FNPCSkillData* Data = Skills.Find(Key);
	if (!Data) return 1.f;

	// DR formula: multiplier = 1 / (1 + recentUse * 0.1)
	// At 0 recent uses: 1.0x
	// At 5 recent uses: ~0.67x
	// At 10 recent uses: ~0.5x
	// At 20 recent uses: ~0.33x
	return 1.f / (1.f + Data->RecentUseCount * 0.1f);
}

void UAoCNPCSkillSystem::DecayRecentUse(float DeltaTime)
{
	for (auto& Pair : Skills)
	{
		FNPCSkillData& Data = Pair.Value;
		if (Data.RecentUseCount > 0.f)
		{
			Data.RecentUseDecayTimer += DeltaTime;
			// Decay by half-life
			float DecayRate = FMath::Loge(2.f) / DiminishingReturnHalfLife;
			Data.RecentUseCount *= FMath::Exp(-DecayRate * DeltaTime);
			if (Data.RecentUseCount < 0.01f)
			{
				Data.RecentUseCount = 0.f;
			}
		}
	}
}

float UAoCNPCSkillSystem::GetSkillLevel(ESkillID Skill) const
{
	uint8 Key = static_cast<uint8>(Skill);
	const FNPCSkillData* Data = Skills.Find(Key);
	return Data ? Data->Level : 0.f;
}

float UAoCNPCSkillSystem::GetAttribute(EAttributeID Attribute) const
{
	uint8 Idx = static_cast<uint8>(Attribute);
	if (Idx < static_cast<uint8>(EAttributeID::MAX))
	{
		return Attributes[Idx];
	}
	return 0.f;
}

void UAoCNPCSkillSystem::SetSkillLevel(ESkillID Skill, float NewLevel)
{
	uint8 Key = static_cast<uint8>(Skill);
	FNPCSkillData* Data = Skills.Find(Key);
	if (Data)
	{
		Data->Level = FMath::Clamp(NewLevel, 0.f, 100.f);
		// Recalculate TotalXP to match
		float TotalXP = 0.f;
		for (float L = 0; L < FMath::FloorToFloat(NewLevel); L += 1.f)
		{
			TotalXP += XPForLevel(L);
		}
		float Frac = NewLevel - FMath::FloorToFloat(NewLevel);
		TotalXP += XPForLevel(FMath::FloorToFloat(NewLevel)) * Frac;
		Data->TotalXP = TotalXP;
		bCombatPowerDirty = true;
	}
}

void UAoCNPCSkillSystem::SetAttribute(EAttributeID Attribute, float NewValue)
{
	uint8 Idx = static_cast<uint8>(Attribute);
	if (Idx < static_cast<uint8>(EAttributeID::MAX))
	{
		Attributes[Idx] = FMath::Clamp(NewValue, 1.f, 100.f);
		bCombatPowerDirty = true;
	}
}

// ---------------------------------------------------------------------------
// Attribute Gains
// ---------------------------------------------------------------------------

void UAoCNPCSkillSystem::ApplyAttributeGain(ESkillID Skill, float XPGained)
{
	float AttrXP = XPGained * AttributeGainRate;
	if (AttrXP < 0.001f) return;

	ESkillCategory Cat = GetSkillCategory(Skill);

	switch (Cat)
	{
	case ESkillCategory::Weapon:
	{
		// Heavy weapons → STR, finesse → DEX, fast → AGI
		switch (Skill)
		{
		case ESkillID::Greatsword:
		case ESkillID::GreatAxe:
		case ESkillID::GreatHammer:
		case ESkillID::GreatMace:
		case ESkillID::Hammer:
			Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.01f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::END)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::END)] + AttrXP * 0.005f, 100.f);
			break;
		case ESkillID::Dagger:
		case ESkillID::Claws:
			Attributes[static_cast<uint8>(EAttributeID::DEX)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::DEX)] + AttrXP * 0.01f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::AGI)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::AGI)] + AttrXP * 0.005f, 100.f);
			break;
		case ESkillID::Bow:
			Attributes[static_cast<uint8>(EAttributeID::DEX)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::DEX)] + AttrXP * 0.008f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::AGI)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::AGI)] + AttrXP * 0.005f, 100.f);
			break;
		case ESkillID::Shield:
			Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.007f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::END)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::END)] + AttrXP * 0.007f, 100.f);
			break;
		case ESkillID::Unarmed:
			Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.005f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::AGI)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::AGI)] + AttrXP * 0.008f, 100.f);
			break;
		default:
			// Sword, axe, mace, spear, scythe, staves — balanced
			Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.006f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::DEX)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::DEX)] + AttrXP * 0.006f, 100.f);
			break;
		}
		break;
	}
	case ESkillCategory::Magic:
	{
		// Offensive magic → INT, healing/utility → WIS
		switch (Skill)
		{
		case ESkillID::Pyromancy:
		case ESkillID::Cryomancy:
		case ESkillID::Stormcalling:
		case ESkillID::Tempest:
		case ESkillID::Umbramancy:
		case ESkillID::Sangromancy:
			Attributes[static_cast<uint8>(EAttributeID::INT)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::INT)] + AttrXP * 0.01f, 100.f);
			break;
		case ESkillID::Verdancy:
		case ESkillID::Radiance:
			Attributes[static_cast<uint8>(EAttributeID::WIS)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::WIS)] + AttrXP * 0.01f, 100.f);
			break;
		case ESkillID::Arcana:
		case ESkillID::Dominion:
			Attributes[static_cast<uint8>(EAttributeID::INT)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::INT)] + AttrXP * 0.006f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::WIS)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::WIS)] + AttrXP * 0.006f, 100.f);
			break;
		default:
			Attributes[static_cast<uint8>(EAttributeID::INT)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::INT)] + AttrXP * 0.008f, 100.f);
			break;
		}
		break;
	}
	case ESkillCategory::Gathering:
	{
		Attributes[static_cast<uint8>(EAttributeID::END)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::END)] + AttrXP * 0.01f, 100.f);
		Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.003f, 100.f);
		break;
	}
	case ESkillCategory::Crafting:
	{
		// Complex crafting → INT, precision crafting → DEX
		switch (Skill)
		{
		case ESkillID::Enchanting:
		case ESkillID::Alchemy:
			Attributes[static_cast<uint8>(EAttributeID::INT)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::INT)] + AttrXP * 0.008f, 100.f);
			break;
		case ESkillID::Jewelcrafting:
		case ESkillID::Tailoring:
			Attributes[static_cast<uint8>(EAttributeID::DEX)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::DEX)] + AttrXP * 0.008f, 100.f);
			break;
		default:
			Attributes[static_cast<uint8>(EAttributeID::DEX)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::DEX)] + AttrXP * 0.005f, 100.f);
			Attributes[static_cast<uint8>(EAttributeID::INT)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::INT)] + AttrXP * 0.005f, 100.f);
			break;
		}
		break;
	}
	case ESkillCategory::Construction:
	{
		Attributes[static_cast<uint8>(EAttributeID::STR)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::STR)] + AttrXP * 0.007f, 100.f);
		Attributes[static_cast<uint8>(EAttributeID::END)] = FMath::Min(Attributes[static_cast<uint8>(EAttributeID::END)] + AttrXP * 0.005f, 100.f);
		break;
	}
	}
}

// ---------------------------------------------------------------------------
// Combat Calculations
// ---------------------------------------------------------------------------

float UAoCNPCSkillSystem::GetCombatPower() const
{
	if (!bCombatPowerDirty)
	{
		return CachedCombatPower;
	}

	// Combat power = (best weapon skill * 2 + best magic skill) + attribute bonuses
	float BestWeapon = 0.f;
	float BestMagic = 0.f;
	float SecondWeapon = 0.f;

	for (const auto& Pair : Skills)
	{
		ESkillID ID = static_cast<ESkillID>(Pair.Key);
		float Level = Pair.Value.Level;

		if (IsWeaponSkill(ID))
		{
			if (Level > BestWeapon)
			{
				SecondWeapon = BestWeapon;
				BestWeapon = Level;
			}
			else if (Level > SecondWeapon)
			{
				SecondWeapon = Level;
			}
		}
		else if (IsMagicSkill(ID))
		{
			BestMagic = FMath::Max(BestMagic, Level);
		}
	}

	float Power = BestWeapon * 2.f + SecondWeapon * 0.5f + BestMagic * 1.5f;

	// Attribute bonuses
	float STR = Attributes[static_cast<uint8>(EAttributeID::STR)];
	float DEX = Attributes[static_cast<uint8>(EAttributeID::DEX)];
	float AGI = Attributes[static_cast<uint8>(EAttributeID::AGI)];
	float INT = Attributes[static_cast<uint8>(EAttributeID::INT)];
	float WIS = Attributes[static_cast<uint8>(EAttributeID::WIS)];
	float END = Attributes[static_cast<uint8>(EAttributeID::END)];

	Power += (STR + DEX + AGI) * 0.5f; // Physical attributes
	Power += (INT + WIS) * 0.3f;       // Mental attributes
	Power += END * 0.4f;               // Endurance/HP contribution

	// Mastery bonus
	for (const auto& Pair : Skills)
	{
		if (Pair.Value.bMasteryUnlocked)
		{
			Power += Pair.Value.MasteryLevel * 0.5f;
		}
	}

	CachedCombatPower = Power;
	bCombatPowerDirty = false;
	return CachedCombatPower;
}

float UAoCNPCSkillSystem::GetEffectiveDamage(ESkillID WeaponSkill) const
{
	float SkillLevel = GetSkillLevel(WeaponSkill);
	float STR = Attributes[static_cast<uint8>(EAttributeID::STR)];
	float DEX = Attributes[static_cast<uint8>(EAttributeID::DEX)];

	// Base damage scales with skill level
	float BaseDamage = SkillLevel * 1.5f;

	// Attribute scaling depends on weapon type
	float AttrBonus = 0.f;
	switch (WeaponSkill)
	{
	case ESkillID::Greatsword:
	case ESkillID::GreatAxe:
	case ESkillID::GreatHammer:
	case ESkillID::GreatMace:
	case ESkillID::Hammer:
		AttrBonus = STR * 0.8f + DEX * 0.2f;
		break;
	case ESkillID::Dagger:
	case ESkillID::Claws:
		AttrBonus = DEX * 0.8f + STR * 0.2f;
		break;
	case ESkillID::Bow:
		AttrBonus = DEX * 0.7f + STR * 0.3f;
		break;
	default:
		AttrBonus = STR * 0.5f + DEX * 0.5f;
		break;
	}

	// Mastery bonus
	uint8 Key = static_cast<uint8>(WeaponSkill);
	const FNPCSkillData* Data = Skills.Find(Key);
	float MasteryMult = 1.0f;
	if (Data && Data->bMasteryUnlocked)
	{
		MasteryMult = 1.0f + (Data->MasteryLevel * 0.005f); // Up to 50% bonus at mastery 100
	}

	return (BaseDamage + AttrBonus) * MasteryMult;
}

float UAoCNPCSkillSystem::GetEffectiveDefense() const
{
	float ShieldSkill = GetSkillLevel(ESkillID::Shield);
	float AGI = Attributes[static_cast<uint8>(EAttributeID::AGI)];
	float END = Attributes[static_cast<uint8>(EAttributeID::END)];

	// Defense = shield skill + AGI dodge + END toughness
	float Defense = ShieldSkill * 1.0f + AGI * 0.5f + END * 0.3f;

	return Defense;
}

float UAoCNPCSkillSystem::GetSpellPower(ESkillID MagicSchool) const
{
	if (!IsMagicSkill(MagicSchool)) return 0.f;

	float SkillLevel = GetSkillLevel(MagicSchool);
	float INT = Attributes[static_cast<uint8>(EAttributeID::INT)];
	float WIS = Attributes[static_cast<uint8>(EAttributeID::WIS)];

	float BasePower = SkillLevel * 2.0f;

	// INT for offensive, WIS for healing/utility
	float AttrBonus = 0.f;
	switch (MagicSchool)
	{
	case ESkillID::Verdancy:
	case ESkillID::Radiance:
		AttrBonus = WIS * 0.8f + INT * 0.2f;
		break;
	case ESkillID::Pyromancy:
	case ESkillID::Cryomancy:
	case ESkillID::Stormcalling:
	case ESkillID::Tempest:
	case ESkillID::Umbramancy:
	case ESkillID::Sangromancy:
		AttrBonus = INT * 0.8f + WIS * 0.2f;
		break;
	default:
		AttrBonus = INT * 0.5f + WIS * 0.5f;
		break;
	}

	// Mastery bonus
	uint8 Key = static_cast<uint8>(MagicSchool);
	const FNPCSkillData* Data = Skills.Find(Key);
	float MasteryMult = 1.0f;
	if (Data && Data->bMasteryUnlocked)
	{
		MasteryMult = 1.0f + (Data->MasteryLevel * 0.005f);
	}

	return (BasePower + AttrBonus) * MasteryMult;
}

// ---------------------------------------------------------------------------
// Crafting
// ---------------------------------------------------------------------------

bool UAoCNPCSkillSystem::CanCraft(FName RecipeName) const
{
	// Recipe skill requirements (simplified — real system would have a recipe database)
	// Convention: RecipeName format is "SkillName_Level" e.g. "WeaponSmithing_30"
	FString RecipeStr = RecipeName.ToString();
	FString SkillPart, LevelPart;

	if (RecipeStr.Split(TEXT("_"), &SkillPart, &LevelPart))
	{
		float RequiredLevel = FCString::Atof(*LevelPart);

		// Find the matching skill
		for (const auto& Pair : Skills)
		{
			if (Pair.Value.SkillName.ToString() == SkillPart)
			{
				return Pair.Value.Level >= RequiredLevel;
			}
		}
	}

	// Fallback: check all crafting skills against a generic level requirement
	return false;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

TArray<ESkillID> UAoCNPCSkillSystem::GetTopSkills(int32 Count) const
{
	// Collect all skills into sortable array
	TArray<TPair<ESkillID, float>> SkillLevels;
	SkillLevels.Reserve(Skills.Num());

	for (const auto& Pair : Skills)
	{
		SkillLevels.Add(TPair<ESkillID, float>(static_cast<ESkillID>(Pair.Key), Pair.Value.Level));
	}

	// Sort descending by level
	SkillLevels.Sort([](const TPair<ESkillID, float>& A, const TPair<ESkillID, float>& B)
	{
		return A.Value > B.Value;
	});

	TArray<ESkillID> Result;
	int32 Num = FMath::Min(Count, SkillLevels.Num());
	for (int32 i = 0; i < Num; ++i)
	{
		Result.Add(SkillLevels[i].Key);
	}
	return Result;
}

ESkillID UAoCNPCSkillSystem::GetBestWeaponSkill() const
{
	ESkillID Best = ESkillID::Unarmed;
	float BestLevel = 0.f;

	for (const auto& Pair : Skills)
	{
		ESkillID ID = static_cast<ESkillID>(Pair.Key);
		if (IsWeaponSkill(ID) && Pair.Value.Level > BestLevel)
		{
			BestLevel = Pair.Value.Level;
			Best = ID;
		}
	}
	return Best;
}

ESkillID UAoCNPCSkillSystem::GetBestMagicSkill() const
{
	ESkillID Best = ESkillID::MAX;
	float BestLevel = 0.f;

	for (const auto& Pair : Skills)
	{
		ESkillID ID = static_cast<ESkillID>(Pair.Key);
		if (IsMagicSkill(ID) && Pair.Value.Level > BestLevel)
		{
			BestLevel = Pair.Value.Level;
			Best = ID;
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------
// Static Category Helpers
// ---------------------------------------------------------------------------

bool UAoCNPCSkillSystem::IsWeaponSkill(ESkillID Skill)
{
	return Skill >= ESkillID::Sword && Skill <= ESkillID::Unarmed;
}

bool UAoCNPCSkillSystem::IsMagicSkill(ESkillID Skill)
{
	return Skill >= ESkillID::Arcana && Skill <= ESkillID::Dominion;
}

bool UAoCNPCSkillSystem::IsGatheringSkill(ESkillID Skill)
{
	return Skill >= ESkillID::Mining && Skill <= ESkillID::Skinning;
}

bool UAoCNPCSkillSystem::IsCraftingSkill(ESkillID Skill)
{
	return Skill >= ESkillID::WeaponSmithing && Skill <= ESkillID::Leatherworking;
}

bool UAoCNPCSkillSystem::IsConstructionSkill(ESkillID Skill)
{
	return Skill >= ESkillID::Masonry && Skill <= ESkillID::Architecture;
}

ESkillCategory UAoCNPCSkillSystem::GetSkillCategory(ESkillID Skill)
{
	if (IsWeaponSkill(Skill)) return ESkillCategory::Weapon;
	if (IsMagicSkill(Skill)) return ESkillCategory::Magic;
	if (IsGatheringSkill(Skill)) return ESkillCategory::Gathering;
	if (IsCraftingSkill(Skill)) return ESkillCategory::Crafting;
	return ESkillCategory::Construction;
}

FName UAoCNPCSkillSystem::GetSkillName(ESkillID Skill)
{
	uint8 Idx = static_cast<uint8>(Skill);
	if (Idx < static_cast<uint8>(ESkillID::MAX))
	{
		return SkillNames[Idx];
	}
	return TEXT("Unknown");
}

// ---------------------------------------------------------------------------
// Passive Gains
// ---------------------------------------------------------------------------

void UAoCNPCSkillSystem::TickPassiveGains(float DeltaTime)
{
	// Decay diminishing returns
	DecayRecentUse(DeltaTime);

	// Background skill gain from current practice activity
	if (CurrentPracticeSkill != ESkillID::MAX)
	{
		float XP = PassiveGainRate * DeltaTime;
		GainSkillXP(CurrentPracticeSkill, XP, 1.0f);
	}
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

FSkillSystemState UAoCNPCSkillSystem::SerializeState() const
{
	FSkillSystemState State;
	State.Skills = Skills;

	State.Attributes.SetNum(static_cast<int32>(EAttributeID::MAX));
	for (int32 i = 0; i < static_cast<int32>(EAttributeID::MAX); ++i)
	{
		State.Attributes[i] = Attributes[i];
	}

	State.TotalCombatPower = GetCombatPower();
	return State;
}

void UAoCNPCSkillSystem::DeserializeState(const FSkillSystemState& State)
{
	Skills = State.Skills;

	for (int32 i = 0; i < FMath::Min(State.Attributes.Num(), static_cast<int32>(EAttributeID::MAX)); ++i)
	{
		Attributes[i] = State.Attributes[i];
	}

	bCombatPowerDirty = true;
}

// ---------------------------------------------------------------------------
// Archetype Initialization
// ---------------------------------------------------------------------------

void UAoCNPCSkillSystem::InitializeFromArchetype(EPersonalityArchetype Archetype)
{
	// Reset all skills
	for (uint8 i = 0; i < static_cast<uint8>(ESkillID::MAX); ++i)
	{
		ESkillID ID = static_cast<ESkillID>(i);
		InitSkill(ID, GetSkillName(ID), 0.f);
	}

	auto RandRange = [this](float Min, float Max) -> float
	{
		return RandStream.FRandRange(Min, Max);
	};

	switch (Archetype)
	{
	case EPersonalityArchetype::Villager:
	{
		// Gathering 20-40, crafting 10-30, combat 5-15
		SetSkillLevel(ESkillID::Mining, RandRange(20.f, 40.f));
		SetSkillLevel(ESkillID::Woodcutting, RandRange(15.f, 35.f));
		SetSkillLevel(ESkillID::Fishing, RandRange(10.f, 30.f));
		SetSkillLevel(ESkillID::Farming, RandRange(20.f, 40.f));
		SetSkillLevel(ESkillID::Herbalism, RandRange(10.f, 25.f));
		SetSkillLevel(ESkillID::Cooking, RandRange(15.f, 35.f));

		// Random crafting specialization
		ESkillID CraftSkill = static_cast<ESkillID>(static_cast<uint8>(ESkillID::WeaponSmithing) + RandStream.RandRange(0, 10));
		SetSkillLevel(CraftSkill, RandRange(15.f, 30.f));

		// Minimal combat
		SetSkillLevel(ESkillID::Unarmed, RandRange(5.f, 15.f));
		ESkillID WeaponChoice = RandStream.RandRange(0, 1) == 0 ? ESkillID::Sword : ESkillID::Axe;
		SetSkillLevel(WeaponChoice, RandRange(5.f, 15.f));

		SetAttribute(EAttributeID::STR, RandRange(12.f, 22.f));
		SetAttribute(EAttributeID::END, RandRange(15.f, 25.f));
		SetAttribute(EAttributeID::DEX, RandRange(12.f, 20.f));
		break;
	}
	case EPersonalityArchetype::Guard:
	{
		// Weapon 30-60, shield 20-40
		ESkillID PrimaryWeapon;
		int32 WeaponRoll = RandStream.RandRange(0, 3);
		switch (WeaponRoll)
		{
		case 0: PrimaryWeapon = ESkillID::Sword; break;
		case 1: PrimaryWeapon = ESkillID::Spear; break;
		case 2: PrimaryWeapon = ESkillID::Mace; break;
		default: PrimaryWeapon = ESkillID::Axe; break;
		}
		SetSkillLevel(PrimaryWeapon, RandRange(30.f, 60.f));
		SetSkillLevel(ESkillID::Shield, RandRange(20.f, 40.f));
		SetSkillLevel(ESkillID::Unarmed, RandRange(10.f, 25.f));

		// Some guards have a bow for ranged
		if (RandStream.FRand() > 0.5f)
		{
			SetSkillLevel(ESkillID::Bow, RandRange(10.f, 25.f));
		}

		SetAttribute(EAttributeID::STR, RandRange(20.f, 35.f));
		SetAttribute(EAttributeID::END, RandRange(20.f, 35.f));
		SetAttribute(EAttributeID::AGI, RandRange(12.f, 22.f));
		SetAttribute(EAttributeID::DEX, RandRange(12.f, 22.f));
		break;
	}
	case EPersonalityArchetype::Bandit:
	{
		// Weapon 20-50, varied, stealth-adjacent
		ESkillID PrimaryWeapon;
		int32 WeaponRoll = RandStream.RandRange(0, 4);
		switch (WeaponRoll)
		{
		case 0: PrimaryWeapon = ESkillID::Dagger; break;
		case 1: PrimaryWeapon = ESkillID::Sword; break;
		case 2: PrimaryWeapon = ESkillID::Bow; break;
		case 3: PrimaryWeapon = ESkillID::Axe; break;
		default: PrimaryWeapon = ESkillID::Claws; break;
		}
		SetSkillLevel(PrimaryWeapon, RandRange(20.f, 50.f));

		// Secondary weapon
		ESkillID Secondary = (PrimaryWeapon == ESkillID::Bow) ? ESkillID::Dagger : ESkillID::Bow;
		SetSkillLevel(Secondary, RandRange(10.f, 30.f));

		// Some bandits know basic magic
		if (RandStream.FRand() > 0.7f)
		{
			ESkillID MagicSchool = static_cast<ESkillID>(static_cast<uint8>(ESkillID::Arcana) + RandStream.RandRange(0, 9));
			SetSkillLevel(MagicSchool, RandRange(10.f, 25.f));
		}

		SetSkillLevel(ESkillID::Skinning, RandRange(10.f, 30.f));
		SetSkillLevel(ESkillID::Hunting, RandRange(10.f, 25.f));
		SetSkillLevel(ESkillID::Cooking, RandRange(5.f, 20.f));

		SetAttribute(EAttributeID::STR, RandRange(15.f, 28.f));
		SetAttribute(EAttributeID::AGI, RandRange(18.f, 30.f));
		SetAttribute(EAttributeID::DEX, RandRange(15.f, 28.f));
		break;
	}
	case EPersonalityArchetype::Merchant:
	{
		// Crafting 30-50, social skills
		SetSkillLevel(ESkillID::Cooking, RandRange(25.f, 45.f));
		SetSkillLevel(ESkillID::Alchemy, RandRange(15.f, 35.f));

		// Random crafting specialty
		ESkillID Specialty;
		int32 SpecRoll = RandStream.RandRange(0, 3);
		switch (SpecRoll)
		{
		case 0: Specialty = ESkillID::WeaponSmithing; break;
		case 1: Specialty = ESkillID::ArmorSmithing; break;
		case 2: Specialty = ESkillID::Jewelcrafting; break;
		default: Specialty = ESkillID::Tailoring; break;
		}
		SetSkillLevel(Specialty, RandRange(30.f, 50.f));

		// Minimal self-defense
		SetSkillLevel(ESkillID::Dagger, RandRange(8.f, 20.f));

		SetAttribute(EAttributeID::INT, RandRange(20.f, 30.f));
		SetAttribute(EAttributeID::WIS, RandRange(15.f, 28.f));
		SetAttribute(EAttributeID::DEX, RandRange(15.f, 25.f));
		break;
	}
	case EPersonalityArchetype::Mage:
	{
		// Magic schools 30-70, 1-3 schools focus
		int32 NumSchools = RandStream.RandRange(1, 3);
		TArray<uint8> ChosenSchools;
		for (int32 i = 0; i < NumSchools; ++i)
		{
			uint8 SchoolIdx;
			do {
				SchoolIdx = static_cast<uint8>(ESkillID::Arcana) + RandStream.RandRange(0, 9);
			} while (ChosenSchools.Contains(SchoolIdx));
			ChosenSchools.Add(SchoolIdx);

			float Level = (i == 0) ? RandRange(40.f, 70.f) : RandRange(20.f, 50.f);
			SetSkillLevel(static_cast<ESkillID>(SchoolIdx), Level);
		}

		// Always have some Arcana
		if (!ChosenSchools.Contains(static_cast<uint8>(ESkillID::Arcana)))
		{
			SetSkillLevel(ESkillID::Arcana, RandRange(20.f, 40.f));
		}

		// Staff for melee
		ESkillID StaffType = RandStream.RandRange(0, 1) == 0 ? ESkillID::Staff1H : ESkillID::Staff2H;
		SetSkillLevel(StaffType, RandRange(15.f, 30.f));

		// Enchanting / Alchemy affinity
		SetSkillLevel(ESkillID::Enchanting, RandRange(20.f, 40.f));
		SetSkillLevel(ESkillID::Alchemy, RandRange(15.f, 35.f));
		SetSkillLevel(ESkillID::Herbalism, RandRange(10.f, 25.f));

		SetAttribute(EAttributeID::INT, RandRange(25.f, 40.f));
		SetAttribute(EAttributeID::WIS, RandRange(20.f, 35.f));
		SetAttribute(EAttributeID::STR, RandRange(8.f, 15.f));
		SetAttribute(EAttributeID::END, RandRange(10.f, 18.f));
		break;
	}
	case EPersonalityArchetype::Hunter:
	{
		// Bow 40-60, hunting/skinning 30-50
		SetSkillLevel(ESkillID::Bow, RandRange(40.f, 60.f));
		SetSkillLevel(ESkillID::Hunting, RandRange(30.f, 50.f));
		SetSkillLevel(ESkillID::Skinning, RandRange(30.f, 50.f));
		SetSkillLevel(ESkillID::Herbalism, RandRange(15.f, 30.f));

		// Melee backup
		ESkillID BackupWeapon = RandStream.RandRange(0, 1) == 0 ? ESkillID::Dagger : ESkillID::Spear;
		SetSkillLevel(BackupWeapon, RandRange(15.f, 30.f));

		// Survival crafts
		SetSkillLevel(ESkillID::Cooking, RandRange(20.f, 35.f));
		SetSkillLevel(ESkillID::Leatherworking, RandRange(15.f, 30.f));
		SetSkillLevel(ESkillID::BowCrafting, RandRange(10.f, 25.f));

		SetAttribute(EAttributeID::DEX, RandRange(22.f, 35.f));
		SetAttribute(EAttributeID::AGI, RandRange(18.f, 30.f));
		SetAttribute(EAttributeID::END, RandRange(18.f, 28.f));
		break;
	}
	case EPersonalityArchetype::Hermit:
	{
		// Gathering 40-60, self-sufficient
		SetSkillLevel(ESkillID::Mining, RandRange(30.f, 55.f));
		SetSkillLevel(ESkillID::Woodcutting, RandRange(30.f, 55.f));
		SetSkillLevel(ESkillID::Fishing, RandRange(35.f, 60.f));
		SetSkillLevel(ESkillID::Herbalism, RandRange(35.f, 55.f));
		SetSkillLevel(ESkillID::Farming, RandRange(25.f, 45.f));
		SetSkillLevel(ESkillID::Cooking, RandRange(30.f, 50.f));

		// Self-defense
		SetSkillLevel(ESkillID::Unarmed, RandRange(10.f, 25.f));

		// Some hermits dabble in magic
		if (RandStream.FRand() > 0.4f)
		{
			ESkillID MagicSchool = RandStream.RandRange(0, 1) == 0 ? ESkillID::Verdancy : ESkillID::Radiance;
			SetSkillLevel(MagicSchool, RandRange(15.f, 35.f));
		}

		// Construction for their dwelling
		SetSkillLevel(ESkillID::Carpentry, RandRange(20.f, 40.f));

		SetAttribute(EAttributeID::WIS, RandRange(20.f, 35.f));
		SetAttribute(EAttributeID::END, RandRange(20.f, 30.f));
		SetAttribute(EAttributeID::INT, RandRange(15.f, 28.f));
		break;
	}
	}

	bCombatPowerDirty = true;

	UE_LOG(LogAoCSkill, Log, TEXT("Skills initialized from archetype %d. CombatPower: %.1f"),
		static_cast<int32>(Archetype), GetCombatPower());
}

int32 UAoCNPCSkillSystem::GetSkillUseCount(const FString& SkillName) const
{
	// Stub: track skill usage in future
	return 0;
}


float UAoCNPCSkillSystem::GetAttributeValue(EAttributeID Attribute) const
{
	// TODO: Implement attribute lookup
	return 10.f;
}
