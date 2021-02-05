#include "Pathfinder.h"
#include "Kismet/GameplayStatics.h"
#include "CharacterOMEGAPARENT.h"
#include "GridBase_Base.h"
#include "Private_Pathfinder.h"

// Sets default values for this component's properties
UPathfinder::UPathfinder()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}


// Called when the game starts
void UPathfinder::BeginPlay()
{
	Super::BeginPlay();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGridManager_Parent::StaticClass(), FoundActors);
	if (FoundActors.Num() > 0)
	{
		GridManager = Cast<AGridManager_Parent>(FoundActors[0]);
	}
}


// Called every frame
void UPathfinder::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}


FGridStruct* UPathfinder::GetTileStruct(FVector2D Tile)
{
	return GridManager->GridStruct.Find(Tile);
}

bool UPathfinder::IsTileOccupied(FVector2D Tile)
{
	return IsValid(GetTileStruct(Tile)->OccupiedBy);
}

bool UPathfinder::IsCurrentTileOccupiedByPlayer()
{
	return IsTileOccupied(CurrentTile) && GetTileStruct(CurrentTile)->OccupiedBy->Team == Team::Player; // IsTileOccupied() is there to protect from nullptrs
}

bool UPathfinder::IsCurrentTileOccupiedByEnemy()
{
	return IsTileOccupied(CurrentTile) && GetTileStruct(CurrentTile)->OccupiedBy->Team == Team::Enemy; // IsTileOccupied() is there to protect from nullptrs
}

TArray<FVector2D> UPathfinder::GetTileNeighbors(FVector2D Tile, bool IncludePlayers, bool IncludeEnemies)
{
	TArray<FVector2D> OutNeighbors;
	TArray<FVector2D> CardinalDirections;

	CardinalDirections.Add(FVector2D(0, 1));  // Up
	CardinalDirections.Add(FVector2D(1, 0));  // Right
	CardinalDirections.Add(FVector2D(0, -1)); // Down
	CardinalDirections.Add(FVector2D(-1, 0)); // Left

	for (FVector2D CardinalDirection : CardinalDirections)
	{
		FVector2D AdjacentTile = CardinalDirection + Tile;

		if(MovementArea.Num() > 0) // This checks if the MovementArea array has data. It only should when this is called in the FindPathToTarget function
		{
			if (MovementArea.Contains(AdjacentTile))
			{
				goto EVALUATE;
			}
		}
		else if (GetTileStruct(AdjacentTile))
		{
			EVALUATE:if (IsTileOccupied(AdjacentTile))
			{
				FGridStruct* AdjacentTileStruct = GetTileStruct(AdjacentTile);
				ACharacterOMEGAPARENT* Char = AdjacentTileStruct->OccupiedBy;
				if (!IncludePlayers ^ (Char->Team == Team::Player) || IncludeEnemies ^ (Char->Team == Team::Player)) // Returns true if both booleans are the same, i.e. char is a player and include players, or if an enemy and include enemies
				{
					OutNeighbors.AddUnique(AdjacentTile);
				}
			}
			else
			{
				OutNeighbors.AddUnique(AdjacentTile);
			}
		}
	}
	return OutNeighbors;
}


TArray<FVector2D> UPathfinder::BreadthSearch(FVector2D StartTile, int32 CharacterMovement, TArray<FVector2D>& TilesToMakeRed, TArray<ACharacterOMEGAPARENT*>& CharsInRange, bool IsPlayerCalling)
{
	ClearMemberVariables();
	MaxMovement = CharacterMovement;
	NumOfPossibleTiles = GetMovementOptionArea(MaxMovement + 1); // The + 1 is so it will search for the red highlights
	IsPlayerPathfinding = IsPlayerCalling;
	Queue.Add(StartTile);
	ValidTiles.AddUnique(StartTile);
	ResetFinalCosts();

	for (int32 i = 1; i <= NumOfPossibleTiles; i++)
	{
		if (Queue.Num() > 0)
		{
			for (FVector2D NeighborTile : GetTileNeighbors(Queue[0]))
			{
				CurrentTile = NeighborTile;
				EvaluateCurrentTileForBreadthSearch();
			}
			Queue.RemoveAt(0);
		}
	}

	FilterRedTiles();
	TilesToMakeRed = RedTiles;
	CharsInRange = CharsFound;
	return ValidTiles;
}

void UPathfinder::ClearMemberVariables()
{
	Queue.Empty();
	ValidTiles.Empty();
	RedTiles.Empty();
	CharsFound.Empty();
	Path.Empty();
	Queue.Empty();
	EstablishedTiles.Empty();
	MovementArea.Empty();
	FinalCostFromCurrentNeighbor = 0;
}

