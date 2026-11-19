#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <link.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define TAG "SF2LAN-NATIVE"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

#define PLAYER_ONE_INDEX 0
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
#define RVA_DOJO_FIXED_UPDATE 0x3108844u
#define RVA_GET_AXIS_VALUE 0x313995Cu
#define RVA_GET_KEY_DOWN 0x313A4ECu
#define RVA_GET_AXIS 0x313A918u
#define RVA_BUILD_PVP_XML 0x32E3CD8u
#define RVA_GET_SCENE_MANAGER 0x351B940u
#define RVA_LOAD_SCENE 0x35255B4u
#define PVP_SCENE_ID 9

#define PVP_XML_PATH "/sdcard/Android/data/com.sf2.de/files/sf2lan-pvp.xml"

typedef bool (*BoolInputMethod)(void *, int32_t, int32_t, const void *);
typedef float (*FloatInputMethod)(void *, int32_t, int32_t, const void *);
typedef void (*VoidInstanceMethod)(void *, const void *);
typedef void *(*PvpXmlBuilder)(void *, const void *);
typedef void *(*GetSceneManager)(const void *);
typedef void (*LoadScene)(void *, int32_t, void *, void *, bool, const void *);

typedef void *(*Il2CppObjectGetClass)(void *object);
typedef const void *(*Il2CppClassGetMethodFromName)(void *klass, const char *name, int args_count);
typedef void *(*Il2CppRuntimeInvoke)(const void *method, void *object, void **params, void **exception);
typedef const uint16_t *(*Il2CppStringChars)(void *string);
typedef int32_t (*Il2CppStringLength)(void *string);

static BoolInputMethod original_get_axis_up;
static BoolInputMethod original_get_axis_down;
static BoolInputMethod original_get_key;
static BoolInputMethod original_get_key_up;
static VoidInstanceMethod original_dojo_fixed_update;
static FloatInputMethod original_get_axis_value;
static BoolInputMethod original_get_key_down;
static BoolInputMethod original_get_axis;
static PvpXmlBuilder original_build_pvp_xml;

static _Atomic uint32_t horizontal_bits;
static _Atomic uint32_t vertical_bits;
static _Atomic uint32_t buttons;
static _Atomic uint32_t key_down_latch;
static _Atomic uint32_t key_up_latch;
static _Atomic uint32_t axis_down_latch;
static _Atomic uint32_t axis_up_latch;
static _Atomic int64_t received_at_ms;
static _Atomic uint32_t local_left_x_bits;
static _Atomic uint32_t local_left_y_bits;
static _Atomic uint32_t local_dpad_x_bits;
static _Atomic uint32_t local_dpad_y_bits;
static _Atomic uint32_t local_buttons;
static _Atomic bool hook_installed;
static _Atomic bool hook_failed;
static _Atomic uint32_t pvp_xml_dump_count;
static _Atomic uintptr_t il2cpp_base;
static _Atomic bool pvp_open_requested;

static void maybe_open_pvp(void) {
    if (!atomic_exchange_explicit(&pvp_open_requested, false, memory_order_acq_rel)) return;
    uintptr_t base = atomic_load_explicit(&il2cpp_base, memory_order_acquire);
    if (!base) {
        atomic_store_explicit(&pvp_open_requested, true, memory_order_release);
        return;
    }
    GetSceneManager get_manager = (GetSceneManager)(base + RVA_GET_SCENE_MANAGER);
    LoadScene load_scene = (LoadScene)(base + RVA_LOAD_SCENE);
    void *manager = get_manager(NULL);
    if (!manager) {
        LOGE("Gerenciador de cenas ainda indisponivel; abertura do PVP adiada.");
        atomic_store_explicit(&pvp_open_requested, true, memory_order_release);
        return;
    }
    LOGI("Solicitando a cena PVP %d pelo carregador interno.", PVP_SCENE_ID);
    load_scene(manager, PVP_SCENE_ID, NULL, NULL, true, NULL);
}

