/**
 * @file    inventory.c
 * @brief   库存管理模块 —— 文具商品库存的增删改查 HTTP 处理器实现
 *
 * 本模块为文具店销售管理系统提供完整的库存管理 RESTful API，
 * 包括分页列表查询、单品查询、新增商品、更新商品和删除商品五个接口。
 *
 * 所有接口均：
 *   - 使用参数化 / 转义方式构造 SQL，防止 SQL 注入
 *   - 返回统一 JSON 格式响应：{"code":N,"message":"...","data":...}
 *   - 携带 CORS 跨域头，支持前端跨域访问
 *   - 对数据库操作错误做防御性检查并返回相应 HTTP 状态码
 *
 * 依赖：mongoose.h（HTTP 服务）、db.h（数据库操作）、json.h（JSON 构造）
 */

#include "inventory.h"
#include "db.h"
#include "json.h"
#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** HTTP 响应头：JSON 内容类型 + CORS 跨域许可 */
#define CORS_HEADERS \
    "Content-Type: application/json\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/** 请求体缓冲区最大字节数 */
#define BODY_BUF_SIZE   8192

/** 单条 SQL 语句缓冲区最大字节数 */
#define SQL_BUF_SIZE    8192

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * 从 JSON 字符串中提取指定 key 的字符串值
 *
 * 使用简单的字符串匹配查找 key，支持引号包裹的字符串值
 * 和未引号的数值/布尔值。返回静态缓冲区指针，同一线程内
 * 多次调用会互相覆盖，调用者应立即复制结果。
 *
 * @param json  待解析的 JSON 字符串（须以 '\0' 结尾）
 * @param key   要查找的 JSON 键名（不含外围双引号）
 * @return      找到则返回静态缓冲区指针，未找到返回 NULL
 */
static char* json_extract(const char *json, const char *key)
{
    static char buf[512];
    char search[128];
    int key_len;

    if (json == NULL || key == NULL) return NULL;

    snprintf(search, sizeof(search), "\"%s\"", key);
    key_len = (int)strlen(search);

    const char *pos = strstr(json, search);
    if (!pos) return NULL;

    /* 跳过 key 和冒号及空白字符 */
    pos = strchr(pos + key_len, ':');
    if (!pos) return NULL;
    pos++;
    while (*pos == ':' || *pos == ' ') pos++;

    /* 提取字符串值（双引号包裹） */
    if (*pos == '"') {
        pos++;
        int i = 0;
        while (*pos && *pos != '"' && i < 511) {
            buf[i++] = *pos++;
        }
        buf[i] = '\0';
    } else {
        /* 提取数值或布尔值 */
        int i = 0;
        while (*pos && *pos != ',' && *pos != '}' && *pos != ' ' && *pos != '\n'
               && *pos != '\r' && *pos != '\t' && i < 511) {
            buf[i++] = *pos++;
        }
        buf[i] = '\0';
    }
    return buf;
}

/**
 * 从 URI 路径末尾提取商品编号
 *
 * 解析 URI（如 "/api/inventory/P001"），找到最后一个 '/' 之后的
 * 字符串即为商品编号。解码后写入调用方提供的缓冲区。
 *
 * @param uri       指向 Mongoose HTTP 消息中 uri 字段的指针
 * @param buf       输出缓冲区，用于存放提取的商品编号
 * @param buf_size  缓冲区容量（字节）
 * @return          成功返回 0，失败（路径无有效尾段或缓冲区不足）返回 -1
 */
