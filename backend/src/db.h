#ifndef DB_H
#define DB_H

#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== MySQL 连接配置 ========== */
#define DB_HOST     "localhost"
#define DB_PORT     3306
#define DB_USER     "root"
#define DB_PASS     "20060222"
#define DB_NAME     "stationery_db"

/* ========== 连接管理 ========== */

/**
 * 获取数据库连接
 * 创建并返回一个新的 MySQL 连接，已配置 UTF-8 字符集和自动重连
 * 失败时打印错误到 stderr 并返回 NULL
 */
MYSQL* db_get_connection(void);

/**
 * 释放数据库连接
 * 安全关闭连接并释放资源，conn 为 NULL 时无操作
 */
void db_release_connection(MYSQL *conn);

/**
 * 检查连接是否有效
 * 返回 1 表示连接正常，0 表示连接已断开
 */
int db_ping(MYSQL *conn);

/* ========== 查询执行 ========== */

/**
 * 执行非查询 SQL（INSERT / UPDATE / DELETE）
 * 返回受影响的行数，失败返回 -1
 * 错误信息打印到 stderr
 */
int db_execute(MYSQL *conn, const char *sql);

/**
 * 执行查询 SQL（SELECT），返回结果集
 * 调用者必须通过 db_free_result() 释放返回的结果集
 * 失败时打印错误到 stderr 并返回 NULL
 */
MYSQL_RES* db_query(MYSQL *conn, const char *sql);

/**
 * 释放查询结果集
 * result 为 NULL 时无操作
 */
void db_free_result(MYSQL_RES *result);

/* ========== 事务控制 ========== */

/** 开启事务，成功返回 0，失败返回 -1 */
int db_begin_transaction(MYSQL *conn);

/** 提交事务，成功返回 0，失败返回 -1 */
int db_commit(MYSQL *conn);

/** 回滚事务，成功返回 0，失败返回 -1 */
int db_rollback(MYSQL *conn);

/* ========== 辅助查询 ========== */

/**
 * 检查商品编号是否存在
 * 存在返回 1，不存在返回 0，出错返回 -1
 */
int db_product_exists(MYSQL *conn, const char *product_code);

/**
 * 查询当前库存数量
 * 成功返回库存数量，失败返回 -1
 */
int db_get_stock(MYSQL *conn, const char *product_code);

/**
 * 获取最后一次错误信息
 * 返回 MySQL 连接上最近一次操作的错误描述字符串
 */
const char* db_error(MYSQL *conn);

/**
 * HTML 转义（防 XSS）
 * 将 src 中的 & < > " ' 替换为对应的 HTML 实体
 * dst 为输出缓冲区，dst_size 为缓冲区大小
 */
void db_escape_html(const char *src, char *dst, int dst_size);

#endif /* DB_H */
