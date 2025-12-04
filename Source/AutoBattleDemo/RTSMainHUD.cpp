#include "RTSMainHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "RTSPlayerController.h"
#include "RTSGameMode.h"
#include "RTSGameInstance.h"

void URTSMainHUD::NativeConstruct()
{
    Super::NativeConstruct();

    // 绑定点击事件
    if (Btn_BuySoldier)
    {
        Btn_BuySoldier->OnClicked.AddDynamic(this, &URTSMainHUD::OnClickBuySoldier);
    }
    if (Btn_BuyArcher)
    {
        Btn_BuyArcher->OnClicked.AddDynamic(this, &URTSMainHUD::OnClickBuyArcher);
    }
    if (Btn_StartBattle)
    {
        Btn_StartBattle->OnClicked.AddDynamic(this, &URTSMainHUD::OnClickStartBattle);
    }
}

void URTSMainHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // 实时更新金币显示
    // 这种写法虽然不是最高效(每帧调)，但是最稳妥，防止金币变了UI没变
    URTSGameInstance* GI = Cast<URTSGameInstance>(GetGameInstance());
    if (GI && Text_GoldInfo)
    {
        FString GoldStr = FString::Printf(TEXT("Gold: %d"), GI->PlayerGold);
        Text_GoldInfo->SetText(FText::FromString(GoldStr));
    }
}

void URTSMainHUD::OnClickBuySoldier()
{
    // 获取 Controller，告诉它我要买步兵
    ARTSPlayerController* PC = Cast<ARTSPlayerController>(GetOwningPlayer());
    if (PC)
    {
        PC->OnSelectUnitToPlace(EUnitType::Soldier);
    }
}

void URTSMainHUD::OnClickBuyArcher()
{
    // 告诉 Controller 我要买弓箭手
    ARTSPlayerController* PC = Cast<ARTSPlayerController>(GetOwningPlayer());
    if (PC)
    {
        PC->OnSelectUnitToPlace(EUnitType::Archer);
    }
}

void URTSMainHUD::OnClickStartBattle()
{
    // 获取 GameMode，开始战斗
    ARTSGameMode* GM = Cast<ARTSGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        GM->StartBattlePhase();

        // 隐藏开始按钮，防止重复点击
        if (Btn_StartBattle)
        {
            Btn_StartBattle->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}