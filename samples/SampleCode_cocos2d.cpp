#include "mgBoard.h"
#include "mgDataManager.h"
#include "mgGameScene.h"
#include "mgWorldScene.h"
#include "AppDelegate.h"
#include "mgSoundManager.h"
#include "mgMark.h"
#include "mgGameDataManager.h"

using namespace cocos2d;

const float	mgBoard::READY_TIME		= 2.0f;
const float	mgBoard::GAME_OVER_TIME	= 2.0f;
const float	mgBoard::COMBO_TIME		= 2.0f;  // 콤보 유지 제한 시간 (Feedback Loop)
const float	mgBoard::FEVER_TIME		= 10.0f; // 피버 모드 지속 시간 (Reward System)
const float	mgBoard::ADD_TIME		= 10.0f;

mgBoard::mgBoard(mgGameScene* pScene, int nX, int nY, int nWidth, int nHeight)
	: m_pGameScene(pScene)
	, m_rect(nX, nY, nWidth, nHeight)
	, m_packingRect(nullptr, m_rect)
	, m_state(NONE)
	, m_fReadyTime(0)
	, m_fGameOverTime(0)
	, m_fComboTime(0.0f)
	, m_nComboCount(0)
	, m_bFever(false)
	, m_nFever(0)
	, m_fFeverTime(0)
	, m_bCountdown(false)
	, m_nRemainItemCount(0)
	, m_pHintObject(nullptr)
	, m_nSoundCountDown(-1)
{
}

mgBoard::~mgBoard(void)
{
}

/**
 * @brief 게임의 메인 업데이트 루프 (Game Loop & State Machine)
 * @details 프레임마다 호출되며, 현재 게임 상태(State)에 따라 로직을 분기 처리함.
 * [INIT -> PLAY -> GAME_OVER]의 생명주기를 관리.
 */
void mgBoard::visit(Renderer* renderer, const kmMat4 &parentTransform, bool parentTransformUpdated)
{
	float fDelta = Director::getInstance()->getDeltaTime();

	// 1. 게임 준비 상태 (Initialization State)
	if( m_state == INIT_GAME )
	{
		if( (m_fReadyTime -= fDelta) <= 0.0f )
		{
			m_fReadyTime = 0.0f;
			startGame(); // 상태 전이: INIT -> PLAY
		}
	}
	// 2. 게임 플레이 상태 (Core Gameplay Loop)
	else if( m_state == PLAY )
	{
		updateTime(fDelta); // 제한 시간 및 카운트다운 관리

		// 아이템 쿨타임 관리
		m_pGameScene->setItemCoolTime_Hint(fDelta);
		m_pGameScene->setItemCoolTime_Time(fDelta);

		// Fever Mode System: 피버 상태와 일반 상태의 타이머 로직 분리
		if( m_bFever )
		{
			if( (m_fFeverTime -= fDelta) <= 0.0f )
			{
				setFeverMode(false); // 피버 종료
			}
		}
		else
		{
			// Combo System: 일정 시간 입력이 없으면 콤보 초기화
			if( m_fComboTime > 0.0f )
			{
				if( (m_fComboTime -= fDelta) <= 0.0f )
				{
					m_fComboTime = 0.0f;
					m_nComboCount = 0;
					m_pGameScene->onComboEnd();
				}
			}
		}

		// Dynamic Object Pooling & Regeneration
		// 삭제된(RECREATE) 오브젝트를 감지하고, 빈 공간에 새로운 오브젝트를 채워넣음
		int nObjectCount = m_objectList.size();
		for( int nObject = 0; nObject < nObjectCount; ++nObject )
		{
			mgObject* pObject = m_objectList[nObject];
			if( pObject->getState() == mgObject::RECREATE )
			{
				pObject->setState(mgObject::NONE);

				// 기존 오브젝트 데이터 반환
				mgObjectData*	pOldData = pObject->getData();
				pOldData->setUsed(false);

				// 빈 공간(Rect) 확보
				mgPackingRect*	pParentRect = pObject->getPackingRect()->emptyRect();
				float fRectSize = min<float>(pParentRect->getRect().size.width, pParentRect->getRect().size.height);

				// 크기에 맞는 새 오브젝트 데이터 검색
				mgObjectData* pData = findObjectDataForSize(m_pStage->m_minObjectSize, fRectSize);
				if( pData == nullptr )
					continue;

				// Packing Algorithm: 빈 공간에 새 오브젝트 삽입
				int nObjectSize = pData->getSize();
				mgPackingRect* pRect = pParentRect->insertRect(Rect(0.0f, 0.0f, nObjectSize, nObjectSize));
				if( pRect != nullptr )
				{
					pObject->setData(pData, pRect);
					pData->setUsed(true);

					++m_nRemainItemCount;
				}
				break;
			}
		}
	}
	// 3. 게임 오버 상태 (Result State)
	else if( m_state == GAME_OVER )
	{
		if( (m_fGameOverTime -= fDelta) <= 0.0f )
		{
			m_fGameOverTime = 0.0f;
			showResult(); // 결과 화면 출력
		}
	}

	if( m_state != GAME_OVER )
	{
		Node::visit(renderer, parentTransform, parentTransformUpdated);
	}
}

