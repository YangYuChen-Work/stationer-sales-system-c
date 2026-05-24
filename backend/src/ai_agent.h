/**
 * @file    ai_agent.h
 * @brief   AI 大模型接入模块 —— 预留接口声明
 *
 * 本模块为文具店销售管理系统提供 AI 智能代理接口，支持：
 *   1. 自然语言查询 —— 用户可用日常语言查询销售、库存等数据
 *   2. 销售预测     —— 基于历史数据预测未来销量趋势
 *   3. 智能选品建议 —— 根据销售数据推荐进货策略
 *
 * 当前状态：接口已定义，所有实现返回 501 Not Implemented。
 * 待 AI 服务配置完成后，开发者需完成以下步骤以启用该模块：
 *   1. 设置环境变量 AI_API_KEY 和 AI_API_URL
 *   2. 实现 ai_send_request() 中的 HTTP 调用逻辑（建议使用 libcurl）
 *   3. 实现各接口函数中的提示词构造和 JSON 响应解析逻辑
 *   4. 取消本文件中 AI_ENABLED 宏的注释
 *   5. 在 api.c 中取消 #include "ai_agent.h" 和对应路由块的注释
 *
 * 依赖：mongoose.h（HTTP 请求处理框架）
 * 外部依赖（启用后）：libcurl（HTTP 客户端库，用于调用 AI API）
 */

#ifndef AI_AGENT_H
#define AI_AGENT_H

#include "mongoose.h"

/* ================================================================
 * AI 功能编译开关
 *
 * 取消下一行的注释即可启用 AI 集成功能。
 * 注意：启用前请确保已完成上述 5 个准备步骤，否则代码将无法通过编译。
 * ================================================================ */
// #define AI_ENABLED

/* ================================================================
 * 公开 HTTP 处理函数
 *
 * 所有函数签名遵循 Mongoose 事件处理器约定，
 * 由 api.c 中的路由分发逻辑根据 HTTP 方法和 URI 路径调用。
 * ================================================================ */

/**
 * AI 自然语言查询接口
 * POST /api/ai/ask
 *
 * 接收用户以自然语言描述的查询问题（如"上月铅笔销量是多少"），
 * 由后端构造包含数据库表结构和业务上下文的提示词，
 * 发送至 AI 大模型服务（OpenAI 兼容 API），
 * 返回模型生成的回答文本。
 *
 * 请求体格式: {"question": "用户输入的自然语言问题"}
 * 响应格式:   {"code": 200, "message": "ok", "data": {"answer": "AI 回答"}}
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_ai_ask(struct mg_connection *c, struct mg_http_message *hm);

/**
 * AI 销售预测接口
 * GET /api/ai/forecast
 *
 * 基于历史销售数据，请求 AI 模型对未来一段时间的销量趋势进行预测。
 * 后端从数据库提取近期销售统计数据，构造分析提示词发送至 AI 服务，
 * 返回包含预测值和趋势分析的 JSON 数据。
 *
 * 查询参数:  period（预测周期，如 "7d"/"30d"）、category（品类过滤，可选）
 * 响应格式:  {"code": 200, "message": "ok", "data": {"forecast": [...]}}
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_ai_forecast(struct mg_connection *c, struct mg_http_message *hm);

/**
 * AI 智能选品建议接口
 * GET /api/ai/recommend
 *
 * 根据当前库存水位、历史销售数据和市场趋势，
 * 请求 AI 模型给出进货优先级和数量建议，
 * 帮助店主优化采购决策、避免库存积压或断货。
 *
 * 查询参数:  category（品类过滤，可选）
 * 响应格式:  {"code": 200, "message": "ok", "data": {"recommendations": [...]}}
 *
 * @param c   Mongoose 连接对象
 * @param hm  已解析的 HTTP 请求消息
 */
void handle_ai_recommend(struct mg_connection *c, struct mg_http_message *hm);

/* ================================================================
 * 内部辅助函数（AI 功能启用后实现）
 *
 * 以下函数在当前版本中仅作声明，不提供实现。
 * 开发者启用 AI 功能时，需自行实现这些函数。
 * ================================================================ */

/**
 * 向 AI 大模型服务发送 HTTP 请求
 *
 * 使用 libcurl 向配置的 AI API 端点发送 POST 请求，
 * 请求体遵循 OpenAI Chat Completions API 格式。
 *
 * 环境变量依赖:
 *   AI_API_URL — AI 服务地址（如 https://api.openai.com/v1/chat/completions）
 *   AI_API_KEY — API 密钥（用于 Bearer Token 认证）
 *
 * @param prompt     已构造完成的提示词（系统提示词 + 用户问题）
 * @return           AI 返回的原始 JSON 响应字符串（malloc 分配，调用者负责 free）
 *                   失败时返回 NULL
 */
// char* ai_send_request(const char *prompt);

/**
 * 从 AI 服务返回的 JSON 响应中提取回答文本
 *
 * 解析 OpenAI Chat Completions API 的标准响应格式:
 *   {"choices": [{"message": {"content": "回答内容"}}]}
 * 提取 choices[0].message.content 字段的值。
 *
 * @param response_json  ai_send_request() 返回的原始 JSON 字符串
 * @return               提取出的回答文本（malloc 分配，调用者负责 free）
 *                       解析失败时返回 NULL
 */
// char* ai_extract_answer(const char *response_json);

#endif /* AI_AGENT_H */
