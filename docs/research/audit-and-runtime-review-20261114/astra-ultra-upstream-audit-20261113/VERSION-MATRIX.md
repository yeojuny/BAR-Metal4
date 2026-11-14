> Publication copy: original conclusions and historical version scope preserved. Personal absolute paths were neutralized; source links are pinned to the audited a33ebfe revision. Raw logs, JSON manifests, binaries, screenshots and the original plan are not included in this Markdown-only export. See the parent README for later runtime results and limitations.

# 소스·바이너리·패치 대응표

2026-11-13 America/Los_Angeles. 원본은 수정하지 않았으며 패치는 메모리에서만 재구성했다. 전체 SHA256과 실제 파일 경로, 서명 검증 출력, 의존성 및 wrapper는 JSON manifest (`LOCAL-EVIDENCE/SOURCE-BINARY-PATCH-MANIFEST.json`; not included in this Markdown-only export)에 있다.

## 고정점

| 대상 | 정확한 버전 | 관측 범위 |
|---|---|---|
| 원격 감사 branch | `a33ebfe888bc8abe0053df05e5e2892c0788d6a8` | 시작 시 ls-remote, base 대비 1 commit/34 파일(19 production+7 docs+8 test runner) |
| 원격 main | `7f37af54f8a95926a8474f8acb595a68e98ae50a` | 계획 문서 및 19 대응 파일 조회. 현재 branch와 코드 동등하지 않음 |
| 로컬 HEAD | `6e9ce8f89afb14d632cce33e4742bab11b76b60f` | 19 production + 9 vendor dirty |
| 계획 문서 | blob `f3d256edc12bec6964065095d4e409412eb88a87` | UTF-8 220,082 bytes, 2,116 lines. 별도 PLAN-original.md |
| cloud 표기 | `937539c` | 공개 GitHub commit 조회 422. 제공 hash표 19파일은 working/current branch와 일치 |

## 패치 순서

| 순서 | 파일 | 변경 파일 수 | SHA256 |
|---|---|---:|---|
| 1 | `mac-compile-fixes.patch` | 9 | `c7dfd4753ac97d87510a8f00204a2e64aff3d58f13c49b95da01611e56abe974` |
| 2 | `review-r1-r5-production.patch` | 8 | `594d566cdf06b9bb108ce445547a9a6dee0a962f3d894f6fc6a5e44ac4697dd9` |
| 3 | `review-source-completion.patch` | 2 | `fbddbb136ca5a91738cf0b4f42ba7d8bf6d4abafd283ce244dbaeb00eaa19b89` |

`6e9ce8f + mac9 + review8 + companion2`를 원래 hunk/context 그대로 메모리에서 합성하면 현재 production 19파일과 전부 byte-exact 일치한다. 원격 a33ebfe의 19파일도 모두 같다. vendor 변경은 합성에서 제외했다.

## 파일별 포함 관계

경로는 repository 기준. M=mac compile patch, R=review-r1-r5 patch, C=completion patch. 최소판=base+M, 후속=base+M+R+C. `main=base`는 그 파일만의 동일성이다.

