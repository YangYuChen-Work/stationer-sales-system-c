@echo off
chcp 65001 >nul
title 文具店销售管理系统 - 数据库初始化

echo ========================================
echo   文具店销售管理系统 - 数据库初始化
echo ========================================
echo.

REM 检查 MySQL 命令是否可用
where mysql >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 未找到 mysql 命令。
    echo.
    echo 请确保已安装 MySQL 8.0，并将 mysql.exe 所在目录加入 PATH 环境变量。
    echo 通常路径: C:\Program Files\MySQL\MySQL Server 8.0\bin
    echo.
    pause
    exit /b 1
)

REM 测试 MySQL 连接
echo [1/3] 检测 MySQL 连接...
set /p DB_PASS=请输入 MySQL root 密码:
mysql -u root -p%DB_PASS% -e "SELECT 1;" >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 无法连接 MySQL，请检查用户名/密码是否正确。
    pause
    exit /b 1
)
echo       连接成功。

REM 导入建表脚本
echo.
echo [2/3] 导入数据库表结构...
mysql -u root -p%DB_PASS% < "%~dp0..\database\schema.sql"
if %errorlevel% neq 0 (
    echo [错误] 导入 schema.sql 失败。
    pause
    exit /b 1
)
echo       表结构创建完成。

REM 导入初始数据
echo.
echo [3/3] 导入初始数据...
mysql -u root -p%DB_PASS% < "%~dp0..\database\seed.sql"
if %errorlevel% neq 0 (
    echo [错误] 导入 seed.sql 失败。
    pause
    exit /b 1
)
echo       初始数据导入完成。

echo.
echo ========================================
echo   数据库初始化成功！
echo   现在可以运行 start.bat 启动系统。
echo ========================================
echo.
pause
