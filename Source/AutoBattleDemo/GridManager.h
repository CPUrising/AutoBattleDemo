#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "GridManager.generated.h"

// 单个格子的配置数据
USTRUCT(BlueprintType)
struct FGridConfig
{
    GENERATED_BODY()

        UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 X;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        int32 Y;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        bool bIsBlocked;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
        TSubclassOf<AActor> BuildingClass; // 该格子上的建筑类型
};

// 关卡地图数据资产
UCLASS()
class AUTOBATTLEDEMO_API ULevelDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        int32 GridWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        int32 GridHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        float CellSize;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        TArray<FGridConfig> GridConfigurations; // 格子配置列表

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        FIntPoint PlayerBaseLocation; // 玩家基地位置

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Level Config")
        FIntPoint EnemyBaseLocation; // 敌人基地位置
};

// 格子节点结构体
USTRUCT(BlueprintType)
struct FGridNode
{
    GENERATED_BODY()

        int32 X;
    int32 Y;
    FVector WorldLocation;
    bool bIsBlocked;
    float Cost;
    AActor* OccupyingActor; // 当前占据的演员（建筑/单位）

    // 寻路相关
    float GCost; // 从起点到当前节点的成本
    float HCost; // 到终点的预估成本
    FGridNode* ParentNode;

    FGridNode() : X(0), Y(0), bIsBlocked(false), Cost(1.0f),
        OccupyingActor(nullptr), GCost(0), HCost(0), ParentNode(nullptr) {}

    float GetFCost() const { return GCost + HCost; }
};

// 用于通知单位重新寻路的委托
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGridUpdated, int32, int32); // 保持不变，语义仍匹配

UCLASS()
class AUTOBATTLEDEMO_API AGridManager : public AActor
{
    GENERATED_BODY()

public:
    AGridManager();

    // 初始化网格
    UFUNCTION(BlueprintCallable, Category = "Grid")
        void InitializeGridFromLevelData(ULevelDataAsset* LevelData);

    // 更新格子状态（动态阻挡）
    UFUNCTION(BlueprintCallable, Category = "Grid")
        void SetTileBlocked(int32 X, int32 Y, bool bBlocked, AActor* OccupyingActor = nullptr);

    // 世界坐标转格子坐标
    UFUNCTION(BlueprintCallable, Category = "Grid")
        bool WorldToGrid(FVector WorldPos, int32& OutX, int32& OutY);

    // 格子坐标转世界坐标
    UFUNCTION(BlueprintCallable, Category = "Grid")
        FVector GridToWorld(int32 X, int32 Y);

    // 检查格子是否可走
    UFUNCTION(BlueprintCallable, Category = "Grid")
        bool IsTileWalkable(int32 X, int32 Y);

    // 寻路算法（A*）
    UFUNCTION(BlueprintCallable, Category = "Pathfinding")
        TArray<FVector> FindPath(FVector StartPos, FVector EndPos);

    // 获取当前关卡数据
    UFUNCTION(BlueprintCallable, Category = "Level")
        ULevelDataAsset* GetCurrentLevelData() const { return CurrentLevelData; }

    // 加载新关卡
    UFUNCTION(BlueprintCallable, Category = "Level")
        void LoadLevelData(ULevelDataAsset* NewLevelData);

    // 获取网格更新委托
    FOnGridUpdated& GetGridUpdatedDelegate() { return OnGridUpdated; }

protected:
    virtual void BeginPlay() override;

private:
    // 检查坐标是否有效
    bool IsValidCoordinate(int32 X, int32 Y) const;

    // 获取邻居节点
    TArray<FGridNode*> GetNeighborNodes(FGridNode* CurrentNode);

    // 计算启发式成本
    float CalculateHCost(int32 X, int32 Y, int32 TargetX, int32 TargetY);

    // 重置寻路数据
    void ResetPathfindingData();

    // 从配置生成网格
    void GenerateGridFromConfig(ULevelDataAsset* LevelData);

    // 网格数据（一维数组模拟二维）
    TArray<FGridNode> GridNodes;

    // 当前关卡数据
    UPROPERTY(Transient)
        ULevelDataAsset* CurrentLevelData;

    // 网格属性
    int32 GridWidth;
    int32 GridHeight;
    float CellSize;

    // 网格更新委托（通知单位重新寻路）
    FOnGridUpdated OnGridUpdated;

    // 调试用：绘制网格
    void DrawDebugGrid();
};