#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerController_Parent.h"
#include "GridManager_Parent.h"
#include "Pathfinder.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class TOPDOWN_API UPathfinder : public UActorComponent
{
	GENERATED_BODY()

private:

	/**
	 * 
	 * Private Functions
	 * 
	*/

	// BreadthSearch
	int32 GetMovementOptionArea(int32 Movement);
	void ResetFinalCosts();
	// Evaluates tile and adds it to appropriate arrays like ValidTiles, TilesToMakeRed, and also populates the CharsInRange array.
	void EvaluateCurrentTileForBreadthSearch();
	bool TileShouldBeRed();
	bool EnemyIsPathfindingAndCurrentTileIsOccupiedByPlayer();
	bool PlayerIsPathfindingAndCurrentTileIsOccupiedByEnemy();
	void AddCurrentTileToValidTiles();
	bool CurrentTileIsWalkable();
	// Some of the tiles that clear the check to go into the RedTiles array are also in the ValidTiles, and this function removes those.
	void FilterRedTiles();

	// FindPathToTarget
	void Pathfind();
	int32 GetEstimatedCostToTile(FVector2D Tile);
	FVector2D FindCheapestInQueue();
	bool QueueElementIsBetterThanQueueCheapest(FVector2D QueueElem, FVector2D QueueCheapest);
	void SetTileVariables(FVector2D PreviousTile, FVector2D CurrentNeighbor);
	bool IsThisFirstEnemyPathfind(TArray<FVector2D> BestPath);

	// Both
	void ClearMemberVariables();


	/**
	 * 
	 * Member Variables
	 * 
	*/

	// BreadthSearch
	TArray<ACharacterOMEGAPARENT*> CharsFound;
	int32 NumOfPossibleTiles;

	// FindPathToTarget
	TArray<FVector2D> MovementArea;
	FVector2D InitialTile;
	FVector2D DestinationTile;
	TArray<FVector2D> Path;
	TArray<FVector2D> EstablishedTiles;
	int32 FinalCostFromCurrentNeighbor;

	// BreadthSearch and FindPathToTarget
	TArray<FVector2D> Queue;
	FVector2D CurrentTile;
	FGridStruct* CurrentTileStruct;
	TArray<FVector2D> ValidTiles;
	int32 MaxMovement;
	int32 MoveCost;
	TArray<FVector2D> RedTiles;
	bool IsPlayerPathfinding;
	
protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Sets default values for this component's properties
	UPathfinder();

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	AGridManager_Parent* GridManager;

	UPROPERTY(EditAnywhere)
	APlayerController* Owner = Cast<APlayerController>(GetOwner());

	bool IsTileOccupied(FVector2D Tile);

	bool IsCurrentTileOccupiedByPlayer();

	UFUNCTION(BlueprintCallable, BluePrintPure, Category = "Utility")
	bool IsCurrentTileOccupiedByEnemy();

	FGridStruct* GetTileStruct(FVector2D Tile);

	UFUNCTION(BlueprintCallable, BluePrintPure)
	// This overload is for general use
	TArray<FVector2D> GetTileNeighbors(FVector2D Tile, bool IncludePlayers = true, bool IncludeEnemies = true);

	UFUNCTION(BlueprintCallable, Category = "Grid Struct")
	void ResetPathfindingTileVars();

	UFUNCTION(BlueprintCallable)
	// Returns: Tiles outside movement range to make red if player is calling, Chars found if enemy is calling, and return value is the valid movement area.
	TArray<FVector2D> BreadthSearch(FVector2D StartTile, int32 CharacterMovement, TArray<FVector2D>& TilesToMakeRed, TArray<ACharacterOMEGAPARENT*>& CharsFound, bool IsPlayerCalling = true);

	TArray<FVector2D> RetracePath();

	UFUNCTION(BlueprintCallable)
	// IsPlayerCalling is true if a player character is pathfinding, false if an enemy is. This way if an enemy calls this they can go through other enemies.
	TArray<FVector2D> FindPathToTarget(TArray<FVector2D> AvailableMovementArea, FVector2D StartTile, FVector2D TargetTile, bool IsPlayerCalling = true); 

	UFUNCTION(BlueprintCallable)
	TArray<FVector2D> FindPathAsEnemy(FVector2D StartTile, TArray<FVector2D> PossibleTargetTiles);
};