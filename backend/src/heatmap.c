/**
 * @file    heatmap.c
 * @brief   销售热力分析模块 —— 品类热力排行与销售分布热力图数据
 *
 * 本模块提供两个 HTTP 处理函数，分别对应：
 *   1. 品类销售热力排行（按品类汇总，计算占比与热力等级）
 *   2. 销售热力分布数据（按天/周/月维度，含各品类细分）
 *
 * 依赖：db.h, json.h, mongoose.h, MySQL C API
 */

#include "heatmap.h"
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

#define MAX_SQL_LEN      8192
#define MAX_STR_LEN      512
#define MAX_BODY_LEN     65536
#define MAX_ROWS         200
#define MAX_CAT_BREAKDOWN 2000
#define DEFAULT_DAYS     30

/* 星期名称查找表 */
static const char *day_of_week_names[] = {
    "周日", "周一", "周二", "周三", "周四", "周五", "周六"
};

/* ================================================================
 * 内部数据结构
 * ================================================================ */

/** 品类热力排行缓存行 */
typedef struct {
    char   category[MAX_STR_LEN];
    double total_sales;
    int    total_quantity;
    int    product_count;
    double percentage;
    char   heat_level[8];   /* "hot" / "warm" / "cool" */
} CategoryHeatRow;

/** 热力图主数据行（按天/周/月） */
typedef struct {
    char   date_label[48];     /* 日期标签 */
    char   day_of_week[8];     /* 星期几（仅 daily 维度有效） */
    double total_amount;
    int    total_orders;
} HeatmapRow;

/** 品类细分数据 */
typedef struct {
    char   period_key[48];     /* 与 HeatmapRow.date_label 对齐的键 */
    char   category[MAX_STR_LEN];
    double amount;
    int    quantity;
} CatBreakdown;

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
 * 内部辅助函数 —— 查询参数提取
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

/* ================================================================
 * 内部辅助函数 —— 日期工具
 * ================================================================ */

/**
 * 获取当前日期字符串，格式 "YYYY-MM-DD"
 */
static void get_today_date(char *buf, int buf_size)
{
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    strftime(buf, buf_size, "%Y-%m-%d", tm_now);
}

/**
 * 获取 N 天前的日期字符串，格式 "YYYY-MM-DD"
 */
static void get_date_days_ago(char *buf, int buf_size, int days_ago)
{
    time_t t = time(NULL) - (time_t)days_ago * 86400;
    struct tm *tm_val = localtime(&t);
    strftime(buf, buf_size, "%Y-%m-%d", tm_val);
}

/**
 * 根据日期字符串 "YYYY-MM-DD" 计算星期几
 * 返回静态字符串指针：周日/周一/周二/...
 */
static const char* get_day_of_week(const char *date_str)
{
    int y = 0, m = 0, d = 0;
    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3) return "";
    if (m < 1 || m > 12 || d < 1 || d > 31) return "";

    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = y - 1900;
    tm_val.tm_mon  = m - 1;
    tm_val.tm_mday = d;
    tm_val.tm_isdst = -1;

    time_t t = mktime(&tm_val);
    if (t == (time_t)-1) return "";

    if (tm_val.tm_wday < 0 || tm_val.tm_wday > 6) return "";
    return day_of_week_names[tm_val.tm_wday];
}

/**
 * 根据日期字符串计算所在周的周一~周日范围
 * 结果格式 "YYYY-MM-DD~YYYY-MM-DD"
 */
