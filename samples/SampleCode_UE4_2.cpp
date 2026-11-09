/**
 * @brief 캐릭터의 지상 회전(Grounded Rotation) 및 로코모션 제어 로직
 * @details MovementAction, 속도, 조준 모드에 따라 캐릭터의 회전을 갱신합니다.
 */
void PlayerLocomotionComponent::UpdateGroundedRotation(float _DeltaTime)
{
	if (Player == nullptr)
	{
		return;
	}

	if (MovementAction == EMovementAction::None)
	{
		// 이동 중이거나 일정 속도 이상일 때 회전 갱신 (RootMotion 사용 시 제외)
		const bool canUpdateMovingRot = (IsMoving && HasMovementInput) || ((150.f < Speed) && (Player->HasAnyRootMotion() == false));
		if (canUpdateMovingRot)
		{
			// --- 캐릭터 이동 시 회전 처리 (Locomotion Rotation) ---
			const float groundedRotationRate = CalculateGroundedRotationRate();

			// 1. 이동 방향으로 자동 회전 (AutoMoving)
			if (RotationMode == ERotationMode::AutoMoving)
			{
				SmoothCharacterRotation({ 0.0f, LastVelocityRotation.Yaw, 0.0f }, 800.0f, groundedRotationRate, _DeltaTime, true);
			}
			// 2. 카메라가 보는 방향으로 회전 (LookingDirection - 전투/탐색)
			else if (RotationMode == ERotationMode::LookingDirection)
			{
				float yawValue;
				if (Gait == EGait::Running)
				{
					yawValue = LastVelocityRotation.Yaw; // 달릴 때는 속도 방향 우선
				}
				else
				{
					// AnimCurve(YawOffset)를 반영하여 자연스러운 회전 구현 (Strafe 등)
					const float yawOffsetCurveVal = GetAnimCurveValue(NAME_YawOffset);
					yawValue = AimingRotation.Yaw + yawOffsetCurveVal;
				}

				SmoothCharacterRotation(FRotator(0.f, yawValue, 0.f), 500.f, groundedRotationRate, _DeltaTime);
			}
			// 3. 조준 모드 (Aiming) - 정밀 회전 필요
			else if (RotationMode == ERotationMode::Aiming || RotationMode == ERotationMode::HipAiming)
			{
				const float controlYaw = AimingRotation.Yaw;
				SmoothCharacterRotation(FRotator(0.f, controlYaw, 0.f), 1000.f, 20.f, _DeltaTime);
			}
			// 4. 스코프 조준 (ScopeAiming) - 즉각적인 회전 반영
			else if (RotationMode == ERotationMode::ScopeAiming)
			{
				// ScopeAiming일 경우 카메라 회전값 캐릭터에 바로 적용
				const float controlYaw = AimingRotation.Yaw;
				SmoothCharacterRotation(FRotator(0.0f, controlYaw, 0.0f), 0.0f, 0.0f, _DeltaTime);
			}
		}
		else
		{
			// --- 캐릭터 정지 시 회전 처리 (Turn In Place) ---

			// Spine 회전 허용 시, 상체와 하체 각도가 많이 벌어지면 하체를 강제로 회전
			if (0.99f <= GetAnimCurveValue(TEXT("Enable_SpineRotation")))
			{
				LimitRotation(-100.f, 100.f, 20.f, _DeltaTime);
			}

			// 애니메이션 커브(RotationAmount)를 이용한 절차적 회전 (Procedural Rotation)
			// 30fps 애니메이션 데이터 기반으로 프레임 보정 수행
			const float rotAmountCurve = GetAnimCurveValue(NAME_RotationAmount);

			if (0.001f < FMath::Abs(rotAmountCurve))
			{
				if (Player->GetLocalRole() == ROLE_AutonomousProxy)
				{
					// 30프레임 애니메이션 기준으로 프레임별로 캐릭터 회전 수행한다.
					TargetRotation.Yaw = UKismetMathLibrary::NormalizeAxis(TargetRotation.Yaw + (rotAmountCurve * (_DeltaTime / (1.f / 30.f))));
					Player->SetActorRotation(TargetRotation);
				}
				else
				{
					Player->AddActorWorldRotation(FRotator(0.f, rotAmountCurve * (_DeltaTime / (1.f / 30.f)), 0.f));
				}

				TargetRotation = Player->GetActorRotation();
			}

			if (RotationMode == ERotationMode::ScopeAiming)
			{
				const float controlYaw = AimingRotation.Yaw;
				SmoothCharacterRotation(FRotator(0.f, controlYaw, 0.f), 0.f, 0.f, _DeltaTime);
			}
		}
	}
}

/**
 * @brief 캐릭터 회전 보간 처리 (Interpolation)
 * @details 목표 회전과 액터 회전을 단계적으로 보간합니다.
 */
void PlayerLocomotionComponent::SmoothCharacterRotation(FRotator _Target, float _TargetInterpSpeed, float _ActorInterpSpeed, float _DeltaTime, bool IsAutoMoving)
{
	if (Player == nullptr)
	{
		return;
	}

	// 목표 회전값 보간 (Interpolation)
	TargetRotation = FMath::RInterpConstantTo(TargetRotation, _Target, _DeltaTime, _TargetInterpSpeed);
	// 실제 액터 회전 적용 (현재 회전 -> 목표 회전 부드럽게 보간)
	Player->SetActorRotation(FMath::RInterpTo(Player->GetActorRotation(), TargetRotation, _DeltaTime, _ActorInterpSpeed));

	if (IsAutoMoving)
	{
		APlayerController* PlayerController = Player->GetController<APlayerController>();
		if (PlayerController)
		{
			// 컨트롤러 회전도 동기화하여 카메라와 캐릭터의 일치감 형성
			PlayerController->SetControlRotation(FMath::RInterpTo(PlayerController->GetControlRotation(), TargetRotation, _DeltaTime, _ActorInterpSpeed));
		}
	}
}

void PlayerLocomotionComponent::LimitRotation(float _AimYawMin, float _AimYawMax, float _InterpSpeed, float _DeltaTime)
{
	if (Player == nullptr)
	{
		return;
	}

	// Enable_SpineRotation 커브값이 1일때 카메라와 캐릭터의 방향이 일정 각도 이상 벌어지면
	// 캐릭터를 강제로 회전시킨다. (몸이 꼬이는 현상 방지)

	FRotator delta = AimingRotation - Player->GetActorRotation();
	delta.Normalize();
	const float rangeVal = delta.Yaw;

	if (rangeVal < _AimYawMin || _AimYawMax < rangeVal)
	{
		const float controlRotYaw = AimingRotation.Yaw;
		const float targetYaw = controlRotYaw + (0.f < rangeVal ? _AimYawMin : _AimYawMax);
		SmoothCharacterRotation(FRotator(0.f, targetYaw, 0.f), 0.f, _InterpSpeed, _DeltaTime);
	}
}
