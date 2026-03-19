# DO NOT EVER REMOVE ANY FUNCTION IMPLEMENTATION WITHOUT USER CONSENT

# CLAUDE.md — Development Instructions

Operational procedures, guardrails, and limitations for Claude Code agents on this project.

---

## Quick Rules

1. **ALWAYS** run `/stm32-safety-audit` on modified `.c`/`.h` files before commit
2. **ALWAYS** read files before proposing changes
3. **ALWAYS** build after significant changes
4. **ALWAYS** test memory impact before adding large buffers (>500 bytes RAM: ask user first)
5. **NEVER** re-initialize HAL peripherals in application code
6. **NEVER** use `__attribute__((packed))` structs for flash I/O
7. **NEVER** delete an active LVGL screen before loading the new one
8. **NEVER** commit broken code or CRITICAL audit findings unaddressed
9. **NEVER** modify or add code outside `USER CODE BEGIN` / `USER CODE END` blocks in CubeMX-generated files
10. Avoid overcomplicated code with excessive error checking

---

## Code Generation Rules

### No HAL Re-initialization

All STM32 HAL peripherals (SPI, ADC, TIM, OPAMP, GPIO, DMA, etc.) are already initialized by STM32CubeMX-generated code in `main.c`. **Do not touch them.**

- **Never add** `HAL_SPI_Init()`, `HAL_ADC_Init()`, `MX_GPIO_Init()`, etc. in application code
- **Never modify** `hspiX.Init.*`, `hadcX.Init.*` fields and then call `HAL_*_Init()`
- **Only use** `HAL_*_Start()`, `HAL_*_Stop()`, and runtime control functions
- Configuration changes must be made in the `.ioc` file and regenerated via STM32CubeMX

**Why**: Re-initializing peripherals at runtime corrupts DMA state machines and causes hard-to-debug artifacts.

### Preserve USER CODE Blocks

STM32CubeMX regenerates `Core/` files on any peripheral change. All custom code in those files must live inside USER CODE blocks:

```c
/* USER CODE BEGIN Includes */
#include "sensor.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
void my_function(void) { ... }
/* USER CODE END 0 */
```

**Safe zones** (not regenerated): `Application/User/Core/` and everything inside USER CODE blocks.
**Danger zones** (regenerated): `Core/Src/main.c`, `Core/Inc/main.h`, `Core/Src/stm32g4xx_it.c` — outside USER CODE blocks.

---

## Memory Management

### Monitor RAM Usage

Before implementing changes that add variables, buffers, or objects:

1. Estimate size of new allocations
2. Build and check: `arm-none-eabi-size stm32g474_PPO_Display.elf`
3. Inspect the map file for large allocations: `grep -A 5 ".bss" STM32CubeIDE/Debug/stm32g474_PPO_Display.map`
4. Keep reasonable headroom — if adding >500 bytes, confirm with user first

If RAM overflow occurs: reduce buffer sizes, use LVGL pool (`lv_malloc()`), move constants to flash (`const`).

### Memory Sections Reference

```
.bss     # Uninitialised global/static variables (RAM)
.data    # Initialised global/static variables (RAM)
.heap    # Dynamic allocation - malloc (RAM)
.stack   # Function call stack (RAM)
.text    # Program code (Flash)
.rodata  # Read-only data - const (Flash)
```

---

## Build System

### Build Commands

```bash
# Debug build
cd "G:/STM/g474_port/STM32CubeIDE/Debug"
make -j4 all

# Release build
cd "G:/STM/g474_port/STM32CubeIDE/Release"
make -j4 all

# Check memory usage
arm-none-eabi-size stm32g474_PPO_Display.elf

# Detailed .bss breakdown
arm-none-eabi-objdump -h stm32g474_PPO_Display.elf | grep .bss
```

If `make` is not in PATH, use the full path:
`C:\ST\STM32CubeIDE_1.19.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.make.win32_2.2.0.202409170845\tools\bin\make.exe`

### CRITICAL: First Build After Adding New Files

When adding new `.c`/`.h` files, the first build MUST be done inside STM32CubeIDE (Project → Build All / Ctrl+B). This regenerates `subdir.mk` and `objects.list`. After that, command-line `make` works normally.

### Build Output Indicators

| Output | Meaning | Action |
|--------|---------|--------|
| `Finished building target: stm32g474_PPO_Display.elf` | Success | Flash it |
| `region 'RAM' overflowed by X bytes` | Memory overflow | Reduce allocations |
| `undefined reference to 'function'` | Missing implementation | Add .c to build |
| `warning: large stack frame (>1024 bytes)` | Stack overflow risk | Reduce locals |

---

## Refactoring Safety Protocol

Before ANY major structural change (data layout, global state, flash storage format, multi-file interface):

```bash
# 1. Safety checkpoint commit
git add -A
git commit -m "Pre-refactoring checkpoint: <description>"

# 2. Verify rollback exists
git log --oneline

# 3. Make incremental changes, build frequently

# 4. Rollback if needed
git reset --hard <hash>
cd STM32CubeIDE/Debug && make clean && make all
```

