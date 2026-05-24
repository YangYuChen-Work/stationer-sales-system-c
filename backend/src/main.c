// ============================================================
// main.c — 文具店销售管理系统后端入口
// ============================================================
// 初始化 Mongoose HTTP 服务器、测试数据库连接、进入事件循环
// 监听端口: 8080
// ============================================================

#include "mongoose.h"
#include "api.h"
#include "db.h"
#include <stdio.h>

// Mongoose 事件处理回调
// ev == MG_EV_HTTP_MSG 时处理 HTTP 请求
static void event_handler(struct mg_connection *c, int ev, void *ev_data) {
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;

        if (mg_match(hm->uri, mg_str("/api/#"), NULL)) {
            // API 请求 → 路由处理器
            api_route(c, hm);
        } else {
            // 非 API 请求 → 托管前端静态文件 (SPA 模式)
            struct mg_http_serve_opts opts = {
                .root_dir = "frontend/dist",
                .page404  = "frontend/dist/index.html"
            };
            mg_http_serve_dir(c, hm, &opts);
        }
    }
}

int main(void) {
    struct mg_mgr mgr;          // Mongoose 事件管理器
    mg_mgr_init(&mgr);          // 初始化

    printf("========================================\n");
    printf("  文具店销售管理系统 v1.0\n");
    printf("  Stationery Store Management System\n");
    printf("========================================\n\n");

    // 测试数据库连接
    printf("[Init] 正在连接 MySQL 数据库...\n");
    MYSQL *test_conn = db_get_connection();
    if (test_conn) {
        printf("[Init] ✓ 数据库连接成功 (localhost:3306/stationery_db)\n");
        db_release_connection(test_conn);
    } else {
        printf("[Init] ✗ 数据库连接失败！请检查:\n");
        printf("       1. MySQL 服务是否已启动\n");
        printf("       2. 数据库 stationery_db 是否已创建\n");
        printf("       3. 用户名/密码是否正确 (root/20060222)\n");
        printf("       4. 运行: mysql -u root -p20060222 < database/schema.sql\n");
    }

    // 启动 HTTP 服务
    printf("\n[Init] 启动 HTTP 服务器...\n");
    mg_http_listen(&mgr, "http://0.0.0.0:8080", event_handler, NULL);
    printf("[Init] ✓ 服务已启动: http://localhost:8080\n");
    printf("[Init] 前端页面: http://localhost:8080\n");
    printf("[Init] API 路径: http://localhost:8080/api\n");
    printf("[Init] 按 Ctrl+C 停止服务\n\n");
    printf("========================================\n\n");

    // 进入事件循环 (无限循环，处理请求)
    for (;;) {
        mg_mgr_poll(&mgr, 1000);  // 每秒轮询一次
    }

    // 清理 (实际上不会执行到这里，因为上面是无限循环)
    mg_mgr_free(&mgr);
    return 0;
}
