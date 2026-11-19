#include <jni.h>
#include <android/log.h>
#include <link.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define TAG "SF2LAN-NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define PLAYER_TWO_INDEX 1
#define INPUT_FRESH_MS 750
#define AXIS_THRESHOLD 0.30f

#define BUTTON_PUNCH (1u << 0)
#define BUTTON_KICK (1u << 1)
#define BUTTON_RANGED (1u << 2)
#define BUTTON_MAGIC (1u << 3)
#define BUTTON_BOSS (1u << 4)
#define BUTTON_PAUSE (1u << 5)

// RVAs confirmados na Definitive Edition 2.41.9 ARM64.
#define RVA_GET_AXIS_UP 0x3138B58u
#define RVA_GET_AXIS_DOWN 0x3138CB8u
#define RVA_GET_KEY 0x3138E70u
#define RVA_GET_KEY_UP 0x31393BCu
#define RVA_GET_AXIS_VALUE 0x313995Cu
#define RVA_GET_KEY_DOWN 0x313A4ECu
#define RVA_GET_AXIS 0x313A918u

typedef bool (*BoolInputMethod)(void *, int32_t, int32_t, const void *);
typedef float (*FloatInputMethod)(void *, int32_t, int32_t, const void *);

static BoolInputMethod original_get_axis_up;
static BoolInputMethod original_get_axis_down;
static BoolInputMethod original_get_key;
static BoolInputMethod original_get_key_up;
static FloatInputMethod original_get_axis_value;
static BoolInputMethod original_get_key_down;
static BoolInputMethod original_get_axis;

static _Atomic uint32_t horizontal_bits;
static _Atomic uint32_t vertical_bits;
static _Atomic uint32_t buttons;
static _Atomic uint32_t key_down_latch;
static _Atomic uint32_t key_up_latch;
static _Atomic uint32_t axis_down_latch;
static _Atomic uint32_t axis_up_latch;
static _Atomic int64_t received_at_ms;
static _Atomic bool hook_installed;
static _Atomic bool hook_failed;

static uint32_t float_bits(float value) {
    union { float f; uint32_t u; } converted = { .f = value };
    return converted.u;
}

static float bits_float(uint32_t value) {
    union { float f; uint32_t u; } converted = { .u = value };
    return converted.f;
}

static int64_t now_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static bool input_is_fresh(void) {
    int64_t received = atomic_load_explicit(&received_at_ms, memory_order_acquire);
    int64_t age = now_ms() - received;
    return received > 0 && age >= 0 && age <= INPUT_FRESH_MS;
}

static uint32_t key_to_button(int32_t key) {
    // Layout Xbox usado pelo PVP: X=soco, A=chute, B=distancia, Y=magia.
    switch (key) {
        case 2: return BUTTON_PUNCH;  // X
        case 0: return BUTTON_KICK;   // A
        case 1: return BUTTON_RANGED; // B
        case 3: return BUTTON_MAGIC;  // Y
        case 18:                     // L1
        case 19: return BUTTON_BOSS;  // R1
        case 22: return BUTTON_PAUSE; // Start
        default: return 0;
    }
}

static float remote_axis_value(int32_t axis) {
    switch (axis) {
        case 0: // LeftStickX
        case 4: // DPadX
            return bits_float(atomic_load_explicit(&horizontal_bits, memory_order_relaxed));
        case 1: // LeftStickY
        case 5: // DPadY
            return bits_float(atomic_load_explicit(&vertical_bits, memory_order_relaxed));
        default:
            return 0.0f;
    }
}

static uint32_t axis_mask(int32_t axis) {
    return axis >= 0 && axis < 8 ? (1u << axis) : 0;
}

static bool consume_latch(_Atomic uint32_t *latch, uint32_t mask) {
    if (!mask) return false;
    uint32_t previous = atomic_fetch_and_explicit(latch, ~mask, memory_order_acq_rel);
    return (previous & mask) != 0;
}

static bool hooked_get_key(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_key(self, index, key, method);
    uint32_t mask = key_to_button(key);
    return input_is_fresh() && mask && (atomic_load_explicit(&buttons, memory_order_relaxed) & mask);
}

static bool hooked_get_key_down(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_key_down(self, index, key, method);
    return input_is_fresh() && consume_latch(&key_down_latch, key_to_button(key));
}

