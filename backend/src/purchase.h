#ifndef PURCHASE_H
#define PURCHASE_H

#include "mongoose.h"

/**
 * 进货管理模块 —— 进货记录列表、新增进货记录、进货流程（更新库存+生成记录）
 *
 * 路由映射:
 *   GET  /api/purchases          -> handle_purchase_list
 *   POST /api/purchases          -> handle_purchase_create
 *   POST /api/purchases/restock  -> handle_purchase_restock
 */

/** 进货记录列表（GET /api/purchases） */
void handle_purchase_list(struct mg_connection *c, struct mg_http_message *hm);

/** 新增进货记录（POST /api/purchases）—— 仅记录，不影响库存 */
void handle_purchase_create(struct mg_connection *c, struct mg_http_message *hm);

/** 进货流程（POST /api/purchases/restock）—— 增加库存 + 生成进货记录，事务保护 */
void handle_purchase_restock(struct mg_connection *c, struct mg_http_message *hm);

#endif /* PURCHASE_H */
