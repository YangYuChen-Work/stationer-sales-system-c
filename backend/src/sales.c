/**
 * @file    sales.c
 * @brief   销售管理模块 —— 销售记录 CRUD 与结账流程
 *
 * 本模块提供四个 HTTP 处理函数，分别对应：
 *   1. 销售记录列表（分页 + 查询筛选）
 *   2. 按日期排序的销售记录（升序 / 降序）
 *   3. 新增单条销售记录（仅记录，不联动库存）
 *   4. 销售结账流程（事务保护：扣减库存 + 生成销售记录）
 *
 * 依赖：db.h, json.h, mongoose.h, MySQL C API
 */

#include "sales.h"
#include "db.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

#define MAX_BODY_LEN     65536
#define MAX_SQL_LEN      8192
#define MAX_STR_LEN      512
#define MAX_ITEMS        100
#define DEFAULT_PAGE_SIZE 20
#define MAX_PAGE_SIZE     100

/* ================================================================
 * 内部辅助函数 —— HTTP 响应
 * ================================================================ */

/**
 * 发送 JSON 响应并添加 CORS 头
 * 自动释放 json_str 内存
 */
static void send_json_resp(struct mg_connection *c, char *json_str)
{
    mg_http_reply(c, 200,
                  "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n",
                  "%s\n", json_str ? json_str : "{}");
    free(json_str);
}

/**
 * 发送带有自定义 HTTP 状态码的 JSON 响应
 * 用于返回 4xx / 5xx 错误
 */
static void send_json_resp_status(struct mg_connection *c, int http_status, char *json_str)
{
    mg_http_reply(c, http_status,
                  "Content-Type: application/json\r\n"
                  "Access-Control-Allow-Origin: *\r\n"
                  "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
                  "Access-Control-Allow-Headers: Content-Type\r\n",
                  "%s\n", json_str ? json_str : "{}");
    free(json_str);
}

/* ================================================================
 * 内部辅助函数 —— URL 查询参数提取
 * ================================================================ */

/**
 * 从 HTTP 消息的 query 字符串中提取指定参数值
 * 若参数不存在则返回默认值字符串
 */
static void get_query_param(struct mg_http_message *hm, const char *name,
                            char *buf, int buf_size, const char *default_val)
{
    int ret = mg_http_get_var(&hm->query, name, buf, buf_size);
    if (ret <= 0 || buf[0] == '\0') {
        if (default_val) {
            strncpy(buf, default_val, buf_size - 1);
            buf[buf_size - 1] = '\0';
        } else {
            buf[0] = '\0';
        }
    }
}

/**
 * 从查询参数中提取整数值，解析失败返回默认值
 */
static int get_query_param_int(struct mg_http_message *hm, const char *name, int default_val)
{
    char buf[64] = {0};
    get_query_param(hm, name, buf, sizeof(buf), NULL);
    if (buf[0] == '\0') return default_val;
    int val = atoi(buf);
    return (val > 0) ? val : default_val;
}

/* ================================================================
 * 内部辅助函数 —— JSON Body 简单解析
 *
 * 因项目无第三方 JSON 解析库，使用基于字符串查找的轻量解析器。
 * 仅支持本项目定义的简单 JSON 格式（单层对象，不含嵌套）。
 * ================================================================ */

/**
 * 从 JSON 字符串中提取指定 key 对应的字符串值
 * 例如 json={"product_code":"P001"} key="product_code" → out="P001"
 * 返回 1 表示找到，0 表示未找到
 */
static int json_get_string(const char *json, const char *key, char *out, int out_size)
{
    if (!json || !key || !out || out_size <= 0) return 0;
    out[0] = '\0';

    /* 构造搜索模式: "key" */
    char search[MAX_STR_LEN];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return 0;

    /* 跳过 key 部分，查找冒号后的第一个双引号 */
    pos = strchr(pos + strlen(search), ':');
    if (!pos) return 0;
    pos = strchr(pos, '"');
    if (!pos) return 0;
    pos++;  /* 跳过开头的双引号 */

    /* 找到值的结束双引号 */
    const char *end = strchr(pos, '"');
    if (!end) return 0;

    int len = (int)(end - pos);
    if (len >= out_size) len = out_size - 1;
    strncpy(out, pos, len);
    out[len] = '\0';

    return 1;
}