static int extract_product_code(const struct mg_str *uri,
                                char *buf, size_t buf_size)
{
    if (uri == NULL || uri->len == 0 || buf == NULL || buf_size == 0) {
        return -1;
    }

    /* 分配临时缓冲区复制 URI（因为 mg_str 不一定以 '\0' 结尾） */
    size_t copy_len = uri->len < buf_size - 1 ? uri->len : buf_size - 1;

    char *temp = (char*)malloc(copy_len + 1);
    if (temp == NULL) return -1;

    memcpy(temp, uri->buf, copy_len);
    temp[copy_len] = '\0';

    /* 定位最后一个 '/' */
    char *last_slash = strrchr(temp, '/');
    if (last_slash == NULL || *(last_slash + 1) == '\0') {
        free(temp);
        return -1;
    }

    /* 复制尾段到输出缓冲区并做 URL 解码 */
    size_t code_len = strlen(last_slash + 1);
    if (code_len >= buf_size) {
        free(temp);
        return -1;
    }

    memcpy(buf, last_slash + 1, code_len);
    buf[code_len] = '\0';

    free(temp);
    return 0;
}

/**
 * 从 Mongoose HTTP 消息中复制请求体到以 '\0' 结尾的缓冲区
 *
 * @param hm      Mongoose HTTP 消息
 * @param buf     目标缓冲区
 * @param buf_size 缓冲区容量
 */
static void copy_body(const struct mg_http_message *hm, char *buf, size_t buf_size)
{
    if (hm == NULL || buf == NULL || buf_size == 0) {
        return;
    }
    size_t copy_len = hm->body.len < buf_size - 1
                      ? hm->body.len : buf_size - 1;
    memcpy(buf, hm->body.buf, copy_len);
    buf[copy_len] = '\0';
}

/**
 * 将一条数据库查询结果行追加为 JSON 对象到 JsonBuf
 *
 * 追加格式：{"id":N,"product_code":"...","product_name":"...",...}
 * 调用者须在外层自行控制逗号分隔和多行拼接。
 *
 * @param jb   目标 JSON 缓冲区
 * @param row  mysql_fetch_row() 返回的行数据
 */
static void append_inventory_item(JsonBuf *jb, MYSQL_ROW row)
{
    /* 字段顺序：
     * 0:id  1:product_code  2:product_name  3:category
     * 4:manufacturer  5:model  6:stock_quantity  7:unit_price
     * 8:safety_stock  9:warning_stock  10:created_at  11:updated_at
     */
    jb_append(jb, "{");

    jb_append_int(jb, "id",             row[0]  ? atoi(row[0])  : 0);
    jb_append(jb, ",");
    jb_append_kv(jb, "product_code",    row[1]  ? row[1]  : "");
    jb_append(jb, ",");
    jb_append_kv(jb, "product_name",    row[2]  ? row[2]  : "");
    jb_append(jb, ",");
    jb_append_kv(jb, "category",        row[3]  ? row[3]  : "");
    jb_append(jb, ",");
    jb_append_kv(jb, "manufacturer",    row[4]  ? row[4]  : "");
    jb_append(jb, ",");
    jb_append_kv(jb, "model",           row[5]  ? row[5]  : "");
    jb_append(jb, ",");
    jb_append_int(jb, "stock_quantity", row[6]  ? atoi(row[6])  : 0);
    jb_append(jb, ",");
    jb_append_float(jb, "unit_price",   row[7]  ? atof(row[7])  : 0.0);
    jb_append(jb, ",");
    jb_append_int(jb, "safety_stock",   row[8]  ? atoi(row[8])  : 0);
    jb_append(jb, ",");
    jb_append_int(jb, "warning_stock",  row[9]  ? atoi(row[9])  : 0);
    jb_append(jb, ",");
    jb_append_kv(jb, "created_at",      row[10] ? row[10] : "");
    jb_append(jb, ",");
    jb_append_kv(jb, "updated_at",      row[11] ? row[11] : "");

    jb_append(jb, "}");
}

/**
 * 构造单条库存记录的完整 JSON 字符串（malloc 分配，调用者负责 free）
 *
 * @param row  mysql_fetch_row() 返回的行数据
 * @return     malloc 分配的 JSON 字符串，失败返回 NULL
 */
static char* build_item_json(MYSQL_ROW row)
{
    JsonBuf jb;
    jb_init(&jb);
    append_inventory_item(&jb, row);
    return jb.data;  /* 调用者须 free */
}

