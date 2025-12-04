// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseGameEntity.h"

// Sets default values
ABaseGameEntity::ABaseGameEntity()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ABaseGameEntity::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ABaseGameEntity::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

// Called to bind functionality to input
void ABaseGameEntity::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

}

