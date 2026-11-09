#include "stdafx.h"
#include "FontTextureCache.h"
#include <algorithm>
#include <cmath>

FontTextureCache::~FontTextureCache()
{
	Clear();
}

bool FontTextureCache::Init(LPDIRECT3DDEVICE9 device)
{
	if (m_pDevice != device)
		Clear();
	m_pDevice = device;
	return (device != nullptr);
}

void FontTextureCache::Clear()
{
	// 속성 기반 캐시 정리
	for (auto& it : m_cacheMap)
	{
		if (it.second.pTexture)
			it.second.pTexture->Release();
	}
	m_cacheMap.clear();

	// 인덱스 기반 캐시 정리
	for (auto& it : m_indexCacheMap)
	{
		if (it.second.pTexture)
			it.second.pTexture->Release();
	}
	m_indexCacheMap.clear();
}

void FontTextureCache::Release()
{
	Clear();
}

const FontTextureCache::TextureNode* FontTextureCache::GetCache(int textIndex)
{
	auto it = m_indexCacheMap.find(textIndex);
	if (it != m_indexCacheMap.end())
		return &it->second;
	else
		return nullptr;
}

const FontTextureCache::TextureNode* FontTextureCache::GetCache(
	const std::string& text,
	std::string fontName,
	int fontWeight,
	int fontSize,
	DWORD color,
	DWORD outlineColor,
	DWORD glowColor,
	int align,
	int outlineOffset,
	int sizeX,
	int sizeY)
{
	FontCacheKey key = { text, fontName, fontWeight, fontSize, align, outlineOffset, color, outlineColor, glowColor, sizeX, sizeY };
	auto it = m_cacheMap.find(key);
	if (it != m_cacheMap.end())
		return &it->second;
	else
		return nullptr;
}

FontTextureCache::TextureNode* FontTextureCache::CreateCache(
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
	int sizeX,
	int sizeY)
{
	TextureNode tex = CreateTextTexture(text, fontName, fontWeight, fontSize, align,
		color, outlineColor, outlineOffset, glowColor, sizeX, sizeY);
	if (!tex.pTexture)
		return nullptr;

	auto it = m_indexCacheMap.find(textIndex);
	if (it != m_indexCacheMap.end())
	{
		if (it->second.pTexture)
			it->second.pTexture->Release();
		it->second = tex;
		return &it->second;
	}
	return &m_indexCacheMap.emplace(textIndex, tex).first->second;
}

FontTextureCache::TextureNode* FontTextureCache::CreateCache(
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
	int sizeY)
{
	FontCacheKey key = { text, fontName, fontWeight, fontSize, align, outlineOffset, color, outlineColor, glowColor, sizeX, sizeY };

	TextureNode tex = CreateTextTexture(text, fontName, fontWeight, fontSize,
		align, color, outlineColor, outlineOffset, glowColor, sizeX, sizeY);
	if (!tex.pTexture)
		return nullptr;

	auto it = m_cacheMap.find(key);
	if (it != m_cacheMap.end())
	{
		if (it->second.pTexture)
			it->second.pTexture->Release();
		it->second = tex;
		return &it->second;
	}
	return &m_cacheMap.emplace(key, tex).first->second;
}

void FontTextureCache::RemoveCache(int textIndex)
{
	auto it = m_indexCacheMap.find(textIndex);
	if (it != m_indexCacheMap.end()) {
		if (it->second.pTexture) {
			it->second.pTexture->Release();
		}
		m_indexCacheMap.erase(it);
	}
}

bool FontTextureCache::HasCache(int textIndex) const
{
	return m_indexCacheMap.find(textIndex) != m_indexCacheMap.end();
}

