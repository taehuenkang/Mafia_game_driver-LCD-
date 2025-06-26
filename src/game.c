#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 역할 문자열
static const char* role_names[] = {
    "Mafia",
    "Civilian",
    "Police",
    "Doctor"
};

const char* role_to_string(role_t r) {
    if (r < 0 || r >= sizeof(role_names)/sizeof(*role_names))
        return "Unknown";
    return role_names[r];
}

int assign_roles(size_t num_players, role_t roles[]) {
    if (num_players < 5 || roles == NULL)
        return -1;

    // 기본 구성: 1 Mafia, 1 Police, 1 Doctor, 나머지 Civilian
    size_t idx = 0;
    roles[idx++] = ROLE_MAFIA;
    roles[idx++] = ROLE_POLICE;
    roles[idx++] = ROLE_DOCTOR;
    for (; idx < num_players; ++idx) {
        roles[idx] = ROLE_CIVILIAN;
    }

    // Fisher–Yates shuffle
    srand((unsigned)time(NULL));
    for (size_t i = num_players - 1; i > 0; --i) {
        size_t j = rand() % (i + 1);
        role_t tmp = roles[i];
        roles[i] = roles[j];
        roles[j] = tmp;
    }
    return 0;
}

int tally_votes(const int votes[], size_t num_players) {
    int max_votes = 0, max_idx = -1, tie = 0;
    int counts[num_players];
    memset(counts, 0, sizeof(counts));
    for (size_t i = 0; i < num_players; ++i) {
        if (votes[i] >= 0 && votes[i] < (int)num_players)
            counts[votes[i]]++;
    }
    for (size_t i = 0; i < num_players; ++i) {
        if (counts[i] > max_votes) {
            max_votes = counts[i];
            max_idx = i;
            tie = 0;
        } else if (counts[i] == max_votes && max_votes > 0) {
            tie = 1;
        }
    }
    return (tie || max_votes == 0) ? -1 : max_idx;
}

int check_win_condition(const role_t roles[], const int alive[], size_t num_players) {
    int mafia = 0, citizen = 0;
    for (size_t i = 0; i < num_players; ++i) {
        if (!alive[i]) continue;
        if (roles[i] == ROLE_MAFIA) mafia++;
        else citizen++;
    }
    if (mafia == 0) return 1; // 시민 승
    if (mafia >= citizen) return 2; // 마피아 승
    return 0; // 진행중
}