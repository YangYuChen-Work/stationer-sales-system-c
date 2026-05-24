@echo off
chcp 65001 >nul
rem ============================================================
rem build.bat — 文具店销售管理系统后端 (Windows)
rem 需要: MinGW-w64 (gcc) + MySQL C Connector
rem 使用方法: build.bat && stationery_server.exe
rem ============================================================

echo === 文具店销售管理系统 — 编译脚本 ===
echo.

set "MYSQL_PATH=G:\A_Development_Software\MySQL8.0\MySQL_Server"
rem 如果上面的路径不匹配，请修改为你的 MySQL 实际安装路径
rem set "MYSQL_PATH=C:\Program Files\MySQL\MySQL Server 8.0"

rem 检查 MySQL 库路径
if not exist "%MYSQL_PATH%\include\mysql.h" (
    echo [警告] MySQL 头文件未找到于 %MYSQL_PATH%
    echo        请将 MYSQL_PATH 变量修改为您的 MySQL 安装路径
    echo        或通过 -I 和 -L 参数手动指定路径
    echo.
)

set CFLAGS=-Wall -O2 -std=c11 -I"%MYSQL_PATH%\include" -Ilib
set LDFLAGS=-L"%MYSQL_PATH%\lib" -lmysql -lws2_32
rem 运行前需确保 libmysql.dll 在 PATH 中，或将其复制到 exe 同级目录

echo [编译] gcc %CFLAGS% ...

gcc %CFLAGS% ^
    src/main.c ^
    src/db.c ^
    src/json.c ^
    src/api.c ^
    src/inventory.c ^
    src/sales.c ^
    src/purchase.c ^
    src/search.c ^
    src/stats.c ^
    src/alert.c ^
    src/suggest.c ^
    src/heatmap.c ^
    src/ai_agent.c ^
    lib/mongoose.c ^
    -o stationery_server.exe ^
    %LDFLAGS%

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [错误] 编译失败！请检查:
    echo   1. GCC (MinGW-w64) 是否已安装并添加到 PATH
    echo   2. MySQL C 开发库是否已安装
    echo   3. 可通过设置 MYSQL_PATH 变量指定 MySQL 路径
    goto :eof
)

echo.
echo [成功] 编译完成: stationery_server.exe
echo.
echo 启动服务: stationery_server.exe
echo 前端开发: cd frontend ^&^& npm run dev
echo.
