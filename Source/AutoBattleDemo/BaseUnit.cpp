#include "BaseUnit.h"
#include "BaseBuilding.h"
#include "GridManager.h"
#include "Kismet/GameplayStatics.h"

#include "EngineUtils.h"
#include "DrawDebugHelpers.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"

ABaseUnit::ABaseUnit()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 创建胶囊体 (根组件)
    CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
    CapsuleComp->SetupAttachment(RootComponent);
    CapsuleComp->InitCapsuleSize(40.0f, 90.0f);
    CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));

    // 2. 创建模型
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CapsuleComp);
    // 保持为 0，具体偏移在蓝图里调
    MeshComp->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

    // 榛樿灞炴��
    MaxHealth = 100.0f;
    AttackRange = 150.0f;
    Damage = 10.0f;
    MoveSpeed = 300.0f;
    AttackInterval = 1.0f;

    UnitType = EUnitType::Barbarian; // 榛樿閲庤洰浜�
    CurrentState = EUnitState::Idle;
    LastAttackTime = 0.0f;
    CurrentPathIndex = 0;
    CurrentTarget = nullptr;
    GridManagerRef = nullptr;
    bIsActive = false;

    TeamID = ETeam::Player;
    bIsTargetable = true; // [修改] 兵当然可以被攻击
}

void ABaseUnit::BeginPlay()
{
    Super::BeginPlay();

    // 1. 获取 GridManager
    for (TActorIterator<AGridManager> It(GetWorld()); It; ++It)
    {
        GridManagerRef = *It;
        break;
    }

    if (!GridManagerRef)
    {
        // 只是警告
        // UE_LOG(LogTemp, Error, TEXT("[Unit] %s cannot find GridManager!"), *GetName());
    }

    // 2. 自动激活逻辑 (针对手动放置在战场的敌人)
    // 如果我是敌人，且地图是战场，那我就不需要 GameMode 来激活我，我自己激活自己
    FString MapName = GetWorld()->GetMapName();
    if (MapName.Contains("BattleField") && TeamID == ETeam::Enemy)
    {
        SetUnitActive(true);
    }

    // 记录模型的初始位置（攻击冲撞动画用）
    if (MeshComp)
    {
        OriginalMeshOffset = MeshComp->GetRelativeLocation();
    }
    bIsLunging = false;

    // 随机化移动速度 (+/- 15%)
    MoveSpeed *= FMath::RandRange(0.85f, 1.15f);
}

