# 서효진 | 게임 클라이언트 프로그래머

Unreal Engine, C++, DirectX 기반 게임 클라이언트 개발 경험을 정리한 포트폴리오입니다. 구현 사례와 프로젝트 이력은 아래 PDF에서 확인할 수 있습니다.

[포트폴리오 PDF 보기](./SeoHyojin_Portfolio.pdf)

## 코드 샘플

| 파일 | 주요 구현 내용 |
| --- | --- |
| [UE4 전투·피격 처리](./samples/SampleCode_UE4_1.cpp) | 전투 이벤트 분기, 피격 상태와 넉백 처리, 타격 이펙트·사운드, 역경직(Hit Stop) 타이머 |
| [UE4 로코모션 회전](./samples/SampleCode_UE4_2.cpp) | 이동·조준 상태에 따른 캐릭터 회전, 회전 보간, 시선과 캐릭터 방향 차이 제한 |
| [DirectX 9 폰트 텍스처 캐시](./samples/FontTextureCache.cpp) · [헤더](./samples/FontTextureCache.h) | 텍스트 속성별 텍스처 캐싱, Glow·Outline 렌더링, DirectX 리소스와 렌더링 상태 관리 |
| [퀵메뉴 시스템](./samples/QuickMenuSystem.cpp) · [헤더](./samples/QuickMenuSystem.h) | 메뉴 슬롯 등록·해제, UI 이벤트 처리, 로컬 설정 저장·로드와 변경 상태 비교 |
| [cocos2d 퍼즐 보드](./samples/SampleCode_cocos2d.cpp) | 오브젝트 배치와 선택, 키워드 탐색, 콤보·피버·타이머·점수 처리 |

## 코드 범위

각 파일은 해당 기능의 핵심 로직을 보여주기 위해 선별한 프로젝트 코드 발췌본입니다. 원 프로젝트의 헤더·클래스·리소스 및 엔진 설정에 의존하므로 이 저장소만으로는 빌드되지 않습니다.