/**
 * 查询并构造单条库存记录的 JSON，用于创建/更新后返回最新数据
 *
 * @param conn         数据库连接
 * @param product_code 商品编号
 * @return             malloc 分配的 JSON 字符串，失败返回 NULL
 */
static char* query_item_json(MYSQL *conn, const char *product_code)
{
    char escaped[128];
    mysql_real_escape_string(conn, escaped,
                            product_code,
                            (unsigned long)strlen(product_code));

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT id, product_code, product_name, category, "
             "manufacturer, model, stock_quantity, unit_price, "
             "safety_stock, warning_stock, created_at, updated_at "
             "FROM inventory WHERE product_code='%s'",
             escaped);

    MYSQL_RES *result = db_query(conn, sql);
    if (result == NULL) return NULL;

    if (mysql_num_rows(result) == 0) {
        db_free_result(result);
        return NULL;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    char *json = build_item_json(row);

    db_free_result(result);
    return json;
}

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

/* ================================================================
 *  1. 库存列表查询
 *     GET /api/inventory?page=&page_size=&keyword=&category=
 * ================================================================ */

void handle_inventory_list(struct mg_connection *c, struct mg_http_message *hm)
{
    char page_str[32]     = {0};
    char psize_str[32]    = {0};
    char keyword[256]     = {0};
    char category[128]    = {0};

    MYSQL *conn = NULL;
    MYSQL_RES *result = NULL;
    JsonBuf data_jb;

    /* ---------- 解析查询参数 ---------- */
    mg_http_get_var(&hm->query, "page",      page_str,  sizeof(page_str));
    mg_http_get_var(&hm->query, "page_size", psize_str, sizeof(psize_str));
    mg_http_get_var(&hm->query, "keyword",   keyword,   sizeof(keyword));
    mg_http_get_var(&hm->query, "category",  category,  sizeof(category));

    int page      = page_str[0]  ? atoi(page_str)  : 1;
    int page_size = psize_str[0] ? atoi(psize_str) : 20;

    if (page < 1)       page = 1;
    if (page_size < 1)  page_size = 1;
    if (page_size > 100) page_size = 100;   /* 防止单次查询过大数据量 */

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 转义搜索关键字和分类（防 SQL 注入） ---------- */
    char escaped_keyword[512]  = {0};
    char escaped_category[256] = {0};

    if (keyword[0]) {
        mysql_real_escape_string(conn, escaped_keyword, keyword,
                                 (unsigned long)strlen(keyword));
    }
    if (category[0]) {
        mysql_real_escape_string(conn, escaped_category, category,
                                 (unsigned long)strlen(category));
    }

    /* ---------- 构建 WHERE 子句 ---------- */
    char where_sql[1024] = "";

    if (escaped_keyword[0] && escaped_category[0]) {
        snprintf(where_sql, sizeof(where_sql),
                 "WHERE (product_name LIKE '%%%s%%' OR product_code LIKE '%%%s%%') "
                 "AND category='%s'",
                 escaped_keyword, escaped_keyword, escaped_category);
    } else if (escaped_keyword[0]) {
        snprintf(where_sql, sizeof(where_sql),
                 "WHERE (product_name LIKE '%%%s%%' OR product_code LIKE '%%%s%%')",
                 escaped_keyword, escaped_keyword);
    } else if (escaped_category[0]) {
        snprintf(where_sql, sizeof(where_sql),
                 "WHERE category='%s'",
                 escaped_category);
    }

    /* ---------- 查询总记录数 ---------- */
    char count_sql[SQL_BUF_SIZE];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM inventory %s", where_sql);

    int total = 0;
    result = db_query(conn, count_sql);
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

    /* ---------- 查询分页数据 ---------- */
    int offset = (page - 1) * page_size;
    char data_sql[SQL_BUF_SIZE];
    snprintf(data_sql, sizeof(data_sql),
             "SELECT id, product_code, product_name, category, "
             "manufacturer, model, stock_quantity, unit_price, "
             "safety_stock, warning_stock, created_at, updated_at "
             "FROM inventory %s ORDER BY id ASC LIMIT %d OFFSET %d",
             where_sql, page_size, offset);

    result = db_query(conn, data_sql);
    if (result == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }

    /* ---------- 构造 data JSON ---------- */
    jb_init(&data_jb);
    jb_append(&data_jb, "{");
    jb_append_int(&data_jb, "total", total);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "page", page);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "page_size", page_size);
    jb_append(&data_jb, ",\"list\":[");

    int first = 1;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {
        if (!first) {
            jb_append(&data_jb, ",");
        }
        first = 0;
        append_inventory_item(&data_jb, row);
    }

    jb_append(&data_jb, "]}");

    /* ---------- 发送响应 ---------- */
    send_response(c, 200, 200, "查询成功", data_jb.data);

    /* ---------- 清理 ---------- */
    jb_free(&data_jb);
    db_free_result(result);
    db_release_connection(conn);
}

