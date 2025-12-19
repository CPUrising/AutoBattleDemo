#include "RTSPlayerController.h"
#include "RTSGameMode.h"
#include "GridManager.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"
#include "RTSMainHUD.h"
#include "DrawDebugHelpers.h"
#include "RTSGameInstance.h"
#include "RTSCoreTypes.h"
#include "Building_Resource.h" // 必须引用，用于点击收集资源

ARTSPlayerController::ARTSPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
    PrimaryActorTick.bCanEverTick = true;

    bIsPlacingUnit = false;
    bIsPlacingBuilding = false;
    PendingUnitType = EUnitType::Soldier;
    PendingBuildingType = EBuildingType::None;
}

void ARTSPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (IsLocalPlayerController() && MainHUDClass)
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

void ARTSPlayerController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 无论是造兵还是造建筑，都更新幽灵和网格
    if (bIsPlacingUnit || bIsPlacingBuilding)
    {
        UpdatePlacementGhost();

        AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
        if (GridManager)
        {
            int32 HoverX = -1;
            int32 HoverY = -1;
            FHitResult Hit;
            GetHitResultUnderCursor(ECC_Visibility, false, Hit);

            if (Hit.bBlockingHit)
            {
                GridManager->WorldToGrid(Hit.Location, HoverX, HoverY);
            }
            GridManager->DrawGridVisuals(HoverX, HoverY);
        }
    }
}

void ARTSPlayerController::OnSelectUnitToPlace(EUnitType UnitType)
{
    PendingUnitType = UnitType;
    bIsPlacingUnit = true;
    bIsPlacingBuilding = false; // 互斥

    // 生成幽灵
    if (!PreviewGhostActor && PlacementPreviewClass)
    {
        PreviewGhostActor = GetWorld()->SpawnActor<AActor>(PlacementPreviewClass, FVector::ZeroVector, FRotator::ZeroRotator);
    }
    if (PreviewGhostActor) PreviewGhostActor->SetActorHiddenInGame(false);
}

void ARTSPlayerController::OnSelectBuildingToPlace(EBuildingType BuildingType)
{
    PendingBuildingType = BuildingType;
    bIsPlacingBuilding = true;
    bIsPlacingUnit = false; // 互斥

    // 生成幽灵
    if (!PreviewGhostActor && PlacementPreviewClass)
    {
        PreviewGhostActor = GetWorld()->SpawnActor<AActor>(PlacementPreviewClass, FVector::ZeroVector, FRotator::ZeroRotator);
    }
    if (PreviewGhostActor) PreviewGhostActor->SetActorHiddenInGame(false);
}

void ARTSPlayerController::HandleLeftClick()
{
    FHitResult Hit;
    GetHitResultUnderCursor(ECC_Visibility, false, Hit);

    AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));

    if (!Hit.bBlockingHit) return;

    // 1. 如果在网格范围内
    if (GridManager)
    {
        int32 X, Y;
        if (GridManager->WorldToGrid(Hit.Location, X, Y))
        {
            ARTSGameMode* GM = Cast<ARTSGameMode>(GetWorld()->GetAuthGameMode());
            if (!GM) return;

            bool bSuccess = false;

            // --- 造兵模式 ---
            if (bIsPlacingUnit)
            {
                // 这里的Cost先写死，建议后期建立一个配置表 GetUnitCost(Type)
                int32 Cost = 50;
                if (PendingUnitType == EUnitType::Archer) Cost = 100;
                if (PendingUnitType == EUnitType::Giant) Cost = 300;
                if (PendingUnitType == EUnitType::Bomber) Cost = 150;

                bSuccess = GM->TryBuyUnit(PendingUnitType, Cost, X, Y);

                if (bSuccess) bIsPlacingUnit = false;
            }
            // --- 造建筑模式 ---
            else if (bIsPlacingBuilding)
            {
                int32 Cost = 200;
                if (PendingBuildingType == EBuildingType::Resource) Cost = 150;
                if (PendingBuildingType == EBuildingType::Wall) Cost = 50;

                bSuccess = GM->TryBuildBuilding(PendingBuildingType, Cost, X, Y);

                if (bSuccess) bIsPlacingBuilding = false;
            }

            // 处理成功/失败反馈
            if (bSuccess)
            {
                if (PreviewGhostActor) PreviewGhostActor->SetActorHiddenInGame(true);
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Placed Successfully!"));
                return; // 放置成功就结束，不再执行下面的点击逻辑
            }
            else if (bIsPlacingUnit || bIsPlacingBuilding)
            {
                if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Cannot Place Here!"));
                return;
            }
        }
    }

    // 2. 如果不是放置模式，检测是否点击了资源建筑
    AActor* HitActor = Hit.GetActor();
    if (HitActor)
    {
        ABuilding_Resource* ResBuilding = Cast<ABuilding_Resource>(HitActor);
        if (ResBuilding)
        {
            float Amount = ResBuilding->CollectResource();
            if (Amount > 0)
            {
                URTSGameInstance* GI = Cast<URTSGameInstance>(GetGameInstance());
                if (GI)
                {
                    if (ResBuilding->bProducesGold)
                        GI->PlayerGold += Amount;
                    else
                        GI->PlayerElixir += Amount;
                }
            }
        }
    }
}

void ARTSPlayerController::UpdatePlacementGhost()
{
    if (!PreviewGhostActor) return;

    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(PreviewGhostActor);
    QueryParams.AddIgnoredActor(this);

    FHitResult Hit;
    FVector WorldLoc, WorldDir;
    DeprojectMousePositionToWorld(WorldLoc, WorldDir);
    GetWorld()->LineTraceSingleByChannel(Hit, WorldLoc, WorldLoc + WorldDir * 10000.0f, ECC_Visibility, QueryParams);

    if (Hit.bBlockingHit)
    {
        AGridManager* GridManager = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
        if (GridManager)
        {
            int32 X, Y;
            if (GridManager->WorldToGrid(Hit.Location, X, Y))
            {
                FVector SnapPos = GridManager->GridToWorld(X, Y);

                FVector Origin, BoxExtent;
                PreviewGhostActor->GetActorBounds(false, Origin, BoxExtent);
                float HoverHeight = BoxExtent.Z + 2.0f;
                SnapPos.Z += HoverHeight;

                PreviewGhostActor->SetActorLocation(SnapPos);
            }
        }
    }
}

void ARTSPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindAction("LeftClick", IE_Pressed, this, &ARTSPlayerController::HandleLeftClick);
}