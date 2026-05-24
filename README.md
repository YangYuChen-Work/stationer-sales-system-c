# 文具店销售管理系统 — 项目介绍与技术选型说明

> **版本**: v1.0 | **日期**: 2026-05-23

---

## 目录

- [一、项目背景与目标](#一项目背景与目标)
- [二、系统功能全景](#二系统功能全景)
- [三、技术选型与决策理由](#三技术选型与决策理由)
- [四、系统架构详解](#四系统架构详解)
- [五、AI 大模型接入预留设计](#五ai-大模型接入预留设计)

---

## 一、项目背景与目标

### 1.1 项目定位

文具店销售管理系统是一套面向中小型文具零售门店的数字化管理工具，覆盖门店日常运营中的**库存管理、销售收银、进货补货、统计分析**四大核心场景。

### 1.2 要解决的核心问题

| 痛点 | 解决方案 |
|------|----------|
| 库存账目混乱，不清楚哪些商品缺货 | 实时库存管理 + 三级智能预警 |
| 进货凭经验，缺乏数据支撑 | 基于销售速率的自动补货建议 |
| 不清楚哪些品类赚钱、哪些滞销 | 品类热力分析 + 销售排行看板 |
| 日常记手工账效率低、易出错 | 销售/进货业务闭环，自动联动库存 |

### 1.3 设计原则

- **业务闭环**: 销售自动扣库存，进货自动增库存，杜绝数据不一致
- **智能辅助**: 不只记录数据，更进一步提供预警、建议等决策支持
- **渐进演进**: 架构预留 AI 大模型接口，后续可接入智能问答、需求预测等能力
- **工程化标准**: 见名知意的目录结构、模块化设计、完善的代码注释

---

## 二、系统功能全景

### 2.1 功能模块图

```
文具店销售管理系统
├── 1. 首页分析看板
│   ├── 今日经营概览（销售额/订单数/毛利）
│   ├── 库存预警列表（三级：危险/警告/安全）
│   └── 品类销售热力图
│
├── 2. 库存管理
│   ├── 商品信息 CRUD（名称/编号/类别/厂家/型号/库存/单价）
│   ├── 按商品编号精准查询
│   └── 分页列表 + 关键词搜索
│
├── 3. 销售管理
│   ├── 销售记录查询（支持日期排序）
│   ├── 收银流程（自动扣减库存 + 生成交易记录）
│   └── 多条件筛选（日期/类别/价格区间）
│
├── 4. 进货管理
│   ├── 进货记录查询
│   ├── 进货流程（自动增加库存 + 生成进货记录）
│   └── 成本统计
│
├── 5. 高级搜索
│   └── 多维度组合查询（类型/日期/类别/厂家/价格/数量）
│
├── 6. 智能分析（核心亮点）
│   ├── 库存智能预警（三级阈值 + 实时标记）
│   ├── 自动补货建议（基于30天销售速率 × 安全系数）
│   └── 销售热力看板（品类热度 + 时段分布 + TOP10 排行）
│
└── 7. AI 扩展能力（预留）
    ├── 智能问答（自然语言查询库存/销售数据）
    ├── 销售预测（基于历史数据的趋势预测）
    └── 智能选品建议（基于热力分析的进货推荐）
```

---

## 三、技术选型与决策理由

### 3.1 总览

| 层 | 选型 | 理由 |
|---|------|------|
| 前端框架 | **Vue 3** (Composition API) | 学习曲线平缓、生态成熟、Element Plus 完美配套 |
| 构建工具 | **Vite** | 秒级冷启动、HMR 热更新、开箱即用 |
| UI 组件库 | **Element Plus** | 企业级中后台首选，表格/表单/弹窗/分页组件完备 |
| 图表库 | **ECharts 5.x** | 国产顶级图表库，热力图/柱状图/饼图原生支持 |
| 后端语言 | **C 语言 (C11)** | 课程要求，同时也是理解底层原理的最佳语言 |
| HTTP 服务 | **Mongoose 7.x** | 单文件嵌入式 Web 服务器，MIT 协议，跨平台 |
| 数据库 | **MySQL 8.0** | 关系型数据库标杆，事务支持完善，适合进销存场景 |
| 数据库驱动 | **libmysqlclient** | MySQL 官方 C API 库 |

### 3.2 为什么前端选择 Vue 3 + Element Plus

**核心考量 — 与 C 后端的兼容性：**
前后端完全分离，通过 HTTP JSON API 通信。Vue 3 的 Vite 开发服务器只需配置 `proxy` 将 `/api` 请求转发到 C 后端即可，架构清爽无耦合。

**对比 React / Angular：**

| 维度 | Vue 3 | React | Angular |
|------|-------|-------|---------|
| 学习曲线 | ★★☆（平缓） | ★★★（中等） | ★★★★（陡峭） |
| 中后台配套 | Element Plus（官方） | Ant Design（社区） | Material（社区） |
| <script setup> 语法 | 极致简洁 | JSX（灵活但冗长） | 模板+类装饰器 |
| 构建速度 | Vite 秒级 | CRA/Next.js 较慢 | ng serve 较慢 |
| 适合本项目的理由 | **最佳匹配** — 快速出页面、组件丰富 | 过度灵活，本项目无复杂状态管理需求 | 过重，学习成本不匹配工期 |

**抉择**: Vue 3 — Element Plus 的 Table/Form/Dialog/DatePicker 组件覆盖本项目全部 UI 需求，无需额外造轮子。

### 3.3 为什么 C 语言 + Mongoose 做后端

**核心挑战**: C 语言原生不支持 HTTP 协议解析，如果从零写 HTTP 服务器：

```
手写 Winsock → 解析 HTTP Method/URI/Headers/Body → 路由分发 → 拼接响应
                    ↑ 至少 500+ 行且易出安全漏洞
```

**Mongoose 的价值:**

| Mongoose 做的事 | 我们不用做的事 |
|----------------|---------------|
| TCP 连接管理、epoll/select 多路复用 | 手写网络 I/O |
| HTTP 请求解析（Method/URI/Headers/Body） | 手写状态机解析 HTTP 协议 |
| 路由匹配 + 静态文件服务 | 手写 URL 路由 + MIME 类型映射 |
| 自动处理 CORS、Keep-Alive | 手写跨域头、连接管理 |
| 通过函数指针注册路由 → 调用我们的业务函数 | 只需关注业务逻辑 |

**效果**: 90% 的代码聚焦在「数据库操作 + 业务逻辑」上，10% 是路由注册。

### 3.4 为什么 MySQL + 事务

文具店进销存场景天然需要**事务一致性**：

```
销售流程 = 扣减库存 + 写入销售记录 （要么都成功，要么都回滚）
进货流程 = 增加库存 + 写入进货记录 （同理）
```

MySQL InnoDB 的行级锁 + ACID 事务保证数据绝不会出现「库存扣了但没销售记录」或「进货记录写了但库存没加」的半成品状态。

### 3.5 为什么预留 AI 大模型接口

文具零售场景有三个典型的 AI 应用方向：

| 应用场景 | 说明 | 技术路径 |
|----------|------|----------|
| **自然语言查数据** | "上周笔类卖了多少钱？" → 自动生成 SQL 查询 | NL2SQL + LLM |
| **销售预测** | 基于历史数据预测下周销量，指导进货 | 时序预测模型 / LLM 推理 |
| **智能选品建议** | "开学季该多进什么？" → 结合季节+热度推荐 | RAG + LLM |

架构层面已经为此做好了准备（见下文第五节）。

---

## 四、系统架构详解

### 4.1 物理架构

```
┌──────────────────────────────────────────────────┐
│ 用户浏览器 (Chrome / Edge / Firefox)              │
│ http://localhost:5173 (Vite Dev Server)           │
│ Vue 3 SPA — Element Plus — ECharts               │
└──────────────┬───────────────────────────────────┘
               │ HTTP REST (JSON)
               │ /api/* 代理到 :8080
┌──────────────▼───────────────────────────────────┐
│ C HTTP Server (Mongoose) — localhost:8080         │
│                                                   │
│ ┌─────────────────────────────────────────────┐  │
│ │              路由分发层 (api.c)              │  │
│ ├──────────┬──────────┬──────────┬────────────┤  │
│ │inventory │  sales   │purchase  │ stats      │  │
│ │  .c/.h   │  .c/.h   │  .c/.h   │ .c/.h      │  │
│ ├──────────┼──────────┼──────────┼────────────┤  │
│ │ alert    │ suggest  │ heatmap  │ search     │  │
│ │  .c/.h   │  .c/.h   │  .c/.h   │ .c/.h      │  │
│ ├──────────┼──────────┼──────────┼────────────┤  │
│ │              AI 模块 (预留)                  │  │
│ │          ai_agent.c/.h                       │  │
│ └──────────┴──────────┴──────────┴────────────┘  │
│                                                   │
│ ┌─────────────────────────────────────────────┐  │
│ │          数据库连接池 (db.c/.h)              │  │
│ └─────────────────────────────────────────────┘  │
└──────────────┬───────────────────────────────────┘
               │ TCP :3306
┌──────────────▼───────────────────────────────────┐
│ MySQL 8.0 — 文具店数据库 stationery_db            │
│ ┌──────────┐ ┌──────────┐ ┌──────────────────┐  │
│ │inventory │ │  sales   │ │   purchases      │  │
│ └──────────┘ └──────────┘ └──────────────────┘  │
└──────────────────────────────────────────────────┘

                    ┌─────────────────────┐
  未来接入:          │   AI 大模型服务      │
                    │ (OpenAI / 本地模型)  │
                    └─────────────────────┘
```

### 4.2 数据流 — 以「销售收银流程」为例

```
用户点击「结算」
    │
    ▼
CheckoutDialog.vue
    │ 构造 POST /api/sales/checkout
    │ Body: { items: [{product_code, quantity, sale_price}] }
    ▼
C 后端: api.c → sales.c → sales_checkout()
    │
    ├── 1. db_begin_transaction()         // 开启事务
    ├── 2. 遍历 items:
    │   ├── db_check_stock(code)          // 检查库存是否充足
    │   │   └── 不足 → db_rollback() → 返回 409
    │   ├── db_deduct_stock(code, qty)    // UPDATE inventory SET stock = stock - qty
    │   └── db_insert_sale(...)           // INSERT INTO sales
    ├── 3. db_commit()                     // 提交事务
    └── 4. 返回 JSON 响应
    │
    ▼
Vue 前端: 解析响应 → 显示小票 → 刷新列表
```

---

## 五、AI 大模型接入预留设计

### 5.1 设计理念

**面向接口编程** — AI 模块与业务模块之间通过明确的接口通信，互不耦合。

当 AI 模块未接入时，所有接口返回 `{"code": 501, "message": "AI 服务未配置"}`，不影响现有业务运行。

当 AI 模块接入后（如对接 OpenAI API 或本地大模型），只需实现具体逻辑即可无缝工作。

### 5.2 预留 API 接口清单

#### 5.2.1 智能问答 — NL2SQL 自然语言查询

```
POST /api/ai/ask
```

**请求体:**

```json
{
  "question": "上周笔类商品销售额是多少？"
}
```

**预留响应（AI 未接入时）:**

```json
{
  "code": 501,
  "message": "AI 服务未配置，请配置后重试",
  "data": null
}
```

**AI 接入后预期响应:**

```json
{
  "code": 200,
  "message": "查询完成",
  "data": {
    "question": "上周笔类商品销售额是多少？",
    "sql_generated": "SELECT SUM(total_amount) FROM sales WHERE category='笔类' AND sale_date BETWEEN '2026-05-12' AND '2026-05-18'",
    "answer": "上周笔类商品销售总额为 ¥3,280.00，共售出 420 件。",
    "data": {
      "total_amount": 3280.00,
      "total_quantity": 420,
      "period": { "start": "2026-05-12", "end": "2026-05-18" }
    }
  }
}
```

#### 5.2.2 销售预测

```
GET /api/ai/forecast
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| product_code | string | 否 | 商品编号，不传则返回全部 |
| days | int | 否 | 预测未来天数，默认 7 |
| method | string | 否 | 预测算法: `moving_avg` / `llm` |

**AI 接入后预期响应:**

```json
{
  "code": 200,
  "message": "预测完成",
  "data": {
    "method": "llm",
    "forecast_days": 7,
    "predictions": [
      {
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "forecast": [6, 8, 5, 7, 9, 6, 4],
        "weekly_total": 45,
        "confidence": 0.85
      }
    ]
  }
}
```

#### 5.2.3 智能选品建议

```
GET /api/ai/recommend
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| scenario | string | 否 | 场景: `back_to_school` / `exam_season` / `daily` |
| limit | int | 否 | 推荐数量，默认 10 |

**AI 接入后预期响应:**

```json
{
  "code": 200,
  "message": "推荐完成",
  "data": {
    "scenario": "back_to_school",
    "recommendations": [
      {
        "rank": 1,
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "reason": "开学季笔类需求增长 200%，当前库存偏低，建议将库存从 120 提升至 300",
        "suggested_order": 180
      }
    ]
  }
}
```

### 5.3 后端 C 代码预留结构

```c
// backend/src/ai_agent.h
#ifndef AI_AGENT_H
#define AI_AGENT_H

// ============================================================
// AI 大模型接入模块 — 预留接口
// ============================================================
// 当前状态: 接口已定义，实现为空（返回 501）
// 接入方式:
//   1. 设置环境变量 AI_API_KEY 和 AI_API_URL
//   2. 实现 ai_send_request() 中的 HTTP 调用逻辑
//   3. 实现各接口函数中的 JSON 解析逻辑
// ============================================================

// 启用/禁用 AI 功能的编译开关
// #define AI_ENABLED   // 取消此行注释以启用 AI 功能

// 处理 POST /api/ai/ask  — 自然语言查询
void handle_ai_ask(struct mg_connection *c, struct mg_http_message *hm);

// 处理 GET  /api/ai/forecast  — 销售预测
void handle_ai_forecast(struct mg_connection *c, struct mg_http_message *hm);

// 处理 GET  /api/ai/recommend — 智能选品建议
void handle_ai_recommend(struct mg_connection *c, struct mg_http_message *hm);

// ============================================================
// 内部辅助函数（AI 启用后实现）
// ============================================================

// 向 AI 大模型服务发送 HTTP 请求
// 参数: prompt — 构造好的提示词
// 返回: AI 返回的 JSON 字符串 (调用者负责释放内存)
char* ai_send_request(const char *prompt);

// 解析 AI 返回的 JSON 并提取回答文本
char* ai_extract_answer(const char *response_json);

#endif // AI_AGENT_H
```

### 5.4 前端预留入口

```javascript
// frontend/src/api/ai.js (预留文件)
// AI 功能 API 封装 — 后续接入大模型时启用

import { request } from './index'

// 自然语言查询
export function askQuestion(question) {
  return request({
    url: '/api/ai/ask',
    method: 'POST',
    data: { question }
  })
}

// 销售预测
export function getForecast(params) {
  return request({
    url: '/api/ai/forecast',
    method: 'GET',
    params
  })
}

// 智能选品推荐
export function getRecommend(params) {
  return request({
    url: '/api/ai/recommend',
    method: 'GET',
    params
  })
}
```

### 5.5 接入 AI 的典型流程

```
1. 部署大模型服务（如 Ollama 本地部署 / OpenAI API / 其他）
2. 取消 ai_agent.h 中 AI_ENABLED 的注释，重新编译
3. 设置环境变量:
   export AI_API_URL="https://api.openai.com/v1/chat/completions"
   export AI_API_KEY="sk-xxxxxxxx"
   export AI_MODEL="gpt-4o"
4. 在 ai_agent.c 中实现 ai_send_request():
   - 使用 libcurl 或 mg_http 发送 POST 请求到 AI_API_URL
   - 携带 prompt + 数据库查询结果作为上下文
   - 解析返回的 JSON，提取 AI 回答
5. 重启服务 → AI 功能生效
```

### 5.6 数据库查询接口（供 AI 模块调用）

为了方便 AI 模块获取数据上下文，预留了内部查询接口：

```c
// backend/src/db.h — 供 AI 模块使用的内部查询接口

// 获取指定时间段的销售汇总（供 AI 预测/推荐使用）
int db_query_sales_summary(
    const char *product_code,   // NULL = 全部
    const char *start_date,
    const char *end_date,
    char **result_json           // 输出: JSON 格式结果
);

// 获取库存全量数据（供 AI 选品推荐使用）
int db_query_inventory_full(char **result_json);

// 执行自然语言生成的 SQL（供 AI NL2SQL 使用）
// 注意: 必须做 SQL 注入防护和权限校验
int db_exec_ai_sql(const char *sql, char **result_json);
```

---

> **文档制作者**: AI 软件开发助手 | **最后更新**: 2026-05-23
