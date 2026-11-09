#include "QuickMenuSystem.h"
#include <fstream>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define CONFIG_FILE_PATH			"UserData/QuickMenu.dat"
#define UI_NAME_CATEGORY			"Btn_Category_"
#define UI_NAME_SLOT				"Btn_Slot_"
#define UI_NAME_REGIST				"Btn_Regist_"
#define TAB_BUTTON_COUNT			5
#define MAX_REGIST_SLOTCOUNT		10

QuickMenuSystem::QuickMenuSystem()
	: m_pSpr_ButtonIcon(nullptr)
	, m_iCurrentTabIndex(0)
	, m_iCurrentSlotIndex(0)
	, m_iRegistCount(0)
	, m_iSelectRegIndex(0)
	, m_pBtnSave(nullptr)
{
}

QuickMenuSystem::~QuickMenuSystem()
{
	if (m_pSpr_ButtonIcon)
	{
		delete m_pSpr_ButtonIcon;
		m_pSpr_ButtonIcon = nullptr;
	}
}

bool QuickMenuSystem::Initialize(GameApplication* pApp)
{
	if (!pApp) return false;

	if (!UIWindow::LoadLayout("QuickMenu.layout", pApp))
		return false;

	// 아이콘 스프라이트 로드
	m_pSpr_ButtonIcon = new Sprite();
	m_pSpr_ButtonIcon->Load("Icon_QuickMenu.spr");

	m_mapPrev_RegistData.clear();
	InitUIButtons();

	UIWindow::Hide();
	return true;
}

bool QuickMenuSystem::OnOpen(GameApplication* pApp)
{
	UIWindow::Show();

	ClearAllData();
	m_mapPrev_RegistData.clear();
	LoadUserConfig();

	// UI 초기 상태 설정
	m_iCurrentTabIndex = 0;
	m_iCurrentSlotIndex = 1;

	InitSlotButtons();
	InitSlotBtnSprite();
	SetRegistSlot();

	LoadTabUI(m_iCurrentTabIndex);
	UpdateSaveButtonState();

	return true;
}

// -------------------------------------------------------------------------
// 데이터 저장 및 로드 (Binary I/O)
// -------------------------------------------------------------------------

void QuickMenuSystem::SaveUserConfig()
{
	if (m_mapRegistData.size() > MAX_REGIST_SLOTCOUNT)
		return;

	std::string fullPath = g_GameSystem.GetSavePath() + "/" + CONFIG_FILE_PATH;

	FILE* file = fopen(fullPath.c_str(), "wb");
	if (!file) return;

	// 파일 쓰기가 끝까지 성공한 경우에만 저장 완료 상태로 전환합니다.
	int count = static_cast<int>(m_mapRegistData.size());
	bool saved = fwrite(&count, sizeof(int), 1, file) == 1;

	for (auto const& [key, data] : m_mapRegistData)
	{
		if (!saved) break;
		saved = fwrite(&key, sizeof(int), 1, file) == 1 &&
			fwrite(&data.iTabIndex, sizeof(int), 1, file) == 1 &&
			fwrite(&data.iSlotIndex, sizeof(int), 1, file) == 1 &&
			fwrite(&data.iSpriteIndex, sizeof(int), 1, file) == 1 &&
			fwrite(&data.iMenuIndex, sizeof(int), 1, file) == 1;
	}

	if (fclose(file) != 0)
		saved = false;
	if (saved)
		m_mapPrev_RegistData = m_mapRegistData;
}

void QuickMenuSystem::LoadUserConfig()
{
	std::string fullPath = g_GameSystem.GetSavePath() + "/" + CONFIG_FILE_PATH;

	FILE* file = fopen(fullPath.c_str(), "rb");
	if (!file) return;

	int count = 0;
	bool valid = fread(&count, sizeof(int), 1, file) == 1 &&
		count >= 0 && count <= MAX_REGIST_SLOTCOUNT;
	std::map<int, QuickMenuSaveData> loadedData;

	for (int i = 0; valid && i < count; ++i)
	{
		int iKey = -1;
		QuickMenuSaveData data;
		valid = fread(&iKey, sizeof(int), 1, file) == 1 &&
			fread(&data.iTabIndex, sizeof(int), 1, file) == 1 &&
			fread(&data.iSlotIndex, sizeof(int), 1, file) == 1 &&
			fread(&data.iSpriteIndex, sizeof(int), 1, file) == 1 &&
			fread(&data.iMenuIndex, sizeof(int), 1, file) == 1;
		if (valid)
			valid = iKey >= 1 && iKey <= MAX_REGIST_SLOTCOUNT &&
				loadedData.emplace(iKey, data).second;
	}
	fclose(file);
	if (valid)
	{
		m_mapRegistData = loadedData;
		m_mapPrev_RegistData = loadedData;
	}
}

// -------------------------------------------------------------------------
// 이벤트 처리 및 입력
// -------------------------------------------------------------------------

