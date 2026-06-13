#include "VectorEffect.h"

#include <Ext/Helper/Scripts.h>
#include <Ext/Helper/Status.h>
#include <Ext/Helper/Physics.h>
#include <Ext/Helper/Weapon.h>
#include <Ext/Helper/FLH.h>

#include <Ext/ObjectType/AttachEffect.h>
#include <Ext/EffectType/AttachEffectScript.h>
#include <Ext/BulletType/BulletStatus.h>

void VectorEffect::OnStart()
{
	if (pTechno && pTechno->WhatAmI() == AbstractType::Building)
	{
		Deactivate();
		return;
	}

	_initialLocation = pObject->GetCoords();
	_elapsedFrames = 0;
	_moveFrame = 0;

	if (Data->TargetRandF.X != 0 || Data->TargetRandF.Y != 0
		|| Data->TargetRandL.X != 0 || Data->TargetRandL.Y != 0
		|| Data->TargetRandH.X != 0 || Data->TargetRandH.Y != 0)
	{
		_randomTargetOffset.X = Random::RandomRanged(Data->TargetRandF.X, Data->TargetRandF.Y);
		_randomTargetOffset.Y = Random::RandomRanged(Data->TargetRandL.X, Data->TargetRandL.Y);
		_randomTargetOffset.Z = Random::RandomRanged(Data->TargetRandH.X, Data->TargetRandH.Y);
	}

	if (pTechno)
	{
		if (Data->StartSpeed > 0)
		{
			_currentSpeed = Data->StartSpeed;
		}
		else
		{
			TechnoTypeClass* pType = pTechno->GetTechnoType();
			if (GetLocoType(pTechno) == LocoType::Jumpjet)
			{
				_currentSpeed = pType->JumpjetSpeed;
			}
			else
			{
				_currentSpeed = pType->Speed;
			}
			if (_currentSpeed <= 0)
			{
				_currentSpeed = pType->Speed;
			}
		}
	}
	else if (pBullet)
	{
		if (Data->StartSpeed > 0)
		{
			_currentSpeed = Data->StartSpeed;
		}
		else
		{
			_currentSpeed = pBullet->Speed;
		}
	}

	// 保存初始 Origin 位置（NoUpdate 模式用）
	if (Data->OriginNoUpdate)
	{
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::Target:
			if (pTechno && pTechno->Target)
				_initialOriginPos = pTechno->Target->GetCoords();
			else if (pBullet)
				_initialOriginPos = pBullet->TargetCoords;
			break;
		case VectorData::VectorOrigin::Launcher:
			if (pBullet && pBullet->Owner)
				_initialOriginPos = pBullet->Owner->GetCoords();
			else if (pTechno)
				_initialOriginPos = pTechno->GetCoords();
			break;
		case VectorData::VectorOrigin::Source:
			if (AE && AE->pSource)
				_initialOriginPos = AE->pSource->GetCoords();
			break;
		default:
			break;
		}
	}

	switch (Data->Origin)
	{
	case VectorData::VectorOrigin::Target:
		if (pTechno && pTechno->Target)
		{
			_initialTarget = pTechno->Target->GetCoords();
		}
		else if (pBullet)
		{
			_initialTarget = pBullet->TargetCoords;
		}
		break;
	case VectorData::VectorOrigin::Launcher:
		if (pBullet)
		{
			_pLauncher = pBullet->Owner;
		}
		else if (pTechno)
		{
			_pLauncher = pTechno;
		}
		break;
	case VectorData::VectorOrigin::Source:
		if (AE && AE->pSource)
		{
			_pSource = AE->pSource;
		}
		break;
	case VectorData::VectorOrigin::FLH:
		if (pTechno)
		{
			CoordStruct unitPos = pTechno->GetCoords();
			DirStruct unitFacing = pTechno->PrimaryFacing.Current();
			_initialOriginPos = GetFLHAbsoluteCoords(unitPos, Data->OriginFLH, unitFacing);
		}
		break;
	default:
		break;
	}

#ifdef DEBUG_AE
	Debug::Log("Vector: [%s] Start, Pos=(%d,%d,%d), Speed=%d\n",
		pObject->GetType()->ID, _initialLocation.X, _initialLocation.Y, _initialLocation.Z, _currentSpeed);
#endif
}

