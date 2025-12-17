#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "RTSCoreTypes.h"
#include "BaseGameEntity.generated.h"

// 所有游戏实体的基类（兵种和建筑的共同父类）
UCLASS()
class AUTOBATTLEDEMO_API ABaseGameEntity : public APawn
{
    GENERATED_BODY()

public:
    ABaseGameEntity();

    virtual void BeginPlay() override;

    // --- 核心属性 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
        float MaxHealth;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stats")
        float CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
        ETeam TeamID;

    // 标记：是否可以被攻击
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
        bool bIsTargetable = true;

    // --- 接口 ---
    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
        class AController* EventInstigator, AActor* DamageCauser) override;

    virtual void Die();

    // 虚函数：子类可以重写死亡逻辑
    UFUNCTION(BlueprintNativeEvent, Category = "Entity")
        void OnDeath();
    virtual void OnDeath_Implementation();
};