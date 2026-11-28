// Included after native helpers: all IL2CPP calls stay on the simulation thread.
// Network threads exchange value records only. Each accepted event is replayed
// once and acknowledged only after the native selector succeeds.
#define LAN_ACTION_CAPACITY 128u
struct lan_action_event {
    uint32_t sequence, tick;
    int32_t parameter, last_parameter;
    bool flag_one, flag_two;
    char name[128];
};
static pthread_mutex_t lan_action_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct lan_action_event outgoing_actions[LAN_ACTION_CAPACITY];
static uint32_t action_head, action_count, action_sequence, applied_action_sequence;
static struct lan_action_event incoming_action;
static bool incoming_action_valid;
static bool action_overflow_logged;
static _Thread_local bool replaying_lan_action;
static _Atomic bool remote_action_authority;

static void reset_lan_actions(void) {
    pthread_mutex_lock(&lan_action_mutex);
    action_head = action_count = action_sequence = applied_action_sequence = 0;
    incoming_action_valid = action_overflow_logged = false;
    atomic_store(&remote_action_authority, false);
    pthread_mutex_unlock(&lan_action_mutex);
}

static int action_controller_side(void *controller) {
    uintptr_t fighters[2] = {atomic_load(&lan_local_fighter), atomic_load(&lan_remote_fighter)};
    for (int i = 0; i < 2; ++i) {
        if (fighters[i] && *(void **)((uint8_t *)fighters[i] + 0x290u) == controller) return i + 1;
    }
    return 0;
}

static bool hooked_select_animation(void *controller, void *action, int32_t parameter,
    bool flag_one, bool flag_two, int32_t last_parameter, const void *method) {
    int side = atomic_load(&lan_fight_active) ? action_controller_side(controller) : 0;
    int role = atomic_load(&lan_player_role);
    if (side && side != role && atomic_load(&remote_action_authority) && !replaying_lan_action) {
        // The owner publishes actual decisions. Input prediction must not start
        // a second animation on the remote Fighter after authority is acquired.
        return false;
    }
    bool success = original_select_animation(controller, action, parameter,
        flag_one, flag_two, last_parameter, method);
    if (!success || !action || side != role || side == 0 || replaying_lan_action) return success;
    struct lan_action_event event = {0};
    managed_string_to_ascii(*(void **)((uint8_t *)action + 0x68u), event.name, sizeof(event.name));
    if (!event.name[0]) return success;
    event.tick = atomic_load(&fight_state_tick);
    event.parameter = parameter;
    event.last_parameter = last_parameter;
    event.flag_one = flag_one;
    event.flag_two = flag_two;
    pthread_mutex_lock(&lan_action_mutex);
    if (action_count < LAN_ACTION_CAPACITY) {
        event.sequence = ++action_sequence;
        outgoing_actions[(action_head + action_count++) % LAN_ACTION_CAPACITY] = event;
        LOGI("LAN action publish: player=%d seq=%u tick=%u name=%s args=%d/%d/%d/%d.",
            side, event.sequence, event.tick, event.name, parameter, flag_one, flag_two, last_parameter);
    } else if (!action_overflow_logged) {
        action_overflow_logged = true;
        LOGE("LAN action queue full: synchronization requires restarting this fight.");
    }
    pthread_mutex_unlock(&lan_action_mutex);
    return success;
}

