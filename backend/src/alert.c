/**
 * @file    alert.c
 * @brief   库存智能预警模块 —— 自动检测并分类低库存商品
 *
 * 本模块扫描 inventory 表中所有库存数量低于安全库存的商品，
 * 按严重等级分为两个等级：
 *   - danger:  当前库存 <= 预警库存 (warning_stock)，需要立即补货
 *   - warning: 预警库存 < 当前库存 <= 安全库存 (safety_stock)，建议尽快补货
 *
 * 接口返回分类统计（danger_count / warning_count）和明细列表，
 * 支持按 level 参数筛选特定等级的商品。
 *
 * 依赖：mongoose.h（HTTP 服务）、db.h（数据库操作）、json.h（JSON 构造）
 */

#include "alert.h"
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
    "Content-Type: application/json; charset=utf-8\r\n" \
    "Access-Control-Allow-Origin: *\r\n" \
    "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/** 查询参数缓冲区大小 */
#define PARAM_BUF_SIZE   64

/** SQL 语句缓冲区大小 */
#define SQL_BUF_SIZE     2048

/** status_text 字符串缓冲区大小 */
#define STATUS_BUF_SIZE  128

/* ================================================================
 * 内部辅助函数 —— HTTP 响应
 * ================================================================ */

/**
 * 发送业务错误响应
 * 使用 json_error 构造标准错误 JSON 并通过 mg_http_reply 发送
 *
 * @param c           Mongoose 连接
 * @param http_status HTTP 状态码
 * @param code        业务错误码
 * @param message     错误提示信息
 */
static void send_error(struct mg_connection *c, int http_status,
                       int code, const char *message)
{
    char *resp = json_error(code, message);
    if (resp) {
        mg_http_reply(c, http_status, CORS_HEADERS, "%s", resp);
        free(resp);
    }
}

/**
 * 发送成功 JSON 响应
 * 使用 json_response 构造标准成功 JSON 并通过 mg_http_reply 发送
 *
 * @param c           Mongoose 连接
 * @param code        业务状态码
 * @param message     提示信息
 * @param data_json   数据 JSON 片段（可为 NULL，表示 null）
 */
static void send_response(struct mg_connection *c, int code,
                          const char *message, const char *data_json)
{
    char *resp = json_response(code, message, data_json);
    if (resp) {
        mg_http_reply(c, 200, CORS_HEADERS, "%s", resp);
        free(resp);
    }
}

/* ================================================================
 * 内部辅助函数 —— 预警等级判定
 * ================================================================ */

/**
 * 根据当前库存、安全库存、预警库存判定预警等级
 *
 * 分类规则：
 *   - 当前库存 <= 预警库存                  → "danger"
 *   - 预警库存 <  当前库存 <= 安全库存     → "warning"
 *
 * @param stock_quantity  当前库存数量
 * @param safety_stock    安全库存阈值
 * @param warning_stock   预警库存阈值
 * @return                等级字符串："danger" 或 "warning"
 */
static const char* classify_alert_level(int stock_quantity,
                                        int safety_stock,
                                        int warning_stock)
{
    if (stock_quantity <= warning_stock) {
        return "danger";
    }
    /* stock_quantity > warning_stock 但 <= safety_stock（调用方已保证） */
    return "warning";
}

/**
 * 生成预警状态的描述文本
 *
 * @param level           预警等级："danger" 或 "warning"
 * @param stock_quantity  当前库存数量
 * @param buf             输出缓冲区
 * @param buf_size        缓冲区大小
 */
static void make_status_text(const char *level, int stock_quantity,
                             char *buf, int buf_size)
{
    if (strcmp(level, "danger") == 0) {
        snprintf(buf, buf_size,
                 "严重缺货 — 仅剩 %d 件", stock_quantity);
    } else {
        snprintf(buf, buf_size,
                 "库存偏低 — 建议补货");
    }
}

/* ================================================================
 * 核心处理函数 —— 库存预警列表
 *     GET /api/alerts?level=all|danger|warning
 * ================================================================ */