/**
 * 从 JSON 字符串中提取指定 key 对应的整数值
 * 返回 1 表示找到，0 表示未找到
 */
static int json_get_int(const char *json, const char *key, int *out)
{
    if (!json || !key || !out) return 0;
    *out = 0;

    char search[MAX_STR_LEN];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return 0;

    pos = strchr(pos + strlen(search), ':');
    if (!pos) return 0;
    pos++;  /* 跳过冒号 */

    /* 跳过空白字符 */
    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;

    /* 检查是否为 null */
    if (strncmp(pos, "null", 4) == 0) return 0;

    *out = atoi(pos);
    return 1;
}

/**
 * 从 JSON 字符串中提取指定 key 对应的浮点数值
 * 返回 1 表示找到，0 表示未找到
 */
static int json_get_double(const char *json, const char *key, double *out)
{
    if (!json || !key || !out) return 0;
    *out = 0.0;

    char search[MAX_STR_LEN];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return 0;

    pos = strchr(pos + strlen(search), ':');
    if (!pos) return 0;
    pos++;

    while (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r') pos++;

    if (strncmp(pos, "null", 4) == 0) return 0;

    *out = atof(pos);
    return 1;
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

/* ================================================================
 * 内部辅助函数 —— SQL 安全转义
 * ================================================================ */

/**
 * 对字符串进行 MySQL 转义，结果写入 out，返回转义后的长度
 */
static unsigned long mysql_escape(char *out, const char *src, MYSQL *conn)
{
    if (!src) {
        out[0] = '\0';
        return 0;
    }
    return mysql_real_escape_string(conn, out, src, (unsigned long)strlen(src));
}

/* ================================================================
 * 内部辅助函数 —— 构建销售记录的 JSON 对象
 * ================================================================ */

/**
 * 将一行销售查询结果追加为 JSON 对象
 * 在构建分页列表时调用
 */
static void append_sale_row_json(JsonBuf *jb, MYSQL_ROW row)
{
    jb_append(jb, "{");
    jb_append_kv(jb, "id",           row[0]);  /* id */
    jb_append(jb, ",");
    jb_append_kv(jb, "product_code", row[1]);  /* product_code */
    jb_append(jb, ",");
    jb_append_kv(jb, "product_name", row[2]);  /* product_name */
    jb_append(jb, ",");
    jb_append_kv(jb, "category",     row[3]);  /* category */
    jb_append(jb, ",");
    jb_append_kv(jb, "sale_date",    row[4]);  /* sale_date */
    jb_append(jb, ",");
    jb_append_int(jb, "quantity",    row[5] ? atoi(row[5]) : 0);
    jb_append(jb, ",");
    jb_append_float(jb, "sale_price", row[6] ? atof(row[6]) : 0.0);
    jb_append(jb, ",");
    jb_append_float(jb, "total_amount", row[7] ? atof(row[7]) : 0.0);
    jb_append(jb, ",");
    jb_append_kv(jb, "created_at",  row[8]);  /* created_at */
    jb_append(jb, "}");
}

/* ================================================================
 * 内部辅助函数 —— 从 inventory 表获取商品名称和类别
 * ================================================================ */

/**
 * 查询指定 product_code 的商品名称和类别
 * 找到返回 1，未找到返回 0
 */
static int get_product_info(MYSQL *conn, const char *product_code,
                            char *product_name, int name_size,
                            char *category, int cat_size)
{
    char escaped_code[MAX_STR_LEN];
    mysql_escape(escaped_code, product_code, conn);

    char sql[MAX_SQL_LEN];
    snprintf(sql, sizeof(sql),
             "SELECT product_name, category FROM inventory WHERE product_code='%s'",
             escaped_code);

    MYSQL_RES *result = db_query(conn, sql);
    if (!result) return 0;

    int found = 0;
    if (mysql_num_rows(result) > 0) {
        MYSQL_ROW row = mysql_fetch_row(result);
        if (row[0]) {
            strncpy(product_name, row[0], name_size - 1);
            product_name[name_size - 1] = '\0';
        } else {
            product_name[0] = '\0';
        }
        if (row[1]) {
            strncpy(category, row[1], cat_size - 1);
            category[cat_size - 1] = '\0';
        } else {
            category[0] = '\0';
        }
        found = 1;
    }

    db_free_result(result);
    return found;
}

/* ================================================================
 * 1. handle_sales_list —— 销售记录列表（分页 + 多条件筛选）
 *
 * 支持通过 keyword、start_date、end_date、category 进行筛选，
 * 结果按 created_at 降序排列，支持分页。
 * ================================================================ */

void handle_sales_list(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取查询参数 ---------- */
    int page      = get_query_param_int(hm, "page", 1);
    int page_size = get_query_param_int(hm, "page_size", DEFAULT_PAGE_SIZE);
    if (page_size > MAX_PAGE_SIZE) page_size = MAX_PAGE_SIZE;

    char keyword[MAX_STR_LEN]    = {0};
    char start_date[MAX_STR_LEN] = {0};
    char end_date[MAX_STR_LEN]   = {0};
    char category[MAX_STR_LEN]   = {0};

    get_query_param(hm, "keyword",    keyword,    sizeof(keyword),    NULL);
    get_query_param(hm, "start_date", start_date, sizeof(start_date), NULL);
    get_query_param(hm, "end_date",   end_date,   sizeof(end_date),   NULL);
    get_query_param(hm, "category",   category,   sizeof(category),   NULL);

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    /* ---------- 构造 WHERE 子句 ---------- */
    char where_clause[MAX_SQL_LEN] = {0};
    char conds[5][MAX_SQL_LEN];
    int cond_count = 0;

    /* keyword: 模糊匹配 product_code 或 product_name */
    if (keyword[0] != '\0') {
        char escaped_kw[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_kw, keyword, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "(product_code LIKE '%%%s%%' OR product_name LIKE '%%%s%%')",
                 escaped_kw, escaped_kw);
    }

    /* start_date: sale_date >= ? */
    if (start_date[0] != '\0') {
        char escaped_sd[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_sd, start_date, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "sale_date >= '%s'", escaped_sd);
    }

    /* end_date: sale_date <= ? */
    if (end_date[0] != '\0') {
        char escaped_ed[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_ed, end_date, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "sale_date <= '%s'", escaped_ed);
    }

    /* category: 精确匹配 */
    if (category[0] != '\0') {
        char escaped_cat[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_cat, category, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "category = '%s'", escaped_cat);
    }

    /* 拼接 WHERE 子句 */
    if (cond_count > 0) {
        strcat(where_clause, "WHERE ");
        for (int i = 0; i < cond_count; i++) {
            if (i > 0) strcat(where_clause, " AND ");
            strcat(where_clause, conds[i]);
        }
    }

    /* ---------- 查询总记录数 ---------- */
    char count_sql[MAX_SQL_LEN];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM sales %s", where_clause);

    int total = 0;
    MYSQL_RES *count_res = db_query(conn, count_sql);
    if (count_res) {
        MYSQL_ROW row = mysql_fetch_row(count_res);
        if (row && row[0]) total = atoi(row[0]);
        db_free_result(count_res);
    }

    /* ---------- 计算分页 ---------- */
    int total_pages = (total > 0) ? (int)ceil((double)total / page_size) : 1;
    if (page < 1) page = 1;
    if (page > total_pages) page = total_pages;
    int offset = (page - 1) * page_size;

    /* ---------- 查询分页数据 ---------- */
    char data_sql[MAX_SQL_LEN];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT id, product_code, product_name, category, "
             "sale_date, quantity, sale_price, total_amount, created_at "
             "FROM sales %s ORDER BY created_at DESC LIMIT %d OFFSET %d",
             where_clause, page_size, offset);

    MYSQL_RES *data_res = db_query(conn, data_sql);

    /* ---------- 构造 JSON 响应 ---------- */
    JsonBuf jb;
    jb_init(&jb);

    /* 构建 list 数组 */
    jb_append(&jb, "[");
    if (data_res) {
        int row_count = (int)mysql_num_rows(data_res);
        for (int i = 0; i < row_count; i++) {
            MYSQL_ROW row = mysql_fetch_row(data_res);
            if (i > 0) jb_append(&jb, ",");
            append_sale_row_json(&jb, row);
        }
        db_free_result(data_res);
    }
    jb_append(&jb, "]");
    const char *list_json = jb_get(&jb);  /* 暂存指针 */

    /* 构建 data 对象 */
    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append_int(&jb_data, "total", total);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "page", page);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "page_size", page_size);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "total_pages", total_pages);
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"list\":%s", list_json);
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "查询成功", jb_get(&jb_data));

    /* 释放临时缓冲区（注意：jb_data.data 已作为 JSON 片段传入 json_response，
       json_response 会复制内容到新缓冲区，所以此处可安全释放） */
    jb_free(&jb);
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}