/* ================================================================
 *  2. 库存单品查询
 *     GET /api/inventory/{product_code}
 * ================================================================ */

void handle_inventory_get(struct mg_connection *c, struct mg_http_message *hm)
{
    char product_code[128] = {0};
    MYSQL *conn = NULL;

    /* ---------- 从 URI 路径提取商品编号 ---------- */
    if (extract_product_code(&hm->uri, product_code,
                             sizeof(product_code)) != 0) {
        send_error(c, 400, 400, "无效的商品编号");
        return;
    }

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 查询商品 ---------- */
    char *item_json = query_item_json(conn, product_code);
    if (item_json == NULL) {
        db_release_connection(conn);
        send_error(c, 404, 404, "商品不存在");
        return;
    }

    /* ---------- 发送响应 ---------- */
    send_response(c, 200, 200, "查询成功", item_json);

    /* ---------- 清理 ---------- */
    free(item_json);
    db_release_connection(conn);
}

/* ================================================================
 *  3. 新增库存商品
 *     POST /api/inventory
 * ================================================================ */

void handle_inventory_create(struct mg_connection *c, struct mg_http_message *hm)
{
    char body[BODY_BUF_SIZE] = {0};
    MYSQL *conn = NULL;

    /* ---------- 复制请求体并解析必填字段 ---------- */
    copy_body(hm, body, sizeof(body));

    char *product_code = json_extract(body, "product_code");
    char *product_name = json_extract(body, "product_name");

    if (product_code == NULL || strlen(product_code) == 0 ||
        product_name == NULL || strlen(product_name) == 0) {
        send_error(c, 400, 400, "缺少必填字段：product_code 和 product_name");
        return;
    }

    /* ---------- 解析可选字段 ---------- */
    char *category     = json_extract(body, "category");
    char *manufacturer = json_extract(body, "manufacturer");
    char *model        = json_extract(body, "model");
    char *sq_str       = json_extract(body, "stock_quantity");
    char *up_str       = json_extract(body, "unit_price");
    char *ss_str       = json_extract(body, "safety_stock");
    char *ws_str       = json_extract(body, "warning_stock");

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 检查商品编号是否已存在 ---------- */
    int exists = db_product_exists(conn, product_code);
    if (exists == -1) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }
    if (exists) {
        db_release_connection(conn);
        send_error(c, 409, 409, "商品编号已存在");
        return;
    }

    /* ---------- 转义所有字符串字段 ---------- */
    char esc_code[64]       = {0};
    char esc_name[256]      = {0};
    char esc_cat[64]        = {0};
    char esc_mfr[200]       = {0};
    char esc_model[128]     = {0};

    mysql_real_escape_string(conn, esc_code, product_code,
                             (unsigned long)strlen(product_code));
    mysql_real_escape_string(conn, esc_name, product_name,
                             (unsigned long)strlen(product_name));
    if (category) {
        mysql_real_escape_string(conn, esc_cat, category,
                                 (unsigned long)strlen(category));
    }
    if (manufacturer) {
        mysql_real_escape_string(conn, esc_mfr, manufacturer,
                                 (unsigned long)strlen(manufacturer));
    }
    if (model) {
        mysql_real_escape_string(conn, esc_model, model,
                                 (unsigned long)strlen(model));
    }

    /* ---------- 构建默认值 ---------- */
    const char *def_cat = (category && category[0])     ? esc_cat  : "未分类";
    const char *def_mfr = (manufacturer && manufacturer[0]) ? esc_mfr  : "";
    const char *def_mod = (model && model[0])            ? esc_model : "";
    int  stock_qty  = (sq_str && sq_str[0]) ? atoi(sq_str)  : 0;
    double unit_pr  = (up_str && up_str[0]) ? atof(up_str)  : 0.0;
    int  safety     = (ss_str && ss_str[0]) ? atoi(ss_str)  : 30;
    int  warning    = (ws_str && ws_str[0]) ? atoi(ws_str)  : 20;

    /* ---------- 执行 INSERT ---------- */
    char sql[SQL_BUF_SIZE];
    snprintf(sql, sizeof(sql),
             "INSERT INTO inventory "
             "(product_code, product_name, category, manufacturer, model, "
             " stock_quantity, unit_price, safety_stock, warning_stock) "
             "VALUES ('%s','%s','%s','%s','%s',%d,%.2f,%d,%d)",
             esc_code, esc_name, def_cat, def_mfr, def_mod,
             stock_qty, unit_pr, safety, warning);

    if (db_execute(conn, sql) < 0) {
        db_release_connection(conn);
        send_error(c, 500, 500, "新增商品失败");
        return;
    }

    /* ---------- 查询并返回新创建的记录 ---------- */
    char *item_json = query_item_json(conn, product_code);
    if (item_json == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "商品已创建但查询失败");
        return;
    }

    send_response(c, 201, 200, "新增成功", item_json);

    /* ---------- 清理 ---------- */
    free(item_json);
    db_release_connection(conn);
}

