/**
 * @file    suggest.c
 * @brief   自动补货建议模块 —— 基于 30 天销售速度计算采购建议
 *
 * 本模块为文具店销售管理系统提供自动补货建议功能：
 *   对库存中的每件商品（可选按分类筛选），计算过去 30 天的日均销量，
 *   再根据公式「日均销量 x 补货周期(30天) x 安全系数(1.2) - 当前库存」
 *   得出建议采购量，并标注紧急程度供采购决策参考。
 *
 * 依赖：mongoose.h（HTTP 服务）、db.h（数据库操作）、json.h（JSON 构造）
 */

#include "suggest.h"
#include "db.h"
#include "json.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** HTTP 响应头：JSON 内容类型 + CORS 跨域许可 */
#define CORS_HEADERS \
    "Content-Type: application/json\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/** 单条 SQL 语句缓冲区最大字节数 */
#define SQL_BUF_SIZE    2048

/** 补货周期（天） */
#define RESTOCK_CYCLE   30

/** 安全系数（库存缓冲倍数） */
#define SAFETY_FACTOR   1.2

/** 最小日均销量（避免除以零） */
#define MIN_DAILY_SALES 0.01

/* ================================================================
 * 辅助宏：浮点数舍入到 2 位小数
 * 使用 round(val * 100) / 100 避免 printf 格式化精度问题
 * ================================================================ */
#define ROUND2(val)  (round((val) * 100.0) / 100.0)

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * 发送 JSON 错误响应（使用 json_error + mg_http_reply）
 *
 * @param c           Mongoose 连接
 * @param status_code HTTP 状态码
 * @param code        业务错误码
 * @param message     错误提示信息
 */
static void send_error(struct mg_connection *c, int status_code,
                       int code, const char *message)
{
    char *resp = json_error(code, message);
    if (resp) {
        mg_http_reply(c, status_code, CORS_HEADERS, "%s", resp);
        free(resp);
    }
}

/**
 * 发送 JSON 成功响应
 *
 * @param c           Mongoose 连接
 * @param status_code HTTP 状态码
 * @param code        业务状态码
 * @param message     提示信息
 * @param data_json   数据 JSON 片段（可为 NULL）
 */
static void send_response(struct mg_connection *c, int status_code,
                          int code, const char *message, const char *data_json)
{
    char *resp = json_response(code, message, data_json);
    if (resp) {
        mg_http_reply(c, status_code, CORS_HEADERS, "%s", resp);
        free(resp);
    }
}

/**
 * 查询指定商品在过去 30 天内的总销量
 *
 * @param conn         数据库连接
 * @param product_code 商品编号
 * @return             30 天内销量总和，查询失败返回 0
 */
static double get_sales_30day(MYSQL *conn, const char *product_code)
{
    /* 转义商品编号，防止 SQL 注入 */
    char escaped[128];
    mysql_real_escape_string(conn, escaped,
        product_code, (unsigned long)strlen(product_code));

    char sql[SQL_BUF_SIZE];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(SUM(quantity), 0) FROM sales "
             "WHERE product_code='%s' "
             "AND sale_date >= DATE_SUB(CURDATE(), INTERVAL 30 DAY)",
             escaped);

    double total = 0.0;
    MYSQL_RES *result = db_query(conn, sql);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && row[0]) {
            total = atof(row[0]);
        }
        db_free_result(result);
    }

    return total;
}

/**
 * 查询指定商品的平均进货单价（基于 purchases 表历史记录）
 *
 * @param conn         数据库连接
 * @param product_code 商品编号
 * @return             平均进货单价，若无进货记录则返回 0.0
 */
static double get_avg_purchase_price(MYSQL *conn, const char *product_code)
{
    /* 转义商品编号，防止 SQL 注入 */
    char escaped[128];
    mysql_real_escape_string(conn, escaped,
        product_code, (unsigned long)strlen(product_code));

    char sql[SQL_BUF_SIZE];
    snprintf(sql, sizeof(sql),
             "SELECT COALESCE(AVG(unit_price), 0) FROM purchases "
             "WHERE product_code='%s'",
             escaped);

    double avg_price = 0.0;
    MYSQL_RES *result = db_query(conn, sql);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && row[0]) {
            avg_price = atof(row[0]);
        }
        db_free_result(result);
    }

    return avg_price;
}

