# Malloc Lab

이 저장소는 **CS:APP Malloc Lab**을 Docker + VSCode DevContainer 환경에서 바로 실습할 수 있도록 구성한 저장소입니다.  
핵심 목표는 **C 언어로 `malloc`, `free`, `realloc`을 직접 구현**해 보면서 동적 메모리 할당기와 힙 메모리 관리 방식을 이해하는 것입니다.

지난 주에 사용했던 `malloc`을 이번 주에는 직접 만들어 봅니다.  
랩 코드를 수정하고 채점 프로그램을 실행하면서 점수를 확인할 수 있으므로, 메모리와 포인터를 손으로 다뤄 보는 데 집중할 수 있습니다.

## 학습 목표

- 동적 메모리 할당이 어떻게 동작하는지 이해하기
- C 언어 포인터 연산과 메모리 레이아웃에 익숙해지기
- 힙 블록의 분할, 병합, 재사용 과정을 직접 구현해 보기
- 메모리 단편화와 할당 정책의 trade-off를 체감하기
- `implicit free list`를 시작으로 더 나은 allocator 설계를 확장해 보기

## 학습 키워드

- 시스템 콜
- 데이터 세그먼트와 힙
- 가상 메모리
- 메모리 단편화
- `sbrk`, `mmap`
- alignment
- header / footer
- boundary tag
- coalescing
- free list

## 이 저장소에서 구현하는 것

과제의 중심은 `malloc-lab/mm.c`입니다. 아래 4개 함수를 직접 구현합니다.

- `mm_init`
- `mm_malloc`
- `mm_free`
- `mm_realloc`

작성한 코드는 `malloc-lab/mdriver`가 테스트하고 채점합니다.  
처음에는 **implicit list 방식**으로 기본 동작을 완성하는 것이 목표이고, 여유가 있다면 다음 방식으로 개선해 볼 수 있습니다.

- explicit free list
- segregated free list
- buddy system

## 실제 시스템과의 연결

실제 운영체제의 메모리 할당기는 필요할 때 커널에 메모리를 요청하며, 이 과정에서 `sbrk`나 `mmap` 같은 메커니즘이 등장합니다.  
이 저장소에서는 `malloc-lab/memlib.c`의 `mem_sbrk`가 그 역할을 단순화해서 흉내 냅니다. 덕분에 운영체제 의존성보다는 **allocator의 정책과 자료구조**에 집중할 수 있습니다.

즉, 이 랩은 단순히 함수를 몇 개 작성하는 과제가 아니라 다음 질문에 답해 보는 과정입니다.

- 빈 블록을 어떻게 찾을 것인가?
- 남는 공간은 어떻게 분할할 것인가?
- 해제된 블록은 언제, 어떻게 병합할 것인가?
- `realloc`은 복사를 최소화하면서 어떻게 처리할 것인가?
- 성능과 메모리 효율 사이에서 어떤 선택을 할 것인가?

## 저장소 구성

```text
malloc_lab_docker/
├── .devcontainer/            # Docker / DevContainer 설정
├── .vscode/                  # VSCode 빌드/디버깅 설정
├── README.md                 # 저장소 안내 문서
└── malloc-lab/
    ├── mm.c                  # 직접 구현할 allocator 코드
    ├── mm.h                  # allocator 인터페이스
    ├── mdriver.c             # 테스트 및 채점 드라이버
    ├── memlib.c              # 힙과 mem_sbrk를 흉내 내는 메모리 모델
    ├── traces/               # 다양한 할당/해제 패턴 테스트 케이스
    ├── Makefile              # 빌드 스크립트
    ├── README.md             # 원본 handout에 가까운 간단한 안내
    ├── 7주차_학습가이드_핵심요약.md
    ├── CSAPP 9장 입문편 — 가상 메모리 (9.1_9.12 전체).pdf
    └── CSAPP 9장 심화편 — 가상 메모리 (9.1_9.12 전체).pdf
```

## 시작 방법

### 1. 저장소 열기

```bash
git clone --depth=1 https://github.com/krafton-jungle/malloc_lab_docker.git
cd malloc_lab_docker
```

VSCode에서 폴더를 연 뒤 `Dev Containers: Reopen in Container`를 실행하면 동일한 C 개발 환경으로 실습할 수 있습니다.

### 2. 빌드

```bash
cd malloc-lab
make
```

### 3. 기본 테스트 실행

```bash
./mdriver
```

특정 trace만 골라서 실행할 수도 있습니다.

```bash
./mdriver -f traces/binary2-bal.rep
```

VSCode에서는 `F5`로 `mdriver -V -f short1-bal.rep` 디버깅을 바로 시작할 수 있습니다.

## 권장 진행 순서

1. `make` 후 `./mdriver`를 실행해 현재 상태를 확인합니다.
2. 초기에는 `out of memory` 오류가 날 수 있으니, 먼저 **implicit list 기반 malloc**을 구현해 이 오류를 없앱니다.
3. 블록 분할, 병합, 재할당 로직을 정리해 기본 점수를 확보합니다.
4. 이후 `explicit list`, `seglist` 같은 구조를 적용해 성능과 utilization 점수를 높여 봅니다.

개념이 어렵다면 **CS:APP 3/e 9.9장**을 천천히 읽으면서 진행하는 것이 가장 좋습니다.  
교재의 코드를 이해하고 직접 옮겨 써서 동작시켜 보는 것부터 시작하면 됩니다.

## 참고 자료

- CMU Malloc Lab 문서: [malloclab.pdf](http://csapp.cs.cmu.edu/3e/malloclab.pdf)
- 저장소 내 학습 자료:
  - `malloc-lab/CSAPP 9장 입문편 — 가상 메모리 (9.1_9.12 전체).pdf`
  - `malloc-lab/CSAPP 9장 심화편 — 가상 메모리 (9.1_9.12 전체).pdf`
  - `malloc-lab/7주차_학습가이드_핵심요약.md`

## 요약

이 저장소는 단순한 실행 환경이 아니라, **메모리 할당기를 직접 구현하며 시스템 프로그래밍 감각을 익히는 실습 저장소**입니다.  
`mm.c`를 고치고 `mdriver`로 검증하면서, 포인터·힙·단편화·할당 전략이 실제로 어떤 의미를 가지는지 확인해 보세요.