VectorResult VectorEffect::GetVectorResult()
{
	VectorResult result;
	if (Data->Freeze)
	{
		result.Freeze = true;
		// 取第一个 Freeze VectorEffect 的初始位置作为冻结锚点
		if (result.FrozenPos.IsEmpty())
		{
			result.FrozenPos = _initialLocation;
		}
	}
	else
	{
		CoordStruct currentPos = pObject->GetCoords();
		CoordStruct originPos = currentPos; // 默认原点为当前位置，后续根据 Origin 类型调整
		switch (Data->Origin)
		{
		case VectorData::VectorOrigin::World:
		case VectorData::VectorOrigin::Realtime:
			break;
		case VectorData::VectorOrigin::Target:
			originPos = Data->OriginNoUpdate ? _initialOriginPos : _initialTarget;
			break;
		case VectorData::VectorOrigin::Launcher:
			if (Data->OriginNoUpdate)
				originPos = _initialOriginPos;
			else if (_pLauncher && !IsDeadOrInvisible(_pLauncher))
				originPos = _pLauncher->GetCoords();
			break;
		case VectorData::VectorOrigin::Source:
			if (Data->OriginNoUpdate)
				originPos = _initialOriginPos;
			else if (_pSource && !IsDeadOrInvisible(_pSource))
				originPos = _pSource->GetCoords();
			break;
		case VectorData::VectorOrigin::Self:
			originPos = _initialLocation;
			break;
		case VectorData::VectorOrigin::FLH:
			originPos = _initialOriginPos;
			break;
		default:
			break;
		}

		CoordStruct frameTarget;
		frameTarget.X = originPos.X + Data->TargetFLH.X + _randomTargetOffset.X;
		frameTarget.Y = originPos.Y + Data->TargetFLH.Y + _randomTargetOffset.Y;
		frameTarget.Z = originPos.Z + Data->TargetFLH.Z + _randomTargetOffset.Z;

		int speed = _currentSpeed;

		// MoveTo FLH 位移
		CoordStruct moveFlh{};
		moveFlh.X = Data->MoveTo.X + static_cast<int>(Data->GrowRate.X * _moveFrame);
		moveFlh.Y = Data->MoveTo.Y + static_cast<int>(Data->GrowRate.Y * _moveFrame);
		moveFlh.Z = Data->MoveTo.Z + static_cast<int>(Data->GrowRate.Z * _moveFrame);

		// FLH → 世界坐标（只用速度XY分量算朝向，Z轴始终垂直地面）
		CoordStruct moveDisp;
		if (Data->Origin != VectorData::VectorOrigin::World && pBullet)
		{
			double vx = pBullet->Velocity.X;
			double vy = pBullet->Velocity.Y;
			double len = std::sqrt(vx * vx + vy * vy);
			if (len > 1e-6)
			{
				double fwdX = vx / len;
				double fwdY = vy / len;
				moveDisp.X = static_cast<int>(moveFlh.X * fwdX - moveFlh.Y * fwdY);
				moveDisp.Y = static_cast<int>(moveFlh.X * fwdY + moveFlh.Y * fwdX);
				moveDisp.Z = moveFlh.Z;
			}
			else
			{
				moveDisp = moveFlh;
			}
		}
		else
		{
			moveDisp = moveFlh;
		}
		// 朝目标飞行 + MoveTo 偏移
		CoordStruct nextPos;
		if (!frameTarget.IsEmpty() && (Data->TargetFLH.X != 0 || Data->TargetFLH.Y != 0 || Data->TargetFLH.Z != 0))
		{
			CoordStruct dirVec;
			dirVec.X = frameTarget.X - currentPos.X;
			dirVec.Y = frameTarget.Y - currentPos.Y;
			dirVec.Z = frameTarget.Z - currentPos.Z;
			double dirLen = std::sqrt(static_cast<double>(dirVec.X * dirVec.X + dirVec.Y * dirVec.Y + dirVec.Z * dirVec.Z));
			if (dirLen > 1e-6)
			{
				nextPos.X = currentPos.X + static_cast<int>(dirVec.X / dirLen * speed) + moveDisp.X;
				nextPos.Y = currentPos.Y + static_cast<int>(dirVec.Y / dirLen * speed) + moveDisp.Y;
				nextPos.Z = currentPos.Z + static_cast<int>(dirVec.Z / dirLen * speed) + moveDisp.Z;
			}
			else
			{
				nextPos.X = currentPos.X + moveDisp.X;
				nextPos.Y = currentPos.Y + moveDisp.Y;
				nextPos.Z = currentPos.Z + moveDisp.Z;
			}
		}
		else
		{
			nextPos.X = currentPos.X + moveDisp.X;
			nextPos.Y = currentPos.Y + moveDisp.Y;
			nextPos.Z = currentPos.Z + moveDisp.Z;
		}

		// 合并偏移量获得新位置
		result.MoveDisp.X += nextPos.X - currentPos.X;
		result.MoveDisp.Y += nextPos.Y - currentPos.Y;
		result.MoveDisp.Z += nextPos.Z - currentPos.Z;
	}

	return result;
}
