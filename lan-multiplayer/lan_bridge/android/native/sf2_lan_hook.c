#include <jni.h>
#include <android/log.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <link.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
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
#define FIGHT_START_WAIT_TIMEOUT_MS 15000
#define REMOTE_FIGHT_STATE_FRESH_MS 500
#define FRAME_PACING_MAX_LEAD 8.0f
#define AXIS_THRESHOLD 0.30f

#ifndef SF2_LAN_DIAGNOSTIC_ALLOW_REMOTE_AI
#define SF2_LAN_DIAGNOSTIC_ALLOW_REMOTE_AI 0
#endif

#ifndef SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
#define SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS 0
#endif

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
#define RVA_GAMEPAD_CONTROLLER_CURRENT 0x313890Cu
#define RVA_SET_GAMEPAD_DUAL_MODE 0x313A7F4u
#define RVA_GET_AXIS 0x313A918u
#define RVA_TOUCH_INPUT_CTOR 0x315D230u
#define RVA_PRE_FIGHT_VIEWER_INIT 0x31E2E08u
#define RVA_PRE_FIGHT_CREATE_VS 0x31E36DCu
#define RVA_FIGHT_SCENE_CREATE_FIGHT 0x3234A0Cu
#define RVA_FIGHT_SCENE_START_FIGHT 0x3234BB8u
#define RVA_FIGHT_SCENE_FIXED_UPDATE 0x3234C50u
#define RVA_FIGHT_FIXED_UPDATE 0x34EC2E4u
#define RVA_FIGHT_SIMULATION_STEP 0x34EEE70u
#define RVA_REGISTER_INPUT_BINDING 0x33DEE24u
#define RVA_RESOLVE_INPUT_FIGHTER 0x33DF1ACu
#define RVA_FIGHTER_AI_ANIMATION_EVENT 0x33A4D40u
#define RVA_AI_PLANNER_INITIALIZE 0x30307ECu
#define RVA_AI_PLANNER_CANCEL 0x302EC50u
#define RVA_AI_PLANNER_SELECT_ACTION 0x302C034u
#define RVA_FIGHTER_CTOR 0x33987C0u
#define RVA_RANDOM_FIGHTER_PROFILE_CREATE 0x33C1AF0u
#define RVA_CREATE_FIGHT_PROFILES 0x33C289Cu
#define RVA_FIGHTER_APPLY_CONFIG_ITEM 0x3397218u
#define RVA_FIGHTER_APPLY_EQUIPMENT_ITEM 0x339BA2Cu
#define RVA_FIGHT_INPUT_PRESS 0x34E94C4u
#define RVA_FIGHT_INPUT_RELEASE 0x34EC58Cu
#define RVA_IL2CPP_ASSEMBLY_GET_IMAGE 0x1A6DF98u
#define RVA_IL2CPP_CLASS_FROM_NAME 0x1A6DFD0u
#define RVA_IL2CPP_DOMAIN_GET 0x1A6E4C8u
#define RVA_IL2CPP_DOMAIN_GET_ASSEMBLIES 0x1A6E4D4u
#define RVA_IL2CPP_OBJECT_NEW 0x1A6E8A4u
#define RVA_IL2CPP_STRING_NEW 0x1A6E908u
#define RVA_FIGHT_CONFIG_MANAGER_CURRENT 0x32936E4u
#define RVA_FIGHT_CONFIG_BY_NAME 0x32AD7D8u
#define RVA_FIGHT_CONFIG_CLEAR_PLAYER 0x31200C0u
#define RVA_FIGHT_CONFIG_SET_PLAYER 0x3120130u
#define RVA_FIGHT_CONFIG_CLEAR_OPPONENT 0x3120190u
#define RVA_FIGHT_CONFIG_SET_OPPONENT 0x31215FCu
#define RVA_FIGHT_CONFIG_EQUIPMENT_RULE_GET 0x3121DCCu
#define RVA_PLAYER_PROFILE_COPY_CTOR 0x3169DC4u
#define RVA_PLAYER_PROFILE_ITEM_BY_SLOT 0x316F390u
#define RVA_PLAYER_PROFILE_ITEM_GET 0x316F674u
#define RVA_PLAYER_PROFILE_SET_ITEM 0x316F7E8u
#define RVA_ITEM_BY_NAME 0x32AC484u
#define RVA_ITEM_MANAGER_CURRENT 0x32A917Cu
#define RVA_ITEM_MANAGER_BY_NAME 0x3216890u
#define RVA_PRE_FIGHT_TIME_PASSED_FRAMES 0x31E2D60u
#define RVA_PRE_FIGHT_TIME_TOTAL_FRAMES 0x31E4194u
#define RVA_PRE_FIGHT_TIME_LEFT 0x31E42A0u
#define RVA_PRE_FIGHT_TIME_LEFT_FRAMES 0x31E51D4u
#define RVA_FIGHTER_APPLY_DAMAGE 0x339CF90u
#define RVA_FIGHTER_DAMAGE_GET 0x33A6D0Cu
#define RVA_FIGHTER_DAMAGE_SET 0x339B580u
#define RVA_FIGHTER_RESOLVE_RESULT 0x3397894u
#define RVA_FIGHTER_ROUNDS_LOST_GET 0x33A8604u
#define RVA_FIGHTER_POSITION_GET 0x339FB0Cu
#define RVA_NODE_POSITION_COPY 0x3321460u
#define RVA_VECTOR3_POSITION_SET 0x2FA6690u

#if SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
#define DIAGNOSTIC_FIGHTER_FLOAT_COUNT 16
#endif

#define FIGHT_SCENE_FIGHT_OFFSET 0x90u
#define FIGHT_SCENE_PRE_FIGHT_OFFSET 0x98u
#define FIGHT_PLAYER_OFFSET 0xB0u
#define FIGHT_OPPONENT_OFFSET 0xB8u
#define FIGHT_PLAYER_PROFILE_OFFSET 0x98u
#define FIGHT_OPPONENT_PROFILE_OFFSET 0xA0u
#define FIGHT_CONFIG_MODE_OFFSET 0x18u
#define FIGHT_CONFIG_BASE_EQUIPMENT_RULE_OFFSET 0x30u
#define FIGHT_CONFIG_EQUIPMENT_RULE_OFFSET 0x150u
#define FIGHTER_PROFILE_OFFSET 0x148u
#define FIGHTER_OPPONENT_OFFSET 0x130u
#define FIGHTER_AI_PLANNER_OFFSET 0x298u
#define FIGHTER_POSITION_MODEL_OFFSET 0x250u
#define POSITION_MODEL_ROOT_NODE_OFFSET 0x30u

#define PRE_FIGHT_VIEWER_OFFSET 0x80u
#define VIEWER_LEFT_MODEL_OFFSET 0x58u
#define VIEWER_RIGHT_MODEL_OFFSET 0x60u
#define SCREEN_MODEL_LIFE_BAR_OFFSET 0x30u
#define LIFE_BAR_VALUE_OFFSET 0x80u
#define LIFE_BAR_HIT_VALUE_OFFSET 0x84u

#define PROFILE_SKELETON_OFFSET 0xB8u
#define PROFILE_WEAPON_OFFSET 0xC0u
#define PROFILE_ARMOR_OFFSET 0xC8u
#define PROFILE_HELM_OFFSET 0xD0u
#define PROFILE_RANGED_OFFSET 0xD8u
#define PROFILE_MAGIC_OFFSET 0xE0u
#define PROFILE_NAME_OFFSET 0x188u
#define PROFILE_AVATAR_OFFSET 0x198u
#define PROFILE_WEAPON_PERKS_OFFSET 0x1B0u
#define PROFILE_ARMOR_PERKS_OFFSET 0x1B8u
#define PROFILE_HELM_PERKS_OFFSET 0x1C0u

#define FIGHT_CONFIG_IDENTITY_MAX 512

typedef bool (*BoolInputMethod)(void *, int32_t, int32_t, const void *);
typedef float (*FloatInputMethod)(void *, int32_t, int32_t, const void *);
typedef void (*FightSceneCreateFightMethod)(void *, void *, void *, void *, const void *);
typedef void (*InstanceMethod)(void *, const void *);
typedef void (*RegisterInputBindingMethod)(int32_t, int32_t, int32_t, const void *);
typedef void *(*ResolveInputFighterMethod)(void *, const void *);
typedef void *(*GetCurrentMethod)(const void *);
typedef void (*SetBoolMethod)(void *, bool, const void *);
typedef void *(*ObjectNewMethod)(void *);
typedef void *(*DomainGetMethod)(void);
typedef const void **(*DomainGetAssembliesMethod)(void *, size_t *);
typedef void *(*AssemblyGetImageMethod)(const void *);
typedef void *(*ClassFromNameMethod)(void *, const char *, const char *);
typedef void *(*StringNewMethod)(const char *);
typedef void (*TouchInputCtorMethod)(void *, int32_t, int32_t, const void *);
typedef void (*PreFightViewerInitMethod)(
    void *, void *, void *, void *, bool, const void *);
typedef void (*PreFightCreateVSMethod)(
    void *, void *, void *, int32_t, bool, bool, bool, bool, const void *);
typedef void (*FightInputMethod)(void *, void *, const void *);
typedef void *(*PlannerSelectActionMethod)(
    void *, void *, int32_t, const void *);
typedef void *(*GetSingletonMethod)(const void *);
typedef void *(*GetFightConfigByNameMethod)(void *, void *, const void *);
typedef void (*ConfigInstanceMethod)(void *, const void *);
typedef void (*ConfigProfileMethod)(void *, void *, const void *);
typedef void (*ProfileCopyCtorMethod)(void *, void *, const void *);
typedef void *(*ProfileItemBySlotMethod)(void *, void *, const void *);
typedef void *(*ProfileItemGetMethod)(void *, void *, void *, const void *);
typedef void (*ProfileSetItemMethod)(void *, void *, void *, const void *);
typedef void *(*ItemByNameMethod)(void *, const void *);
typedef void *(*ItemManagerByNameMethod)(void *, void *, const void *);
typedef void (*GcWbarrierSetFieldMethod)(void *, void **, void *);
typedef int32_t (*IntInstanceMethod)(void *, const void *);
typedef float (*FighterFloatMethod)(void *, const void *);
typedef void (*FighterSetFloatMethod)(void *, float, const void *);
typedef void (*FighterApplyEquipmentItemMethod)(
    void *, void *, bool, bool, bool, const void *);
typedef void (*FighterApplyConfigItemMethod)(
    void *, void *, void *, const void *);
typedef void *(*RandomFighterProfileCreateMethod)(const void *);
typedef void (*CreateFightProfilesMethod)(
    void *, void **, void **, const void *);
typedef struct { float x; float y; float z; } Vector3Value;
typedef void *(*ObjectGetterMethod)(void *, const void *);
typedef void (*VectorSetMethod)(void *, float, float, float, const void *);
typedef void (*NodePositionCopyMethod)(void *, void *, const void *);

static BoolInputMethod original_get_axis_up;
static BoolInputMethod original_get_axis_down;
static BoolInputMethod original_get_key;
static BoolInputMethod original_get_key_up;
static FloatInputMethod original_get_axis_value;
static BoolInputMethod original_get_key_down;
static BoolInputMethod original_get_axis;
static FightSceneCreateFightMethod original_fight_scene_create_fight;
static InstanceMethod original_fight_scene_start_fight;
static InstanceMethod original_fight_scene_fixed_update;
static InstanceMethod original_fight_fixed_update;
static InstanceMethod original_fight_simulation_step;
static RegisterInputBindingMethod original_register_input_binding;
static ResolveInputFighterMethod original_resolve_input_fighter;
static FightInputMethod original_fighter_ai_animation_event;
static ConfigProfileMethod ai_planner_initialize;
static InstanceMethod ai_planner_cancel;
static PlannerSelectActionMethod original_ai_planner_select_action;
static ConfigProfileMethod original_fighter_ctor;
static FighterApplyConfigItemMethod original_fighter_apply_config_item;
static RandomFighterProfileCreateMethod original_random_fighter_profile_create;
static CreateFightProfilesMethod original_create_fight_profiles;
static FighterApplyEquipmentItemMethod original_fighter_apply_equipment_item;
static GetCurrentMethod gamepad_controller_current;
static SetBoolMethod set_gamepad_dual_mode;
static ObjectNewMethod il2cpp_object_new;
static DomainGetMethod il2cpp_domain_get;
static DomainGetAssembliesMethod il2cpp_domain_get_assemblies;
static AssemblyGetImageMethod il2cpp_assembly_get_image;
static ClassFromNameMethod il2cpp_class_from_name;
static StringNewMethod il2cpp_string_new;
static TouchInputCtorMethod touch_input_ctor;
static PreFightViewerInitMethod original_pre_fight_viewer_init;
static PreFightCreateVSMethod original_pre_fight_create_vs;
static FightInputMethod original_fight_input_activate;
static FightInputMethod original_fight_input_deactivate;
static GetSingletonMethod fight_config_manager_current;
static GetFightConfigByNameMethod fight_config_by_name;
static ConfigInstanceMethod fight_config_clear_player;
static ConfigProfileMethod fight_config_set_player;
static ConfigInstanceMethod fight_config_clear_opponent;
static ConfigProfileMethod fight_config_set_opponent;
static ObjectGetterMethod original_fight_config_equipment_rule_get;
static ProfileCopyCtorMethod player_profile_copy_ctor;
static ProfileItemBySlotMethod original_player_profile_item_by_slot;
static ProfileItemGetMethod original_player_profile_item_get;
static ProfileSetItemMethod player_profile_set_item;
static ItemByNameMethod item_by_name;
static GetSingletonMethod item_manager_current;
static ItemManagerByNameMethod item_manager_by_name;
static GcWbarrierSetFieldMethod il2cpp_gc_wbarrier_set_field;
static IntInstanceMethod pre_fight_time_passed_frames;
static IntInstanceMethod pre_fight_time_total_frames;
static IntInstanceMethod pre_fight_time_left;
static IntInstanceMethod pre_fight_time_left_frames;
static FighterSetFloatMethod original_fighter_apply_damage;
static FighterFloatMethod fighter_damage_get;
static FighterSetFloatMethod fighter_damage_set;
static InstanceMethod fighter_resolve_result;
static IntInstanceMethod fighter_rounds_lost_get;
static ObjectGetterMethod fighter_position_get;
static VectorSetMethod fighter_position_set;
static NodePositionCopyMethod original_node_position_copy;
#if SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
static FighterFloatMethod diagnostic_fighter_float_readers[DIAGNOSTIC_FIGHTER_FLOAT_COUNT];
static uint32_t diagnostic_previous_float_bits[2][DIAGNOSTIC_FIGHTER_FLOAT_COUNT];
static bool diagnostic_previous_float_valid[2];
static uint32_t diagnostic_float_generation;
struct diagnostic_fight_scalars {
    uint32_t value_118;
    uint32_t value_11c;
    uint64_t value_120;
    int32_t value_128;
    int32_t value_130;
    uint32_t bits_134;
    int32_t value_13c;
    uint32_t bits_144;
    int32_t value_17c;
    uint16_t flags;
};
static struct diagnostic_fight_scalars diagnostic_previous_fight_scalars;
static bool diagnostic_previous_fight_scalars_valid;
struct diagnostic_hud_state {
    int32_t time_left;
    int32_t time_left_frames;
    uint32_t left_life_bits;
    uint32_t left_hit_bits;
    uint32_t right_life_bits;
    uint32_t right_hit_bits;
};
static struct diagnostic_hud_state diagnostic_previous_hud_state;
static bool diagnostic_previous_hud_state_valid;
#endif
static void *touch_input_class;
static bool touch_input_class_lookup_attempted;
static void *player_profile_class;
static bool player_profile_class_lookup_attempted;

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
static _Atomic uint32_t local_touch_buttons;
static _Atomic uint32_t local_touch_button_pulses;
static _Atomic int32_t local_touch_direction;
static _Atomic bool hook_installed;
static _Atomic bool hook_failed;
static _Atomic bool session_ready;
static _Atomic bool lan_fight_active;
static _Atomic bool logged_player_two_input;
static _Atomic uint32_t fight_create_count;
static _Atomic uint32_t input_route_log_count;
static _Atomic uint32_t injected_input_log_count;
static _Atomic uint32_t local_input_reroute_log_count;
static _Atomic uint32_t fight_input_source_log_count;
static _Atomic uint32_t suppressed_remote_ai_log_count;
static _Atomic bool remote_ai_initialization_forwarded;
static _Atomic uint32_t suppressed_remote_ai_input_count;
static _Atomic uint32_t injected_buttons;
static _Atomic int32_t injected_direction;
static _Atomic uintptr_t lan_remote_profile;
static _Atomic uintptr_t lan_local_fighter;
static _Atomic uintptr_t lan_remote_fighter;
static _Atomic uintptr_t lan_remote_ai_planner;
static _Atomic uintptr_t lan_active_scene;
static _Atomic uintptr_t lan_active_fight;
static _Atomic uint32_t lan_fight_generation;
static _Atomic int64_t fight_created_at_ms;
static _Atomic int64_t fight_start_at_ms;
static _Atomic bool fight_start_logged;
static _Atomic bool fight_wait_timeout_logged;
static _Atomic bool fight_start_method_invoked;
static _Atomic bool fight_wakeup_input_sent;
static _Atomic uint32_t fight_state_tick;
static _Atomic uint32_t fight_state_bits[8];
static _Atomic uint32_t fight_authority_bits[2];
static _Atomic int32_t fight_authority_rounds[2];
static _Atomic uint32_t fight_authority_position_bits[6];
static _Atomic uint32_t remote_authority_damage_bits[2];
static _Atomic int32_t remote_authority_rounds[2];
static _Atomic uint32_t remote_authority_position_bits[6];
static _Atomic int64_t remote_authority_received_at_ms;
static _Atomic uint32_t spatial_calibration_generation;
static _Atomic uint32_t spatial_write_log_count;
static _Atomic uint32_t spatial_hook_log_count;
static _Atomic uint32_t spatial_rejection_log_count;
static _Atomic uint32_t facing_write_log_count;
static _Atomic uint32_t combat_write_log_count;
static float spatial_anchor_local[6];
static float spatial_anchor_remote[6];
static _Atomic int32_t lan_player_role;
static _Atomic uint32_t suppressed_client_damage_count;
static _Atomic uint32_t remote_time_passed_bits;
static _Atomic int64_t remote_fight_state_received_at_ms;
static _Atomic uint32_t pacing_hold_count;
static _Atomic int32_t lan_original_fight_mode;
static _Atomic uint64_t lan_original_fight_fingerprint;
static _Thread_local bool dispatching_remote_control;
static _Thread_local bool forwarding_remote_ai_initialization;
static _Thread_local bool constructing_lan_fight;
static _Thread_local bool applying_remote_damage;
static _Atomic uint32_t profile_item_trace_count;
static _Atomic bool profile_item_trace_enabled;
static _Atomic uint32_t profile_item_set_override_count;
static _Atomic uint32_t fighter_item_override_count;
static _Atomic uint32_t fighter_config_item_block_count;
static _Atomic uint32_t random_profile_override_count;