FontTextureCache::TextureNode FontTextureCache::CreateTextTexture(
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
	int sizeY)
{
	TextureNode result = {};
	if (!m_pDevice || text.empty() || fontSize <= 0)
		return result;

	LPD3DXFONT pFont = nullptr;
	if (FAILED(D3DXCreateFontA(m_pDevice, fontSize, 0, fontWeight, 1, false, DEFAULT_CHARSET,
		OUT_OUTLINE_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
		fontName.c_str(), &pFont)) || !pFont)
		return result;

	RECT rect = { 0,0,0,0 };
	pFont->DrawTextA(nullptr, text.c_str(), -1, &rect, DT_CALCRECT, D3DCOLOR_XRGB(0, 0, 0));
	const int texWidth = (std::max)((std::max)(rect.right - rect.left, sizeX), 1);
	const int texHeight = (std::max)((std::max)(rect.bottom - rect.top, sizeY), 1);
	if (FAILED(m_pDevice->CreateTexture(texWidth, texHeight, 1, D3DUSAGE_RENDERTARGET,
		D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &result.pTexture, nullptr)) || !result.pTexture)
	{
		pFont->Release();
		return result;
	}

	LPDIRECT3DSURFACE9 pOldRT = nullptr;
	LPDIRECT3DSURFACE9 pNewRT = nullptr;
	LPD3DXSPRITE pSprite = nullptr;
	IDirect3DStateBlock9* pStateBlock = nullptr;
	D3DVIEWPORT9 oldViewport = {};
	bool viewportSaved = false;
	bool targetChanged = false;
	bool spriteBegun = false;

	// 텍스처 생성 중 변경한 장치 상태는 성공 여부와 관계없이 복구합니다.
	auto finish = [&](bool success) -> TextureNode {
		if (spriteBegun && pSprite)
			pSprite->End();
		if (targetChanged && pOldRT)
			m_pDevice->SetRenderTarget(0, pOldRT);
		if (pStateBlock)
		{
			pStateBlock->Apply();
			pStateBlock->Release();
		}
		if (viewportSaved)
			m_pDevice->SetViewport(&oldViewport);
		if (pSprite) pSprite->Release();
		if (pNewRT) pNewRT->Release();
		if (pOldRT) pOldRT->Release();
		pFont->Release();
		if (!success && result.pTexture)
		{
			result.pTexture->Release();
			result.pTexture = nullptr;
		}
		return result;
	};

	if (FAILED(result.pTexture->GetSurfaceLevel(0, &pNewRT)) || !pNewRT ||
		FAILED(m_pDevice->GetRenderTarget(0, &pOldRT)) || !pOldRT)
		return finish(false);
	viewportSaved = SUCCEEDED(m_pDevice->GetViewport(&oldViewport));
	if (!viewportSaved ||
		FAILED(m_pDevice->CreateStateBlock(D3DSBT_ALL, &pStateBlock)) || !pStateBlock ||
		FAILED(pStateBlock->Capture()) ||
		FAILED(D3DXCreateSprite(m_pDevice, &pSprite)) || !pSprite)
		return finish(false);

	if (FAILED(m_pDevice->SetRenderTarget(0, pNewRT)))
		return finish(false);
	targetChanged = true;

	// 뷰포트 설정
	D3DVIEWPORT9 vp = { 0, 0, (DWORD)texWidth, (DWORD)texHeight, 0.0f, 1.0f };
	if (FAILED(m_pDevice->SetViewport(&vp)))
		return finish(false);

	// 알파 블렌딩 설정
	if (FAILED(m_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE)) ||
		FAILED(m_pDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA)) ||
		FAILED(m_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA)) ||
		FAILED(m_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0)))
		return finish(false);

	RECT drawRect = { 0, 0, texWidth, texHeight };
	const int glowRadius = 5;

	if (FAILED(pSprite->Begin(D3DXSPRITE_SORT_TEXTURE | D3DXSPRITE_ALPHABLEND)))
		return finish(false);
	spriteBegun = true;

	// 4. Glow 효과 렌더링 (알파 감쇠)
	if (D3DCOLOR_GETA(glowColor) > 0)
	{
		for (int y = -glowRadius; y <= glowRadius; y++) {
			for (int x = -glowRadius; x <= glowRadius; x++) {
				float distance = sqrtf(float(x * x + y * y));
				if (distance > glowRadius) continue;

				float factor = 1.0f - (distance / glowRadius);
				BYTE alpha = (BYTE)(D3DCOLOR_GETA(glowColor) * sqrtf(factor));

				if (alpha < 15) continue;

				D3DCOLOR softGlowColor = D3DCOLOR_ARGB(alpha, D3DCOLOR_GETR(glowColor), D3DCOLOR_GETG(glowColor), D3DCOLOR_GETB(glowColor));
				RECT glowRect = drawRect;
				OffsetRect(&glowRect, x, y);
				pFont->DrawTextA(pSprite, text.c_str(), -1, &glowRect, align, softGlowColor);
			}
		}
	}

	// 5. 아웃라인 렌더링
	if (outlineOffset > 0)
	{
		int samples = 16;
		for (int a = 0; a < samples; a++) {
			float angle = (float)a / samples * D3DX_PI * 2.0f;
			int dx = (int)(cos(angle) * outlineOffset);
			int dy = (int)(sin(angle) * outlineOffset);
			RECT outlineRect = drawRect;
			OffsetRect(&outlineRect, dx, dy);
			pFont->DrawTextA(pSprite, text.c_str(), -1, &outlineRect, align, outlineColor);
		}
	}

	// 6. 메인 텍스트 렌더링
	pFont->DrawTextA(pSprite, text.c_str(), -1, &drawRect, align, color);
	const bool spriteEnded = SUCCEEDED(pSprite->End());
	spriteBegun = false;
	if (!spriteEnded)
		return finish(false);

	result.width = texWidth;
	result.height = texHeight;

	// 결과값 저장 (추후 참조용)
	result.TextColor = color;
	result.OutlineColor = outlineColor;
	result.GlowColor = glowColor;
	result.fOutlineSize = (float)outlineOffset;

	return finish(true);
}
