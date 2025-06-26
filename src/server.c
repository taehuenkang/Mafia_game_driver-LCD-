#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <fcntl.h>  
#include <sys/socket.h>
#include <arpa/inet.h>
#include "game.h"
#include "utils.h"
#include "network.h"

#define PORT     12345
#define BACKLOG  5
#define MAX_CLIENTS 10
#define MIN_PLAYERS 5

typedef enum { PHASE_WAIT, PHASE_NIGHT, PHASE_DAY, PHASE_VOTE, PHASE_ARG, PHASE_FINAL_VOTE, PHASE_END } phase_t;

typedef struct {
    int fd;
    char nickname[32];
    role_t role;
    int alive;
    int vote; // 투표 대상 인덱스
} player_t;

static int vote_ret = -1;
static player_t players[MAX_CLIENTS];
static size_t num_players = 0;
static phase_t phase = PHASE_WAIT;
static pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
static int lcd_fd = -1;  

// 밤 행동 결과 저장용
static int mafia_target = -1;
static int police_target = -1;
static int doctor_target = -1;
static int mafia_votes[MAX_CLIENTS]; // 마피아가 여러 명일 때 투표
static int mafia_vote_count = 0;
static bool night_action_done[MAX_CLIENTS];
static bool vote_done[MAX_CLIENTS];

// 함수 선언부
void send_to_player(size_t idx, const char* msg);
void broadcast_alive(const char* msg);
void add_player(int fd, const char* nickname);
void remove_player(int fd);
void* game_loop(void* arg);
void broadcast_message(const char* msg, int except_fd);
void print_player();

void reset_night_actions() {
    mafia_target = -1;
    police_target = -1;
    doctor_target = -1;
    mafia_vote_count = 0;
    memset(mafia_votes, -1, sizeof(mafia_votes));
    memset(night_action_done, 0, sizeof(night_action_done));
}

void reset_votes() {
    for (size_t i = 0; i < num_players; ++i) {
        players[i].vote = -1;
        vote_done[i] = false;
    }
}

// 모든 밤 행동이 끝났는지 체크
bool all_night_actions_done() {
    for (size_t i = 0; i < num_players; ++i) {
        if (!players[i].alive) continue;
        if ((players[i].role == ROLE_MAFIA || players[i].role == ROLE_POLICE || players[i].role == ROLE_DOCTOR) && !night_action_done[i])
            return false;
    }
    return true;
}

// 모든 투표가 끝났는지 체크
bool all_votes_done() {
    for (size_t i = 0; i < num_players; ++i) {
        if (players[i].alive && !vote_done[i])
            return false;
    }
    return true;
}

// 밤 행동 집계 및 처리
void process_night() {
    // 마피아 투표 집계
    int mafia_counts[MAX_CLIENTS] = {0};
    for (int i = 0; i < mafia_vote_count; ++i) {
        if (mafia_votes[i] >= 0 && mafia_votes[i] < (int)num_players && players[mafia_votes[i]].alive)
            mafia_counts[mafia_votes[i]]++;
    }
    int max = 0, target = -1, tie = 0;
    for (size_t i = 0; i < num_players; ++i) {
        if (mafia_counts[i] > max) {
            max = mafia_counts[i]; target = i; tie = 0;
        } else if (mafia_counts[i] == max && max > 0) {
            tie = 1;
        }
    }
    mafia_target = (tie || max == 0) ? -1 : target;
    // 의사가 살리면 생존
    if (mafia_target >= 0 && mafia_target == doctor_target) {
        mafia_target = -1;
    }
    // 희생자 처리
    if (mafia_target >= 0) {
        players[mafia_target].alive = 0;
        char msg[128];
        snprintf(msg, sizeof(msg), "[안내] 밤에 %s님이 사망했습니다.\n", players[mafia_target].nickname);
        broadcast_message(msg,-1);
        remove_player(players[mafia_target].fd);
    } else {
        broadcast_message("[안내] 밤에 아무도 사망하지 않았습니다.\n",-1);
    }
    // 경찰 결과 안내
    if (police_target >= 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "[안내] 경찰이 조사한 결과: %s님은 %s입니다.\n", players[police_target].nickname, players[police_target].role == ROLE_MAFIA ? "마피아" : "마피아 아님");
        for (size_t i = 0; i < num_players; ++i) {
            if (players[i].role == ROLE_POLICE && players[i].alive)
                send_to_player(i, msg);
        }
    }
}

