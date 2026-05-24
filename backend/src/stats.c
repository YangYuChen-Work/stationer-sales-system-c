/**
 * @file    stats.c
 * @brief   统计分析模块 —— 综合统计概览与橡皮类专项统计 HTTP 处理器实现
 *
 * 本模块为文具店销售管理系统提供数据分析维度的 RESTful API，
 * 包含两个接口：
 *   1. handle_stats_overview —— 综合统计概览（库存/销售/进货汇总 + TOP10 排行 + 库存预警）
 *   2. handle_stats_eraser   —— 橡皮类产品近 7 天销售明细统计
 *
 * 所有接口均：
 *   - 使用参数化 / 转义方式构造 SQL，防止 SQL 注入
 *   - 返回统一 JSON 格式响应：{"code":N,"message":"...","data":...}
 *   - 携带 CORS 跨域头，支持前端跨域访问
 *   - 对数据库操作错误做防御性检查并返回相应 HTTP 状态码
 *
 * 依赖：mongoose.h(HTTP 服务)、db.h(数据库操作)、json.h(JSON 构造)
 */

#include "stats.h"
#include "db.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** 单条 SQL 语句缓冲区最大字节数 */
#define SQL_BUF_SIZE    4096

/** 通用字符串缓冲区大小 */
#define STR_BUF_SIZE    512

/* ================================================================
 * 内部辅助函数 —— HTTP 响应
 * ================================================================ */

/**
 * 发送 JSON 响应并添加 CORS 头，自动释放 json_str 内存
 */
static void send_json_resp(struct mg_connection *c, char *json_str)
{
    mg_http_reply(c, 200,
                  "Content-Type: application/json; charset=utf-8\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n",
                  "%s\n", json_str ? json_str : "{}");
    free(json_str);
}

/**
 * 发送带有自定义 HTTP 状态码的 JSON 错误响应
 */
static void send_json_error(struct mg_connection *c, int http_status, char *json_str)
{
    mg_http_reply(c, http_status,
                  "Content-Type: application/json; charset=utf-8\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n",
                  "%s\n", json_str ? json_str : "{}");
    free(json_str);
}

/* ================================================================
 * 内部辅助函数 —— 日期工具
 * ================================================================ */

/**
 * 获取当前日期字符串，格式 "YYYY-MM-DD"
 * 结果写入 buf，缓冲区至少 11 字节
 */
static void get_today_date(char *buf, int buf_size)
{
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(buf, buf_size, "%Y-%m-%d", tm_now);
}

/**
 * 获取 N 天前的日期字符串，格式 "YYYY-MM-DD"
 * 结果写入 buf，缓冲区至少 11 字节
 */
static void get_date_days_ago(char *buf, int buf_size, int days_ago)
{
    time_t now = time(NULL);
    time_t target = now - (days_ago * 86400);
    struct tm *tm_target = localtime(&target);
    strftime(buf, buf_size, "%Y-%m-%d", tm_target);
}

/* ================================================================
 * 内部辅助函数 —— 数据库查询辅助
 * ================================================================ */

/**
 * 执行单值整数查询（SELECT COUNT/SUM 等）
 * 成功返回结果值，失败返回 default_val
 */
static int query_int(MYSQL *conn, const char *sql, int default_val)
{
    MYSQL_RES *res = db_query(conn, sql);
    if (!res) return default_val;

    int val = default_val;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) val = atoi(row[0]);

    db_free_result(res);
    return val;
}

/**
 * 执行单值浮点查询（SELECT SUM 等）
 * 成功返回结果值，失败返回 default_val
 */
static double query_double(MYSQL *conn, const char *sql, double default_val)
{
    MYSQL_RES *res = db_query(conn, sql);
    if (!res) return default_val;

    double val = default_val;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row && row[0]) val = atof(row[0]);

    db_free_result(res);
    return val;
}

/* ================================================================
 * 1. handle_stats_overview —— 综合统计概览
 *    GET /api/stats/overview
 *
 * 返回五部分统计数据：
 *   - inventory_summary : 库存汇总（商品总数、分类数、库存总值、库存总量）
 *   - sales_summary     : 销售汇总（当日/本周/本月/全部）
 *   - purchase_summary  : 进货汇总（当月/全部）
 *   - sales_ranking     : 销售排行 TOP10
 *   - inventory_warnings: 库存预警列表
 * ================================================================ */

