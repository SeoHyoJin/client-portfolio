#include "APStageGameState.h"
#include "Engine/World.h"
#include "TimerManager.h"

#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

#include "../Character/PlayerCharacter.h"
#include "../Base/BtSupportInterface.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

namespace BEM
{
	FVector calcDir(const FVector& src, const FVector& dst, bool excludeZ = false)
	{
		FVector dir = dst - src;
		if (excludeZ)
		{
			dir.Z = 0.f;
		}
		return dir.GetSafeNormal();
	}

	// VFX Evaluation: 타격 위치 및 방향에 따른 이펙트 스폰
	void evaluateHitEffect(USceneComponent* AttachToComponent, const FBattleEffectData& effectData, const FVector& dir)
	{
		if (AttachToComponent != nullptr && effectData.hitEffect != nullptr)
		{
			FRotator tmpRotator = FRotator::ZeroRotator;

			if (effectData.isRandomRotEffect)
			{
				tmpRotator = FRotator(
					effectData.rotX ? FMath::RandRange(0.f, 359.f) : 0.f,
					effectData.rotY ? FMath::RandRange(0.f, 359.f) : 0.f,
					effectData.rotZ ? FMath::RandRange(0.f, 359.f) : 0.f);
			}
			else
			{
				tmpRotator = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, dir);
			}

			effectData.isAttachToTarget ?
				UGameplayStatics::SpawnEmitterAttached(effectData.hitEffect, AttachToComponent, FName(TEXT("None")), FVector::ZeroVector, tmpRotator, effectData.effectScale, EAttachLocation::Type::KeepRelativeOffset) :
				UGameplayStatics::SpawnEmitterAtLocation(AttachToComponent, effectData.hitEffect,
					effectData.isRandomSpawnInBound ? UKismetMathLibrary::RandomPointInBoundingBox(AttachToComponent->GetComponentLocation(), FVector(45.f)) : AttachToComponent->GetComponentLocation(),
					tmpRotator, effectData.effectScale);
		}
	}

	// VFX Overload: Impact Location 기반 스폰
	void evaluateHitEffect(USceneComponent* AttachToComponent, const FBattleEffectData& effectData, const FVector& dir, const FVector& impactLoc)
	{
		if (AttachToComponent != nullptr && effectData.hitEffect != nullptr)
		{
			FRotator tmpRotator = FRotator::ZeroRotator;

			if (effectData.isRandomRotEffect)
			{
				tmpRotator = FRotator(
					effectData.rotX ? FMath::RandRange(0.f, 359.f) : 0.f,
					effectData.rotY ? FMath::RandRange(0.f, 359.f) : 0.f,
					effectData.rotZ ? FMath::RandRange(0.f, 359.f) : 0.f);
			}
			else
			{
				tmpRotator = UKismetMathLibrary::FindLookAtRotation(FVector::ZeroVector, dir);
			}

			effectData.isAttachToTarget ?
				UGameplayStatics::SpawnEmitterAttached(effectData.hitEffect, AttachToComponent, FName(TEXT("None")), impactLoc, tmpRotator, effectData.effectScale,	EAttachLocation::Type::KeepWorldPosition) :
				UGameplayStatics::SpawnEmitterAtLocation(AttachToComponent, effectData.hitEffect,
					effectData.isRandomSpawnInBound ? UKismetMathLibrary::RandomPointInBoundingBox(impactLoc, FVector(45.f)) : impactLoc,
					tmpRotator, effectData.effectScale);
		}
	}

	// SFX Evaluation
	void evaluateSoundEffect(USceneComponent* worldContext, const FSFXData& sfxData)
	{
		if (worldContext != nullptr && sfxData.SFX != nullptr)
		{
			UGameplayStatics::PlaySoundAtLocation(worldContext, sfxData.SFX, worldContext->GetComponentLocation(), sfxData.volume, sfxData.pitch, sfxData.startTime);
		}
	}
}

void AAPStageGameState::BeginPlay()
{
	Super::BeginPlay();

}

void AAPStageGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool AAPStageGameState::onDamageEvent(AActor* causer, AActor* victim,
	UPrimitiveComponent * OtherComp, const TScriptInterface<class IBtSupportInterface>& battleSupport,
	UPARAM(ref) const FBattleData& battleData)
{
	bool result = false;

	if (causer == nullptr || victim == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("battle causer or victim is nullptr"));
		return result;
	} else if (causer == victim)
	{
		UE_LOG(LogTemp, Warning, TEXT("battle causer and victim is equal"));
		return result;
	}

	if (!causer->GetClass()->ImplementsInterface(UCharStateInterface::StaticClass()) ||
		!victim->GetClass()->ImplementsInterface(UCharStateInterface::StaticClass()))
	{
		return false;
	}

	switch (ICharStateInterface::Execute_getCharType(victim))
	{
	case ECharType::PLAYER:
	case ECharType::ENEMY:
	{
		APlayerCharacter* attackingCharacter = Cast<APlayerCharacter>(causer);
		APlayerCharacter* targetCharacter = Cast<APlayerCharacter>(victim);
		if (attackingCharacter == nullptr || targetCharacter == nullptr)
		{
			return false;
		}
		result = onDamageEventOnBattleCharacters(attackingCharacter, targetCharacter, battleData, OtherComp);
	}
		break;
	case ECharType::NEUTRAL:
	{
		if (battleSupport == nullptr) return false;

		const float atkDmg = (ICharStateInterface::Execute_getAtkDamage(causer) * battleData.hitData.dmgRate);

		UGameplayStatics::ApplyDamage(victim, atkDmg, nullptr, causer, nullptr);

		FBtSupportData btSupportData = battleSupport->getBtSupportData();

		FVector dir;

		btSupportData.getTraj().Equals(FVector::ZeroVector) ?
			dir = causer->GetActorForwardVector() :
			dir = btSupportData.getTraj();

		FVector impactPoint;
		IBtSupportInterface::Execute_getImpactPoint(battleSupport.GetObject(), impactPoint);

		if (ICharStateInterface::Execute_checkDead(victim))
		{
			const float V = FMath::Sqrt(FMath::Square(battleData.hitData.forwardImpactPower) + FMath::Square(battleData.hitData.upwardImpactPower));

			ICharStateInterface::Execute_onDestroyMesh(victim, impactPoint, dir, V);
		}

		// VFX
		BEM::evaluateHitEffect(OtherComp, battleData.effectData, dir, impactPoint);

		// SFX
		BEM::evaluateSoundEffect(OtherComp, battleData.effectData.SFXData);

		result = true;
	}
		break;
	}

	return result;
}

/**
 * @brief 캐릭터 간 데미지 처리 및 피격 반응(Hit Reaction) 로직
 * @details 단순 HP 감소 외에 Hit State(Stun, Airborne 등)와 물리적 넉백(Launch)을 처리
 */
