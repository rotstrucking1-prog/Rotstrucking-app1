// Copyright Architect of Creation. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AoCGameState.generated.h"

/** Enum describing current weather state across the server. */
UENUM(BlueprintType)
enum class EAoCWeatherState : uint8
{
	Clear		UMETA(DisplayName = "Clear"),
	Cloudy		UMETA(DisplayName = "Cloudy"),
	Rain		UMETA(DisplayName = "Rain"),
	Storm		UMETA(DisplayName = "Storm"),
	Snow		UMETA(DisplayName = "Snow"),
	Fog			UMETA(DisplayName = "Fog"),
	Sandstorm	UMETA(DisplayName = "Sandstorm")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeatherChanged, EAoCWeatherState, NewWeather);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServerMessageBroadcast, const FString&, Message);

/**
 * AAoCGameState
 * Replicated game state holding server time, weather, and server-wide broadcasts.
 */
UCLASS()
class ARCHITECTOFCREATION_API AAoCGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AAoCGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ---- Server Time ----

	/** Replicated server time in seconds since the world started. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerTime, BlueprintReadOnly, Category = "AoC|Time")
	float ServerTime;

	UFUNCTION()
	void OnRep_ServerTime();

	/** Get the current server time. */
	UFUNCTION(BlueprintCallable, Category = "AoC|Time")
	float GetServerTime() const { return ServerTime; }

	// ---- Weather ----

	/** Current replicated weather state. */
	UPROPERTY(ReplicatedUsing = OnRep_WeatherState, BlueprintReadOnly, Category = "AoC|Weather")
	EAoCWeatherState WeatherState;

	UFUNCTION()
	void OnRep_WeatherState();

	/** Set the weather state (server only). */
	UFUNCTION(BlueprintCallable, Category = "AoC|Weather", meta = (BlueprintAuthorityOnly))
	void SetWeatherState(EAoCWeatherState NewState);

	/** Fired on all clients when the weather changes. */
	UPROPERTY(BlueprintAssignable, Category = "AoC|Weather")
	FOnWeatherChanged OnWeatherChanged;

	// ---- Server Broadcast ----

	/** Last broadcast message from the server. */
	UPROPERTY(ReplicatedUsing = OnRep_ServerMessage, BlueprintReadOnly, Category = "AoC|Broadcast")
	FString ServerMessage;

	UFUNCTION()
	void OnRep_ServerMessage();

	/** Broadcast a message to all connected clients (server only). */
	UFUNCTION(BlueprintCallable, Category = "AoC|Broadcast", meta = (BlueprintAuthorityOnly))
	void BroadcastServerMessage(const FString& Message);

	/** Fired on all clients when a server message is broadcast. */
	UPROPERTY(BlueprintAssignable, Category = "AoC|Broadcast")
	FOnServerMessageBroadcast OnServerMessageBroadcast;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
};