void ABaseUnit::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsActive) return;

    switch (CurrentState)
    {
    case EUnitState::Idle:
        if (!CurrentTarget)
        {
            CurrentTarget = FindClosestTarget();
            if (CurrentTarget && GridManagerRef)
            {
                RequestPathToTarget();
                if (PathPoints.Num() > 0) CurrentState = EUnitState::Moving;
            }
        }
        else
        {
            // 检查目标有效性
            ABaseGameEntity* TargetEntity = Cast<ABaseGameEntity>(CurrentTarget);
            if (!TargetEntity || TargetEntity->CurrentHealth <= 0) CurrentTarget = nullptr;
        }
        break;

    case EUnitState::Moving:
        // 在 Moving 状态下，真正的移动逻辑在下面统一处理
        // 这里只负责检查是否需要切换状态
        if (CurrentTarget)
        {
            float Dist = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());

            float ActualDistToTarget = 0.0f;

            // 使用物理碰撞表面的最近点来计算距离
            FVector MyLoc = GetActorLocation();
            FVector TargetLoc = CurrentTarget->GetActorLocation();

            // 尝试找目标的碰撞体 (PrimitiveComponent)
            UPrimitiveComponent* TargetPrim = Cast<UPrimitiveComponent>(CurrentTarget->GetRootComponent());
            if (!TargetPrim)
            {
                // 如果根组件不是碰撞体，尝试找 Mesh
                TargetPrim = CurrentTarget->FindComponentByClass<UStaticMeshComponent>();
            }

            if (TargetPrim)
            {
                FVector ClosestPoint;
                // 计算我离目标表面最近的点
                TargetPrim->GetClosestPointOnCollision(MyLoc, ClosestPoint);

                // 抹平 Z 轴差异 (只算水平距离)
                ClosestPoint.Z = MyLoc.Z;

                ActualDistToTarget = FVector::Dist(MyLoc, ClosestPoint);
            }
            else
            {
                // 保底逻辑：如果目标没碰撞体，回退到原来的半径算法
                float DistCenter = FVector::Dist(MyLoc, TargetLoc);
                FVector Origin, BoxExtent;
                CurrentTarget->GetActorBounds(false, Origin, BoxExtent);
                float Radius = FMath::Max(BoxExtent.X, BoxExtent.Y);
                ActualDistToTarget = FMath::Max(0.0f, DistCenter - Radius);
            }

            // [判定] 只要走进 (攻击距离 + 目标半径) 的范围内，就开始打！
            // 这样兵就不会试图走进塔的身体里了
            if (Dist <= (AttackRange + 10.0f))
            {
                CurrentState = EUnitState::Attacking;
                PathPoints.Empty();

                // 停止物理移动，防止滑步
                // FinalVelocity = FVector::ZeroVector; (这一步在下面自动处理了)
            }
        }
        else
        {
            // 目标丢了，停下来
            CurrentState = EUnitState::Idle;
        }
        break;

    case EUnitState::Attacking:
        PerformAttack();
        break;
    }

    // [重写] 强力防重叠移动逻辑 (Anti-Blob Movement)
    
    FVector FinalVelocity = FVector::ZeroVector;
    FVector CurrentLoc = GetActorLocation();

    // 1. 计算【寻路引力】 (Seek Force)
    // 只有在 Moving 状态才产生向前的力
    if (CurrentState == EUnitState::Moving && PathPoints.Num() > 0 && CurrentPathIndex < PathPoints.Num())
    {
        FVector TargetPoint = PathPoints[CurrentPathIndex];
        TargetPoint.Z = CurrentLoc.Z; // 抹平高度

        FVector SeekDir = (TargetPoint - CurrentLoc).GetSafeNormal();

        // 检查到达
        if (FVector::DistSquared2D(CurrentLoc, TargetPoint) < 900.0f) // 30cm
        {
            CurrentPathIndex++;
        }

        // 基础移动力
        FinalVelocity += SeekDir * MoveSpeed;
    }

    // 2. 计算【强力避让】 (Hard Separation)
    // 无论什么状态，只要重叠了，就必须推开！
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // 搜索半径设为 80 (比胶囊体稍大，形成安全气囊)
    bool bHit = GetWorld()->OverlapMultiByChannel(
        Overlaps, CurrentLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(80.0f), Params
    );

    if (bHit)
    {
        FVector SeparationVec = FVector::ZeroVector;
        int32 SeparationCount = 0;

        for (const FOverlapResult& Res : Overlaps)
        {
            ABaseUnit* OtherUnit = Cast<ABaseUnit>(Res.GetActor());
            // 只推开活着的单位 (包括敌人)
            if (OtherUnit && OtherUnit->CurrentHealth > 0)
            {
                FVector Dir = CurrentLoc - OtherUnit->GetActorLocation();
                Dir.Z = 0; // 绝对不要在垂直方向产生力！

                float Dist = Dir.Size();

                // 如果距离太近 (重叠了)
                if (Dist < 80.0f)
                {
                    // 距离越近，推力呈指数级增长
                    // 如果 Dist 是 1，推力就是 80
                    // 如果 Dist 是 80，推力就是 1
                    float PushStrength = (80.0f - Dist) / (Dist + 0.1f); // +0.1防止除以0

                    // 这里的 5000.0f 是一个巨大的系数，确保推力远大于寻路力
                    SeparationVec += Dir.GetSafeNormal() * PushStrength * 5000.0f;
                    SeparationCount++;
                }
            }
        }

        // 如果发生了重叠，叠加这个巨大的推力
        if (SeparationCount > 0)
        {
            FinalVelocity += SeparationVec;
        }
    }

    // 3. 执行移动
    // 使用 AddActorWorldOffset 并开启 Sweep，防止穿墙
    if (!FinalVelocity.IsNearlyZero())
    {
        // 限制最大速度，防止飞出去
        // 正常移动速度是 300，这里允许瞬间被推开的速度稍微快一点(比如600)，但不允许无限大
        FinalVelocity = FinalVelocity.GetClampedToMaxSize(MoveSpeed * 2.0f);

        // 抹除 Z 轴，最后一道防线
        FinalVelocity.Z = 0.0f;

        FHitResult MoveHit;
        AddActorWorldOffset(FinalVelocity * DeltaTime, true, &MoveHit);

        // 如果被墙挡住了，把剩余的速度沿着墙面滑行 (SlideAlongSurface)
        if (MoveHit.IsValidBlockingHit())
        {
            FVector Normal = MoveHit.Normal;
            FVector SlideDir = FVector::VectorPlaneProject(FinalVelocity, Normal);
            AddActorWorldOffset(SlideDir * DeltaTime, true);
        }

        // 旋转朝向 (只在有明显位移时转头)
        if (FinalVelocity.SizeSquared() > 100.0f)
        {
            FRotator TargetRot = FinalVelocity.Rotation();
            FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, 10.0f);
            SetActorRotation(NewRot);
        }
    }

    // --- 处理攻击动画 ---
    if (bIsLunging && MeshComp)
    {
        LungeTimer += DeltaTime * LungeSpeed;

        // 使用 Sin 函数模拟：0 -> 1 -> 0 (出去再回来)
        // PI 是半个圆周，正好是从 0 到 峰值 再回 0
        float Alpha = FMath::Sin(LungeTimer);

        if (LungeTimer >= PI) // 动画结束 (Sin(PI) = 0)
        {
            bIsLunging = false;
            LungeTimer = 0.0f;
            MeshComp->SetRelativeLocation(OriginalMeshOffset); // 归位
        }
        else
        {
            // 向前移动 (X轴是前方)
            FVector ForwardOffset = FVector(Alpha * LungeDistance, 0.f, 0.f);
            MeshComp->SetRelativeLocation(OriginalMeshOffset + ForwardOffset);
        }
    }
}

