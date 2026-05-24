/**
 * db.c - 数据库连接模块实现
 *
 * 提供 MySQL 数据库连接管理、查询执行、事务控制和辅助查询功能。
 * 所有数据库操作统一通过本模块进行，错误信息输出到 stderr，
 * 避免污染 stdout（stdout 用于 HTTP 响应）。
 */

#include "db.h"

/* ================================================================
 *  连接管理
 * ================================================================ */

MYSQL* db_get_connection(void)
{
    MYSQL *conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "[DB] mysql_init() 失败：内存不足\n");
        return NULL;
    }

    /* 设置 UTF-8 字符集，确保中文数据正确存取 */
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    /* 启用自动重连，应对连接超时断开的情况 */
    bool reconnect = 1;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    /* 建立实际连接 */
    if (mysql_real_connect(conn, DB_HOST, DB_USER, DB_PASS, DB_NAME,
                           DB_PORT, NULL, 0) == NULL) {
        fprintf(stderr, "[DB] mysql_real_connect() 失败：%s\n",
                mysql_error(conn));
        mysql_close(conn);
        return NULL;
    }

    return conn;
}

void db_release_connection(MYSQL *conn)
{
    if (conn != NULL) {
        mysql_close(conn);
    }
}

int db_ping(MYSQL *conn)
{
    if (conn == NULL) {
        return 0;
    }
    return mysql_ping(conn) == 0;
}

/* ================================================================
 *  查询执行
 * ================================================================ */

int db_execute(MYSQL *conn, const char *sql)
{
    if (conn == NULL || sql == NULL) {
        fprintf(stderr, "[DB] db_execute() 参数无效：conn 或 sql 为 NULL\n");
        return -1;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "[DB] 执行 SQL 失败：%s\n", mysql_error(conn));
        fprintf(stderr, "[DB] SQL: %s\n", sql);
        return -1;
    }

    /* 返回受影响的行数 */
    my_ulonglong affected = mysql_affected_rows(conn);
    return (int)affected;
}

MYSQL_RES* db_query(MYSQL *conn, const char *sql)
{
    if (conn == NULL || sql == NULL) {
        fprintf(stderr, "[DB] db_query() 参数无效：conn 或 sql 为 NULL\n");
        return NULL;
    }

    if (mysql_query(conn, sql) != 0) {
        fprintf(stderr, "[DB] 查询 SQL 失败：%s\n", mysql_error(conn));
        fprintf(stderr, "[DB] SQL: %s\n", sql);
        return NULL;
    }

    MYSQL_RES *result = mysql_store_result(conn);
    if (result == NULL) {
        fprintf(stderr, "[DB] mysql_store_result() 失败：%s\n",
                mysql_error(conn));
    }

    return result;
}

void db_free_result(MYSQL_RES *result)
{
    if (result != NULL) {
        mysql_free_result(result);
    }
}

/* ================================================================
 *  事务控制
 * ================================================================ */

int db_begin_transaction(MYSQL *conn)
{
    return db_execute(conn, "START TRANSACTION");
}

int db_commit(MYSQL *conn)
{
    return db_execute(conn, "COMMIT");
}

int db_rollback(MYSQL *conn)
{
    return db_execute(conn, "ROLLBACK");
}

/* ================================================================
 *  辅助查询
 * ================================================================ */

int db_product_exists(MYSQL *conn, const char *product_code)
{
    if (conn == NULL || product_code == NULL) {
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT 1 FROM inventory WHERE product_code='%s'",
             product_code);

    MYSQL_RES *result = db_query(conn, sql);
    if (result == NULL) {
        return -1;
    }

    int exists = (mysql_num_rows(result) > 0) ? 1 : 0;
    db_free_result(result);
    return exists;
}

int db_get_stock(MYSQL *conn, const char *product_code)
{
    if (conn == NULL || product_code == NULL) {
        return -1;
    }

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT stock_quantity FROM inventory WHERE product_code='%s'",
             product_code);

    MYSQL_RES *result = db_query(conn, sql);
    if (result == NULL) {
        return -1;
    }

    /* 商品不存在则返回 -1 */
    if (mysql_num_rows(result) == 0) {
        db_free_result(result);
        return -1;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int stock = (row && row[0]) ? atoi(row[0]) : -1;
    db_free_result(result);
    return stock;
}

const char* db_error(MYSQL *conn)
{
    if (conn == NULL) {
        return "数据库连接为空";
    }
    return mysql_error(conn);
}

void db_escape_html(const char *src, char *dst, int dst_size)
{
    if (src == NULL || dst == NULL || dst_size <= 0) {
        return;
    }

    const char *p = src;
    char *out = dst;
    char *end = dst + dst_size - 1;  /* 预留一个字节给 '\0' */

    while (*p != '\0' && out < end) {
        switch (*p) {
        case '&':
            if (out + 5 <= end) {
                strcpy(out, "&amp;");
                out += 5;
            } else {
                goto done;
            }
            break;
        case '<':
            if (out + 4 <= end) {
                strcpy(out, "&lt;");
                out += 4;
            } else {
                goto done;
            }
            break;
        case '>':
            if (out + 4 <= end) {
                strcpy(out, "&gt;");
                out += 4;
            } else {
                goto done;
            }
            break;
        case '"':
            if (out + 6 <= end) {
                strcpy(out, "&quot;");
                out += 6;
            } else {
                goto done;
            }
            break;
        case '\'':
            if (out + 5 <= end) {
                strcpy(out, "&#39;");
                out += 5;
            } else {
                goto done;
            }
            break;
        default:
            *out++ = *p;
            break;
        }
        p++;
    }

done:
    *out = '\0';
}
