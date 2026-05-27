// Copyright Architect of Creation. All Rights Reserved.

#include "AoCCharacterBase.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// ─────────────────────────────────────────────────────────────────────────────
// UAoCBaseAttributeSet
// ─────────────────────────────────────────────────────────────────────────────

UAoCBaseAttributeSet::UAoCBaseAttributeSet()
{
	InitHealth(100.0f);
	InitMaxHealth(100.0f);
	InitMana(50.0f);
	InitMaxMana(50.0f);
	InitStamina(100.0f);
	InitMaxStamina(100.0f);
	InitStrength(1.0f);
	InitDexterity(1.0f);
	InitIntelligence(1.0f);
	InitVitality(1.0f);
	InitQuickness(1.0f);
	InitWisdom(1.0f);
}

void UAoCBaseAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Strength, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Dexterity, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Intelligence, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Vitality, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Quickness, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UAoCBaseAttributeSet, Wisdom, COND_None, REPNOTIFY_Always);
}

void UAoCBaseAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Clamp Health to [0, MaxHealth]
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
	else if (Attribute == GetManaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxMana());
	}
	else if (Attribute == GetStaminaAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxStamina());
	}
}

void UAoCBaseAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	if (Data.EvaluatedData.Attribute == GetHealthAttribute())
	{
		SetHealth(FMath::Clamp(GetHealth(), 0.0f, GetMaxHealth()));

		// Notify owning character of health change
		AAoCCharacterBase* OwnerCharacter = Cast<AAoCCharacterBase>(GetOwningActor());
		if (OwnerCharacter)
		{
			OwnerCharacter->HandleHealthChanged(GetHealth(), GetMaxHealth());
		}
	}
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())
	{
		SetMana(FMath::Clamp(GetMana(), 0.0f, GetMaxMana()));
	}
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())
	{
		SetStamina(FMath::Clamp(GetStamina(), 0.0f, GetMaxStamina()));
	}
}

void UAoCBaseAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Health, OldValue);
}

void UAoCBaseAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, MaxHealth, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Mana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Mana, OldValue);
}

void UAoCBaseAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, MaxMana, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Stamina, OldValue);
}

void UAoCBaseAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, MaxStamina, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Strength(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Strength, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Dexterity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Dexterity, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Intelligence(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Intelligence, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Vitality(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Vitality, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Quickness(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Quickness, OldValue);
}

void UAoCBaseAttributeSet::OnRep_Wisdom(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UAoCBaseAttributeSet, Wisdom, OldValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// AAoCCharacterBase
// ─────────────────────────────────────────────────────────────────────────────

AAoCCharacterBase::AAoCCharacterBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bIsDead = false;

	AbilitySystemComponent = CreateDefaultSubobject<UAoCAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	SkillComponent = CreateDefaultSubobject<UAoCSkillComponent>(TEXT("SkillComponent"));

	AttributeSet = CreateDefaultSubobject<UAoCBaseAttributeSet>(TEXT("AttributeSet"));
}

void AAoCCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAoCCharacterBase, bIsDead);
}

UAbilitySystemComponent* AAoCCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void AAoCCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void AAoCCharacterBase::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Initialize ability actor info on the server
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}
}

float AAoCCharacterBase::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float AAoCCharacterBase::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

float AAoCCharacterBase::GetMana() const
{
	return AttributeSet ? AttributeSet->GetMana() : 0.0f;
}

float AAoCCharacterBase::GetMaxMana() const
{
	return AttributeSet ? AttributeSet->GetMaxMana() : 0.0f;
}

float AAoCCharacterBase::GetStamina() const
{
	return AttributeSet ? AttributeSet->GetStamina() : 0.0f;
}

float AAoCCharacterBase::GetMaxStamina() const
{
	return AttributeSet ? AttributeSet->GetMaxStamina() : 0.0f;
}

bool AAoCCharacterBase::IsAlive() const
{
	return !bIsDead && GetHealth() > 0.0f;
}

void AAoCCharacterBase::OnRep_bIsDead()
{
	if (bIsDead)
	{
		EnableRagdoll();
	}
}

void AAoCCharacterBase::HandleDeath()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// Disable movement and collision
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	EnableRagdoll();

	OnCharacterDied.Broadcast(this);

	// Detach from controller
	DetachFromControllerPendingDestroy();
}

void AAoCCharacterBase::EnableRagdoll()
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		MeshComp->SetSimulatePhysics(true);
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
	}
}

void AAoCCharacterBase::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	OnHealthChanged.Broadcast(NewHealth, MaxHealth);

	if (NewHealth <= 0.0f && !bIsDead && HasAuthority())
	{
		HandleDeath();
	}
}
