#include <assert.h>
#include <stdio.h>
#include "../../lan_bridge/android/native/sf2_action_timing.h"

int main(void) {
    assert(lan_action_start_frame(-1, 2, 3, 36, 39, true) == 14);
    assert(lan_action_start_frame(-1, 2, 3, 38, 9, true) == 14);
    assert(lan_action_start_frame(-1, 2, 3, 2, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, 36, 251, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, 36, -1, true) == -1);
    assert(lan_action_start_frame(-1, 2, 1, 36, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 0, 36, 0, true) == -1);
    assert(lan_action_start_frame(-1, -1, 3, 36, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, NAN, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, INFINITY, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, -10, 0, true) == -1);
    assert(lan_action_start_frame(-1, 2, 3, 121, 0, true) == -1);
    assert(lan_action_start_frame(8, 2, 3, 36, 0, false) == 8);
    puts("13 native action timing assertions passed");
    return 0;
}