static bool hooked_get_key_up(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_key_up(self, index, key, method);
    return consume_latch(&key_up_latch, key_to_button(key));
}

static float hooked_get_axis_value(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_axis_value(self, index, axis, method);
    return input_is_fresh() ? remote_axis_value(axis) : 0.0f;
}

static bool hooked_get_axis(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_axis(self, index, axis, method);
    float value = input_is_fresh() ? remote_axis_value(axis) : 0.0f;
    return value >= AXIS_THRESHOLD || value <= -AXIS_THRESHOLD;
}

static bool hooked_get_axis_down(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_axis_down(self, index, axis, method);
    return input_is_fresh() && consume_latch(&axis_down_latch, axis_mask(axis));
}

static bool hooked_get_axis_up(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX) return original_get_axis_up(self, index, axis, method);
    return consume_latch(&axis_up_latch, axis_mask(axis));
}

struct library_location {
    uintptr_t base;
    uintptr_t end;
};

static int find_il2cpp(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    if (!info->dlpi_name || !strstr(info->dlpi_name, "libil2cpp.so")) return 0;
    struct library_location *location = data;
    location->base = (uintptr_t)info->dlpi_addr;
    location->end = location->base;
    for (ElfW(Half) i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *header = &info->dlpi_phdr[i];
        if (header->p_type == PT_LOAD) {
            uintptr_t segment_end = location->base + header->p_vaddr + header->p_memsz;
            if (segment_end > location->end) location->end = segment_end;
        }
    }
    return 1;
}

static void write_absolute_jump(uint8_t *destination, const void *target) {
    const uint32_t ldr_x16_literal_8 = 0x58000050u;
    const uint32_t br_x16 = 0xD61F0200u;
    memcpy(destination, &ldr_x16_literal_8, sizeof(ldr_x16_literal_8));
    memcpy(destination + 4, &br_x16, sizeof(br_x16));
    uintptr_t address = (uintptr_t)target;
    memcpy(destination + 8, &address, sizeof(address));
}

static bool branch_reachable(uintptr_t from, uintptr_t to) {
    intptr_t distance = (intptr_t)to - (intptr_t)from;
    return (distance & 3) == 0 && distance >= -0x08000000 && distance < 0x08000000;
}

static uint32_t encode_branch(uintptr_t from, uintptr_t to) {
    intptr_t words = ((intptr_t)to - (intptr_t)from) >> 2;
    return 0x14000000u | ((uint32_t)words & 0x03ffffffu);
}

