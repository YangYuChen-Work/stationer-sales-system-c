/**
 * @file    search.c
 * @brief   多条件组合查询模块 —— 跨 inventory / sales / purchases 三表的统一查询
 *
 * 本模块提供一个统一的多条件组合查询接口，可根据 type 参数自动选择
 * 目标数据表（库存 / 销售 / 进货），然后动态组合所有提供的筛选条件
 * （关键字、精确匹配、日期范围、价格区间、数量区间等），返回分页 JSON 结果。
 *
 * 依赖：mongoose.h（HTTP 服务）、db.h（数据库操作）、json.h（JSON 构造）
 */

#include "search.h"
#include "db.h"
#include "json.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <math.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** HTTP 响应头：JSON 内容类型 + CORS 跨域许可 */
#define CORS_HEADERS \
    "Content-Type: application/json; charset=utf-8\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/** 单条 SQL 语句缓冲区最大字节数 */
#define SQL_BUF_SIZE    8192

/** 查询参数字符串缓冲区最大字节数 */
#define PARAM_BUF_SIZE  256

/* ================================================================
 * 内部辅助函数 —— HTTP 响应
 * ================================================================ */

/**
 * 发送 JSON 错误响应
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
 * 发送 JSON 成功响应（使用 json_response 包装 data）
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

/* ================================================================
 * 内部辅助函数 —— 参数提取
 * ================================================================ */

/**
 * 从 Mongoose HTTP 查询参数中提取字符串值
 * 若参数不存在或为空，dst 保持为空字符串
 */
static void get_param(struct mg_http_message *hm, const char *name,
                      char *dst, int dst_size)
{
    int ret = mg_http_get_var(&hm->query, name, dst, dst_size);
    if (ret <= 0) {
        dst[0] = '\0';
    }
}

/**
 * 从查询参数中提取整数值，不存在或解析失败返回默认值
 */
static int get_param_int(struct mg_http_message *hm, const char *name,
                         int default_val)
{
    char buf[64] = {0};
    get_param(hm, name, buf, sizeof(buf));
    if (buf[0] == '\0') return default_val;
    int val = atoi(buf);
    return (val > 0) ? val : default_val;
}

/**
 * 从查询参数中提取浮点数值，不存在或解析失败返回默认值
 */
static double get_param_double(struct mg_http_message *hm, const char *name,
                               double default_val)
{
    char buf[64] = {0};
    get_param(hm, name, buf, sizeof(buf));
    if (buf[0] == '\0') return default_val;
    return atof(buf);
}

/* ================================================================
 * 内部辅助函数 —— JSON 行序列化
 *
 * 根据查询类型（type），将数据库行数据追加为 JSON 对象。
 * 不同表的字段结构不同，需要分别处理。
 * ================================================================ */

/**
 * 将一条 inventory 表查询结果追加为 JSON 对象
 * inventory 字段顺序：
 *   0:id  1:product_code  2:product_name  3:category
 *   4:manufacturer  5:model  6:stock_quantity  7:unit_price
 *   8:safety_stock  9:warning_stock  10:created_at  11:updated_at
 */
static void append_inventory_row(JsonBuf *jb, MYSQL_ROW row)
{
    jb_append(jb, "{");
    jb_append_int(jb,   "id",             row[0]  ? atoi(row[0])  : 0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_code",   row[1]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_name",   row[2]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "category",       row[3]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "manufacturer",   row[4]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "model",          row[5]);
    jb_append(jb, ",");
    jb_append_int(jb,   "stock_quantity", row[6]  ? atoi(row[6])  : 0);
    jb_append(jb, ",");
    jb_append_float(jb, "unit_price",     row[7]  ? atof(row[7])  : 0.0);
    jb_append(jb, ",");
    jb_append_int(jb,   "safety_stock",   row[8]  ? atoi(row[8])  : 0);
    jb_append(jb, ",");
    jb_append_int(jb,   "warning_stock",  row[9]  ? atoi(row[9])  : 0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "created_at",     row[10]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "updated_at",     row[11]);
    jb_append(jb, "}");
}

/**
 * 将一条 sales 表查询结果追加为 JSON 对象
 * sales 字段顺序：
 *   0:id  1:product_code  2:product_name  3:category
 *   4:sale_date  5:quantity  6:sale_price  7:total_amount  8:created_at
 */
static void append_sales_row(JsonBuf *jb, MYSQL_ROW row)
{
    jb_append(jb, "{");
    jb_append_int(jb,   "id",           row[0] ? atoi(row[0]) : 0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_code", row[1]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_name", row[2]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "category",     row[3]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "sale_date",    row[4]);
    jb_append(jb, ",");
    jb_append_int(jb,   "quantity",     row[5] ? atoi(row[5]) : 0);
    jb_append(jb, ",");
    jb_append_float(jb, "sale_price",   row[6] ? atof(row[6]) : 0.0);
    jb_append(jb, ",");
    jb_append_float(jb, "total_amount", row[7] ? atof(row[7]) : 0.0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "created_at",   row[8]);
    jb_append(jb, "}");
}