static void apply_received_lan_action(void) {
    struct lan_action_event event;
    pthread_mutex_lock(&lan_action_mutex);
    bool valid = incoming_action_valid;
    event = incoming_action;
    pthread_mutex_unlock(&lan_action_mutex);
    if (!valid) return;
    int role = atomic_load(&lan_player_role);
    uintptr_t fighter = atomic_load(role == 1 ? &lan_remote_fighter : &lan_local_fighter);
    if (!fighter) return;
    void *controller = *(void **)((uint8_t *)fighter + 0x290u);
    void *list = *(void **)((uint8_t *)fighter + 0x3a0u);
    if (!controller || !list) return;
    int32_t count = *(int32_t *)((uint8_t *)list + 0x18u);
    void *items = *(void **)((uint8_t *)list + 0x10u);
    if (!items || count < 0 || count > 2048) return;
    void *action = NULL;
    for (int32_t i = 0; i < count; ++i) {
        void *candidate = *(void **)((uint8_t *)items + 0x20u + i * sizeof(void *));
        if (!candidate) continue;
        char name[128];
        managed_string_to_ascii(*(void **)((uint8_t *)candidate + 0x68u), name, sizeof(name));
        if (!strcmp(name, event.name)) { action = candidate; break; }
    }
    if (!action) return; // no foreign pointers or guessed action substitution
    replaying_lan_action = true;
    bool success = original_select_animation(controller, action, event.parameter,
        event.flag_one, event.flag_two, event.last_parameter, NULL);
    replaying_lan_action = false;
    if (!success) return;
    atomic_store(&remote_action_authority, true);
    pthread_mutex_lock(&lan_action_mutex);
    applied_action_sequence = event.sequence;
    incoming_action_valid = false;
    pthread_mutex_unlock(&lan_action_mutex);
    LOGI("LAN action applied: player=%d seq=%u sourceTick=%u localTick=%u name=%s.",
        3 - role, event.sequence, event.tick, atomic_load(&fight_state_tick), event.name);
}

JNIEXPORT jstring JNICALL Java_com_sf2_lan_LanNativeBridge_nativeReadAction(JNIEnv *env, jclass type) {
    (void)type;
    char record[256] = "";
    pthread_mutex_lock(&lan_action_mutex);
    if (action_count) {
        struct lan_action_event *e = &outgoing_actions[action_head];
        snprintf(record, sizeof(record), "%u,%u,%d,%d,%d,%d,%s", e->sequence,
            e->tick, e->parameter, e->flag_one, e->flag_two, e->last_parameter, e->name);
    }
    pthread_mutex_unlock(&lan_action_mutex);
    return (*env)->NewStringUTF(env, record);
}

JNIEXPORT void JNICALL Java_com_sf2_lan_LanNativeBridge_nativeAckAction(JNIEnv *env, jclass type, jint sequence) {
    (void)env; (void)type;
    pthread_mutex_lock(&lan_action_mutex);
    if (action_count && outgoing_actions[action_head].sequence == (uint32_t)sequence) {
        action_head = (action_head + 1) % LAN_ACTION_CAPACITY;
        --action_count;
    }
    pthread_mutex_unlock(&lan_action_mutex);
}

JNIEXPORT jint JNICALL Java_com_sf2_lan_LanNativeBridge_nativeAppliedAction(JNIEnv *env, jclass type) {
    (void)env; (void)type;
    pthread_mutex_lock(&lan_action_mutex);
    uint32_t seq = applied_action_sequence;
    pthread_mutex_unlock(&lan_action_mutex);
    return (jint)seq;
}

JNIEXPORT void JNICALL Java_com_sf2_lan_LanNativeBridge_nativeReceiveAction(JNIEnv *env, jclass type, jstring record) {
    (void)type;
    if (!record || !atomic_load(&lan_fight_active)) return;
    const char *text = (*env)->GetStringUTFChars(env, record, NULL);
    if (!text) return;
    struct lan_action_event event = {0};
    int one, two, consumed = 0;
    int fields = sscanf(text, "%u,%u,%d,%d,%d,%d,%127[A-Za-z0-9_-]%n",
        &event.sequence, &event.tick, &event.parameter, &one, &two,
        &event.last_parameter, event.name, &consumed);
    bool valid = fields == 7 && text[consumed] == '\0' && event.sequence > 0 &&
        (one == 0 || one == 1) && (two == 0 || two == 1) &&
        event.parameter >= -1 && event.parameter <= 100000 &&
        event.last_parameter >= -1 && event.last_parameter <= 100000;
    (*env)->ReleaseStringUTFChars(env, record, text);
    if (!valid) return;
    event.flag_one = one; event.flag_two = two;
    pthread_mutex_lock(&lan_action_mutex);
    if (!incoming_action_valid && event.sequence == applied_action_sequence + 1) {
        incoming_action = event;
        incoming_action_valid = true;
    }
    pthread_mutex_unlock(&lan_action_mutex);
}
