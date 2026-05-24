/**
 * @file    json.h
 * @brief   JSON 工具模块 —— 轻量级 JSON 字符串构造库（零外部依赖）
 *
 * 本模块提供动态增长的字符串缓冲区 JsonBuf 以及一系列格式化追加函数，
 * 用于构造符合 JSON 规范的 API 响应字符串。所有函数均为可重入设计，
 * 不同 JsonBuf 实例可在多线程环境下独立使用而无竞态问题。
 *
 * 依赖：仅标准 C 库（stdio, stdlib, string, stdarg）
 */

#ifndef JSON_H
#define JSON_H

/* ================================================================
 * 数据结构
 * ================================================================ */

/**
 * JsonBuf —— JSON 字符串缓冲区（动态增长）
 *
 * 内部维护一个使用 malloc/realloc 分配的字符数组，容量不足时自动扩容。
 * 使用者无需关心底层内存管理细节。
 */
typedef struct {
    char *data;   /**< 缓冲区数据指针（以 '\0' 结尾的 C 字符串） */
    int   len;    /**< 当前有效字符串长度（不含结尾 '\0'） */
    int   cap;    /**< 缓冲区总容量（字节数） */
} JsonBuf;

/* ================================================================
 * 缓冲区生命周期管理
 * ================================================================ */

/**
 * 初始化 JsonBuf 缓冲区
 * 分配初始容量（2048 字节），置 len=0，首字符写入 '\0'
 */
void jb_init(JsonBuf *jb);

/**
 * 释放 JsonBuf 缓冲区占用的堆内存
 * 释放后将 data 置为 NULL，len 和 cap 置为 0
 */
void jb_free(JsonBuf *jb);

/**
 * 返回缓冲区当前内容的 C 字符串指针
 * 调用者不应修改或释放该指针所指向的内存
 */
const char* jb_get(JsonBuf *jb);

/* ================================================================
 * 底层追加函数
 * ================================================================ */

/**
 * 以 printf 风格向缓冲区追加格式化字符串
 * 缓冲区容量不足时自动扩容（策略：翻倍，若仍不足则按需分配）
 */
void jb_append(JsonBuf *jb, const char *fmt, ...);

/**
 * 追加原始字符串（不转义、不加引号）
 * 等价于 jb_append(jb, "%s", str)
 */
void jb_append_raw(JsonBuf *jb, const char *str);

/* ================================================================
 * JSON 值追加函数
 * ================================================================ */

/**
 * 追加 JSON 字符串值 —— 自动添加双引号并转义特殊字符
 *
 * 转义规则：
 *   "  →  \"      \  →  \\
 *   \n →  \\n     \r →  \\r     \t →  \\t
 *
 * 若 str 为 NULL，则输出 JSON null 值（不加引号）
 */
void jb_append_string(JsonBuf *jb, const char *str);

/* ================================================================
 * JSON 键值对追加函数
 * ================================================================ */

/**
 * 追加整数值键值对 —— 格式: "key":val
 */
void jb_append_int(JsonBuf *jb, const char *key, int val);

/**
 * 追加浮点值键值对 —— 格式: "key":val（val 保留 2 位小数）
 */
void jb_append_float(JsonBuf *jb, const char *key, double val);

/**
 * 追加字符串键值对 —— 格式: "key":"escaped_val"（val 自动转义）
 * 若 val 为 NULL，则输出 "key":null
 */
void jb_append_kv(JsonBuf *jb, const char *key, const char *val);

/* ================================================================
 * 高层响应构造函数
 * ================================================================ */

/**
 * 构造标准成功响应 JSON 字符串
 *
 * 格式: {"code":N,"message":"...","data":...}
 *
 * @param code       业务状态码
 * @param message    提示信息（会自动转义）
 * @param data_json  数据字段，应为合法的 JSON 片段；若为 NULL 则输出 null
 * @return           malloc 分配的 JSON 字符串，调用者负责 free
 */
char* json_response(int code, const char *message, const char *data_json);

/**
 * 构造简单错误响应 JSON 字符串
 *
 * 格式: {"code":N,"message":"...","data":null}
 *
 * @param code       错误状态码
 * @param message    错误提示信息（会自动转义）
 * @return           malloc 分配的 JSON 字符串，调用者负责 free
 */
char* json_error(int code, const char *message);

#endif /* JSON_H */
