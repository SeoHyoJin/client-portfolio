#pragma once
#include <d3dx9.h>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <map>
#include <tuple>

/**
 * @class FontTextureCache
 * @brief 폰트 텍스처 캐싱 시스템
 * @details 매 프레임 폰트를 렌더링하는 비용을 줄이기 위해, 속성(Key)별로 텍스처를 생성하여 캐싱하고 재사용합니다.
 */
class FontTextureCache
{
public:
	// 캐시 검색을 위한 키 구조체
	struct FontCacheKey {
		std::string text;
		std::string fontName;
		int fontWeight;
		int fontSize;
		int align;
		int outlineOffset;
		DWORD color;
		DWORD outlineColor;
		DWORD glowColor;
		int sizeX;
		int sizeY;

		// 맵 정렬을 위한 비교 연산자
		bool operator<(const FontCacheKey& other) const {
			return std::tie(text, fontName, fontWeight, fontSize, align, outlineOffset, color, outlineColor, glowColor, sizeX, sizeY)
				< std::tie(other.text, other.fontName, other.fontWeight, other.fontSize, other.align, other.outlineOffset, other.color,
					other.outlineColor, other.glowColor, other.sizeX, other.sizeY);
		}
	};

	FontTextureCache() = default;
	~FontTextureCache();
	FontTextureCache(const FontTextureCache&) = delete;
	FontTextureCache& operator=(const FontTextureCache&) = delete;

	// 캐싱된 텍스처 정보
	struct TextureNode {
		IDirect3DTexture9* pTexture = nullptr;
		int width = 0;
		int height = 0;
		float fOutlineSize = 0.0f;
		DWORD TextColor = 0xFF000000;
		DWORD OutlineColor = 0xFF000000;
		DWORD GlowColor = 0x00000000;
	};

	bool Init(LPDIRECT3DDEVICE9 device);
	void Clear();
	void Release();

	// 텍스트 인덱스(ID)로 캐시 검색
	const TextureNode* GetCache(int textIndex);

	// 텍스트 속성으로 캐시 검색 (ID 없는 경우)
	const TextureNode* GetCache(
		const std::string& text,
		std::string fontName,
		int fontWeight,
		int fontSize,
		DWORD color,
		DWORD outlineColor,
		DWORD glowColor,
		int align = 0,
		int outlineOffset = 0,
		int sizeX = 0,
		int sizeY = 0
	);

	// 텍스트 인덱스로 캐시 생성
	TextureNode* CreateCache(
		int textIndex,
		const std::string& text,
		std::string fontName,
		int fontWeight,
		int fontSize,
		int align,
		DWORD color,
		DWORD outlineColor,
		int outlineOffset,
		DWORD glowColor,
		int sizeX = 0,
		int sizeY = 0
	);

	// 텍스트 속성으로 캐시 생성
	TextureNode* CreateCache(
		const std::string& text,
		std::string fontName,
		int fontWeight,
		int fontSize,
		int align,
		DWORD color,
		DWORD outlineColor,
		int outlineOffset,
		DWORD glowColor,
		int sizeX,
		int sizeY
	);

	void RemoveCache(int textIndex);
	bool HasCache(int textIndex) const;
	std::size_t GetCacheSize() const { return m_cacheMap.size() + m_indexCacheMap.size(); }

private:
	LPDIRECT3DDEVICE9 m_pDevice = nullptr;

	// 속성 기반 캐시 (Dynamic Text)
	std::map<FontCacheKey, TextureNode> m_cacheMap;

	// ID 기반 캐시 (Static Text)
	std::unordered_map<int, TextureNode> m_indexCacheMap;

	// 실제 텍스처 생성 함수 (DirectX 9)
	TextureNode CreateTextTexture(
		const std::string& text,
		std::string fontName,
		int fontWeight,
		int fontSize,
		int align,
		DWORD color,
		DWORD outlineColor,
		int outlineOffset,
		DWORD glowColor,
		int sizeX,
		int sizeY
	);
};