/* ================================================================
 * 2. handle_sales_sorted —— 按销售日期排序的销售记录
 *
 * 与列表接口类似，但默认按 sale_date 降序排列，
 * 支持 order=asc 改为升序，响应中包含 sort_order 字段。
 * ================================================================ */

void handle_sales_sorted(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取查询参数 ---------- */
    int page      = get_query_param_int(hm, "page", 1);
    int page_size = get_query_param_int(hm, "page_size", DEFAULT_PAGE_SIZE);
    if (page_size > MAX_PAGE_SIZE) page_size = MAX_PAGE_SIZE;

    char order[MAX_STR_LEN] = {0};
    get_query_param(hm, "order", order, sizeof(order), "desc");

    /* 确定排序方向，防止 SQL 注入：仅允许 asc 或 desc */
    int sort_asc = (strcmp(order, "asc") == 0) ? 1 : 0;

    /* 筛选参数 */
    char keyword[MAX_STR_LEN]    = {0};
    char start_date[MAX_STR_LEN] = {0};
    char end_date[MAX_STR_LEN]   = {0};
    char category[MAX_STR_LEN]   = {0};

    get_query_param(hm, "keyword",    keyword,    sizeof(keyword),    NULL);
    get_query_param(hm, "start_date", start_date, sizeof(start_date), NULL);
    get_query_param(hm, "end_date",   end_date,   sizeof(end_date),   NULL);
    get_query_param(hm, "category",   category,   sizeof(category),   NULL);

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    /* ---------- 构造 WHERE 子句 ---------- */
    char where_clause[MAX_SQL_LEN] = {0};
    char conds[5][MAX_SQL_LEN];
    int cond_count = 0;

    if (keyword[0] != '\0') {
        char escaped_kw[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_kw, keyword, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "(product_code LIKE '%%%s%%' OR product_name LIKE '%%%s%%')",
                 escaped_kw, escaped_kw);
    }

    if (start_date[0] != '\0') {
        char escaped_sd[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_sd, start_date, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "sale_date >= '%s'", escaped_sd);
    }

    if (end_date[0] != '\0') {
        char escaped_ed[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_ed, end_date, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "sale_date <= '%s'", escaped_ed);
    }

    if (category[0] != '\0') {
        char escaped_cat[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_cat, category, conn);
        snprintf(conds[cond_count++], MAX_SQL_LEN,
                 "category = '%s'", escaped_cat);
    }

    if (cond_count > 0) {
        strcat(where_clause, "WHERE ");
        for (int i = 0; i < cond_count; i++) {
            if (i > 0) strcat(where_clause, " AND ");
            strcat(where_clause, conds[i]);
        }
    }

    /* ---------- 查询总记录数 ---------- */
    char count_sql[MAX_SQL_LEN];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM sales %s", where_clause);

    int total = 0;
    MYSQL_RES *count_res = db_query(conn, count_sql);
    if (count_res) {
        MYSQL_ROW row = mysql_fetch_row(count_res);
        if (row && row[0]) total = atoi(row[0]);
        db_free_result(count_res);
    }

    /* ---------- 计算分页 ---------- */
    int total_pages = (total > 0) ? (int)ceil((double)total / page_size) : 1;
    if (page < 1) page = 1;
    if (page > total_pages) page = total_pages;
    int offset = (page - 1) * page_size;

    /* ---------- 查询分页数据 ---------- */
    char data_sql[MAX_SQL_LEN];
    const char *order_dir = sort_asc ? "ASC" : "DESC";
    snprintf(data_sql, sizeof(data_sql),
             "SELECT id, product_code, product_name, category, "
             "sale_date, quantity, sale_price, total_amount, created_at "
             "FROM sales %s ORDER BY sale_date %s, id %s "
             "LIMIT %d OFFSET %d",
             where_clause, order_dir, order_dir, page_size, offset);

    MYSQL_RES *data_res = db_query(conn, data_sql);

    /* ---------- 构造 JSON 响应 ---------- */
    JsonBuf jb;
    jb_init(&jb);

    jb_append(&jb, "[");
    if (data_res) {
        int row_count = (int)mysql_num_rows(data_res);
        for (int i = 0; i < row_count; i++) {
            MYSQL_ROW row = mysql_fetch_row(data_res);
            if (i > 0) jb_append(&jb, ",");
            append_sale_row_json(&jb, row);
        }
        db_free_result(data_res);
    }
    jb_append(&jb, "]");
    const char *list_json = jb_get(&jb);

    /* 构建 data 对象 */
    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append_int(&jb_data, "total", total);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "page", page);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "page_size", page_size);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "total_pages", total_pages);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "sort_order", sort_asc ? "asc" : "desc");
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"list\":%s", list_json);
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "查询成功", jb_get(&jb_data));

    jb_free(&jb);
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}