static pthread_mutex_t fight_config_identity_mutex = PTHREAD_MUTEX_INITIALIZER;
static char lan_original_fight_identity[FIGHT_CONFIG_IDENTITY_MAX + 1];

static bool should_hold_lan_fight(void);
static void hooked_fight_scene_start_fight(void *self, const void *method);

static int64_t now_ms(void);
static uint32_t float_bits(float value);

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size) {
    const uint8_t *bytes = (const uint8_t *)data;
    for (size_t index = 0; index < size; index++) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int32_t managed_string_length(void *managed_string) {
    if (managed_string == NULL) return 0;
    int32_t length = *(int32_t *)((uint8_t *)managed_string + 0x10u);
    return length >= 0 && length <= 4096 ? length : 0;
}

static uint64_t hash_managed_string(uint64_t hash, void *managed_string) {
    int32_t length = managed_string_length(managed_string);
    hash = hash_bytes(hash, &length, sizeof(length));
    if (length > 0) {
        hash = hash_bytes(hash, (uint8_t *)managed_string + 0x14u,
            (size_t)length * sizeof(uint16_t));
    }
    return hash;
}

static size_t copy_managed_string_ascii(void *managed_string, char *output, size_t capacity) {
    if (capacity == 0) return 0;
    int32_t length = managed_string_length(managed_string);
    if (length <= 0) {
        output[0] = '\0';
        return 0;
    }
    const uint16_t *characters = (const uint16_t *)((uint8_t *)managed_string + 0x14u);
    size_t written = 0;
    for (int32_t index = 0; index < length && written + 1 < capacity; index++) {
        uint16_t value = characters[index];
        output[written++] = value >= 0x20 && value <= 0x7e ? (char)value : '?';
    }
    output[written] = '\0';
    return written;
}

static void append_config_identity(
    char *identity,
    size_t capacity,
    uint32_t offset,
    void *managed_string
) {
    char value[128];
    if (copy_managed_string_ascii(managed_string, value, sizeof(value)) == 0) return;
    size_t used = strnlen(identity, capacity);
    if (used >= capacity - 1) return;
    snprintf(identity + used, capacity - used, "%03x=%s;", offset, value);
}

static void capture_original_fight_config(void *config) {
    if (config == NULL) {
        atomic_store_explicit(&lan_original_fight_mode, -1, memory_order_release);
        atomic_store_explicit(&lan_original_fight_fingerprint, 0, memory_order_release);
        return;
    }

    static const uint32_t string_offsets[] = {
        0x30u, 0x38u, 0x48u, 0xE0u, 0x118u, 0x150u, 0x188u
    };
    static const uint32_t scalar_offsets[] = {
        0x18u, 0x28u, 0x50u, 0x54u, 0x64u
    };
    uint64_t fingerprint = UINT64_C(14695981039346656037);
    for (size_t index = 0; index < sizeof(scalar_offsets) / sizeof(scalar_offsets[0]); index++) {
        uint32_t value = *(uint32_t *)((uint8_t *)config + scalar_offsets[index]);
        fingerprint = hash_bytes(fingerprint, &value, sizeof(value));
    }

    char identity[FIGHT_CONFIG_IDENTITY_MAX + 1] = {0};
    for (size_t index = 0; index < sizeof(string_offsets) / sizeof(string_offsets[0]); index++) {
        void *value = *(void **)((uint8_t *)config + string_offsets[index]);
        fingerprint = hash_managed_string(fingerprint, value);
        append_config_identity(identity, sizeof(identity), string_offsets[index], value);
    }
    int32_t mode = *(int32_t *)((uint8_t *)config + FIGHT_CONFIG_MODE_OFFSET);
    if (identity[0] == '\0') {
        snprintf(identity, sizeof(identity), "mode=%d;anonymous-config", mode);
    }
    if (fingerprint == 0) fingerprint = 1;

    pthread_mutex_lock(&fight_config_identity_mutex);
    strncpy(lan_original_fight_identity, identity, FIGHT_CONFIG_IDENTITY_MAX);
    lan_original_fight_identity[FIGHT_CONFIG_IDENTITY_MAX] = '\0';
    pthread_mutex_unlock(&fight_config_identity_mutex);
    atomic_store_explicit(&lan_original_fight_mode, mode, memory_order_release);
    atomic_store_explicit(
        &lan_original_fight_fingerprint, fingerprint, memory_order_release);
    LOGI("FightConfig original: mode=%d fingerprint=%016llx identity=%s.",
        mode, (unsigned long long)fingerprint, identity);
}

#define REMOTE_ITEM_ID_MAX 96

struct remote_fighter_profile {
    bool available;
    int32_t level;
    char armor[REMOTE_ITEM_ID_MAX + 1];
    char helm[REMOTE_ITEM_ID_MAX + 1];
    char weapon[REMOTE_ITEM_ID_MAX + 1];
    char ranged[REMOTE_ITEM_ID_MAX + 1];
    char magic[REMOTE_ITEM_ID_MAX + 1];
};

static pthread_mutex_t remote_profile_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct remote_fighter_profile remote_profile;
static pthread_mutex_t local_profile_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct remote_fighter_profile local_profile;

static struct remote_fighter_profile snapshot_remote_profile(void) {
    struct remote_fighter_profile snapshot;
    pthread_mutex_lock(&remote_profile_mutex);
    snapshot = remote_profile;
    pthread_mutex_unlock(&remote_profile_mutex);
    return snapshot;
}

static struct remote_fighter_profile snapshot_local_profile(void) {
    struct remote_fighter_profile snapshot;
    pthread_mutex_lock(&local_profile_mutex);
    snapshot = local_profile;
    pthread_mutex_unlock(&local_profile_mutex);
    return snapshot;
}

static struct remote_fighter_profile canonical_shadow_profile(void) {
    struct remote_fighter_profile profile = {
        .available = true,
        .level = 52,
        .armor = "Body",
        .helm = "Head",
        .weapon = "WEAPON_NINJA_SWORD",
        .ranged = "NoRanged",
        .magic = "NoMagic",
    };
    return profile;
}

static void *first_list_item(void *list) {
    if (list == NULL) return NULL;
    int32_t size = *(int32_t *)((uint8_t *)list + 0x18u);
    void *items = *(void **)((uint8_t *)list + 0x10u);
    if (size <= 0 || items == NULL) return NULL;
    return *(void **)((uint8_t *)items + 0x20u);
}

static void *resolve_managed_class(const char *name) {
    if (il2cpp_domain_get == NULL || il2cpp_domain_get_assemblies == NULL ||
        il2cpp_assembly_get_image == NULL || il2cpp_class_from_name == NULL) return NULL;

    void *domain = il2cpp_domain_get();
    size_t assembly_count = 0;
    const void **assemblies = domain == NULL ? NULL
        : il2cpp_domain_get_assemblies(domain, &assembly_count);
    for (size_t i = 0; assemblies != NULL && i < assembly_count; i++) {
        void *image = il2cpp_assembly_get_image(assemblies[i]);
        if (image == NULL) continue;
        void *klass = il2cpp_class_from_name(image, "", name);
        if (klass != NULL) return klass;
    }
    return NULL;
}

static void *clone_player_profile(void *source) {
    if (source == NULL || il2cpp_object_new == NULL || player_profile_copy_ctor == NULL) {
        return NULL;
    }
    if (player_profile_class == NULL && !player_profile_class_lookup_attempted) {
        player_profile_class_lookup_attempted = true;
        player_profile_class = resolve_managed_class("EFPINIIBPLJ");
        LOGI("FightPVP: classe de perfil localizada: %p.", player_profile_class);
    }
    if (player_profile_class == NULL) return NULL;

    void *copy = il2cpp_object_new(player_profile_class);
    if (copy == NULL) return NULL;
    player_profile_copy_ctor(copy, source, NULL);
    return copy;
}

static void set_managed_field(void *owner, void **target, void *value) {
    if (il2cpp_gc_wbarrier_set_field != NULL) {
        il2cpp_gc_wbarrier_set_field(owner, target, value);
    } else {
        *target = value;
    }
}

static bool resolve_profile_appearance(
    struct remote_fighter_profile snapshot,
    const char **avatar,
    const char **skeleton
) {
    if (strcmp(snapshot.armor, "Body") == 0 ||
        strcmp(snapshot.armor, "ARMOR_OLD_LEATHER") == 0) {
        *avatar = "avatar_hero";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "BODY_LYNX") == 0) {
        *avatar = "boss_lynx";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "ARMOR_ROBE") == 0) {
        *avatar = "boss_hermit";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "BODY_BUTCHER") == 0) {
        *avatar = "boss_butcher";
        *skeleton = "SkeletonHeavy";
    } else if (strcmp(snapshot.armor, "ARMOR_WASP") == 0) {
        *avatar = "boss_wasp";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "ARMOR_HUNTRESS") == 0) {
        *avatar = "boss_huntress";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "ARMOR_SHOGUN") == 0) {
        *avatar = "boss_shogun";
        *skeleton = "Skeleton";
    } else if (strcmp(snapshot.armor, "BODY_TITAN") == 0) {
        *avatar = "avatar_human";
        *skeleton = "Skeleton";
    } else {
        LOGE("Perfil LAN: aparencia desconhecida para armor=%s.", snapshot.armor);
        return false;
    }
    return true;
}

static bool apply_profile_appearance(
    void *profile,
    struct remote_fighter_profile snapshot,
    const char *label,
    const char **skeleton_id
) {
    const char *avatar = NULL;
    const char *skeleton = NULL;
    if (profile == NULL || il2cpp_string_new == NULL ||
        !resolve_profile_appearance(snapshot, &avatar, &skeleton)) return false;

    void *managed_name = il2cpp_string_new("Shadow");
    void *managed_avatar = il2cpp_string_new(avatar);
    if (managed_name == NULL || managed_avatar == NULL) return false;
    set_managed_field(
        profile, (void **)((uint8_t *)profile + PROFILE_NAME_OFFSET), managed_name);
    set_managed_field(
        profile, (void **)((uint8_t *)profile + PROFILE_AVATAR_OFFSET), managed_avatar);
    if (skeleton_id != NULL) *skeleton_id = skeleton;
    LOGI(
        "Perfil %s: nome=Shadow avatar=%s skeletonCatalogo=%s.",
        label, avatar, skeleton);
    return true;
}

static bool equip_remote_item(
    void *profile,
    const char *slot,
    const char *item_id
) {
    if (profile == NULL || slot == NULL || item_id == NULL || item_id[0] == '\0' ||
        il2cpp_string_new == NULL || item_by_name == NULL ||
        player_profile_set_item == NULL) return false;

    void *managed_item_id = il2cpp_string_new(item_id);
    void *manager = item_manager_current == NULL ? NULL : item_manager_current(NULL);
    void *item = managed_item_id == NULL || manager == NULL || item_manager_by_name == NULL
        ? NULL
        : item_manager_by_name(manager, managed_item_id, NULL);
    const char *resolver = "item-manager";
    if (item == NULL && managed_item_id != NULL && item_by_name != NULL) {
        item = item_by_name(managed_item_id, NULL);
        resolver = "static-fallback";
    }
    if (item == NULL) {
        LOGE("Perfil LAN: item nao localizado slot=%s id=%s.", slot, item_id);
        return false;
    }
    void *managed_slot = il2cpp_string_new(slot);
    if (managed_slot == NULL) return false;
    player_profile_set_item(profile, managed_slot, item, NULL);
    LOGI("Perfil LAN: slot=%s id=%s item=%p resolver=%s.",
        slot, item_id, item, resolver);
    return true;
}

static bool apply_profile_equipment(
    void *profile,
    struct remote_fighter_profile snapshot,
    const char *label
) {
    if (!snapshot.available || profile == NULL) {
        LOGE("Perfil %s indisponivel durante a criacao da luta.", label);
        return false;
    }
    const char *skeleton = NULL;
    bool appearance_applied = apply_profile_appearance(
        profile, snapshot, label, &skeleton);
    unsigned applied = 0;
    applied += equip_remote_item(profile, "Skeleton", skeleton) ? 1u : 0u;
    applied += equip_remote_item(profile, "Armor", snapshot.armor) ? 1u : 0u;
    applied += equip_remote_item(profile, "Helm", snapshot.helm) ? 1u : 0u;
    applied += equip_remote_item(profile, "Weapon", snapshot.weapon) ? 1u : 0u;
    applied += equip_remote_item(profile, "Ranged", snapshot.ranged) ? 1u : 0u;
    applied += equip_remote_item(profile, "Magic", snapshot.magic) ? 1u : 0u;
    LOGI(
        "Perfil %s: avatar=%d itens=%u/6 nivel=%d.",
        label, appearance_applied ? 1 : 0, applied, snapshot.level);
    return appearance_applied && applied == 6;
}

static int32_t clear_managed_reference_list(void *list) {
    if (list == NULL) return 0;
    int32_t size = *(int32_t *)((uint8_t *)list + 0x18u);
    void *items = *(void **)((uint8_t *)list + 0x10u);
    if (size < 0 || size > 4096 || (size > 0 && items == NULL)) return -1;
    for (int32_t index = 0; index < size; index++) {
        void **target = (void **)((uint8_t *)items + 0x20u +
            (size_t)index * sizeof(void *));
        set_managed_field(items, target, NULL);
    }
    *(int32_t *)((uint8_t *)list + 0x18u) = 0;
    (*(int32_t *)((uint8_t *)list + 0x1Cu))++;
    return size;
}

static bool clear_profile_enchantments(void *profile, const char *label) {
    if (profile == NULL) return false;
    int32_t weapon = clear_managed_reference_list(
        *(void **)((uint8_t *)profile + PROFILE_WEAPON_PERKS_OFFSET));
    int32_t armor = clear_managed_reference_list(
        *(void **)((uint8_t *)profile + PROFILE_ARMOR_PERKS_OFFSET));
    int32_t helm = clear_managed_reference_list(
        *(void **)((uint8_t *)profile + PROFILE_HELM_PERKS_OFFSET));
    bool cleared = weapon >= 0 && armor >= 0 && helm >= 0;
    LOGI(
        "Perfil %s: encantamentos removidos weapon=%d armor=%d helm=%d ok=%d.",
        label, weapon, armor, helm, cleared ? 1 : 0);
    return cleared;
}

static void *clone_shadow_profile(void *source, const char *label) {
    void *clone = clone_player_profile(source);
    if (clone == NULL) return NULL;
    struct remote_fighter_profile baseline = canonical_shadow_profile();
    bool equipment = apply_profile_equipment(clone, baseline, label);
    bool no_enchantments = clear_profile_enchantments(clone, label);
    if (!equipment || !no_enchantments) return NULL;
    return clone;
}

// RANDOM_EQUIPMENT cria um perfil temporario e alguns consumidores guardam os
// itens escolhidos antes que o setter virtual possa substitui-los. Durante uma
// luta LAN, entregue diretamente uma copia independente do Shadow canonico.
// Fora dessa janela, preserve integralmente a fabrica original do jogo.
static void *hooked_random_fighter_profile_create(const void *method) {
    bool lan_active = atomic_load_explicit(
        &session_ready, memory_order_acquire) &&
        (constructing_lan_fight || atomic_load_explicit(
            &lan_fight_active, memory_order_acquire));
    if (lan_active) {
        void *source = (void *)atomic_load_explicit(
            &lan_remote_profile, memory_order_acquire);
        void *clone = clone_shadow_profile(source, "perfil-aleatorio-LAN");
        if (clone != NULL) {
            uint32_t index = atomic_fetch_add_explicit(
                &random_profile_override_count, 1u, memory_order_relaxed);
            if (index < 16u) {
                LOGI(
                    "Fabrica RANDOM_EQUIPMENT substituida por Shadow canonico: source=%p clone=%p.",
                    source, clone);
            }
            return clone;
        }
        LOGE("Fabrica RANDOM_EQUIPMENT: falha ao clonar perfil canonico; usando original.");
    }
    return original_random_fighter_profile_create(method);
}

