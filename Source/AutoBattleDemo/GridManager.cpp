#include "GridManager.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

AGridManager::AGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
    GridWidth = 20;
    GridHeight = 20;
    CellSize = 100.0f;
}

void AGridManager::BeginPlay()
{
    Super::BeginPlay();
}

void AGridManager::InitializeGridFromLevelData(ULevelDataAsset* LevelData)
{
    if (LevelData)
    {
        LoadLevelData(LevelData);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("LevelData is null!"));
    }
}

void AGridManager::SetTileBlocked(int32 X, int32 Y, bool bBlocked, AActor* OccupyingActor)
{
    if (!IsValidCoordinate(X, Y)) return;

    int32 Index = Y * GridWidth + X;
    if (Index >= 0 && Index < GridNodes.Num())
    {
        GridNodes[Index].bIsBlocked = bBlocked;
        GridNodes[Index].OccupyingActor = OccupyingActor;

        // 通知所有单位该格子状态已更新，需要重新寻路
        OnGridUpdated.Broadcast(X, Y);

        // 调试输出
        UE_LOG(LogTemp, Log, TEXT("Grid (%d,%d) updated - Blocked: %s"), X, Y, bBlocked ? TEXT("True") : TEXT("False"));
    }
}

bool AGridManager::WorldToGrid(FVector WorldPos, int32& OutX, int32& OutY)
{
    OutX = FMath::FloorToInt((WorldPos.X) / CellSize);
    OutY = FMath::FloorToInt((WorldPos.Y) / CellSize);

    return IsValidCoordinate(OutX, OutY);
}

FVector AGridManager::GridToWorld(int32 X, int32 Y)
{
    if (!IsValidCoordinate(X, Y)) return FVector::ZeroVector;

    return FVector(
        X * CellSize + CellSize / 2.0f,
        Y * CellSize + CellSize / 2.0f,
        0.0f
    );
}

bool AGridManager::IsTileWalkable(int32 X, int32 Y)
{
    if (!IsValidCoordinate(X, Y)) return false;

    int32 Index = Y * GridWidth + X;
    return !GridNodes[Index].bIsBlocked;
}

TArray<FVector> AGridManager::FindPath(FVector StartPos, FVector EndPos)
{
    TArray<FVector> Path;

    // 转换起点和终点到网格坐标
    int32 StartX, StartY, EndX, EndY;
    if (!WorldToGrid(StartPos, StartX, StartY) || !WorldToGrid(EndPos, EndX, EndY))
    {
        UE_LOG(LogTemp, Warning, TEXT("Start or end position is out of grid bounds!"));
        return Path;
    }

    // 检查终点是否可走
    if (!IsTileWalkable(EndX, EndY))
    {
        UE_LOG(LogTemp, Warning, TEXT("End position is blocked!"));
        return Path;
    }

    // 重置寻路数据
    ResetPathfindingData();

    // 找到起点和终点节点
    FGridNode* StartNode = &GridNodes[StartY * GridWidth + StartX];
    FGridNode* EndNode = &GridNodes[EndY * GridWidth + EndX];

    // ========== 修复：替换 TPriorityQueue 为 TArray + 排序 ==========
    // 开放列表（存储待处理节点）
    TArray<FGridNode*> OpenList;
    // 关闭列表（存储已处理节点）
    TSet<FGridNode*> ClosedList;

    // 初始化起点节点
    StartNode->GCost = 0;
    StartNode->HCost = CalculateHCost(StartNode->X, StartNode->Y, EndX, EndY);
    OpenList.Add(StartNode);

    while (OpenList.Num() > 0)
    {
        // 1. 从开放列表中找到 FCost 最小的节点（替代优先队列）
        FGridNode* CurrentNode = nullptr;
        float MinFCost = TNumericLimits<float>::Max();
        for (FGridNode* Node : OpenList)
        {
            if (Node->GetFCost() < MinFCost)
            {
                MinFCost = Node->GetFCost();
                CurrentNode = Node;
            }
        }

        if (!CurrentNode) break; // 无可用节点，退出

        // 2. 将当前节点移出开放列表，加入关闭列表
        OpenList.Remove(CurrentNode);
        ClosedList.Add(CurrentNode);

        // 3. 到达终点，回溯生成路径
        if (CurrentNode == EndNode)
        {
            FGridNode* PathNode = EndNode;
            while (PathNode != nullptr)
            {
                Path.Insert(PathNode->WorldLocation, 0);
                PathNode = PathNode->ParentNode;
            }
            return Path;
        }

        // 4. 处理邻居节点
        TArray<FGridNode*> Neighbors = GetNeighborNodes(CurrentNode);
        for (FGridNode* Neighbor : Neighbors)
        {
            // 跳过不可走或已处理的节点
            if (Neighbor->bIsBlocked || ClosedList.Contains(Neighbor))
                continue;

            // 计算从当前节点到邻居的成本
            float TentativeGCost = CurrentNode->GCost +
                FVector::Dist(CurrentNode->WorldLocation, Neighbor->WorldLocation) * Neighbor->Cost;

            // 如果是更优路径，更新邻居节点
            if (TentativeGCost < Neighbor->GCost || !OpenList.Contains(Neighbor))
            {
                Neighbor->GCost = TentativeGCost;
                Neighbor->HCost = CalculateHCost(Neighbor->X, Neighbor->Y, EndX, EndY);
                Neighbor->ParentNode = CurrentNode;

                // 未在开放列表则添加
                if (!OpenList.Contains(Neighbor))
                {
                    OpenList.Add(Neighbor);
                }
            }
        }
    }

    // 找不到路径
    UE_LOG(LogTemp, Warning, TEXT("No path found!"));
    return Path;
}