// 투표 집계 및 처리
int process_vote() {
    int votes[MAX_CLIENTS];
    for (size_t i = 0; i < num_players; ++i) votes[i] = players[i].vote;
    int out_idx = tally_votes(votes, num_players);
    if (out_idx >= 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "[안내] 투표 결과 %s님이 지목되었습니다.\n", players[out_idx].nickname);
        broadcast_message(msg,-1);
    } else {
        broadcast_message("[안내] 투표 결과 아무도 지목되지 않았습니다.\n",-1);
    }
     return out_idx;
}

void process_final_vote(int vote_idx){
    int count_0 = 0;
    int count_1 = 0;
    int votes[num_players-1];
    for (size_t i = 0; i < num_players; ++i) votes[i] = players[i].vote;
    for(size_t i = 0; i < num_players; ++i){
        if(votes[i]) count_1++;
        else count_0++;
    }
    if(count_0 >= count_1) broadcast_message("[안내] 투표 결과 처형되지 않았습니다.\n",-1);
    else{ 
        players[vote_idx].alive = 0;
        char msg[128];
        snprintf(msg, sizeof(msg), "[안내] 투표 결과 %s님이 처형되었습니다.\n", players[vote_idx].nickname);
        broadcast_message(msg,-1);
        remove_player(players[vote_idx].fd);        
    }
}

void send_to_player(size_t idx, const char* msg) {
    if (players[idx].alive)
        send(players[idx].fd, msg, strlen(msg), 0);
}

// 플레이어 리스트 출력
void print_player()
{
    char list[512] = "[안내] 플레이어 목록:\n";
    for (size_t i = 0; i < num_players; ++i) {
        char entry[64];
        snprintf(entry, sizeof(entry), "%zu: %s\n", i, players[i].nickname);
        strncat(list, entry, sizeof(list) - strlen(list) - 1);
    }
    broadcast_alive(list);  
}

