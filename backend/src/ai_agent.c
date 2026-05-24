/**
 * @file    ai_agent.c
 * @brief   AI 大模型接入模块 —— 预留接口实现（当前返回 501）
 *
 * 本模块为文具店销售管理系统的 AI 智能代理模块提供预留接口实现。
 * 当前所有三个接口均返回 HTTP 501 (Not Implemented)，
 * 表示接口已定义但 AI 服务尚未配置。
 *
 * 依赖：
 *   - ai_agent.h  — 接口声明
 *   - json.h      — JSON 响应构造（json_error 函数）
 *   - mongoose.h  — HTTP 服务器框架
 *
 * 外部依赖（启用 AI 功能后）：
 *   - libcurl      — 用于向 AI API 发送 HTTP 请求
 *   - 第三方 JSON 解析库（如 cJSON）— 用于解析 AI 返回的复杂 JSON 响应
 *
 * 启用 AI 功能步骤：
 *   1. 安装 libcurl 开发包（Ubuntu: apt install libcurl4-openssl-dev）
 *   2. 设置环境变量 AI_API_URL 和 AI_API_KEY
 *   3. 取消 ai_agent.h 中 #define AI_ENABLED 的注释
 *   4. 实现本文件中各接口函数的提示词构造和 HTTP 调用逻辑
 *   5. 在 api.c 中取消 #include "ai_agent.h" 和对应路由块的注释
 *   6. 在 Makefile 中添加 -lcurl 链接选项
 *
 * AI API 预期格式（OpenAI Chat Completions 兼容）:
 *
 *   请求:
 *   POST {AI_API_URL}
 *   Authorization: Bearer {AI_API_KEY}
 *   Content-Type: application/json
 *   {
 *     "model": "gpt-3.5-turbo",
 *     "messages": [
 *       {"role": "system", "content": "你是文具店销售管理助手..."},
 *       {"role": "user",   "content": "用户问题..."}
 *     ],
 *     "temperature": 0.7
 *   }
 *
 *   响应:
 *   {
 *     "choices": [{
 *       "message": {
 *         "role": "assistant",
 *         "content": "模型回答内容"
 *       }
 *     }]
 *   }
 */

#include "ai_agent.h"
#include "json.h"
#include <stdlib.h>   /* free */

/* ================================================================
 * 内部常量
 * ================================================================ */

/**
 * CORS 跨域响应头
 * 允许前端从任意来源发起 AJAX 请求，支持常见的 HTTP 方法。
 */
#define CORS_HEADERS \
    "Access-Control-Allow-Origin: *\r\n"                     \
    "Access-Control-Allow-Methods: GET,POST,PUT,DELETE,OPTIONS\r\n" \
    "Access-Control-Allow-Headers: Content-Type\r\n"

/* ================================================================
 * 内部辅助函数
 * ================================================================ */

/**
 * 发送 501 Not Implemented 响应
 *
 * 统一向客户端返回"AI 服务未配置"的错误信息，
 * 附带 CORS 头和标准 JSON 错误格式。
 *
 * @param c   Mongoose 连接对象
 */
static void send_not_implemented(struct mg_connection *c)
{
    char *resp = json_error(501, "AI 服务未配置，请配置后重试");
    mg_http_reply(c, 501,
                  "Content-Type: application/json; charset=utf-8\r\n"
                  CORS_HEADERS,
                  "%s\n", resp ? resp : "{}");
    free(resp);
}

/* ================================================================
 * 公开接口实现
 * ================================================================ */

/**
 * POST /api/ai/ask —— AI 自然语言查询
 *
 * 启用后将实现以下流程：
 *   1. 从请求体中解析用户问题（JSON 字段 "question"）
 *   2. 构造系统提示词，包含数据库表结构（商品表、销售表、进货表）
 *      和业务规则说明，引导 AI 理解查询意图
 *   3. 调用 ai_send_request() 将提示词发送至 AI 服务
 *   4. 解析 AI 返回的 JSON 响应，提取回答文本
 *   5. 将回答包装为标准 API 响应格式返回前端
 *
 * 当前状态：返回 501 提示用户配置 AI 服务
 */
void handle_ai_ask(struct mg_connection *c, struct mg_http_message *hm)
{
    (void)hm;  /* 当前未使用请求体，消除编译警告 */
    send_not_implemented(c);
}

/**
 * POST /api/ai/forecast —— AI 销售预测
 *
 * 启用后将实现以下流程：
 *   1. 从查询参数中提取预测周期（period）和品类过滤条件（category）
 *   2. 查询数据库获取对应时间段的历史销售统计数据（按日期/品类汇总）
 *   3. 构造分析提示词，将销售数据以表格形式嵌入，
 *      要求 AI 分析趋势并给出未来销量预测
 *   4. 调用 ai_send_request() 获取预测结果
 *   5. 解析 AI 返回的结构化数据（预测值列表、置信度、趋势描述等）
 *   6. 将预测结果包装为标准 API 响应格式返回前端
 *
 * 当前状态：返回 501 提示用户配置 AI 服务
 */
void handle_ai_forecast(struct mg_connection *c, struct mg_http_message *hm)
{
    (void)hm;  /* 当前未使用查询参数，消除编译警告 */
    send_not_implemented(c);
}

/**
 * POST /api/ai/recommend —— AI 智能选品建议
 *
 * 启用后将实现以下流程：
 *   1. 从查询参数中提取品类过滤条件（category）
 *   2. 查询数据库获取：
 *      - 各商品当前库存量及安全库存阈值
 *      - 近 30 天各商品销售量和销售额
 *      - 近 30 天各商品进货量和进货成本
 *   3. 构造分析提示词，将库存和销售数据嵌入，
 *      要求 AI 综合评估并给出进货优先级排序和数量建议
 *   4. 调用 ai_send_request() 获取建议结果
 *   5. 解析 AI 返回的结构化建议数据（商品列表、建议进货量、理由等）
 *   6. 将建议结果包装为标准 API 响应格式返回前端
 *
 * 当前状态：返回 501 提示用户配置 AI 服务
 */
void handle_ai_recommend(struct mg_connection *c, struct mg_http_message *hm)
{
    (void)hm;  /* 当前未使用查询参数，消除编译警告 */
    send_not_implemented(c);
}