/**
 * 将一条 purchases 表查询结果追加为 JSON 对象
 * purchases 字段顺序：
 *   0:id  1:product_code  2:product_name  3:category
 *   4:quantity  5:unit_price  6:total_cost  7:purchase_date  8:created_at
 */
static void append_purchases_row(JsonBuf *jb, MYSQL_ROW row)
{
    jb_append(jb, "{");
    jb_append_int(jb,   "id",            row[0] ? atoi(row[0]) : 0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_code",  row[1]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "product_name",  row[2]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "category",      row[3]);
    jb_append(jb, ",");
    jb_append_int(jb,   "quantity",      row[4] ? atoi(row[4]) : 0);
    jb_append(jb, ",");
    jb_append_float(jb, "unit_price",    row[5] ? atof(row[5]) : 0.0);
    jb_append(jb, ",");
    jb_append_float(jb, "total_cost",    row[6] ? atof(row[6]) : 0.0);
    jb_append(jb, ",");
    jb_append_kv(jb,    "purchase_date", row[7]);
    jb_append(jb, ",");
    jb_append_kv(jb,    "created_at",    row[8]);
    jb_append(jb, "}");
}

/* ================================================================
 * 核心处理函数：handle_search
 *
 * 处理逻辑：
 *   1. 解析 type 查询参数，确定目标表（inventory / sales / purchases）
 *   2. 解析所有可选筛选参数
 *   3. 根据目标表动态构建 WHERE 子句
 *   4. 执行 COUNT 查询获取分页总数
 *   5. 执行主查询获取当前页数据（LIMIT / OFFSET）
 *   6. 构造包含 conditions 和 list 的分页 JSON 响应
 *   7. 发送响应并清理资源
 *
 * 支持的查询参数：
 *   type           required  目标表：inventory / sales / purchases
 *   keyword        可选      模糊匹配 product_name 或 product_code
 *   product_code   可选      精确匹配
 *   category       可选      精确匹配
 *   manufacturer   可选      精确匹配（仅 inventory）
 *   start_date     可选      日期起始（sales: sale_date, purchases: purchase_date）
 *   end_date       可选      日期结束
 *   min_price      可选      最低价格（inventory: unit_price, sales: sale_price, purchases: unit_price）
 *   max_price      可选      最高价格
 *   min_quantity   可选      最低数量（inventory: stock_quantity, sales/purchases: quantity）
 *   max_quantity   可选      最高数量
 *   page           可选      页码，默认 1
 *   page_size      可选      每页条数，默认 20，最大 100
 * ================================================================ */

void handle_search(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 1. 解析查询参数 ---------- */

    /* 必填参数：查询类型 */
    char type[PARAM_BUF_SIZE] = {0};
    get_param(hm, "type", type, sizeof(type));

    /* 通用筛选参数 */
    char keyword[PARAM_BUF_SIZE]      = {0};
    char product_code[PARAM_BUF_SIZE] = {0};
    char category[PARAM_BUF_SIZE]     = {0};
    char manufacturer[PARAM_BUF_SIZE] = {0};
    char start_date[PARAM_BUF_SIZE]   = {0};
    char end_date[PARAM_BUF_SIZE]     = {0};

    get_param(hm, "keyword",      keyword,      sizeof(keyword));
    get_param(hm, "product_code", product_code, sizeof(product_code));
    get_param(hm, "category",     category,     sizeof(category));
    get_param(hm, "manufacturer", manufacturer, sizeof(manufacturer));
    get_param(hm, "start_date",   start_date,   sizeof(start_date));
    get_param(hm, "end_date",     end_date,     sizeof(end_date));

    /* 数值区间参数（-1 表示未提供） */
    double min_price     = get_param_double(hm, "min_price", -1.0);
    double max_price     = get_param_double(hm, "max_price", -1.0);
    int    min_quantity  = get_param_int(hm, "min_quantity", -1);
    int    max_quantity  = get_param_int(hm, "max_quantity", -1);

    /* 分页参数 */
    int page      = get_param_int(hm, "page", 1);
    int page_size = get_param_int(hm, "page_size", 20);
    if (page < 1)       page = 1;
    if (page_size < 1)  page_size = 1;
    if (page_size > 100) page_size = 100;

    /* ---------- 2. 校验 type 参数 ---------- */

    if (type[0] == '\0') {
        send_error(c, 400, 400,
                   "缺少必填参数 type（可选值：inventory / sales / purchases）");
        return;
    }

    int is_inventory = (strcmp(type, "inventory") == 0);
    int is_sales     = (strcmp(type, "sales") == 0);
    int is_purchases = (strcmp(type, "purchases") == 0);

    if (!is_inventory && !is_sales && !is_purchases) {
        send_error(c, 400, 400,
                   "type 参数无效，可选值：inventory / sales / purchases");
        return;
    }

    /* ---------- 3. 获取数据库连接 ---------- */

    MYSQL *conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 4. 转义所有字符串参数（防 SQL 注入） ---------- */

    char esc_keyword[PARAM_BUF_SIZE * 2 + 1]      = {0};
    char esc_product_code[PARAM_BUF_SIZE * 2 + 1] = {0};
    char esc_category[PARAM_BUF_SIZE * 2 + 1]     = {0};
    char esc_manufacturer[PARAM_BUF_SIZE * 2 + 1] = {0};
    char esc_start_date[PARAM_BUF_SIZE * 2 + 1]   = {0};
    char esc_end_date[PARAM_BUF_SIZE * 2 + 1]     = {0};

    if (keyword[0]) {
        mysql_real_escape_string(conn, esc_keyword, keyword,
                                 (unsigned long)strlen(keyword));
    }
    if (product_code[0]) {
        mysql_real_escape_string(conn, esc_product_code, product_code,
                                 (unsigned long)strlen(product_code));
    }
    if (category[0]) {
        mysql_real_escape_string(conn, esc_category, category,
                                 (unsigned long)strlen(category));
    }
    if (manufacturer[0]) {
        mysql_real_escape_string(conn, esc_manufacturer, manufacturer,
                                 (unsigned long)strlen(manufacturer));
    }
    if (start_date[0]) {
        mysql_real_escape_string(conn, esc_start_date, start_date,
                                 (unsigned long)strlen(start_date));
    }
    if (end_date[0]) {
        mysql_real_escape_string(conn, esc_end_date, end_date,
                                 (unsigned long)strlen(end_date));
    }

    /* ---------- 5. 动态构建 WHERE 子句 ---------- */

    /* 根据 type 确定表名及字段映射 */
    const char *table_name;
    const char *date_field;   /* 日期字段（inventory 无） */
    const char *price_field;  /* 价格字段 */
    const char *qty_field;    /* 数量字段 */

    if (is_inventory) {
        table_name  = "inventory";
        date_field  = NULL;            /* inventory 表无日期字段 */
        price_field = "unit_price";
        qty_field   = "stock_quantity";
    } else if (is_sales) {
        table_name  = "sales";
        date_field  = "sale_date";
        price_field = "sale_price";
        qty_field   = "quantity";
    } else { /* purchases */
        table_name  = "purchases";
        date_field  = "purchase_date";
        price_field = "unit_price";
        qty_field   = "quantity";
    }

    /* 拼接 WHERE 条件（从 WHERE 1=1 开始，方便后续 AND 拼接） */
    char where_sql[SQL_BUF_SIZE];
    int  where_len = 0;

    where_len += snprintf(where_sql + where_len,
                          sizeof(where_sql) - where_len,
                          "WHERE 1=1");

    /* keyword —— 模糊匹配 product_name 或 product_code */
    if (esc_keyword[0]) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND (product_name LIKE '%%%s%%' "
                              "OR product_code LIKE '%%%s%%')",
                              esc_keyword, esc_keyword);
    }

    /* product_code —— 精确匹配 */
    if (esc_product_code[0]) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND product_code='%s'",
                              esc_product_code);
    }

    /* category —— 精确匹配 */
    if (esc_category[0]) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND category='%s'",
                              esc_category);
    }

    /* manufacturer —— 精确匹配（仅 inventory 表有此字段） */
    if (esc_manufacturer[0] && is_inventory) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND manufacturer='%s'",
                              esc_manufacturer);
    }

    /* 日期范围过滤（sales: sale_date, purchases: purchase_date） */
    if (date_field != NULL && esc_start_date[0]) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s >= '%s'",
                              date_field, esc_start_date);
    }
    if (date_field != NULL && esc_end_date[0]) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s <= '%s'",
                              date_field, esc_end_date);
    }

    /* 价格区间过滤 */
    if (min_price >= 0) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s >= %.2f",
                              price_field, min_price);
    }
    if (max_price >= 0) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s <= %.2f",
                              price_field, max_price);
    }

    /* 数量区间过滤 */
    if (min_quantity >= 0) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s >= %d",
                              qty_field, min_quantity);
    }
    if (max_quantity >= 0) {
        where_len += snprintf(where_sql + where_len,
                              sizeof(where_sql) - where_len,
                              " AND %s <= %d",
                              qty_field, max_quantity);
    }

    /* ---------- 6. 执行 COUNT 查询（分页总数） ---------- */

    char count_sql[SQL_BUF_SIZE];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM %s %s", table_name, where_sql);

    int total = 0;
    MYSQL_RES *result = db_query(conn, count_sql);
    if (result != NULL) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row && row[0]) {
            total = atoi(row[0]);
        }
        db_free_result(result);
        result = NULL;
    } else {
        /* COUNT 查询失败是严重错误 */
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }

    /* ---------- 7. 执行主查询（分页数据） ---------- */

    int total_pages = (total > 0) ? (int)ceil((double)total / page_size) : 1;
    if (page > total_pages) page = total_pages;
    int offset = (page - 1) * page_size;

    /* 根据 type 选择 SELECT 字段列表 */
    const char *select_fields;

    if (is_inventory) {
        select_fields =
            "id, product_code, product_name, category, "
            "manufacturer, model, stock_quantity, unit_price, "
            "safety_stock, warning_stock, created_at, updated_at";
    } else if (is_sales) {
        select_fields =
            "id, product_code, product_name, category, "
            "sale_date, quantity, sale_price, total_amount, created_at";
    } else { /* purchases */
        select_fields =
            "id, product_code, product_name, category, "
            "quantity, unit_price, total_cost, purchase_date, created_at";
    }

    char data_sql[SQL_BUF_SIZE];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT %s FROM %s %s ORDER BY id DESC LIMIT %d OFFSET %d",
             select_fields, table_name, where_sql, page_size, offset);

    result = db_query(conn, data_sql);
    if (result == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }

    /* ---------- 8. 构造 conditions JSON 对象 ---------- */

    JsonBuf cond_jb;
    jb_init(&cond_jb);
    jb_append(&cond_jb, "{");

    /* type 始终包含 */
    jb_append_kv(&cond_jb, "type", type);

    /* 仅包含实际使用的非空参数 */
    if (keyword[0]) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "keyword", keyword);
    }
    if (product_code[0]) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "product_code", product_code);
    }
    if (category[0]) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "category", category);
    }
    if (manufacturer[0] && is_inventory) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "manufacturer", manufacturer);
    }
    if (start_date[0]) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "start_date", start_date);
    }
    if (end_date[0]) {
        jb_append(&cond_jb, ",");
        jb_append_kv(&cond_jb, "end_date", end_date);
    }
    if (min_price >= 0) {
        jb_append(&cond_jb, ",");
        jb_append_float(&cond_jb, "min_price", min_price);
    }
    if (max_price >= 0) {
        jb_append(&cond_jb, ",");
        jb_append_float(&cond_jb, "max_price", max_price);
    }
    if (min_quantity >= 0) {
        jb_append(&cond_jb, ",");
        jb_append_int(&cond_jb, "min_quantity", min_quantity);
    }
    if (max_quantity >= 0) {
        jb_append(&cond_jb, ",");
        jb_append_int(&cond_jb, "max_quantity", max_quantity);
    }

    jb_append(&cond_jb, "}");
    const char *conditions_json = jb_get(&cond_jb);

    /* ---------- 9. 构造 list 数组 ---------- */

    JsonBuf list_jb;
    jb_init(&list_jb);
    jb_append(&list_jb, "[");

    int first = 1;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {
        if (!first) {
            jb_append(&list_jb, ",");
        }
        first = 0;

        if (is_inventory) {
            append_inventory_row(&list_jb, row);
        } else if (is_sales) {
            append_sales_row(&list_jb, row);
        } else {
            append_purchases_row(&list_jb, row);
        }
    }
    jb_append(&list_jb, "]");
    const char *list_json = jb_get(&list_jb);

    /* ---------- 10. 构造完整 data JSON ---------- */

    JsonBuf data_jb;
    jb_init(&data_jb);
    jb_append(&data_jb, "{");
    jb_append_int(&data_jb, "total", total);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "page", page);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "page_size", page_size);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "total_pages", total_pages);
    jb_append(&data_jb, ",");
    jb_append(&data_jb, "\"conditions\":%s", conditions_json);
    jb_append(&data_jb, ",");
    jb_append(&data_jb, "\"list\":%s", list_json);
    jb_append(&data_jb, "}");

    /* ---------- 11. 发送响应 ---------- */

    send_response(c, 200, 200, "查询成功", jb_get(&data_jb));

    /* ---------- 12. 清理资源 ---------- */

    jb_free(&list_jb);
    jb_free(&cond_jb);
    jb_free(&data_jb);
    db_free_result(result);
    db_release_connection(conn);
}
