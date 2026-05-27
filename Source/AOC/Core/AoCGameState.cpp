// Copyright Architect of Creation. All Rights Reserved.

#include "AoCGameState.h"
#include "Net/UnrealNetwork.h"

AAoCGameState::AAoCGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	ServerTime = 0.0f;
	WeatherState = EAoCWeatherState::Clear;
}

void AAoCGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAoCGameState, ServerTime);
	DOREPLIFETIME(AAoCGameState, WeatherState);
	DOREPLIFETIME(AAoCGameState, ServerMessage);
}

void AAoCGameState::BeginPlay()
{
	Super::BeginPlay();
}

void AAoCGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		ServerTime += DeltaSeconds;
	}
}

void AAoCGameState::OnRep_ServerTime()
{
	// Clients can use this for interpolation or UI updates
}

void AAoCGameState::OnRep_WeatherState()
{
	OnWeatherChanged.Broadcast(WeatherState);
}

void AAoCGameState::SetWeatherState(EAoCWeatherState NewState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (WeatherState != NewState)
	{
		WeatherState = NewState;
		OnWeatherChanged.Broadcast(WeatherState);
	}
}

void AAoCGameState::OnRep_ServerMessage()
{
	OnServerMessageBroadcast.Broadcast(ServerMessage);
}

void AAoCGameState::BroadcastServerMessage(const FString& Message)
{
	if (!HasAuthority())
	{
		return;
	}

	ServerMessage = Message;
	OnServerMessageBroadcast.Broadcast(ServerMessage);
}