| 파일 | 패치 | 최소판=현재 | 후속=현재=remote | main=base | working SHA256 앞12자리 |
|---|---|---|---|---|
| `engine/rts/Lua/LuaNativeCUSSource.cpp` | M | 동일 | 동일 | 다름 | `a682e97ec47d` |
| `engine/rts/Rendering/Backend/NativeDeferredColorData.cpp` | M | 동일 | 동일 | 다름 | `59315dd79092` |
| `engine/rts/Rendering/Backend/NativeDeferredColorData.h` | M | 동일 | 동일 | 다름 | `70c3a8c17ecc` |
| `engine/rts/Rendering/Backend/NativeMapTexturePublication.cpp` | M | 동일 | 동일 | 다름 | `96890388457a` |
| `engine/rts/Rendering/Backend/NativeMapTexturePublication.h` | M | 동일 | 동일 | 다름 | `f291131ff36d` |
| `engine/rts/Rendering/Backend/NativeMetalInfoCapture.cpp` | R | 다름 | 동일 | 동일 | `a4620ba737fa` |
| `engine/rts/Rendering/Backend/NativeMetalInfoCapture.h` | R | 다름 | 동일 | 동일 | `7c22d13003b2` |
| `engine/rts/Rendering/Backend/NativeShieldDepthBacking.cpp` | M | 동일 | 동일 | 다름 | `01f0f5ba49f8` |
| `engine/rts/Rendering/Backend/NativeShieldDepthBacking.h` | M | 동일 | 동일 | 다름 | `c0b80b6111c8` |
| `engine/rts/Rendering/Backend/NativeVolumetricCloudContext.cpp` | M | 동일 | 동일 | 다름 | `99c1f42a35d1` |
| `engine/rts/Rendering/Metal4/Metal4CUSSource.mm` | R | 다름 | 동일 | 다름 | `3b9a02a3b30e` |
| `engine/rts/Rendering/Metal4/Metal4MetalInfoBridge.mm` | R | 다름 | 동일 | 동일 | `981ae4e83493` |
| `engine/rts/Rendering/Metal4/Metal4OpaqueModelSource.h` | M | 동일 | 동일 | 다름 | `58e76e023715` |
| `engine/rts/Rendering/Metal4/Metal4PostprocessNative.h` | C | 다름 | 동일 | 동일 | `09535f733d43` |
| `engine/rts/Rendering/Metal4/Metal4PostprocessNative.mm` | R | 다름 | 동일 | 다름 | `4c4bd941a8bd` |
| `engine/rts/Rendering/Metal4/Metal4SceneBridge.cpp` | R | 다름 | 동일 | 다름 | `1e4bc111bd2f` |
| `engine/rts/Rendering/Metal4/Metal4SceneBridge.h` | R | 다름 | 동일 | 다름 | `d33ed6208154` |
| `engine/rts/Rendering/Metal4/Metal4UnitUniformCopy.mm` | R | 다름 | 동일 | 동일 | `ce0faa76f6f0` |
| `engine/rts/Rendering/Metal4/Metal4WorldRenderer.mm` | C | 다름 | 동일 | 다름 | `b5fac9ec7138` |

main의 대응 19파일 중 5개가 base와 같고, 현재 working과 같은 파일은 0개다. 이 19파일 비교를 main 전체 감사로 확장하지 않는다.

## vendor 원본 보존