void ABaseUnit::SetUnitActive(bool bActive)
{
    bIsActive = bActive;

    if (bActive)
    {
        CurrentState = EUnitState::Idle;
        UE_LOG(LogTemp, Warning, TEXT("[Unit] %s AI ACTIVATED!"), *GetName());
    }
    else
    {
        CurrentState = EUnitState::Idle;
        CurrentTarget = nullptr;
        PathPoints.Empty();
        UE_LOG(LogTemp, Warning, TEXT("[Unit] %s AI DEACTIVATED"), *GetName());
    }
}

AActor* ABaseUnit::FindClosestTarget()
{
    AActor* ClosestActor = nullptr;
    float ClosestDistance = FLT_MAX;

    // 1. 获取所有实体 (包括兵和建筑)
    TArray<AActor*> AllEntities;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseGameEntity::StaticClass(), AllEntities);

    for (AActor* Actor : AllEntities)
    {
        ABaseGameEntity* Entity = Cast<ABaseGameEntity>(Actor);

        // 过滤条件：
        // 1. 必须存在
        // 2. 必须是敌人 (TeamID 不同)
        // 3. 必须活着
        // 4. 必须可被攻击 (bIsTargetable)
        if (Entity &&
            Entity->TeamID != this->TeamID &&
            Entity->CurrentHealth > 0 &&
            Entity->bIsTargetable)
        {
            float Distance = FVector::Dist(GetActorLocation(), Entity->GetActorLocation());

            // 计算体积半径 (Mesh 优先)
            float TargetRadius = 0.0f;
            UStaticMeshComponent* TargetMesh = Entity->FindComponentByClass<UStaticMeshComponent>();

            if (TargetMesh)
            {
                // 如果有 Mesh，用 Mesh 的边界
                FVector BoxExtent = TargetMesh->Bounds.BoxExtent;
                TargetRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
            }
            else
            {
                // 如果没有 Mesh，回退到 Actor 边界 (参数设为 false，只算根组件)
                FVector Origin, BoxExtent;
                Entity->GetActorBounds(false, Origin, BoxExtent);
                TargetRadius = FMath::Max(BoxExtent.X, BoxExtent.Y);
            }

            // 使用边缘距离判定
            if (Distance <= (AttackRange + TargetRadius))
            {
                return Entity; // 就在脸上了，直接打
            }

            if (Distance < ClosestDistance)
            {
                ClosestDistance = Distance;
                ClosestActor = Entity;
            }
        }
    }

    return ClosestActor;
}

