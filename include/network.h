#ifndef NETWORK_H
#define NETWORK_H

// 서버 소켓 생성: PORT 번호로 바인드 후 listen까지 수행
// 실패 시 음수 반환, 성공 시 리스닝 소켓 fd 반환
int create_server_socket(int port, int backlog);

// 클라이언트 소켓 생성: 서버주소와 PORT에 connect
// 실패 시 음수 반환, 성공 시 연결된 소켓 fd 반환
int create_client_socket(const char* server_ip, int port);

#endif // NETWORK_H