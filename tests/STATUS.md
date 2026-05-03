# MP1 Test Bench — STATUS

자체 검증 인프라 구현 진행 현황. Plan: `~/.claude/plans/greedy-scribbling-willow.md`.

## Goals

- 작년 AG가 검사하던 24개 case (`tmp/sp25_mp1_final_ag_report.txt`) 자체 재현
- Register clobber (callee-saved violation) 검출
- NULL deref 등 fault 발생 시 다음 test로 graceful 회복
- 픽셀 단위 framebuffer 검증
- 보너스 ~10 custom 테스트 (점수 외 진단용)
- 출력은 AG report 포맷과 흡사 → 작년 결과(67/85)와 직접 비교

## Phase progress

- [x] **A+B. Skeleton + Foundation** — 디렉토리, weak stubs, halt swap, banner
- [x] **C. Recovery infra** — `setjmp.S` + `fault_handler` longjmp 분기, NULL deref smoke test 통과
- [x] **D. Clobber wrapper** — `clobber.S` 6-arg trampoline, 12-bit sentinel mask, good/bad self-test 통과
- [x] **E. Framebuffer infra** — `fb.h/fb.c` 1.2MB×2 fb, clear/diff/summary helpers, 3개 ref drawers, smoke 통과
- [x] **F. Tests** — runner + AG-style report + 24 baseline + 10 extras (pristine = 0/85, 0/10 모두 missing)
- [x] **G. Verification & polish** — pristine link audit, baseline output 캡처, sign-off; mutation testing은 사용자 구현 진행하면서 자연스럽게 검증

## File inventory (current)

| File | Status | Notes |
| --- | --- | --- |
| `test_framework.h` | minimal | header w/ stub address externs + globals reset proto |
| `test_globals.c` | minimal | 4 skyline globals + reset() |
| `test_stubs.S` | done (Phase A) | 8 weak stubs, each with `_stub_<fn>` alias for address comparison |
| `halt_replace.c` | Phase C done | exports halt_success/failure/panic/fault_handler. fault_handler longjmps when `test_recover_armed` set |
| `test_main.c` | Phase C done | banner + recovery smoke test + impl status, halt_success |
| `setjmp.S` | Phase C done | RV64 setjmp/longjmp (13 doublewords: ra, sp, s0-s11) |
| `clobber.S` | Phase D done | 6-arg trampoline + sentinel pattern (`0xC10BBE5...`) + 12-bit mask global; includes good/bad self-test helpers |
| `fb.c` / `fb.h` | Phase E done | actual + expected fb (각 1.2MB), clear/diff/summary, ref_draw_{star,window,beacon} |
| `tests_star.c` | Phase F2 done | 8 cases, 30pt total |
| `tests_window.c` | Phase F2 done | 10 cases, 35pt total |
| `tests_beacon.c` | Phase F2 done | 6 cases, 20pt total |
| `tests_extra.c` | Phase F3 done | 10 bonus diagnostic cases |
| `test_runner.c` | Phase F1 done | run_test, run_test_group, AG-style formatter, addr_eq helper |
| (no separate `report.c`) | merged | runner emits AG-style output directly via kprintf |

## Test inventory (24 baseline + extras)

### Star (30pt) — TODO

| Case | Pts | Status |
| --- | --- | --- |
| add_single_star | 4 | implemented |
| add_multiple_stars | 6 | implemented |
| remove_single_star | 2 | implemented |
| remove_multiple_star | 4 | implemented |
| remove_star_not_in_list | 4 | implemented |
| draw_star_simple | 4 | implemented |
| draw_star_null | 1 | implemented |
| draw_star_complex | 5 | implemented |

### Window (35pt) — TODO

| Case | Pts | Status |
| --- | --- | --- |
| add_one_window | 4 | implemented |
| add_multiple_windows | 6 | implemented |
| remove_one_window | 2 | implemented |
| remove_multiple_window | 4 | implemented |
| remove_window_not_in_array | 4 | implemented |
| draw_window_simple | 2 | implemented |
| draw_window_null | 1 | implemented |
| draw_window_clipping_side | 4 | implemented |
| draw_window_clipping_bottom | 4 | implemented |
| draw_window_outside | 4 | implemented |