static uint8_t *allocate_hook_page(uintptr_t library_end) {
    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t hint = (library_end + (uintptr_t)page_size - 1) & ~((uintptr_t)page_size - 1);
    void *memory = mmap((void *)(hint + 0x10000), (size_t)page_size,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return memory == MAP_FAILED ? NULL : memory;
}

static bool install_one(
    uintptr_t target,
    const void *replacement,
    uint8_t *slot,
    void **original
) {
    uint8_t *stub = slot;
    uint8_t *trampoline = slot + 16;
    const void *branch_target = replacement;
    if (!branch_reachable(target, (uintptr_t)replacement)) {
        write_absolute_jump(stub, replacement);
        branch_target = stub;
    }
    if (!branch_reachable(target, (uintptr_t)branch_target)) {
        LOGE("Nao foi possivel posicionar o stub perto do RVA 0x%lx.", (unsigned long)target);
        return false;
    }

    memcpy(trampoline, (const void *)target, 4);
    // instrucao original + padding + salto absoluto para target+4
    uint32_t ldr_x16_literal_12 = 0x58000070u;
    uint32_t br_x16 = 0xD61F0200u;
    uint32_t nop = 0xD503201Fu;
    memcpy(trampoline + 4, &ldr_x16_literal_12, 4);
    memcpy(trampoline + 8, &br_x16, 4);
    memcpy(trampoline + 12, &nop, 4);
    uintptr_t continuation = target + 4;
    memcpy(trampoline + 16, &continuation, sizeof(continuation));
    *original = trampoline;

    long page_size = sysconf(_SC_PAGESIZE);
    uintptr_t page = target & ~((uintptr_t)page_size - 1);
    if (mprotect((void *)page, (size_t)page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return false;
    uint32_t branch = encode_branch(target, (uintptr_t)branch_target);
    memcpy((void *)target, &branch, sizeof(branch));
    __builtin___clear_cache((char *)target, (char *)(target + 4));
    mprotect((void *)page, (size_t)page_size, PROT_READ | PROT_EXEC);
    return true;
}

JNIEXPORT jboolean JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeInstall(JNIEnv *env, jclass type) {
    (void)env;
    (void)type;
    if (atomic_load_explicit(&hook_installed, memory_order_acquire)) return JNI_TRUE;
    if (atomic_load_explicit(&hook_failed, memory_order_acquire)) return JNI_FALSE;

    struct library_location location = {0};
    dl_iterate_phdr(find_il2cpp, &location);
    if (!location.base) return JNI_FALSE;

    uint8_t *hook_page = allocate_hook_page(location.end);
    if (!hook_page) {
        LOGE("Nao foi possivel reservar memoria executavel para os trampolins.");
        atomic_store_explicit(&hook_failed, true, memory_order_release);
        return JNI_FALSE;
    }

    struct hook_spec {
        uintptr_t rva;
        const void *replacement;
        void **original;
    } hooks[] = {
        {RVA_GET_AXIS_UP, hooked_get_axis_up, (void **)&original_get_axis_up},
        {RVA_GET_AXIS_DOWN, hooked_get_axis_down, (void **)&original_get_axis_down},
        {RVA_GET_KEY, hooked_get_key, (void **)&original_get_key},
        {RVA_GET_KEY_UP, hooked_get_key_up, (void **)&original_get_key_up},
        {RVA_GET_AXIS_VALUE, hooked_get_axis_value, (void **)&original_get_axis_value},
        {RVA_GET_KEY_DOWN, hooked_get_key_down, (void **)&original_get_key_down},
        {RVA_GET_AXIS, hooked_get_axis, (void **)&original_get_axis},
    };
    const size_t count = sizeof(hooks) / sizeof(hooks[0]);
    for (size_t i = 0; i < count; i++) {
        if (!install_one(location.base + hooks[i].rva, hooks[i].replacement,
            hook_page + i * 64, hooks[i].original)) {
            LOGE("Instalacao interrompida no hook %zu.", i);
            // Impede uma segunda tentativa sobre prologos que talvez ja tenham sido alterados.
            atomic_store_explicit(&hook_failed, true, memory_order_release);
            return JNI_FALSE;
        }
    }
    __builtin___clear_cache((char *)hook_page, (char *)(hook_page + count * 64));
    atomic_store_explicit(&hook_installed, true, memory_order_release);
    LOGI("Sete leitores do GamePad 1 redirecionados; base libil2cpp=%p.", (void *)location.base);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeUpdateInput(
    JNIEnv *env,
    jclass type,
    jfloat horizontal,
    jfloat vertical,
    jint new_buttons,
    jlong sequence,
    jlong received
) {
    (void)env;
    (void)type;
    (void)sequence;
    uint32_t old_buttons = atomic_exchange_explicit(&buttons, (uint32_t)new_buttons, memory_order_acq_rel);
    atomic_fetch_or_explicit(&key_down_latch, (uint32_t)new_buttons & ~old_buttons, memory_order_relaxed);
    atomic_fetch_or_explicit(&key_up_latch, old_buttons & ~(uint32_t)new_buttons, memory_order_relaxed);

    float old_horizontal = bits_float(atomic_exchange_explicit(&horizontal_bits, float_bits(horizontal), memory_order_acq_rel));
    float old_vertical = bits_float(atomic_exchange_explicit(&vertical_bits, float_bits(vertical), memory_order_acq_rel));
    float old_axes[] = {old_horizontal, old_vertical, 0.0f, 0.0f, old_horizontal, old_vertical};
    float new_axes[] = {horizontal, vertical, 0.0f, 0.0f, horizontal, vertical};
    for (uint32_t axis = 0; axis < 6; axis++) {
        bool was_active = old_axes[axis] >= AXIS_THRESHOLD || old_axes[axis] <= -AXIS_THRESHOLD;
        bool is_active = new_axes[axis] >= AXIS_THRESHOLD || new_axes[axis] <= -AXIS_THRESHOLD;
        if (!was_active && is_active) atomic_fetch_or_explicit(&axis_down_latch, 1u << axis, memory_order_relaxed);
        if (was_active && !is_active) atomic_fetch_or_explicit(&axis_up_latch, 1u << axis, memory_order_relaxed);
    }
    atomic_store_explicit(&received_at_ms, (int64_t)received, memory_order_release);
}