// 게임 상태 전환 및 진행(밤/낮/투표/승패)
void* game_loop(void* arg) {
    while (1) {
        pthread_mutex_lock(&game_mutex);
        if (phase == PHASE_WAIT && num_players >= MIN_PLAYERS) {
            // 역할 분배
            role_t roles[MAX_CLIENTS];
            assign_roles(num_players, roles);
            for (size_t i = 0; i < num_players; ++i) {
                players[i].role = roles[i];
                players[i].alive = 1;
                players[i].vote = -1;
                char msg[128];
                snprintf(msg, sizeof(msg), "[안내] 당신의 역할은 [%s]입니다.\n", role_to_string(roles[i]));
                send(players[i].fd, msg, strlen(msg), 0);
            }
            // 낮부터 시작
            phase = PHASE_DAY;
            broadcast_alive("[안내] 게임이 시작되었습니다! 낮이 되었습니다. 모두 채팅 가능합니다.\n");
            print_player();
        } else if (phase == PHASE_NIGHT && all_night_actions_done()) {
            process_night();
            phase = PHASE_DAY;
            broadcast_alive("[안내] 낮이 되었습니다. 모두 채팅 가능합니다.\n");
            print_player();
        } else if (phase == PHASE_DAY) {
            int elapsed = 0;
            pthread_mutex_unlock(&game_mutex); // 낮 시작 시 바로 뮤텍스 해제
            while (elapsed < 600) { // 0.2초 * 600 = 120초(2분)
                usleep(200000); // 0.2초
                pthread_mutex_lock(&game_mutex);
                if (lcd_fd >= 0) {
                    int rem_sec = (600 - elapsed) / 5;
                    char buf[17];
                    snprintf(buf, sizeof(buf), "morning %3d", rem_sec);
                    write(lcd_fd, buf, strlen(buf));
                }
                if(elapsed == 300) broadcast_alive("\n ❗️투표까지 1분 남았습니다❗️\n");
                if(elapsed == 450) broadcast_alive("\n ❗️투표까지 30초 남았습니다❗️\n");
                pthread_mutex_unlock(&game_mutex);
                elapsed++;
            }
            pthread_mutex_lock(&game_mutex); // 낮 끝나고 다시 뮤텍스 획득
            phase = PHASE_VOTE;
            reset_votes();
            broadcast_alive("[안내] 투표 시간입니다. 플레이어 번호를 입력하세요.\n");
            print_player();
        } else if (phase == PHASE_VOTE && all_votes_done()) {
            vote_ret = process_vote();
            if (vote_ret>=0) phase = PHASE_ARG;
            else phase = PHASE_NIGHT;
        } 
        else if(phase == PHASE_ARG){
            char msg[128];
            snprintf(msg, sizeof(msg), "[안내] 최후 변론 시간입니다. %s 님은 10초간 최후 변론하세요.\n", players[vote_ret].nickname);
            broadcast_alive(msg);

            int elapsed = 0;
            pthread_mutex_unlock(&game_mutex); // 낮 시작 시 바로 뮤텍스 해제
            while (elapsed < 50) { // 0.2초 * 50 = 10초
                usleep(200000); // 0.2초
                elapsed++;
            }
            pthread_mutex_lock(&game_mutex); 
            phase = PHASE_FINAL_VOTE;
            reset_votes();
            broadcast_alive("[안내] 최종 투표 시간입니다 살린다(0), 죽인다(1)을 입력하세요.\n");
        } 
        else if(phase == PHASE_FINAL_VOTE && all_votes_done()){
            process_final_vote(vote_ret);
            int alive_arr[MAX_CLIENTS];
            for (size_t i = 0; i < num_players; ++i) alive_arr[i] = players[i].alive;
            int mafia = 0, citizen = 0;
            for (size_t i = 0; i < num_players; ++i) {
                if (!alive_arr[i]) continue;
                if (players[i].role == ROLE_MAFIA) mafia++;
                else citizen++;
            }
            if (mafia == 0) {
                broadcast_message("[안내] 시민팀 승리!\n",-1);
                phase = PHASE_END;
            } else {
                phase = PHASE_NIGHT;
                reset_night_actions();
                if (lcd_fd >= 0) {
                    char buf[17];
                    snprintf(buf, sizeof(buf), "night wait...");
                    write(lcd_fd, buf, strlen(buf));
                }
                broadcast_alive("[안내] 밤이 되었습니다. 마피아/경찰/의사는 행동을 입력하세요.\n");
                print_player();
                for (size_t i = 0; i < num_players; ++i) {
                    if (players[i].role == ROLE_MAFIA && players[i].alive)
                        send_to_player(i, "[행동] 밤입니다. 죽일 플레이어 번호를 입력하세요.\n");
                    if (players[i].role == ROLE_POLICE && players[i].alive)
                        send_to_player(i, "[행동] 밤입니다. 조사할 플레이어 번호를 입력하세요.\n");
                    if (players[i].role == ROLE_DOCTOR && players[i].alive)
                        send_to_player(i, "[행동] 밤입니다. 살릴 플레이어 번호를 입력하세요.\n");
                }
            }
        }else if (phase == PHASE_END) {
            broadcast_message("[안내] 게임이 종료되었습니다.\n",-1);
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        pthread_mutex_unlock(&game_mutex);
        usleep(200000);
    }
    return NULL;
}

void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    char nick_buf[32] = {0};
    send(client_fd, "Enter your nickname: ", 21, 0);
    int len = recv(client_fd, nick_buf, sizeof(nick_buf)-1, 0);
    if (len <= 0) { close(client_fd); return NULL; }
    trim_newline(nick_buf);
    add_player(client_fd, nick_buf);
    char msg_buf[256];
    size_t my_idx = 0;
    while (1) {
        len = recv(client_fd, msg_buf, sizeof(msg_buf)-1, 0);
        if (len <= 0) break;
        msg_buf[len] = '\0';
        trim_newline(msg_buf);
        pthread_mutex_lock(&game_mutex);
        for (my_idx = 0; my_idx < num_players; ++my_idx)
            if (players[my_idx].fd == client_fd) break;
        if (!players[my_idx].alive) { pthread_mutex_unlock(&game_mutex); continue; }
        if (phase == PHASE_NIGHT) {
            if (players[my_idx].role == ROLE_MAFIA) {
                int t = atoi(msg_buf);
                mafia_votes[mafia_vote_count++] = t;
                night_action_done[my_idx] = true;
            } else if (players[my_idx].role == ROLE_POLICE) {
                police_target = atoi(msg_buf);
                night_action_done[my_idx] = true;
            } else if (players[my_idx].role == ROLE_DOCTOR) {
                doctor_target = atoi(msg_buf);
                night_action_done[my_idx] = true;
            }
        } else if (phase == PHASE_WAIT || phase == PHASE_DAY) {
            char send_buf[300];
            snprintf(send_buf, sizeof(send_buf), "[%s] %s\n", players[my_idx].nickname, msg_buf);
            broadcast_message(send_buf, -1); // 모든 클라이언트에게 전송
        } else if(phase == PHASE_ARG){

            if(client_fd == players[vote_ret].fd)
            {
                 char send_buf[300];
                 snprintf(send_buf, sizeof(send_buf), "[%s] %s\n", players[my_idx].nickname, msg_buf);
                broadcast_message(send_buf, -1); // 모든 클라이언트에게 전송
            }
            
        }else if (phase == PHASE_VOTE || phase == PHASE_FINAL_VOTE) {
            int vote_idx = atoi(msg_buf);
            if (vote_idx >= 0 && vote_idx < (int)num_players && players[vote_idx].alive) {
                players[my_idx].vote = vote_idx;
                vote_done[my_idx] = true;
            }
        } 
        pthread_mutex_unlock(&game_mutex);
    }
    remove_player(client_fd);
    return NULL;
}