### Beacon (20pt) — TODO

| Case | Pts | Status |
| --- | --- | --- |
| start_beacon | 5 | implemented |
| draw_beacon_simple | 2 | implemented |
| draw_beacon_null | 1 | implemented |
| draw_beacon_clipping_side | 4 | implemented |
| draw_beacon_clipping_bottom | 4 | implemented |
| draw_beacon_outside | 4 | implemented |

### Extras (10, 진단용) — TODO

| Case | 검출 대상 |
| --- | --- |
| star_stress | malloc/free 누수 |
| window_overflow | cap 4000 |
| window_remove_compaction | array 무결성 |
| draw_star_corners | 좌표 경계 |
| draw_window_full_screen | uint8_t 한계 |
| beacon_period_zero | div-0 fault recovery |
| beacon_t_wrap | uint64 modulo 64-bit clean |
| start_beacon_struct_layout | byte-level field offset |
| clipping_negative | uint16 huge값 |
| callee_saved_isolated | 어느 함수의 clobber인지 격리 |

## Verification log

### 2026-04-29 — Phase A+B baseline

- `make clean && make test.elf` → 빌드 성공, warning 0
- `make run-test` (또는 `qemu-system-riscv64 ... -kernel test.elf`)
  - banner 출력
  - 8개 함수 모두 `MISSING (weak stub)` 보고 (pristine `mp1.S` 상태)
  - `halt_success` → QEMU exit code 0
- weak stub address 비교 메커니즘 동작 확인됨

### 2026-04-29 — Phase C recovery infra

- `setjmp.S` + `fault_handler` longjmp 분기 추가
- `recovery_smoke_test()` 가 NULL pointer 의도적 deref:
  - load access fault 발생 (`mcause=5`)
  - `fault_handler`가 cause/mepc 캡처 후 longjmp
  - setjmp 호출 지점 복귀, 후속 코드 정상 진행
  - PASS 출력 후 halt_success
- 결과: pristine `mp1.S`로 빌드/실행, recovery smoke + 8 missing detect + clean exit

### 2026-04-29 — Phase D clobber wrapper

- `clobber.S` 6-arg trampoline 추가 (sentinel pattern `0xC10BBE5000000000+N`)
- 12-bit `clobber_mask` global, bit N = sN 침해 표시
- `clobber_smoke_test()` 자가 검증:
  - 정상 fn (`clobber_test_good`, bare ret) → mask=0x0 PASS (false positive 없음)
  - 침해 fn (`clobber_test_bad_s0`, s0를 garbage로 덮음) → mask=0x1 PASS (정확히 s0 비트만)
- 결과: pristine 빌드 clean, recovery+clobber smoke 모두 PASS, clean exit

### 2026-04-29 — Phase E framebuffer infra

- `fb.h/fb.c` 추가: actual + expected (각 614400 bytes, 총 ~1.2MB BSS)
- `fb_clear`, `fb_clear_both`, `fb_diff_count`, `fb_print_diff_summary`, `fb_in_screen`
- C reference drawers: `ref_draw_star`, `ref_draw_window`, `ref_draw_beacon`
  - 좌표 클리핑 / NULL 체크 / beacon time-gate / period 0 방어 모두 spec대로 구현
- `fb_smoke_test()` 자가 검증:
  - 둘 다 0 clear → diff=0 PASS
  - 한 픽셀 다르게 set → diff=1 PASS
  - ref_draw_star + 동일 직접 write → diff=0 PASS
- 결과: pristine `mp1.S`로 4개 smoke (recovery + clobber good/bad + fb 3가지) 모두 PASS, 8 missing detect, clean exit

### 2026-04-29 — Phase F1+F2+F3 test runner + 24 baseline + 10 extras

