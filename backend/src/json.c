/**
 * @file    json.c
 * @brief   JSON 工具模块实现 —— 轻量级 JSON 字符串构造库
 *
 * 本模块为文具店销售管理系统所有后端模块提供统一的 JSON 响应构造能力。
 * 通过动态增长的字符串缓冲区（JsonBuf）和一组简洁的 API，避免手写
 * 字符串拼接带来的错误和内存管理问题。
 *
 * 依赖：仅标准 C 库 —— <stdio.h>, <stdlib.h>, <string.h>, <stdarg.h>
 * 线程安全：不同 JsonBuf 实例相互独立，无共享全局状态
 */

#include "json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ================================================================
 * 内部常量
 * ================================================================ */

/** JsonBuf 初始分配容量（字节） */
#define JSON_BUF_INIT_CAP  2048

/* ================================================================
 * 内部辅助函数 —— 字符串 JSON 转义
 * ================================================================ */

/**
 * 对源字符串进行 JSON 转义，返回 malloc 分配的新字符串
 * 调用者负责 free 返回值；若 str 为 NULL 则返回 NULL
 *
 * 转义字符映射：
 *   "  →  \"     \  →  \\
 *   \n →  \n     \r →  \r     \t →  \t
 * （输出中 \ 表示字面反斜杠字符，为 C 字符串转义写法）
 */
static char* escape_json_string(const char *str)
{
    if (str == NULL) return NULL;

    /* 第一遍扫描：统计需要额外分配的字节数 */
    int extra = 0;
    for (const char *p = str; *p != '\0'; p++) {
        switch (*p) {
            case '"':   /* "  →  \"   +1 字节 */
            case '\\':  /* \  →  \\   +1 字节 */
            case '\n':  /* 换行 → \n  +1 字节 */
            case '\r':  /* 回车 → \r  +1 字节 */
            case '\t':  /* 制表 → \t  +1 字节 */
                extra++;
                break;
            default:
                break;
        }
    }

    /* 分配转义后的字符串空间 */
    size_t orig_len = strlen(str);
    char  *escaped  = (char*)malloc(orig_len + extra + 1);
    if (escaped == NULL) return NULL;

    /* 第二遍扫描：逐字符复制并转义 */
    char *out = escaped;
    for (const char *p = str; *p != '\0'; p++) {
        switch (*p) {
            case '"':  *out++ = '\\'; *out++ = '"';  break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\n': *out++ = '\\'; *out++ = 'n';  break;
            case '\r': *out++ = '\\'; *out++ = 'r';  break;
            case '\t': *out++ = '\\'; *out++ = 't';  break;
            default:   *out++ = *p;                   break;
        }
    }
    *out = '\0';

    return escaped;
}

/* ================================================================
 * 缓冲区生命周期管理
 * ================================================================ */

void jb_init(JsonBuf *jb)
{
    if (jb == NULL) return;

    jb->data = (char*)malloc(JSON_BUF_INIT_CAP);
    if (jb->data == NULL) {
        /* 内存分配失败：设置为安全初始状态 */
        jb->len = 0;
        jb->cap = 0;
        return;
    }
    jb->len = 0;
    jb->cap = JSON_BUF_INIT_CAP;
    jb->data[0] = '\0';   /* 空字符串 */
}

void jb_free(JsonBuf *jb)
{
    if (jb == NULL) return;

    free(jb->data);
    jb->data = NULL;
    jb->len  = 0;
    jb->cap  = 0;
}

const char* jb_get(JsonBuf *jb)
{
    if (jb == NULL) return NULL;
    return jb->data;
}

/* ================================================================
 * 底层追加函数
 * ================================================================ */