/**
 * 构建补货建议原因描述文本
 *
 * @param buf           输出缓冲区
 * @param buf_size      缓冲区容量
 * @param days_of_stock 当前库存可支撑天数
 * @param urgency       紧急程度（"high" / "medium" / "low"）
 */
static void build_reason(char *buf, int buf_size,
                         double days_of_stock, const char *urgency)
{
    if (strcmp(urgency, "high") == 0) {
        snprintf(buf, buf_size,
                 "库存严重不足，仅够 %.1f 天销售", days_of_stock);
    } else if (strcmp(urgency, "medium") == 0) {
        snprintf(buf, buf_size,
                 "库存偏低，仅够 %.1f 天销售", days_of_stock);
    } else {
        snprintf(buf, buf_size,
                 "库存充足，可供 %.1f 天销售", days_of_stock);
    }
}

/* ================================================================
 *  handle_suggest_list —— 自动补货建议列表
 *  GET /api/suggestions?category=XXX
 *
 *  算法流程：
 *    1. 从 inventory 表查询所有商品（可选按 category 筛选）
 *    2. 对每件商品查询 30 天销售额，计算日均销量
 *    3. 计算建议采购量 = MAX(0, 日均销量 x 30 x 1.2 - 当前库存)
 *    4. 查询平均进货单价，计算预估成本
 *    5. 根据当前库存可支撑天数确定紧急程度
 *    6. 汇总为 JSON 数组返回
 * ================================================================ */