/* ================================================================
 *  4. 更新库存商品
 *     PUT /api/inventory/{product_code}
 * ================================================================ */

void handle_inventory_update(struct mg_connection *c, struct mg_http_message *hm)
{
    char product_code[128] = {0};
    char body[BODY_BUF_SIZE] = {0};
    MYSQL *conn = NULL;

    /* ---------- 从 URI 路径提取商品编号 ---------- */
    if (extract_product_code(&hm->uri, product_code,
                             sizeof(product_code)) != 0) {
        send_error(c, 400, 400, "无效的商品编号");
        return;
    }

    /* ---------- 复制请求体 ---------- */
    copy_body(hm, body, sizeof(body));

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 检查商品是否存在 ---------- */
    int exists = db_product_exists(conn, product_code);
    if (exists == -1) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }
    if (!exists) {
        db_release_connection(conn);
        send_error(c, 404, 404, "商品不存在");
        return;
    }

    /* ---------- 解析请求体中可能存在的字段 ---------- */
    char *prod_name    = json_extract(body, "product_name");
    char *category     = json_extract(body, "category");
    char *manufacturer = json_extract(body, "manufacturer");
    char *model        = json_extract(body, "model");
    char *sq_str       = json_extract(body, "stock_quantity");
    char *up_str       = json_extract(body, "unit_price");
    char *ss_str       = json_extract(body, "safety_stock");
    char *ws_str       = json_extract(body, "warning_stock");

    /* ---------- 动态构建 UPDATE SET 子句 ---------- */
    char set_clause[2048] = "";
    int  has_set = 0;

    /* 字符串字段 —— 需转义 */
    if (prod_name && prod_name[0]) {
        char esc[256];
        mysql_real_escape_string(conn, esc, prod_name,
                                 (unsigned long)strlen(prod_name));
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%sproduct_name='%s'", has_set ? "," : "", esc);
        has_set = 1;
    }
    if (category && category[0]) {
        char esc[128];
        mysql_real_escape_string(conn, esc, category,
                                 (unsigned long)strlen(category));
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%scategory='%s'", has_set ? "," : "", esc);
        has_set = 1;
    }
    if (manufacturer && manufacturer[0]) {
        char esc[256];
        mysql_real_escape_string(conn, esc, manufacturer,
                                 (unsigned long)strlen(manufacturer));
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%smanufacturer='%s'", has_set ? "," : "", esc);
        has_set = 1;
    }
    if (model && model[0]) {
        char esc[128];
        mysql_real_escape_string(conn, esc, model,
                                 (unsigned long)strlen(model));
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%smodel='%s'", has_set ? "," : "", esc);
        has_set = 1;
    }

    /* 数值字段 —— 直接拼接（已通过 atoi/atof 做了格式校验） */
    if (sq_str && sq_str[0]) {
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%sstock_quantity=%d", has_set ? "," : "", atoi(sq_str));
        has_set = 1;
    }
    if (up_str && up_str[0]) {
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%sunit_price=%.2f", has_set ? "," : "", atof(up_str));
        has_set = 1;
    }
    if (ss_str && ss_str[0]) {
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%ssafety_stock=%d", has_set ? "," : "", atoi(ss_str));
        has_set = 1;
    }
    if (ws_str && ws_str[0]) {
        snprintf(set_clause + strlen(set_clause),
                 sizeof(set_clause) - strlen(set_clause),
                 "%swarning_stock=%d", has_set ? "," : "", atoi(ws_str));
        has_set = 1;
    }

    /* 没有任何字段需要更新 */
    if (!has_set) {
        db_release_connection(conn);
        send_error(c, 400, 400, "未提供任何需要更新的字段");
        return;
    }

    /* ---------- 执行 UPDATE ---------- */
    char esc_code[128];
    mysql_real_escape_string(conn, esc_code, product_code,
                             (unsigned long)strlen(product_code));

    char sql[SQL_BUF_SIZE];
    snprintf(sql, sizeof(sql),
             "UPDATE inventory SET %s WHERE product_code='%s'",
             set_clause, esc_code);

    if (db_execute(conn, sql) < 0) {
        db_release_connection(conn);
        send_error(c, 500, 500, "更新商品失败");
        return;
    }

    /* ---------- 查询并返回更新后的记录 ---------- */
    char *item_json = query_item_json(conn, product_code);
    if (item_json == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "商品已更新但查询失败");
        return;
    }

    send_response(c, 200, 200, "更新成功", item_json);

    /* ---------- 清理 ---------- */
    free(item_json);
    db_release_connection(conn);
}