int32 UPathfinder::GetMovementOptionArea(int32 Movement)
{
	return ((Movement * 2) + 2) * (Movement + 1) - ((Movement * 2) + 1);
}

void UPathfinder::ResetFinalCosts()
{
	for(auto& Elem : GridManager->GridStruct)
	{
		GetTileStruct(Elem.Key)->FinalCost = 0;
	}
}

void UPathfinder::EvaluateCurrentTileForBreadthSearch()
{
	CurrentTileStruct = GetTileStruct(CurrentTile);
	MoveCost = GetTileStruct(Queue[0])->FinalCost + CurrentTileStruct->TileMovementCost;
	if (TileShouldBeRed())
	{
		RedTiles.AddUnique(CurrentTile);
		if (EnemyIsPathfindingAndCurrentTileIsOccupiedByPlayer())
		{
			CharsFound.AddUnique(CurrentTileStruct->OccupiedBy);
		}
	}
	else if (CurrentTileIsWalkable())
	{
		AddCurrentTileToValidTiles();
	}
}

bool UPathfinder::TileShouldBeRed()
{
	return MoveCost > MaxMovement || // Tile outside movement range
	CurrentTileStruct->TerrainType == Impossible ||
	PlayerIsPathfindingAndCurrentTileIsOccupiedByEnemy() ||
	EnemyIsPathfindingAndCurrentTileIsOccupiedByPlayer();
}

bool UPathfinder::PlayerIsPathfindingAndCurrentTileIsOccupiedByEnemy()
{
	return IsPlayerPathfinding && IsCurrentTileOccupiedByEnemy();
}

bool UPathfinder::EnemyIsPathfindingAndCurrentTileIsOccupiedByPlayer()
{
	return !IsPlayerPathfinding && IsCurrentTileOccupiedByPlayer();
}

void UPathfinder::AddCurrentTileToValidTiles()
{
	Queue.AddUnique(CurrentTile);
	CurrentTileStruct->FinalCost = MoveCost;
	ValidTiles.AddUnique(CurrentTile);
}

bool UPathfinder::CurrentTileIsWalkable()
{
	return MoveCost <= MaxMovement && (!ValidTiles.Contains(CurrentTile) || 
	MoveCost < CurrentTileStruct->FinalCost);
}

void UPathfinder::FilterRedTiles() 
{
	for (FVector2D RedTile : RedTiles)
	{
		if (ValidTiles.Contains(RedTile))
		{
			RedTiles.Remove(RedTile);
		}
	}
}

/* ==========================================
	Breadth Search ends and Find Path starts	
========================================== */


TArray<FVector2D> UPathfinder::FindPathToTarget(TArray<FVector2D> AvailableMovementArea, FVector2D StartTile, FVector2D TargetTile, bool IsPlayerCalling)
{
	if (StartTile == TargetTile)
	{
		Path.Add(StartTile);
		return Path;
	}
	
	ClearMemberVariables();
	ResetPathfindingTileVars();
	MovementArea = AvailableMovementArea;
	InitialTile = StartTile;
	CurrentTile = InitialTile;
	DestinationTile = TargetTile;
	IsPlayerPathfinding = IsPlayerCalling;
	int32 EstimatedCost = GetEstimatedCostToTile(DestinationTile);
	CurrentTileStruct = GetTileStruct(CurrentTile);
	CurrentTileStruct->FinalCost = EstimatedCost;
	CurrentTileStruct->EstimatedCostToTarget = EstimatedCost;
	Queue.AddUnique(InitialTile);

	Pathfind();
	
	MovementArea.Empty(); // I empty this so it's not effecting GetTileNeighbors function
	return Path;
}

void UPathfinder::ResetPathfindingTileVars()
{
	for (auto& Element : GridManager->GridStruct)
	{
		FGridStruct* TileStruct = GetTileStruct(Element.Key);
		TileStruct->FinalCost = 0;
		TileStruct->CostFromStart = 0;
		TileStruct->EstimatedCostToTarget = 0;
		TileStruct->PreviousTileCoords = FVector2D(0, 0);
	}
}

