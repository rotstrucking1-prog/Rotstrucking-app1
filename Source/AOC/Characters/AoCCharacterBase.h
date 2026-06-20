// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "AoCCharacterBase.generated.h"

class UAoCAbilitySystemComponent;
class UAoCSkillComponent;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDied, AAoCCharacterBase*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

// ─────────────────────────────────────────────────────────────────────────────
// Attribute Set
// ─────────────────────────────────────────────────────────────────────────────

/**
 * UAoCBaseAttributeSet
 * Core RPG attributes for all characters in Architect of Creation.
 */
UCLASS()
class AOC_API UAoCBaseAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UAoCBaseAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	// ── Vital Attributes ──

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData Health;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Health);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Health);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Health);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Health);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData MaxHealth;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, MaxHealth);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxHealth);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxHealth);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxHealth);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData Mana;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Mana);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Mana);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Mana);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Mana);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData MaxMana;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, MaxMana);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxMana);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxMana);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxMana);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData Stamina;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Stamina);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Stamina);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Stamina);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Stamina);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "AoC|Attributes|Vital")
	FGameplayAttributeData MaxStamina;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, MaxStamina);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(MaxStamina);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(MaxStamina);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(MaxStamina);

	// ── Core Stats ──

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Strength, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Strength;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Strength);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Strength);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Strength);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Strength);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Dexterity, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Dexterity;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Dexterity);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Dexterity);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Dexterity);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Dexterity);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Intelligence, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Intelligence;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Intelligence);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Intelligence);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Intelligence);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Intelligence);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Vitality, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Vitality;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Vitality);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Vitality);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Vitality);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Vitality);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Quickness, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Quickness;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Quickness);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Quickness);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Quickness);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Quickness);

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Wisdom, Category = "AoC|Attributes|Stats")
	FGameplayAttributeData Wisdom;
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(UAoCBaseAttributeSet, Wisdom);
	GAMEPLAYATTRIBUTE_VALUE_GETTER(Wisdom);
	GAMEPLAYATTRIBUTE_VALUE_SETTER(Wisdom);
	GAMEPLAYATTRIBUTE_VALUE_INITTER(Wisdom);

protected:
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Strength(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Dexterity(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Intelligence(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Vitality(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Quickness(const FGameplayAttributeData& OldValue);
	UFUNCTION() void OnRep_Wisdom(const FGameplayAttributeData& OldValue);
};

// ─────────────────────────────────────────────────────────────────────────────
// Base Character
// ─────────────────────────────────────────────────────────────────────────────

/**
 * AAoCCharacterBase
 * Base character class for all characters in Architect of Creation.
 * Provides GAS integration, skill system, attribute set, and death handling.
 */
UCLASS(Abstract)
class AOC_API AAoCCharacterBase : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AAoCCharacterBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── IAbilitySystemInterface ──
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// ── Components ──

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Components")
	TObjectPtr<UAoCAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Components")
	TObjectPtr<UAoCSkillComponent> SkillComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AoC|Components")
	TObjectPtr<UAoCBaseAttributeSet> AttributeSet;

	// ── Health Helpers ──

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetMana() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetMaxMana() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	float GetMaxStamina() const;

	UFUNCTION(BlueprintCallable, Category = "AoC|Attributes")
	bool IsAlive() const;

	// ── Death ──

	UPROPERTY(ReplicatedUsing = OnRep_bIsDead, BlueprintReadOnly, Category = "AoC|State")
	bool bIsDead;

	UFUNCTION()
	void OnRep_bIsDead();

	UFUNCTION(BlueprintCallable, Category = "AoC|Combat")
	virtual void HandleDeath();

	/** Enable ragdoll physics on the mesh. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Combat")
	void EnableRagdoll();

	UPROPERTY(BlueprintAssignable, Category = "AoC|Combat")
	FOnCharacterDied OnCharacterDied;

	UPROPERTY(BlueprintAssignable, Category = "AoC|Combat")
	FOnHealthChanged OnHealthChanged;

	/** Called when Health attribute changes. Bound to GAS delegate in BeginPlay. */
	void HandleHealthChanged(float NewHealth, float MaxHealth);

protected:
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
};