static void log_profile_equipment(const char *label, void *profile) {
    if (profile == NULL) {
        LOGI("Perfil %s: nulo.", label);
        return;
    }
    LOGI(
        "Perfil %s=%p itens skeleton=%p armor=%p helm=%p weapon=%p ranged=%p magic=%p.",
        label, profile,
        *(void **)((uint8_t *)profile + PROFILE_SKELETON_OFFSET),
        *(void **)((uint8_t *)profile + PROFILE_ARMOR_OFFSET),
        *(void **)((uint8_t *)profile + PROFILE_HELM_OFFSET),
        *(void **)((uint8_t *)profile + PROFILE_WEAPON_OFFSET),
        *(void **)((uint8_t *)profile + PROFILE_RANGED_OFFSET),
        *(void **)((uint8_t *)profile + PROFILE_MAGIC_OFFSET));
}

static int32_t replace_all_list_items(void *list, void *value) {
    if (list == NULL || value == NULL) return 0;
    int32_t size = *(int32_t *)((uint8_t *)list + 0x18u);
    void *items = *(void **)((uint8_t *)list + 0x10u);
    if (size <= 0 || size > 4096 || items == NULL) return 0;
    for (int32_t index = 0; index < size; index++) {
        void **target = (void **)((uint8_t *)items + 0x20u +
            (size_t)index * sizeof(void *));
        set_managed_field(items, target, value);
    }
    return size;
}

// Esta fabrica e executada antes da FightScene e alimenta os modelos visuais
// de PreFight/Viewer. Padronizar apenas a FightScene deixa esses modelos com o
// equipamento RANDOM_EQUIPMENT que ja havia sido materializado. Substitua as
// duas saidas no primeiro ponto comum, mantendo a rotina original fora do LAN.
static void hooked_create_fight_profiles(
    void *fight_config,
    void **player_output,
    void **opponents_output,
    const void *method
) {
    original_create_fight_profiles(
        fight_config, player_output, opponents_output, method);
    if (!atomic_load_explicit(&session_ready, memory_order_acquire) ||
        player_output == NULL || opponents_output == NULL) return;

    void *original_player = *player_output;
    void *opponents = *opponents_output;
    void *original_opponent = first_list_item(opponents);
    if (original_opponent == NULL) original_opponent = original_player;
    // Preserve duas bases independentes. Clonar os dois lados da mesma base
    // faz o jogo compartilhar estruturas internas de animacao e StartFight
    // deixa de retornar. O preset canonico aplicado abaixo continua igualando
    // nome e equipamento sem fundir o estado de cada lutador.
    void *player_shadow = clone_shadow_profile(
        original_player, "fabrica-visual-esquerda");
    void *opponent_shadow = clone_shadow_profile(
        original_opponent, "fabrica-visual-direita");
    if (player_shadow == NULL || opponent_shadow == NULL) {
        LOGE(
            "Fabrica visual LAN: clones indisponiveis player=%p opponent=%p.",
            player_shadow, opponent_shadow);
        return;
    }
    int32_t replaced = replace_all_list_items(opponents, opponent_shadow);
    *player_output = player_shadow;
    LOGI(
        "Fabrica visual LAN padronizada: config=%p player=%p opponent=%p adversarios=%d.",
        fight_config, player_shadow, opponent_shadow, replaced);
}

static void *prepare_pvp_fight_config(
    void *fight_config,
    void *player_profile,
    void *opponents,
    void **effective_player_profile
) {
    if (fight_config_clear_player == NULL || fight_config_set_player == NULL ||
        fight_config_clear_opponent == NULL || fight_config_set_opponent == NULL ||
        fight_config == NULL) return NULL;

    // Mantem Duel para preservar os lados Player/Opponent e a virada nativa.
    // A inicializacao de entrada e separada dos eventos de decisao da IA no
    // hook do Fighter, abaixo.
    void *config = fight_config;
    void *original_opponent = first_list_item(opponents);
    void *player_shadow = clone_shadow_profile(
        player_profile, "Shadow-esquerda");
    void *opponent_shadow = clone_shadow_profile(
        original_opponent, "Shadow-direita");
    if (config == NULL || player_shadow == NULL || opponent_shadow == NULL) {
        LOGE(
            "FightLAN Duel: clones Shadow indisponiveis config=%p playerClone=%p opponentClone=%p originalPlayer=%p originalOpponent=%p.",
            fight_config, player_shadow, opponent_shadow,
            player_profile, original_opponent);
        return NULL;
    }

    // Duel usa o sentinela RANDOM_EQUIPMENT neste campo. Ele materializa um
    // conjunto aleatorio em caches visuais depois que os perfis Shadow ja
    // foram preparados, fazendo cada processo escolher armas diferentes.
    // Com a regra vazia, o getter nativo deixa de solicitar esse override e
// os dois lados conservam o equipamento canonico dos perfis clonados.
    void **base_equipment_rule = (void **)((uint8_t *)config +
        FIGHT_CONFIG_BASE_EQUIPMENT_RULE_OFFSET);
    void **equipment_rule = (void **)((uint8_t *)config +
        FIGHT_CONFIG_EQUIPMENT_RULE_OFFSET);
    void *original_base_equipment_rule = *base_equipment_rule;
    void *original_equipment_rule = *equipment_rule;
    set_managed_field(config, base_equipment_rule, NULL);
    set_managed_field(config, equipment_rule, NULL);
    LOGI(
        "FightLAN Duel: regras RANDOM_EQUIPMENT removidas config=%p baseOriginal=%p overrideOriginal=%p.",
        config, original_base_equipment_rule, original_equipment_rule);

    LOGI(
        "FightLAN Duel: quatro modelos fixados no Shadow humano basico, sem encantamentos, ranged ou magia.");
    atomic_store_explicit(
        &lan_remote_profile, (uintptr_t)opponent_shadow, memory_order_release);
    log_profile_equipment("Shadow-esquerda", player_shadow);
    log_profile_equipment("Shadow-direita", opponent_shadow);
    int32_t replaced_opponents = replace_all_list_items(opponents, opponent_shadow);
    if (replaced_opponents <= 0) {
        LOGE("FightLAN Duel: lista de adversarios nao aceitou o perfil remoto.");
        return NULL;
    }

    fight_config_clear_player(config, NULL);
    fight_config_clear_opponent(config, NULL);
    fight_config_set_player(config, player_shadow, NULL);
    fight_config_set_opponent(config, opponent_shadow, NULL);
    if (effective_player_profile != NULL) {
        *effective_player_profile = player_shadow;
    }
    LOGI(
        "FightLAN Duel humano: mode=%d config=%p playerProfile=%p opponentClone=%p originalOpponent=%p adversariosSubstituidos=%d.",
        *(int32_t *)((uint8_t *)config + FIGHT_CONFIG_MODE_OFFSET), config,
        player_shadow, opponent_shadow, original_opponent, replaced_opponents);
    return config;
}

// O Duel consulta esta regra ainda na preparacao da tela, antes de
// FightScene.CreateFight. Bloquear somente dentro de prepare_pvp_fight_config
// era tarde demais: os modelos ja podiam carregar o conjunto aleatorio.
// O getter nativo admite retorno nulo (ele proprio usa um campo opcional de
// fallback), portanto durante a sessao LAN fazemos a luta conservar os itens
// Body/Head/WEAPON_NINJA_SWORD dos perfis Shadow canonicos desde a primeira
// leitura. O ID da espada precisa chegar antes da criacao do controlador de
// animacao; trocar somente o modelo visual mantem a pose de soco-ingles.
static void *hooked_fight_config_equipment_rule_get(
    void *self,
    const void *method
) {
    if (atomic_load_explicit(&session_ready, memory_order_acquire)) {
        return NULL;
    }
    return original_fight_config_equipment_rule_get(self, method);
}

static void managed_string_to_ascii(
    void *managed_string,
    char *destination,
    size_t capacity
) {
    if (destination == NULL || capacity == 0) return;
    destination[0] = '\0';
    if (managed_string == NULL) return;

    int32_t length = *(int32_t *)((uint8_t *)managed_string + 0x10u);
    if (length < 0 || length > 4096) return;
    const uint16_t *characters = (const uint16_t *)((uint8_t *)managed_string + 0x14u);
    size_t count = (size_t)length;
    if (count >= capacity) count = capacity - 1;
    for (size_t index = 0; index < count; index++) {
        uint16_t character = characters[index];
        destination[index] = character >= 0x20u && character <= 0x7Eu
            ? (char)character : '?';
    }
    destination[count] = '\0';
}

static bool managed_string_equals_ascii(void *managed_string, const char *ascii) {
    if (managed_string == NULL || ascii == NULL) return false;
    int32_t length = *(int32_t *)((uint8_t *)managed_string + 0x10u);
    size_t ascii_length = strlen(ascii);
    if (length < 0 || (size_t)length != ascii_length) return false;
    const uint16_t *characters = (const uint16_t *)((uint8_t *)managed_string + 0x14u);
    for (size_t index = 0; index < ascii_length; index++) {
        if (characters[index] != (uint8_t)ascii[index]) return false;
    }
    return true;
}

static void *canonical_profile_item_for_slot(void *slot) {
    if (!atomic_load_explicit(&session_ready, memory_order_acquire) ||
        (!constructing_lan_fight && !atomic_load_explicit(
            &lan_fight_active, memory_order_acquire))) {
        return NULL;
    }
    void *profile = (void *)atomic_load_explicit(
        &lan_remote_profile, memory_order_acquire);
    if (profile == NULL) return NULL;

    size_t offset = 0;
    if (managed_string_equals_ascii(slot, "Skeleton")) {
        offset = PROFILE_SKELETON_OFFSET;
    } else if (managed_string_equals_ascii(slot, "Weapon")) {
        offset = PROFILE_WEAPON_OFFSET;
    } else if (managed_string_equals_ascii(slot, "Armor")) {
        offset = PROFILE_ARMOR_OFFSET;
    } else if (managed_string_equals_ascii(slot, "Helm")) {
        offset = PROFILE_HELM_OFFSET;
    } else if (managed_string_equals_ascii(slot, "Ranged")) {
        offset = PROFILE_RANGED_OFFSET;
    } else if (managed_string_equals_ascii(slot, "Magic")) {
        offset = PROFILE_MAGIC_OFFSET;
    }
    return offset == 0 ? NULL : *(void **)((uint8_t *)profile + offset);
}

static bool should_trace_profile_item_get(void) {
    if (!atomic_load_explicit(&session_ready, memory_order_acquire) ||
        !atomic_load_explicit(&profile_item_trace_enabled, memory_order_acquire)) {
        return false;
    }
    uint32_t index = atomic_fetch_add_explicit(
        &profile_item_trace_count, 1u, memory_order_relaxed);
    if (index >= 96u) {
        atomic_store_explicit(
            &profile_item_trace_enabled, false, memory_order_release);
        return false;
    }
    return true;
}

// Diagnostico temporario: o perfil ja contem os seis ponteiros canonicos, mas
// a cena ainda desenha armas diferentes. Estes dois acessores revelam quais
// slots o construtor visual consulta e qual objeto ele realmente recebe.
static void *hooked_player_profile_item_by_slot(
    void *profile,
    void *slot,
    const void *method
) {
    void *original_item = original_player_profile_item_by_slot(
        profile, slot, method);
    void *canonical_item = canonical_profile_item_for_slot(slot);
    void *item = canonical_item == NULL ? original_item : canonical_item;
    if (should_trace_profile_item_get()) {
        char slot_name[96];
        managed_string_to_ascii(slot, slot_name, sizeof(slot_name));
        LOGI("Perfil getter privado: profile=%p slot=%s original=%p retorno=%p override=%d.",
            profile, slot_name, original_item, item,
            canonical_item != NULL ? 1 : 0);
    }
    return item;
}

static void *hooked_player_profile_item_get(
    void *profile,
    void *slot,
    void *fallback,
    const void *method
) {
    void *original_item = original_player_profile_item_get(
        profile, slot, fallback, method);
    void *canonical_item = canonical_profile_item_for_slot(slot);
    void *item = canonical_item == NULL ? original_item : canonical_item;
    if (should_trace_profile_item_get()) {
        char slot_name[96];
        char fallback_name[96];
        managed_string_to_ascii(slot, slot_name, sizeof(slot_name));
        managed_string_to_ascii(fallback, fallback_name, sizeof(fallback_name));
        LOGI("Perfil getter virtual: profile=%p slot=%s fallback=%s original=%p retorno=%p override=%d.",
            profile, slot_name, fallback_name, original_item, item,
            canonical_item != NULL ? 1 : 0);
    }
    return item;
}

static void hooked_player_profile_set_item(
    void *profile,
    void *slot,
    void *item,
    const void *method
) {
    void *canonical_item = canonical_profile_item_for_slot(slot);
    void *effective_item = canonical_item == NULL ? item : canonical_item;
    if (canonical_item != NULL && canonical_item != item) {
        uint32_t index = atomic_fetch_add_explicit(
            &profile_item_set_override_count, 1u, memory_order_relaxed);
        if (index < 48u) {
            char slot_name[96];
            void *caller = __builtin_return_address(0);
            Dl_info caller_info;
            memset(&caller_info, 0, sizeof(caller_info));
            uintptr_t caller_rva = 0u;
            if (caller != NULL && dladdr(caller, &caller_info) != 0 &&
                caller_info.dli_fbase != NULL) {
                caller_rva = (uintptr_t)caller -
                    (uintptr_t)caller_info.dli_fbase;
            }
            managed_string_to_ascii(slot, slot_name, sizeof(slot_name));
            LOGI(
                "Perfil setter LAN: profile=%p slot=%s solicitado=%p canonico=%p callerRva=0x%" PRIxPTR ".",
                profile, slot_name, item, canonical_item, caller_rva);
        }
    }
    player_profile_set_item(profile, slot, effective_item, method);
}

static void hooked_fighter_apply_equipment_item(
    void *fighter,
    void *item,
    bool equip,
    bool refresh,
    bool update_effects,
    const void *method
) {
    void *slot = item == NULL ? NULL : *(void **)((uint8_t *)item + 0x30u);
    void *canonical_item = canonical_profile_item_for_slot(slot);
    void *effective_item = canonical_item == NULL ? item : canonical_item;
    if (canonical_item != NULL && canonical_item != item) {
        uint32_t index = atomic_fetch_add_explicit(
            &fighter_item_override_count, 1u, memory_order_relaxed);
        if (index < 48u) {
            char slot_name[96];
            managed_string_to_ascii(slot, slot_name, sizeof(slot_name));
            LOGI("Fighter item LAN: fighter=%p slot=%s solicitado=%p canonico=%p.",
                fighter, slot_name, item, canonical_item);
        }
    }
    original_fighter_apply_equipment_item(
        fighter, effective_item, equip, refresh, update_effects, method);
}

// O duelo materializa cada item sorteado diretamente no modelo do Fighter
// antes de gravá-lo no PlayerProfile. Os hooks do perfil garantem que ambas as
// instâncias concordem sobre o save, mas chegam tarde demais para impedir essa
// aparência divergente. Na luta LAN, o perfil já contém o conjunto canônico
// completo; portanto ignore somente essa etapa de materialização aleatória.
static void hooked_fighter_apply_config_item(
    void *fighter,
    void *config_item,
    void *profile,
    const void *method
) {
    bool lan_active = atomic_load_explicit(
        &session_ready, memory_order_acquire) &&
        (constructing_lan_fight || atomic_load_explicit(
            &lan_fight_active, memory_order_acquire));
    if (lan_active) {
        uint32_t index = atomic_fetch_add_explicit(
            &fighter_config_item_block_count, 1u, memory_order_relaxed);
        if (index < 48u) {
            void *item_id = config_item == NULL ? NULL :
                *(void **)((uint8_t *)config_item + 0x18u);
            char item_name[96];
            managed_string_to_ascii(item_id, item_name, sizeof(item_name));
            LOGI(
                "Fighter config item LAN bloqueado: fighter=%p profile=%p item=%s config=%p.",
                fighter, profile, item_name, config_item);
        }
        return;
    }
    original_fighter_apply_config_item(fighter, config_item, profile, method);
}

// O Fighter copia arma, esqueleto e dados de animacao para caches internos no
// construtor. Alterar somente o PlayerProfile usado por FightScene e suficiente
// para o HUD, mas nao para o modelo visual. Reaplique o preset canonico no
// ultimo ponto anterior a essa copia, limitado a janela de criacao da luta LAN.
static void hooked_fighter_ctor(
    void *fighter,
    void *profile,
    const void *method
) {
    if (constructing_lan_fight && atomic_load_explicit(
        &session_ready, memory_order_acquire)) {
        int32_t role = atomic_load_explicit(&lan_player_role, memory_order_acquire);
        struct remote_fighter_profile canonical_profile = canonical_shadow_profile();
        bool applied = apply_profile_equipment(
            profile, canonical_profile, "construtor-do-Fighter");
        LOGI(
            "Fighter.ctor LAN: fighter=%p profile=%p presetHumano=%d role=%d.",
            fighter, profile, applied ? 1 : 0, role);
    }
    original_fighter_ctor(fighter, profile, method);
}

