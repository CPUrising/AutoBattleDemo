#pragma once
#include "CoreMinimal.h"
#include "BaseGameEntity.h"
#include "BaseUnit.generated.h"

// 士兵状态机
UENUM()
enum class EUnitState : uint8
{
    Idle,       // 站桩
    Moving,     // 移动中
    Attacking   // 攻击中
};

UCLASS()
class AUTOBATTLEDEMO_API ABaseUnit : public ABaseGameEntity
{
    GENERATED_BODY()

public:
    ABaseUnit();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // --- 供 GameMode 调用 ---
    UFUNCTION(BlueprintCallable)
        void SetUnitActive(bool bActive);

    // --- 战斗属性 ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
        float AttackRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
        float Damage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
        float AttackInterval;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
        float MoveSpeed;

    // --- 组件 ---
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class UCapsuleComponent* CapsuleComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
        class UStaticMeshComponent* MeshComp;

protected:
    // --- 核心AI逻辑（可被子类重写） ---

    // 寻找目标（子类可重写改变优先级）
    virtual AActor* FindClosestEnemyBuilding();

    // 请求路径
    void RequestPathToTarget();

    // 沿路径移动
    void MoveAlongPath(float DeltaTime);

    // 执行攻击（子类可重写改变攻击方式）
    virtual void PerformAttack();

    // 当前状态
    EUnitState CurrentState;

    // 路径缓存
    TArray<FVector> PathPoints;
    int32 CurrentPathIndex;

    // 当前目标
    UPROPERTY()
        AActor* CurrentTarget;

    // 攻击计时
    float LastAttackTime;

    // GridManager 引用
    class AGridManager* GridManagerRef;

    // AI 激活状态
    bool bIsActive;
};