#ifndef GAME_H
#define GAME_H

#include <stddef.h>

typedef enum {
    ROLE_MAFIA,
    ROLE_CIVILIAN,
    ROLE_POLICE,
    ROLE_DOCTOR
} role_t;

// num_players 명에게 roles 배열에 역할을 할당합니다.
// MIN_PLAYERS(=5) 기준으로는 Mafia 1, Police 1, Doctor 1, 나머지는 Civilian.
// 더 많은 인원이 들어오면 Mafia 비율을 늘리거나 Civilian으로 처리.
// 성공 시 0, 실패 시 –1 반환.
int assign_roles(size_t num_players, role_t roles[]);

// role_t → 문자열로 변환
const char* role_to_string(role_t r);

// 투표 집계: votes 배열에서 최다 득표 인덱스 반환(동점 시 -1)
int tally_votes(const int votes[], size_t num_players);

// 승패 판정: 남은 마피아/시민 수로 결과 반환(0: 진행중, 1: 시민 승, 2: 마피아 승)
int check_win_condition(const role_t roles[], const int alive[], size_t num_players);

#endif // GAME_H