// O ViewerFight cria e pode reter o modelo do jogador antes de
// FightScene.CreateFight. Se o perfil for alterado somente dentro da
// FightScene, o lutador da esquerda continua com a arma/pose do duelo
// anterior enquanto o da direita usa o preset LAN. Padronize ambos aqui,
// antes de qualquer modelo visual ser construído, sempre com o humano comum
// anunciado pela sessão.
static void hooked_pre_fight_viewer_init(
    void *self,
    void *viewer_config,
    void *player_profile,
    void *opponent_profile,
    bool show_fight,
    const void *method
) {
    bool session_active = atomic_load_explicit(
        &session_ready, memory_order_acquire);
    if (session_active) {
        atomic_store_explicit(&profile_item_trace_count, 0u, memory_order_release);
        atomic_store_explicit(
            &profile_item_trace_enabled, true, memory_order_release);
        int32_t role = atomic_load_explicit(
            &lan_player_role, memory_order_acquire);
        struct remote_fighter_profile canonical_profile = canonical_shadow_profile();
        bool player_applied = apply_profile_equipment(
            player_profile, canonical_profile, "ViewerInit-esquerda");
        bool opponent_applied = apply_profile_equipment(
            opponent_profile, canonical_profile, "ViewerInit-direita");
        LOGI(
            "PreFight.ViewerInit LAN: config=%p player=%p opponent=%p humanoEsquerda=%d humanoDireita=%d role=%d.",
            viewer_config, player_profile, opponent_profile,
            player_applied ? 1 : 0, opponent_applied ? 1 : 0, role);
    }
    original_pre_fight_viewer_init(
        self, viewer_config, player_profile, opponent_profile, show_fight, method);
}

// CreateVS e o primeiro ponto conhecido que recebe os perfis do jogador e
// dos adversarios antes de FightScene/Fighter copiarem arma e controlador de
// animacao. Corrigir aqui evita que a cena reutilize o sai ou o esqueleto do
// save original mesmo quando o PlayerProfile posterior ja informa Fists.
static void hooked_pre_fight_create_vs(
    void *self,
    void *player_profile,
    void *opponents,
    int32_t round,
    bool first_flag,
    bool second_flag,
    bool third_flag,
    bool fourth_flag,
    const void *method
) {
    void *effective_player = player_profile;
    if (atomic_load_explicit(&session_ready, memory_order_acquire)) {
        int32_t role = atomic_load_explicit(
            &lan_player_role, memory_order_acquire);
        void *original_opponent = first_list_item(opponents);
        void *player_shadow = clone_shadow_profile(
            player_profile, "CreateVS-Shadow-esquerda");
        void *opponent_shadow = clone_shadow_profile(
            original_opponent, "CreateVS-Shadow-direita");
        int32_t opponents_replaced = 0;
        if (player_shadow != NULL && opponent_shadow != NULL) {
            effective_player = player_shadow;
            opponents_replaced = replace_all_list_items(opponents, opponent_shadow);
        }
        LOGI(
            "PreFight.CreateVS LAN: round=%d playerOriginal=%p playerShadow=%p opponentShadow=%p adversariosSubstituidos=%d role=%d.",
            round, player_profile, player_shadow, opponent_shadow,
            opponents_replaced, role);
    }
    original_pre_fight_create_vs(
        self, effective_player, opponents, round, first_flag, second_flag,
        third_flag, fourth_flag, method);
}

static void hooked_register_input_binding(
    int32_t fighter_side,
    int32_t device_type,
    int32_t device_index,
    const void *method
) {
    original_register_input_binding(fighter_side, device_type, device_index, method);
    LOGI(
        "Input binding: fighter_side=%d device_type=%d device_index=%d.",
        fighter_side, device_type, device_index);
}

static void *hooked_resolve_input_fighter(void *input, const void *method) {
    void *fighter = original_resolve_input_fighter(input, method);
    if (atomic_load_explicit(&fight_create_count, memory_order_acquire) == 0) return fighter;

    uint32_t log_number = atomic_fetch_add_explicit(
        &input_route_log_count, 1, memory_order_relaxed) + 1;
    if (log_number <= 40) {
        int32_t control = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x10u);
        int32_t index = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x14u);
        LOGI(
            "Input route #%u: input=%p control=%d index=%d fighter=%p.",
            log_number, input, control, index, fighter);
    } else if (log_number == 41) {
        LOGI("Input route: limite diagnostico de 40 eventos atingido.");
    }
    return fighter;
}

static bool is_lan_remote_fighter(void *fighter) {
    if (fighter == NULL || !atomic_load_explicit(
        &session_ready, memory_order_acquire)) return false;

    uintptr_t known_fighter = atomic_load_explicit(
        &lan_remote_fighter, memory_order_acquire);
    if (known_fighter != 0 && known_fighter == (uintptr_t)fighter) return true;

    uintptr_t remote_profile = atomic_load_explicit(
        &lan_remote_profile, memory_order_acquire);
    if (remote_profile == 0) return false;
    void *fighter_profile = *(void **)((uint8_t *)fighter + FIGHTER_PROFILE_OFFSET);
    return (uintptr_t)fighter_profile == remote_profile;
}

static bool is_lan_fighter(void *fighter) {
    if (fighter == NULL || !atomic_load_explicit(
        &session_ready, memory_order_acquire)) return false;

    uintptr_t candidate = (uintptr_t)fighter;
    return candidate == atomic_load_explicit(
            &lan_local_fighter, memory_order_acquire) ||
        candidate == atomic_load_explicit(
            &lan_remote_fighter, memory_order_acquire);
}

static void *hooked_ai_planner_select_action(
    void *planner,
    void *opponent,
    int32_t action_index,
    const void *method
) {
    uintptr_t remote_planner = atomic_load_explicit(
        &lan_remote_ai_planner, memory_order_acquire);
    if (remote_planner != 0 && (uintptr_t)planner == remote_planner &&
        atomic_load_explicit(&lan_fight_active, memory_order_acquire)) {
        uint32_t number = atomic_fetch_add_explicit(
            &suppressed_remote_ai_input_count, 1, memory_order_relaxed) + 1;
        if (number <= 20) {
            LOGI(
                "Selecao autonoma remota bloqueada #%u: planner=%p opponent=%p actionIndex=%d.",
                number, planner, opponent, action_index);
        }
        return NULL;
    }
    return original_ai_planner_select_action(
        planner, opponent, action_index, method);
}

// HPDIKFPFIEI.HPLCHBDPFMC encaminha cada evento de animacao ao NDABNAKKEFE,
// o planejador da IA. No adversario LAN esses eventos precisam ser ignorados:
// os comandos validos chegam por FightInput com o indice reservado ao jogador 2.
static void hooked_fighter_ai_animation_event(
    void *fighter,
    void *animation,
    const void *method
) {
#if SF2_LAN_DIAGNOSTIC_ALLOW_REMOTE_AI
    // Exclusivo de builds de diagnostico: deixa o oponente agir para que dano
    // real revele quais campos IL2CPP representam vida/energia. A build LAN
    // normal compila este bloco como zero e continua suprimindo a IA remota.
    original_fighter_ai_animation_event(fighter, animation, method);
    return;
#endif
    // StartFight pode ser chamado uma unica vez durante a apresentacao e ser
    // adiado enquanto os dois peers aguardam o mesmo START. Nessa situacao a
    // FightScene ainda nao recebe FixedUpdate, portanto nao existe uma segunda
    // oportunidade natural para liberar a luta. Os eventos de animacao rodam
    // na thread principal durante a intro e servem como ponto seguro para
    // repetir o StartFight depois do instante negociado.
    if (atomic_load_explicit(&lan_fight_active, memory_order_acquire) &&
        !atomic_load_explicit(&fight_start_method_invoked, memory_order_acquire)) {
        void *scene = (void *)atomic_load_explicit(
            &lan_active_scene, memory_order_acquire);
        if (scene != NULL && !should_hold_lan_fight()) {
            hooked_fight_scene_start_fight(scene, NULL);
        }
    }
    if (!is_lan_fighter(fighter)) {
        original_fighter_ai_animation_event(fighter, animation, method);
        return;
    }

    bool is_remote = is_lan_remote_fighter(fighter);

    // O Duel exige a inicializacao do planejador remoto para concluir a
    // apresentacao dos lutadores. Inicialize-o uma unica vez, mas mantenha a
    // selecao de acoes bloqueada no hook acima; os unicos comandos aceitos
    // depois disso sao os FightInput recebidos do segundo jogador.
    if (is_remote && ai_planner_initialize != NULL && !atomic_load_explicit(
        &remote_ai_initialization_forwarded, memory_order_acquire)) {
        void *planner = *(void **)((uint8_t *)fighter +
            FIGHTER_AI_PLANNER_OFFSET);
        void *opponent = *(void **)((uint8_t *)fighter +
            FIGHTER_OPPONENT_OFFSET);
        if (planner != NULL && opponent != NULL) {
            atomic_store_explicit(
                &lan_remote_ai_planner, (uintptr_t)planner, memory_order_release);
            atomic_store_explicit(
                &remote_ai_initialization_forwarded, true, memory_order_release);
            forwarding_remote_ai_initialization = true;
            ai_planner_initialize(planner, opponent, NULL);
            forwarding_remote_ai_initialization = false;
            if (ai_planner_cancel != NULL) {
                ai_planner_cancel(planner, NULL);
            }
            LOGI(
                "Planejador remoto inicializado e cancelado somente para liberar a intro: planner=%p opponent=%p; selecao autonoma bloqueada.",
                planner, opponent);
        } else {
            LOGI(
                "Planejador remoto ainda indisponivel na intro: planner=%p opponent=%p.",
                planner, opponent);
        }
    }

    uint32_t number = atomic_fetch_add_explicit(
        &suppressed_remote_ai_log_count, 1, memory_order_relaxed) + 1;
    if (number <= 20) {
        LOGI(
            "IA LAN suprimida #%u: lado=%s fighter=%p profile=%p animation=%p.",
            number, is_remote ? "remoto" : "local", fighter,
            *(void **)((uint8_t *)fighter + FIGHTER_PROFILE_OFFSET), animation);
    } else if (number == 21) {
        LOGI("IA LAN: limite diagnostico de 20 eventos atingido.");
    }
}

static void hooked_fight_scene_start_fight(void *self, const void *method) {
    bool lan_active = atomic_load_explicit(
        &lan_fight_active, memory_order_acquire);
    if (!lan_active) {
        original_fight_scene_start_fight(self, method);
        return;
    }
    if (should_hold_lan_fight()) {
        LOGI("FightScene.StartFight adiado ate o START sincronizado.");
        return;
    }
    bool already_started = atomic_exchange_explicit(
        &fight_start_method_invoked, true, memory_order_acq_rel);
    if (already_started) return;
    original_fight_scene_start_fight(self, method);
    LOGI("FightScene.StartFight executado sem liberar planejadores de IA.");
}

static void log_fight_input_source(void *fight, void *input, bool pressed) {
    if (atomic_load_explicit(&fight_create_count, memory_order_acquire) == 0) return;

    uint32_t number = atomic_fetch_add_explicit(
        &fight_input_source_log_count, 1, memory_order_relaxed) + 1;
    if (number <= 120) {
        int32_t control = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x10u);
        int32_t index = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x14u);
        LOGI(
            "Fight input source #%u: remote=%d pressed=%d control=%d index=%d input=%p fight=%p.",
            number, dispatching_remote_control ? 1 : 0, pressed ? 1 : 0,
            control, index, input, fight);
    } else if (number == 121) {
        LOGI("Fight input source: limite diagnostico de 120 eventos atingido.");
    }
}

static uint32_t control_to_button(int32_t control) {
    switch (control) {
        case 9: return BUTTON_PUNCH;
        case 10: return BUTTON_KICK;
        case 11: return BUTTON_RANGED;
        case 12: return BUTTON_MAGIC;
        case 14: return BUTTON_BOSS;
        default: return 0;
    }
}

static void capture_local_touch(void *input, bool active) {
    if (dispatching_remote_control || input == NULL) return;
    int32_t control = *(int32_t *)((uint8_t *)input + 0x10u);
    int32_t index = *(int32_t *)((uint8_t *)input + 0x14u);
    if (index != PLAYER_ONE_INDEX) return;

    if (control >= 1 && control <= 8) {
        atomic_store_explicit(
            &local_touch_direction, active ? control : 0, memory_order_release);
        return;
    }
    uint32_t mask = control_to_button(control);
    if (mask == 0) return;
    if (active) {
        atomic_fetch_or_explicit(&local_touch_buttons, mask, memory_order_relaxed);
        // Preserva toques mais curtos do que o intervalo de envio UDP.
        atomic_fetch_or_explicit(&local_touch_button_pulses, mask, memory_order_relaxed);
    } else {
        atomic_fetch_and_explicit(&local_touch_buttons, ~mask, memory_order_relaxed);
    }
}

static bool dispatch_local_control_as_player_two(
    void *fight,
    void *source_input,
    bool pressed
);

// 0x34EC58C recebe o inicio/atualizacao do toque; 0x34E94C4 recebe o fim.
// Os nomes anteriores estavam invertidos, o que enviava a sequencia remota ao contrario.
static void hooked_fight_input_activate(void *fight, void *input, const void *method) {
    log_fight_input_source(fight, input, true);
    if (forwarding_remote_ai_initialization) {
        uint32_t number = atomic_fetch_add_explicit(
            &suppressed_remote_ai_input_count, 1, memory_order_relaxed) + 1;
        if (number <= 20) {
            int32_t control = input == NULL ? -1
                : *(int32_t *)((uint8_t *)input + 0x10u);
            int32_t index = input == NULL ? -1
                : *(int32_t *)((uint8_t *)input + 0x14u);
            LOGI(
                "Comando da inicializacao da IA descartado #%u: pressed=1 control=%d index=%d.",
                number, control, index);
        }
        return;
    }
    capture_local_touch(input, true);
    int32_t index = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x14u);
    if (index == PLAYER_ONE_INDEX && atomic_load_explicit(
        &lan_player_role, memory_order_acquire) == 2 &&
        dispatch_local_control_as_player_two(fight, input, true)) return;
    original_fight_input_activate(fight, input, method);
}

static void hooked_fight_input_deactivate(void *fight, void *input, const void *method) {
    log_fight_input_source(fight, input, false);
    if (forwarding_remote_ai_initialization) {
        uint32_t number = atomic_fetch_add_explicit(
            &suppressed_remote_ai_input_count, 1, memory_order_relaxed) + 1;
        if (number <= 20) {
            int32_t control = input == NULL ? -1
                : *(int32_t *)((uint8_t *)input + 0x10u);
            int32_t index = input == NULL ? -1
                : *(int32_t *)((uint8_t *)input + 0x14u);
            LOGI(
                "Comando da inicializacao da IA descartado #%u: pressed=0 control=%d index=%d.",
                number, control, index);
        }
        return;
    }
    capture_local_touch(input, false);
    int32_t index = input == NULL ? -1 : *(int32_t *)((uint8_t *)input + 0x14u);
    if (index == PLAYER_ONE_INDEX && atomic_load_explicit(
        &lan_player_role, memory_order_acquire) == 2 &&
        dispatch_local_control_as_player_two(fight, input, false)) return;
    original_fight_input_deactivate(fight, input, method);
}

static void log_player_two_input_once(void) {
    if (atomic_load_explicit(&fight_create_count, memory_order_acquire) == 0) return;
    bool was_logged = atomic_exchange_explicit(
        &logged_player_two_input, true, memory_order_acq_rel);
    if (!was_logged) {
        LOGI("FightScene ativa consultou o GamePad 1 pela primeira vez.");
    }
}