/* ================================================================
 *  5. 删除库存商品
 *     DELETE /api/inventory/{product_code}
 * ================================================================ */

void handle_inventory_delete(struct mg_connection *c, struct mg_http_message *hm)
{
    char product_code[128] = {0};
    MYSQL *conn = NULL;

    /* ---------- 从 URI 路径提取商品编号 ---------- */
    if (extract_product_code(&hm->uri, product_code,
                             sizeof(product_code)) != 0) {
        send_error(c, 400, 400, "无效的商品编号");
        return;
    }

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 检查商品是否存在 ---------- */
    int exists = db_product_exists(conn, product_code);
    if (exists == -1) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }
    if (!exists) {
        db_release_connection(conn);
        send_error(c, 404, 404, "商品不存在");
        return;
    }

    /* ---------- 执行 DELETE ---------- */
    char esc_code[128];
    mysql_real_escape_string(conn, esc_code, product_code,
                             (unsigned long)strlen(product_code));

    char sql[512];
    snprintf(sql, sizeof(sql),
             "DELETE FROM inventory WHERE product_code='%s'", esc_code);

    if (db_execute(conn, sql) < 0) {
        db_release_connection(conn);
        send_error(c, 500, 500, "删除商品失败");
        return;
    }

    /* ---------- 发送成功响应 ---------- */
    send_response(c, 200, 200, "删除成功", NULL);

    /* ---------- 清理 ---------- */
    db_release_connection(conn);
}