void mgBoard::initGame()
{
	m_state = INIT_GAME;
	m_fReadyTime = READY_TIME;

	m_pStage = mgWorld::getInstance()->getCurrentMap()->getCurrentStage();
	m_pCharacter = mgGameDataManager::getInstance()->getCurrentCharacter();

	m_bCountdown = false;
	m_fInitTime = m_pStage->m_nTime;
	m_fTime = m_fInitTime;
	m_pGameScene->setTime(m_fInitTime, m_fTime);
	m_pGameScene->setFever(MAX_FEVER, 0);
	m_nTheme = m_pStage->m_nTheme;
	m_nStep = m_pStage->m_nStep;

	initObject(m_nTheme,m_nStep);
	m_nMissionScore = m_pStage->m_nHp;
	m_pGameScene->setMissionScore(m_nMissionScore);
	m_nScore = 0;
	m_nGold = 0;
	m_nMaxCombo = 0;
	m_pGameScene->setScore(m_nMissionScore, m_nScore);

	m_fComboTime = 0.0f;
	m_nComboCount = 0;

	m_bFever = false;
	m_nFever = 0;
	m_fFeverTime = 0.0f;

	m_fGameOverTime = 0.0f;

	// items initialization
	m_bFindObject = true;
	m_pHintObject = nullptr;
	m_bAddTime = true;
	m_bCleanScreen = true;
	m_bChangeKeyword = true;

	m_nSoundCountDown = -1;
}

void mgBoard::initObject(int nTheme,int nStep)
{
	mgDataManager* pDataManager = mgDataManager::getInstance();
	TextureCache* pTextureCache = Director::getInstance()->getTextureCache();

	int nDataCount = 0;
	switch (nTheme)
	{
	case 1:
		nDataCount = pDataManager->getObjectCount1(nTheme,nStep);
		break;
	case 2:
		nDataCount = pDataManager->getObjectCount2(nTheme,nStep);
		break;
	case 3:
		nDataCount = pDataManager->getObjectCount3(nTheme,nStep);
		break;
	case 4:
		nDataCount = pDataManager->getObjectCount4(nTheme,nStep);
		break;
	default:
		break;
	}
	for( int i = 0; i < nDataCount; ++i )
	{
		mgObjectData* pData = NULL;
		switch (nTheme)
		{
		case 1:
			pData = pDataManager->getObjectAt1(i,nTheme,nStep);
			break;
		case 2:
			pData = pDataManager->getObjectAt2(i,nTheme,nStep);
			break;
		case 3:
			pData = pDataManager->getObjectAt3(i,nTheme,nStep);
			break;
		case 4:
			pData = pDataManager->getObjectAt4(i,nTheme,nStep);
			break;
		default:
			break;
		}
		if (pData != nullptr)
		{
			pTextureCache->addImage("img/" + pData->getID() + ".png");
		}
	}
}

void mgBoard::startGame()
{
	m_state = PLAY;
	resetBoard();
	m_pGameScene->startGame();
}

void mgBoard::pause()
{
	m_state = PAUSE;
	mgSoundManager::getInstance()->pauseAllEffects();
}

void mgBoard::resume()
{
	m_state = PLAY;
	mgSoundManager::getInstance()->resumeAllEffects();
}

void mgBoard::gameOver(bool bCompleted)
{
	m_state = GAME_OVER;
	m_fGameOverTime = GAME_OVER_TIME;
	m_bCompleted = bCompleted;

	mgSoundManager::getInstance()->stopEffect(m_nSoundCountDown);
	m_pGameScene->gameOver(m_bCompleted);
}

void mgBoard::showResult()
{
	m_state = SHOW_RESULT;

	// Score Calculation Logic
	int nClearBonus = m_nScore;
	m_nScore += (int)m_fTime * HP_BONUS;
	m_nScore += m_nMaxCombo * MAX_COMBO_BONUS;
	m_nGold = m_nScore * GOLD_RATE / 100;

	mgSoundManager::getInstance()->stopEffect(m_nSoundCountDown);
	m_pGameScene->showResult(m_bCompleted, m_nScore, m_nGold, nClearBonus, m_fTime, m_nMaxCombo);
}