bool AAPStageGameState::onDamageEventOnBattleCharacters(APlayerCharacter * causer, APlayerCharacter * victim, const FBattleData & battleData, UPrimitiveComponent * OtherComp)
{
	if (causer == nullptr || victim == nullptr)
	{
		return false;
	}

	// 아군 오사(Friendly Fire) 방지
	if (ICharStateInterface::Execute_getCharType(causer) == ICharStateInterface::Execute_getCharType(victim))
	{
		UE_LOG(LogTemp, Warning, TEXT("Friendly Fire"));
		return false;
	}

	// Affect Status
	UCharStateHandler* vStHandler = victim->getStateHandler();

	if (vStHandler == nullptr) return false;

	// 데미지 공식: (기본 공격력 * 스킬 계수) * (피격자 방어 감소율)
	const float atkDmg = (ICharStateInterface::Execute_getAtkDamage(causer) * battleData.hitData.dmgRate) * (FMath::Clamp(1.f - victim->DmgDecreaseRate, 0.f, 1.f));

	UGameplayStatics::ApplyDamage(victim, atkDmg, nullptr, causer, nullptr);

	const FVector dir = BEM::calcDir(causer->GetActorLocation(), victim->GetActorLocation(), true);

	// --- 상태 판정 및 애니메이션 처리 ---
	if (ICharStateInterface::Execute_checkDead(victim))
	{
		vStHandler->setCurState(ECharState::DEAD, true);

		// 사망 시 날아가는 물리적 힘 적용
		FVector impactVec = (dir * battleData.hitData.forwardImpactPower * 100.f)
			+ (FVector::UpVector * FMath::Clamp(battleData.hitData.upwardImpactPower * 100.f, battleData.hitData.upwardImpactPower, 15000.f));

		ICharStateInterface::Execute_onPlayDead(victim, impactVec);
	}
	else
	{
		EHittedState newHit = EHittedState::END;
		const EHittedState& curHittedState = vStHandler->getCurHittedState();

		// treat diffent to superarmor with groggy.
		if (ICharStateInterface::Execute_checkStun(victim))
		{
			if (curHittedState != EHittedState::STUN)
			{
				vStHandler->setCurHittedState(EHittedState::STUN);
				vStHandler->setCurState(ECharState::HITTED, true);

				newHit = EHittedState::STUN;
			}
		}
		else if (vStHandler->isSuperAmorState == false)
		{
			// 피격 시 공격자를 바라보도록 회전 (Hit Rotation)
			victim->SetActorRotation(FRotator(0.f, UKismetMathLibrary::FindLookAtRotation(victim->GetActorLocation(), causer->GetActorLocation()).Yaw, 0.f));

#pragma region Evaluate Hit (물리적 피격 반응 계산)

			FVector impactVec = dir * battleData.hitData.forwardImpactPower + FVector::UpVector * battleData.hitData.upwardImpactPower;

			// grab atk (잡기)
			if (battleData.hitData.atkType == EAtkType::GRAB)
			{
				if (USkeletalMeshComponent* mesh = victim->GetMesh())
				{
					if (UAnimInstance* animInstance = mesh->GetAnimInstance())
					{
						animInstance->StopAllMontages(0.f);
					}
				}

				vStHandler->setCurHittedState(EHittedState::GRABBED);
				newHit = EHittedState::GRABBED;

			}
			// in air (공중 피격 처리)
			else if (
				vStHandler->getCurState() == ECharState::JUMP ||
				curHittedState == EHittedState::AIR_START ||
				curHittedState == EHittedState::AIR_FALL ||
				curHittedState == EHittedState::DOWN)
			{
				impactVec *= 0.5f;

				switch (battleData.hitData.atkType)
				{
				case EAtkType::NORMAL: victim->LaunchCharacter(impactVec + FVector::UpVector * 300.f, false, true); break;
				case EAtkType::HUGE: victim->LaunchCharacter(impactVec + FVector::UpVector * 400.f, false, true); break;
				case EAtkType::DOWN: victim->LaunchCharacter(impactVec + -FVector::UpVector * 300.f, false, false); break; // 스매시
				case EAtkType::AIRBORNE: victim->LaunchCharacter(impactVec + FVector::UpVector * 600.f, true, true); break; // 띄우기
				}

				newHit = EHittedState::HITONAIR;
				vStHandler->setCurHittedState(EHittedState::AIR_START);
			}
			else
			{
				// 지상 피격 처리
				// 무거우면 못 뜸 (Heavy Armor Logic)
				if (vStHandler->isHeavyArmor)
				{
					impactVec.Z = 0.f;
				}

				switch (battleData.hitData.atkType)
				{
				case EAtkType::NORMAL:
				{
					victim->LaunchCharacter(impactVec, true, false);
					newHit = EHittedState::NORMAL;
				}
				break;
				case EAtkType::HUGE:
				{
					victim->LaunchCharacter(impactVec, false, false);
					newHit = EHittedState::GROGGY;
				}
				break;
				case EAtkType::DOWN:
				case EAtkType::AIRBORNE:
				{
					victim->LaunchCharacter(impactVec, true, true);

					// 최대 그로기 상태로 컷.
					newHit = vStHandler->isHeavyArmor ? EHittedState::GROGGY : EHittedState::AIR_START;
				}
				break;
				default: newHit = EHittedState::NORMAL; break;
				}

				vStHandler->setCurHittedState(newHit);
			}
#pragma endregion

			vStHandler->setCurState(ECharState::HITTED, true);

		}

		ICharStateInterface::Execute_onPlayHittedAnim(victim, newHit);
	}

	/*
	Event causer, victim
	*/
	causer->onGiveHit(victim);
	victim->onGetHitted(causer);

	/*
	Effect & Hit Stop (타격감 연출)
	*/
	// 역경직(Hit Stop) 적용: 타격 순간 시간을 잠시 멈춤
	if (battleData.hitData.shouldHitBack & PlayerGlobal::castEnumToInt8<EHitbackTarget>(EHitbackTarget::Self)) setHitback(causer, battleData.hitData.hitBackTime[static_cast<uint32>(EHitbackTarget::Self)]);
	if (battleData.hitData.shouldHitBack & PlayerGlobal::castEnumToInt8<EHitbackTarget>(EHitbackTarget::Opponent)) setHitback(victim, battleData.hitData.hitBackTime[static_cast<uint32>(EHitbackTarget::Opponent)]);

	// VFX
	BEM::evaluateHitEffect(OtherComp, battleData.effectData, dir);

	// SFX
	BEM::evaluateSoundEffect(OtherComp, battleData.effectData.SFXData);

	// Camera Effect (카메라 쉐이크)
	if (!battleData.effectData.camShakeData.isShakeOnSpawn)
	{
		shakeCam(victim, battleData.effectData.camShakeData);
	}

	return true;
}