static void hooked_fight_scene_create_fight(
    void *self,
    void *fight_config,
    void *player_profile,
    void *opponents,
    const void *method
) {
    bool activate_lan_fight = atomic_load_explicit(
        &session_ready, memory_order_acquire);
    atomic_store_explicit(&lan_remote_profile, 0, memory_order_release);
    atomic_store_explicit(&lan_local_fighter, 0, memory_order_release);
    atomic_store_explicit(&lan_remote_fighter, 0, memory_order_release);
    atomic_store_explicit(&lan_remote_ai_planner, 0, memory_order_release);
    capture_original_fight_config(fight_config);
    int32_t original_mode = fight_config == NULL ? -1
        : *(int32_t *)((uint8_t *)fight_config + FIGHT_CONFIG_MODE_OFFSET);
    void *effective_player_profile = player_profile;
    void *pvp_config = activate_lan_fight
        ? prepare_pvp_fight_config(
            fight_config, player_profile, opponents, &effective_player_profile)
        : NULL;
    bool lan_configured = activate_lan_fight && pvp_config != NULL;
    void *effective_config = pvp_config == NULL ? fight_config : pvp_config;
    LOGI(
        "FightScene: sessaoPronta=%d modo original=%d configOriginal=%p configEfetiva=%p.",
        activate_lan_fight ? 1 : 0, original_mode, fight_config, effective_config);

    if (lan_configured) {
        void *gamepad_controller = gamepad_controller_current(NULL);
        if (gamepad_controller != NULL) {
            set_gamepad_dual_mode(gamepad_controller, true, NULL);
            // Mantem os dois lados vinculados explicitamente: os controles
            // nativos continuam no jogador local (Touch 0) e os comandos LAN
            // usam Touch 1, sem trocar a semantica Player/Opponent do Duel.
            original_register_input_binding(0, 0, PLAYER_ONE_INDEX, NULL);
            original_register_input_binding(1, 0, PLAYER_TWO_INDEX, NULL);
            LOGI(
                "FightScene: DualMode ativo e vinculos Player -> Touch 0 / Enemy -> Touch 1 solicitados.");
        } else {
            LOGE("FightScene: GamePadController.Current indisponivel.");
        }
    }

    constructing_lan_fight = lan_configured;
    original_fight_scene_create_fight(
        self, effective_config, effective_player_profile, opponents, method);
    constructing_lan_fight = false;

    void *fight = self == NULL ? NULL : *(void **)((uint8_t *)self + FIGHT_SCENE_FIGHT_OFFSET);
    void *player = fight == NULL ? NULL : *(void **)((uint8_t *)fight + FIGHT_PLAYER_OFFSET);
    void *opponent = fight == NULL ? NULL : *(void **)((uint8_t *)fight + FIGHT_OPPONENT_OFFSET);
    void *fight_player_profile = fight == NULL ? NULL
        : *(void **)((uint8_t *)fight + FIGHT_PLAYER_PROFILE_OFFSET);
    void *fight_opponent_profile = fight == NULL ? NULL
        : *(void **)((uint8_t *)fight + FIGHT_OPPONENT_PROFILE_OFFSET);
    uint32_t count = atomic_fetch_add_explicit(
        &fight_create_count, 1, memory_order_relaxed) + 1;
    atomic_store_explicit(&injected_buttons, 0, memory_order_release);
    atomic_store_explicit(&injected_direction, 0, memory_order_release);
    atomic_store_explicit(&local_input_reroute_log_count, 0, memory_order_release);
    atomic_store_explicit(&local_touch_buttons, 0, memory_order_release);
    atomic_store_explicit(&local_touch_button_pulses, 0, memory_order_release);
    atomic_store_explicit(&local_touch_direction, 0, memory_order_release);
    atomic_store_explicit(&fight_state_tick, 0, memory_order_release);
    atomic_store_explicit(&remote_time_passed_bits, float_bits(0.0f), memory_order_release);
    atomic_store_explicit(&remote_fight_state_received_at_ms, 0, memory_order_release);
    atomic_store_explicit(&pacing_hold_count, 0, memory_order_release);
    atomic_store_explicit(&remote_authority_received_at_ms, 0, memory_order_release);
    atomic_store_explicit(&spatial_calibration_generation, 0, memory_order_release);
    atomic_store_explicit(&spatial_write_log_count, 0, memory_order_release);
    atomic_store_explicit(&spatial_hook_log_count, 0, memory_order_release);
    atomic_store_explicit(&spatial_rejection_log_count, 0, memory_order_release);
    atomic_store_explicit(&facing_write_log_count, 0, memory_order_release);
    atomic_store_explicit(&combat_write_log_count, 0, memory_order_release);
    atomic_store_explicit(&suppressed_client_damage_count, 0, memory_order_release);
    for (size_t i = 0; i < sizeof(fight_state_bits) / sizeof(fight_state_bits[0]); i++) {
        atomic_store_explicit(&fight_state_bits[i], float_bits(0.0f), memory_order_relaxed);
    }
    for (size_t i = 0; i < 2; i++) {
        atomic_store_explicit(&fight_authority_bits[i], float_bits(0.0f), memory_order_relaxed);
        atomic_store_explicit(&fight_authority_rounds[i], 0, memory_order_relaxed);
        atomic_store_explicit(&remote_authority_damage_bits[i], float_bits(0.0f), memory_order_relaxed);
        atomic_store_explicit(&remote_authority_rounds[i], 0, memory_order_relaxed);
    }
    for (size_t i = 0; i < 6; i++) {
        atomic_store_explicit(
            &remote_authority_position_bits[i], float_bits(0.0f), memory_order_relaxed);
    }
    atomic_store_explicit(&suppressed_remote_ai_log_count, 0, memory_order_release);
    atomic_store_explicit(
        &remote_ai_initialization_forwarded, false, memory_order_release);
    atomic_store_explicit(
        &suppressed_remote_ai_input_count, 0, memory_order_release);
    atomic_store_explicit(
        &lan_local_fighter, lan_configured ? (uintptr_t)player : 0,
        memory_order_release);
    atomic_store_explicit(
        &lan_remote_fighter, lan_configured ? (uintptr_t)opponent : 0,
        memory_order_release);
    atomic_store_explicit(
        &lan_active_scene, lan_configured ? (uintptr_t)self : 0,
        memory_order_release);
    atomic_store_explicit(
        &lan_active_fight, lan_configured ? (uintptr_t)fight : 0,
        memory_order_release);
    if (!lan_configured) {
        atomic_store_explicit(&lan_remote_profile, 0, memory_order_release);
    }
    atomic_store_explicit(
        &lan_fight_active, lan_configured,
        memory_order_release);
    if (lan_configured) {
        int64_t created_at = now_ms();
        atomic_store_explicit(&fight_created_at_ms, created_at, memory_order_release);
        atomic_store_explicit(&fight_start_at_ms, 0, memory_order_release);
        atomic_store_explicit(&fight_start_logged, false, memory_order_release);
        atomic_store_explicit(&fight_wait_timeout_logged, false, memory_order_release);
        atomic_store_explicit(&fight_start_method_invoked, false, memory_order_release);
        atomic_store_explicit(&fight_wakeup_input_sent, false, memory_order_release);
        uint32_t generation = atomic_fetch_add_explicit(
            &lan_fight_generation, 1, memory_order_acq_rel) + 1;
        if (generation == 0) {
            atomic_store_explicit(&lan_fight_generation, 1, memory_order_release);
            generation = 1;
        }
        LOGI("Arena LAN carregada: generation=%u; aguardando START.", generation);
    } else {
        atomic_store_explicit(&fight_created_at_ms, 0, memory_order_release);
        atomic_store_explicit(&fight_start_at_ms, 0, memory_order_release);
        atomic_store_explicit(&fight_start_method_invoked, false, memory_order_release);
        atomic_store_explicit(&fight_wakeup_input_sent, false, memory_order_release);
    }
    LOGI(
        "FightScene.CreateFight #%u: scene=%p fight=%p player=%p opponent=%p config=%p profiles=%p/%p.",
        count, self, fight, player, opponent, effective_config, player_profile, opponents);
    log_profile_equipment("jogador-na-luta", fight_player_profile);
    log_profile_equipment("adversario-na-luta", fight_opponent_profile);
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

static bool should_hold_lan_fight(void) {
    if (!atomic_load_explicit(&lan_fight_active, memory_order_acquire) ||
        !atomic_load_explicit(&session_ready, memory_order_acquire)) return false;

    int64_t now = now_ms();
    int64_t start_at = atomic_load_explicit(&fight_start_at_ms, memory_order_acquire);
    if (start_at > 0) {
        if (now < start_at) return true;
        bool was_logged = atomic_exchange_explicit(
            &fight_start_logged, true, memory_order_acq_rel);
        if (!was_logged) {
            LOGI("Simulacao LAN liberada no START: atraso=%lldms.",
                (long long)(now - start_at));
        }
    } else {
        int64_t created_at = atomic_load_explicit(
            &fight_created_at_ms, memory_order_acquire);
        if (created_at > 0 && now - created_at <= FIGHT_START_WAIT_TIMEOUT_MS) {
            return true;
        }
        bool timeout_logged = atomic_exchange_explicit(
            &fight_wait_timeout_logged, true, memory_order_acq_rel);
        if (!timeout_logged) {
            LOGE("START nao chegou em %dms; simulacao liberada por seguranca.",
                FIGHT_START_WAIT_TIMEOUT_MS);
        }
        return false;
    }

    // O pacing compara quadros de uma simulacao que ja esta em andamento.
    // Antes de StartFight, os snapshots de preparacao podem incrementar o tick
    // enquanto framesPassed continua em zero; trata-los como atraso remoto
    // impediria justamente a primeira chamada que inicia a luta.
    if (!atomic_load_explicit(
        &fight_start_method_invoked, memory_order_acquire)) return false;

    int64_t remote_received = atomic_load_explicit(
        &remote_fight_state_received_at_ms, memory_order_acquire);
    uint32_t local_tick = atomic_load_explicit(&fight_state_tick, memory_order_acquire);
    if (local_tick > 0 && remote_received > 0 && now >= remote_received &&
        now - remote_received <= REMOTE_FIGHT_STATE_FRESH_MS) {
        float local_passed = bits_float(atomic_load_explicit(
            &fight_state_bits[6], memory_order_relaxed));
        float remote_passed = bits_float(atomic_load_explicit(
            &remote_time_passed_bits, memory_order_relaxed));
        if (local_passed > remote_passed + FRAME_PACING_MAX_LEAD) {
            uint32_t holds = atomic_fetch_add_explicit(
                &pacing_hold_count, 1, memory_order_relaxed) + 1;
            if (holds == 1 || holds % 120u == 0u) {
                LOGI("Frame pacing LAN: hold=%u local=%.0f remoto=%.0f lead=%.0f.",
                    holds, local_passed, remote_passed, local_passed - remote_passed);
            }
            return true;
        }
    }
    return false;
}

static bool input_is_fresh(void) {
    if (!atomic_load_explicit(&session_ready, memory_order_acquire)) return false;
    int64_t received = atomic_load_explicit(&received_at_ms, memory_order_acquire);
    int64_t age = now_ms() - received;
    return received > 0 && age >= 0 && age <= INPUT_FRESH_MS;
}

static int32_t remote_direction(void) {
    if (!input_is_fresh()) return 0; // QuadrantZero
    float horizontal = bits_float(atomic_load_explicit(&horizontal_bits, memory_order_relaxed));
    float vertical = bits_float(atomic_load_explicit(&vertical_bits, memory_order_relaxed));
    bool left = horizontal <= -AXIS_THRESHOLD;
    bool right = horizontal >= AXIS_THRESHOLD;
    bool down = vertical <= -AXIS_THRESHOLD;
    bool up = vertical >= AXIS_THRESHOLD;

    if (up && right) return 2;  // QuadrantUpForward
    if (right && down) return 4; // QuadrantDownForward
    if (down && left) return 6;  // QuadrantDownBack
    if (up && left) return 8;    // QuadrantUpBack
    if (up) return 1;
    if (right) return 3;
    if (down) return 5;
    if (left) return 7;
    return 0;
}

static void *resolve_touch_input_class(void) {
    if (touch_input_class != NULL) return touch_input_class;
    if (touch_input_class_lookup_attempted) return NULL;
    touch_input_class_lookup_attempted = true;

    if (il2cpp_domain_get == NULL || il2cpp_domain_get_assemblies == NULL ||
        il2cpp_assembly_get_image == NULL || il2cpp_class_from_name == NULL) {
        LOGE("APIs IL2CPP insuficientes para localizar ICEJFAAGIAG.");
        return NULL;
    }

    touch_input_class = resolve_managed_class("ICEJFAAGIAG");
    if (touch_input_class != NULL) {
        LOGI("Classe ICEJFAAGIAG localizada via API IL2CPP: %p.", touch_input_class);
        return touch_input_class;
    }

    LOGE("Classe ICEJFAAGIAG nao encontrada.");
    return NULL;
}

static bool dispatch_local_control_as_player_two(
    void *fight,
    void *source_input,
    bool pressed
) {
    if (fight == NULL || source_input == NULL || il2cpp_object_new == NULL ||
        touch_input_ctor == NULL || original_fight_input_activate == NULL ||
        original_fight_input_deactivate == NULL) return false;

    void *klass = resolve_touch_input_class();
    if (klass == NULL) return false;
    void *input = il2cpp_object_new(klass);
    if (input == NULL) return false;
    int32_t control = *(int32_t *)((uint8_t *)source_input + 0x10u);
    touch_input_ctor(input, control, PLAYER_TWO_INDEX, NULL);
    log_fight_input_source(fight, input, pressed);
    if (pressed) {
        original_fight_input_activate(fight, input, NULL);
    } else {
        original_fight_input_deactivate(fight, input, NULL);
    }

    uint32_t number = atomic_fetch_add_explicit(
        &local_input_reroute_log_count, 1, memory_order_relaxed) + 1;
    if (number <= 20) {
        LOGI(
            "Controle local do jogador 2 redirecionado #%u: control=%d pressed=%d index=%d.",
            number, control, pressed ? 1 : 0, PLAYER_TWO_INDEX);
    } else if (number == 21) {
        LOGI("Controle local do jogador 2: limite diagnostico de 20 eventos atingido.");
    }
    return true;
}

static bool dispatch_remote_control(void *fight, int32_t control, bool pressed) {
    if (fight == NULL || il2cpp_object_new == NULL || touch_input_ctor == NULL ||
        original_fight_input_activate == NULL || original_fight_input_deactivate == NULL) return false;

    void *klass = resolve_touch_input_class();
    if (klass == NULL) return false;
    void *input = il2cpp_object_new(klass);
    if (input == NULL) return false;

    int32_t role = atomic_load_explicit(&lan_player_role, memory_order_acquire);
    int32_t target_index = role == 2 ? PLAYER_ONE_INDEX : PLAYER_TWO_INDEX;
    touch_input_ctor(input, control, target_index, NULL);
    dispatching_remote_control = true;
    log_fight_input_source(fight, input, pressed);
    if (pressed) {
        original_fight_input_activate(fight, input, NULL);
    } else {
        original_fight_input_deactivate(fight, input, NULL);
    }
    dispatching_remote_control = false;

    uint32_t number = atomic_fetch_add_explicit(
        &injected_input_log_count, 1, memory_order_relaxed) + 1;
    if (number <= 40) {
        LOGI("Comando LAN #%u: control=%d pressed=%d index=%d input=%p fight=%p.",
            number, control, pressed ? 1 : 0, target_index, input, fight);
    } else if (number == 41) {
        LOGI("Comando LAN: limite diagnostico de 40 eventos atingido.");
    }
    return true;
}

static bool dispatch_lan_wakeup_input(void *fight) {
    if (fight == NULL || il2cpp_object_new == NULL || touch_input_ctor == NULL ||
        original_fight_input_activate == NULL ||
        original_fight_input_deactivate == NULL) return false;
    void *klass = resolve_touch_input_class();
    if (klass == NULL) return false;
    void *input = il2cpp_object_new(klass);
    if (input == NULL) return false;
    touch_input_ctor(input, 9, PLAYER_ONE_INDEX, NULL);
    original_fight_input_activate(fight, input, NULL);
    original_fight_input_deactivate(fight, input, NULL);
    LOGI("Pulso humano inicial enviado ao Player 1 para liberar a entrada da luta.");
    return true;
}

static int32_t button_control(uint32_t mask) {
    switch (mask) {
        case BUTTON_PUNCH: return 9;
        case BUTTON_KICK: return 10;
        case BUTTON_RANGED: return 11;
        case BUTTON_MAGIC: return 12;
        case BUTTON_BOSS: return 14;
        default: return -1;
    }
}

static void inject_remote_input(void *fight) {
    int32_t previous_direction = atomic_load_explicit(
        &injected_direction, memory_order_acquire);
    int32_t direction = remote_direction();
    if (direction != previous_direction) {
        if (previous_direction != 0) {
            dispatch_remote_control(fight, previous_direction, false);
        }
        if (direction != 0) {
            dispatch_remote_control(fight, direction, true);
        }
        atomic_store_explicit(&injected_direction, direction, memory_order_release);
    }

    uint32_t previous_buttons = atomic_load_explicit(
        &injected_buttons, memory_order_acquire);
    uint32_t current_buttons = input_is_fresh()
        ? atomic_load_explicit(&buttons, memory_order_relaxed) & ~BUTTON_PAUSE
        : 0;
    uint32_t changed = previous_buttons ^ current_buttons;
    const uint32_t masks[] = {
        BUTTON_PUNCH, BUTTON_KICK, BUTTON_RANGED, BUTTON_MAGIC, BUTTON_BOSS
    };
    for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
        uint32_t mask = masks[i];
        if ((changed & mask) == 0) continue;
        int32_t control = button_control(mask);
        if (control >= 0) {
            dispatch_remote_control(fight, control, (current_buttons & mask) != 0);
        }
    }
    atomic_store_explicit(&injected_buttons, current_buttons, memory_order_release);
}

static float read_life_bar_value(void *viewer, uintptr_t model_offset, uintptr_t value_offset) {
    if (viewer == NULL) return 0.0f;
    void *screen_model = *(void **)((uint8_t *)viewer + model_offset);
    void *life_bar = screen_model == NULL ? NULL
        : *(void **)((uint8_t *)screen_model + SCREEN_MODEL_LIFE_BAR_OFFSET);
    return life_bar == NULL ? 0.0f
        : *(float *)((uint8_t *)life_bar + value_offset);
}