void jb_append(JsonBuf *jb, const char *fmt, ...)
{
    if (jb == NULL || fmt == NULL) return;

    va_list args;
    int     needed;

    /* 第一步：用 vsnprintf(NULL, 0, ...) 计算所需字符数（不含结尾 '\0'） */
    va_start(args, fmt);
    needed = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (needed < 0) return;  /* 格式化错误 */

    /* 第二步：检查缓冲区容量是否足够（需要 needed 个字符 + 结尾 '\0'） */
    while (jb->len + needed + 1 > jb->cap) {
        /* 扩容策略：容量翻倍；若仍不够，则直接扩至所需大小 */
        int new_cap = jb->cap * 2;
        if (new_cap < jb->len + needed + 1) {
            new_cap = jb->len + needed + 1;
        }
        char *new_data = (char*)realloc(jb->data, new_cap);
        if (new_data == NULL) return;  /* 内存不足，放弃本次追加 */
        jb->data = new_data;
        jb->cap  = new_cap;
    }

    /* 第三步：执行实际的格式化写入 */
    va_start(args, fmt);
    vsnprintf(jb->data + jb->len, jb->cap - jb->len, fmt, args);
    va_end(args);

    jb->len += needed;  /* 更新长度 */
}

void jb_append_raw(JsonBuf *jb, const char *str)
{
    if (str == NULL) return;
    jb_append(jb, "%s", str);
}

/* ================================================================
 * JSON 值追加函数
 * ================================================================ */

void jb_append_string(JsonBuf *jb, const char *str)
{
    if (jb == NULL) return;

    /* NULL 字符串特殊处理：输出 JSON null 值 */
    if (str == NULL) {
        jb_append(jb, "null");
        return;
    }

    /* 对字符串内容进行 JSON 转义 */
    char *escaped = escape_json_string(str);
    if (escaped == NULL) {
        /* 转义失败（内存不足），回退输出 null */
        jb_append(jb, "null");
        return;
    }

    /* 输出双引号包裹的转义后字符串 */
    jb_append(jb, "\"%s\"", escaped);

    free(escaped);
}

/* ================================================================
 * JSON 键值对追加函数
 * ================================================================ */

void jb_append_int(JsonBuf *jb, const char *key, int val)
{
    if (jb == NULL || key == NULL) return;
    /* 格式: "key":val */
    jb_append(jb, "\"%s\":%d", key, val);
}

void jb_append_float(JsonBuf *jb, const char *key, double val)
{
    if (jb == NULL || key == NULL) return;
    /* 格式: "key":val，浮点数保留 2 位小数 */
    jb_append(jb, "\"%s\":%.2f", key, val);
}

void jb_append_kv(JsonBuf *jb, const char *key, const char *val)
{
    if (jb == NULL || key == NULL) return;

    /* 附加值字段 */
    if (val == NULL) {
        jb_append(jb, "\"%s\":null", key);
        return;
    }

    /* 先追加 key 部分，再借助 jb_append_string 完成转义 */
    char *escaped = escape_json_string(val);
    if (escaped == NULL) {
        jb_append(jb, "\"%s\":null", key);
        return;
    }

    jb_append(jb, "\"%s\":\"%s\"", key, escaped);
    free(escaped);
}

/* ================================================================
 * 高层响应构造函数
 * ================================================================ */

char* json_response(int code, const char *message, const char *data_json)
{
    JsonBuf jb;

    jb_init(&jb);

    /* 构造 JSON 对象: { */
    jb_append(&jb, "{");

    /* code 字段 */
    jb_append_int(&jb, "code", code);
    jb_append(&jb, ",");

    /* message 字段 */
    jb_append_kv(&jb, "message", message);
    jb_append(&jb, ",");

    /* data 字段 —— 直接插入调用方提供的 JSON 片段或 null */
    if (data_json != NULL && strlen(data_json) > 0) {
        jb_append(&jb, "\"data\":%s", data_json);
    } else {
        jb_append(&jb, "\"data\":null");
    }

    jb_append(&jb, "}");

    /* 返回缓冲区数据指针，JsonBuf 自身为栈变量，无需额外清理 */
    return jb.data;
}

char* json_error(int code, const char *message)
{
    /* 错误响应固定 data=null，直接委托 json_response 处理 */
    return json_response(code, message, NULL);
}
