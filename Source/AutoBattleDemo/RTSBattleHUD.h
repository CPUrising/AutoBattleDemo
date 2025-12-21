#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RTSBattleHUD.generated.h"

class UButton;
class UTextBlock;

UCLASS()
class AUTOBATTLEDEMO_API URTSBattleHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	// --- 战斗界面只需要这几个控件 ---

	UPROPERTY(meta = (BindWidget))
		UButton* Btn_Retreat; // 撤退按钮

	//UPROPERTY(meta = (BindWidget))
	//	UTextBlock* Text_TimeInfo; // (可选) 倒计时

	UFUNCTION()
		void OnClickRetreat();
};