void handle_alert_list(struct mg_connection *c, struct mg_http_message *hm)
{
    char level_param[PARAM_BUF_SIZE] = {0};
    MYSQL *conn = NULL;
    MYSQL_RES *result = NULL;
    JsonBuf data_jb;

    /* ---------- 解析查询参数 ---------- */
    mg_http_get_var(&hm->query, "level", level_param, sizeof(level_param));

    /* 默认值："all"；仅接受 "danger"、"warning"、"all" */
    if (level_param[0] == '\0') {
        strcpy(level_param, "all");
    }

    /* 校验 level 参数合法性 */
    int filter_level = 0;  /* 0=all, 1=danger, 2=warning */
    if (strcmp(level_param, "danger") == 0) {
        filter_level = 1;
    } else if (strcmp(level_param, "warning") == 0) {
        filter_level = 2;
    } else if (strcmp(level_param, "all") != 0) {
        /* 非法的 level 参数值 */
        send_error(c, 400, 400,
                   "level 参数无效，可选值：all、danger、warning");
        return;
    }

    /* ---------- 获取数据库连接 ---------- */
    conn = db_get_connection();
    if (conn == NULL) {
        send_error(c, 500, 500, "数据库连接失败");
        return;
    }

    /* ---------- 查询库存不足的商品 ---------- */
    const char *alert_sql =
        "SELECT product_code, product_name, category, "
        "stock_quantity, safety_stock, warning_stock, unit_price, manufacturer "
        "FROM inventory "
        "WHERE stock_quantity <= safety_stock "
        "ORDER BY stock_quantity ASC";

    result = db_query(conn, alert_sql);
    if (result == NULL) {
        db_release_connection(conn);
        send_error(c, 500, 500, "数据库查询失败");
        return;
    }

    /* ---------- 遍历结果集，分类并构建 JSON 列表 ---------- */
    int danger_count  = 0;   /* 严重缺货计数                       */
    int warning_count = 0;   /* 库存偏低计数                       */
    int total_alerts  = 0;   /* 实际返回列表中的条目数（筛选后）   */
    int first_item    = 1;   /* 列表逗号分隔控制                   */

    /*
     * 使用两阶段处理以避免对 JSON 已输出内容的回退操作：
     *   第一阶段 — 遍历所有行，统计 danger_count / warning_count
     *   第二阶段 — 再次遍历，输出符合筛选条件的行到 JSON
     *
     * 由于结果集可能较小（只含库存不足的商品），
     * 我们采用单次遍历 + 数据暂存的方式：
     *   将符合筛选条件的行信息暂存到临时数组，
     *   第一遍遍历统计全部计数（不受筛选影响），
     *   然后构建 JSON 输出。
     */

    /* 临时存储：最多存储结果集中的所有行（一般数量不大） */
    #define MAX_ALERT_ROWS  1000

    /* 每行的关键字段暂存 */
    typedef struct {
        char product_code[64];
        char product_name[256];
        char category[64];
        int  stock_quantity;
        int  safety_stock;
        int  warning_stock;
        double unit_price;
        char manufacturer[256];
        const char *level;         /* "danger" 或 "warning" */
        char status_text[STATUS_BUF_SIZE];
    } AlertRow;

    AlertRow rows[MAX_ALERT_ROWS];
    int row_count = 0;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != NULL) {
        /*
         * 字段顺序（与 SELECT 语句一致）：
         * 0:product_code  1:product_name  2:category
         * 3:stock_quantity  4:safety_stock  5:warning_stock
         * 6:unit_price     7:manufacturer
         */
        int stock_qty  = row[3] ? atoi(row[3]) : 0;
        int safety     = row[4] ? atoi(row[4]) : 0;
        int warning    = row[5] ? atoi(row[5]) : 0;

        /* 判定预警等级 */
        const char *lvl = classify_alert_level(stock_qty, safety, warning);

        /* 累计统计（不受筛选影响，始终统计全部） */
        if (strcmp(lvl, "danger") == 0) {
            danger_count++;
        } else {
            warning_count++;
        }

        /* 按 level 筛选 */
        if (filter_level == 1 && strcmp(lvl, "danger") != 0) continue;
        if (filter_level == 2 && strcmp(lvl, "warning") != 0) continue;

        /* 暂存行数据（防止超出容量） */
        if (row_count >= MAX_ALERT_ROWS) continue;

        AlertRow *ar = &rows[row_count];

        /* 复制字符串字段 */
        if (row[0]) {
            snprintf(ar->product_code, sizeof(ar->product_code), "%s", row[0]);
        } else {
            ar->product_code[0] = '\0';
        }
        if (row[1]) {
            snprintf(ar->product_name, sizeof(ar->product_name), "%s", row[1]);
        } else {
            ar->product_name[0] = '\0';
        }
        if (row[2]) {
            snprintf(ar->category, sizeof(ar->category), "%s", row[2]);
        } else {
            ar->category[0] = '\0';
        }
        if (row[7]) {
            snprintf(ar->manufacturer, sizeof(ar->manufacturer), "%s", row[7]);
        } else {
            ar->manufacturer[0] = '\0';
        }

        /* 复制数值字段 */
        ar->stock_quantity = stock_qty;
        ar->safety_stock   = safety;
        ar->warning_stock  = warning;
        ar->unit_price     = row[6] ? atof(row[6]) : 0.0;

        /* 复制等级和状态文本 */
        ar->level = lvl;
        make_status_text(lvl, stock_qty, ar->status_text, sizeof(ar->status_text));

        row_count++;
    }

    /* 释放查询结果集 */
    db_free_result(result);
    result = NULL;

    /* 根据筛选条件调整计数值用于响应 */
    if (filter_level == 1) {
        /* 仅展示 danger，warning_count 在响应中置零 */
        total_alerts = danger_count;
    } else if (filter_level == 2) {
        /* 仅展示 warning，danger_count 在响应中置零 */
        total_alerts = warning_count;
    } else {
        total_alerts = danger_count + warning_count;
    }

    /* ---------- 构造 data JSON ---------- */
    jb_init(&data_jb);
    jb_append(&data_jb, "{");

    jb_append_int(&data_jb, "total_alerts", total_alerts);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "danger_count", danger_count);
    jb_append(&data_jb, ",");
    jb_append_int(&data_jb, "warning_count", warning_count);
    jb_append(&data_jb, ",\"list\":[");

    /* 输出暂存的预警行 */
    for (int i = 0; i < row_count; i++) {
        AlertRow *ar = &rows[i];

        if (!first_item) {
            jb_append(&data_jb, ",");
        }
        first_item = 0;

        jb_append(&data_jb, "{");
        jb_append_kv(&data_jb, "product_code",   ar->product_code);
        jb_append(&data_jb, ",");
        jb_append_kv(&data_jb, "product_name",   ar->product_name);
        jb_append(&data_jb, ",");
        jb_append_kv(&data_jb, "category",       ar->category);
        jb_append(&data_jb, ",");
        jb_append_int(&data_jb, "stock_quantity", ar->stock_quantity);
        jb_append(&data_jb, ",");
        jb_append_int(&data_jb, "safety_stock",   ar->safety_stock);
        jb_append(&data_jb, ",");
        jb_append_int(&data_jb, "warning_stock",  ar->warning_stock);
        jb_append(&data_jb, ",");
        jb_append_float(&data_jb, "unit_price",   ar->unit_price);
        jb_append(&data_jb, ",");
        jb_append_kv(&data_jb, "manufacturer",   ar->manufacturer);
        jb_append(&data_jb, ",");
        jb_append_kv(&data_jb, "level",          ar->level);
        jb_append(&data_jb, ",");
        jb_append_kv(&data_jb, "status_text",    ar->status_text);
        jb_append(&data_jb, "}");
    }

    jb_append(&data_jb, "]}");

    /* ---------- 发送响应 ---------- */
    send_response(c, 200, "查询成功", data_jb.data);

    /* ---------- 清理 ---------- */
    jb_free(&data_jb);
    db_release_connection(conn);
}
