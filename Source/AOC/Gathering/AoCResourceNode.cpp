#include "AoCResourceNode.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "Engine/World.h"

AAoCResourceNode::AAoCResourceNode()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create root mesh component
	NodeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NodeMesh"));
	RootComponent = NodeMesh;

	// Glow mesh created on demand for mega nodes
	GlowMesh = nullptr;
	GlowMaterialInstance = nullptr;

	// Defaults
	NodeType = EResourceNodeType::OreVein;
	NodeSize = EResourceNodeSize::Medium;
	Tier = EResourceTier::Tier1;
	ResourceID = FName("Iron");
	DisplayName = TEXT("Iron Ore Vein");
	MaxHealth = 10;
	CurrentHealth = 10;
	YieldPerHit = 10;
	RespawnTime = 300.0f;
	bIsDepleted = false;
	bIsMegaNode = false;
}

void AAoCResourceNode::BeginPlay()
{
	Super::BeginPlay();
	InitializeNode();
}

void AAoCResourceNode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Pulse glow for mega nodes
	if (bIsMegaNode && GlowMesh && GlowMaterialInstance && !bIsDepleted)
	{
		const float Intensity = GetPulseIntensity();
		GlowMaterialInstance->SetScalarParameterValue(FName("EmissiveStrength"), Intensity);
	}
}

void AAoCResourceNode::InitializeNode()
{
	switch (NodeSize)
	{
	case EResourceNodeSize::Small:
		MaxHealth = 5;
		YieldPerHit = 10;
		bIsMegaNode = false;
		break;
	case EResourceNodeSize::Medium:
		MaxHealth = 10;
		YieldPerHit = 15;
		bIsMegaNode = false;
		break;
	case EResourceNodeSize::Large:
		MaxHealth = 20;
		YieldPerHit = 25;
		bIsMegaNode = false;
		break;
	case EResourceNodeSize::Mega:
		MaxHealth = 50;
		YieldPerHit = 50;
		bIsMegaNode = true;
		RespawnTime = 0.0f; // Mega nodes do not respawn
		break;
	}

	CurrentHealth = MaxHealth;
	bIsDepleted = false;

	if (bIsMegaNode)
	{
		SetupMegaNodeVisuals();
	}
}

FResourceYield AAoCResourceNode::HarvestHit(float SkillLevel)
{
	FResourceYield Yield;
	Yield.ResourceID = ResourceID;
	Yield.Tier = Tier;

	if (bIsDepleted)
	{
		Yield.MinYield = 0;
		Yield.MaxYield = 0;
		return Yield;
	}

	// Calculate yield based on skill level
	const float SkillMultiplier = FMath::Lerp(1.0f, 2.5f, FMath::Clamp(SkillLevel / 100.0f, 0.0f, 1.0f));
	const float BaseYieldMin = YieldPerHit * 0.8f;
	const float BaseYieldMax = YieldPerHit * 1.2f;
	const float RandomYield = FMath::RandRange(BaseYieldMin, BaseYieldMax);
	const int32 FinalYield = FMath::RoundToInt32(RandomYield * SkillMultiplier);

	Yield.MinYield = FinalYield;
	Yield.MaxYield = FinalYield;

	// Reduce health
	CurrentHealth--;

	// Broadcast hit event
	OnNodeHit.Broadcast(this, CurrentHealth);

	// Check for depletion
	if (CurrentHealth <= 0)
	{
		OnDepleted();
	}

	return Yield;
}

bool AAoCResourceNode::IsDepleted() const
{
	return bIsDepleted;
}

void AAoCResourceNode::OnDepleted()
{
	bIsDepleted = true;

	// Visual feedback — shrink the node
	if (NodeMesh)
	{
		NodeMesh->SetWorldScale3D(FVector(0.1f));
	}

	// Broadcast depletion event
	OnNodeDepleted.Broadcast(this);

	// Start respawn timer if not a mega node
	if (!bIsMegaNode && RespawnTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RespawnTimerHandle,
				this,
				&AAoCResourceNode::Respawn,
				RespawnTime,
				false
			);
		}
	}
}

void AAoCResourceNode::Respawn()
{
	CurrentHealth = MaxHealth;
	bIsDepleted = false;

	// Reset visuals
	if (NodeMesh)
	{
		NodeMesh->SetWorldScale3D(FVector(1.0f));
	}
}

void AAoCResourceNode::SetupMegaNodeVisuals()
{
	if (!GlowMesh)
	{
		GlowMesh = NewObject<UStaticMeshComponent>(this, TEXT("GlowMesh"));
		if (GlowMesh)
		{
			GlowMesh->SetupAttachment(RootComponent);
			GlowMesh->RegisterComponent();

			// Copy the static mesh from the main node mesh if available
			if (NodeMesh && NodeMesh->GetStaticMesh())
			{
				GlowMesh->SetStaticMesh(NodeMesh->GetStaticMesh());
			}

			// Scale slightly larger for glow overlay
			GlowMesh->SetWorldScale3D(FVector(1.05f));
			GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// Create dynamic emissive material
			UMaterialInterface* BaseMaterial = GlowMesh->GetMaterial(0);
			if (BaseMaterial)
			{
				GlowMaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);
				if (GlowMaterialInstance)
				{
					GlowMaterialInstance->SetScalarParameterValue(FName("EmissiveStrength"), 5.0f);
					GlowMesh->SetMaterial(0, GlowMaterialInstance);
				}
			}
		}
	}
}

float AAoCResourceNode::GetPulseIntensity() const
{
	if (const UWorld* World = GetWorld())
	{
		const float TimeSeconds = World->GetTimeSeconds();
		return 5.0f + 3.0f * FMath::Sin(TimeSeconds * 2.0f);
	}
	return 5.0f;
}