void ABaseUnit::RequestPathToTarget()
{
    if (!GridManagerRef)
    {
        GridManagerRef = Cast<AGridManager>(UGameplayStatics::GetActorOfClass(GetWorld(), AGridManager::StaticClass()));
    }

    if (!CurrentTarget || !GridManagerRef)
    {
        // UE_LOG(LogTemp, Error, TEXT("[Unit] %s cannot request path!"), *GetName());
        return;
    }

    FVector StartPos = GetActorLocation();
    FVector EndPos = CurrentTarget->GetActorLocation();

    PathPoints = GridManagerRef->FindPath(StartPos, EndPos);
    CurrentPathIndex = 0;

    UE_LOG(LogTemp, Log, TEXT("[Unit] %s path: %d waypoints"), *GetName(), PathPoints.Num());

    /*调试画线
    if (PathPoints.Num() > 1)
    {
        for (int32 i = 0; i < PathPoints.Num() - 1; i++)
        {
            DrawDebugLine(GetWorld(), PathPoints[i], PathPoints[i + 1],
                FColor::Cyan, false, 3.0f, 0, 3.0f);
        }
    }
    */

    // 路径抖动 (Path Jitter)
    // 给每个路径点加一个随机偏移，让大家不要走成一条直线
    if (PathPoints.Num() > 0)
    {
        // 生成一个固定的随机种子，或者针对每个兵生成不同的偏移
        // 偏移量不要太大，最好小于格子大小的一半 (GridSize 100 -> Offset < 50)
        float JitterAmount = 40.0f;

        for (int32 i = 0; i < PathPoints.Num() - 1; i++) // 最后一个点(终点)通常不偏移，保证能走到目标跟前
        {
            float RandomX = FMath::RandRange(-JitterAmount, JitterAmount);
            float RandomY = FMath::RandRange(-JitterAmount, JitterAmount);

            PathPoints[i].X += RandomX;
            PathPoints[i].Y += RandomY;

            // Z轴保持不变，防止入土
        }
    }

    // 简单的路径平滑：如果第一个点就在脚下，直接跳过
    if (PathPoints.Num() > 1 && FVector::DistSquared(PathPoints[0], GetActorLocation()) < 2500.0f)
    {
        CurrentPathIndex = 1;
    }
}

