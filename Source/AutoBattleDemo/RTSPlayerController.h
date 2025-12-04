#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RTSCoreTypes.h" // 引用枚举
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

    // --- 新增：覆盖 BeginPlay (用来创建 UI) ---
    virtual void BeginPlay() override;

    // --- 交互逻辑 ---

    // 玩家点击了UI上的“购买步兵”
    UFUNCTION(BlueprintCallable)
        void OnSelectUnitToPlace(EUnitType UnitType);

    // 鼠标点击左键 (在 Blueprint 中绑定 Input Action: LeftClick)
    UFUNCTION(BlueprintCallable)
        void HandleLeftClick();

private:
    // 当前正在“拖拽/悬停”准备放置的单位类型
    EUnitType PendingUnitType;
    bool bIsPlacingUnit;

    // 一个临时的 Actor，跟着鼠标跑，显示预览效果
    UPROPERTY()
        AActor* PlacementPreviewActor;

protected:
    // --- UI 配置 ---

    // 1. 暴露给编辑器：请在这里选择 WBP_RTSMain
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
        TSubclassOf<URTSMainHUD> MainHUDClass;

    // 2. 内部持有：创建出来的 UI 实例
    UPROPERTY()
        URTSMainHUD* MainHUDInstance;
};