void AGridManager::LoadLevelData(ULevelDataAsset* NewLevelData)
{
    if (!NewLevelData) return;

    CurrentLevelData = NewLevelData;
    GenerateGridFromConfig(NewLevelData);

    // 绘制调试网格
    DrawDebugGrid();

    UE_LOG(LogTemp, Log, TEXT("Loaded level data: Grid %dx%d, Cell size: %f"),
        GridWidth, GridHeight, CellSize);
}

bool AGridManager::IsValidCoordinate(int32 X, int32 Y) const
{
    return X >= 0 && X < GridWidth&& Y >= 0 && Y < GridHeight;
}

TArray<FGridNode*> AGridManager::GetNeighborNodes(FGridNode* CurrentNode)
{
    TArray<FGridNode*> Neighbors;

    // 检查8个方向的邻居（如果需要4方向寻路可以注释掉对角线方向）
    for (int32 YOffset = -1; YOffset <= 1; YOffset++)
    {
        for (int32 XOffset = -1; XOffset <= 1; XOffset++)
        {
            // 跳过当前节点
            if (XOffset == 0 && YOffset == 0)
                continue;

            int32 CheckX = CurrentNode->X + XOffset;
            int32 CheckY = CurrentNode->Y + YOffset;

            if (IsValidCoordinate(CheckX, CheckY))
            {
                Neighbors.Add(&GridNodes[CheckY * GridWidth + CheckX]);
            }
        }
    }

    return Neighbors;
}

float AGridManager::CalculateHCost(int32 X, int32 Y, int32 TargetX, int32 TargetY)
{
    // 使用曼哈顿距离作为启发式函数
    int32 DeltaX = FMath::Abs(X - TargetX);
    int32 DeltaY = FMath::Abs(Y - TargetY);
    return (DeltaX + DeltaY) * 10.0f; // 乘以格子大小相关的系数
}

void AGridManager::ResetPathfindingData()
{
    for (auto& Node : GridNodes)
    {
        Node.GCost = 0;
        Node.HCost = 0;
        Node.ParentNode = nullptr;
    }
}

void AGridManager::GenerateGridFromConfig(ULevelDataAsset* LevelData)
{
    GridWidth = LevelData->GridWidth;
    GridHeight = LevelData->GridHeight;
    CellSize = LevelData->CellSize;
    GridNodes.Empty(GridWidth * GridHeight);

    // 初始化网格
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            FGridNode Node;
            Node.X = X;
            Node.Y = Y;
            Node.WorldLocation = GridToWorld(X, Y);
            Node.bIsBlocked = false;
            Node.OccupyingActor = nullptr;
            Node.Cost = 1.0f;

            GridNodes.Add(Node);
        }
    }

    // 应用关卡配置
    for (const auto& GridConfig : LevelData->GridConfigurations)
    {
        SetTileBlocked(GridConfig.X, GridConfig.Y, GridConfig.bIsBlocked);

        // 如果配置了建筑类型，生成建筑
        if (GridConfig.BuildingClass)
        {
            FVector SpawnLocation = GridToWorld(GridConfig.X, GridConfig.Y);
            SpawnLocation.Z += 50.0f; // 稍微抬高避免碰撞
            AActor* NewBuilding = GetWorld()->SpawnActor<AActor>(
                GridConfig.BuildingClass,
                SpawnLocation,
                FRotator::ZeroRotator
                );

            if (NewBuilding)
            {
                // 更新格子的占据者
                int32 Index = GridConfig.Y * GridWidth + GridConfig.X;
                if (Index >= 0 && Index < GridNodes.Num())
                {
                    GridNodes[Index].OccupyingActor = NewBuilding;
                }
            }
        }
    }
}

void AGridManager::DrawDebugGrid()
{
#if WITH_EDITOR
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            FVector WorldLoc = GridToWorld(X, Y);
            FVector BoxExtent(CellSize / 2.0f - 1.0f, CellSize / 2.0f - 1.0f, 50.0f);

            // 根据是否被阻挡设置不同颜色
            FColor Color = IsTileWalkable(X, Y) ? FColor::Green : FColor::Red;

            DrawDebugBox(GetWorld(), WorldLoc, BoxExtent, Color, false, 30.0f, 0, 2.0f);
        }
    }
#endif
}