//void ABaseUnit::MoveAlongPath(float DeltaTime)
//{
//    if (PathPoints.Num() == 0 || CurrentPathIndex >= PathPoints.Num()) { CurrentState = EUnitState::Idle; return; }
//    if (!CurrentTarget || CurrentTarget->IsPendingKill()) { CurrentState = EUnitState::Idle; CurrentTarget = nullptr; PathPoints.Empty(); return; }
//
//    FVector TargetPoint = PathPoints[CurrentPathIndex];
//    FVector CurrentLocation = GetActorLocation();
//
//    // 1. 抹平 Z 轴
//    TargetPoint.Z = CurrentLocation.Z;
//
//    // 2. 计算方向
//    FVector Direction = (TargetPoint - CurrentLocation).GetSafeNormal();
//
//    // [关键] 强制 Z 为 0，绝对防止钻地
//    Direction.Z = 0.0f;
//
//    // 3. 物理移动
//    FHitResult Hit;
//    AddActorWorldOffset(Direction * MoveSpeed * DeltaTime, true, &Hit);
//
//    // 4. 撞墙处理
//    if (Hit.bBlockingHit)
//    {
//        AActor* HitActor = Hit.GetActor();
//        ABaseUnit* HitUnit = Cast<ABaseUnit>(HitActor);
//        // 如果撞到敌人，且我是普通兵，打它
//        if (HitUnit && HitUnit->TeamID != this->TeamID && HitUnit->CurrentHealth > 0)
//        {
//            if (UnitType != EUnitType::Giant && UnitType != EUnitType::Bomber)
//            {
//                CurrentTarget = HitUnit;
//                CurrentState = EUnitState::Attacking;
//                PathPoints.Empty();
//                return;
//            }
//        }
//    }
//
//    // 5. 旋转
//    if (!Direction.IsNearlyZero())
//    {
//        FRotator NewRot = FMath::RInterpTo(GetActorRotation(), Direction.Rotation(), DeltaTime, 10.0f);
//        SetActorRotation(NewRot);
//    }
//
//    // 6. [修复] 到达判定
//    float DistanceToPoint = FVector::DistSquared2D(CurrentLocation, TargetPoint);
//
//    // 如果是脱困模式，必须走得准一点 (10cm)，防止还没走出包围圈就停了
//    // 正常模式可以宽一点 (30cm)
//    float AcceptanceRadiusSq = bIsUnstucking ? 100.0f : 900.0f;
//
//    if (DistanceToPoint < AcceptanceRadiusSq)
//    {
//        CurrentPathIndex++;
//
//        // 路径走完了，判断是否能打到目标
//        if (CurrentPathIndex >= PathPoints.Num())
//        {
//            if (CurrentTarget)
//            {
//                float Distance = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
//                if (Distance <= AttackRange + 200.0f)
//                {
//                    CurrentState = EUnitState::Attacking;
//                    UE_LOG(LogTemp, Warning, TEXT("[Unit] %s path complete, starting attack!"), *GetName());
//                }
//                else
//                {
//                    // 还没走到，重新寻路
//                    RequestPathToTarget();
//                    if (PathPoints.Num() == 0)
//                    {
//                        CurrentState = EUnitState::Idle;
//                    }
//                }
//            }
//            else
//            {
//                CurrentState = EUnitState::Idle;
//            }
//        }
//    }
//}

void ABaseUnit::PerformAttack()
{
    if (!CurrentTarget)
    {
        CurrentState = EUnitState::Idle;
        return;
    }

    float ActualDistToTarget = FLT_MAX;
    UPrimitiveComponent* TargetPrim = Cast<UPrimitiveComponent>(CurrentTarget->GetRootComponent());
    if (!TargetPrim) TargetPrim = CurrentTarget->FindComponentByClass<UStaticMeshComponent>();

    if (TargetPrim)
    {
        FVector ClosestPoint;
        TargetPrim->GetClosestPointOnCollision(GetActorLocation(), ClosestPoint);
        ClosestPoint.Z = GetActorLocation().Z;
        ActualDistToTarget = FVector::Dist(GetActorLocation(), ClosestPoint);
    }
    else
    {
        // 保底逻辑...
        float DistCenter = FVector::Dist(GetActorLocation(), CurrentTarget->GetActorLocation());
        FVector Origin, BoxExtent;
        CurrentTarget->GetActorBounds(false, Origin, BoxExtent);
        ActualDistToTarget = FMath::Max(0.0f, DistCenter - FMath::Max(BoxExtent.X, BoxExtent.Y));
    }

    // 宽松的攻击判定 (增加 50.0f 的缓冲，防止并在边缘鬼畜切换状态)
    if (ActualDistToTarget > (AttackRange + 50.0f))
    {
        RequestPathToTarget();
        if (PathPoints.Num() > 0)
        {
            CurrentState = EUnitState::Moving;
        }
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastAttackTime >= AttackInterval)
    {
        FDamageEvent DamageEvent;
        CurrentTarget->TakeDamage(Damage, DamageEvent, nullptr, this);
        LastAttackTime = CurrentTime;

        FVector Direction = (CurrentTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        if (!Direction.IsNearlyZero())
        {
            FRotator NewRotation = Direction.Rotation();
            NewRotation.Pitch = 0;
            NewRotation.Roll = 0;
            SetActorRotation(NewRotation);

            // 触发冲撞动画
            bIsLunging = true;
            LungeTimer = 0.0f;
        }

        UE_LOG(LogTemp, Log, TEXT("[Unit] %s attacked %s for %f damage!"),
            *GetName(), *CurrentTarget->GetName(), Damage);
    }
}