static size_t utf16_to_utf8(const uint16_t *source, size_t source_length, char *destination) {
    size_t output = 0;
    for (size_t index = 0; index < source_length; index++) {
        uint32_t codepoint = source[index];
        if (codepoint >= 0xd800 && codepoint <= 0xdbff && index + 1 < source_length) {
            uint32_t low = source[index + 1];
            if (low >= 0xdc00 && low <= 0xdfff) {
                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                index++;
            }
        }
        if (codepoint <= 0x7f) {
            destination[output++] = (char)codepoint;
        } else if (codepoint <= 0x7ff) {
            destination[output++] = (char)(0xc0 | (codepoint >> 6));
            destination[output++] = (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0xffff) {
            destination[output++] = (char)(0xe0 | (codepoint >> 12));
            destination[output++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            destination[output++] = (char)(0x80 | (codepoint & 0x3f));
        } else {
            destination[output++] = (char)(0xf0 | (codepoint >> 18));
            destination[output++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
            destination[output++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
            destination[output++] = (char)(0x80 | (codepoint & 0x3f));
        }
    }
    destination[output] = '\0';
    return output;
}

static void dump_pvp_xml(void *xml_node) {
    if (!xml_node) return;
    Il2CppObjectGetClass object_get_class = (Il2CppObjectGetClass)dlsym(RTLD_DEFAULT, "il2cpp_object_get_class");
    Il2CppClassGetMethodFromName get_method =
        (Il2CppClassGetMethodFromName)dlsym(RTLD_DEFAULT, "il2cpp_class_get_method_from_name");
    Il2CppRuntimeInvoke runtime_invoke = (Il2CppRuntimeInvoke)dlsym(RTLD_DEFAULT, "il2cpp_runtime_invoke");
    Il2CppStringChars string_chars = (Il2CppStringChars)dlsym(RTLD_DEFAULT, "il2cpp_string_chars");
    Il2CppStringLength string_length = (Il2CppStringLength)dlsym(RTLD_DEFAULT, "il2cpp_string_length");
    if (!object_get_class || !get_method || !runtime_invoke || !string_chars || !string_length) {
        LOGE("APIs IL2CPP ausentes para extrair o XML do PVP.");
        return;
    }

    void *klass = object_get_class(xml_node);
    const void *method = klass ? get_method(klass, "get_OuterXml", 0) : NULL;
    void *exception = NULL;
    void *text = method ? runtime_invoke(method, xml_node, NULL, &exception) : NULL;
    if (!text || exception) {
        LOGE("Nao foi possivel obter XmlNode.OuterXml do PVP.");
        return;
    }

    int32_t length = string_length(text);
    const uint16_t *characters = string_chars(text);
    if (length < 0 || !characters || (size_t)length > (SIZE_MAX - 1) / 4) return;
    char *utf8 = malloc((size_t)length * 4 + 1);
    if (!utf8) return;
    size_t bytes = utf16_to_utf8(characters, (size_t)length, utf8);
    FILE *output = fopen(PVP_XML_PATH, "wb");
    if (output) {
        fwrite(utf8, 1, bytes, output);
        fputc('\n', output);
        fclose(output);
        uint32_t count = atomic_fetch_add_explicit(&pvp_xml_dump_count, 1, memory_order_relaxed) + 1;
        LOGI("XML do PVP capturado em %s (%zu bytes, captura %u).", PVP_XML_PATH, bytes, count);
    } else {
        LOGE("Nao foi possivel gravar %s.", PVP_XML_PATH);
    }
    free(utf8);
}

static void *hooked_build_pvp_xml(void *self, const void *method) {
    void *xml_node = original_build_pvp_xml(self, method);
    dump_pvp_xml(xml_node);
    return xml_node;
}

static void hooked_dojo_fixed_update(void *self, const void *method) {
    original_dojo_fixed_update(self, method);
    maybe_open_pvp();
}

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

static void capture_local_button(int32_t key, bool pressed) {
    uint32_t mask = key_to_button(key);
    if (!mask) return;
    if (pressed) {
        atomic_fetch_or_explicit(&local_buttons, mask, memory_order_relaxed);
    } else {
        atomic_fetch_and_explicit(&local_buttons, ~mask, memory_order_relaxed);
    }
}

static void capture_local_axis(int32_t axis, float value) {
    _Atomic uint32_t *destination = NULL;
    switch (axis) {
        case 0: destination = &local_left_x_bits; break;
        case 1: destination = &local_left_y_bits; break;
        case 4: destination = &local_dpad_x_bits; break;
        case 5: destination = &local_dpad_y_bits; break;
        default: return;
    }
    atomic_store_explicit(destination, float_bits(value), memory_order_relaxed);
}

static float strongest_axis(float primary, float secondary) {
    float primary_magnitude = primary < 0.0f ? -primary : primary;
    float secondary_magnitude = secondary < 0.0f ? -secondary : secondary;
    return primary_magnitude >= secondary_magnitude ? primary : secondary;
}

static bool hooked_get_key(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) {
        bool result = original_get_key(self, index, key, method);
        if (index == PLAYER_ONE_INDEX) capture_local_button(key, result);
        return result;
    }
    uint32_t mask = key_to_button(key);
    return input_is_fresh() && mask && (atomic_load_explicit(&buttons, memory_order_relaxed) & mask);
}

static bool hooked_get_key_down(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) {
        bool result = original_get_key_down(self, index, key, method);
        if (index == PLAYER_ONE_INDEX && result) capture_local_button(key, true);
        return result;
    }
    return input_is_fresh() && consume_latch(&key_down_latch, key_to_button(key));
}

static bool hooked_get_key_up(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX) {
        bool result = original_get_key_up(self, index, key, method);
        if (index == PLAYER_ONE_INDEX && result) capture_local_button(key, false);
        return result;
    }
    return consume_latch(&key_up_latch, key_to_button(key));
}

static float hooked_get_axis_value(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX) {
        float result = original_get_axis_value(self, index, axis, method);
        if (index == PLAYER_ONE_INDEX) capture_local_axis(axis, result);
        maybe_open_pvp();
        return result;
    }
    float result = input_is_fresh() ? remote_axis_value(axis) : 0.0f;
    maybe_open_pvp();
    return result;
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
        {RVA_DOJO_FIXED_UPDATE, hooked_dojo_fixed_update, (void **)&original_dojo_fixed_update},
        {RVA_GET_AXIS_VALUE, hooked_get_axis_value, (void **)&original_get_axis_value},
        {RVA_GET_KEY_DOWN, hooked_get_key_down, (void **)&original_get_key_down},
        {RVA_GET_AXIS, hooked_get_axis, (void **)&original_get_axis},
        {RVA_BUILD_PVP_XML, hooked_build_pvp_xml, (void **)&original_build_pvp_xml},
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
    atomic_store_explicit(&il2cpp_base, location.base, memory_order_release);
    atomic_store_explicit(&hook_installed, true, memory_order_release);
    LOGI("Sete leitores do GamePad 1 e o gerador XML do PVP redirecionados; base libil2cpp=%p.",
        (void *)location.base);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeRequestOpenPvp(JNIEnv *env, jclass type) {
    (void)env;
    (void)type;
    atomic_store_explicit(&pvp_open_requested, true, memory_order_release);
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

JNIEXPORT jfloatArray JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeReadLocalInput(JNIEnv *env, jclass type) {
    (void)type;
    float left_x = bits_float(atomic_load_explicit(&local_left_x_bits, memory_order_relaxed));
    float left_y = bits_float(atomic_load_explicit(&local_left_y_bits, memory_order_relaxed));
    float dpad_x = bits_float(atomic_load_explicit(&local_dpad_x_bits, memory_order_relaxed));
    float dpad_y = bits_float(atomic_load_explicit(&local_dpad_y_bits, memory_order_relaxed));
    float snapshot[] = {
        strongest_axis(left_x, dpad_x),
        strongest_axis(left_y, dpad_y),
        (float)atomic_load_explicit(&local_buttons, memory_order_relaxed)
    };
    jfloatArray result = (*env)->NewFloatArray(env, 3);
    if (result != NULL) {
        (*env)->SetFloatArrayRegion(env, result, 0, 3, snapshot);
    }
    return result;
}
