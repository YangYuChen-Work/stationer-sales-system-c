#!/bin/bash
# 文具店销售管理系统 - 启动脚本 (Mac/Linux)

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "========================================"
echo "  文具店销售管理系统 v1.0"
echo "========================================"
echo ""

# 检查可执行文件
if [ ! -f "stationery_server" ]; then
    echo "[错误] 未找到 stationery_server"
    echo "       请先编译: make"
    exit 1
fi

# 检查前端文件
if [ ! -f "frontend/dist/index.html" ]; then
    echo "[错误] 未找到前端文件 frontend/dist/index.html"
    echo "       请先执行: cd frontend && npm run build"
    exit 1
fi

echo "[启动] 正在启动服务器..."
echo "       浏览器将在 2 秒后自动打开..."
echo ""

# Mac 用 open，Linux 用 xdg-open
(sleep 2 && (command -v open &> /dev/null && open http://localhost:8080 || xdg-open http://localhost:8080)) &

# 启动服务器
./stationery_server