/**
 * @brief 역경직(Hit Stop) 타이머 설정
 * @details CustomTimeDilation을 0에 가깝게 조절하여 타격 순간의 임팩트(Freeze Frame) 연출
 */
void AAPStageGameState::setHitback(APlayerCharacter * target, const float time)
{
	if (target == nullptr || time <= 0.f)
	{
		return;
	}

	FString targetName;
	target->GetName(targetName);

	FHitbackPair* pair = hitbackTimerArray.Find(targetName);

	togglePlayingAnimState(target, false); // 시간 정지

	if (pair == nullptr)
	{
		FTimerHandle newTh;
		FTimerDelegate td;
		td.BindUFunction(this, TEXT("clearHitback"), targetName);

		GetWorldTimerManager().SetTimer(newTh,td, time, false);

		hitbackTimerArray.Add(targetName, FHitbackPair(target, newTh));
	}
	else
	{
		// 이미 역경직 중이라면 시간 연장
		FTimerDelegate td;
		td.BindUFunction(this, TEXT("clearHitback"), targetName);
		GetWorldTimerManager().SetTimer(pair->Value, td, time, false);
	}
}

/**
 * @brief 캐릭터 애니메이션 속도 제어 (Time Dilation)
 */
void AAPStageGameState::togglePlayingAnimState(APlayerCharacter* target, const bool isOn)
{
	if (target == nullptr)
	{
		return;
	}

	// isOn=true: 정상 속도(1.0), isOn=false: 정지 상태(0.001)
	target->CustomTimeDilation = isOn ? 1.f : 0.001f;
}

void AAPStageGameState::clearHitback(const FString targetName)
{
	FHitbackPair* pair = hitbackTimerArray.Find(targetName);

	if (pair != nullptr)
	{
		APlayerCharacter* target = pair->Key;

		if (IsValid(target))
		{
			togglePlayingAnimState(target, true); // 정상 속도 복귀
		}
		GetWorldTimerManager().ClearTimer(pair->Value);
		hitbackTimerArray.Remove(targetName);
	}
}

void AAPStageGameState::shakeCam(TWeakObjectPtr<APlayerCharacter> obj, const FCamShakeData& shakeData)
{
	if (obj.IsValid())
	{
		if (shakeData.camShake)
		{
			UGameplayStatics::PlayWorldCameraShake(obj.Get(), shakeData.camShake, obj->GetActorLocation(),
				shakeData.innerRadius, shakeData.OuterRadius, shakeData.Falloff, shakeData.bOrientShakeTowardsEpicenter);
		}
	}
}