void mgBoard::clear()
{
	removeAllChildren();
	m_packingRect.emptyRect();
	m_objectList.clear();
}

// Fever Mode Control
void mgBoard::setFeverMode(bool bFever)
{
	m_bFever = bFever;

	if( m_bFever )
	{
		m_nFever = MAX_FEVER;
		m_fFeverTime = FEVER_TIME;

		mgSoundManager::getInstance()->playEffect("snd/fever.mp3");

		m_fComboTime = 0.0f;
		m_nComboCount = 0;
	}
	else
	{
		m_nFever = 0;
		m_fFeverTime = 0.0f;
	}

	m_pGameScene->setFeverMode(m_bFever);
}

mgBoard::STATE mgBoard::getState() const
{
	return m_state;
}

int mgBoard::getComboCount() const
{
	return m_nComboCount;
}

/**
 * @brief 유저 입력(터치) 처리 및 게임 로직 판정 (Input Handling)
 * @details 오브젝트 피킹, 정답 체크, 콤보 계산, 점수 합산 등 핵심 게임 플레이 로직
 * @return true: 유효한 오브젝트 선택됨, false: 선택 실패
 */
bool mgBoard::pick(int nX, int nY)
{
	if( m_state != PLAY )
		return false;

	int nObjectCount = m_objectList.size();
	for (int nObject = 0; nObject < nObjectCount; ++nObject)
	{
		mgObject* pObject = m_objectList[nObject];
		if( pObject->pick(nX, nY) )
		{
			// 정답 체크 (Correct Logic)
			if( pObject->hasKeyword(m_strKeyword) )
			{
				pObject->setState(mgObject::ATTACK);

				// Score Calculation with Multiplier
				int nScore = 0;
				if( m_bFever )
					nScore = m_pCharacter->m_nPower * FEVER_RATE / 100;
				else if(m_nComboCount > 0 )
					nScore = m_pCharacter->m_nPower * COMBO_RATE / 100;
				else
					nScore = m_pCharacter->m_nPower;

				m_nScore += nScore;

				// Update UI & Rewards
				m_pGameScene->setScore(m_nMissionScore, m_nScore);
				if(m_nComboCount > 0)
				{
					m_pGameScene->setGold(REWARD_BONUS);
				}
				else
				{
					m_pGameScene->setGold(REWARD_GOLD);
				}

				// Check Win Condition
				if(m_nScore >= m_nMissionScore)
				{
					setFeverMode(false);
					gameOver(true);

					mgWorldScene* pWorldScene = ((AppDelegate*)Application::getInstance())->m_pWorldScene;

					int nValue = m_pGameScene->GetStarValue();
					mgWorld::getInstance()->getCurrentMap()->updateCurrentStage(nValue);
					CCUserDefault::sharedUserDefault()->setIntegerForKey("StageNum",m_pStage->m_nNumber +1);
					pWorldScene->updateMap();

					char szSaveDataName[64];
					sprintf(szSaveDataName,"Stage%d_Star",m_pStage->m_nNumber);
					CCUserDefault::sharedUserDefault()->setIntegerForKey(szSaveDataName,nValue);
				}
				else
				{
					// Continue Game Loop
					if( --m_nRemainItemCount == 0 )
						resetBoard();
					else
					{
						if( --m_nFindCount == 0 )
							selectKeyword();
						else
							m_pGameScene->setKeyword(m_strKeyword, m_strPronounce, m_strSound, m_nFindCount);
					}
				}

				// Feedback Loop (Sound & Effects)
				if( m_bFever )
				{
					mgSoundManager::getInstance()->playEffect("snd/hammer.mp3");
				}
				else
				{
					++m_nComboCount;
					if(m_nComboCount > m_nMaxCombo)
						m_nMaxCombo = m_nComboCount;

					if(m_nComboCount >= 2 )
						m_pGameScene->onCombo(m_nComboCount);

					// Dynamic Sound Pitch based on Combo
					if(m_nComboCount < 10 )
					{
						char szSoundName[256];
						sprintf(szSoundName, "snd/pang_%02d.mp3", m_nComboCount);
						mgSoundManager::getInstance()->playEffect(szSoundName);
					}
					else
						mgSoundManager::getInstance()->playEffect("snd/pang_09.mp3");

					// Fever Gauge Logic
					if(m_nComboCount <= 10 )
						m_nFever += FEVER_XP_1_9;
					else
						m_nFever += FEVER_XP_10;

					if(m_nFever >= MAX_FEVER )
						setFeverMode(true);
					else
						m_fComboTime = COMBO_TIME;
				}

				if( m_pHintObject != nullptr )
				{
					m_pHintObject->setHint(false);
					m_pHintObject = nullptr;
				}

				return true;
			}
			else
			{
				// 오답 처리 (Penalty Logic)
				m_nComboCount = 0;
				updateTime(5.0f); // Time Penalty

				mgSoundManager::getInstance()->playEffect("snd/wrong_01.mp3");

				mgMark* pMark = mgMark::create();
				pMark->showMark("ui/gm_X.png", nX, nY);
				addChild(pMark);
				mgMark* pMinusSec = mgMark::create();
				pMinusSec->showEffect("ui/gm_5sec.png", nX + 35, nY + 35);
				addChild(pMinusSec,999);
			}
			break;
		}
	}
	return false;
}