QuickMenuSystem::EQuickMenuEvent QuickMenuSystem::GetEventIndex(const char* szName)
{
	if (!szName) return EVENT_NONE;

	// 정적 매핑
	static const std::unordered_map<std::string, EQuickMenuEvent> buttonMap = {
		{ "Btn_Close", EVENT_CLOSE },
		{ "Btn_Save", EVENT_SAVE },
		{ "Btn_Cancel", EVENT_CANCEL },
		{ "Btn_Reset", EVENT_RESET }
	};

	auto itr = buttonMap.find(szName);
	if (itr != buttonMap.end()) return itr->second;

	// 숫자 접미사가 정확히 일치하는 버튼 이름만 이벤트로 처리합니다.
	auto parseEvent = [szName](const char* prefix, int first, int last, int eventStart) -> EQuickMenuEvent {
		const std::size_t prefixLength = std::strlen(prefix);
		if (std::strncmp(szName, prefix, prefixLength) != 0)
			return EVENT_NONE;
		const char* number = szName + prefixLength;
		if (*number < '0' || *number > '9')
			return EVENT_NONE;
		char* end = nullptr;
		const long index = std::strtol(number, &end, 10);
		if (*end != '\0' || index < first || index > last)
			return EVENT_NONE;
		return static_cast<EQuickMenuEvent>(eventStart + static_cast<int>(index) - first);
	};

	EQuickMenuEvent event = parseEvent(UI_NAME_CATEGORY, 0, TAB_BUTTON_COUNT - 1, EVENT_CATEGORY_START);
	if (event != EVENT_NONE) return event;
	event = parseEvent(UI_NAME_SLOT, 1, EVENT_SLOT_END - EVENT_SLOT_START + 1, EVENT_SLOT_START);
	if (event != EVENT_NONE) return event;
	event = parseEvent(UI_NAME_REGIST, 1, MAX_REGIST_SLOTCOUNT, EVENT_REGIST_START);
	if (event != EVENT_NONE) return event;
	return EVENT_NONE;
}

int QuickMenuSystem::OnMouseLUp(const char* pName, POINT ptPos, int iMessage)
{
	if (!pName) return 0;

	EQuickMenuEvent eventID = GetEventIndex(pName);

	// 1. 카테고리 탭 변경
	if (eventID >= EVENT_CATEGORY_START && eventID <= EVENT_CATEGORY_END)
	{
		m_iCurrentTabIndex = eventID - EVENT_CATEGORY_START;
		LoadTabUI(m_iCurrentTabIndex);
	}
	// 2. 메뉴 슬롯 선택 (등록)
	else if (eventID >= EVENT_SLOT_START && eventID <= EVENT_SLOT_END)
	{
		int slotIndex = eventID - EVENT_SLOT_START;
		// 슬롯 등록 로직 (생략된 부분 구현: 데이터 추가 및 저장 버튼 갱신)
		SlotUIBinding* pSlot = GetMenuSlot(m_iCurrentTabIndex, slotIndex);
		if (pSlot && pSlot->pMenuBtn && !pSlot->pMenuBtn->IsDisabled())
		{
			QuickMenuSaveData newData;
			newData.iTabIndex = m_iCurrentTabIndex;
			newData.iSlotIndex = slotIndex;
			newData.iMenuIndex = pSlot->iMenuIndex;

			int emptyIdx = FindEmptySlotIndex();
			if (emptyIdx != -1) {
				m_mapRegistData[emptyIdx] = newData;
				SetRegistSlot();
				UpdateSaveButtonState();
			}
		}
	}
	// 3. 등록된 슬롯 선택 (삭제)
	else if (eventID >= EVENT_REGIST_START && eventID <= EVENT_REGIST_END)
	{
		int regIndex = eventID - EVENT_REGIST_START + 1;
		RemoveRegistData(regIndex);
	}
	// 4. 제어 버튼
	else
	{
		switch (eventID)
		{
		case EVENT_CLOSE:
		case EVENT_CANCEL:
			if (IsDataChanged(m_mapRegistData, m_mapPrev_RegistData))
				ShowMessageBox(4051); // "저장하지 않고 종료하시겠습니까?"
			else
				OnClose();
			break;
		case EVENT_SAVE:
			SaveUserConfig();
			break;
		case EVENT_RESET:
			ClearAllData();
			UpdateSaveButtonState();
			break;
		}
	}
	return 1;
}

// -------------------------------------------------------------------------
// 유틸리티
// -------------------------------------------------------------------------

bool QuickMenuSystem::IsDataChanged(const std::map<int, QuickMenuSaveData>& Data, const std::map<int, QuickMenuSaveData>& PrevData)
{
	if (Data.size() != PrevData.size()) return true;

	auto itr_Data = Data.begin();
	auto it_PrevData = PrevData.begin();

	while (itr_Data != Data.end())
	{
		const QuickMenuSaveData& current = itr_Data->second;
		const QuickMenuSaveData& previous = it_PrevData->second;
		if (itr_Data->first != it_PrevData->first ||
			current.iTabIndex != previous.iTabIndex ||
			current.iSlotIndex != previous.iSlotIndex ||
			current.iSpriteIndex != previous.iSpriteIndex ||
			current.iMenuIndex != previous.iMenuIndex)
			return true;
		++itr_Data;
		++it_PrevData;
	}
	return false;
}

void QuickMenuSystem::UpdateSaveButtonState()
{
	m_pBtnSave = dynamic_cast<UIButton*>(FindControl("Btn_Save"));
	if (m_pBtnSave)
	{
		bool bChanged = IsDataChanged(m_mapRegistData, m_mapPrev_RegistData);
		m_pBtnSave->SetDisable(!bChanged);
	}
}
