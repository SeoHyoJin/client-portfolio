#pragma once
#include "UIWindow.h"
#include <map>
#include <unordered_map>
#include <vector>
#include <string>

// 저장용 데이터 구조체
struct QuickMenuSaveData
{
	int iTabIndex;
	int iSlotIndex;
	int iSpriteIndex;
	int iMenuIndex;

	QuickMenuSaveData()
		: iTabIndex(-1), iSlotIndex(-1), iSpriteIndex(-1), iMenuIndex(-1) {}
};

// 런타임 UI 바인딩 구조체
struct SlotUIBinding
{
	int iMenuIndex;
	UIButton* pMenuBtn;
	UIButton* pNoticeBtn;
	int	iTooltipIndex;

	SlotUIBinding()
		: iMenuIndex(-1), pMenuBtn(nullptr), pNoticeBtn(nullptr), iTooltipIndex(-1) {}
};

// 텍스트 설정용 구조체
struct TextButtonInfo {
	const char* name;
	int textIndex;
	int xOffset;
	int yOffset;
};

/**
 * @class QuickMenuSystem
 * @brief 퀵메뉴(즐겨찾기) 시스템
 * @details 유저가 자주 사용하는 메뉴를 등록/해제하고, 로컬 파일에 설정을 저장하는 시스템
 */
class QuickMenuSystem : public UIWindow
{
public:
	// UI 이벤트 ID
	enum EQuickMenuEvent
	{
		EVENT_NONE = 0,
		EVENT_CLOSE,
		EVENT_SAVE,
		EVENT_CANCEL,
		EVENT_RESET,

		EVENT_CATEGORY_START = 100, // 0~4
		EVENT_CATEGORY_END = 104,

		EVENT_SLOT_START = 200,     // 1~12
		EVENT_SLOT_END = 211,

		EVENT_REGIST_START = 300,   // 1~10
		EVENT_REGIST_END = 309,
	};

	QuickMenuSystem();
	virtual ~QuickMenuSystem();

public:
	virtual bool Initialize(GameApplication* pApp) override;
	virtual int Update(const float dTime, InputManager* pInput, int iMessage) override;
	virtual int Render(GameApplication* pApp) override;

	virtual bool OnOpen(GameApplication* pApp);
	virtual bool OnClose();

	// 입력 처리
	virtual int OnMouseLUp(const char* pName, POINT ptPos, int iMessage) override;

	// 데이터 관리
	void LoadUserConfig();
	std::map<int, QuickMenuSaveData> GetQuickMenuData() { return m_mapRegistData; }
	Sprite* GetButtonSprite() { return m_pSpr_ButtonIcon; }

private:
	void SaveUserConfig();
	void LoadTabUI(int iTabIndex);

	// UI 설정 및 초기화
	void InitUIButtons();
	void SetRegistSlot(int iRegSlotIndex, int iTabIndex, int iSlotIndex);
	void SetRegistSlot();
	void SetSlotMenuState(int iTabIndex, int iSlotIndex, bool bEnable);

	void InitSlotButtons();
	void InitSlotBtnSprite();
	void ClearAllData();
	void InitRegistSlot(int iRegSlotIndex);
	void RemoveRegistData(int iRegSlotIndex);

	// 상태 제어
	void UpdateSaveButtonState();
	bool IsDataChanged(const std::map<int, QuickMenuSaveData>& Data, const std::map<int, QuickMenuSaveData>& PrevData);

	// 이벤트 파싱
	EQuickMenuEvent GetEventIndex(const char* szName);
	SlotUIBinding* GetMenuSlot(int iTabIndex, int iSlotIndex);
	int FindEmptySlotIndex();

	void ShowMessageBox(int iMsgIndex);
	void DrawToolTip();

private:
	int m_iCurrentTabIndex;
	int m_iCurrentSlotIndex;
	int m_iSelectRegIndex;
	int m_iRegistCount;

	Sprite* m_pSpr_ButtonIcon;

	std::unordered_map<int, std::vector<SlotUIBinding>> m_mapSlotData;
	std::map<int, QuickMenuSaveData> m_mapRegistData;
	std::map<int, QuickMenuSaveData> m_mapPrev_RegistData; // 변경 감지용 원본

	UIButton* m_pBtnSave;
};
