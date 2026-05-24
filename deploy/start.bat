@echo off
chcp 65001 >nul
title 文具店销售管理系统

echo ========================================
echo   文具店销售管理系统 v1.0
echo ========================================
echo.

REM 切换到项目根目录 (bat 文件所在目录的上级)
cd /d "%~dp0.."

REM 检查关键文件
if not exist "stationery_server.exe" (
    echo [错误] 未找到 stationery_server.exe
    echo        当前目录: %cd%
    pause
    exit /b 1
)

if not exist "libmysql.dll" (
    echo [错误] 未找到 libmysql.dll
    pause
    exit /b 1
)

if not exist "frontend\dist\index.html" (
    echo [错误] 未找到前端文件 frontend\dist\index.html
    echo        请先执行: npm run build
    pause
    exit /b 1
)

REM 2 秒后打开浏览器 (给服务器启动留时间)
echo [启动] 正在启动服务器...
echo        浏览器将在 2 秒后自动打开...
echo.
start "" cmd /c "timeout /t 2 /nobreak >nul && start http://localhost:8080"

REM 启动后端服务器 (前台运行，关闭窗口即停止服务)
stationery_server.exe
