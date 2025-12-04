#include "RTSPlayerController.h"
#include "RTSGameMode.h"
#include "GridManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h" //  CreateWidget 
#include "RTSMainHUD.h"           //  UI C++ 类

ARTSPlayerController::ARTSPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    bIsPlacingUnit = false;
    PendingUnitType = EUnitType::Soldier;
}

void ARTSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // 1. 检查是否是本地玩家 (防止在多人联机的服务端创建 UI，虽然这里是单机但这是好习惯)
    if (IsLocalPlayerController())
    {
        // 2. 检查有没有在编辑器里设置 UI 类
        if (MainHUDClass)
        {
            // 3. 创建 UI 实例
            MainHUDInstance = CreateWidget<URTSMainHUD>(this, MainHUDClass);

            // 4. 添加到屏幕
            if (MainHUDInstance)
            {
                MainHUDInstance->AddToViewport();

                // (可选) 设为 GameAndUI 模式，确保能点按钮
                FInputModeGameAndUI InputMode;
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                SetInputMode(InputMode);
                bShowMouseCursor = true;
            }
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("RTSPlayerController: MainHUDClass is NOT set! Go to BP_RTSPlayerController and assign WBP_RTSMain."));
        }
    }
}


void ARTSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // 这里可以加一个 PlacementPreviewActor 跟随鼠标逻辑
}

void ARTSPlayerController::OnSelectUnitToPlace(EUnitType UnitType)
{
    PendingUnitType = UnitType;
    bIsPlacingUnit = true;
}

void ARTSPlayerController::HandleLeftClick()
{
    if (!bIsPlacingUnit) return;

    FHitResult Hit;
    GetHitResultUnderCursor(ECC_Visibility, false, Hit);

    if (Hit.bBlockingHit)
    {
        ARTSGameMode* GM = Cast<ARTSGameMode>(GetWorld()->GetAuthGameMode());
        AGridManager* GridMgr = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

        if (GM && GridMgr)
        {
            int32 X, Y;
            // 只有点在网格内才有效
            if (GridMgr->WorldToGrid(Hit.Location, X, Y))
            {
                if (GridMgr->IsTileWalkable(X, Y))
                {
                    // 这里价格先写死，或者通过配置表获取
                    int32 Cost = (PendingUnitType == EUnitType::Soldier) ? 50 : 100;

                    // 调用 GM 尝试购买
                    if (GM->TryBuyUnit(PendingUnitType, Cost, X, Y))
                    {
                        // 购买成功
                        // bIsPlacingUnit = false; // 可选：买完一个就取消，还是继续买
                    }
                }
            }
        }
    }
}

// 记得绑定输入
void ARTSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("LeftClick", IE_Pressed, this, &ARTSPlayerController::HandleLeftClick);
}