void handle_stats_overview(struct mg_connection *c, struct mg_http_message *hm)
{
    (void)hm; /* 该接口不使用查询参数，消除编译警告 */

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_error(c, 500, err);
        return;
    }

    /* ================================================================
     * 第一部分：库存汇总
     * ================================================================ */
    int total_products    = query_int(conn,
        "SELECT COUNT(*) FROM inventory", 0);
    int total_categories  = query_int(conn,
        "SELECT COUNT(DISTINCT category) FROM inventory", 0);
    int total_stock_qty   = query_int(conn,
        "SELECT COALESCE(SUM(stock_quantity), 0) FROM inventory", 0);
    double total_stock_value = query_double(conn,
        "SELECT COALESCE(SUM(stock_quantity * unit_price), 0) FROM inventory", 0.0);

    /* ---------- 构建库存汇总 JSON ---------- */
    JsonBuf jb_inv;
    jb_init(&jb_inv);
    jb_append(&jb_inv, "{");
    jb_append_int(&jb_inv, "total_products", total_products);
    jb_append(&jb_inv, ",");
    jb_append_int(&jb_inv, "total_categories", total_categories);
    jb_append(&jb_inv, ",");
    jb_append_float(&jb_inv, "total_stock_value", total_stock_value);
    jb_append(&jb_inv, ",");
    jb_append_int(&jb_inv, "total_stock_quantity", total_stock_qty);
    jb_append(&jb_inv, "}");

    /* ================================================================
     * 第二部分：销售汇总
     *
     * 分别统计当日、本周、当月、全部的销售总额与销售笔数
     * ================================================================ */

    /* 当日销售 */
    double today_amount = query_double(conn,
        "SELECT COALESCE(SUM(total_amount), 0) FROM sales "
        "WHERE sale_date = CURDATE()", 0.0);
    int today_count = query_int(conn,
        "SELECT COUNT(*) FROM sales WHERE sale_date = CURDATE()", 0);

    /* 本周销售（使用 YEARWEEK 函数，mode=1 表示周一为一周起始） */
    double week_amount = query_double(conn,
        "SELECT COALESCE(SUM(total_amount), 0) FROM sales "
        "WHERE YEARWEEK(sale_date, 1) = YEARWEEK(CURDATE(), 1)", 0.0);
    int week_count = query_int(conn,
        "SELECT COUNT(*) FROM sales "
        "WHERE YEARWEEK(sale_date, 1) = YEARWEEK(CURDATE(), 1)", 0);

    /* 本月销售 */
    double month_amount = query_double(conn,
        "SELECT COALESCE(SUM(total_amount), 0) FROM sales "
        "WHERE MONTH(sale_date) = MONTH(CURDATE()) "
        "AND YEAR(sale_date) = YEAR(CURDATE())", 0.0);
    int month_count = query_int(conn,
        "SELECT COUNT(*) FROM sales "
        "WHERE MONTH(sale_date) = MONTH(CURDATE()) "
        "AND YEAR(sale_date) = YEAR(CURDATE())", 0);

    /* 全部销售汇总 */
    double total_amount = query_double(conn,
        "SELECT COALESCE(SUM(total_amount), 0) FROM sales", 0.0);
    int total_sales_count = query_int(conn,
        "SELECT COUNT(*) FROM sales", 0);

    /* ---------- 构建销售汇总 JSON ---------- */
    JsonBuf jb_sales;
    jb_init(&jb_sales);
    jb_append(&jb_sales, "{");
    jb_append_float(&jb_sales, "today_amount", today_amount);
    jb_append(&jb_sales, ",");
    jb_append_int(&jb_sales, "today_count", today_count);
    jb_append(&jb_sales, ",");
    jb_append_float(&jb_sales, "week_amount", week_amount);
    jb_append(&jb_sales, ",");
    jb_append_int(&jb_sales, "week_count", week_count);
    jb_append(&jb_sales, ",");
    jb_append_float(&jb_sales, "month_amount", month_amount);
    jb_append(&jb_sales, ",");
    jb_append_int(&jb_sales, "month_count", month_count);
    jb_append(&jb_sales, ",");
    jb_append_float(&jb_sales, "total_amount", total_amount);
    jb_append(&jb_sales, ",");
    jb_append_int(&jb_sales, "total_count", total_sales_count);
    jb_append(&jb_sales, "}");

    /* ================================================================
     * 第三部分：进货汇总
     *
     * 统计当月进货总成本和当月/全部进货记录数
     * ================================================================ */

    /* 当月进货成本 */
    double month_purchase_cost = query_double(conn,
        "SELECT COALESCE(SUM(total_cost), 0) FROM purchases "
        "WHERE MONTH(purchase_date) = MONTH(CURDATE()) "
        "AND YEAR(purchase_date) = YEAR(CURDATE())", 0.0);

    /* 当月进货记录数 */
    int month_purchase_count = query_int(conn,
        "SELECT COUNT(*) FROM purchases "
        "WHERE MONTH(purchase_date) = MONTH(CURDATE()) "
        "AND YEAR(purchase_date) = YEAR(CURDATE())", 0);

    /* 全部进货记录数 */
    int total_purchase_count = query_int(conn,
        "SELECT COUNT(*) FROM purchases", 0);

    /* ---------- 构建进货汇总 JSON ---------- */
    JsonBuf jb_purchase;
    jb_init(&jb_purchase);
    jb_append(&jb_purchase, "{");
    jb_append_float(&jb_purchase, "month_cost", month_purchase_cost);
    jb_append(&jb_purchase, ",");
    jb_append_int(&jb_purchase, "month_count", month_purchase_count);
    jb_append(&jb_purchase, ",");
    jb_append_int(&jb_purchase, "total_count", total_purchase_count);
    jb_append(&jb_purchase, "}");

    /* ================================================================
     * 第四部分：销售排行 TOP10
     *
     * 按销售数量降序排列，取前 10 名，包含排名、商品编号、名称、
     * 销售总数量和销售总金额
     * ================================================================ */

    JsonBuf jb_ranking;
    jb_init(&jb_ranking);
    jb_append(&jb_ranking, "[");

    MYSQL_RES *ranking_res = db_query(conn,
        "SELECT product_code, product_name, SUM(quantity) as total_sold, "
        "SUM(total_amount) as total_amount "
        "FROM sales GROUP BY product_code, product_name "
        "ORDER BY total_sold DESC LIMIT 10");

    if (ranking_res) {
        int rank = 0;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(ranking_res))) {
            rank++;
            if (rank > 1) jb_append(&jb_ranking, ",");
            jb_append(&jb_ranking, "{");
            jb_append_int(&jb_ranking, "rank", rank);
            jb_append(&jb_ranking, ",");
            jb_append_kv(&jb_ranking, "product_code", row[0] ? row[0] : "");
            jb_append(&jb_ranking, ",");
            jb_append_kv(&jb_ranking, "product_name", row[1] ? row[1] : "");
            jb_append(&jb_ranking, ",");
            jb_append_int(&jb_ranking, "total_sold", row[2] ? atoi(row[2]) : 0);
            jb_append(&jb_ranking, ",");
            jb_append_float(&jb_ranking, "total_amount", row[3] ? atof(row[3]) : 0.0);
            jb_append(&jb_ranking, "}");
        }
        db_free_result(ranking_res);
    }
    jb_append(&jb_ranking, "]");

    /* ================================================================
     * 第五部分：库存预警
     *
     * 查询所有库存量 <= 安全库存的商品，并根据库存量是否
     * <= 预警库存区分告警等级：
     *   - "danger"  : stock_quantity <= warning_stock（紧急告警）
     *   - "warning" : stock_quantity <= safety_stock（常规预警）
     * ================================================================ */

    JsonBuf jb_warnings;
    jb_init(&jb_warnings);
    jb_append(&jb_warnings, "[");

    MYSQL_RES *warn_res = db_query(conn,
        "SELECT product_code, product_name, stock_quantity, "
        "safety_stock, warning_stock "
        "FROM inventory WHERE stock_quantity <= safety_stock "
        "ORDER BY stock_quantity ASC");

    if (warn_res) {
        int first = 1;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(warn_res))) {
            if (!first) jb_append(&jb_warnings, ",");
            first = 0;

            int stock_qty   = row[2] ? atoi(row[2]) : 0;
            int safety      = row[3] ? atoi(row[3]) : 0;
            int warning_val = row[4] ? atoi(row[4]) : 0;

            /* 判断告警等级 */
            const char *level;
            if (stock_qty <= warning_val) {
                level = "danger";
            } else {
                level = "warning";
            }

            jb_append(&jb_warnings, "{");
            jb_append_kv(&jb_warnings, "product_code", row[0] ? row[0] : "");
            jb_append(&jb_warnings, ",");
            jb_append_kv(&jb_warnings, "product_name", row[1] ? row[1] : "");
            jb_append(&jb_warnings, ",");
            jb_append_int(&jb_warnings, "stock_quantity", stock_qty);
            jb_append(&jb_warnings, ",");
            jb_append_int(&jb_warnings, "safety_stock", safety);
            jb_append(&jb_warnings, ",");
            jb_append_int(&jb_warnings, "warning_stock", warning_val);
            jb_append(&jb_warnings, ",");
            jb_append_kv(&jb_warnings, "level", level);
            jb_append(&jb_warnings, "}");
        }
        db_free_result(warn_res);
    }
    jb_append(&jb_warnings, "]");

    /* ================================================================
     * 组装最终 data JSON
     *
     * 将上述五个子部分拼入同一个 JSON 对象中
     * ================================================================ */

    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append(&jb_data, "\"inventory_summary\":%s", jb_get(&jb_inv));
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"sales_summary\":%s", jb_get(&jb_sales));
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"purchase_summary\":%s", jb_get(&jb_purchase));
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"sales_ranking\":%s", jb_get(&jb_ranking));
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"inventory_warnings\":%s", jb_get(&jb_warnings));
    jb_append(&jb_data, "}");

    /* ---------- 构造最终响应 ---------- */
    char *resp = json_response(200, "统计概览查询成功", jb_get(&jb_data));

    /* ---------- 释放所有临时缓冲区 ---------- */
    jb_free(&jb_inv);
    jb_free(&jb_sales);
    jb_free(&jb_purchase);
    jb_free(&jb_ranking);
    jb_free(&jb_warnings);
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}

