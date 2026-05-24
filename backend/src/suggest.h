#ifndef SUGGEST_H
#define SUGGEST_H

#include "mongoose.h"

/**
 * 自动补货建议模块 - suggest.h
 *
 * 根据过去 30 天的销售数据，计算每件商品的建议采购量。
 * 算法：建议采购量 = 日均销量 x 补货周期(30天) x 安全系数(1.2) - 当前库存
 */

/**
 * 自动补货建议列表查询
 * GET /api/suggestions?category=XXX
 *
 * 对库存中的每件商品（可选按分类筛选），计算：
 *   - 过去 30 天日均销量
 *   - 建议采购量（日均销量 x 30 x 1.2 - 当前库存）
 *   - 预估采购成本（建议采购量 x 平均进货单价）
 *   - 补货紧急程度（高 / 中 / 低）
 *
 * @param c   Mongoose 连接对象，用于发送 HTTP 响应
 * @param hm  已解析的 HTTP 请求消息，包含查询参数 category
 */
void handle_suggest_list(struct mg_connection *c, struct mg_http_message *hm);

#endif /* SUGGEST_H */
