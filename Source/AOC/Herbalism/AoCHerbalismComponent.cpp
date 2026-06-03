#include "AoCHerbalismComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

UAoCHerbalismComponent::UAoCHerbalismComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	HerbalismSkill = 0.0f;
	bIsGathering = false;
	CurrentHerbNode = nullptr;
}

void UAoCHerbalismComponent::BeginPlay()
{
	Super::BeginPlay();
	InitHerbDatabase();
}

void UAoCHerbalismComponent::AddHerb(FName ID, const FString& Name, EHerbTier HerbTier, const FString& Desc, const FString& Compound, float Time)
{
	FHerbData Data;
	Data.HerbID = ID;
	Data.DisplayName = Name;
	Data.Tier = HerbTier;
	Data.Description = Desc;
	Data.ActiveCompound = Compound;
	Data.GatherTime = Time;
	Data.bDiscovered = false;
	HerbDatabase.Add(ID, Data);
}

void UAoCHerbalismComponent::InitHerbDatabase()
{
	HerbDatabase.Empty();

	// ===== COMMON (Gather Time: 2s) =====
	AddHerb(FName("Yarrow"),
		TEXT("Yarrow (Achillea millefolium)"), EHerbTier::Common,
		TEXT("A hardy perennial with feathery leaves and white flower clusters. Used to staunch bleeding and reduce fevers."),
		TEXT("Achilleine"), 2.0f);

	AddHerb(FName("Chamomile"),
		TEXT("Chamomile (Matricaria chamomilla)"), EHerbTier::Common,
		TEXT("Small daisy-like flowers with a sweet, apple-like fragrance. Calms nerves and soothes stomach ailments."),
		TEXT("Bisabolol"), 2.0f);

	AddHerb(FName("Dandelion"),
		TEXT("Dandelion (Taraxacum officinale)"), EHerbTier::Common,
		TEXT("A ubiquitous weed with bright yellow flowers. Every part is useful — roots for tonics, leaves for salads."),
		TEXT("Taraxacin"), 2.0f);

	AddHerb(FName("Plantain"),
		TEXT("Plantain (Plantago major)"), EHerbTier::Common,
		TEXT("Broad-leafed ground plant found along paths. Its crushed leaves draw out poisons and soothe insect stings."),
		TEXT("Aucubin"), 2.0f);

	AddHerb(FName("Elderberry"),
		TEXT("Elderberry (Sambucus nigra)"), EHerbTier::Common,
		TEXT("A shrub bearing clusters of dark purple berries. Brewed into syrups for colds and respiratory ailments."),
		TEXT("Anthocyanins"), 2.0f);

	// ===== UNCOMMON (Gather Time: 3.5s) =====
	AddHerb(FName("Valerian"),
		TEXT("Valerian (Valeriana officinalis)"), EHerbTier::Uncommon,
		TEXT("Tall plant with fragrant pink flowers but a pungent root. A powerful sedative and sleep aid."),
		TEXT("Valerenic Acid"), 3.5f);

	AddHerb(FName("Comfrey"),
		TEXT("Comfrey (Symphytum officinale)"), EHerbTier::Uncommon,
		TEXT("Large hairy leaves and bell-shaped flowers. Known as 'knitbone' for its ability to mend fractures."),
		TEXT("Allantoin"), 3.5f);

	AddHerb(FName("Echinacea"),
		TEXT("Echinacea (Echinacea purpurea)"), EHerbTier::Uncommon,
		TEXT("Purple coneflower with a spiky center. Bolsters the body's natural defenses against disease."),
		TEXT("Echinacein"), 3.5f);

	AddHerb(FName("StJohnsWort"),
		TEXT("St. John's Wort (Hypericum perforatum)"), EHerbTier::Uncommon,
		TEXT("Yellow star-shaped flowers that bloom near midsummer. Lifts dark moods and eases melancholy."),
		TEXT("Hypericin"), 3.5f);

	AddHerb(FName("Feverfew"),
		TEXT("Feverfew (Tanacetum parthenium)"), EHerbTier::Uncommon,
		TEXT("Small white daisy-like flowers with a bitter taste. Relieves headaches and reduces inflammation."),
		TEXT("Parthenolide"), 3.5f);

	// ===== RARE (Gather Time: 5s) =====
	AddHerb(FName("Arnica"),
		TEXT("Arnica (Arnica montana)"), EHerbTier::Rare,
		TEXT("Bright yellow mountain flower. Applied as a poultice to bruises and sprains with remarkable effect."),
		TEXT("Helenalin"), 5.0f);

	AddHerb(FName("Goldenseal"),
		TEXT("Goldenseal (Hydrastis canadensis)"), EHerbTier::Rare,
		TEXT("Low-growing plant with a single white flower and golden root. A potent antimicrobial and wound cleanser."),
		TEXT("Berberine"), 5.0f);

	AddHerb(FName("Ginseng"),
		TEXT("Ginseng (Panax ginseng)"), EHerbTier::Rare,
		TEXT("Forked root of legendary restorative power. Grants vigor and sharpens the mind when consumed."),
		TEXT("Ginsenosides"), 5.0f);

	AddHerb(FName("Mandrake"),
		TEXT("Mandrake (Mandragora officinarum)"), EHerbTier::Rare,
		TEXT("A root shaped like a human figure, shrouded in myth. Powerful anesthetic and ingredient in potent elixirs."),
		TEXT("Hyoscyamine"), 5.0f);

	AddHerb(FName("Belladonna"),
		TEXT("Belladonna (Atropa belladonna)"), EHerbTier::Rare,
		TEXT("Deadly nightshade with glossy black berries. Extremely toxic, but in skilled hands, a valuable medicine."),
		TEXT("Atropine"), 5.0f);

	// ===== EPIC (Gather Time: 7s) =====
	AddHerb(FName("Ghostpipe"),
		TEXT("Ghost Pipe (Monotropa uniflora)"), EHerbTier::Epic,
		TEXT("A ghostly white plant devoid of chlorophyll. Grows in deep shadow and is said to ease physical and emotional pain."),
		TEXT("Monotropein"), 7.0f);

	AddHerb(FName("DragonsBlood"),
		TEXT("Dragon's Blood (Daemonorops draco resin)"), EHerbTier::Epic,
		TEXT("Deep crimson resin harvested from rare tropical palms. Used in wound sealing, enchanting inks, and ritual magic."),
		TEXT("Dracoresinotannol"), 7.0f);

	AddHerb(FName("Wormwood"),
		TEXT("Wormwood (Artemisia absinthium)"), EHerbTier::Epic,
		TEXT("Silver-green bitter herb of great potency. Used in anti-parasitic preparations and visionary brews."),
		TEXT("Thujone"), 7.0f);

	AddHerb(FName("BlackCohosh"),
		TEXT("Black Cohosh (Actaea racemosa)"), EHerbTier::Epic,
		TEXT("Tall woodland plant with white candle-like flower spikes. Eases pain and has strong anti-inflammatory properties."),
		TEXT("Actein"), 7.0f);

	AddHerb(FName("AngelicaRoot"),
		TEXT("Angelica Root (Angelica archangelica)"), EHerbTier::Epic,
		TEXT("Stout aromatic plant said to have been revealed by an archangel. A digestive tonic and ward against plague."),
		TEXT("Angelicin"), 7.0f);
}

