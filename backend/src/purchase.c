/**
 * purchase.c —— 进货管理模块实现
 *
 * 提供进货记录列表查询、单条进货记录新增、批量进货流程（更新库存 + 生成进货记录）。
 * 批量进货使用 MySQL 事务确保库存更新与进货记录写入的原子性。
 *
 * 依赖: db.h（数据库操作）、json.h（JSON 响应构造）、mongoose.h（HTTP 请求解析）
 */

#include "purchase.h"
#include "db.h"
#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 *  内部辅助函数
 * ================================================================ */

/**
 * 获取当天日期字符串 —— 格式 YYYY-MM-DD
 */
static void get_today(char *buf, int size)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, size, "%Y-%m-%d", tm_info);
}

/**
 * 发送带 CORS 头的 JSON 响应，发送后自动释放 body 内存
 */
static void send_json(struct mg_connection *c, int status, char *body)
{
    mg_http_reply(c, status,
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type",
        "%s", body);
    free(body);
}

/**
 * 从 HTTP 查询参数中提取整数值
 */
static int get_query_int(struct mg_http_message *hm, const char *name, int default_val)
{
    char buf[32];
    int ret = mg_http_get_var(&hm->query, name, buf, sizeof(buf));
    return (ret > 0) ? atoi(buf) : default_val;
}

/**
 * 从 HTTP 查询参数中提取字符串值
 */
static void get_query_str(struct mg_http_message *hm, const char *name,
                          char *dst, int dst_size)
{
    int ret = mg_http_get_var(&hm->query, name, dst, dst_size);
    if (ret <= 0) dst[0] = '\0';
}

/**
 * 从 inventory 表查询商品名称和分类
 * 返回 1 表示查询成功，0 表示商品不存在
 */
static int get_product_info(MYSQL *conn, const char *product_code,
                            char *product_name, int name_size,
                            char *category, int cat_size)
{
    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT product_name, category FROM inventory WHERE product_code='%s'",
             product_code);

    MYSQL_RES *res = db_query(conn, sql);
    if (!res || mysql_num_rows(res) == 0) {
        db_free_result(res);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row[0])
        snprintf(product_name, name_size, "%s", row[0]);
    else
        product_name[0] = '\0';

    if (row[1])
        snprintf(category, cat_size, "%s", row[1]);
    else
        category[0] = '\0';

    db_free_result(res);
    return 1;
}

/* ================================================================
 *  1. 进货记录列表 —— GET /api/purchases
 * ================================================================ */

void handle_purchase_list(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---- 解析分页及筛选参数 ---- */
    int page = get_query_int(hm, "page", 1);
    int page_size = get_query_int(hm, "page_size", 20);
    if (page < 1) page = 1;
    if (page_size < 1 || page_size > 100) page_size = 20;

    char keyword[128] = {0};
    char start_date[32] = {0};
    char end_date[32] = {0};
    get_query_str(hm, "keyword", keyword, sizeof(keyword));
    get_query_str(hm, "start_date", start_date, sizeof(start_date));
    get_query_str(hm, "end_date", end_date, sizeof(end_date));

    /* ---- 获取数据库连接 ---- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json(c, 500, err);
        return;
    }

    /* ---- 动态构建 WHERE 条件 ---- */
    char where[512];
    int  where_len = 0;
    strcpy(where, "WHERE 1=1");
    where_len = strlen(where);

    /* 按商品名称或编号模糊搜索 */
    if (keyword[0]) {
        where_len += snprintf(where + where_len, sizeof(where) - where_len,
            " AND (product_code LIKE '%%%s%%' OR product_name LIKE '%%%s%%')",
            keyword, keyword);
    }
    /* 按起始日期筛选 */
    if (start_date[0]) {
        where_len += snprintf(where + where_len, sizeof(where) - where_len,
            " AND purchase_date >= '%s'", start_date);
    }
    /* 按结束日期筛选 */
    if (end_date[0]) {
        where_len += snprintf(where + where_len, sizeof(where) - where_len,
            " AND purchase_date <= '%s'", end_date);
    }

    /* ---- 查询总记录数 ---- */
    char count_sql[1024];
    snprintf(count_sql, sizeof(count_sql),
             "SELECT COUNT(*) FROM purchases %s", where);

    MYSQL_RES *res = db_query(conn, count_sql);
    int total = 0;
    if (res) {
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0]) total = atoi(row[0]);
        db_free_result(res);
    }

    int total_pages = (total > 0) ? ((total + page_size - 1) / page_size) : 1;
    int offset = (page - 1) * page_size;

    /* ---- 查询当前页数据，按进货日期降序排列 ---- */
    char data_sql[2048];
    snprintf(data_sql, sizeof(data_sql),
        "SELECT id, product_code, product_name, category, quantity, "
        "unit_price, total_cost, purchase_date, created_at "
        "FROM purchases %s ORDER BY purchase_date DESC LIMIT %d OFFSET %d",
        where, page_size, offset);

    res = db_query(conn, data_sql);

    /* ---- 构建分页 JSON 响应 ---- */
    JsonBuf jb;
    jb_init(&jb);
    jb_append(&jb, "{");
    jb_append_int(&jb, "total", total);
    jb_append(&jb, ",");
    jb_append_int(&jb, "page", page);
    jb_append(&jb, ",");
    jb_append_int(&jb, "page_size", page_size);
    jb_append(&jb, ",");
    jb_append_int(&jb, "total_pages", total_pages);
    jb_append(&jb, ",\"list\":[");

    int first = 1;
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            if (!first) jb_append(&jb, ",");
            first = 0;

            jb_append(&jb, "{");
            jb_append_int(&jb, "id",              row[0] ? atoi(row[0]) : 0);
            jb_append(&jb, ",");
            jb_append_kv(&jb,  "product_code",    row[1]);
            jb_append(&jb, ",");
            jb_append_kv(&jb,  "product_name",    row[2]);
            jb_append(&jb, ",");
            jb_append_kv(&jb,  "category",        row[3]);
            jb_append(&jb, ",");
            jb_append_int(&jb, "quantity",        row[4] ? atoi(row[4]) : 0);
            jb_append(&jb, ",");
            jb_append_float(&jb, "unit_price",    row[5] ? atof(row[5]) : 0.0);
            jb_append(&jb, ",");
            jb_append_float(&jb, "total_cost",    row[6] ? atof(row[6]) : 0.0);
            jb_append(&jb, ",");
            jb_append_kv(&jb,  "purchase_date",   row[7]);
            jb_append(&jb, ",");
            jb_append_kv(&jb,  "created_at",      row[8]);
            jb_append(&jb, "}");
        }
        db_free_result(res);
    }

    jb_append(&jb, "]}");

    char *response = json_response(200, "查询成功", jb.data);
    jb_free(&jb);
    send_json(c, 200, response);
    db_release_connection(conn);
}

