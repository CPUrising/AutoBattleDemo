#include "BaseBuilding.h"
#include "Components/StaticMeshComponent.h"

ABaseBuilding::ABaseBuilding()
{
    PrimaryActorTick.bCanEverTick = true;

    // 创建模型组件
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingMesh"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    MeshComp->SetCollisionObjectType(ECC_WorldStatic);
    MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
    MeshComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

    // 默认属性
    MaxHealth = 500.0f;
    TeamID = ETeam::Enemy;
    bIsTargetable = true;

    BuildingType = EBuildingType::Other;
    BuildingLevel = 1;
    MaxLevel = 5;

    BaseUpgradeGoldCost = 100;
    BaseUpgradeElixirCost = 50;
}

void ABaseBuilding::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("[Building] %s | Type: %d | Level: %d"),
        *GetName(), (int32)BuildingType, BuildingLevel);
}

void ABaseBuilding::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ABaseBuilding::LevelUp()
{
    if (!CanUpgrade())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Building] %s cannot upgrade (Max level reached)"), *GetName());
        return;
    }

    BuildingLevel++;

    // 应用升级加成
    ApplyLevelUpBonus();

    UE_LOG(LogTemp, Warning, TEXT("[Building] %s upgraded to Level %d | HP: %f"),
        *GetName(), BuildingLevel, MaxHealth);

    // 恢复满血
    CurrentHealth = MaxHealth;
}

void ABaseBuilding::GetUpgradeCost(int32& OutGold, int32& OutElixir)
{
    // 升级费用随等级递增
    OutGold = BaseUpgradeGoldCost * BuildingLevel;
    OutElixir = BaseUpgradeElixirCost * BuildingLevel;
}

bool ABaseBuilding::CanUpgrade() const
{
    return BuildingLevel < MaxLevel;
}

void ABaseBuilding::ApplyLevelUpBonus()
{
    // 每级提升 20% 血量
    MaxHealth *= 1.2f;
    CurrentHealth = MaxHealth;
}

void ABaseBuilding::NotifyActorOnClicked(FKey ButtonPressed)
{
    Super::NotifyActorOnClicked(ButtonPressed);

    // 调试：点击显示建筑信息
    UE_LOG(LogTemp, Warning, TEXT("[Building] Clicked: %s | Level: %d | HP: %f/%f"),
        *GetName(), BuildingLevel, CurrentHealth, MaxHealth);

    if (GEngine)
    {
        FString Msg = FString::Printf(TEXT("%s | Lv.%d | HP: %.0f/%.0f"),
            *GetName(), BuildingLevel, CurrentHealth, MaxHealth);
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
    }
}