/* ================================================================
 * 3. handle_sales_create —— 新增单条销售记录（仅记录）
 *
 * 本接口仅创建销售记录，不扣减库存。
 * 库存扣减由 /api/sales/checkout 接口负责。
 *
 * 必填: product_code, quantity (>0), sale_price (>0)
 * 可选: sale_date（默认当天）
 * ================================================================ */

void handle_sales_create(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取请求体 ---------- */
    char body[MAX_BODY_LEN] = {0};
    int body_len = (int)hm->body.len;
    if (body_len > MAX_BODY_LEN - 1) body_len = MAX_BODY_LEN - 1;
    if (body_len > 0) {
        memcpy(body, hm->body.buf, body_len);
        body[body_len] = '\0';
    }

    if (body[0] == '\0') {
        char *err = json_error(400, "请求体不能为空");
        send_json_resp_status(c, 400, err);
        return;
    }

    /* ---------- 解析 JSON 字段 ---------- */
    char product_code[MAX_STR_LEN] = {0};
    int quantity   = 0;
    double sale_price = 0.0;
    char sale_date[MAX_STR_LEN] = {0};

    if (!json_get_string(body, "product_code", product_code, sizeof(product_code)) ||
        product_code[0] == '\0') {
        char *err = json_error(400, "缺少必填字段: product_code");
        send_json_resp_status(c, 400, err);
        return;
    }

    if (!json_get_int(body, "quantity", &quantity) || quantity <= 0) {
        char *err = json_error(400, "quantity 必须为正整数");
        send_json_resp_status(c, 400, err);
        return;
    }

    if (!json_get_double(body, "sale_price", &sale_price) || sale_price <= 0.0) {
        char *err = json_error(400, "sale_price 必须为正数");
        send_json_resp_status(c, 400, err);
        return;
    }

    /* sale_date 为可选字段，默认当天 */
    if (!json_get_string(body, "sale_date", sale_date, sizeof(sale_date)) ||
        sale_date[0] == '\0') {
        get_today_date(sale_date, sizeof(sale_date));
    }

    /* ---------- 验证商品是否存在 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    int exists = db_product_exists(conn, product_code);
    if (exists <= 0) {
        char err_msg[MAX_SQL_LEN];
        snprintf(err_msg, sizeof(err_msg), "商品编号不存在: %s", product_code);
        char *err = json_error(404, err_msg);
        send_json_resp_status(c, 404, err);
        db_release_connection(conn);
        return;
    }

    /* ---------- 获取商品名称和类别 ---------- */
    char product_name[MAX_STR_LEN] = {0};
    char category[MAX_STR_LEN] = {0};
    if (!get_product_info(conn, product_code,
                          product_name, sizeof(product_name),
                          category, sizeof(category))) {
        char *err = json_error(404, "获取商品信息失败");
        send_json_resp_status(c, 404, err);
        db_release_connection(conn);
        return;
    }

    /* ---------- 计算总金额 ---------- */
    double total_amount = quantity * sale_price;

    /* ---------- 插入销售记录 ---------- */
    char escaped_code[MAX_STR_LEN * 2 + 1];
    char escaped_name[MAX_STR_LEN * 2 + 1];
    char escaped_cat[MAX_STR_LEN * 2 + 1];
    char escaped_date[MAX_STR_LEN * 2 + 1];

    mysql_escape(escaped_code, product_code, conn);
    mysql_escape(escaped_name, product_name, conn);
    mysql_escape(escaped_cat, category, conn);
    mysql_escape(escaped_date, sale_date, conn);

    char insert_sql[MAX_SQL_LEN];
    snprintf(insert_sql, sizeof(insert_sql),
             "INSERT INTO sales (product_code, product_name, category, "
             "sale_date, quantity, sale_price, total_amount) "
             "VALUES ('%s', '%s', '%s', '%s', %d, %.2f, %.2f)",
             escaped_code, escaped_name, escaped_cat, escaped_date,
             quantity, sale_price, total_amount);

    int affected = db_execute(conn, insert_sql);
    if (affected < 0) {
        char *err = json_error(500, "插入销售记录失败");
        send_json_resp_status(c, 500, err);
        db_release_connection(conn);
        return;
    }

    long long insert_id = (long long)mysql_insert_id(conn);

    /* ---------- 构造返回 JSON ---------- */
    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append_int(&jb_data, "id", (int)insert_id);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "product_code", product_code);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "product_name", product_name);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "category", category);
    jb_append(&jb_data, ",");
    jb_append_int(&jb_data, "quantity", quantity);
    jb_append(&jb_data, ",");
    jb_append_float(&jb_data, "sale_price", sale_price);
    jb_append(&jb_data, ",");
    jb_append_float(&jb_data, "total_amount", total_amount);
    jb_append(&jb_data, ",");
    jb_append_kv(&jb_data, "sale_date", sale_date);
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "新增销售记录成功", jb_get(&jb_data));
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}

