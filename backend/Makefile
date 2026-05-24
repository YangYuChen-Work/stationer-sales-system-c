# ============================================================
# Makefile — 文具店销售管理系统后端 (Mac/Linux)
# 使用方法: make && ./stationery_server
# ============================================================

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -std=c11

# MySQL 路径 (根据系统自动检测)
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
    # Mac (Homebrew MySQL)
    # Apple Silicon 路径
    MYSQL_PREFIX := $(shell test -d /opt/homebrew/opt/mysql && echo /opt/homebrew/opt/mysql || echo /usr/local/opt/mysql)
    CFLAGS  += -I$(MYSQL_PREFIX)/include/mysql
    LDFLAGS  = -L$(MYSQL_PREFIX)/lib -lmysqlclient -lpthread -lm
else
    # Linux
    LDFLAGS  = -lmysqlclient -lpthread -lm
endif

# 源文件列表
SRCS = src/main.c \
       src/db.c \
       src/json.c \
       src/api.c \
       src/inventory.c \
       src/sales.c \
       src/purchase.c \
       src/search.c \
       src/stats.c \
       src/alert.c \
       src/suggest.c \
       src/heatmap.c \
       src/ai_agent.c \
       lib/mongoose.c

# 输出可执行文件
TARGET = stationery_server

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRCS)
	@echo "=== 编译文具店销售管理系统后端 ==="
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✓ 编译完成: $(TARGET)"

clean:
	rm -f $(TARGET)
	@echo "✓ 清理完成"

run: $(TARGET)
	./$(TARGET)

help:
	@echo "make        — 编译项目"
	@echo "make clean  — 清理编译产物"
	@echo "make run    — 编译并运行"
