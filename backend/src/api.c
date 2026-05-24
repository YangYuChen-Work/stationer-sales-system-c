/* ================================================================
 * 路由分发模块 - api.c
 *
 * 全局 HTTP 请求路由入口。接收所有 HTTP 请求，
 * 根据 HTTP 方法 + URI 路径将其分派到对应的业务处理函数。
 *
 * 路由匹配优先级：从具体到宽泛，越具体的路径越靠前匹配。
 * 例如 /api/sales/sorted 必须在 /api/sales 之前检查，
 * 避免被宽泛路径提前捕获。
 * ================================================================ */

#include "api.h"

/* ----- 已有业务模块 ----- */
#include "inventory.h"      /* handle_inventory_*    */
#include "sales.h"          /* handle_sales_*        */
#include "purchase.h"       /* handle_purchase_*     */
#include "json.h"           /* json_error            */

/* ----- 未来待实现的业务模块（取消注释即可启用） ----- */
#include "search.h"       /* handle_search          */
#include "stats.h"          /* handle_stats_*         */
#include "alert.h"        /* handle_alert_list      */
#include "suggest.h"      /* handle_suggest_list    */
#include "heatmap.h"       /* handle_heatmap_*       */
#include "ai_agent.h"     /* handle_ai_*            */

/* ================================================================
 * CORS 预检请求处理
 * ================================================================ */
void api_handle_cors(struct mg_connection *c, struct mg_http_message *hm) {
    (void)hm;  /* 未使用，消除编译警告 */
    mg_http_reply(c, 204,
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n",
        "");
}

/* ================================================================
 * 辅助宏：方法比较
 *
 * mg_vcmp 返回 0 表示相等，以下宏简化书写。
 * ================================================================ */
#define METHOD_IS(m)  (mg_strcmp(hm->method, mg_str(m)) == 0)
#define URI_IS(p)      (mg_strcmp(hm->uri, mg_str(p)) == 0)
#define URI_MATCH(p)   mg_match(hm->uri, mg_str(p), NULL)

/* ================================================================
 * 路由分发入口
 *
 * 路由表结构（优先级从高到低）：
 *
 *   OPTIONS 任意路径          → CORS 预检
 *
 *   库存模块:
 *     GET    /api/inventory/{code}  → 单品查询（通配符必须在精确路径之后）
 *     GET    /api/inventory      → 库存列表（分页+筛选）
 *     POST   /api/inventory      → 新增商品
 *     PUT    /api/inventory/{code}  → 更新商品
 *     DELETE /api/inventory/{code}  → 删除商品
 *
 *   销售模块（具体路径优先!）:
 *     GET    /api/sales/sorted   → 销售记录排序
 *     POST   /api/sales/checkout → 销售结账（事务）
 *     GET    /api/sales          → 销售记录列表
 *     POST   /api/sales          → 新增销售记录
 *
 *   进货模块（具体路径优先!）:
 *     POST   /api/purchases/restock → 进货流程（事务）
 *     GET    /api/purchases         → 进货记录列表
 *     POST   /api/purchases         → 新增进货记录
 *
 *   未来模块（已注释，取消注释即可启用）:
 *     GET    /api/search             → 全局搜索
 *     GET    /api/stats/overview     → 统计概览
 *     GET    /api/stats/eraser       → 橡皮擦专项统计
 *     GET    /api/alerts             → 库存预警列表
 *     GET    /api/suggestions        → 采购建议列表
 *     GET    /api/heatmap/category   → 热力图-品类分布
 *     GET    /api/heatmap/sales      → 热力图-销售数据
 *     POST   /api/ai/ask             → AI 问答
 *     POST   /api/ai/forecast        → AI 销量预测
 *     POST   /api/ai/recommend       → AI 采购推荐
 *
 *   默认: 404
 * ================================================================ */