int main() {
    int listen_fd = create_server_socket(PORT, BACKLOG);
    if (listen_fd < 0) return 1;
    printf("Server listening on port %d (fd=%d)\n", PORT, listen_fd);

    // LCD 디바이스 열기
    lcd_fd = open("/dev/lcd_i2c", O_WRONLY);
    if (lcd_fd < 0) {
        perror("Failed to open /dev/lcd_i2c");
        // 계속 실행해도 되지만 LCD 출력은 동작하지 않음
    }

    // 게임 루프 스레드 생성
    pthread_t game_tid;
    pthread_create(&game_tid, NULL, game_loop, NULL);

    while (1) {
        int* client_fd = malloc(sizeof(int));
        *client_fd = accept(listen_fd, NULL, NULL);
        if (*client_fd < 0) { free(client_fd); continue; }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }
    close(listen_fd);
    return 0;
}

// 생존자에게 메시지 브로드캐스트
void broadcast_alive(const char* msg) {
    for (size_t i = 0; i < num_players; ++i) {
        if (players[i].alive)
            send(players[i].fd, msg, strlen(msg), 0);
    }
}

// 모든 접속자에게 메시지 브로드캐스트
void broadcast_message(const char* msg, int except_fd) {
    for (size_t i = 0; i < num_players; ++i) {
        if (players[i].fd != except_fd)
            send(players[i].fd, msg, strlen(msg), 0);
    }
}

// 플레이어 추가
void add_player(int fd, const char* nickname) {
    if (num_players < MAX_CLIENTS) {
        players[num_players].fd = fd;
        strncpy(players[num_players].nickname, nickname, sizeof(players[num_players].nickname)-1);
        players[num_players].nickname[sizeof(players[num_players].nickname)-1] = '\0';
        players[num_players].role = ROLE_CIVILIAN;
        players[num_players].alive = 1;
        players[num_players].vote = -1;
        num_players++;
    }
}

// 플레이어 제거
void remove_player(int fd) {
    for (size_t i = 0; i < num_players; ++i) {
        if (players[i].fd == fd) {
            players[i].alive = 0;
            close(players[i].fd);
            // 자리 채우기(간단화)
            for (size_t j = i; j + 1 < num_players; ++j) {
                players[j] = players[j+1];
            }
            num_players--;
            break;
        }
    }
}