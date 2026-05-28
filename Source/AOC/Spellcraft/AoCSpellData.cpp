// Source/AOC/Spellcraft/AoCSpellData.cpp
// Auto-generated spell database — 160 spells across 10 schools

#include "AoCSpellData.h"

// ─── Static members ──────────────────────────────────────────────────────────
bool UAoCSpellDatabase::bInitialised = false;
TArray<FAoCSpellInfo> UAoCSpellDatabase::SpellTable;

// ─── Helper ──────────────────────────────────────────────────────────────────
static FAoCSpellInfo MakeSpell(
    FName ID, const FString& Name, const FString& Desc,
    EAoCMagicSchool School, EAoCSpellType Type, int32 Level,
    float Damage, float Mana, float Cast, float CD, float Range,
    float Radius, float Duration, float ProjSpeed, int32 NumProj,
    bool bAOE, bool bChanneled, bool bReqTarget)
{
    FAoCSpellInfo S;
    S.SpellID            = ID;
    S.DisplayName        = Name;
    S.Description        = Desc;
    S.School             = School;
    S.Type               = Type;
    S.SchoolLevel        = Level;
    S.BaseDamage         = Damage;
    S.ManaCost           = Mana;
    S.CastTime           = Cast;
    S.Cooldown           = CD;
    S.Range              = Range;
    S.Radius             = Radius;
    S.Duration           = Duration;
    S.ProjectileSpeed    = ProjSpeed;
    S.NumberOfProjectiles= NumProj;
    S.bIsAOE             = bAOE;
    S.bIsChanneled       = bChanneled;
    S.bRequiresTarget    = bReqTarget;
    return S;
}

// ─── School visuals ──────────────────────────────────────────────────────────
FAoCSchoolVisuals UAoCSpellDatabase::GetSchoolVisuals(EAoCMagicSchool School)
{
    FAoCSchoolVisuals V;
    switch (School)
    {
    case EAoCMagicSchool::Arcana:
        V.PrimaryColor   = FLinearColor(0.6078f, 0.3490f, 1.0000f, 1.f);
        V.SecondaryColor = FLinearColor(1.0000f, 0.8431f, 0.0000f, 1.f);
        V.GlowIntensity  = 3.0f;
        V.ParticleScale  = 1.00f;
        V.ProjectileMeshScale = FVector(0.25f);
        V.ImpactDecalSize = 80.f;
        break;
    case EAoCMagicSchool::Pyromancy:
        V.PrimaryColor   = FLinearColor(1.0000f, 0.2706f, 0.0000f, 1.f);
        V.SecondaryColor = FLinearColor(1.0000f, 0.4000f, 0.0000f, 1.f);
        V.GlowIntensity  = 4.0f;
        V.ParticleScale  = 1.05f;
        V.ProjectileMeshScale = FVector(0.27f);
        V.ImpactDecalSize = 90.f;
        break;
    case EAoCMagicSchool::Cryomancy:
        V.PrimaryColor   = FLinearColor(0.0000f, 0.7490f, 1.0000f, 1.f);
        V.SecondaryColor = FLinearColor(0.8784f, 1.0000f, 1.0000f, 1.f);
        V.GlowIntensity  = 3.5f;
        V.ParticleScale  = 1.10f;
        V.ProjectileMeshScale = FVector(0.29f);
        V.ImpactDecalSize = 100.f;
        break;
    case EAoCMagicSchool::Stormcalling:
        V.PrimaryColor   = FLinearColor(1.0000f, 0.8431f, 0.0000f, 1.f);
        V.SecondaryColor = FLinearColor(0.2549f, 0.4118f, 0.8824f, 1.f);
        V.GlowIntensity  = 4.5f;
        V.ParticleScale  = 1.15f;
        V.ProjectileMeshScale = FVector(0.31f);
        V.ImpactDecalSize = 110.f;
        break;
    case EAoCMagicSchool::Necromancy:
        V.PrimaryColor   = FLinearColor(0.2900f, 0.4900f, 0.2500f, 1.f);
        V.SecondaryColor = FLinearColor(0.1200f, 0.3500f, 0.1000f, 1.f);
        V.GlowIntensity  = 4.0f;
        V.ParticleScale  = 1.20f;
        V.ProjectileMeshScale = FVector(0.33f);
        V.ImpactDecalSize = 120.f;
        break;
    case EAoCMagicSchool::Verdancy:
        V.PrimaryColor   = FLinearColor(0.1961f, 0.8039f, 0.1961f, 1.f);
        V.SecondaryColor = FLinearColor(0.5451f, 0.2706f, 0.0745f, 1.f);
        V.GlowIntensity  = 2.5f;
        V.ParticleScale  = 1.25f;
        V.ProjectileMeshScale = FVector(0.35f);
        V.ImpactDecalSize = 130.f;
        break;
    case EAoCMagicSchool::Umbramancy:
        V.PrimaryColor   = FLinearColor(0.2941f, 0.0000f, 0.5098f, 1.f);
        V.SecondaryColor = FLinearColor(0.1098f, 0.1098f, 0.1098f, 1.f);
        V.GlowIntensity  = 5.0f;
        V.ParticleScale  = 1.30f;
        V.ProjectileMeshScale = FVector(0.37f);
        V.ImpactDecalSize = 140.f;
        break;
    case EAoCMagicSchool::Radiance:
        V.PrimaryColor   = FLinearColor(1.0000f, 1.0000f, 0.9412f, 1.f);
        V.SecondaryColor = FLinearColor(1.0000f, 0.8431f, 0.0000f, 1.f);
        V.GlowIntensity  = 4.0f;
        V.ParticleScale  = 1.35f;
        V.ProjectileMeshScale = FVector(0.39f);
        V.ImpactDecalSize = 150.f;
        break;
    case EAoCMagicSchool::Sangromancy:
        V.PrimaryColor   = FLinearColor(0.5451f, 0.0000f, 0.0000f, 1.f);
        V.SecondaryColor = FLinearColor(0.8627f, 0.0784f, 0.2353f, 1.f);
        V.GlowIntensity  = 4.5f;
        V.ParticleScale  = 1.40f;
        V.ProjectileMeshScale = FVector(0.41f);
        V.ImpactDecalSize = 160.f;
        break;
    case EAoCMagicSchool::Dominion:
        V.PrimaryColor   = FLinearColor(0.7216f, 0.5255f, 0.0431f, 1.f);
        V.SecondaryColor = FLinearColor(0.8039f, 0.5216f, 0.2471f, 1.f);
        V.GlowIntensity  = 3.5f;
        V.ParticleScale  = 1.45f;
        V.ProjectileMeshScale = FVector(0.43f);
        V.ImpactDecalSize = 170.f;
        break;
    default: break;
    }
    return V;
}