- `test_runner.c` 추가: `run_test`, `run_test_group`, AG-style formatter, addr_eq helper (GCC 컴파일타임 fold 우회)
- `test_framework.h`: `test_result`/`test_entry`/`test_fn_t` types, `REQUIRE_IMPL` macro (addr_eq 기반)
- `tests_star.c`: 8 cases (30pt) — add/remove/draw star + null + complex
- `tests_window.c`: 10 cases (35pt) — add/remove/draw window + clipping side/bottom/outside
- `tests_beacon.c`: 6 cases (20pt) — start_beacon + draw clipping/null/outside
- `tests_extra.c`: 10 bonus diagnostic cases (stress, overflow, corner, struct layout, t_wrap, period_zero 등)
- 결과: pristine `mp1.S`로 빌드 시 24개 모두 FAIL "function not implemented (weak stub)", subtotal 0/85, extras 0/10 passed, clean exit
- AG report 포맷 일치 (`>---------<NAME>---PASSED|FAILED`, ` PASS` / `ERROR DETECTED:\n<reason>`, `Score: x/y`)

### 2026-04-29 — Phase G verification & polish

- Pristine link audit: `riscv64-unknown-elf-nm test.elf | grep -E ' [WT] (add_star|remove_star|...)' ` → 8개 모두 `W` (weak), 인접 주소 (각 2바이트 c.ret stub). 학생이 mp1.S를 채우면 자동으로 `T` (strong)로 전환.
- Baseline output `test/expected_pristine_output.txt` 캡처 (251줄). 향후 회귀 비교용. 단 `mepc=0x...` 주소는 빌드마다 다름 (recovery_smoke_test의 NULL deref 위치) — diff 시 그 줄만 normalize 필요.
- Banner 문구 "Phase C build" 제거 (의미 stale).
- 사용자 구현 진행하며 mutation 자연 검증: function 채우면 해당 test PASS로 전환, ABI 위반시 clobber penalty 자동 표시, NULL miss시 fault recovery로 자동 catch.

## Known limitations / TODOs

- **Memory leak detection**: `memory.o` 소스 비공개라 자동 hook 불가. 구조적 invariant (list length, win_cnt)만 검사. 바이트 단위 누수 검출은 수동 GDB 점검에 의존.
- **Style 채점**: 코멘트 품질, label naming 같은 휴먼 판단 필요 → 자동 안 함. README 자가 체크리스트 참고.

## Sign-off criteria

이 test bench가 "완성"되려면:

1. ✅ Phase A+B: pristine 빌드 / banner / 8 missing detect
2. ✅ Phase C: 의도적 NULL deref 후 후속 test 정상 진행
3. ✅ Phase D: 의도적 s0 clobber → mask=0x1 정확히 보고
4. ✅ Phase E: ref drawer로 expected fb 채우고 diff 정확히 0 보고
5. ✅ Phase F: 24 baseline + 10 extras 모두 코드 작성, pristine = 0 score
6. ✅ Phase G: pristine link audit (8 W symbols), baseline output 캡처, sign-off

**🎯 Test bench 완성**. 사용자가 `mp1.S`를 채워가면서 추가 검증이 일어남:
- `add_star` 작성 → strong symbol overrides weak → relevant tests transition to PASS
- ABI 위반 시 → clobber_mask 자동 검출, -15 1회 적용
- NULL miss 시 → fault_handler가 recover, 해당 test FAIL with cause
- 픽셀 오류 시 → fb_diff_count > 0, fb_print_diff_summary로 위치 식별 가능

## Usage

```sh
cd redo/mp1
make test.elf       # build (works on pristine mp1.S thanks to weak stubs)
make run-test       # run in QEMU -nographic, prints AG-style report

# Quick regression check (output should be byte-identical to baseline
# except for variable mepc address in recovery smoke test):
qemu-system-riscv64 -machine virt -bios none -kernel test.elf \\
    -m 128M -serial mon:stdio -nographic | diff - test/expected_pristine_output.txt
```
