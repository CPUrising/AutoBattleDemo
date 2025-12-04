#include "RTSPlayerController.h"
#include "RTSGameMode.h"
#include "GridManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "RTSMainHUD.h"
#include "DrawDebugHelpers.h"

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

    if (IsLocalPlayerController())
    {
        if (MainHUDClass)
        {
            MainHUDInstance = CreateWidget<URTSMainHUD>(this, MainHUDClass);
            if (MainHUDInstance)
            {
                MainHUDInstance->AddToViewport();
                FInputModeGameAndUI InputMode;
                InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
                SetInputMode(InputMode);
                bShowMouseCursor = true;
            }
        }
    }
}

void ARTSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ARTSPlayerController::OnSelectUnitToPlace(EUnitType UnitType)
{
    PendingUnitType = UnitType;
    bIsPlacingUnit = true;
}

// 核心修复在这里！
void ARTSPlayerController::HandleLeftClick()
{
    // 1. 声明并获取 Hit (射线检测结果)
    FHitResult Hit;
    GetHitResultUnderCursor(ECC_Visibility, false, Hit);

    // 2. 声明并获取 GridManager (地图管理者)
    AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    // 3. 声明 X 和 Y 变量
    int32 X = 0;
    int32 Y = 0;

    // 4. 只有当点到了东西，且 GridManager 存在时才执行
    if (Hit.bBlockingHit && GridManager)
    {
        // 尝试把世界坐标转为格子坐标
        if (GridManager->WorldToGrid(Hit.Location, X, Y))
        {
            // ==========================================
            // 测试模式：左键点击哪里，就从 (0,0) 寻路到哪里
            // ==========================================

            // A. 获取起点 (0,0) 的世界坐标
            FVector StartPos = GridManager->GridToWorld(0, 0);

            // B. 获取鼠标点击点 (目标点) 的世界坐标
            FVector EndPos = GridManager->GridToWorld(X, Y);

            // C. 执行寻路 (调用 A 成员写的算法)
            TArray<FVector> Path = GridManager->FindPath(StartPos, EndPos);

            // D. 画出来看看
            if (Path.Num() > 0)
            {
                // 清除之前的线 (防止屏幕太乱)
                FlushPersistentDebugLines(GetWorld());

                // 画出每一个路径点 (蓝色球)
                for (const FVector& Point : Path)
                {
                    DrawDebugSphere(GetWorld(), Point, 15.0f, 12, FColor::Blue, false, 5.0f);
                }

                // 画出连线 (青色线)
                for (int32 i = 0; i < Path.Num() - 1; i++)
                {
                    DrawDebugLine(GetWorld(), Path[i], Path[i + 1], FColor::Cyan, false, 5.0f, 0, 3.0f);
                }

                // 打印成功日志
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, FString::Printf(TEXT("Path Found! Steps: %d"), Path.Num()));
            }
            else
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("No Path Found!"));
            }
        }
    }
}

void ARTSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("LeftClick", IE_Pressed, this, &ARTSPlayerController::HandleLeftClick);
}