void UPathfinder::Pathfind()
{
	while (Queue.Num() > 0)
	{
		CurrentTile = FindCheapestInQueue();
		Queue.Remove(CurrentTile);
		EstablishedTiles.AddUnique(CurrentTile);
		TArray<FVector2D> Neighbors = GetTileNeighbors(CurrentTile, IsPlayerPathfinding, !IsPlayerPathfinding);
		for (FVector2D CurrentNeighbor : Neighbors)
		{
			if (!EstablishedTiles.Contains(CurrentNeighbor) ||
			FinalCostFromCurrentNeighbor < GetTileStruct(CurrentNeighbor)->CostFromStart)
			{
				FinalCostFromCurrentNeighbor = GetTileStruct(CurrentTile)->CostFromStart + GetTileStruct(CurrentNeighbor)->TileMovementCost;
				SetTileVariables(CurrentTile, CurrentNeighbor);
				if (CurrentNeighbor == DestinationTile)
				{
					Path = RetracePath();
					return;
				}
				Queue.AddUnique(CurrentNeighbor);
			}
		}
	}
}

int32 UPathfinder::GetEstimatedCostToTile(FVector2D Tile)
{
	FVector2D TempDistance = Tile - DestinationTile;
	FVector2D FinalDistance = FVector2D(abs(TempDistance.X), abs(TempDistance.Y));
	return int32(FinalDistance.X + FinalDistance.Y);
}

FVector2D UPathfinder::FindCheapestInQueue()
{
	FVector2D QueueCheapest = Queue[0];
	for (FVector2D Element : Queue)
	{
		FGridStruct* QueueElemStruct = GetTileStruct(Element);
		FGridStruct* QueueCheapestStruct = GetTileStruct(QueueCheapest);
		if (QueueElementIsBetterThanQueueCheapest(Element, QueueCheapest))
		{
			QueueCheapest = Element;
		}
	}
	return QueueCheapest;
}

bool UPathfinder::QueueElementIsBetterThanQueueCheapest(FVector2D QueueElem, FVector2D QueueCheapest)
{
	FGridStruct* QueueElemStruct = GetTileStruct(QueueElem);
	FGridStruct* QueueCheapestStruct = GetTileStruct(QueueCheapest);
	// Conditions for being true: QueueElem FinalCost is less than QueueCheapest's, or if they are equal but QueueElem is closer to destination
	return (QueueElemStruct->FinalCost < QueueCheapestStruct->FinalCost) || 
	(QueueElemStruct->FinalCost == QueueCheapestStruct->FinalCost && 
	QueueElemStruct->EstimatedCostToTarget < QueueCheapestStruct->EstimatedCostToTarget);
}

void UPathfinder::SetTileVariables(FVector2D PreviousTile, FVector2D CurrentNeighbor)
{
	FGridStruct* CurrentNeighborStruct = GetTileStruct(CurrentNeighbor);
	CurrentNeighborStruct->FinalCost = FinalCostFromCurrentNeighbor + CurrentNeighborStruct->TileMovementCost;
	CurrentNeighborStruct->CostFromStart = FinalCostFromCurrentNeighbor;
	CurrentNeighborStruct->EstimatedCostToTarget = GetEstimatedCostToTile(CurrentNeighbor);
	CurrentNeighborStruct->PreviousTileCoords = PreviousTile;
}

TArray<FVector2D> UPathfinder::RetracePath()
{
	TArray<FVector2D> InvertedPath;
	TArray<FVector2D> OutPath;
	CurrentTile = DestinationTile;

	while (CurrentTile != InitialTile)
	{
		InvertedPath.AddUnique(CurrentTile);
		CurrentTile = GetTileStruct(CurrentTile)->PreviousTileCoords;
	}
	for (int32 i = InvertedPath.Num() - 1; i >= 0; i--)
	{
		OutPath.AddUnique(InvertedPath[i]);
	}
	return OutPath;
}


TArray<FVector2D> UPathfinder::FindPathAsEnemy(FVector2D StartTile, TArray<FVector2D> PossibleTargetTiles)
{
	TArray<FVector2D> CurrentPath,
	BestPath;
	int32 CurrentPathCost,
	BestPathCost = 999;
	for(FVector2D TargetTileElement : PossibleTargetTiles)
	{
		TArray<FVector2D> Argument;
		CurrentPath = FindPathToTarget(Argument, StartTile, TargetTileElement, false);
		CurrentPathCost = GetTileStruct(CurrentPath.Last())->FinalCost; // Make sure to keep the pathfinding variables non-zero after pathfinding so I can use this outside the function
		if (IsThisFirstEnemyPathfind(BestPath))
		{
			BestPath = CurrentPath;
			BestPathCost = GetTileStruct(BestPath.Last())->FinalCost;
		}
		else if (CurrentPathCost < BestPathCost)
		{
			BestPath = CurrentPath;
			BestPathCost = GetTileStruct(BestPath.Last())->FinalCost;
		}
	}
	return BestPath;
}

bool UPathfinder::IsThisFirstEnemyPathfind(TArray<FVector2D> BestPath)
{
	return !BestPath.IsValidIndex(0);
}

