#!/bin/bash
# 文具店销售管理系统 - 数据库初始化 (Mac/Linux)

set -e

echo "========================================"
echo "  文具店销售管理系统 - 数据库初始化"
echo "========================================"
echo ""

# 检查 mysql 命令
if ! command -v mysql &> /dev/null; then
    echo "[错误] 未找到 mysql 命令。"
    echo ""
    echo "Mac 安装方法:"
    echo "  brew install mysql"
    echo ""
    exit 1
fi

# 获取数据库密码
read -sp "请输入 MySQL root 密码: " DB_PASS
echo ""

# 测试连接
echo "[1/3] 检测 MySQL 连接..."
if ! mysql -u root -p"$DB_PASS" -e "SELECT 1;" &> /dev/null; then
    echo "[错误] 无法连接 MySQL，请检查用户名/密码是否正确。"
    exit 1
fi
echo "      连接成功。"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# 导入建表脚本
echo ""
echo "[2/3] 导入数据库表结构..."
mysql -u root -p"$DB_PASS" < "$PROJECT_DIR/database/schema.sql"
echo "      表结构创建完成。"

# 导入初始数据
echo ""
echo "[3/3] 导入初始数据..."
mysql -u root -p"$DB_PASS" < "$PROJECT_DIR/database/seed.sql"
echo "      初始数据导入完成。"

echo ""
echo "========================================"
echo "  数据库初始化成功！"
echo "  现在可以运行 ./start.sh 启动系统。"
echo "========================================"
echo ""
