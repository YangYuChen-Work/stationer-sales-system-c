#ifndef ALERT_H
#define ALERT_H

#include "mongoose.h"

/* ================================================================
 * 库存智能预警模块 - alert.h
 *
 * 提供库存智能预警查询接口，自动筛选库存低于安全库存的商
 * 品，按严重等级分类（严重缺货 / 库存偏低），帮助管理员快
 * 速发现并处理库存风险。
 *
 * 路由映射:
 *   GET /api/alerts?level=all|danger|warning
 * ================================================================ */

/**
 * 库存预警列表查询
 * GET /api/alerts?level=all|danger|warning
 *
 * 查询所有 stock_quantity <= safety_stock 的商品，按严重等级分为：
 *   - danger:  库存 <= warning_stock   （严重缺货）
 *   - warning: 预警库存 < 库存 <= 安全库存 （库存偏低）
 *
 * 可选参数 level 用于按等级筛选，默认为 "all" 返回全部。
 *
 * @param c   Mongoose 连接对象，用于发送 HTTP 响应
 * @param hm  已解析的 HTTP 请求消息，包含查询参数
 */
void handle_alert_list(struct mg_connection *c, struct mg_http_message *hm);

#endif /* ALERT_H */