/* ================================================================
 * 2. handle_stats_eraser —— 橡皮类产品近 7 天销售统计
 *    GET /api/stats/eraser
 *
 * 返回格式：
 *   {
 *     "category": "橡皮类",
 *     "period": {"start": "YYYY-MM-DD", "end": "YYYY-MM-DD"},
 *     "weekly_sales_total": 285.00,
 *     "weekly_sales_count": 42,
 *     "products": [
 *       {product_code, product_name, current_stock, weekly_sold,
 *        weekly_amount, unit_price},
 *       ...
 *     ]
 *   }
 *
 * 通过 LEFT JOIN 将 inventory 与 sales 表关联，一次查询获取
 * 每个橡皮类产品的当前库存和周期内销售数据
 * ================================================================ */

void handle_stats_eraser(struct mg_connection *c, struct mg_http_message *hm)
{
    (void)hm; /* 该接口不使用查询参数，消除编译警告 */

    /* ---------- 计算日期范围：7 天前 ~ 今天 ---------- */
    char start_date[STR_BUF_SIZE] = {0};
    char end_date[STR_BUF_SIZE]   = {0};
    get_date_days_ago(start_date, sizeof(start_date), 6);
    get_today_date(end_date, sizeof(end_date));

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_error(c, 500, err);
        return;
    }

    /* ---------- 转义日期参数（防 SQL 注入） ---------- */
    char esc_start[STR_BUF_SIZE * 2 + 1] = {0};
    char esc_end[STR_BUF_SIZE * 2 + 1]   = {0};
    mysql_real_escape_string(conn, esc_start, start_date,
                             (unsigned long)strlen(start_date));
    mysql_real_escape_string(conn, esc_end, end_date,
                             (unsigned long)strlen(end_date));

    /* ================================================================
     * 查询橡皮类产品在周期内的销售总额和总笔数
     * ================================================================ */

    char weekly_sql[SQL_BUF_SIZE];
    snprintf(weekly_sql, sizeof(weekly_sql),
             "SELECT COALESCE(SUM(total_amount), 0), COUNT(*) FROM sales "
             "WHERE category = '橡皮类' "
             "AND sale_date BETWEEN '%s' AND '%s'",
             esc_start, esc_end);

    double weekly_sales_total = 0.0;
    int    weekly_sales_count = 0;

    MYSQL_RES *weekly_res = db_query(conn, weekly_sql);
    if (weekly_res) {
        MYSQL_ROW row = mysql_fetch_row(weekly_res);
        if (row) {
            if (row[0]) weekly_sales_total = atof(row[0]);
            if (row[1]) weekly_sales_count = atoi(row[1]);
        }
        db_free_result(weekly_res);
    }

    /* ================================================================
     * 查询每个橡皮类产品的明细：
     *   当前库存（from inventory）
     *   周期内销售量、销售额（LEFT JOIN sales）
     *
     * LEFT JOIN 确保即使某产品在周期内无销售记录也会被列出
     * ================================================================ */

    JsonBuf jb_products;
    jb_init(&jb_products);
    jb_append(&jb_products, "[");

    char detail_sql[SQL_BUF_SIZE];
    snprintf(detail_sql, sizeof(detail_sql),
             "SELECT i.product_code, i.product_name, i.stock_quantity, "
             "i.unit_price, "
             "COALESCE(SUM(s.quantity), 0) AS weekly_sold, "
             "COALESCE(SUM(s.total_amount), 0) AS weekly_amount "
             "FROM inventory i "
             "LEFT JOIN sales s ON i.product_code = s.product_code "
             "AND s.sale_date BETWEEN '%s' AND '%s' "
             "WHERE i.category = '橡皮类' "
             "GROUP BY i.product_code, i.product_name, "
             "i.stock_quantity, i.unit_price "
             "ORDER BY i.product_code ASC",
             esc_start, esc_end);

    MYSQL_RES *detail_res = db_query(conn, detail_sql);

    if (detail_res) {
        int first = 1;
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(detail_res))) {
            if (!first) jb_append(&jb_products, ",");
            first = 0;

            jb_append(&jb_products, "{");
            /* product_code   -> row[0] */
            jb_append_kv(&jb_products, "product_code", row[0] ? row[0] : "");
            jb_append(&jb_products, ",");
            /* product_name   -> row[1] */
            jb_append_kv(&jb_products, "product_name", row[1] ? row[1] : "");
            jb_append(&jb_products, ",");
            /* current_stock  -> row[2] */
            jb_append_int(&jb_products, "current_stock", row[2] ? atoi(row[2]) : 0);
            jb_append(&jb_products, ",");
            /* unit_price     -> row[3] */
            jb_append_float(&jb_products, "unit_price", row[3] ? atof(row[3]) : 0.0);
            jb_append(&jb_products, ",");
            /* weekly_sold    -> row[4] */
            jb_append_int(&jb_products, "weekly_sold", row[4] ? atoi(row[4]) : 0);
            jb_append(&jb_products, ",");
            /* weekly_amount  -> row[5] */
            jb_append_float(&jb_products, "weekly_amount", row[5] ? atof(row[5]) : 0.0);
            jb_append(&jb_products, "}");
        }
        db_free_result(detail_res);
    }
    jb_append(&jb_products, "]");

    /* ================================================================
     * 组装最终 data JSON
     * ================================================================ */

    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    /* 分类名称 */
    jb_append_kv(&jb_data, "category", "橡皮类");
    jb_append(&jb_data, ",");
    /* 统计周期 */
    jb_append(&jb_data, "\"period\":{");
    jb_append_kv(&jb_data, "start", start_date);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "end", end_date);
    jb_append(&jb_data, "}");
    jb_append(&jb_data, ",");
    /* 周期内销售汇总 */
    jb_append_float(&jb_data, "weekly_sales_total", weekly_sales_total);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "weekly_sales_count", weekly_sales_count);
    jb_append(&jb_data, ",");
    /* 产品明细列表 */
    jb_append(&jb_data, "\"products\":%s", jb_get(&jb_products));
    jb_append(&jb_data, "}");

    /* ---------- 构造最终响应 ---------- */
    char *resp = json_response(200, "橡皮类专项统计查询成功", jb_get(&jb_data));

    /* ---------- 释放所有临时缓冲区 ---------- */
    jb_free(&jb_products);
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}