/* ================================================================
 *  2. 新增进货记录（仅记录） —— POST /api/purchases
 *
 *  说明：本接口仅向 purchases 表写入一条进货记录，不会修改 inventory 表。
 *        如需同时更新库存请使用 /api/purchases/restock 接口。
 * ================================================================ */

void handle_purchase_create(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---- 解析 JSON 请求体 ---- */
    char   *product_code       = mg_json_get_str(hm->body, "$.product_code");
    long    quantity_parsed    = mg_json_get_long(hm->body, "$.quantity", 0);
    double  unit_price_val     = 0.0;
    mg_json_get_num(hm->body, "$.unit_price", &unit_price_val);
    char   *purchase_date_raw  = mg_json_get_str(hm->body, "$.purchase_date");

    /* ---- 校验必填字段 ---- */
    if (!product_code || strlen(product_code) == 0) {
        free(product_code);
        free(purchase_date_raw);
        char *err = json_error(400, "商品编号(product_code)为必填项");
        send_json(c, 400, err);
        return;
    }

    int quantity = (int)quantity_parsed;
    if (quantity <= 0) {
        free(product_code);
        free(purchase_date_raw);
        char *err = json_error(400, "进货数量(quantity)必须大于0");
        send_json(c, 400, err);
        return;
    }

    double unit_price = unit_price_val;
    if (unit_price <= 0.0) {
        free(product_code);
        free(purchase_date_raw);
        char *err = json_error(400, "进货单价(unit_price)必须大于0");
        send_json(c, 400, err);
        return;
    }

    /* 计算进货总成本 */
    double total_cost = quantity * unit_price;

    /* 进货日期：如未提供则默认当天 */
    char purchase_date[32];
    if (purchase_date_raw && strlen(purchase_date_raw) > 0) {
        snprintf(purchase_date, sizeof(purchase_date), "%s", purchase_date_raw);
    } else {
        get_today(purchase_date, sizeof(purchase_date));
    }
    free(purchase_date_raw);

    /* ---- 获取数据库连接 ---- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        free(product_code);
        char *err = json_error(500, "数据库连接失败");
        send_json(c, 500, err);
        return;
    }

    /* ---- 查询商品信息（必须存在于 inventory 表中） ---- */
    char product_name[128] = {0};
    char category[32]      = {0};
    if (!get_product_info(conn, product_code,
                          product_name, sizeof(product_name),
                          category, sizeof(category))) {
        free(product_code);
        char *err = json_error(400, "商品不存在，请先在库存管理中新增");
        send_json(c, 400, err);
        db_release_connection(conn);
        return;
    }

    /* ---- 写入进货记录 ---- */
    char sql[1024];
    snprintf(sql, sizeof(sql),
        "INSERT INTO purchases (product_code, product_name, category, "
        "quantity, unit_price, total_cost, purchase_date) "
        "VALUES ('%s', '%s', '%s', %d, %.2f, %.2f, '%s')",
        product_code, product_name, category,
        quantity, unit_price, total_cost, purchase_date);

    int affected = db_execute(conn, sql);
    if (affected < 0) {
        free(product_code);
        char *err = json_error(500, "新增进货记录失败");
        send_json(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* 获取最后插入的自增 ID */
    MYSQL_RES *id_res = db_query(conn, "SELECT LAST_INSERT_ID()");
    int new_id = 0;
    if (id_res) {
        MYSQL_ROW row = mysql_fetch_row(id_res);
        if (row && row[0]) new_id = atoi(row[0]);
        db_free_result(id_res);
    }

    /* ---- 构建响应 ---- */
    JsonBuf jb;
    jb_init(&jb);
    jb_append(&jb, "{");
    jb_append_int(&jb, "id", new_id);
    jb_append(&jb, ",");
    jb_append_kv(&jb, "product_code", product_code);
    jb_append(&jb, ",");
    jb_append_kv(&jb, "product_name", product_name);
    jb_append(&jb, ",");
    jb_append_kv(&jb, "category", category);
    jb_append(&jb, ",");
    jb_append_int(&jb, "quantity", quantity);
    jb_append(&jb, ",");
    jb_append_float(&jb, "unit_price", unit_price);
    jb_append(&jb, ",");
    jb_append_float(&jb, "total_cost", total_cost);
    jb_append(&jb, ",");
    jb_append_kv(&jb, "purchase_date", purchase_date);
    jb_append(&jb, "}");

    char *response = json_response(200, "新增进货记录成功", jb.data);
    jb_free(&jb);
    send_json(c, 200, response);

    free(product_code);
    db_release_connection(conn);
}

/* ================================================================
 *  3. 进货流程（增加库存 + 生成进货记录） —— POST /api/purchases/restock
 *
 *  核心业务逻辑：
 *    解析 {items: [{product_code, quantity, unit_price}, ...]}，
 *    在单次事务中为每件商品执行：
 *      a. 校验商品是否存在于 inventory 表 → 不存在则回滚返回 400
 *      b. 获取商品名称和分类
 *      c. UPDATE inventory SET stock_quantity = stock_quantity + quantity
 *      d. INSERT INTO purchases 生成进货记录（total_cost = quantity × unit_price）
 *    任一步骤失败即回滚全部操作。
 * ================================================================ */

void handle_purchase_restock(struct mg_connection *c, struct mg_http_message *hm)
{
    /* 单次进货最多支持 100 件商品 */
    #define MAX_RESTOCK_ITEMS 100

    /* ---- 定义数据结构 ---- */
    typedef struct {
        char   product_code[32];
        int    quantity;
        double unit_price;
    } RestockItem;

    typedef struct {
        int    id;
        char   product_code[32];
        char   product_name[128];
        char   category[32];
        int    quantity;
        double unit_price;
        double total_cost;
        char   purchase_date[32];
    } RestockResult;

    /* ---- 解析 items 数组 ---- */
    RestockItem items[MAX_RESTOCK_ITEMS];
    int         item_count = 0;

    for (int i = 0; i < MAX_RESTOCK_ITEMS; i++) {
        char path[128];

        /* 检查第 i 个元素是否存在 */
        snprintf(path, sizeof(path), "$.items[%d].product_code", i);
        char *code = mg_json_get_str(hm->body, path);
        if (!code || strlen(code) == 0) {
            free(code);
            break;  /* 数组遍历结束 */
        }
        snprintf(items[i].product_code, sizeof(items[i].product_code), "%s", code);
        free(code);

        /* 解析进货数量 */
        snprintf(path, sizeof(path), "$.items[%d].quantity", i);
        long qty = mg_json_get_long(hm->body, path, -1);
        if (qty <= 0) {
            char *err = json_error(400, "进货数量(quantity)必须大于0");
            send_json(c, 400, err);
            return;
        }
        items[i].quantity = (int)qty;

        /* 解析进货单价 */
        snprintf(path, sizeof(path), "$.items[%d].unit_price", i);
        double up = 0.0;
        mg_json_get_num(hm->body, path, &up);
        if (up <= 0.0) {
            char *err = json_error(400, "进货单价(unit_price)必须大于0");
            send_json(c, 400, err);
            return;
        }
        items[i].unit_price = up;

        item_count++;
    }

    if (item_count == 0) {
        char *err = json_error(400, "进货列表(items)不能为空");
        send_json(c, 400, err);
        return;
    }

    /* ---- 获取数据库连接并开启事务 ---- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json(c, 500, err);
        return;
    }

    if (db_begin_transaction(conn) != 0) {
        char *err = json_error(500, "开启事务失败");
        send_json(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* ---- 逐件处理 ---- */
    RestockResult results[MAX_RESTOCK_ITEMS];
    int           result_count = 0;
    double        grand_total  = 0.0;
    char          today[32];
    get_today(today, sizeof(today));

    for (int i = 0; i < item_count; i++) {
        RestockItem *item = &items[i];

        /* a. 校验商品是否存在于 inventory 表中 */
        char product_name[128] = {0};
        char category[32]      = {0};
        if (!get_product_info(conn, item->product_code,
                              product_name, sizeof(product_name),
                              category, sizeof(category))) {
            /* 商品不存在 —— 回滚事务并返回错误 */
            db_rollback(conn);
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg),
                     "商品不存在，请先在库存管理中新增: %s", item->product_code);
            char *err = json_error(400, err_msg);
            send_json(c, 400, err);
            db_release_connection(conn);
            return;
        }

        double total_cost = item->quantity * item->unit_price;

        /* b. 更新库存：增加对应商品的库存数量 */
        char update_sql[512];
        snprintf(update_sql, sizeof(update_sql),
            "UPDATE inventory SET stock_quantity = stock_quantity + %d "
            "WHERE product_code = '%s'",
            item->quantity, item->product_code);

        if (db_execute(conn, update_sql) < 0) {
            /* 库存更新失败 —— 回滚 */
            db_rollback(conn);
            char *err = json_error(500, "更新库存失败");
            send_json(c, 500, err);
            db_release_connection(conn);
            return;
        }

        /* c. 生成进货记录 */
        char insert_sql[1024];
        snprintf(insert_sql, sizeof(insert_sql),
            "INSERT INTO purchases (product_code, product_name, category, "
            "quantity, unit_price, total_cost, purchase_date) "
            "VALUES ('%s', '%s', '%s', %d, %.2f, %.2f, '%s')",
            item->product_code, product_name, category,
            item->quantity, item->unit_price, total_cost, today);

        if (db_execute(conn, insert_sql) < 0) {
            /* 进货记录写入失败 —— 回滚 */
            db_rollback(conn);
            char *err = json_error(500, "新增进货记录失败");
            send_json(c, 500, err);
            db_release_connection(conn);
            return;
        }

        /* 获取自增 ID */
        MYSQL_RES *id_res = db_query(conn, "SELECT LAST_INSERT_ID()");
        int new_id = 0;
        if (id_res) {
            MYSQL_ROW row = mysql_fetch_row(id_res);
            if (row && row[0]) new_id = atoi(row[0]);
            db_free_result(id_res);
        }

        /* 保存结果供响应使用 */
        RestockResult *r = &results[result_count++];
        r->id = new_id;
        snprintf(r->product_code, sizeof(r->product_code), "%s", item->product_code);
        snprintf(r->product_name, sizeof(r->product_name), "%s", product_name);
        snprintf(r->category, sizeof(r->category), "%s", category);
        r->quantity   = item->quantity;
        r->unit_price = item->unit_price;
        r->total_cost = total_cost;
        snprintf(r->purchase_date, sizeof(r->purchase_date), "%s", today);

        grand_total += total_cost;
    }

    /* ---- 提交事务（全部处理成功） ---- */
    if (db_commit(conn) != 0) {
        db_rollback(conn);
        char *err = json_error(500, "提交事务失败");
        send_json(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* ---- 构建响应 JSON ---- */
    JsonBuf jb;
    jb_init(&jb);
    jb_append(&jb, "{\"purchase_records\":[");

    for (int i = 0; i < result_count; i++) {
        if (i > 0) jb_append(&jb, ",");
        RestockResult *r = &results[i];
        jb_append(&jb, "{");
        jb_append_int(&jb,   "id",             r->id);
        jb_append(&jb, ",");
        jb_append_kv(&jb,    "product_code",   r->product_code);
        jb_append(&jb, ",");
        jb_append_kv(&jb,    "product_name",   r->product_name);
        jb_append(&jb, ",");
        jb_append_kv(&jb,    "category",       r->category);
        jb_append(&jb, ",");
        jb_append_int(&jb,   "quantity",       r->quantity);
        jb_append(&jb, ",");
        jb_append_float(&jb, "unit_price",     r->unit_price);
        jb_append(&jb, ",");
        jb_append_float(&jb, "total_cost",     r->total_cost);
        jb_append(&jb, ",");
        jb_append_kv(&jb,    "purchase_date",  r->purchase_date);
        jb_append(&jb, "}");
    }

    jb_append(&jb, "],");
    jb_append_float(&jb, "total_cost", grand_total);
    jb_append(&jb, "}");

    char *response = json_response(200, "进货完成，库存已更新", jb.data);
    jb_free(&jb);
    send_json(c, 200, response);
    db_release_connection(conn);
}