**NEVER** commit broken or half-refactored code.

---

## Code Modification Guidelines

- **Read before editing** — never propose changes to code you haven't seen
- **Prefer editing existing files** over creating new ones — organise by domain, not by utility type
- **Build after** adding new files, changing memory allocations, modifying peripheral configs, or any change affecting >50 lines

---

## Flash Programming Guidelines

### No Packed Structs for Flash I/O

`__attribute__((packed))` structs cause unaligned access faults (HardFault) and corrupt flash writes on Cortex-M4. Use explicit `uint64_t` array packing instead.

```c
// WRONG:
typedef struct __attribute__((packed)) { uint32_t magic; float slope; } cal_t;
uint64_t* ptr = (uint64_t*)&entry;  // broken alignment

// CORRECT:
static uint64_t make_dword(uint32_t lo, uint32_t hi) {
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}
// Float → uint32_t via union (safe type punning):
typedef union { float f; uint32_t u; } fu32_t;
fu32_t c = { .f = slope_value };
buf[0] = make_dword(c.u, intercept_u32);
```

### Flash Programming Sequence (STM32G4)

```c
HAL_FLASH_Unlock();
__HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
// Erase page(s)
HAL_FLASHEx_Erase(&erase_init, &page_error);
// Write doublewords (8-byte aligned address, 8 bytes at a time)
for (int i = 0; i < n; i++)
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, addr + i*8, buf[i]);
HAL_FLASH_Lock();
```

Flash can only be written once after erase. Always check `HAL_FLASH_GetError()`.

---

## LVGL Guidelines

### Screen Switching — Create → Load → Delete

```c
// CORRECT:
lv_obj_t* new_scr = screen_xxx_create();
lv_scr_load_anim(new_scr, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
if (current_scr) lv_obj_del(current_scr);  // delete AFTER load

// WRONG — causes HardFault:
lv_obj_del(current_scr);
lv_scr_load(new_scr);
```

### DMA Flush Completion

Signal LVGL flush completion in the DMA callback, **not** inside the flush function:

```c
// CORRECT (in stm32g4xx_it.c):
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        lv_disp_flush_ready(&disp_drv);
    }
}

// WRONG — DMA still in progress when this runs:
void flush_cb(...) {
    HAL_SPI_Transmit_DMA(...);
    lv_disp_flush_ready(&disp_drv);  // too early
}
```

### String Buffers

LVGL v8 labels store a pointer, not a copy. Stack buffers become dangling pointers:

```c
// WRONG:
char buf[32]; snprintf(buf, 32, "%d", v); lv_label_set_text(lbl, buf);

// CORRECT:
static char buf[32]; snprintf(buf, 32, "%d", v); lv_label_set_text(lbl, buf);
```

### Display Background

Always set explicit background opacity to prevent rendering artefacts:

```c
lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);  // required
```

---

## Safety Audit Protocol

Run `/stm32-safety-audit <filename>` before committing any change to:
- `.c`/`.h` files in `Application/User/Core/` or `Core/Src/`
- Calibration, sensor, or safety-critical code
- Flash storage or persistence modules
- Interrupt handlers or DMA callbacks

**Severity response**:
- CRITICAL: must fix before commit
- HIGH: fix in same commit or create follow-up
- MEDIUM/LOW: document as tech debt

Exempt: documentation-only, pure cosmetic LVGL styling, build scripts.

---

## Testing Procedures

### Pre-Implementation Checklist

- [ ] Read relevant source files to understand existing code
- [ ] Identify affected files and data structures
- [ ] Estimate memory impact

### Runtime Testing Checklist

- [ ] Builds without errors, no RAM overflow in linker output
- [ ] No hard faults or crashes
- [ ] Display renders without artefacts (no stripes, correct colours)
- [ ] DMA transfers complete (SPI TxCplt callback fires)
- [ ] LVGL timer handler runs in main loop
- [ ] Screen transitions work
- [ ] Sensor readings update at expected rate

### Debugging

Use `volatile` debug counters watched in debugger — not printf (wastes memory and blocks):

```c
volatile uint32_t g_dbg_flush_count = 0;
volatile uint32_t g_dbg_dma_complete = 0;
```

---

## Common Pitfalls

| Pitfall | Symptom | Fix |
|---------|---------|-----|
| RAM overflow | "region 'RAM' overflowed" | Reduce buffers, use LVGL pool |
| Stack overflow | Random crashes, HardFault | Reduce local arrays, use heap |
| Packed struct flash | Garbage data / HardFault | Use `uint64_t` array packing |
| Deleting active LVGL screen | HardFault | Load new first, then delete old |
| Dangling string pointer | Garbage text | Use `static` buffers |
| DMA flush too early | Screen frozen | Call `lv_disp_flush_ready()` in DMA callback |
| New .c not in build | Undefined reference | First build in STM32CubeIDE IDE |
| HAL re-init | Display noise, DMA corruption | Never re-init, only start/stop |