void mgBoard::resetBoard()
{
	clear();

	srand((unsigned)time(nullptr));

	m_nRemainItemCount = 0;
	switch (m_nTheme)
	{
	case 1:
		mgDataManager::getInstance()->resetAllUsedObject1(m_nTheme,m_nStep);
		break;
	case 2:
		mgDataManager::getInstance()->resetAllUsedObject2(m_nTheme,m_nStep);
		break;
	case 3:
		mgDataManager::getInstance()->resetAllUsedObject3(m_nTheme,m_nStep);
		break;
	case 4:
		mgDataManager::getInstance()->resetAllUsedObject4(m_nTheme,m_nStep);
		break;
	default:
		break;
	}

	for( int nObject = 0; nObject < 300; ++nObject )
	{
		mgObjectData* pData = findObjectDataForSize(m_pStage->m_minObjectSize, m_pStage->m_maxObjectSize);
		if( pData == nullptr )
			break;

		int nObjectSize = pData->getSize();
		mgPackingRect* pRect = m_packingRect.insertRect(Rect(0.0f, 0.0f, nObjectSize, nObjectSize));
		if( pRect != nullptr )
		{
			mgObject* pObject = new mgObject(this);
			pObject->init();
			pObject->autorelease(); // Ref Count 관리
			pObject->setData(pData, pRect);
			addChild(pObject, -1);

			m_objectList.push_back(pObject);
			++m_nRemainItemCount;

			pData->setUsed(true);
		}
	}

	selectKeyword();
}

mgObjectData* mgBoard::findObjectDataForSize(int nMinSize, int nMaxSize)
{
	mgDataManager* dataManager = mgDataManager::getInstance();
	if (dataManager == nullptr)
	{
		return nullptr;
	}

	int dataCount = 0;
	switch (m_nTheme)
	{
	case 1: dataCount = dataManager->getObjectCount1(m_nTheme, m_nStep); break;
	case 2: dataCount = dataManager->getObjectCount2(m_nTheme, m_nStep); break;
	case 3: dataCount = dataManager->getObjectCount3(m_nTheme, m_nStep); break;
	case 4: dataCount = dataManager->getObjectCount4(m_nTheme, m_nStep); break;
	default: return nullptr;
	}

	if (dataCount <= 0)
	{
		return nullptr;
	}

	const auto getDataAt = [this, dataManager](int index) -> mgObjectData*
	{
		switch (m_nTheme)
		{
		case 1: return dataManager->getObjectAt1(index, m_nTheme, m_nStep);
		case 2: return dataManager->getObjectAt2(index, m_nTheme, m_nStep);
		case 3: return dataManager->getObjectAt3(index, m_nTheme, m_nStep);
		case 4: return dataManager->getObjectAt4(index, m_nTheme, m_nStep);
		default: return nullptr;
		}
	};

	// Start at a random index, then inspect each candidate once.
	const int startIndex = rand() % dataCount;
	for (int offset = 0; offset < dataCount; ++offset)
	{
		mgObjectData* data = getDataAt((startIndex + offset) % dataCount);
		if (data == nullptr || data->isUsed())
		{
			continue;
		}

		int objectSize = data->getSize();
		if (objectSize == 0)
		{
			TextureCache* textureCache = Director::getInstance()->getTextureCache();
			Texture2D* texture = textureCache->getTextureForKey("img/" + data->getID() + ".png");
			if (texture != nullptr)
			{
				const float maxSize = max<float>(texture->getPixelsWide(), texture->getPixelsHigh()) + 10.0f;
				data->setSize(maxSize);
				objectSize = data->getSize();
			}
		}

		if (objectSize >= nMinSize && objectSize <= nMaxSize)
		{
			return data;
		}
	}

	return nullptr;
}

