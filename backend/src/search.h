#ifndef SEARCH_H
#define SEARCH_H

#include "mongoose.h"

/* ================================================================
 * 多条件组合查询模块 - search.h
 *
 * 提供跨 inventory / sales / purchases 三表的统一多条件查询接口。
 * 支持关键字模糊搜索、精确匹配、日期范围、价格区间、数量区间
 * 等多种筛选条件的动态组合，并返回分页结果。
 * ================================================================ */

/**
 * 多条件组合查询
 * GET /api/search?type=&keyword=&product_code=&category=&manufacturer=
 *     &start_date=&end_date=&min_price=&max_price=&min_quantity=
 *     &max_quantity=&page=1&page_size=20
 *
 * @param c   Mongoose 连接对象，用于发送 HTTP 响应
 * @param hm  已解析的 HTTP 请求消息，包含方法和查询参数
 */
void handle_search(struct mg_connection *c, struct mg_http_message *hm);

#endif /* SEARCH_H */
