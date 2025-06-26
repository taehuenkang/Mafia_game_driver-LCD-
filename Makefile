# 컴파일러 및 옵션
CC       := gcc
CFLAGS   := -Iinclude -Wall -Wextra -pthread

# 빌드 디렉터리
BUILD_DIR := build

# 소스 파일 목록
SRCS := src/server.c \
        src/client.c \
        src/network.c \
        src/game.c \
        src/utils.c

# .c → .o 대응
OBJS := $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

# 최종 바이너리
TARGETS := $(BUILD_DIR)/server \
           $(BUILD_DIR)/client

# 기본 타겟
all: $(TARGETS)

# build 디렉터리 생성
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Pattern rule: src/%.c → build/%.o
$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# server 링크
$(BUILD_DIR)/server: $(BUILD_DIR)/server.o \
                     $(BUILD_DIR)/network.o \
                     $(BUILD_DIR)/game.o \
                     $(BUILD_DIR)/utils.o
	$(CC) $(CFLAGS) $^ -o $@

# client 링크
$(BUILD_DIR)/client: $(BUILD_DIR)/client.o \
                     $(BUILD_DIR)/network.o \
                     $(BUILD_DIR)/utils.o
	$(CC) $(CFLAGS) $^ -o $@

# 깨끗하게 지우기
clean:
	rm -rf $(BUILD_DIR)/*

.PHONY: all clean