static void get_week_range(const char *date_str, char *out, int out_size)
{
    int y = 0, m = 0, d = 0;
    if (sscanf(date_str, "%d-%d-%d", &y, &m, &d) != 3) {
        out[0] = '\0';
        return;
    }

    struct tm tm_val;
    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = y - 1900;
    tm_val.tm_mon  = m - 1;
    tm_val.tm_mday = d;
    tm_val.tm_isdst = -1;

    time_t t = mktime(&tm_val);
    if (t == (time_t)-1) { out[0] = '\0'; return; }

    int wday = tm_val.tm_wday;            /* 0=周日, 1=周一, ..., 6=周六 */
    int days_to_monday = (wday == 0) ? 6 : (wday - 1);
    time_t monday_t     = t - (time_t)days_to_monday * 86400;
    time_t sunday_t     = monday_t + 6 * 86400;

    struct tm mon_tm_val;
    struct tm sun_tm_val;
    {
        struct tm *tmp = localtime(&monday_t);
        if (!tmp) { out[0] = '\0'; return; }
        mon_tm_val = *tmp;
        tmp = localtime(&sunday_t);
        if (!tmp) { out[0] = '\0'; return; }
        sun_tm_val = *tmp;
    }

    snprintf(out, out_size, "%04d-%02d-%02d~%04d-%02d-%02d",
             mon_tm_val.tm_year + 1900, mon_tm_val.tm_mon + 1, mon_tm_val.tm_mday,
             sun_tm_val.tm_year + 1900, sun_tm_val.tm_mon + 1, sun_tm_val.tm_mday);
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
 * 1. handle_heatmap_category —— 品类销售热力排行
 *
 * 按品类汇总指定时间范围内的销售额、销量、商品数，
 * 计算每个品类的销售占比，并标注热力等级。
 *
 * 热力等级规则：
 *   hot   —— 占比 >= 25%
 *   warm  —— 占比 10% ~ 25%
 *   cool  —— 占比 < 10%
 * ================================================================ */

void handle_heatmap_category(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取查询参数 ---------- */
    char start_date[MAX_STR_LEN] = {0};
    char end_date[MAX_STR_LEN]   = {0};

    get_query_param(hm, "start_date", start_date, sizeof(start_date), NULL);
    get_query_param(hm, "end_date",   end_date,   sizeof(end_date),   NULL);

    /* 默认时间范围：最近 30 天 */
    if (end_date[0] == '\0') {
        get_today_date(end_date, sizeof(end_date));
    }
    if (start_date[0] == '\0') {
        get_date_days_ago(start_date, sizeof(start_date), DEFAULT_DAYS);
    }

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    /* ---------- 转义日期参数 ---------- */
    char esc_start[MAX_STR_LEN * 2 + 1];
    char esc_end[MAX_STR_LEN * 2 + 1];
    mysql_escape(esc_start, start_date, conn);
    mysql_escape(esc_end,   end_date,   conn);

    /* ---------- 查询品类汇总 ---------- */
    char sql[MAX_SQL_LEN];
    snprintf(sql, sizeof(sql),
             "SELECT category, SUM(total_amount) as total_sales, "
             "SUM(quantity) as total_quantity, "
             "COUNT(DISTINCT product_code) as product_count "
             "FROM sales "
             "WHERE sale_date BETWEEN '%s' AND '%s' "
             "GROUP BY category "
             "ORDER BY total_sales DESC",
             esc_start, esc_end);

    MYSQL_RES *cat_res = db_query(conn, sql);
    if (!cat_res) {
        char *err = json_error(500, "查询品类汇总失败");
        send_json_resp_status(c, 500, err);
        db_release_connection(conn);
        return;
    }

    /* 读取品类数据到数组 */
    CategoryHeatRow rows[MAX_ROWS];
    int row_count = 0;
    {
        int num = (int)mysql_num_rows(cat_res);
        for (int i = 0; i < num && i < MAX_ROWS; i++) {
            MYSQL_ROW row = mysql_fetch_row(cat_res);
            if (!row) break;
            if (row[0]) {
                strncpy(rows[i].category, row[0], MAX_STR_LEN - 1);
                rows[i].category[MAX_STR_LEN - 1] = '\0';
            } else {
                rows[i].category[0] = '\0';
            }
            rows[i].total_sales    = row[1] ? atof(row[1]) : 0.0;
            rows[i].total_quantity = row[2] ? atoi(row[2]) : 0;
            rows[i].product_count  = row[3] ? atoi(row[3]) : 0;
            rows[i].percentage     = 0.0;
            rows[i].heat_level[0]  = '\0';
            row_count++;
        }
    }
    db_free_result(cat_res);

    /* ---------- 查询总销售额 ---------- */
    double grand_total = 0.0;
    char total_sql[MAX_SQL_LEN];
    snprintf(total_sql, sizeof(total_sql),
             "SELECT COALESCE(SUM(total_amount), 0) FROM sales "
             "WHERE sale_date BETWEEN '%s' AND '%s'",
             esc_start, esc_end);

    MYSQL_RES *total_res = db_query(conn, total_sql);
    if (total_res) {
        MYSQL_ROW row = mysql_fetch_row(total_res);
        if (row && row[0]) grand_total = atof(row[0]);
        db_free_result(total_res);
    }

    /* ---------- 计算百分比与热力等级 ---------- */
    for (int i = 0; i < row_count; i++) {
        if (grand_total > 0.0) {
            rows[i].percentage = round(rows[i].total_sales / grand_total * 10000.0) / 100.0;
        } else {
            rows[i].percentage = 0.0;
        }

        if (rows[i].percentage >= 25.0) {
            strcpy(rows[i].heat_level, "hot");
        } else if (rows[i].percentage >= 10.0) {
            strcpy(rows[i].heat_level, "warm");
        } else {
            strcpy(rows[i].heat_level, "cool");
        }
    }

    db_release_connection(conn);

    /* ---------- 构造 JSON 响应 ---------- */
    /* 构造 categories 数组 */
    JsonBuf jb_cats;
    jb_init(&jb_cats);
    jb_append(&jb_cats, "[");
    for (int i = 0; i < row_count; i++) {
        if (i > 0) jb_append(&jb_cats, ",");
        jb_append(&jb_cats, "{");
        jb_append_kv(&jb_cats, "category", rows[i].category);
        jb_append(&jb_cats, ",");
        jb_append_float(&jb_cats, "total_sales", rows[i].total_sales);
        jb_append(&jb_cats, ",");
        jb_append_int(&jb_cats, "total_quantity", rows[i].total_quantity);
        jb_append(&jb_cats, ",");
        jb_append_int(&jb_cats, "product_count", rows[i].product_count);
        jb_append(&jb_cats, ",");
        jb_append_float(&jb_cats, "percentage", rows[i].percentage);
        jb_append(&jb_cats, ",");
        jb_append_kv(&jb_cats, "heat_level", rows[i].heat_level);
        jb_append(&jb_cats, "}");
    }
    jb_append(&jb_cats, "]");
    const char *cats_json = jb_get(&jb_cats);

    /* 构造 data 对象 */
    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append(&jb_data, "\"period\":{\"start\":\"%s\",\"end\":\"%s\"}", start_date, end_date);
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"categories\":%s", cats_json);
    jb_append(&jb_data, ",");
    jb_append_float(&jb_data, "total_amount", grand_total);
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "查询成功", jb_get(&jb_data));

    jb_free(&jb_cats);
    jb_free(&jb_data);

    send_json_resp(c, resp);
}

