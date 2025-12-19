#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTSCoreTypes.h"
#include "RTSPlayerController.generated.h"

class URTSMainHUD;

UCLASS()
class AUTOBATTLEDEMO_API ARTSPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ARTSPlayerController();
    virtual void Tick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual void BeginPlay() override;

    // --- 交互逻辑 ---

    // 玩家点击了UI上的“购买单位”
    UFUNCTION(BlueprintCallable)
        void OnSelectUnitToPlace(EUnitType UnitType);

    // 新增：玩家点击了UI上的“建造建筑”
    UFUNCTION(BlueprintCallable)
        void OnSelectBuildingToPlace(EBuildingType BuildingType);

    // 鼠标点击左键
    UFUNCTION(BlueprintCallable)
        void HandleLeftClick();

protected:
    // --- UI 配置 ---
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
        TSubclassOf<URTSMainHUD> MainHUDClass;

    UPROPERTY()
        URTSMainHUD* MainHUDInstance;

    // 幽灵 Actor
    UPROPERTY()
        AActor* PreviewGhostActor;

    UPROPERTY(EditDefaultsOnly, Category = "UI")
        TSubclassOf<AActor> PlacementPreviewClass;

    // 辅助函数：更新幽灵位置
    void UpdatePlacementGhost();

private:
    // 当前状态数据
    EUnitType PendingUnitType;
    EBuildingType PendingBuildingType;

    bool bIsPlacingUnit;
    bool bIsPlacingBuilding;
};