// ─── School names ────────────────────────────────────────────────────────────
FString UAoCSpellDatabase::GetSchoolName(EAoCMagicSchool School)
{
    switch (School)
    {
    case EAoCMagicSchool::Arcana: return TEXT("Arcana");
    case EAoCMagicSchool::Pyromancy: return TEXT("Pyromancy");
    case EAoCMagicSchool::Cryomancy: return TEXT("Cryomancy");
    case EAoCMagicSchool::Stormcalling: return TEXT("Stormcalling");
    case EAoCMagicSchool::Necromancy: return TEXT("Necromancy");
    case EAoCMagicSchool::Verdancy: return TEXT("Verdancy");
    case EAoCMagicSchool::Umbramancy: return TEXT("Umbramancy");
    case EAoCMagicSchool::Radiance: return TEXT("Radiance");
    case EAoCMagicSchool::Sangromancy: return TEXT("Sangromancy");
    case EAoCMagicSchool::Dominion: return TEXT("Dominion");
    default: return TEXT("Unknown");
    }
}

// ─── Spell table init ────────────────────────────────────────────────────────
TArray<FAoCSpellInfo>& UAoCSpellDatabase::GetSpellTable()
{
    if (!bInitialised)
    {
        InitialiseSpells();
        bInitialised = true;
    }
    return SpellTable;
}