| 파일 | CRLF→LF 정규화 동일 | base CRLF | working CRLF |
|---|---|---:|---:|
| `engine/rts/lib/tracy/nfd/nfd_win.cpp` | True | 969 | 969 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/benchmarks/bench.cpp` | True | 472 | 472 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/benchmarks/ext/1024cores/spscqueue.h` | True | 139 | 139 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/benchmarks/systemtime.cpp` | True | 137 | 137 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/benchmarks/systemtime.h` | True | 33 | 33 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/readerwritercircularbuffer.h` | True | 288 | 288 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/tests/common/simplethread.cpp` | True | 82 | 82 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/tests/common/simplethread.h` | True | 154 | 154 |
| `engine/tools/pr-downloader/src/lib/readerwriterqueue/tests/stabtest/stabtest.cpp` | True | 80 | 80 |

9개 모두 줄바꿈 정규화 후 동일하다. production 패치에 포함하지 않았으며 원본을 보존했다.

## 실행 파일과 검증 강도

| 산출물 | SHA256 | 증거 |
|---|---|---|
| `BAR P5 Minimal Test.app` signed engine | `e1d1d12e7bc51872747c0e368426c820e5692413fc029e37727bad9e14caef50` | binary 및 outer deep/strict codesign 재검증 성공; arm64 |
| `BAR Review Ready.app` signed engine | `2f00fd56cd057c4baf6eb720442a5c71342e2e45aee0a5cf2fcefb32f4b601cb` | binary 및 outer deep/strict codesign 재검증 성공; arm64 |
| `build-native/spring-metal4-scene` link output | `1df4cf750c8981bc13ddbeb63215a608bb56967416dc9fc25721559fd0d6134e` | 후속17-step 로그의 link와 일치하는 현재 파일, 앱 signed 파일과 별개 |

Minimal 앱은 base+M에 대응한다는 BUILD-NOTES와 패치/기존 build 로그가 있고, 실제 lobby log에 sim616/3605 presented-count 및 sim2002/10619 presented-count 정상 종료가 있다. 후속 Review Ready는 다른 binary이며 실행 성공을 전이할 수 없다. 이번 감사에서 어느 실행 파일도 실행하지 않았다.

codesign 유효성은 현재 번들의 변조 여부 확인이며, source-to-binary reproducible build 증명은 아니다. signed/link 바이너리 차이를 서명만의 차이라고 단정하지 않는다.

## 빌드 및 실행 환경

- CMake 4.5.0 기록, AppleClang 21.0.0.21000401, MacOSX27.2 SDK, C++23, arm64. Mach-O LC_BUILD_VERSION의 minos/sdk 모두 27.2.
- RelWithDebInfo 기본 -O2 -g -DNDEBUG. 그러나 BAR_NATIVE_INTERVENTION_RENDERER_OPTIMIZE=ON이며 실제 WorldRenderer ObjC++ recipe 끝은 -O3 -ffp-contract=fast. 앞선 -ffp-contract=off만 읽으면 잘못된 결론이다.
- 선택한 최종 link recipe에는 -flto가 없다. IPO 지원 검사/옵션명만으로 LTO 적용을 주장하지 않는다.
- 두 wrapper는 render worker=1, BeginReady=${...:-0}. inherited 환경의 opt-in 값은 완전히 지우지 않는다. measured233의 11flags와 accepted245의 13flags를 이 wrapper가 모두 명시적으로 고정하지 않는다.
- 두 wrapper의 Metal API/Shader Validation 기본값은 0. 기존 정상 실행을 Validation PASS로 취급하지 않는다.
- 현재 binary는 /opt/homebrew의 DevIL/minizip/freetype/fontconfig/vorbis/ogg/openal-soft/SDL3와 /opt/homebrew/lib rpath에 의존. wrapper는 Homebrew content도 요구한다. 전체 전이 의존성 bytes는 미고정.

## UI pair

| 파일 | 두 앱의 동일 SHA256 | source strict SHA512 pin |
|---|---|---|
| `gui_options.lua` | `749d590927037421a3ccb6f7c3b78e53c4d6f187a46d809a8f9b447b55a68b8b` | 일치 (True) |
| `gui_healthbars_gl4.lua` | `04488b0fdf28600a600b28d580c9b72f41a53081396cf442f5b6e4638099f49d` | 일치 (True) |

source의 NativeWidgetAdapters.h와 실제 두 앱의 SHA512 일치도 확인했다. 이는 공개 v1.11.2 계열 pair이며 unavailable measured233 UI bytes와 동일하다는 의미는 아니다. Native233 overlay 31파일만으로 measured233 binary/UI 전체가 재현된다는 주장은 원래 문서도 하지 않는다.

## 공개 코드 재현 가능성

- a33ebfe는 후속 Review Ready에 사용된 production source 집합을 제공한다. 이 범위에서 “9개 수정은 아직 GitHub에 없다”는 상태는 더 이상 현재 사실이 아니다.
- a33ebfe 그대로는 최초 Minimal source와 다르다. Minimal을 재구성하려면 6e9ce8f와 정확한9파일 patch(지금은 remote에 그 postimage가 공개됨)를 선택해야 한다.
- source 대응은 닫혔지만, 공개 branch 단독으로 어느 signed 앱의 동일 bytes 또는 portable runtime closure를 재현한다고 인증할 수 없다. toolchain/recipe/UI/game/map/dependencies/서명 recipe의 묶음이 필요하다.
- Review Ready engine.sourceCommit=937539c 및 과거 checkpoint/source manifest 필드는 현재 공개 a33ebfe와 이 대응표를 스스로 표현하지 않는다. manifest를 고칠 다음 작업에서는 full public commit과 patch/tree digest를 함께 적어야 한다. 이번 감사에서는 원본 manifest를 수정하지 않았다.