/* ================================================================
 * 4. handle_sales_checkout —— 销售结账流程（事务保护）
 *
 * 核心业务逻辑：
 *   1. 解析请求体中的 items 数组
 *   2. 开启数据库事务
 *   3. 对每个 item：
 *      a. 校验商品是否存在 → 不存在则回滚返回 404
 *      b. 检查库存是否充足 → 不足则回滚返回 409
 *      c. 扣减库存（UPDATE inventory SET stock_quantity = stock_quantity - ?）
 *      d. 获取商品名称和类别
 *      e. 插入销售记录
 *   4. 提交事务
 *   5. 返回所有创建的销售记录及总金额
 *
 * 任何一步失败都会触发整体回滚，保证数据一致性。
 * ================================================================ */

void handle_sales_checkout(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取请求体 ---------- */
    char body[MAX_BODY_LEN] = {0};
    int body_len = (int)hm->body.len;
    if (body_len > MAX_BODY_LEN - 1) body_len = MAX_BODY_LEN - 1;
    if (body_len > 0) {
        memcpy(body, hm->body.buf, body_len);
        body[body_len] = '\0';
    }

    if (body[0] == '\0') {
        char *err = json_error(400, "请求体不能为空");
        send_json_resp_status(c, 400, err);
        return;
    }

    /* ---------- 解析 items 数组 ---------- */
    /* 找到 "items" 字段及其后的 '[' */
    const char *items_start = strstr(body, "\"items\"");
    if (!items_start) {
        char *err = json_error(400, "缺少必填字段: items");
        send_json_resp_status(c, 400, err);
        return;
    }

    items_start = strchr(items_start + 7, '[');
    if (!items_start) {
        char *err = json_error(400, "items 字段格式错误，应为数组");
        send_json_resp_status(c, 400, err);
        return;
    }

    /* 临时存储解析出的 item 信息 */
    char  item_codes[MAX_ITEMS][MAX_STR_LEN];
    int   item_qtys[MAX_ITEMS];
    double item_prices[MAX_ITEMS];
    int   item_count = 0;

    /* 状态机遍历数组：[ {item1}, {item2}, ... ] */
    const char *p = items_start + 1;  /* 跳过 '[' */

    while (item_count < MAX_ITEMS) {
        /* 跳过空白符，寻找 '{' 或 ']' */
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (*p == ']' || *p == '\0') break;  /* 数组结束 */

        /* 找到对象起始 '{' */
        const char *obj_start = strchr(p, '{');
        if (!obj_start) break;
        const char *obj_end = strchr(obj_start, '}');
        if (!obj_end) break;

        /* 提取当前对象的内容（含花括号） */
        int obj_len = (int)(obj_end - obj_start) + 1;
        char obj_buf[MAX_BODY_LEN];
        if (obj_len >= MAX_BODY_LEN) obj_len = MAX_BODY_LEN - 1;
        memcpy(obj_buf, obj_start, obj_len);
        obj_buf[obj_len] = '\0';

        /* 从对象中提取字段 */
        char code[MAX_STR_LEN] = {0};
        int qty = 0;
        double price = 0.0;

        if (!json_get_string(obj_buf, "product_code", code, sizeof(code)) ||
            code[0] == '\0') {
            char *err = json_error(400, "items 中缺少 product_code");
            send_json_resp_status(c, 400, err);
            return;
        }
        if (!json_get_int(obj_buf, "quantity", &qty) || qty <= 0) {
            char *err = json_error(400, "items 中 quantity 必须为正整数");
            send_json_resp_status(c, 400, err);
            return;
        }
        if (!json_get_double(obj_buf, "sale_price", &price) || price <= 0.0) {
            char *err = json_error(400, "items 中 sale_price 必须为正数");
            send_json_resp_status(c, 400, err);
            return;
        }

        /* 存入临时数组 */
        snprintf(item_codes[item_count], MAX_STR_LEN, "%s", code);
        item_qtys[item_count]   = qty;
        item_prices[item_count] = price;
        item_count++;

        /* 移动到下一个对象 */
        p = obj_end + 1;
    }

    if (item_count == 0) {
        char *err = json_error(400, "items 数组不能为空");
        send_json_resp_status(c, 400, err);
        return;
    }

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    /* ---------- 获取当天日期 ---------- */
    char today[16] = {0};
    get_today_date(today, sizeof(today));

    /* ---------- 存储已创建的销售记录信息（用于响应） ---------- */
    long long sale_ids[MAX_ITEMS];
    char sale_names[MAX_ITEMS][MAX_STR_LEN];
    int  sale_qtys[MAX_ITEMS];
    double sale_prices[MAX_ITEMS];
    double sale_totals[MAX_ITEMS];
    double grand_total = 0.0;

    /* ---------- 开启事务 ---------- */
    if (db_begin_transaction(conn) != 0) {
        char *err = json_error(500, "开启事务失败");
        send_json_resp_status(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* ---------- 逐项处理 ---------- */
    for (int i = 0; i < item_count; i++) {
        const char *code  = item_codes[i];
        int         qty   = item_qtys[i];
        double      price = item_prices[i];

        /* 校验商品是否存在 */
        if (!db_product_exists(conn, code)) {
            db_rollback(conn);
            char err_msg[MAX_SQL_LEN];
            snprintf(err_msg, sizeof(err_msg), "商品编号不存在: %s", code);
            char *err = json_error(404, err_msg);
            send_json_resp_status(c, 404, err);
            db_release_connection(conn);
            return;
        }

        /* 检查当前库存 */
        int current_stock = db_get_stock(conn, code);
        if (current_stock < 0) {
            db_rollback(conn);
            char *err = json_error(500, "查询库存失败");
            send_json_resp_status(c, 500, err);
            db_release_connection(conn);
            return;
        }
        if (current_stock < qty) {
            db_rollback(conn);

            /* 获取商品名称用于错误消息 */
            char name_buf[MAX_STR_LEN] = {0};
            char cat_buf[MAX_STR_LEN]  = {0};
            get_product_info(conn, code, name_buf, sizeof(name_buf),
                             cat_buf, sizeof(cat_buf));

            char err_msg[MAX_SQL_LEN];
            snprintf(err_msg, sizeof(err_msg),
                     "库存不足: %s 当前库存 %d，需要 %d",
                     name_buf[0] ? name_buf : code, current_stock, qty);

            /* 构造 409 响应 data */
            JsonBuf jb_data;
            jb_init(&jb_data);
            jb_append(&jb_data, "{");
            jb_append_kv(&jb_data, "product_code", code);
            jb_append(&jb_data, ",");
            jb_append_int(&jb_data, "current_stock", current_stock);
            jb_append(&jb_data, ",");
            jb_append_int(&jb_data, "requested", qty);
            jb_append(&jb_data, ",");
            jb_append_int(&jb_data, "shortfall", qty - current_stock);
            jb_append(&jb_data, "}");

            char *err = json_response(409, err_msg, jb_get(&jb_data));
            jb_free(&jb_data);
            send_json_resp_status(c, 409, err);
            db_release_connection(conn);
            return;
        }

        /* 扣减库存 */
        char escaped_code[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_code, code, conn);

        char update_sql[MAX_SQL_LEN];
        snprintf(update_sql, sizeof(update_sql),
                 "UPDATE inventory SET stock_quantity = stock_quantity - %d "
                 "WHERE product_code = '%s'",
                 qty, escaped_code);

        if (db_execute(conn, update_sql) < 0) {
            db_rollback(conn);
            char *err = json_error(500, "扣减库存失败");
            send_json_resp_status(c, 500, err);
            db_release_connection(conn);
            return;
        }

        /* 获取商品名称和类别 */
        char product_name[MAX_STR_LEN] = {0};
        char category[MAX_STR_LEN]     = {0};
        if (!get_product_info(conn, code,
                              product_name, sizeof(product_name),
                              category, sizeof(category))) {
            db_rollback(conn);
            char *err = json_error(500, "获取商品信息失败");
            send_json_resp_status(c, 500, err);
            db_release_connection(conn);
            return;
        }

        /* 计算总金额 */
        double total_amount = qty * price;

        /* 插入销售记录 */
        char escaped_name[MAX_STR_LEN * 2 + 1];
        char escaped_cat[MAX_STR_LEN * 2 + 1];
        mysql_escape(escaped_name, product_name, conn);
        mysql_escape(escaped_cat, category, conn);

        char insert_sql[MAX_SQL_LEN];
        snprintf(insert_sql, sizeof(insert_sql),
                 "INSERT INTO sales (product_code, product_name, category, "
                 "sale_date, quantity, sale_price, total_amount) "
                 "VALUES ('%s', '%s', '%s', '%s', %d, %.2f, %.2f)",
                 escaped_code, escaped_name, escaped_cat, today,
                 qty, price, total_amount);

        if (db_execute(conn, insert_sql) < 0) {
            db_rollback(conn);
            char *err = json_error(500, "插入销售记录失败");
            send_json_resp_status(c, 500, err);
            db_release_connection(conn);
            return;
        }

        /* 记录已创建的信息 */
        sale_ids[i]    = (long long)mysql_insert_id(conn);
        snprintf(sale_names[i], MAX_STR_LEN, "%s", product_name);
        sale_qtys[i]   = qty;
        sale_prices[i] = price;
        sale_totals[i] = total_amount;
        grand_total   += total_amount;
    }

    /* ---------- 提交事务 ---------- */
    if (db_commit(conn) != 0) {
        db_rollback(conn);
        char *err = json_error(500, "提交事务失败");
        send_json_resp_status(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* ---------- 构造成功响应 ---------- */
    JsonBuf jb_records;
    jb_init(&jb_records);
    jb_append(&jb_records, "[");
    for (int i = 0; i < item_count; i++) {
        if (i > 0) jb_append(&jb_records, ",");
        jb_append(&jb_records, "{");
        jb_append_int(&jb_records, "id", (int)sale_ids[i]);
        jb_append(&jb_records, ",");
        jb_append_kv(&jb_records, "product_code", item_codes[i]);
        jb_append(&jb_records, ",");
        jb_append_kv(&jb_records, "product_name", sale_names[i]);
        jb_append(&jb_records, ",");
        jb_append_int(&jb_records, "quantity", sale_qtys[i]);
        jb_append(&jb_records, ",");
        jb_append_float(&jb_records, "sale_price", sale_prices[i]);
        jb_append(&jb_records, ",");
        jb_append_float(&jb_records, "total_amount", sale_totals[i]);
        jb_append(&jb_records, ",");
        jb_append_kv(&jb_records, "sale_date", today);
        jb_append(&jb_records, "}");
    }
    jb_append(&jb_records, "]");

    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append(&jb_data, "\"sale_records\":%s", jb_get(&jb_records));
    jb_append(&jb_data, ",");
    jb_append_float(&jb_data, "total_amount", grand_total);
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "销售完成，库存已扣减", jb_get(&jb_data));

    jb_free(&jb_records);
    jb_free(&jb_data);

    send_json_resp(c, resp);
    db_release_connection(conn);
}