bool UAoCHerbalismComponent::StartGatherHerb(AActor* HerbNode, FName HerbID)
{
	if (!HerbNode)
	{
		return false;
	}

	if (bIsGathering)
	{
		return false;
	}

	// Check if herb exists in database
	const FHerbData* HerbPtr = HerbDatabase.Find(HerbID);
	if (!HerbPtr)
	{
		return false;
	}

	bIsGathering = true;
	CurrentHerbNode = HerbNode;
	CurrentGatherHerbID = HerbID;

	// Calculate gather time with skill modifier
	const float AdjustedGatherTime = HerbPtr->GatherTime * GetGatherTimeModifier();

	// Start gather timer
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			GatherTimer,
			this,
			&UAoCHerbalismComponent::OnGatherComplete,
			AdjustedGatherTime,
			false
		);
	}

	UE_LOG(LogTemp, Log, TEXT("Herbalism: Started gathering %s (%.1fs)"),
		*HerbPtr->DisplayName, AdjustedGatherTime);

	return true;
}

void UAoCHerbalismComponent::OnGatherComplete()
{
	if (!bIsGathering)
	{
		return;
	}

	// Check if herb was previously undiscovered
	if (!DiscoveredHerbs.Contains(CurrentGatherHerbID))
	{
		DiscoverHerb(CurrentGatherHerbID);
	}

	// Gain skill XP based on herb tier
	float XPGain = 1.0f;
	if (const FHerbData* HerbPtr = HerbDatabase.Find(CurrentGatherHerbID))
	{
		switch (HerbPtr->Tier)
		{
		case EHerbTier::Common:		XPGain = 1.0f; break;
		case EHerbTier::Uncommon:	XPGain = 2.0f; break;
		case EHerbTier::Rare:		XPGain = 4.0f; break;
		case EHerbTier::Epic:		XPGain = 8.0f; break;
		case EHerbTier::Legendary:	XPGain = 16.0f; break;
		}
	}

	HerbalismSkill = FMath::Clamp(HerbalismSkill + XPGain * 0.1f, 0.0f, 100.0f);

	UE_LOG(LogTemp, Log, TEXT("Herbalism: Gathered %s. Skill now: %.1f"),
		*CurrentGatherHerbID.ToString(), HerbalismSkill);

	// Clear gathering state
	bIsGathering = false;
	CurrentHerbNode = nullptr;
	CurrentGatherHerbID = NAME_None;
}

bool UAoCHerbalismComponent::IsHerbDiscovered(FName HerbID) const
{
	return DiscoveredHerbs.Contains(HerbID);
}

void UAoCHerbalismComponent::DiscoverHerb(FName HerbID)
{
	DiscoveredHerbs.Add(HerbID);

	// Mark as discovered in the database
	if (FHerbData* HerbPtr = HerbDatabase.Find(HerbID))
	{
		HerbPtr->bDiscovered = true;
	}

	// Bonus XP for discovery
	HerbalismSkill = FMath::Clamp(HerbalismSkill + 0.5f, 0.0f, 100.0f);

	UE_LOG(LogTemp, Log, TEXT("Herbalism: Discovered new herb — %s!"), *HerbID.ToString());
}

TArray<FHerbData> UAoCHerbalismComponent::GetDiscoveredHerbs() const
{
	TArray<FHerbData> Result;
	for (const FName& HerbID : DiscoveredHerbs)
	{
		if (const FHerbData* HerbPtr = HerbDatabase.Find(HerbID))
		{
			Result.Add(*HerbPtr);
		}
	}
	return Result;
}

float UAoCHerbalismComponent::GetGatherTimeModifier() const
{
	return FMath::Lerp(1.0f, 0.3f, HerbalismSkill / 100.0f);
}
