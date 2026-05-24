#ifndef HEATMAP_H
#define HEATMAP_H

#include "mongoose.h"

/**
 * 品类销售热力排行
 * GET /api/heatmap/category?start_date=&end_date=
 *
 * 按品类汇总销售额、销量、商品数，计算占比与热力等级。
 * 热力等级：hot（>=25%）、warm（10%~25%）、cool（<10%）。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_heatmap_category(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 销售热力数据（按天/周/月分布，用于热力图渲染）
 * GET /api/heatmap/sales?dimension=daily&start_date=&end_date=&category=
 *
 * 按指定维度汇总每日/每周/每月的销售额与订单数，
 * 同时提供各品类细分数据与汇总统计。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_heatmap_sales(struct mg_connection *c, struct mg_http_message *hm);

#endif /* HEATMAP_H */