static bool read_fighter_position(void *fighter, Vector3Value *position) {
    if (fighter == NULL || position == NULL || fighter_position_get == NULL) return false;
    void *position_object = fighter_position_get(fighter, NULL);
    if (position_object == NULL) return false;
    position->x = *(float *)((uint8_t *)position_object + 0x10u);
    position->y = *(float *)((uint8_t *)position_object + 0x14u);
    position->z = *(float *)((uint8_t *)position_object + 0x18u);
    return isfinite(position->x) && isfinite(position->y) && isfinite(position->z) &&
        fabsf(position->x) <= 1000000.0f && fabsf(position->y) <= 1000000.0f &&
        fabsf(position->z) <= 1000000.0f;
}

static bool set_fighter_position(void *fighter, Vector3Value position) {
    if (fighter == NULL || fighter_position_get == NULL || fighter_position_set == NULL ||
        !isfinite(position.x) || !isfinite(position.y) || !isfinite(position.z)) return false;
    void *position_object = fighter_position_get(fighter, NULL);
    if (position_object == NULL) return false;
    fighter_position_set(position_object, position.x, position.y, position.z, NULL);
    // A rotina IL2CPP acima mantem os efeitos internos do tipo, mas nesta
    // versao ela nao persiste necessariamente o valor usado pela fisica. O
    // vetor retornado por BCJBEELDLAB e individual e seus tres floats foram
    // validados pela telemetria; grava-los diretamente torna a autoridade
    // espacial observavel no frame seguinte.
    *(float *)((uint8_t *)position_object + 0x10u) = position.x;
    *(float *)((uint8_t *)position_object + 0x14u) = position.y;
    *(float *)((uint8_t *)position_object + 0x18u) = position.z;
    uint32_t log_index = atomic_fetch_add_explicit(
        &spatial_write_log_count, 1, memory_order_relaxed);
    if (log_index < 4) {
        LOGI(
            "Autoridade espacial escrita #%u: fighter=%p target=(%.2f,%.2f,%.2f) readback=(%.2f,%.2f,%.2f).",
            log_index + 1, fighter, position.x, position.y, position.z,
            *(float *)((uint8_t *)position_object + 0x10u),
            *(float *)((uint8_t *)position_object + 0x14u),
            *(float *)((uint8_t *)position_object + 0x18u));
    }
    return true;
}

static void sample_local_fight_state(void *scene) {
    if (scene == NULL || pre_fight_time_passed_frames == NULL ||
        pre_fight_time_total_frames == NULL || pre_fight_time_left == NULL ||
        pre_fight_time_left_frames == NULL) return;
    void *pre_fight = *(void **)((uint8_t *)scene + FIGHT_SCENE_PRE_FIGHT_OFFSET);
    void *viewer = pre_fight == NULL ? NULL
        : *(void **)((uint8_t *)pre_fight + PRE_FIGHT_VIEWER_OFFSET);
    if (viewer == NULL) return;

    const float values[] = {
        (float)pre_fight_time_left(pre_fight, NULL),
        (float)pre_fight_time_left_frames(pre_fight, NULL),
        read_life_bar_value(viewer, VIEWER_LEFT_MODEL_OFFSET, LIFE_BAR_VALUE_OFFSET),
        read_life_bar_value(viewer, VIEWER_LEFT_MODEL_OFFSET, LIFE_BAR_HIT_VALUE_OFFSET),
        read_life_bar_value(viewer, VIEWER_RIGHT_MODEL_OFFSET, LIFE_BAR_VALUE_OFFSET),
        read_life_bar_value(viewer, VIEWER_RIGHT_MODEL_OFFSET, LIFE_BAR_HIT_VALUE_OFFSET),
        (float)pre_fight_time_passed_frames(pre_fight, NULL),
        (float)pre_fight_time_total_frames(pre_fight, NULL),
    };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        atomic_store_explicit(&fight_state_bits[i], float_bits(values[i]), memory_order_relaxed);
    }
    atomic_fetch_add_explicit(&fight_state_tick, 1, memory_order_release);

    void *fight = *(void **)((uint8_t *)scene + FIGHT_SCENE_FIGHT_OFFSET);
    void *fighters[] = {
        fight == NULL ? NULL : *(void **)((uint8_t *)fight + FIGHT_PLAYER_OFFSET),
        fight == NULL ? NULL : *(void **)((uint8_t *)fight + FIGHT_OPPONENT_OFFSET),
    };
    for (size_t index = 0; index < 2; index++) {
        void *fighter = fighters[index];
        if (fighter == NULL) continue;
        float damage = fighter_damage_get(fighter, NULL);
        int32_t rounds_lost = fighter_rounds_lost_get(fighter, NULL);
        if (damage >= 0.0f && damage <= 1.0f && rounds_lost >= 0 && rounds_lost <= 2) {
            atomic_store_explicit(
                &fight_authority_bits[index], float_bits(damage), memory_order_relaxed);
            atomic_store_explicit(
                &fight_authority_rounds[index], rounds_lost, memory_order_relaxed);
        }
        // BCJBEELDLAB retorna o vetor fisico do esqueleto do Fighter. Ao
        // contrario dos Components Unity em +0x450/+0x458, este objeto e
        // individual e participa diretamente dos calculos de colisao.
        Vector3Value position;
        if (read_fighter_position(fighter, &position)) {
            const float coordinates[] = {position.x, position.y, position.z};
            for (size_t axis = 0; axis < 3; axis++) {
                atomic_store_explicit(
                    &fight_authority_position_bits[index * 3 + axis],
                    float_bits(coordinates[axis]), memory_order_relaxed);
            }
        }
    }
}

static bool remote_authority_is_fresh(void) {
    int64_t received = atomic_load_explicit(
        &remote_authority_received_at_ms, memory_order_acquire);
    int64_t age = now_ms() - received;
    return received > 0 && age >= 0 && age <= REMOTE_FIGHT_STATE_FRESH_MS;
}

static bool prepare_remote_fight_authority(
    void *fight,
    void *fighters[2],
    Vector3Value targets[2]
) {
    int32_t role = atomic_load_explicit(&lan_player_role, memory_order_acquire);
    int64_t received = atomic_load_explicit(
        &remote_authority_received_at_ms, memory_order_acquire);
    int64_t age = now_ms() - received;
    if (fight == NULL || role != 2 || !remote_authority_is_fresh()) {
        uint32_t log_index = atomic_fetch_add_explicit(
            &spatial_rejection_log_count, 1, memory_order_relaxed);
        if (log_index < 4) {
            LOGI(
                "Autoridade espacial ignorada #%u: fight=%p role=%d received=%lld age=%lldms.",
                log_index + 1, fight, role, (long long)received, (long long)age);
        }
        return false;
    }

    // O jogador 1 define a composicao canonica da arena: jogador 1 sempre a
    // esquerda (indice 0) e jogador 2 sempre a direita (indice 1), inclusive
    // no segundo aparelho. A troca dos comandos no jogador 2 preserva essa
    // mesma ordem de Fighter nos dois processos.
    fighters[0] = *(void **)((uint8_t *)fight + FIGHT_PLAYER_OFFSET);
    fighters[1] = *(void **)((uint8_t *)fight + FIGHT_OPPONENT_OFFSET);
    const size_t authority_index[] = {0, 1};
    Vector3Value local_positions[2];
    Vector3Value remote_positions[2];
    for (size_t local_index = 0; local_index < 2; local_index++) {
        void *fighter = fighters[local_index];
        if (!read_fighter_position(fighter, &local_positions[local_index])) {
            uint32_t log_index = atomic_fetch_add_explicit(
                &spatial_rejection_log_count, 1, memory_order_relaxed);
            if (log_index < 8) {
                LOGI(
                    "Autoridade espacial sem posicao #%u: indice=%u fighter=%p.",
                    log_index + 1, (unsigned)local_index, fighter);
            }
            return false;
        }
        size_t source = authority_index[local_index];
        remote_positions[local_index].x = bits_float(atomic_load_explicit(
            &remote_authority_position_bits[source * 3], memory_order_relaxed));
        remote_positions[local_index].y = bits_float(atomic_load_explicit(
            &remote_authority_position_bits[source * 3 + 1], memory_order_relaxed));
        remote_positions[local_index].z = bits_float(atomic_load_explicit(
            &remote_authority_position_bits[source * 3 + 2], memory_order_relaxed));
        if (!isfinite(remote_positions[local_index].x) ||
            !isfinite(remote_positions[local_index].y) ||
            !isfinite(remote_positions[local_index].z)) return false;
    }

    uint32_t generation = atomic_load_explicit(
        &lan_fight_generation, memory_order_acquire);
    if (atomic_load_explicit(
        &spatial_calibration_generation, memory_order_acquire) != generation) {
        for (size_t local_index = 0; local_index < 2; local_index++) {
            const float remote[] = {
                remote_positions[local_index].x,
                remote_positions[local_index].y,
                remote_positions[local_index].z,
            };
            const float visual_anchor[] = {
                local_positions[local_index].x,
                local_positions[local_index].y,
                local_positions[local_index].z,
            };
            for (size_t axis = 0; axis < 3; axis++) {
                spatial_anchor_local[local_index * 3 + axis] = visual_anchor[axis];
                spatial_anchor_remote[local_index * 3 + axis] = remote[axis];
            }
        }
        atomic_store_explicit(
            &spatial_calibration_generation, generation, memory_order_release);
        LOGI(
            "Sincronismo espacial calibrado: geracao=%u localP=(%.2f,%.2f,%.2f) localO=(%.2f,%.2f,%.2f).",
            generation,
            local_positions[0].x, local_positions[0].y, local_positions[0].z,
            local_positions[1].x, local_positions[1].y, local_positions[1].z);
        return false;
    }

    for (size_t local_index = 0; local_index < 2; local_index++) {
        size_t base = local_index * 3;
        targets[local_index] = (Vector3Value){
            spatial_anchor_local[base] +
                (remote_positions[local_index].x - spatial_anchor_remote[base]),
            spatial_anchor_local[base + 1] +
                (remote_positions[local_index].y - spatial_anchor_remote[base + 1]),
            spatial_anchor_local[base + 2] +
                (remote_positions[local_index].z - spatial_anchor_remote[base + 2]),
        };
    }
    return true;
}

static void *fighter_root_position_node(void *fighter) {
    if (fighter == NULL) return NULL;
    void *model = *(void **)((uint8_t *)fighter + FIGHTER_POSITION_MODEL_OFFSET);
    return model == NULL ? NULL
        : *(void **)((uint8_t *)model + POSITION_MODEL_ROOT_NODE_OFFSET);
}

static void apply_remote_combat_authority(void *fighters[2]) {
    if (!remote_authority_is_fresh() || fighter_damage_get == NULL ||
        fighter_damage_set == NULL || fighter_rounds_lost_get == NULL ||
        fighter_resolve_result == NULL || original_fighter_apply_damage == NULL) return;

    const size_t authority_index[] = {0, 1};
    for (size_t local_index = 0; local_index < 2; local_index++) {
        void *fighter = fighters[local_index];
        if (fighter == NULL) continue;
        size_t source = authority_index[local_index];
        float target_damage = bits_float(atomic_load_explicit(
            &remote_authority_damage_bits[source], memory_order_relaxed));
        int32_t target_rounds = atomic_load_explicit(
            &remote_authority_rounds[source], memory_order_relaxed);
        int32_t current_rounds = fighter_rounds_lost_get(fighter, NULL);
        if (!isfinite(target_damage) || target_damage < 0.0f || target_damage > 1.0f ||
            target_rounds < 0 || target_rounds > 2 ||
            current_rounds < 0 || current_rounds > 2) continue;

        const char *action = NULL;
        float current_damage = fighter_damage_get(fighter, NULL);
        applying_remote_damage = true;
        if (target_rounds > current_rounds) {
            // IMDINIMKHOP incrementa o contador de rounds quando o dano chega
            // a 1, zera a barra e publica os mesmos eventos usados pelo jogo.
            fighter_damage_set(fighter, 1.0f, NULL);
            fighter_resolve_result(fighter, NULL);
            action = "round";
        } else if (target_rounds == current_rounds && isfinite(current_damage)) {
            if (target_damage > current_damage + 0.0005f) {
                original_fighter_apply_damage(
                    fighter, target_damage - current_damage, NULL);
                action = "damage+";
            } else if (target_damage < current_damage - 0.0005f) {
                fighter_damage_set(fighter, target_damage, NULL);
                action = "damage=";
            }
        }
        applying_remote_damage = false;

        if (action != NULL) {
            uint32_t log_index = atomic_fetch_add_explicit(
                &combat_write_log_count, 1, memory_order_relaxed);
            if (log_index < 12) {
                LOGI(
                    "Autoridade de combate #%u: fighter=%p acao=%s dano=%.4f rounds=%d.",
                    log_index + 1, fighter, action, target_damage, target_rounds);
            }
        }
    }
}

static void hooked_node_position_copy(
    void *node,
    void *source_position,
    const void *method
) {
    original_node_position_copy(node, source_position, method);
    if (node == NULL || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) return;

    void *fight = (void *)atomic_load_explicit(
        &lan_active_fight, memory_order_acquire);
    if (fight == NULL) return;
    void *player = *(void **)((uint8_t *)fight + FIGHT_PLAYER_OFFSET);
    void *opponent = *(void **)((uint8_t *)fight + FIGHT_OPPONENT_OFFSET);
    if (node != fighter_root_position_node(player) &&
        node != fighter_root_position_node(opponent)) return;

    if (atomic_load_explicit(&lan_player_role, memory_order_acquire) != 2) {
        return;
    }

    void *authority_fighters[2] = {NULL, NULL};
    Vector3Value authority_targets[2];
    if (!prepare_remote_fight_authority(
        fight, authority_fighters, authority_targets)) return;

    // LODMPBBLOPJ acabou de copiar a posicao calculada para o no apresentado.
    // Sobrescrever os dois vetores neste ponto faz a renderizacao observar a
    // autoridade antes de devolver o controle ao atualizador visual.
    set_fighter_position(authority_fighters[0], authority_targets[0]);
    set_fighter_position(authority_fighters[1], authority_targets[1]);
    apply_remote_combat_authority(authority_fighters);
    uint32_t hook_log_index = atomic_fetch_add_explicit(
        &spatial_hook_log_count, 1, memory_order_relaxed);
    if (hook_log_index < 4) {
        LOGI(
            "No espacial autoritativo #%u: fight=%p node=%p.",
            hook_log_index + 1, fight, node);
    }
}

static void hooked_fighter_apply_damage(
    void *fighter,
    float damage,
    const void *method
) {
    if (!applying_remote_damage && atomic_load_explicit(
        &lan_fight_active, memory_order_acquire) && atomic_load_explicit(
        &lan_player_role, memory_order_acquire) == 2) {
        uint32_t log_index = atomic_fetch_add_explicit(
            &suppressed_client_damage_count, 1, memory_order_relaxed);
        if (log_index < 6) {
            LOGI(
                "Dano local do cliente suprimido #%u: fighter=%p dano=%.4f.",
                log_index + 1, fighter, damage);
        }
        return;
    }
    original_fighter_apply_damage(fighter, damage, method);
}

#if SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
static void trace_all_fighter_floats(void *fight) {
    if (fight == NULL) return;
    uint32_t tick = atomic_load_explicit(&fight_state_tick, memory_order_acquire);
    if (tick % 15u != 0u) return;

    uint32_t generation = atomic_load_explicit(
        &lan_fight_generation, memory_order_acquire);
    if (generation != diagnostic_float_generation) {
        diagnostic_float_generation = generation;
        diagnostic_previous_float_valid[0] = false;
        diagnostic_previous_float_valid[1] = false;
        diagnostic_previous_fight_scalars_valid = false;
        diagnostic_previous_hud_state_valid = false;
    }

    void *fighters[] = {
        *(void **)((uint8_t *)fight + FIGHT_PLAYER_OFFSET),
        *(void **)((uint8_t *)fight + FIGHT_OPPONENT_OFFSET),
    };
    for (size_t fighter_index = 0; fighter_index < 2; fighter_index++) {
        void *fighter = fighters[fighter_index];
        if (fighter == NULL) continue;
        float values[DIAGNOSTIC_FIGHTER_FLOAT_COUNT];
        uint32_t bits[DIAGNOSTIC_FIGHTER_FLOAT_COUNT];
        bool changed = !diagnostic_previous_float_valid[fighter_index];
        for (size_t index = 0; index < DIAGNOSTIC_FIGHTER_FLOAT_COUNT; index++) {
            values[index] = diagnostic_fighter_float_readers[index](fighter, NULL);
            bits[index] = float_bits(values[index]);
            changed = changed || bits[index] != diagnostic_previous_float_bits[fighter_index][index];
        }
        if (!changed) continue;
        memcpy(diagnostic_previous_float_bits[fighter_index], bits, sizeof(bits));
        diagnostic_previous_float_valid[fighter_index] = true;
        LOGI(
            "FLOAT16 %c tick=%u: %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g %.4g",
            fighter_index == 0 ? 'P' : 'O', tick,
            values[0], values[1], values[2], values[3],
            values[4], values[5], values[6], values[7],
            values[8], values[9], values[10], values[11],
            values[12], values[13], values[14], values[15]);
    }
}

