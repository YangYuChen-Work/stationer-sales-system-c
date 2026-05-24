#ifndef INVENTORY_H
#define INVENTORY_H

#include "mongoose.h"

/* ================================================================
 * 库存管理模块 - inventory.h
 *
 * 提供文具商品库存的增删改查 HTTP 请求处理函数。
 * 所有函数签名遵循 Mongoose 事件处理器约定，
 * 返回 JSON 格式的响应体并附带 CORS 跨域头。
 * ================================================================ */

/**
 * 库存列表查询（分页 + 多条件筛选）
 * GET /api/inventory?page=N&page_size=N&keyword=XXX&category=XXX
 *
 * @param c   Mongoose 连接对象，用于发送 HTTP 响应
 * @param hm  已解析的 HTTP 请求消息，包含方法、URI、查询参数和请求体
 */
void handle_inventory_list(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 库存单品查询
 * GET /api/inventory/{product_code}
 *
 * 从 URI 路径末尾提取商品编号，查询并返回单条库存记录。
 * 若商品编号不存在则返回 404 错误。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_inventory_get(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 新增库存商品
 * POST /api/inventory
 * Body: {"product_code":"P001","product_name":"铅笔","category":"笔类",...}
 *
 * 须提供 product_code 和 product_name 两个必填字段。
 * 自动检查商品编号唯一性，重复则返回 409。
 * 成功时返回 201 及新创建的完整记录。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_inventory_create(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 更新库存商品
 * PUT /api/inventory/{product_code}
 * Body: {"product_name":"新名称","category":"新分类",...}
 *
 * URI 路径指定目标商品编号，请求体中的所有字段均为可选，
 * 仅更新提供的字段。若商品不存在则返回 404。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_inventory_update(struct mg_connection *c, struct mg_http_message *hm);

/**
 * 删除库存商品
 * DELETE /api/inventory/{product_code}
 *
 * 根据 URI 路径中的商品编号删除对应库存记录。
 * 删除前先校验商品是否存在，不存在则返回 404。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_inventory_delete(struct mg_connection *c, struct mg_http_message *hm);

#endif /* INVENTORY_H */