void UAoCSpellDatabase::InitialiseSpells()
{
    SpellTable.Reserve(160);

    // ── Arcana ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneBolt"), TEXT("Arcane Bolt"),
        TEXT("A bolt of pure arcane energy that strikes the target."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneShield"), TEXT("Arcane Shield"),
        TEXT("Conjures a shimmering barrier of arcane force around the caster."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Shield, 2,
        0.0f, 17.9f, 0.38f, 3.1f, 3133.0f,
        0.0f, 12.3f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ManaDrain"), TEXT("Mana Drain"),
        TEXT("Siphons mana from the target through a beam of violet light."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Beam, 3,
        10.5f, 24.7f, 0.0f, 4.5f, 3267.0f,
        0.0f, 3.7f, 3600.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneExplosion"), TEXT("Arcane Explosion"),
        TEXT("Unleashes a burst of arcane power in all directions."),
        EAoCMagicSchool::Arcana, EAoCSpellType::AOE, 4,
        65.8f, 31.1f, 1.14f, 6.1f, 3400.0f,
        320.0f, 0.0f, 3800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DispelMagic"), TEXT("Dispel Magic"),
        TEXT("Strips magical buffs and effects from the target."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Instant, 5,
        23.6f, 37.4f, 0.0f, 7.7f, 3533.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Counterspell"), TEXT("Counterspell"),
        TEXT("Interrupts the target's current spellcast."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Instant, 6,
        0.0f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneMissiles"), TEXT("Arcane Missiles"),
        TEXT("Launches a volley of three seeking arcane missiles."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Projectile, 7,
        102.6f, 49.5f, 1.82f, 11.3f, 3800.0f,
        0.0f, 0.0f, 4400.0f, 3,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneNova"), TEXT("Arcane Nova"),
        TEXT("A devastating ring of arcane energy expands from the caster."),
        EAoCMagicSchool::Arcana, EAoCSpellType::AOE, 8,
        114.2f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Spellweave"), TEXT("Spellweave"),
        TEXT("Enhances the caster's next spell with additional potency."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Buff, 9,
        0.0f, 61.1f, 1.3f, 15.2f, 4067.0f,
        0.0f, 39.0f, 4800.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ManaShield"), TEXT("Mana Shield"),
        TEXT("Converts incoming damage into mana drain instead of health loss."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Shield, 10,
        0.0f, 66.8f, 1.02f, 17.2f, 4200.0f,
        0.0f, 31.0f, 5000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneLance"), TEXT("Arcane Lance"),
        TEXT("A piercing lance of concentrated arcane force."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Projectile, 11,
        147.5f, 72.5f, 2.38f, 19.2f, 4333.0f,
        0.0f, 0.0f, 5200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_PhaseShift"), TEXT("Phase Shift"),
        TEXT("Briefly shifts the caster between dimensions, becoming untargetable."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Self, 12,
        0.0f, 78.1f, 2.51f, 21.3f, 0.0f,
        0.0f, 23.3f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneStorm"), TEXT("Arcane Storm"),
        TEXT("Rains arcane bolts across a wide area."),
        EAoCMagicSchool::Arcana, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Nullify"), TEXT("Nullify"),
        TEXT("Silences the target, preventing all spellcasting."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Instant, 14,
        35.9f, 89.1f, 0.0f, 25.6f, 4733.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneBarrage"), TEXT("Arcane Barrage"),
        TEXT("An overwhelming salvo of five arcane bolts."),
        EAoCMagicSchool::Arcana, EAoCSpellType::Projectile, 15,
        189.7f, 94.6f, 2.88f, 27.8f, 4867.0f,
        0.0f, 0.0f, 6000.0f, 5,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArcaneApocalypse"), TEXT("Arcane Apocalypse"),
        TEXT("Tears the fabric of reality, devastating everything in a massive area."),
        EAoCMagicSchool::Arcana, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Pyromancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Firebolt"), TEXT("Firebolt"),
        TEXT("Hurls a small bolt of fire at the target."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Ignite"), TEXT("Ignite"),
        TEXT("Sets the target ablaze, dealing fire damage over time."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::DOT, 2,
        11.4f, 17.9f, 0.38f, 3.1f, 3133.0f,
        0.0f, 4.8f, 3400.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FlameBurst"), TEXT("Flame Burst"),
        TEXT("A sudden explosion of flame at the target location."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 3,
        52.5f, 24.7f, 1.03f, 4.5f, 3267.0f,
        280.0f, 0.0f, 3600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Fireball"), TEXT("Fireball"),
        TEXT("Launches a large ball of fire that explodes on impact."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Projectile, 4,
        65.8f, 31.1f, 1.31f, 6.1f, 3400.0f,
        0.0f, 0.0f, 3800.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FireShield"), TEXT("Fire Shield"),
        TEXT("Surrounds the caster in protective flames that burn attackers."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Shield, 5,
        0.0f, 37.4f, 0.62f, 7.7f, 3533.0f,
        0.0f, 19.3f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Combustion"), TEXT("Combustion"),
        TEXT("Detonates all fire effects on the target for massive damage."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Instant, 6,
        90.7f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FlameWave"), TEXT("Flame Wave"),
        TEXT("Sends a sweeping wave of fire across the ground."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 7,
        102.6f, 49.5f, 1.48f, 11.3f, 3800.0f,
        440.0f, 0.0f, 4400.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Pyroclasm"), TEXT("Pyroclasm"),
        TEXT("A volcanic eruption of magma at the target location."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 8,
        114.2f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_MeteorStrike"), TEXT("Meteor Strike"),
        TEXT("Calls down a flaming meteor from the sky."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Projectile, 9,
        125.5f, 61.1f, 2.11f, 15.2f, 4067.0f,
        0.0f, 0.0f, 4800.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_InfernoWall"), TEXT("Inferno Wall"),
        TEXT("Creates a wall of fire that damages enemies passing through."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 10,
        136.6f, 66.8f, 1.82f, 17.2f, 4200.0f,
        560.0f, 0.0f, 5000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LivingFlame"), TEXT("Living Flame"),
        TEXT("Conjures a fire elemental that attacks nearby enemies."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Summon, 11,
        88.5f, 72.5f, 2.83f, 46.7f, 4333.0f,
        0.0f, 45.0f, 5200.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Immolation"), TEXT("Immolation"),
        TEXT("Engulfs the caster in flames, damaging all nearby enemies."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Self, 12,
        158.3f, 78.1f, 2.51f, 21.3f, 0.0f,
        0.0f, 0.0f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FireStorm"), TEXT("Fire Storm"),
        TEXT("Rains fire across a massive area for several seconds."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_PhoenixStrike"), TEXT("Phoenix Strike"),
        TEXT("A phoenix-shaped projectile that revives the caster on kill."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Projectile, 14,
        179.4f, 89.1f, 2.76f, 25.6f, 4733.0f,
        0.0f, 0.0f, 5800.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DragonsBreath"), TEXT("Dragon\'s Breath"),
        TEXT("Channels a continuous cone of dragonfire."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::Channeled, 15,
        47.4f, 94.6f, 0.0f, 27.8f, 4867.0f,
        0.0f, 9.5f, 6000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Hellfire"), TEXT("Hellfire"),
        TEXT("Unleashes the fires of perdition across a devastating area."),
        EAoCMagicSchool::Pyromancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Cryomancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Frostbolt"), TEXT("Frostbolt"),
        TEXT("Launches a shard of ice that slows the target on hit."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FrostArmor"), TEXT("Frost Armor"),
        TEXT("Coats the caster in ice, increasing armor and slowing attackers."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Buff, 2,
        0.0f, 17.9f, 0.6f, 3.1f, 3133.0f,
        0.0f, 18.0f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_IceSpike"), TEXT("Ice Spike"),
        TEXT("A razor-sharp icicle that pierces the target."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Projectile, 3,
        52.5f, 24.7f, 1.11f, 4.5f, 3267.0f,
        0.0f, 0.0f, 3600.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Blizzard"), TEXT("Blizzard"),
        TEXT("Summons a raging blizzard over the target area."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 4,
        65.8f, 31.1f, 1.14f, 6.1f, 3400.0f,
        320.0f, 0.0f, 3800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_GlacialBarrier"), TEXT("Glacial Barrier"),
        TEXT("Erects a wall of ice that blocks projectiles and movement."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Shield, 5,
        0.0f, 37.4f, 0.62f, 7.7f, 3533.0f,
        0.0f, 19.3f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DeepFreeze"), TEXT("Deep Freeze"),
        TEXT("Encases the target in solid ice, stunning them."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Instant, 6,
        45.4f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_IceLance"), TEXT("Ice Lance"),
        TEXT("A swift lance of ice that deals bonus damage to frozen targets."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Projectile, 7,
        102.6f, 49.5f, 1.82f, 11.3f, 3800.0f,
        0.0f, 0.0f, 4400.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FrozenGround"), TEXT("Frozen Ground"),
        TEXT("Freezes the ground, slowing all enemies in the area."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 8,
        45.7f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Avalanche"), TEXT("Avalanche"),
        TEXT("Sends a cascade of ice and snow crashing into the target area."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 9,
        125.5f, 61.1f, 1.71f, 15.2f, 4067.0f,
        520.0f, 0.0f, 4800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_IcePrison"), TEXT("Ice Prison"),
        TEXT("Traps the target in an icy cage."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Instant, 10,
        41.0f, 66.8f, 0.0f, 17.2f, 4200.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_FrostNova"), TEXT("Frost Nova"),
        TEXT("A burst of frost radiates from the caster, freezing nearby enemies."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 11,
        147.5f, 72.5f, 1.93f, 19.2f, 4333.0f,
        600.0f, 0.0f, 5200.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Permafrost"), TEXT("Permafrost"),
        TEXT("Covers the target in ever-thickening ice, dealing escalating damage."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::DOT, 12,
        47.5f, 78.1f, 1.18f, 21.3f, 4467.0f,
        0.0f, 12.8f, 5400.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_IceStorm"), TEXT("Ice Storm"),
        TEXT("A devastating storm of ice shards blankets a wide area."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_GlacialTomb"), TEXT("Glacial Tomb"),
        TEXT("Encases the target in a massive block of ice."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::Instant, 14,
        179.4f, 89.1f, 0.0f, 25.6f, 4733.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_AbsoluteZero"), TEXT("Absolute Zero"),
        TEXT("Drops the temperature to absolute zero, flash-freezing everything."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 15,
        189.7f, 94.6f, 2.39f, 27.8f, 4867.0f,
        760.0f, 0.0f, 6000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_EternalWinter"), TEXT("Eternal Winter"),
        TEXT("Unleashes an endless winter across a vast area."),
        EAoCMagicSchool::Cryomancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Stormcalling ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LightningBolt"), TEXT("Lightning Bolt"),
        TEXT("Hurls a bolt of lightning at the target."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_StaticShield"), TEXT("Static Shield"),
        TEXT("Surrounds the caster in crackling electricity that shocks attackers."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Shield, 2,
        0.0f, 17.9f, 0.38f, 3.1f, 3133.0f,
        0.0f, 12.3f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ChainLightning"), TEXT("Chain Lightning"),
        TEXT("Lightning arcs between multiple nearby enemies."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Instant, 3,
        52.5f, 24.7f, 0.0f, 4.5f, 3267.0f,
        0.0f, 0.0f, 0.0f, 3,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ThunderClap"), TEXT("Thunder Clap"),
        TEXT("A deafening clap of thunder damages and stuns nearby enemies."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::AOE, 4,
        65.8f, 31.1f, 1.14f, 6.1f, 3400.0f,
        320.0f, 0.0f, 3800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_StormBarrier"), TEXT("Storm Barrier"),
        TEXT("A swirling vortex of wind and lightning protects the caster."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Shield, 5,
        0.0f, 37.4f, 0.62f, 7.7f, 3533.0f,
        0.0f, 19.3f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LightningRod"), TEXT("Lightning Rod"),
        TEXT("Marks the target as a lightning rod, repeatedly striking them."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::DOT, 6,
        27.2f, 43.5f, 0.7f, 9.5f, 3667.0f,
        0.0f, 8.0f, 4200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BallLightning"), TEXT("Ball Lightning"),
        TEXT("Launches a ball of lightning that zaps nearby enemies as it travels."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Projectile, 7,
        102.6f, 49.5f, 1.82f, 11.3f, 3800.0f,
        0.0f, 0.0f, 4400.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_TempestStrike"), TEXT("Tempest Strike"),
        TEXT("Calls a bolt of lightning down on the target from the sky."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Instant, 8,
        114.2f, 55.3f, 0.0f, 13.2f, 3933.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ThunderStorm"), TEXT("Thunder Storm"),
        TEXT("Conjures a thunder storm that randomly strikes the area."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::AOE, 9,
        125.5f, 61.1f, 1.71f, 15.2f, 4067.0f,
        520.0f, 0.0f, 4800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LightningSurge"), TEXT("Lightning Surge"),
        TEXT("Channels a surge of electricity into the target."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Channeled, 10,
        34.2f, 66.8f, 0.0f, 17.2f, 4200.0f,
        0.0f, 7.2f, 5000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Overcharge"), TEXT("Overcharge"),
        TEXT("Supercharges the caster, boosting spell damage with lightning."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Buff, 11,
        0.0f, 72.5f, 1.5f, 19.2f, 4333.0f,
        0.0f, 45.0f, 5200.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_EyeoftheStorm"), TEXT("Eye of the Storm"),
        TEXT("Creates a calm eye around the caster while storms rage outside."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Self, 12,
        158.3f, 78.1f, 2.51f, 21.3f, 0.0f,
        0.0f, 0.0f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LightningBarrage"), TEXT("Lightning Barrage"),
        TEXT("Fires a rapid volley of lightning bolts."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Projectile, 13,
        168.9f, 83.6f, 2.64f, 23.4f, 4600.0f,
        0.0f, 0.0f, 5600.0f, 5,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_StormCall"), TEXT("Storm Call"),
        TEXT("Summons a massive storm that devastates a huge area."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::AOE, 14,
        179.4f, 89.1f, 2.27f, 25.6f, 4733.0f,
        720.0f, 0.0f, 5800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DivineThunder"), TEXT("Divine Thunder"),
        TEXT("A single, impossibly powerful thunderbolt from the heavens."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::Instant, 15,
        189.7f, 94.6f, 0.0f, 27.8f, 4867.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Cataclysm"), TEXT("Cataclysm"),
        TEXT("Unleashes the full fury of the storm upon the battlefield."),
        EAoCMagicSchool::Stormcalling, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Necromancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DeathCoil"), TEXT("Death Coil"),
        TEXT("Hurls a bolt of necrotic energy that damages the living and heals undead minions."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_NecroticStrike"), TEXT("Necrotic Strike"),
        TEXT("Infuses the next melee strike with death energy, applying a healing absorption debuff."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Instant, 2,
        11.4f, 17.9f, 0.0f, 3.1f, 3133.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BoneArmor"), TEXT("Bone Armor"),
        TEXT("Encases the caster in whirling bone fragments that absorb damage."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Shield, 3,
        0.0f, 24.7f, 0.6f, 4.5f, 3267.0f,
        0.0f, 18.0f, 3600.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SoulDrain"), TEXT("Soul Drain"),
        TEXT("Channels a beam that siphons the target's life force, healing the caster."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Beam, 4,
        19.7f, 31.1f, 0.0f, 6.1f, 3400.0f,
        0.0f, 4.4f, 3800.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_RaiseDead"), TEXT("Raise Dead"),
        TEXT("Animates a nearby corpse to fight as a skeletal warrior for the caster."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Summon, 5,
        0.0f, 37.4f, 1.5f, 7.7f, 3533.0f,
        0.0f, 45.0f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DeathGrip"), TEXT("Death Grip"),
        TEXT("A spectral hand reaches out and yanks the target toward the caster."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Instant, 6,
        27.2f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Plague"), TEXT("Plague"),
        TEXT("Unleashes a creeping pestilence that spreads between nearby enemies."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::DOT, 7,
        30.4f, 49.5f, 0.7f, 11.3f, 3800.0f,
        0.0f, 8.0f, 4400.0f, 3,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SummonGhoul"), TEXT("Summon Ghoul"),
        TEXT("Summons a ravenous ghoul from the earth that attacks with vicious claws."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Summon, 8,
        68.5f, 55.3f, 2.43f, 38.7f, 3933.0f,
        0.0f, 36.0f, 4600.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_CorpseExplosion"), TEXT("Corpse Explosion"),
        TEXT("Detonates a nearby corpse in a shower of gore and bone, dealing massive AOE damage."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::AOE, 9,
        125.5f, 61.1f, 1.71f, 15.2f, 4067.0f,
        520.0f, 0.0f, 4800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_AntiMagicShell"), TEXT("Anti-Magic Shell"),
        TEXT("Wraps the caster in a shell that absorbs incoming magical damage."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Shield, 10,
        0.0f, 66.8f, 0.62f, 17.2f, 4200.0f,
        0.0f, 19.3f, 5000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DeathAndDecay"), TEXT("Death and Decay"),
        TEXT("Corrupts the ground in a large area, dealing sustained necrotic damage to all within."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::AOE, 11,
        44.3f, 72.5f, 1.93f, 19.2f, 4333.0f,
        600.0f, 12.0f, 5200.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SoulHarvest"), TEXT("Soul Harvest"),
        TEXT("Rapidly extracts souls from multiple nearby enemies, empowering the caster."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Channeled, 12,
        47.5f, 78.1f, 0.0f, 21.3f, 4467.0f,
        0.0f, 6.7f, 5400.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_UnholyFrenzy"), TEXT("Unholy Frenzy"),
        TEXT("Sacrifices health to enter a state of unholy frenzy, massively boosting attack speed and damage."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Self, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 0.0f,
        0.0f, 0.0f, 5600.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DarkTransformation"), TEXT("Dark Transformation"),
        TEXT("Transforms an undead minion into a monstrous abomination with devastating power."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Instant, 14,
        107.6f, 89.1f, 0.0f, 54.7f, 4733.0f,
        0.0f, 54.0f, 5800.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ArmyOfTheDead"), TEXT("Army of the Dead"),
        TEXT("Raises an army of skeletal warriors from the ground to fight for the caster."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::Summon, 15,
        0.0f, 94.6f, 3.0f, 27.8f, 4867.0f,
        0.0f, 60.0f, 6000.0f, 5,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Apocalypse"), TEXT("Apocalypse"),
        TEXT("Unleashes a cataclysmic wave of death energy, devastating everything in a massive area and raising fallen enemies as temporary minions."),
        EAoCMagicSchool::Necromancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Verdancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_VineWhip"), TEXT("Vine Whip"),
        TEXT("Lashes the target with a thorny vine."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BarkSkin"), TEXT("Bark Skin"),
        TEXT("Hardens the caster's skin like bark, reducing damage taken."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Buff, 2,
        0.0f, 17.9f, 0.6f, 3.1f, 3133.0f,
        0.0f, 18.0f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Entangle"), TEXT("Entangle"),
        TEXT("Roots spring from the ground, immobilizing enemies in the area."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 3,
        10.5f, 24.7f, 1.03f, 4.5f, 3267.0f,
        280.0f, 0.0f, 3600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ThornSpray"), TEXT("Thorn Spray"),
        TEXT("Fires a spray of poisoned thorns at the target."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Projectile, 4,
        65.8f, 31.1f, 1.31f, 6.1f, 3400.0f,
        0.0f, 0.0f, 3800.0f, 3,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_NaturesEmbrace"), TEXT("Nature\'s Embrace"),
        TEXT("The caster is healed by nature's energy over time."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Self, 5,
        62.8f, 37.4f, 1.49f, 7.7f, 0.0f,
        0.0f, 0.0f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_PoisonCloud"), TEXT("Poison Cloud"),
        TEXT("Releases a cloud of toxic spores that poison enemies."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 6,
        90.7f, 43.5f, 1.37f, 9.5f, 3667.0f,
        400.0f, 0.0f, 4200.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_RootSlam"), TEXT("Root Slam"),
        TEXT("Massive roots erupt beneath the target, launching them skyward."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Instant, 7,
        102.6f, 49.5f, 0.0f, 11.3f, 3800.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LivingRoots"), TEXT("Living Roots"),
        TEXT("Summons animated roots that attack nearby enemies."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Summon, 8,
        68.5f, 55.3f, 2.43f, 38.7f, 3933.0f,
        0.0f, 36.0f, 4600.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_NaturesWrath"), TEXT("Nature\'s Wrath"),
        TEXT("Nature itself revolts, damaging all enemies in a wide area."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 9,
        125.5f, 61.1f, 1.71f, 15.2f, 4067.0f,
        520.0f, 0.0f, 4800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Overgrowth"), TEXT("Overgrowth"),
        TEXT("Rapid plant growth overtakes the area, entangling and damaging."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 10,
        136.6f, 66.8f, 1.82f, 17.2f, 4200.0f,
        560.0f, 0.0f, 5000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SporeBurst"), TEXT("Spore Burst"),
        TEXT("Infects the target with parasitic spores that deal damage over time."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::DOT, 11,
        44.3f, 72.5f, 1.1f, 19.2f, 4333.0f,
        0.0f, 12.0f, 5200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_WildGrowth"), TEXT("Wild Growth"),
        TEXT("Rapidly regenerates the caster's health with nature's vitality."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Self, 12,
        126.6f, 78.1f, 2.51f, 21.3f, 0.0f,
        0.0f, 0.0f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_VerdantStorm"), TEXT("Verdant Storm"),
        TEXT("A storm of leaves, thorns, and branches ravages the area."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Stranglehold"), TEXT("Stranglehold"),
        TEXT("Vines constrict the target, dealing crushing damage over time."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::Channeled, 14,
        44.8f, 89.1f, 0.0f, 25.6f, 4733.0f,
        0.0f, 9.1f, 5800.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_WorldTreesFury"), TEXT("World Tree\'s Fury"),
        TEXT("Channels the ancient power of the World Tree to devastate foes."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 15,
        189.7f, 94.6f, 2.39f, 27.8f, 4867.0f,
        760.0f, 0.0f, 6000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_GaiasJudgment"), TEXT("Gaia\'s Judgment"),
        TEXT("Nature passes judgment, unleashing catastrophic verdant fury."),
        EAoCMagicSchool::Verdancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Umbramancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ShadowBolt"), TEXT("Shadow Bolt"),
        TEXT("Fires a bolt of condensed shadow energy."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ShadowCloak"), TEXT("Shadow Cloak"),
        TEXT("Wraps the caster in shadows, increasing evasion."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Buff, 2,
        0.0f, 17.9f, 0.6f, 3.1f, 3133.0f,
        0.0f, 18.0f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Darkness"), TEXT("Darkness"),
        TEXT("Plunges the area into magical darkness, blinding enemies."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::AOE, 3,
        10.5f, 24.7f, 1.03f, 4.5f, 3267.0f,
        280.0f, 0.0f, 3600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Fear"), TEXT("Fear"),
        TEXT("Strikes terror into the target, causing them to flee."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Instant, 4,
        0.0f, 31.1f, 0.0f, 6.1f, 3400.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SoulDrain"), TEXT("Soul Drain"),
        TEXT("Drains the target's life force through a beam of shadow."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Beam, 5,
        15.7f, 37.4f, 0.0f, 7.7f, 3533.0f,
        0.0f, 4.3f, 4000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Nightmare"), TEXT("Nightmare"),
        TEXT("Afflicts the target with horrifying visions that deal damage."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::DOT, 6,
        27.2f, 43.5f, 0.7f, 9.5f, 3667.0f,
        0.0f, 8.0f, 4200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ShadowStep"), TEXT("Shadow Step"),
        TEXT("Teleports the caster through the shadow realm to a new location."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Self, 7,
        0.0f, 49.5f, 1.82f, 11.3f, 0.0f,
        0.0f, 15.0f, 4400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_VoidRift"), TEXT("Void Rift"),
        TEXT("Tears open a rift to the void that damages all nearby."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::AOE, 8,
        114.2f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ShadowNova"), TEXT("Shadow Nova"),
        TEXT("A blast of shadow energy erupts from the caster."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::AOE, 9,
        125.5f, 61.1f, 1.71f, 15.2f, 4067.0f,
        520.0f, 0.0f, 4800.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_UmbralChains"), TEXT("Umbral Chains"),
        TEXT("Shadowy chains bind the target in place."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Instant, 10,
        54.6f, 66.8f, 0.0f, 17.2f, 4200.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DreadAura"), TEXT("Dread Aura"),
        TEXT("Emanates an aura of dread, weakening nearby enemies."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Self, 11,
        147.5f, 72.5f, 2.38f, 19.2f, 0.0f,
        0.0f, 0.0f, 5200.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_VoidWalker"), TEXT("Void Walker"),
        TEXT("Partially phases into the void, reducing damage taken."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Buff, 12,
        0.0f, 78.1f, 1.6f, 21.3f, 4467.0f,
        0.0f, 48.0f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ShadowStorm"), TEXT("Shadow Storm"),
        TEXT("A maelstrom of shadow energy engulfs the area."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_AbyssalGate"), TEXT("Abyssal Gate"),
        TEXT("Opens a gate to the abyss, summoning shadow creatures."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::Summon, 14,
        107.6f, 89.1f, 3.23f, 54.7f, 4733.0f,
        0.0f, 54.0f, 5800.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Doom"), TEXT("Doom"),
        TEXT("Places a mark of doom on the target — after a delay, massive damage."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::DOT, 15,
        56.9f, 94.6f, 1.42f, 27.8f, 4867.0f,
        0.0f, 15.2f, 6000.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Oblivion"), TEXT("Oblivion"),
        TEXT("Erases a section of reality, dealing catastrophic void damage."),
        EAoCMagicSchool::Umbramancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Radiance ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_HolyBolt"), TEXT("Holy Bolt"),
        TEXT("A bolt of sacred light that damages undead and heals allies."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DivineShield"), TEXT("Divine Shield"),
        TEXT("An impenetrable shield of holy light protects the caster."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Shield, 2,
        0.0f, 17.9f, 0.38f, 3.1f, 3133.0f,
        0.0f, 12.3f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Heal"), TEXT("Heal"),
        TEXT("Restores health to the target with divine energy."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 3,
        42.0f, 24.7f, 0.0f, 4.5f, 3267.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Smite"), TEXT("Smite"),
        TEXT("Strikes the target with holy wrath."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 4,
        65.8f, 31.1f, 0.0f, 6.1f, 3400.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BlessingofLight"), TEXT("Blessing of Light"),
        TEXT("Blesses an ally, increasing their damage and defense."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Buff, 5,
        0.0f, 37.4f, 0.9f, 7.7f, 3533.0f,
        0.0f, 27.0f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Purify"), TEXT("Purify"),
        TEXT("Removes all harmful effects from the target."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 6,
        72.6f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_HolyNova"), TEXT("Holy Nova"),
        TEXT("A burst of light damages enemies and heals allies."),
        EAoCMagicSchool::Radiance, EAoCSpellType::AOE, 7,
        102.6f, 49.5f, 1.48f, 11.3f, 3800.0f,
        440.0f, 0.0f, 4400.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Resurrection"), TEXT("Resurrection"),
        TEXT("Brings a fallen ally back to life."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 8,
        91.3f, 55.3f, 0.0f, 13.2f, 3933.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DivineJudgment"), TEXT("Divine Judgment"),
        TEXT("Passes divine judgment on the target, dealing massive holy damage."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 9,
        125.5f, 61.1f, 0.0f, 15.2f, 4067.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Sanctuary"), TEXT("Sanctuary"),
        TEXT("Creates a zone of healing light that restores health over time."),
        EAoCMagicSchool::Radiance, EAoCSpellType::AOE, 10,
        109.3f, 66.8f, 1.82f, 17.2f, 4200.0f,
        560.0f, 0.0f, 5000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_HolyFire"), TEXT("Holy Fire"),
        TEXT("Sacred flames burn the target over time."),
        EAoCMagicSchool::Radiance, EAoCSpellType::DOT, 11,
        44.3f, 72.5f, 1.1f, 19.2f, 4333.0f,
        0.0f, 12.0f, 5200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_AuraofLight"), TEXT("Aura of Light"),
        TEXT("Emanates healing light, slowly restoring health to nearby allies."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Self, 12,
        126.6f, 78.1f, 2.51f, 21.3f, 0.0f,
        0.0f, 0.0f, 5400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_CelestialStorm"), TEXT("Celestial Storm"),
        TEXT("Calls down a storm of divine light across the battlefield."),
        EAoCMagicSchool::Radiance, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DivineIntervention"), TEXT("Divine Intervention"),
        TEXT("Instantly shields an ally from a killing blow."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Instant, 14,
        143.5f, 89.1f, 0.0f, 25.6f, 4733.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_WrathofLight"), TEXT("Wrath of Light"),
        TEXT("Channels the wrath of the heavens across a massive area."),
        EAoCMagicSchool::Radiance, EAoCSpellType::AOE, 15,
        189.7f, 94.6f, 2.39f, 27.8f, 4867.0f,
        760.0f, 0.0f, 6000.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Ascension"), TEXT("Ascension"),
        TEXT("Transcends mortal form, massively boosting all attributes."),
        EAoCMagicSchool::Radiance, EAoCSpellType::Self, 16,
        200.0f, 100.0f, 3.0f, 30.0f, 0.0f,
        0.0f, 0.0f, 6200.0f, 1,
        false, false, false));

    // ── Sangromancy ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodBolt"), TEXT("Blood Bolt"),
        TEXT("Fires a bolt of crystallized blood at the target."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Projectile, 1,
        20.0f, 10.0f, 0.5f, 2.0f, 3000.0f,
        0.0f, 0.0f, 3200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodShield"), TEXT("Blood Shield"),
        TEXT("Forms a barrier of hardened blood around the caster."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Shield, 2,
        0.0f, 17.9f, 0.38f, 3.1f, 3133.0f,
        0.0f, 12.3f, 3400.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_LifeTap"), TEXT("Life Tap"),
        TEXT("Sacrifices health to restore mana."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Self, 3,
        0.0f, 24.7f, 1.11f, 4.5f, 0.0f,
        0.0f, 8.3f, 3600.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodBoil"), TEXT("Blood Boil"),
        TEXT("Causes the target's blood to boil, dealing damage over time."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::DOT, 4,
        19.7f, 31.1f, 0.54f, 6.1f, 3400.0f,
        0.0f, 6.4f, 3800.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SanguineLink"), TEXT("Sanguine Link"),
        TEXT("Links the caster to the target, draining their life force."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Beam, 5,
        15.7f, 37.4f, 0.0f, 7.7f, 3533.0f,
        0.0f, 4.3f, 4000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Hemophilia"), TEXT("Hemophilia"),
        TEXT("Prevents the target's wounds from healing."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::DOT, 6,
        27.2f, 43.5f, 0.7f, 9.5f, 3667.0f,
        0.0f, 8.0f, 4200.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodLance"), TEXT("Blood Lance"),
        TEXT("A spear of solidified blood pierces through enemies."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Projectile, 7,
        102.6f, 49.5f, 1.82f, 11.3f, 3800.0f,
        0.0f, 0.0f, 4400.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_CrimsonTide"), TEXT("Crimson Tide"),
        TEXT("A wave of blood washes over the area, damaging all enemies."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::AOE, 8,
        114.2f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodRitual"), TEXT("Blood Ritual"),
        TEXT("Performs a blood ritual to dramatically boost spell power."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Self, 9,
        125.5f, 61.1f, 2.11f, 15.2f, 0.0f,
        0.0f, 0.0f, 4800.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Exsanguinate"), TEXT("Exsanguinate"),
        TEXT("Rapidly drains the target's blood through dark magic."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Channeled, 10,
        34.2f, 66.8f, 0.0f, 17.2f, 4200.0f,
        0.0f, 7.2f, 5000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodPact"), TEXT("Blood Pact"),
        TEXT("Forges a pact: share damage with an ally to reduce it."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Buff, 11,
        0.0f, 72.5f, 1.5f, 19.2f, 4333.0f,
        0.0f, 45.0f, 5200.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Transfusion"), TEXT("Transfusion"),
        TEXT("Drains life from an enemy and transfers it to an ally."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Beam, 12,
        31.7f, 78.1f, 0.0f, 21.3f, 4467.0f,
        0.0f, 6.7f, 5400.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodStorm"), TEXT("Blood Storm"),
        TEXT("A storm of blood rains from the sky, devastating the area."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_VampiricEmbrace"), TEXT("Vampiric Embrace"),
        TEXT("All damage dealt heals the caster for a percentage."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Self, 14,
        179.4f, 89.1f, 2.76f, 25.6f, 0.0f,
        0.0f, 0.0f, 5800.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_BloodSacrifice"), TEXT("Blood Sacrifice"),
        TEXT("Sacrifices a portion of health for an immense damage burst."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::Instant, 15,
        189.7f, 94.6f, 0.0f, 27.8f, 4867.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_SanguineApocalypse"), TEXT("Sanguine Apocalypse"),
        TEXT("The blood of fallen enemies erupts in a cataclysmic explosion."),
        EAoCMagicSchool::Sangromancy, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

    // ── Dominion ─────────────────────────────────────────
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ForcePush"), TEXT("Force Push"),
        TEXT("Telekinetically pushes the target backward with great force."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 1,
        20.0f, 10.0f, 0.0f, 2.0f, 3000.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Command"), TEXT("Command"),
        TEXT("Compels the target to perform a single action."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 2,
        0.0f, 17.9f, 0.0f, 3.1f, 3133.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Compel"), TEXT("Compel"),
        TEXT("Forces the target to attack their nearest ally."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 3,
        0.0f, 24.7f, 0.0f, 4.5f, 3267.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_DominateMind"), TEXT("Dominate Mind"),
        TEXT("Takes control of the target's mind for the channel duration."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Channeled, 4,
        0.0f, 31.1f, 0.0f, 6.1f, 3400.0f,
        0.0f, 4.4f, 3800.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_MindShield"), TEXT("Mind Shield"),
        TEXT("A psychic barrier that protects against mental attacks."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Shield, 5,
        0.0f, 37.4f, 0.62f, 7.7f, 3533.0f,
        0.0f, 19.3f, 4000.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Subjugate"), TEXT("Subjugate"),
        TEXT("Overwhelms the target's will, stunning them."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 6,
        27.2f, 43.5f, 0.0f, 9.5f, 3667.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_ForceWave"), TEXT("Force Wave"),
        TEXT("A telekinetic wave pushes all nearby enemies away."),
        EAoCMagicSchool::Dominion, EAoCSpellType::AOE, 7,
        102.6f, 49.5f, 1.48f, 11.3f, 3800.0f,
        440.0f, 0.0f, 4400.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_MassCommand"), TEXT("Mass Command"),
        TEXT("Commands all enemies in the area to stop in their tracks."),
        EAoCMagicSchool::Dominion, EAoCSpellType::AOE, 8,
        22.8f, 55.3f, 1.59f, 13.2f, 3933.0f,
        480.0f, 0.0f, 4600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Telekinesis"), TEXT("Telekinesis"),
        TEXT("Lifts and hurls objects or enemies with the power of the mind."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Channeled, 9,
        31.4f, 61.1f, 0.0f, 15.2f, 4067.0f,
        0.0f, 6.7f, 4800.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_MindCrush"), TEXT("Mind Crush"),
        TEXT("Crushes the target's mind, dealing massive psychic damage."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 10,
        136.6f, 66.8f, 0.0f, 17.2f, 4200.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_Thrall"), TEXT("Thrall"),
        TEXT("Dominates an enemy, turning them into a temporary ally."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Summon, 11,
        0.0f, 72.5f, 2.83f, 46.7f, 4333.0f,
        0.0f, 45.0f, 5200.0f, 1,
        false, false, false));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_MassDomination"), TEXT("Mass Domination"),
        TEXT("Attempts to dominate the minds of all enemies in the area."),
        EAoCMagicSchool::Dominion, EAoCSpellType::AOE, 12,
        47.5f, 78.1f, 2.05f, 21.3f, 4467.0f,
        640.0f, 0.0f, 5400.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_PsychicStorm"), TEXT("Psychic Storm"),
        TEXT("A storm of psychic energy ravages the minds of all in the area."),
        EAoCMagicSchool::Dominion, EAoCSpellType::AOE, 13,
        168.9f, 83.6f, 2.16f, 23.4f, 4600.0f,
        680.0f, 0.0f, 5600.0f, 1,
        true, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_WillBreak"), TEXT("Will Break"),
        TEXT("Shatters the target's will, leaving them vulnerable."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Instant, 14,
        179.4f, 89.1f, 0.0f, 25.6f, 4733.0f,
        0.0f, 0.0f, 0.0f, 1,
        false, false, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_AbsoluteControl"), TEXT("Absolute Control"),
        TEXT("Completely controls the target for an extended duration."),
        EAoCMagicSchool::Dominion, EAoCSpellType::Channeled, 15,
        0.0f, 94.6f, 0.0f, 27.8f, 4867.0f,
        0.0f, 9.5f, 6000.0f, 1,
        false, true, true));
    SpellTable.Add(MakeSpell(
        TEXT("Spell_GrandDominion"), TEXT("Grand Dominion"),
        TEXT("Asserts absolute psychic dominion over the entire battlefield."),
        EAoCMagicSchool::Dominion, EAoCSpellType::AOE, 16,
        200.0f, 100.0f, 2.5f, 30.0f, 5000.0f,
        800.0f, 0.0f, 6200.0f, 1,
        true, false, true));

}

// ─── Public accessors ────────────────────────────────────────────────────────
TArray<FAoCSpellInfo> UAoCSpellDatabase::GetAllSpells()
{
    return GetSpellTable();
}

TArray<FAoCSpellInfo> UAoCSpellDatabase::GetSpellsBySchool(EAoCMagicSchool School)
{
    TArray<FAoCSpellInfo> Result;
    for (const FAoCSpellInfo& S : GetSpellTable())
    {
        if (S.School == School)
        {
            Result.Add(S);
        }
    }
    return Result;
}

FAoCSpellInfo* UAoCSpellDatabase::FindSpell(FName SpellID)
{
    for (FAoCSpellInfo& S : GetSpellTable())
    {
        if (S.SpellID == SpellID)
        {
            return &S;
        }
    }
    return nullptr;
}