/* ================================================================
 * 2. handle_heatmap_sales —— 销售热力分布数据
 *
 * 按指定维度（daily / weekly / monthly）汇总销售额与订单数，
 * 同时提供各品类细分数据与汇总统计（最佳销售日、最佳品类等）。
 *
 * 支持的维度：
 *   daily   —— 按天汇总（默认）
 *   weekly  —— 按周汇总
 *   monthly —— 按月汇总
 * ================================================================ */

void handle_heatmap_sales(struct mg_connection *c, struct mg_http_message *hm)
{
    /* ---------- 提取查询参数 ---------- */
    char dimension[MAX_STR_LEN]  = {0};
    char start_date[MAX_STR_LEN] = {0};
    char end_date[MAX_STR_LEN]   = {0};
    char category[MAX_STR_LEN]   = {0};

    get_query_param(hm, "dimension",  dimension,  sizeof(dimension),  "daily");
    get_query_param(hm, "start_date", start_date, sizeof(start_date), NULL);
    get_query_param(hm, "end_date",   end_date,   sizeof(end_date),   NULL);
    get_query_param(hm, "category",   category,   sizeof(category),   NULL);

    /* 验证维度参数 */
    int is_daily   = (strcmp(dimension, "daily")   == 0);
    int is_weekly  = (strcmp(dimension, "weekly")  == 0);
    int is_monthly = (strcmp(dimension, "monthly") == 0);
    if (!is_daily && !is_weekly && !is_monthly) {
        is_daily = 1;
        strcpy(dimension, "daily");
    }

    /* 默认时间范围：最近 30 天 */
    if (end_date[0] == '\0') {
        get_today_date(end_date, sizeof(end_date));
    }
    if (start_date[0] == '\0') {
        get_date_days_ago(start_date, sizeof(start_date), DEFAULT_DAYS);
    }

    /* ---------- 获取数据库连接 ---------- */
    MYSQL *conn = db_get_connection();
    if (!conn) {
        char *err = json_error(500, "数据库连接失败");
        send_json_resp_status(c, 500, err);
        return;
    }

    /* ---------- 转义参数 ---------- */
    char esc_start[MAX_STR_LEN * 2 + 1];
    char esc_end[MAX_STR_LEN * 2 + 1];
    mysql_escape(esc_start, start_date, conn);
    mysql_escape(esc_end,   end_date,   conn);

    /* ---------- 构造 WHERE 子句 ---------- */
    char where_clause[MAX_SQL_LEN];
    snprintf(where_clause, sizeof(where_clause),
             "WHERE sale_date BETWEEN '%s' AND '%s'", esc_start, esc_end);

    if (category[0] != '\0') {
        char esc_cat[MAX_STR_LEN * 2 + 1];
        mysql_escape(esc_cat, category, conn);
        char cat_cond[MAX_SQL_LEN];
        snprintf(cat_cond, sizeof(cat_cond), " AND category = '%s'", esc_cat);
        strcat(where_clause, cat_cond);
    }

    /* ---------- 构造主查询与品类细分查询 ---------- */
    char main_sql[MAX_SQL_LEN];
    char cat_sql[MAX_SQL_LEN];

    if (is_daily) {
        /* 按天汇总主查询 */
        snprintf(main_sql, sizeof(main_sql),
                 "SELECT sale_date, SUM(total_amount), COUNT(*) "
                 "FROM sales %s "
                 "GROUP BY sale_date "
                 "ORDER BY sale_date",
                 where_clause);

        /* 按天 + 品类细分查询 */
        snprintf(cat_sql, sizeof(cat_sql),
                 "SELECT sale_date, category, SUM(total_amount), SUM(quantity) "
                 "FROM sales %s "
                 "GROUP BY sale_date, category "
                 "ORDER BY sale_date, category",
                 where_clause);
    } else if (is_weekly) {
        /* 按周汇总主查询：以周一为周期起始 */
        snprintf(main_sql, sizeof(main_sql),
                 "SELECT "
                 "DATE_SUB(sale_date, INTERVAL WEEKDAY(sale_date) DAY) as week_start, "
                 "SUM(total_amount), COUNT(*) "
                 "FROM sales %s "
                 "GROUP BY week_start "
                 "ORDER BY week_start",
                 where_clause);

        /* 按周 + 品类细分查询 */
        snprintf(cat_sql, sizeof(cat_sql),
                 "SELECT "
                 "DATE_SUB(sale_date, INTERVAL WEEKDAY(sale_date) DAY) as week_start, "
                 "category, SUM(total_amount), SUM(quantity) "
                 "FROM sales %s "
                 "GROUP BY week_start, category "
                 "ORDER BY week_start, category",
                 where_clause);
    } else {
        /* 按月汇总主查询 */
        snprintf(main_sql, sizeof(main_sql),
                 "SELECT DATE_FORMAT(sale_date, '%%Y-%%m') as month_label, "
                 "SUM(total_amount), COUNT(*) "
                 "FROM sales %s "
                 "GROUP BY month_label "
                 "ORDER BY month_label",
                 where_clause);

        /* 按月 + 品类细分查询 */
        snprintf(cat_sql, sizeof(cat_sql),
                 "SELECT DATE_FORMAT(sale_date, '%%Y-%%m') as month_label, "
                 "category, SUM(total_amount), SUM(quantity) "
                 "FROM sales %s "
                 "GROUP BY month_label, category "
                 "ORDER BY month_label, category",
                 where_clause);
    }

    /* ---------- 执行主查询 ---------- */
    MYSQL_RES *main_res = db_query(conn, main_sql);
    if (!main_res) {
        char *err = json_error(500, "查询销售汇总失败");
        send_json_resp_status(c, 500, err);
        db_release_connection(conn);
        return;
    }

    HeatmapRow hrows[MAX_ROWS];
    int hrow_count = 0;
    {
        int num = (int)mysql_num_rows(main_res);
        for (int i = 0; i < num && i < MAX_ROWS; i++) {
            MYSQL_ROW row = mysql_fetch_row(main_res);
            if (!row) break;

            if (is_daily && row[0]) {
                /* 按天：date_label 为原始日期 + 星期 */
                strncpy(hrows[i].date_label, row[0], sizeof(hrows[i].date_label) - 1);
                hrows[i].date_label[sizeof(hrows[i].date_label) - 1] = '\0';
            } else if (is_weekly && row[0]) {
                /* 按周：从周一日期计算周范围标签 */
                char week_range[48] = {0};
                get_week_range(row[0], week_range, sizeof(week_range));
                if (week_range[0] != '\0') {
                    snprintf(hrows[i].date_label, sizeof(hrows[i].date_label), "%s", week_range);
                } else {
                    strncpy(hrows[i].date_label, row[0], sizeof(hrows[i].date_label) - 1);
                }
                hrows[i].date_label[sizeof(hrows[i].date_label) - 1] = '\0';
            } else if (is_monthly && row[0]) {
                snprintf(hrows[i].date_label, sizeof(hrows[i].date_label),
                         "%s", row[0]);
            } else {
                hrows[i].date_label[0] = '\0';
            }

            hrows[i].total_amount = row[1] ? atof(row[1]) : 0.0;
            hrows[i].total_orders = row[2] ? atoi(row[2]) : 0;
            hrows[i].day_of_week[0] = '\0';
            hrow_count++;
        }
    }
    db_free_result(main_res);

    /* 为 daily 维度补充星期几 */
    if (is_daily) {
        for (int i = 0; i < hrow_count; i++) {
            const char *dow = get_day_of_week(hrows[i].date_label);
            if (dow && dow[0] != '\0') {
                strncpy(hrows[i].day_of_week, dow, sizeof(hrows[i].day_of_week) - 1);
                hrows[i].day_of_week[sizeof(hrows[i].day_of_week) - 1] = '\0';
            }
        }
    }

    /* ---------- 执行品类细分查询 ---------- */
    CatBreakdown breakdowns[MAX_CAT_BREAKDOWN];
    int bd_count = 0;

    MYSQL_RES *cat_res = db_query(conn, cat_sql);
    if (cat_res) {
        int num = (int)mysql_num_rows(cat_res);
        for (int i = 0; i < num && i < MAX_CAT_BREAKDOWN; i++) {
            MYSQL_ROW row = mysql_fetch_row(cat_res);
            if (!row) break;

            if (is_daily && row[0]) {
                /* 直接使用日期作为键 */
                strncpy(breakdowns[i].period_key, row[0],
                        sizeof(breakdowns[i].period_key) - 1);
            } else if (is_weekly && row[0]) {
                /* 计算周范围作为键以匹配主数据行 */
                char week_range[48] = {0};
                get_week_range(row[0], week_range, sizeof(week_range));
                if (week_range[0] != '\0') {
                    snprintf(breakdowns[i].period_key, sizeof(breakdowns[i].period_key), "%s", week_range);
                } else {
                    strncpy(breakdowns[i].period_key, row[0],
                            sizeof(breakdowns[i].period_key) - 1);
                }
            } else if (is_monthly && row[0]) {
                snprintf(breakdowns[i].period_key,
                         sizeof(breakdowns[i].period_key), "%s", row[0]);
            } else {
                breakdowns[i].period_key[0] = '\0';
            }
            breakdowns[i].period_key[sizeof(breakdowns[i].period_key) - 1] = '\0';

            if (row[1]) {
                strncpy(breakdowns[i].category, row[1], MAX_STR_LEN - 1);
                breakdowns[i].category[MAX_STR_LEN - 1] = '\0';
            } else {
                breakdowns[i].category[0] = '\0';
            }
            breakdowns[i].amount   = row[2] ? atof(row[2]) : 0.0;
            breakdowns[i].quantity = row[3] ? atoi(row[3]) : 0;
            bd_count++;
        }
        db_free_result(cat_res);
    }

    /* ---------- 释放数据库连接 ---------- */
    db_release_connection(conn);

    /* ---------- 计算汇总信息 ---------- */
    double total_amount    = 0.0;
    double best_period_amt = 0.0;
    int    best_period_idx = -1;

    /* 统计各品类总销售额（用于确定 best_category） */
    typedef struct {
        char   name[MAX_STR_LEN];
        double total;
    } CatTotal;
    CatTotal cat_totals[MAX_ROWS];
    int cat_total_count = 0;

    for (int i = 0; i < hrow_count; i++) {
        total_amount += hrows[i].total_amount;
        if (hrows[i].total_amount > best_period_amt) {
            best_period_amt = hrows[i].total_amount;
            best_period_idx = i;
        }
    }

    /* 从品类细分数据中汇总各品类销售额 */
    for (int i = 0; i < bd_count; i++) {
        int found = 0;
        for (int j = 0; j < cat_total_count; j++) {
            if (strcmp(cat_totals[j].name, breakdowns[i].category) == 0) {
                cat_totals[j].total += breakdowns[i].amount;
                found = 1;
                break;
            }
        }
        if (!found && cat_total_count < MAX_ROWS) {
            strncpy(cat_totals[cat_total_count].name,
                    breakdowns[i].category, MAX_STR_LEN - 1);
            cat_totals[cat_total_count].name[MAX_STR_LEN - 1] = '\0';
            cat_totals[cat_total_count].total = breakdowns[i].amount;
            cat_total_count++;
        }
    }

    /* 找出最佳品类 */
    double best_cat_amt = 0.0;
    char   best_cat_name[MAX_STR_LEN] = {0};
    for (int i = 0; i < cat_total_count; i++) {
        if (cat_totals[i].total > best_cat_amt) {
            best_cat_amt = cat_totals[i].total;
            snprintf(best_cat_name, MAX_STR_LEN, "%s", cat_totals[i].name);
        }
    }

    /* 计算日均/周均/月均 */
    double avg_amount = 0.0;
    if (hrow_count > 0) {
        avg_amount = round(total_amount / hrow_count * 100.0) / 100.0;
    }

    /* ---------- 构造 JSON 响应 ---------- */
    const char *avg_label = is_daily ? "daily_avg_amount"
                          : (is_weekly ? "weekly_avg_amount"
                          : "monthly_avg_amount");

    const char *best_label = is_daily ? "best_day"
                           : (is_weekly ? "best_week"
                           : "best_month");

    /* 构造 heatmap_data 数组 */
    JsonBuf jb_data_arr;
    jb_init(&jb_data_arr);
    jb_append(&jb_data_arr, "[");
    for (int i = 0; i < hrow_count; i++) {
        if (i > 0) jb_append(&jb_data_arr, ",");

        /* 构建该时间段内的 categories 对象 */
        JsonBuf jb_cats;
        jb_init(&jb_cats);
        jb_append(&jb_cats, "{");
        int cat_appended = 0;
        for (int k = 0; k < bd_count; k++) {
            if (strcmp(breakdowns[k].period_key, hrows[i].date_label) == 0) {
                if (cat_appended > 0) jb_append(&jb_cats, ",");
                jb_append(&jb_cats, "\"%s\":{", breakdowns[k].category);
                jb_append_float(&jb_cats, "amount", breakdowns[k].amount);
                jb_append(&jb_cats, ",");
                jb_append_int(&jb_cats, "quantity", breakdowns[k].quantity);
                jb_append(&jb_cats, "}");
                cat_appended++;
            }
        }
        jb_append(&jb_cats, "}");

        jb_append(&jb_data_arr, "{");
        jb_append_kv(&jb_data_arr, "date", hrows[i].date_label);
        jb_append(&jb_data_arr, ",");
        if (is_daily) {
            jb_append_kv(&jb_data_arr, "day_of_week", hrows[i].day_of_week);
            jb_append(&jb_data_arr, ",");
        }
        jb_append_float(&jb_data_arr, "total_amount", hrows[i].total_amount);
        jb_append(&jb_data_arr, ",");
        jb_append_int(&jb_data_arr, "total_orders", hrows[i].total_orders);
        jb_append(&jb_data_arr, ",");
        jb_append(&jb_data_arr, "\"categories\":%s", jb_get(&jb_cats));
        jb_append(&jb_data_arr, "}");

        jb_free(&jb_cats);
    }
    jb_append(&jb_data_arr, "]");
    const char *data_arr_json = jb_get(&jb_data_arr);

    /* 构造 summary 对象 */
    JsonBuf jb_summary;
    jb_init(&jb_summary);
    jb_append(&jb_summary, "{");
    /* best_period */
    jb_append(&jb_summary, "\"%s\":{", best_label);
    if (best_period_idx >= 0) {
        jb_append_kv(&jb_summary, "date", hrows[best_period_idx].date_label);
        jb_append(&jb_summary, ",");
        jb_append_float(&jb_summary, "total_amount", hrows[best_period_idx].total_amount);
        jb_append(&jb_summary, ",");
        jb_append_int(&jb_summary, "total_orders", hrows[best_period_idx].total_orders);
    }
    jb_append(&jb_summary, "}");
    jb_append(&jb_summary, ",");
    /* best_category */
    jb_append(&jb_summary, "\"best_category\":{");
    jb_append_kv(&jb_summary, "name", best_cat_name);
    jb_append(&jb_summary, ",");
    jb_append_float(&jb_summary, "total_amount", best_cat_amt);
    jb_append(&jb_summary, "}");
    jb_append(&jb_summary, ",");
    /* avg */
    jb_append_float(&jb_summary, avg_label, avg_amount);
    jb_append(&jb_summary, ",");
    /* total */
    jb_append_float(&jb_summary, "total_amount", total_amount);
    jb_append(&jb_summary, "}");

    /* 构造 data 对象 */
    JsonBuf jb_data;
    jb_init(&jb_data);
    jb_append(&jb_data, "{");
    jb_append_kv(&jb_data, "dimension", dimension);
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"period\":{\"start\":\"%s\",\"end\":\"%s\"}", start_date, end_date);
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"heatmap_data\":%s", data_arr_json);
    jb_append(&jb_data, ",");
    jb_append(&jb_data, "\"summary\":%s", jb_get(&jb_summary));
    jb_append(&jb_data, "}");

    char *resp = json_response(200, "查询成功", jb_get(&jb_data));

    /* 释放所有临时缓冲区 */
    jb_free(&jb_data_arr);
    jb_free(&jb_summary);
    jb_free(&jb_data);

    send_json_resp(c, resp);
}