static void trace_fight_scalars(void *fight) {
    if (fight == NULL) return;
    uint32_t tick = atomic_load_explicit(&fight_state_tick, memory_order_acquire);
    if (tick % 15u != 0u) return;

    uint8_t *base = (uint8_t *)fight;
    struct diagnostic_fight_scalars current = {
        .value_118 = *(uint32_t *)(base + 0x118u),
        .value_11c = *(uint32_t *)(base + 0x11cu),
        .value_120 = *(uint64_t *)(base + 0x120u),
        .value_128 = *(int32_t *)(base + 0x128u),
        .value_130 = *(int32_t *)(base + 0x130u),
        .bits_134 = *(uint32_t *)(base + 0x134u),
        .value_13c = *(int32_t *)(base + 0x13cu),
        .bits_144 = *(uint32_t *)(base + 0x144u),
        .value_17c = *(int32_t *)(base + 0x17cu),
    };
    const uintptr_t flag_offsets[] = {
        0x12cu, 0x12du, 0x12eu, 0x138u, 0x140u, 0x141u,
        0x142u, 0x148u, 0x149u, 0x178u, 0x179u, 0x17au, 0x180u,
    };
    for (size_t index = 0; index < sizeof(flag_offsets) / sizeof(flag_offsets[0]); index++) {
        if (*(bool *)(base + flag_offsets[index])) current.flags |= (uint16_t)(1u << index);
    }
    if (diagnostic_previous_fight_scalars_valid &&
        memcmp(&current, &diagnostic_previous_fight_scalars, sizeof(current)) == 0) return;

    diagnostic_previous_fight_scalars = current;
    diagnostic_previous_fight_scalars_valid = true;
    LOGI(
        "FIGHTSCALAR tick=%u: u118=%u u11c=%u l120=%llu i128=%d i130=%d f134=%.4g i13c=%d f144=%.4g i17c=%d flags=0x%04x",
        tick, current.value_118, current.value_11c,
        (unsigned long long)current.value_120, current.value_128,
        current.value_130, bits_float(current.bits_134), current.value_13c,
        bits_float(current.bits_144), current.value_17c, current.flags);
}

static void trace_hud_state(void *scene) {
    if (scene == NULL || pre_fight_time_left == NULL ||
        pre_fight_time_left_frames == NULL) return;
    uint32_t tick = atomic_load_explicit(&fight_state_tick, memory_order_acquire);
    if (tick % 15u != 0u) return;

    void *pre_fight = *(void **)((uint8_t *)scene + FIGHT_SCENE_PRE_FIGHT_OFFSET);
    if (pre_fight == NULL) return;
    void *viewer = *(void **)((uint8_t *)pre_fight + PRE_FIGHT_VIEWER_OFFSET);
    if (viewer == NULL) return;

    float left_life = read_life_bar_value(
        viewer, VIEWER_LEFT_MODEL_OFFSET, LIFE_BAR_VALUE_OFFSET);
    float left_hit = read_life_bar_value(
        viewer, VIEWER_LEFT_MODEL_OFFSET, LIFE_BAR_HIT_VALUE_OFFSET);
    float right_life = read_life_bar_value(
        viewer, VIEWER_RIGHT_MODEL_OFFSET, LIFE_BAR_VALUE_OFFSET);
    float right_hit = read_life_bar_value(
        viewer, VIEWER_RIGHT_MODEL_OFFSET, LIFE_BAR_HIT_VALUE_OFFSET);
    struct diagnostic_hud_state current = {
        .time_left = pre_fight_time_left(pre_fight, NULL),
        .time_left_frames = pre_fight_time_left_frames(pre_fight, NULL),
        .left_life_bits = float_bits(left_life),
        .left_hit_bits = float_bits(left_hit),
        .right_life_bits = float_bits(right_life),
        .right_hit_bits = float_bits(right_hit),
    };
    if (diagnostic_previous_hud_state_valid &&
        memcmp(&current, &diagnostic_previous_hud_state, sizeof(current)) == 0) return;

    diagnostic_previous_hud_state = current;
    diagnostic_previous_hud_state_valid = true;
    LOGI(
        "HUDSTATE tick=%u: time=%d frames=%d left=%.4g hit=%.4g right=%.4g hit=%.4g",
        tick, current.time_left, current.time_left_frames,
        left_life, left_hit, right_life, right_hit);
}
#endif

static void hooked_fight_scene_fixed_update(void *self, const void *method) {
    void *fight = self == NULL ? NULL
        : *(void **)((uint8_t *)self + FIGHT_SCENE_FIGHT_OFFSET);
    bool lan_active = fight != NULL && atomic_load_explicit(
        &lan_fight_active, memory_order_acquire);
    if (lan_active) {
        if (should_hold_lan_fight()) return;
        if (!atomic_load_explicit(
            &fight_start_method_invoked, memory_order_acquire)) {
            hooked_fight_scene_start_fight(self, NULL);
        }
        // Mantem o fluxo estavel: snapshot e entrada antes da simulacao. A
        // autoridade espacial e aplicada no getter fisico do lutador.
        sample_local_fight_state(self);
        inject_remote_input(fight);
    }

    original_fight_scene_fixed_update(self, method);
}

static void hooked_fight_fixed_update(void *fight, const void *method) {
    // Mantem chamadas diretas externas intactas. A cena LAN usa explicitamente
    // o trampoline acima para poder executar trabalho depois da simulacao.
    original_fight_fixed_update(fight, method);
}

static void hooked_fight_simulation_step(void *fight, const void *method) {
    original_fight_simulation_step(fight, method);

    if (fight != NULL && atomic_load_explicit(&lan_fight_active, memory_order_acquire)) {
#if SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
        trace_all_fighter_floats(fight);
        trace_fight_scalars(fight);
#endif
    }
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
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        bool result = original_get_key(self, index, key, method);
        if (index == PLAYER_ONE_INDEX) capture_local_button(key, result);
        return result;
    }
    log_player_two_input_once();
    uint32_t mask = key_to_button(key);
    return input_is_fresh() && mask && (atomic_load_explicit(&buttons, memory_order_relaxed) & mask);
}

static bool hooked_get_key_down(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        bool result = original_get_key_down(self, index, key, method);
        if (index == PLAYER_ONE_INDEX && result) capture_local_button(key, true);
        return result;
    }
    log_player_two_input_once();
    return input_is_fresh() && consume_latch(&key_down_latch, key_to_button(key));
}

static bool hooked_get_key_up(void *self, int32_t index, int32_t key, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        bool result = original_get_key_up(self, index, key, method);
        if (index == PLAYER_ONE_INDEX && result) capture_local_button(key, false);
        return result;
    }
    log_player_two_input_once();
    return consume_latch(&key_up_latch, key_to_button(key));
}

static float hooked_get_axis_value(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        float result = original_get_axis_value(self, index, axis, method);
        if (index == PLAYER_ONE_INDEX) capture_local_axis(axis, result);
        return result;
    }
    log_player_two_input_once();
    return input_is_fresh() ? remote_axis_value(axis) : 0.0f;
}

static bool hooked_get_axis(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        return original_get_axis(self, index, axis, method);
    }
    log_player_two_input_once();
    float value = input_is_fresh() ? remote_axis_value(axis) : 0.0f;
    return value >= AXIS_THRESHOLD || value <= -AXIS_THRESHOLD;
}

static bool hooked_get_axis_down(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        return original_get_axis_down(self, index, axis, method);
    }
    log_player_two_input_once();
    return input_is_fresh() && consume_latch(&axis_down_latch, axis_mask(axis));
}

