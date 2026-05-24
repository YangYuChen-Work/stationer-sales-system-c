/**
 * @file    stats.h
 * @brief   统计分析模块 —— 统计概览与橡皮类专项统计接口声明
 *
 * 提供两个 HTTP 处理函数:
 *   1. 综合统计概览（库存、销售、进货汇总 + 销售排行 + 库存预警）
 *   2. 橡皮类产品近 7 天销售专项统计
 *
 * 依赖：mongoose.h
 */

#ifndef STATS_H
#define STATS_H

#include "mongoose.h"

/**
 * 综合统计概览
 * GET /api/stats/overview
 *
 * 返回库存汇总、销售汇总、进货汇总、销售排行 TOP10、库存预警五个维度的统计数据
 */
void handle_stats_overview(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 橡皮类产品近 7 天销售统计
 * GET /api/stats/eraser
 *
 * 返回橡皮类产品的分类汇总（周期内总销售额、总销量）以及每个产品的
 * 当前库存、周期内销售量和销售额明细
 */
void handle_stats_eraser(struct mg_connection *c, struct mg_http_message *hm);

#endif /* STATS_H */
