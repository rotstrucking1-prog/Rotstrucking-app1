// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AoCSpellBarWidget.generated.h"

class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UImage;

/**
 * FAoCSpellSlotWidgetData
 * Display data for a single spell slot in the spell bar.
 */
USTRUCT(BlueprintType)
struct FAoCSpellSlotWidgetData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|UI|SpellBar")
	FName SpellID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|UI|SpellBar")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AoC|UI|SpellBar")
	FString KeybindLabel;

	UPROPERTY(BlueprintReadWrite, Category = "AoC|UI|SpellBar")
	float CooldownRemaining;

	UPROPERTY(BlueprintReadWrite, Category = "AoC|UI|SpellBar")
	float CooldownTotal;

	FAoCSpellSlotWidgetData()
		: SpellID(NAME_None)
		, CooldownRemaining(0.0f)
		, CooldownTotal(0.0f)
	{}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSpellSlotClicked, int32, SlotIndex);

/**
 * UAoCSpellBarWidget
 * Displays 10 spell slots in a horizontal bar with cooldown overlays and a cast bar.
 */
UCLASS()
class ARCHITECTOFCREATION_API UAoCSpellBarWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAoCSpellBarWidget(const FObjectInitializer& ObjectInitializer);

	/** Number of spell slots. */
	static constexpr int32 SpellSlotCount = 10;

	// ── Bound Widgets ──

	/** Horizontal box containing the spell slot child widgets. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> SpellSlotContainer;

	/** Global cooldown overlay bar. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> GlobalCooldownBar;

	/** Cast bar progress. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> CastBar;

	/** Cast bar spell name text. */
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> CastBarSpellName;

	// ── API ──

	/** Set spell data for a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void SetSpell(int32 SlotIndex, FName SpellID, UTexture2D* Icon, const FString& KeybindLabel);

	/** Clear a spell slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void ClearSpell(int32 SlotIndex);

	/** Update the cooldown visual for a specific slot. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void UpdateCooldown(int32 SlotIndex, float RemainingTime, float TotalCooldown);

	/** Update the global cooldown bar. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void UpdateGlobalCooldown(float RemainingTime, float TotalCooldown);

	/** Update the cast bar progress. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void UpdateCastBar(const FString& SpellName, float ElapsedTime, float TotalCastTime);

	/** Hide the cast bar. */
	UFUNCTION(BlueprintCallable, Category = "AoC|UI|SpellBar")
	void HideCastBar();

	// ── Delegate ──

	UPROPERTY(BlueprintAssignable, Category = "AoC|UI|SpellBar")
	FOnSpellSlotClicked OnSpellSlotClicked;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	/** Cached slot data for display. */
	TArray<FAoCSpellSlotWidgetData> SlotData;
};