static bool hooked_get_axis_up(void *self, int32_t index, int32_t axis, const void *method) {
    if (index != PLAYER_TWO_INDEX || !atomic_load_explicit(
        &lan_fight_active, memory_order_acquire)) {
        return original_get_axis_up(self, index, axis, method);
    }
    log_player_two_input_once();
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

#if SF2_LAN_DIAGNOSTIC_TRACE_ALL_FLOATS
    const uintptr_t diagnostic_fighter_float_rvas[] = {
        0x3397400u, 0x339A6ECu, 0x339B510u, 0x339CED4u,
        0x339D8C4u, 0x339FE98u, 0x339A9B0u, 0x33A1BF8u,
        0x33A1C84u, 0x33A5408u, 0x33A5B8Cu, 0x33A67C4u,
        0x33A6C90u, 0x33A6D0Cu, 0x33A8134u, 0x33A8A7Cu,
    };
    for (size_t i = 0; i < DIAGNOSTIC_FIGHTER_FLOAT_COUNT; i++) {
        diagnostic_fighter_float_readers[i] = (FighterFloatMethod)(
            location.base + diagnostic_fighter_float_rvas[i]);
    }
#endif
    pre_fight_time_passed_frames = (IntInstanceMethod)(
        location.base + RVA_PRE_FIGHT_TIME_PASSED_FRAMES);
    pre_fight_time_total_frames = (IntInstanceMethod)(
        location.base + RVA_PRE_FIGHT_TIME_TOTAL_FRAMES);
    pre_fight_time_left = (IntInstanceMethod)(
        location.base + RVA_PRE_FIGHT_TIME_LEFT);
    pre_fight_time_left_frames = (IntInstanceMethod)(
        location.base + RVA_PRE_FIGHT_TIME_LEFT_FRAMES);
    fighter_damage_get = (FighterFloatMethod)(
        location.base + RVA_FIGHTER_DAMAGE_GET);
    fighter_damage_set = (FighterSetFloatMethod)(
        location.base + RVA_FIGHTER_DAMAGE_SET);
    fighter_resolve_result = (InstanceMethod)(
        location.base + RVA_FIGHTER_RESOLVE_RESULT);
    fighter_rounds_lost_get = (IntInstanceMethod)(
        location.base + RVA_FIGHTER_ROUNDS_LOST_GET);
    fighter_position_get = (ObjectGetterMethod)(
        location.base + RVA_FIGHTER_POSITION_GET);
    fighter_position_set = (VectorSetMethod)(
        location.base + RVA_VECTOR3_POSITION_SET);
    ai_planner_initialize = (ConfigProfileMethod)(
        location.base + RVA_AI_PLANNER_INITIALIZE);
    ai_planner_cancel = (InstanceMethod)(
        location.base + RVA_AI_PLANNER_CANCEL);

    gamepad_controller_current =
        (GetCurrentMethod)(location.base + RVA_GAMEPAD_CONTROLLER_CURRENT);
    set_gamepad_dual_mode =
        (SetBoolMethod)(location.base + RVA_SET_GAMEPAD_DUAL_MODE);
    il2cpp_object_new = (ObjectNewMethod)dlsym(RTLD_DEFAULT, "il2cpp_object_new");
    if (il2cpp_object_new == NULL) {
        il2cpp_object_new = (ObjectNewMethod)(location.base + RVA_IL2CPP_OBJECT_NEW);
    }
    il2cpp_string_new = (StringNewMethod)dlsym(RTLD_DEFAULT, "il2cpp_string_new");
    if (il2cpp_string_new == NULL) {
        il2cpp_string_new = (StringNewMethod)(location.base + RVA_IL2CPP_STRING_NEW);
    }
    il2cpp_domain_get = (DomainGetMethod)dlsym(RTLD_DEFAULT, "il2cpp_domain_get");
    il2cpp_domain_get_assemblies = (DomainGetAssembliesMethod)dlsym(
        RTLD_DEFAULT, "il2cpp_domain_get_assemblies");
    il2cpp_assembly_get_image = (AssemblyGetImageMethod)dlsym(
        RTLD_DEFAULT, "il2cpp_assembly_get_image");
    il2cpp_class_from_name = (ClassFromNameMethod)dlsym(
        RTLD_DEFAULT, "il2cpp_class_from_name");
    if (il2cpp_domain_get == NULL) {
        il2cpp_domain_get = (DomainGetMethod)(location.base + RVA_IL2CPP_DOMAIN_GET);
    }
    if (il2cpp_domain_get_assemblies == NULL) {
        il2cpp_domain_get_assemblies = (DomainGetAssembliesMethod)(
            location.base + RVA_IL2CPP_DOMAIN_GET_ASSEMBLIES);
    }
    if (il2cpp_assembly_get_image == NULL) {
        il2cpp_assembly_get_image = (AssemblyGetImageMethod)(
            location.base + RVA_IL2CPP_ASSEMBLY_GET_IMAGE);
    }
    if (il2cpp_class_from_name == NULL) {
        il2cpp_class_from_name = (ClassFromNameMethod)(
            location.base + RVA_IL2CPP_CLASS_FROM_NAME);
    }
    touch_input_ctor = (TouchInputCtorMethod)(location.base + RVA_TOUCH_INPUT_CTOR);
    fight_config_manager_current = (GetSingletonMethod)(
        location.base + RVA_FIGHT_CONFIG_MANAGER_CURRENT);
    fight_config_by_name = (GetFightConfigByNameMethod)(
        location.base + RVA_FIGHT_CONFIG_BY_NAME);
    fight_config_clear_player = (ConfigInstanceMethod)(
        location.base + RVA_FIGHT_CONFIG_CLEAR_PLAYER);
    fight_config_set_player = (ConfigProfileMethod)(
        location.base + RVA_FIGHT_CONFIG_SET_PLAYER);
    fight_config_clear_opponent = (ConfigInstanceMethod)(
        location.base + RVA_FIGHT_CONFIG_CLEAR_OPPONENT);
    fight_config_set_opponent = (ConfigProfileMethod)(
        location.base + RVA_FIGHT_CONFIG_SET_OPPONENT);
    player_profile_copy_ctor = (ProfileCopyCtorMethod)(
        location.base + RVA_PLAYER_PROFILE_COPY_CTOR);
    player_profile_set_item = (ProfileSetItemMethod)(
        location.base + RVA_PLAYER_PROFILE_SET_ITEM);
    item_by_name = (ItemByNameMethod)(location.base + RVA_ITEM_BY_NAME);
    item_manager_current = (GetSingletonMethod)(
        location.base + RVA_ITEM_MANAGER_CURRENT);
    item_manager_by_name = (ItemManagerByNameMethod)(
        location.base + RVA_ITEM_MANAGER_BY_NAME);
    il2cpp_gc_wbarrier_set_field = (GcWbarrierSetFieldMethod)dlsym(
        RTLD_DEFAULT, "il2cpp_gc_wbarrier_set_field");
    if (il2cpp_object_new == NULL || il2cpp_string_new == NULL || il2cpp_domain_get == NULL ||
        il2cpp_domain_get_assemblies == NULL || il2cpp_assembly_get_image == NULL ||
        il2cpp_class_from_name == NULL) {
        LOGE("Nao foi possivel resolver as APIs publicas de objetos e classes IL2CPP.");
        atomic_store_explicit(&hook_failed, true, memory_order_release);
        return JNI_FALSE;
    }

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
        {RVA_FIGHT_SCENE_CREATE_FIGHT, hooked_fight_scene_create_fight,
            (void **)&original_fight_scene_create_fight},
        {RVA_FIGHT_SCENE_START_FIGHT, hooked_fight_scene_start_fight,
            (void **)&original_fight_scene_start_fight},
        {RVA_FIGHT_SCENE_FIXED_UPDATE, hooked_fight_scene_fixed_update,
            (void **)&original_fight_scene_fixed_update},
        {RVA_FIGHT_FIXED_UPDATE, hooked_fight_fixed_update,
            (void **)&original_fight_fixed_update},
        {RVA_FIGHT_SIMULATION_STEP, hooked_fight_simulation_step,
            (void **)&original_fight_simulation_step},
        {RVA_REGISTER_INPUT_BINDING, hooked_register_input_binding,
            (void **)&original_register_input_binding},
        {RVA_RESOLVE_INPUT_FIGHTER, hooked_resolve_input_fighter,
            (void **)&original_resolve_input_fighter},
        {RVA_FIGHTER_AI_ANIMATION_EVENT, hooked_fighter_ai_animation_event,
            (void **)&original_fighter_ai_animation_event},
        {RVA_AI_PLANNER_SELECT_ACTION, hooked_ai_planner_select_action,
            (void **)&original_ai_planner_select_action},
        {RVA_FIGHT_CONFIG_EQUIPMENT_RULE_GET,
            hooked_fight_config_equipment_rule_get,
            (void **)&original_fight_config_equipment_rule_get},
        {RVA_RANDOM_FIGHTER_PROFILE_CREATE,
            hooked_random_fighter_profile_create,
            (void **)&original_random_fighter_profile_create},
        {RVA_CREATE_FIGHT_PROFILES,
            hooked_create_fight_profiles,
            (void **)&original_create_fight_profiles},
        {RVA_PLAYER_PROFILE_ITEM_BY_SLOT,
            hooked_player_profile_item_by_slot,
            (void **)&original_player_profile_item_by_slot},
        {RVA_PLAYER_PROFILE_ITEM_GET,
            hooked_player_profile_item_get,
            (void **)&original_player_profile_item_get},
        {RVA_PLAYER_PROFILE_SET_ITEM,
            hooked_player_profile_set_item,
            (void **)&player_profile_set_item},
        {RVA_FIGHTER_APPLY_CONFIG_ITEM,
            hooked_fighter_apply_config_item,
            (void **)&original_fighter_apply_config_item},
        {RVA_FIGHTER_APPLY_EQUIPMENT_ITEM,
            hooked_fighter_apply_equipment_item,
            (void **)&original_fighter_apply_equipment_item},
        {RVA_FIGHTER_CTOR, hooked_fighter_ctor,
            (void **)&original_fighter_ctor},
        {RVA_PRE_FIGHT_VIEWER_INIT, hooked_pre_fight_viewer_init,
            (void **)&original_pre_fight_viewer_init},
        {RVA_PRE_FIGHT_CREATE_VS, hooked_pre_fight_create_vs,
            (void **)&original_pre_fight_create_vs},
        {RVA_NODE_POSITION_COPY, hooked_node_position_copy,
            (void **)&original_node_position_copy},
        {RVA_FIGHTER_APPLY_DAMAGE, hooked_fighter_apply_damage,
            (void **)&original_fighter_apply_damage},
        {RVA_FIGHT_INPUT_PRESS, hooked_fight_input_deactivate,
            (void **)&original_fight_input_deactivate},
        {RVA_FIGHT_INPUT_RELEASE, hooked_fight_input_activate,
            (void **)&original_fight_input_activate},
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
    LOGI("Injecao LAN na FightScene e tabela de vinculos instrumentadas; base libil2cpp=%p.",
        (void *)location.base);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeSetRemoteProfile(
    JNIEnv *env,
    jclass type,
    jint level,
    jstring armor,
    jstring helm,
    jstring weapon,
    jstring ranged,
    jstring magic
) {
    (void)type;
    struct remote_fighter_profile incoming = {0};
    incoming.level = level;
    jstring values[] = {armor, helm, weapon, ranged, magic};
    char *destinations[] = {
        incoming.armor, incoming.helm, incoming.weapon,
        incoming.ranged, incoming.magic
    };
    bool valid = level >= 1 && level <= 255;
    for (size_t i = 0; valid && i < sizeof(values) / sizeof(values[0]); i++) {
        if (values[i] == NULL) {
            valid = false;
            break;
        }
        jsize length = (*env)->GetStringUTFLength(env, values[i]);
        if (length <= 0 || length > REMOTE_ITEM_ID_MAX) {
            valid = false;
            break;
        }
        const char *source = (*env)->GetStringUTFChars(env, values[i], NULL);
        if (source == NULL) {
            valid = false;
            break;
        }
        memcpy(destinations[i], source, (size_t)length);
        destinations[i][length] = '\0';
        (*env)->ReleaseStringUTFChars(env, values[i], source);
    }
    incoming.available = valid;
    pthread_mutex_lock(&remote_profile_mutex);
    remote_profile = incoming;
    pthread_mutex_unlock(&remote_profile_mutex);
    LOGI("Perfil remoto nativo: %s nivel=%d.", valid ? "carregado" : "rejeitado", level);
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeSetLocalProfile(
    JNIEnv *env,
    jclass type,
    jint level,
    jstring armor,
    jstring helm,
    jstring weapon,
    jstring ranged,
    jstring magic
) {
    (void)type;
    struct remote_fighter_profile incoming = {0};
    incoming.level = level;
    jstring values[] = {armor, helm, weapon, ranged, magic};
    char *destinations[] = {
        incoming.armor, incoming.helm, incoming.weapon,
        incoming.ranged, incoming.magic
    };
    bool valid = level >= 1 && level <= 255;
    for (size_t i = 0; valid && i < sizeof(values) / sizeof(values[0]); i++) {
        if (values[i] == NULL) {
            valid = false;
            break;
        }
        jsize length = (*env)->GetStringUTFLength(env, values[i]);
        if (length <= 0 || length > REMOTE_ITEM_ID_MAX) {
            valid = false;
            break;
        }
        const char *source = (*env)->GetStringUTFChars(env, values[i], NULL);
        if (source == NULL) {
            valid = false;
            break;
        }
        memcpy(destinations[i], source, (size_t)length);
        destinations[i][length] = '\0';
        (*env)->ReleaseStringUTFChars(env, values[i], source);
    }
    incoming.available = valid;
    pthread_mutex_lock(&local_profile_mutex);
    local_profile = incoming;
    pthread_mutex_unlock(&local_profile_mutex);
    LOGI("Perfil local nativo: %s nivel=%d.", valid ? "carregado" : "rejeitado", level);
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeClearLocalProfile(
    JNIEnv *env,
    jclass type
) {
    (void)env;
    (void)type;
    pthread_mutex_lock(&local_profile_mutex);
    memset(&local_profile, 0, sizeof(local_profile));
    pthread_mutex_unlock(&local_profile_mutex);
    LOGI("Perfil local nativo: removido.");
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeClearRemoteProfile(
    JNIEnv *env,
    jclass type
) {
    (void)env;
    (void)type;
    pthread_mutex_lock(&remote_profile_mutex);
    memset(&remote_profile, 0, sizeof(remote_profile));
    pthread_mutex_unlock(&remote_profile_mutex);
    LOGI("Perfil remoto nativo: removido.");
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeSetSessionReady(
    JNIEnv *env,
    jclass type,
    jboolean ready
) {
    (void)env;
    (void)type;
    bool new_value = ready == JNI_TRUE;
    bool previous = atomic_exchange_explicit(
        &session_ready, new_value, memory_order_acq_rel);
    if (!new_value) {
        atomic_store_explicit(&horizontal_bits, float_bits(0.0f), memory_order_release);
        atomic_store_explicit(&vertical_bits, float_bits(0.0f), memory_order_release);
        atomic_store_explicit(&buttons, 0, memory_order_release);
        atomic_store_explicit(&key_down_latch, 0, memory_order_release);
        atomic_store_explicit(&key_up_latch, 0, memory_order_release);
        atomic_store_explicit(&axis_down_latch, 0, memory_order_release);
        atomic_store_explicit(&axis_up_latch, 0, memory_order_release);
        atomic_store_explicit(&received_at_ms, 0, memory_order_release);
        atomic_store_explicit(&lan_fight_generation, 0, memory_order_release);
        atomic_store_explicit(&fight_created_at_ms, 0, memory_order_release);
        atomic_store_explicit(&fight_start_at_ms, 0, memory_order_release);
        atomic_store_explicit(&fight_start_method_invoked, false, memory_order_release);
        atomic_store_explicit(&fight_wakeup_input_sent, false, memory_order_release);
        atomic_store_explicit(&lan_fight_active, false, memory_order_release);
        atomic_store_explicit(&lan_local_fighter, 0, memory_order_release);
        atomic_store_explicit(&lan_remote_fighter, 0, memory_order_release);
        atomic_store_explicit(&lan_remote_ai_planner, 0, memory_order_release);
        atomic_store_explicit(&lan_active_scene, 0, memory_order_release);
        atomic_store_explicit(&lan_active_fight, 0, memory_order_release);
        atomic_store_explicit(&fight_state_tick, 0, memory_order_release);
        atomic_store_explicit(&remote_time_passed_bits, float_bits(0.0f), memory_order_release);
        atomic_store_explicit(&remote_fight_state_received_at_ms, 0, memory_order_release);
        atomic_store_explicit(&pacing_hold_count, 0, memory_order_release);
    atomic_store_explicit(&remote_authority_received_at_ms, 0, memory_order_release);
    atomic_store_explicit(&spatial_calibration_generation, 0, memory_order_release);
    atomic_store_explicit(&spatial_write_log_count, 0, memory_order_release);
    atomic_store_explicit(&spatial_hook_log_count, 0, memory_order_release);
    atomic_store_explicit(&spatial_rejection_log_count, 0, memory_order_release);
    atomic_store_explicit(&facing_write_log_count, 0, memory_order_release);
    atomic_store_explicit(&combat_write_log_count, 0, memory_order_release);
        atomic_store_explicit(&suppressed_client_damage_count, 0, memory_order_release);
        atomic_store_explicit(&lan_original_fight_mode, -1, memory_order_release);
        atomic_store_explicit(&lan_original_fight_fingerprint, 0, memory_order_release);
        pthread_mutex_lock(&fight_config_identity_mutex);
        lan_original_fight_identity[0] = '\0';
        pthread_mutex_unlock(&fight_config_identity_mutex);
        for (size_t i = 0; i < sizeof(fight_state_bits) / sizeof(fight_state_bits[0]); i++) {
            atomic_store_explicit(&fight_state_bits[i], float_bits(0.0f), memory_order_relaxed);
        }
        for (size_t i = 0; i < 2; i++) {
            atomic_store_explicit(&fight_authority_bits[i], float_bits(0.0f), memory_order_relaxed);
            atomic_store_explicit(&fight_authority_rounds[i], 0, memory_order_relaxed);
            atomic_store_explicit(&remote_authority_damage_bits[i], float_bits(0.0f), memory_order_relaxed);
            atomic_store_explicit(&remote_authority_rounds[i], 0, memory_order_relaxed);
        }
        for (size_t i = 0; i < 6; i++) {
            atomic_store_explicit(
                &remote_authority_position_bits[i], float_bits(0.0f), memory_order_relaxed);
        }
    }
    if (previous != new_value) {
        LOGI("Sessao LAN nativa: %s.", new_value ? "pronta" : "inativa");
    }
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeSetPlayerRole(
    JNIEnv *env,
    jclass type,
    jint role
) {
    (void)env;
    (void)type;
    int32_t value = role == 1 || role == 2 ? role : 0;
    atomic_store_explicit(&lan_player_role, value, memory_order_release);
    LOGI("Papel LAN nativo: jogador %d.", value);
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
    float horizontal = strongest_axis(left_x, dpad_x);
    float vertical = strongest_axis(left_y, dpad_y);
    int32_t touch_direction = atomic_load_explicit(
        &local_touch_direction, memory_order_acquire);
    switch (touch_direction) {
        case 1: horizontal = 0.0f; vertical = 1.0f; break;
        case 2: horizontal = 1.0f; vertical = 1.0f; break;
        case 3: horizontal = 1.0f; vertical = 0.0f; break;
        case 4: horizontal = 1.0f; vertical = -1.0f; break;
        case 5: horizontal = 0.0f; vertical = -1.0f; break;
        case 6: horizontal = -1.0f; vertical = -1.0f; break;
        case 7: horizontal = -1.0f; vertical = 0.0f; break;
        case 8: horizontal = -1.0f; vertical = 1.0f; break;
        default: break;
    }
    uint32_t touch_pulses = atomic_exchange_explicit(
        &local_touch_button_pulses, 0, memory_order_acq_rel);
    uint32_t local_button_snapshot = atomic_load_explicit(
        &local_buttons, memory_order_relaxed);
    uint32_t touch_button_snapshot = atomic_load_explicit(
        &local_touch_buttons, memory_order_relaxed);
    float snapshot[] = {
        horizontal,
        vertical,
        (float)(local_button_snapshot | touch_button_snapshot | touch_pulses)
    };
    jfloatArray result = (*env)->NewFloatArray(env, 3);
    if (result != NULL) {
        (*env)->SetFloatArrayRegion(env, result, 0, 3, snapshot);
    }
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeReadLocalFightState(JNIEnv *env, jclass type) {
    (void)type;
    float snapshot[9];
    snapshot[0] = (float)atomic_load_explicit(&fight_state_tick, memory_order_acquire);
    for (size_t i = 0; i < sizeof(fight_state_bits) / sizeof(fight_state_bits[0]); i++) {
        snapshot[i + 1] = bits_float(atomic_load_explicit(
            &fight_state_bits[i], memory_order_relaxed));
    }
    jfloatArray result = (*env)->NewFloatArray(env, 9);
    if (result != NULL) {
        (*env)->SetFloatArrayRegion(env, result, 0, 9, snapshot);
    }
    return result;
}

JNIEXPORT jfloatArray JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeReadLocalFightAuthority(JNIEnv *env, jclass type) {
    (void)type;
    float snapshot[11] = {
        (float)atomic_load_explicit(&fight_state_tick, memory_order_acquire),
        bits_float(atomic_load_explicit(&fight_authority_bits[0], memory_order_relaxed)),
        bits_float(atomic_load_explicit(&fight_authority_bits[1], memory_order_relaxed)),
        (float)atomic_load_explicit(&fight_authority_rounds[0], memory_order_relaxed),
        (float)atomic_load_explicit(&fight_authority_rounds[1], memory_order_relaxed),
    };
    for (size_t index = 0; index < 6; index++) {
        snapshot[index + 5] = bits_float(atomic_load_explicit(
            &fight_authority_position_bits[index], memory_order_relaxed));
    }
    jfloatArray result = (*env)->NewFloatArray(env, 11);
    if (result != NULL) {
        (*env)->SetFloatArrayRegion(env, result, 0, 11, snapshot);
    }
    return result;
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeUpdateRemoteFightClock(
    JNIEnv *env,
    jclass type,
    jfloat frames_passed,
    jlong received_at_ms
) {
    (void)env;
    (void)type;
    if (received_at_ms <= 0 || frames_passed < 0.0f || frames_passed > 1000000.0f) {
        atomic_store_explicit(&remote_fight_state_received_at_ms, 0, memory_order_release);
        atomic_store_explicit(&remote_time_passed_bits, float_bits(0.0f), memory_order_release);
        return;
    }
    atomic_store_explicit(
        &remote_time_passed_bits, float_bits(frames_passed), memory_order_relaxed);
    atomic_store_explicit(
        &remote_fight_state_received_at_ms, (int64_t)received_at_ms, memory_order_release);
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeUpdateAuthoritativeFightState(
    JNIEnv *env,
    jclass type,
    jfloat player_damage,
    jfloat opponent_damage,
    jint player_rounds_lost,
    jint opponent_rounds_lost,
    jfloat player_x,
    jfloat player_y,
    jfloat player_z,
    jfloat opponent_x,
    jfloat opponent_y,
    jfloat opponent_z,
    jlong received_at_ms
) {
    (void)env;
    (void)type;
    if (received_at_ms <= 0 || player_damage < 0.0f || player_damage > 1.0f ||
        opponent_damage < 0.0f || opponent_damage > 1.0f ||
        player_rounds_lost < 0 || player_rounds_lost > 2 ||
        opponent_rounds_lost < 0 || opponent_rounds_lost > 2 ||
        !isfinite(player_x) || !isfinite(player_y) || !isfinite(player_z) ||
        !isfinite(opponent_x) || !isfinite(opponent_y) || !isfinite(opponent_z) ||
        fabsf(player_x) > 1000000.0f || fabsf(player_y) > 1000000.0f ||
        fabsf(player_z) > 1000000.0f || fabsf(opponent_x) > 1000000.0f ||
        fabsf(opponent_y) > 1000000.0f || fabsf(opponent_z) > 1000000.0f) {
        atomic_store_explicit(&remote_authority_received_at_ms, 0, memory_order_release);
        atomic_store_explicit(&spatial_calibration_generation, 0, memory_order_release);
        return;
    }
    atomic_store_explicit(
        &remote_authority_damage_bits[0], float_bits(player_damage), memory_order_relaxed);
    atomic_store_explicit(
        &remote_authority_damage_bits[1], float_bits(opponent_damage), memory_order_relaxed);
    atomic_store_explicit(
        &remote_authority_rounds[0], player_rounds_lost, memory_order_relaxed);
    atomic_store_explicit(
        &remote_authority_rounds[1], opponent_rounds_lost, memory_order_relaxed);
    const float positions[] = {
        player_x, player_y, player_z,
        opponent_x, opponent_y, opponent_z,
    };
    for (size_t index = 0; index < 6; index++) {
        atomic_store_explicit(
            &remote_authority_position_bits[index],
            float_bits(positions[index]), memory_order_relaxed);
    }
    atomic_store_explicit(
        &remote_authority_received_at_ms, (int64_t)received_at_ms, memory_order_release);
}

JNIEXPORT jint JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeGetFightGeneration(JNIEnv *env, jclass type) {
    (void)env;
    (void)type;
    return (jint)atomic_load_explicit(
        &lan_fight_generation, memory_order_acquire);
}

JNIEXPORT jint JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeGetFightConfigMode(JNIEnv *env, jclass type) {
    (void)env;
    (void)type;
    return (jint)atomic_load_explicit(&lan_original_fight_mode, memory_order_acquire);
}

JNIEXPORT jlong JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeGetFightConfigFingerprint(JNIEnv *env, jclass type) {
    (void)env;
    (void)type;
    return (jlong)atomic_load_explicit(
        &lan_original_fight_fingerprint, memory_order_acquire);
}

JNIEXPORT jstring JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeGetFightConfigIdentity(JNIEnv *env, jclass type) {
    (void)type;
    char identity[FIGHT_CONFIG_IDENTITY_MAX + 1];
    pthread_mutex_lock(&fight_config_identity_mutex);
    strncpy(identity, lan_original_fight_identity, FIGHT_CONFIG_IDENTITY_MAX);
    identity[FIGHT_CONFIG_IDENTITY_MAX] = '\0';
    pthread_mutex_unlock(&fight_config_identity_mutex);
    return (*env)->NewStringUTF(env, identity);
}

JNIEXPORT void JNICALL
Java_com_sf2_lan_LanNativeBridge_nativeScheduleFightStart(
    JNIEnv *env,
    jclass type,
    jlong start_at_ms
) {
    (void)env;
    (void)type;
    int64_t now = now_ms();
    int64_t requested = (int64_t)start_at_ms;
    if (!atomic_load_explicit(&lan_fight_active, memory_order_acquire) ||
        requested < now - 1000 || requested > now + 30000) {
        LOGE("START nativo rejeitado: ativo=%d delta=%lldms.",
            atomic_load_explicit(&lan_fight_active, memory_order_acquire) ? 1 : 0,
            (long long)(requested - now));
        return;
    }
    atomic_store_explicit(&fight_start_at_ms, requested, memory_order_release);
    atomic_store_explicit(&fight_start_logged, false, memory_order_release);
    LOGI("START nativo agendado para +%lldms.",
        (long long)(requested - now));
}
