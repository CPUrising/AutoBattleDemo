#include "RTSBattleHUD.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "RTSGameMode.h"

void URTSBattleHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Retreat)
	{
		Btn_Retreat->OnClicked.AddDynamic(this, &URTSBattleHUD::OnClickRetreat);
	}
}

void URTSBattleHUD::OnClickRetreat()
{
	// 撤退逻辑：直接加载回基地地图
	// 基地地图叫 "PlayerBase"
	UGameplayStatics::OpenLevel(this, FName("PlayerBase"));
}