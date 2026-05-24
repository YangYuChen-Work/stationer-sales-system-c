#ifndef SALES_H
#define SALES_H

#include "mongoose.h"

/**
 * 销售记录列表（分页 + 多条件筛选）
 * GET /api/sales?page=&page_size=&keyword=&start_date=&end_date=&category=
 */
void handle_sales_list(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 销售记录按日期排序展示
 * GET /api/sales/sorted?order=desc&page=1&page_size=20
 */
void handle_sales_sorted(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 新增单条销售记录（仅记录，不扣减库存）
 * POST /api/sales
 * Body: {product_code, quantity, sale_price, sale_date?}
 */
void handle_sales_create(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 销售结账流程（扣减库存 + 生成多条销售记录，事务保护）
 * POST /api/sales/checkout
 * Body: {items: [{product_code, quantity, sale_price}, ...]}
 */
void handle_sales_checkout(struct mg_connection *c, struct mg_http_message *hm);

#endif /* SALES_H */
