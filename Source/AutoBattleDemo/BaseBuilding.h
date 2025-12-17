#pragma once
#include "CoreMinimal.h"
#include "BaseGameEntity.h"
#include "BaseBuilding.generated.h"

// 建筑类型枚举
UENUM(BlueprintType)
enum class EBuildingType : uint8
{
    Resource,      // 资源建筑（矿场）
    Defense,       // 防御建筑（炮塔）
    Headquarters,  // 大本营
    Wall,          // 墙
    Other          // 其他
};

UCLASS()
class AUTOBATTLEDEMO_API ABaseBuilding : public ABaseGameEntity
{
    GENERATED_BODY()

public:
    ABaseBuilding();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // --- 建筑属性 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
        EBuildingType BuildingType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
        int32 BuildingLevel;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Building")
        int32 MaxLevel;

    // --- 升级系统 ---
    UFUNCTION(BlueprintCallable, Category = "Building")
        void LevelUp();

    // 获取升级所需资源（供成员C的UI调用）
    UFUNCTION(BlueprintCallable, Category = "Building")
        void GetUpgradeCost(int32& OutGold, int32& OutElixir);

    // 检查是否可以升级
    UFUNCTION(BlueprintCallable, Category = "Building")
        bool CanUpgrade() const;

    // --- 点击交互 ---
    virtual void NotifyActorOnClicked(FKey ButtonPressed) override;

    // --- 组件 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class UStaticMeshComponent* MeshComp;

protected:
    // 升级时的属性提升（子类可重写）
    virtual void ApplyLevelUpBonus();

    // 升级基础花费
    UPROPERTY(EditAnywhere, Category = "Building|Upgrade")
        int32 BaseUpgradeGoldCost;

    UPROPERTY(EditAnywhere, Category = "Building|Upgrade")
        int32 BaseUpgradeElixirCost;
};