void api_route(struct mg_connection *c, struct mg_http_message *hm) {

    /* ---- CORS 预检请求 ---- */
    if (METHOD_IS("OPTIONS")) {
        api_handle_cors(c, hm);
        return;
    }

    /* ================================================================
     * 库存管理模块
     * ================================================================ */

    /* GET /api/inventory —— 库存列表（分页 + 多条件筛选） */
    if (METHOD_IS("GET") && URI_IS("/api/inventory")) {
        handle_inventory_list(c, hm);
        return;
    }

    /* GET /api/inventory/{product_code} —— 单品查询 */
    if (METHOD_IS("GET") && URI_MATCH("/api/inventory/*")) {
        handle_inventory_get(c, hm);
        return;
    }

    /* POST /api/inventory —— 新增库存商品 */
    if (METHOD_IS("POST") && URI_IS("/api/inventory")) {
        handle_inventory_create(c, hm);
        return;
    }

    /* PUT /api/inventory/{product_code} —— 更新库存商品 */
    if (METHOD_IS("PUT") && URI_MATCH("/api/inventory/*")) {
        handle_inventory_update(c, hm);
        return;
    }

    /* DELETE /api/inventory/{product_code} —— 删除库存商品 */
    if (METHOD_IS("DELETE") && URI_MATCH("/api/inventory/*")) {
        handle_inventory_delete(c, hm);
        return;
    }

    /* ================================================================
     * 销售管理模块
     * 注意：/api/sales/sorted 和 /api/sales/checkout 必须在
     * /api/sales 之前匹配，避免被宽泛路径提前捕获！
     * ================================================================ */

    /* GET /api/sales/sorted —— 销售记录按日期排序 */
    if (METHOD_IS("GET") && URI_IS("/api/sales/sorted")) {
        handle_sales_sorted(c, hm);
        return;
    }

    /* POST /api/sales/checkout —— 销售结账（扣减库存 + 生成记录，事务保护） */
    if (METHOD_IS("POST") && URI_IS("/api/sales/checkout")) {
        handle_sales_checkout(c, hm);
        return;
    }

    /* GET /api/sales —— 销售记录列表（分页 + 多条件筛选） */
    if (METHOD_IS("GET") && URI_IS("/api/sales")) {
        handle_sales_list(c, hm);
        return;
    }

    /* POST /api/sales —— 新增单条销售记录（仅记录，不扣减库存） */
    if (METHOD_IS("POST") && URI_IS("/api/sales")) {
        handle_sales_create(c, hm);
        return;
    }

    /* ================================================================
     * 进货管理模块
     * 注意：/api/purchases/restock 必须在 /api/purchases 之前匹配！
     * ================================================================ */

    /* POST /api/purchases/restock —— 进货流程（更新库存 + 生成进货记录，事务保护） */
    if (METHOD_IS("POST") && URI_IS("/api/purchases/restock")) {
        handle_purchase_restock(c, hm);
        return;
    }

    /* GET /api/purchases —— 进货记录列表 */
    if (METHOD_IS("GET") && URI_IS("/api/purchases")) {
        handle_purchase_list(c, hm);
        return;
    }

    /* POST /api/purchases —— 新增进货记录（仅记录，不影响库存） */
    if (METHOD_IS("POST") && URI_IS("/api/purchases")) {
        handle_purchase_create(c, hm);
        return;
    }

    /* ================================================================
     * 未来待实现模块（取消注释对应的 #include 和路由即可启用）
     * ================================================================ */
#if 1  /* --- 搜索模块 --- */
    /* GET /api/search —— 全局搜索 */
    if (METHOD_IS("GET") && URI_IS("/api/search")) {
        handle_search(c, hm);
        return;
    }
#endif

#if 1  /* --- 统计模块 --- */
    /* GET /api/stats/overview —— 统计概览 */
    if (METHOD_IS("GET") && URI_IS("/api/stats/overview")) {
        handle_stats_overview(c, hm);
        return;
    }

    /* GET /api/stats/eraser —— 橡皮擦专项统计 */
    if (METHOD_IS("GET") && URI_IS("/api/stats/eraser")) {
        handle_stats_eraser(c, hm);
        return;
    }
#endif

    /* --- 预警模块 --- */
    /* GET /api/alerts —— 库存预警列表 */
    if (METHOD_IS("GET") && URI_IS("/api/alerts")) {
        handle_alert_list(c, hm);
        return;
    }

/* --- 采购建议模块 --- */
    /* GET /api/suggestions —— 采购建议列表 */
    if (METHOD_IS("GET") && URI_IS("/api/suggestions")) {
        handle_suggest_list(c, hm);
        return;
    }


#if 1  /* --- 热力图模块 --- */
    /* GET /api/heatmap/category —— 热力图-品类分布 */
    if (METHOD_IS("GET") && URI_IS("/api/heatmap/category")) {
        handle_heatmap_category(c, hm);
        return;
    }

    /* GET /api/heatmap/sales —— 热力图-销售数据 */
    if (METHOD_IS("GET") && URI_IS("/api/heatmap/sales")) {
        handle_heatmap_sales(c, hm);
        return;
    }
#endif

#if 1  /* --- AI 智能代理模块 --- */
    /* POST /api/ai/ask —— AI 问答 */
    if (METHOD_IS("POST") && URI_IS("/api/ai/ask")) {
        handle_ai_ask(c, hm);
        return;
    }

    /* POST /api/ai/forecast —— AI 销量预测 */
    if (METHOD_IS("POST") && URI_IS("/api/ai/forecast")) {
        handle_ai_forecast(c, hm);
        return;
    }

    /* POST /api/ai/recommend —— AI 采购推荐 */
    if (METHOD_IS("POST") && URI_IS("/api/ai/recommend")) {
        handle_ai_recommend(c, hm);
        return;
    }
#endif

    /* ================================================================
     * 默认 —— 404 未找到
     * ================================================================ */
    mg_http_reply(c, 404,
        "Content-Type: application/json; charset=utf-8\r\n"
        "Access-Control-Allow-Origin: *\r\n",
        "%s\n", json_error(404, "接口不存在"));
}