void handle_suggest_list(struct mg_connection *c, struct mg_http_message *hm)
{
    char category[128] = {0};
    MYSQL *conn = NULL;
    MYSQL_RES *inv_result = NULL;
    JsonBuf data_jb;
    JsonBuf list_jb;

    /* ---------- 解析查询参数 ---------- */
    mg_http_get_var(&hm->query, "category", category, sizeof(category));

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 构造查询库存商品的 SQL ---------- */
    char escaped_cat[256] = {0};
    if (category[0]) {
        /* 转义分类参数，防 SQL 注入 */
        mysql_real_escape_string(conn, escaped_cat, category,
                                 (unsigned long)strlen(category));
    }

    char inv_sql[SQL_BUF_SIZE];
    if (escaped_cat[0]) {
        snprintf(inv_sql, sizeof(inv_sql),
                 "SELECT product_code, product_name, category, "
                 "stock_quantity, unit_price "
                 "FROM inventory WHERE category='%s' "
                 "ORDER BY category, product_code",
                 escaped_cat);
    } else {
        snprintf(inv_sql, sizeof(inv_sql),
                 "SELECT product_code, product_name, category, "
                 "stock_quantity, unit_price "
                 "FROM inventory "
                 "ORDER BY category, product_code");
    }

    inv_result = db_query(conn, inv_sql);
    if (inv_result == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "查询库存数据失败");
        return;
    }

    /* ---------- 遍历每件商品，计算补货建议 ---------- */
    jb_init(&list_jb);
    jb_append(&list_jb, "[");

    int first = 1;
    int total_items = 0;
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(inv_result)) != NULL) {
        /* 解析字段：
         * 0: product_code  1: product_name  2: category
         * 3: stock_quantity  4: unit_price
         */
        const char *product_code  = row[0] ? row[0] : "";
        const char *product_name  = row[1] ? row[1] : "";
        const char *prod_category = row[2] ? row[2] : "";
        int   current_stock       = row[3] ? atoi(row[3]) : 0;
        double unit_price         = row[4] ? atof(row[4]) : 0.0;

        /* —— 步骤 1：查询最近 30 天总销量 —— */
        double total_sales = get_sales_30day(conn, product_code);

        /* —— 步骤 2：计算日均销量 —— */
        double daily_avg_sales = total_sales / 30.0;
        daily_avg_sales = ROUND2(daily_avg_sales);

        /* —— 步骤 3：计算建议采购量 ——
         *   suggested_qty = MAX(0, daily_avg_sales * 30 * 1.2 - current_stock)
         */
        double required = daily_avg_sales * RESTOCK_CYCLE * SAFETY_FACTOR;
        int suggested_qty = (int)(round(required - current_stock));
        if (suggested_qty < 0) {
            suggested_qty = 0;
        }

        /* —— 步骤 4：估算采购成本 ——
         *   优先使用进货表平均单价，若无进货记录则回退到库存表单价
         */
        double avg_purchase_price = get_avg_purchase_price(conn, product_code);
        double cost_price;
        if (avg_purchase_price > 0.0) {
            cost_price = avg_purchase_price;
        } else {
            cost_price = unit_price;
        }
        double estimated_cost = ROUND2(suggested_qty * cost_price);

        /* —— 步骤 5：计算紧急程度 ——
         *   days_of_stock = current_stock / MAX(daily_avg_sales, 0.01)
         *   "high"   : days_of_stock < 7
         *   "medium" : days_of_stock < 30
         *   "low"    : days_of_stock >= 30
         */
        double divisor = (daily_avg_sales > MIN_DAILY_SALES)
                         ? daily_avg_sales : MIN_DAILY_SALES;
        double days_of_stock = current_stock / divisor;
        days_of_stock = ROUND2(days_of_stock);

        const char *urgency;
        if (days_of_stock < 7.0) {
            urgency = "high";
        } else if (days_of_stock < 30.0) {
            urgency = "medium";
        } else {
            urgency = "low";
        }

        /* —— 步骤 6：构建原因描述文本 —— */
        char reason[256];
        build_reason(reason, sizeof(reason), days_of_stock, urgency);

        /* —— 追加 JSON 对象到列表 —— */
        if (!first) {
            jb_append(&list_jb, ",");
        }
        first = 0;
        total_items++;

        jb_append(&list_jb, "{");
        jb_append_kv(&list_jb,    "product_code",   product_code);
        jb_append(&list_jb, ",");
        jb_append_kv(&list_jb,    "product_name",   product_name);
        jb_append(&list_jb, ",");
        jb_append_kv(&list_jb,    "category",       prod_category);
        jb_append(&list_jb, ",");
        jb_append_int(&list_jb,   "current_stock",  current_stock);
        jb_append(&list_jb, ",");
        jb_append_float(&list_jb, "daily_avg_sales", daily_avg_sales);
        jb_append(&list_jb, ",");
        jb_append_int(&list_jb,   "suggested_quantity", suggested_qty);
        jb_append(&list_jb, ",");
        jb_append_float(&list_jb, "estimated_cost",  estimated_cost);
        jb_append(&list_jb, ",");
        jb_append_kv(&list_jb,    "urgency",         urgency);
        jb_append(&list_jb, ",");
        jb_append_kv(&list_jb,    "reason",          reason);
        jb_append(&list_jb, "}");
    }

    jb_append(&list_jb, "]");

    /* ---------- 构造 data JSON ---------- */
    jb_init(&data_jb);
    jb_append(&data_jb, "{");
    jb_append_kv(&data_jb, "algorithm",
                 "建议采购量 = 日均销量 x 补货周期(30天) x 安全系数(1.2) - 当前库存");
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "total_items", total_items);
    jb_append(&data_jb, ",");
    jb_append(&data_jb, "\"list\":%s", list_jb.data);
    jb_append(&data_jb, "}");

    /* ---------- 发送响应 ---------- */
    send_response(c, 200, 200, "查询成功", data_jb.data);

    /* ---------- 清理 ---------- */
    jb_free(&list_jb);
    jb_free(&data_jb);
    db_free_result(inv_result);
    db_release_connection(conn);
}
