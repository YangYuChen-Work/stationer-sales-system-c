#ifndef API_H
#define API_H

#include "mongoose.h"

/* ================================================================
 * 路由分发模块 - api.h
 *
 * 全局 HTTP 请求路由入口。根据 HTTP 方法与 URI 路径，
 * 将请求分发给对应的业务处理函数。
 * ================================================================ */

/**
 * 路由分发入口 —— 所有 HTTP 请求通过此函数分发
 *
 * 根据请求方法（GET/POST/PUT/DELETE/OPTIONS）与 URI 路径，
 * 匹配路由表并调用对应的业务处理器。未匹配的请求返回 404。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void api_route(struct mg_connection *c, struct mg_http_message *hm);

/**
 * CORS 预检请求处理
 *
 * 响应浏览器发起的 OPTIONS 预检请求，
 * 设置跨域允许头（来源、方法、请求头），返回 204 No Content。
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void api_handle_cors(struct mg_connection *c, struct mg_http_message *hm);

#endif /* API_H */
