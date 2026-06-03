// AoCContextMenu.h
// Architect of Creation - Right-Click Context Menu Manager
// Builds and dispatches context-sensitive menus based on equipped tool + target.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AoCContextMenu.generated.h"

class UUserWidget;

// ─── Enums ───────────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class EContextMenuType : uint8
{
	ShovelOnGround		UMETA(DisplayName = "Shovel On Ground"),
	PickaxeOnGround		UMETA(DisplayName = "Pickaxe On Ground"),
	PickaxeOnRock		UMETA(DisplayName = "Pickaxe On Rock"),
	FurnaceInteract		UMETA(DisplayName = "Furnace Interact"),
	FarmTileInteract	UMETA(DisplayName = "Farm Tile Interact"),
	GenericInteract		UMETA(DisplayName = "Generic Interact")
};

// ─── Structs ─────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct AOC_API FContextMenuOption
{
	GENERATED_BODY()

	/** Unique identifier for this option (e.g. "LowerGround", "SowCrop_Wheat"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	FName OptionID;

	/** Text shown in the menu entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	FString DisplayText;

	/** Optional icon displayed left of the text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu|Assets")
	FSoftObjectPath IconPath;

	/** Whether this option is currently available (greyed out if false). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	bool bEnabled = true;

	/** If true, the option checks the player's skill before allowing execution. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	bool bRequiresSkillCheck = false;

	/** Minimum skill value needed when bRequiresSkillCheck is true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu", meta = (EditCondition = "bRequiresSkillCheck"))
	float RequiredSkill = 0.0f;

	/** Parent option ID for building sub-menu hierarchies.  NAME_None = root level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	FName ParentOptionID = NAME_None;

	/** Sort order within the same parent group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ContextMenu")
	int32 SortOrder = 0;

	FContextMenuOption()
		: OptionID(NAME_None)
		, DisplayText(TEXT(""))
		, bEnabled(true)
		, bRequiresSkillCheck(false)
		, RequiredSkill(0.0f)
		, ParentOptionID(NAME_None)
		, SortOrder(0)
	{}
};

// ─── Delegates ───────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContextMenuOpened, EContextMenuType, MenuType, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnContextMenuClosed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnContextMenuOptionChosen, FName, OptionID, AActor*, TargetActor);

// ─── Component ───────────────────────────────────────────────────────────────

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AOC_API UAoCContextMenuManager : public UActorComponent
{
	GENERATED_BODY()

public:

	UAoCContextMenuManager();

	virtual void BeginPlay() override;

	// ── Public API ──────────────────────────────────────────────────────────

	/**
	 * Show context menu at screen position for the given type and target actor.
	 * @param ScreenPosition  Mouse position in screen-space pixels.
	 * @param MenuType        Determines which options to build.
	 * @param TargetActor     The actor being right-clicked (may be nullptr for ground).
	 */
	UFUNCTION(BlueprintCallable, Category = "ContextMenu")
	void ShowContextMenu(FVector2D ScreenPosition, EContextMenuType MenuType, AActor* TargetActor);

	/** Hide the currently visible context menu. */
	UFUNCTION(BlueprintCallable, Category = "ContextMenu")
	void HideContextMenu();

	/**
	 * Build the options list for a given menu type and target.
	 * Pure data — does not touch UI.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContextMenu")
	TArray<FContextMenuOption> GetMenuOptions(EContextMenuType MenuType, AActor* TargetActor) const;

	/**
	 * Called when the player clicks an option.
	 * Dispatches the action to the correct gameplay system.
	 */
	UFUNCTION(BlueprintCallable, Category = "ContextMenu")
	void OnOptionSelected(FName OptionID);

	/** True if a context menu is currently visible. */
	UFUNCTION(BlueprintPure, Category = "ContextMenu")
	bool IsMenuVisible() const;

	// ── Delegates ───────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "ContextMenu|Events")
	FOnContextMenuOpened OnMenuOpened;

	UPROPERTY(BlueprintAssignable, Category = "ContextMenu|Events")
	FOnContextMenuClosed OnMenuClosed;

	UPROPERTY(BlueprintAssignable, Category = "ContextMenu|Events")
	FOnContextMenuOptionChosen OnOptionChosen;

	// ── Config ──────────────────────────────────────────────────────────────

	/** Widget class to instantiate for the context menu (set in Blueprint). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ContextMenu|Config")
	TSubclassOf<UUserWidget> ContextMenuWidgetClass;

protected:

	/** Cached reference to the currently displayed widget. */
	UPROPERTY()
	UUserWidget* ActiveMenuWidget;

	/** The actor that was right-clicked (cached for dispatch). */
	UPROPERTY()
	AActor* CachedTargetActor;

	/** Current menu type (cached for dispatch). */
	EContextMenuType CachedMenuType;

private:

	// ── Option builders per menu type ───────────────────────────────────────

	TArray<FContextMenuOption> BuildShovelOnGroundOptions(AActor* Target) const;
	TArray<FContextMenuOption> BuildPickaxeOnGroundOptions(AActor* Target) const;
	TArray<FContextMenuOption> BuildPickaxeOnRockOptions(AActor* Target) const;
	TArray<FContextMenuOption> BuildFurnaceOptions(AActor* Target) const;
	TArray<FContextMenuOption> BuildFarmTileOptions(AActor* Target) const;
	TArray<FContextMenuOption> BuildGenericOptions(AActor* Target) const;

	/** Helper — create a leaf option. */
	static FContextMenuOption MakeOption(
		FName InID,
		const FString& InText,
		bool bInEnabled = true,
		bool bSkillCheck = false,
		float Skill = 0.0f,
		const FString& InIconPath = TEXT(""));

	/** Helper — create a parent option and tag sub-items with ParentOptionID. Returns flattened array (parent + children). */
	static TArray<FContextMenuOption> MakeSubmenu(
		FName InID,
		const FString& InText,
		const TArray<FContextMenuOption>& SubItems,
		const FString& InIconPath = TEXT(""));

	// ── Action dispatch ─────────────────────────────────────────────────────

	void DispatchShovelAction(FName OptionID);
	void DispatchPickaxeAction(FName OptionID);
	void DispatchFurnaceAction(FName OptionID);
	void DispatchFarmAction(FName OptionID);
	void DispatchGenericAction(FName OptionID);
};
