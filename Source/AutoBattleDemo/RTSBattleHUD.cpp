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
	ARTSGameMode* GM = Cast<ARTSGameMode>(UGameplayStatics::GetGameMode(this));
	if (GM)
	{
		GM->ReturnToBase();
	}
}