void mgBoard::selectKeyword()
{
	const int nObjectCount = static_cast<int>(m_objectList.size());
	if (nObjectCount == 0)
	{
		return;
	}
	const int startIndex = rand() % nObjectCount;

	mgObject* pSelObject = nullptr;
	for (int offset = 0; offset < nObjectCount; ++offset)
	{
		mgObject* candidate = m_objectList[(startIndex + offset) % nObjectCount];
		if (candidate != nullptr && candidate->getState() == mgObject::IDLE && candidate->getData() != nullptr)
		{
			pSelObject = candidate;
			break;
		}
	}
	if (pSelObject == nullptr)
	{
		return;
	}

	// Select a keyword and count matching idle objects.
	m_strKeyword = pSelObject->getData()->getKeyword(0);
	m_strPronounce = pSelObject->getData()->getPronounce(0);
	m_strSound = pSelObject->getData()->getSound(0);
	// find items has same keyword
	m_nFindCount = 0;
	for (int nObject = 0; nObject < nObjectCount; ++nObject)
	{
		mgObject* pObject = m_objectList[nObject];
		if (pObject == nullptr || pObject->getState() != mgObject::IDLE)
			continue;

		if( pObject->hasKeyword(m_strKeyword) )
			++m_nFindCount;
	}

	// find count
	if (m_nFindCount == 0)
	{
		return;
	}
	m_nFindCount = 1 + rand() % m_nFindCount;

	m_pGameScene->setKeyword(m_strKeyword,m_strPronounce, m_strSound,m_nFindCount);
}

void mgBoard::updateTime(float delta)
{
	if( m_state != PLAY )
		return;

	m_fTime -= delta;

	if( !m_bCountdown && m_fTime <= 10.0f )
	{
		m_bCountdown = true;
		m_nSoundCountDown = mgSoundManager::getInstance()->playEffect("snd/countDown.mp3");
	}
	else if(m_bCountdown && m_fTime > 10.0f )
	{
		m_bCountdown = false;
		mgSoundManager::getInstance()->stopEffect(m_nSoundCountDown);
		m_nSoundCountDown = -1;
	}

	if(m_fTime <= 0.0f )
	{
		m_fTime = 0;

		int nValue = m_pGameScene->GetStarValue();
		if(nValue > 0)
		{
			gameOver(true);
			mgWorldScene* pWorldScene = ((AppDelegate*)Application::getInstance())->m_pWorldScene;

			mgWorld::getInstance()->getCurrentMap()->updateCurrentStage(nValue);
			CCUserDefault::sharedUserDefault()->setIntegerForKey("StageNum",m_pStage->m_nNumber+1);
			pWorldScene->updateMap();
			char szSaveDataName[64];
			sprintf(szSaveDataName,"Stage%d_Star",m_pStage->m_nNumber);
			CCUserDefault::sharedUserDefault()->setIntegerForKey(szSaveDataName,nValue);
		}
		else
		{
			gameOver(false);
		}
	}
	m_pGameScene->setTime(m_fInitTime, m_fTime);
	m_pGameScene->setFever(MAX_FEVER, m_nFever);
}

// items
void mgBoard::findObject()
{
	int nObjectCount = m_objectList.size();
	for (int nObject = 0; nObject < nObjectCount; ++nObject)
	{
		mgObject* pObject = m_objectList[nObject];
		if (pObject->getState() != mgObject::IDLE)
			continue;

		if (pObject->hasKeyword(m_strKeyword))
		{
			m_pHintObject = pObject;
			m_pHintObject->setHint(true);

			mgMark* pAddSec = mgMark::create();
			pAddSec->showEffect("ui/hint_Use.png", 230, -50);
			addChild(pAddSec, 999);

			break;
		}
	}
}

void mgBoard::addTime()
{
	updateTime(-ADD_TIME);
	m_pGameScene->setTime(m_fInitTime, m_fTime);

	mgMark* pAddSec = mgMark::create();
	pAddSec->showEffect("ui/gm_10sec.png", 230, -50);
	addChild(pAddSec,999);
}

void mgBoard::cleanScreen()
{
	if( m_bCleanScreen )
	{
		m_bCleanScreen = false;

		for( auto pObject : m_objectList )
		{
			pObject->clearSkills();
		}
	}
}

void mgBoard::changeKeyword()
{
	if( m_bChangeKeyword )
	{
		m_bChangeKeyword = false;

		selectKeyword();
	}
}
