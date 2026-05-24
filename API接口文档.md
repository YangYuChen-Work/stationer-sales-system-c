# 文具店销售管理系统 — API 接口文档

> **版本**: v1.0  
> **基础URL**: `http://localhost:8080`  
> **数据格式**: JSON (Content-Type: application/json)  
> **字符编码**: UTF-8

---

## 目录

- [1. 通用规范](#1-通用规范)
- [2. 库存管理](#2-库存管理)
- [3. 销售管理](#3-销售管理)
- [4. 进货管理](#4-进货管理)
- [5. 多条件查询](#5-多条件查询)
- [6. 统计分析](#6-统计分析)
- [7. 智能预警与补货建议](#7-智能预警与补货建议)
- [8. 销售热力分析看板](#8-销售热力分析看板)
- [附录: 数据字典](#附录-数据字典)

---

## 1. 通用规范

### 1.1 统一响应格式

所有接口返回统一的 JSON 结构：

```json
{
  "code": 200,
  "message": "操作成功",
  "data": { ... }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| code | int | 状态码，200 表示成功 |
| message | string | 提示信息 |
| data | object/array/null | 返回数据体 |

### 1.2 业务状态码

| 状态码 | 含义 |
|--------|------|
| 200 | 请求成功 |
| 400 | 请求参数错误 |
| 404 | 资源不存在 |
| 409 | 业务冲突（如库存不足） |
| 500 | 服务器内部错误 |

### 1.3 分页规范

支持分页的列表接口统一使用以下查询参数：

| 参数 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| page | int | 1 | 当前页码，最小为 1 |
| page_size | int | 20 | 每页条数，最大 100 |
| keyword | string | — | 模糊搜索关键词 |

分页响应格式：

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "total": 100,
    "page": 1,
    "page_size": 20,
    "total_pages": 5,
    "list": [ ... ]
  }
}
```

---

## 2. 库存管理

### 2.1 库存列表（分页 + 搜索）

```
GET /api/inventory
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页条数，默认 20 |
| keyword | string | 否 | 按商品名称或编号模糊搜索 |
| category | string | 否 | 按类别筛选 |

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "total": 30,
    "page": 1,
    "page_size": 20,
    "total_pages": 2,
    "list": [
      {
        "id": 1,
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "category": "笔类",
        "manufacturer": "晨光文具",
        "model": "GP-1008",
        "stock_quantity": 120,
        "unit_price": 3.50,
        "safety_stock": 30,
        "warning_stock": 20,
        "created_at": "2026-01-15 10:30:00",
        "updated_at": "2026-05-20 14:22:00"
      }
    ]
  }
}
```

---

### 2.2 按编号查询单品

```
GET /api/inventory/:product_code
```

**路径参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| product_code | string | 是 | 商品编号，如 P001 |

**成功响应:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "id": 1,
    "product_code": "P001",
    "product_name": "晨光中性笔 GP-1008",
    "category": "笔类",
    "manufacturer": "晨光文具",
    "model": "GP-1008",
    "stock_quantity": 120,
    "unit_price": 3.50,
    "safety_stock": 30,
    "warning_stock": 20,
    "created_at": "2026-01-15 10:30:00",
    "updated_at": "2026-05-20 14:22:00"
  }
}
```

**失败响应 (404):**

```json
{
  "code": 404,
  "message": "商品编号不存在: P999",
  "data": null
}
```

---

### 2.3 新增商品

```
POST /api/inventory
```

**请求体:**

```json
{
  "product_code": "P031",
  "product_name": "斑马荧光笔 ZW-200",
  "category": "笔类",
  "manufacturer": "斑马文具",
  "model": "ZW-200",
  "stock_quantity": 100,
  "unit_price": 5.80,
  "safety_stock": 25,
  "warning_stock": 15
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| product_code | string | 是 | 商品编号，不可重复 |
| product_name | string | 是 | 商品名称，最长 100 字符 |
| category | string | 是 | 类别 |
| manufacturer | string | 是 | 生产厂家 |
| model | string | 是 | 型号 |
| stock_quantity | int | 是 | 初始库存数量 |
| unit_price | float | 是 | 单价，保留两位小数 |
| safety_stock | int | 否 | 安全库存阈值，默认 30 |
| warning_stock | int | 否 | 告警库存阈值，默认 20 |

**成功响应 (201):**

```json
{
  "code": 200,
  "message": "新增商品成功",
  "data": {
    "id": 31,
    "product_code": "P031",
    "product_name": "斑马荧光笔 ZW-200",
    "category": "笔类",
    "manufacturer": "斑马文具",
    "model": "ZW-200",
    "stock_quantity": 100,
    "unit_price": 5.80
  }
}
```

**失败响应 (409 — 编号重复):**

```json
{
  "code": 409,
  "message": "商品编号已存在: P031",
  "data": null
}
```

---

### 2.4 修改商品信息

```
PUT /api/inventory/:product_code
```

**请求体（全部字段可选，只更新传入的字段）:**

```json
{
  "product_name": "晨光中性笔 GP-1008 升级版",
  "stock_quantity": 150,
  "unit_price": 4.00
}
```

**成功响应:**

```json
{
  "code": 200,
  "message": "商品信息更新成功",
  "data": {
    "product_code": "P001",
    "product_name": "晨光中性笔 GP-1008 升级版",
    "stock_quantity": 150,
    "unit_price": 4.00
  }
}
```

---

### 2.5 删除商品

```
DELETE /api/inventory/:product_code
```

**成功响应:**

```json
{
  "code": 200,
  "message": "商品删除成功",
  "data": {
    "product_code": "P001"
  }
}
```

> **注意**: 存在关联销售或进货记录的商品不可删除，需先清理关联数据。

---

## 3. 销售管理

### 3.1 销售记录列表

```
GET /api/sales
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页条数，默认 20 |
| keyword | string | 否 | 按商品名称/编号模糊搜索 |
| start_date | string | 否 | 起始日期，格式 YYYY-MM-DD |
| end_date | string | 否 | 结束日期，格式 YYYY-MM-DD |
| category | string | 否 | 按类别筛选 |

---

### 3.2 按日期排序展示

```
GET /api/sales/sorted
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| order | string | 否 | 排序方向，asc（升序）或 desc（降序），默认 desc |
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页条数，默认 20 |

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "total": 35,
    "page": 1,
    "page_size": 20,
    "sort_order": "desc",
    "list": [
      {
        "id": 30,
        "product_code": "P003",
        "product_name": "百乐钢笔 BP-88G",
        "category": "笔类",
        "sale_date": "2026-05-23",
        "quantity": 3,
        "sale_price": 68.00,
        "total_amount": 204.00,
        "created_at": "2026-05-23 15:42:00"
      },
      {
        "id": 29,
        "product_code": "P015",
        "product_name": "得力文件夹 A4-蓝色",
        "category": "文件夹类",
        "sale_date": "2026-05-23",
        "quantity": 10,
        "sale_price": 3.50,
        "total_amount": 35.00,
        "created_at": "2026-05-23 11:20:00"
      }
    ]
  }
}
```

---

### 3.3 新增销售记录（仅记录，不联动库存）

```
POST /api/sales
```

**请求体:**

```json
{
  "product_code": "P001",
  "quantity": 5,
  "sale_price": 4.00
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| product_code | string | 是 | 商品编号 |
| quantity | int | 是 | 销售数量，最小 1 |
| sale_price | float | 是 | 实际售价 |
| sale_date | string | 否 | 交易日期，默认当天 |

---

### 3.4 销售流程（扣减库存 + 生成交易记录）

```
POST /api/sales/checkout
```

**请求体:**

```json
{
  "items": [
    {
      "product_code": "P001",
      "quantity": 5,
      "sale_price": 4.00
    },
    {
      "product_code": "P006",
      "quantity": 2,
      "sale_price": 9.50
    }
  ]
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| items | array | 是 | 商品列表，支持一次销售多件商品 |
| items[].product_code | string | 是 | 商品编号 |
| items[].quantity | int | 是 | 购买数量 |
| items[].sale_price | float | 是 | 实际售价 |

**成功响应:**

```json
{
  "code": 200,
  "message": "销售完成，库存已扣减",
  "data": {
    "sale_records": [
      {
        "id": 36,
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "quantity": 5,
        "sale_price": 4.00,
        "total_amount": 20.00,
        "sale_date": "2026-05-23"
      },
      {
        "id": 37,
        "product_code": "P006",
        "product_name": "得力笔记本 A5-60页",
        "quantity": 2,
        "sale_price": 9.50,
        "total_amount": 19.00,
        "sale_date": "2026-05-23"
      }
    ],
    "total_amount": 39.00
  }
}
```

**失败响应 (409 — 库存不足):**

```json
{
  "code": 409,
  "message": "库存不足: 得力笔记本 A5-60页 当前库存 1，需要 2",
  "data": {
    "product_code": "P006",
    "current_stock": 1,
    "requested": 2,
    "shortfall": 1
  }
}
```

---

## 4. 进货管理

### 4.1 进货记录列表

```
GET /api/purchases
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页条数，默认 20 |
| keyword | string | 否 | 按商品名称/编号模糊搜索 |
| start_date | string | 否 | 起始日期，格式 YYYY-MM-DD |
| end_date | string | 否 | 结束日期 |

---

### 4.2 新增进货记录（仅记录）

```
POST /api/purchases
```

**请求体:**

```json
{
  "product_code": "P001",
  "quantity": 50,
  "unit_price": 2.80,
  "purchase_date": "2026-05-20"
}
```

---

### 4.3 进货流程（增加库存 + 生成进货记录）

```
POST /api/purchases/restock
```

**请求体:**

```json
{
  "items": [
    {
      "product_code": "P001",
      "quantity": 50,
      "unit_price": 2.80
    },
    {
      "product_code": "P006",
      "quantity": 30,
      "unit_price": 6.50
    }
  ]
}
```

**成功响应:**

```json
{
  "code": 200,
  "message": "进货完成，库存已更新",
  "data": {
    "purchase_records": [
      {
        "id": 16,
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "quantity": 50,
        "unit_price": 2.80,
        "total_cost": 140.00,
        "purchase_date": "2026-05-23"
      }
    ],
    "total_cost": 335.00
  }
}
```

---

## 5. 多条件查询

### 5.1 多条件组合查询

```
GET /api/search
```

支持对库存、销售、进货数据进行多维度组合查询。

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| type | string | 是 | 查询类型: `inventory` / `sales` / `purchases` |
| keyword | string | 否 | 按名称模糊搜索 |
| product_code | string | 否 | 按编号精确搜索 |
| category | string | 否 | 按类别筛选 |
| manufacturer | string | 否 | 按生产厂家筛选 |
| start_date | string | 否 | 起始日期 (适用于 sales/purchases) |
| end_date | string | 否 | 结束日期 |
| min_price | float | 否 | 最低价格 |
| max_price | float | 否 | 最高价格 |
| min_quantity | int | 否 | 最低数量 |
| max_quantity | int | 否 | 最高数量 |
| page | int | 否 | 页码，默认 1 |
| page_size | int | 否 | 每页条数，默认 20 |

**请求示例 — 查询近一周笔类商品中售价在 3~10 元之间的销售记录:**

```
GET /api/search?type=sales&category=笔类&start_date=2026-05-17&end_date=2026-05-23&min_price=3&max_price=10
```

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "total": 12,
    "page": 1,
    "page_size": 20,
    "conditions": {
      "type": "sales",
      "category": "笔类",
      "start_date": "2026-05-17",
      "end_date": "2026-05-23",
      "min_price": 3.00,
      "max_price": 10.00
    },
    "list": [ ... ]
  }
}
```

---

## 6. 统计分析

### 6.1 综合统计概览

```
GET /api/stats/overview
```

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "inventory_summary": {
      "total_products": 30,
      "total_categories": 8,
      "total_stock_value": 12580.50,
      "total_stock_quantity": 3240
    },
    "sales_summary": {
      "today_sales_amount": 820.00,
      "today_sales_count": 15,
      "this_week_sales_amount": 4520.00,
      "this_month_sales_amount": 18350.00,
      "total_sales_records": 35
    },
    "purchase_summary": {
      "this_month_purchase_cost": 5200.00,
      "total_purchase_records": 15
    },
    "sales_ranking": [
      {
        "rank": 1,
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "total_sold": 180,
        "total_amount": 720.00
      },
      {
        "rank": 2,
        "product_code": "P011",
        "product_name": "樱花橡皮 X-101",
        "total_sold": 150,
        "total_amount": 300.00
      }
    ],
    "inventory_warnings": [
      {
        "product_code": "P025",
        "product_name": "英雄墨水 纯蓝-50ml",
        "stock_quantity": 5,
        "safety_stock": 30,
        "warning_stock": 20,
        "level": "danger"
      }
    ]
  }
}
```

---

### 6.2 橡皮类商品专项统计

```
GET /api/stats/eraser
```

统计近一周内橡皮类商品的销售总额及当前库存。

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "category": "橡皮类",
    "period": {
      "start": "2026-05-17",
      "end": "2026-05-23"
    },
    "weekly_sales_total": 285.00,
    "weekly_sales_count": 42,
    "products": [
      {
        "product_code": "P011",
        "product_name": "樱花橡皮 X-101",
        "current_stock": 185,
        "weekly_sold": 28,
        "weekly_amount": 56.00,
        "unit_price": 2.00
      },
      {
        "product_code": "P012",
        "product_name": "晨光橡皮 4B-考试专用",
        "current_stock": 95,
        "weekly_sold": 14,
        "weekly_amount": 21.00,
        "unit_price": 1.50
      }
    ]
  }
}
```

---

## 7. 智能预警与补货建议

### 7.1 库存预警列表

```
GET /api/alerts
```

获取所有库存低于阈值的商品，按危险程度排序。

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| level | string | 否 | 预警级别: `danger` (低于告警阈值) / `warning` (低于安全阈值) / `all` |

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "total_alerts": 5,
    "danger_count": 2,
    "warning_count": 3,
    "list": [
      {
        "product_code": "P025",
        "product_name": "英雄墨水 纯蓝-50ml",
        "category": "墨水类",
        "stock_quantity": 5,
        "safety_stock": 30,
        "warning_stock": 20,
        "unit_price": 8.50,
        "level": "danger",
        "status_text": "⚠ 严重缺货 — 仅剩 5 件"
      },
      {
        "product_code": "P019",
        "product_name": "晨光剪刀 SC-210",
        "category": "工具类",
        "stock_quantity": 12,
        "safety_stock": 25,
        "warning_stock": 15,
        "unit_price": 6.00,
        "level": "danger",
        "status_text": "⚠ 严重缺货 — 仅剩 12 件"
      },
      {
        "product_code": "P008",
        "product_name": "国誉活页本 B5-26孔",
        "category": "本类",
        "stock_quantity": 22,
        "safety_stock": 30,
        "warning_stock": 20,
        "unit_price": 15.00,
        "level": "warning",
        "status_text": "⚡ 库存偏低 — 建议补货"
      }
    ]
  }
}
```

**预警级别判定规则:**

| 级别 | 条件 | 标签颜色 |
|------|------|----------|
| danger | stock_quantity ≤ warning_stock | 红色 |
| warning | stock_quantity ≤ safety_stock 且 > warning_stock | 橙色 |
| safe | stock_quantity > safety_stock | 绿色 |

---

### 7.2 自动补货建议

```
GET /api/suggestions
```

基于近 30 天销售速率和安全库存系数，计算每件商品的建议采购量。

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| category | string | 否 | 按类别筛选 |

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "algorithm": "建议采购量 = 日均销量 × 补货周期(30天) × 安全系数(1.2) - 当前库存",
    "list": [
      {
        "product_code": "P025",
        "product_name": "英雄墨水 纯蓝-50ml",
        "category": "墨水类",
        "current_stock": 5,
        "daily_avg_sales": 1.8,
        "suggested_quantity": 60,
        "estimated_cost": 510.00,
        "urgency": "high",
        "reason": "库存严重不足，仅够 2.8 天销售"
      },
      {
        "product_code": "P001",
        "product_name": "晨光中性笔 GP-1008",
        "category": "笔类",
        "current_stock": 120,
        "daily_avg_sales": 6.5,
        "suggested_quantity": 114,
        "estimated_cost": 319.20,
        "urgency": "medium",
        "reason": "畅销品，建议维持 30 天安全库存"
      },
      {
        "product_code": "P030",
        "product_name": "派克钢笔 墨水囊-5支装",
        "category": "笔类",
        "current_stock": 45,
        "daily_avg_sales": 0.3,
        "suggested_quantity": 0,
        "estimated_cost": 0,
        "urgency": "low",
        "reason": "库存充足，无需补货"
      }
    ]
  }
}
```

**补货量计算公式:**

```
建议采购量 = MAX(0, (日均销量 × 补货周期(30天) × 安全系数(1.2)) - 当前库存)
```

**紧急程度判定:**

| 级别 | 条件 | 说明 |
|------|------|------|
| high | 当前库存 < 7天需求 | 须立即补货 |
| medium | 当前库存 < 30天需求 | 建议尽快补货 |
| low | 当前库存 ≥ 30天需求 | 库存充足 |

---

## 8. 销售热力分析看板

### 8.1 品类销售热度

```
GET /api/heatmap/category
```

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| start_date | string | 否 | 起始日期 |
| end_date | string | 否 | 结束日期 |

**响应示例:**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "period": {
      "start": "2026-04-23",
      "end": "2026-05-23"
    },
    "categories": [
      {
        "category": "笔类",
        "total_sales": 8500.00,
        "total_quantity": 1250,
        "product_count": 12,
        "percentage": 38.5,
        "heat_level": "hot"
      },
      {
        "category": "本类",
        "total_sales": 4800.00,
        "total_quantity": 520,
        "product_count": 6,
        "percentage": 21.7,
        "heat_level": "warm"
      },
      {
        "category": "橡皮类",
        "total_sales": 1200.00,
        "total_quantity": 600,
        "product_count": 3,
        "percentage": 5.4,
        "heat_level": "cool"
      }
    ],
    "total_amount": 22100.00
  }
}
```

**热度等级:**

| 等级 | 条件 | 说明 |
|------|------|------|
| hot | 占比 ≥ 25% | 核心品类 |
| warm | 占比 10% ~ 25% | 常规品类 |
| cool | 占比 < 10% | 小众品类 |

---

### 8.2 时段销售热力分析

```
GET /api/heatmap/sales
```

分析销量在不同时间维度的分布，用于热力图渲染。

**查询参数:**

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| dimension | string | 否 | 维度: `daily` / `weekly` / `monthly`，默认 `daily` |
| start_date | string | 否 | 起始日期 |
| end_date | string | 否 | 结束日期 |
| category | string | 否 | 按品类筛选 |

**响应示例 (daily):**

```json
{
  "code": 200,
  "message": "查询成功",
  "data": {
    "dimension": "daily",
    "period": {
      "start": "2026-05-01",
      "end": "2026-05-23"
    },
    "heatmap_data": [
      {
        "date": "2026-05-01",
        "day_of_week": "周四",
        "total_amount": 680.00,
        "total_orders": 22,
        "categories": {
          "笔类": {"amount": 280.00, "quantity": 45},
          "本类": {"amount": 200.00, "quantity": 12},
          "橡皮类": {"amount": 60.00, "quantity": 30}
        }
      },
      {
        "date": "2026-05-02",
        "day_of_week": "周五",
        "total_amount": 750.00,
        "total_orders": 25,
        "categories": {
          "笔类": {"amount": 320.00, "quantity": 52},
          "本类": {"amount": 180.00, "quantity": 10}
        }
      }
    ],
    "summary": {
      "best_day": {"date": "2026-05-15", "amount": 1250.00},
      "best_category": {"category": "笔类", "amount": 8500.00},
      "daily_avg_amount": 645.00,
      "total_amount": 14835.00
    }
  }
}
```

---

## 附录: 数据字典

### A. 商品类别枚举

| 类别编码 | 类别名称 | 说明 |
|----------|----------|------|
| 笔类 | 书写工具 | 钢笔、中性笔、铅笔、荧光笔等 |
| 本类 | 纸张本册 | 笔记本、活页本、便签本等 |
| 尺类 | 测量工具 | 直尺、三角尺、量角器等 |
| 橡皮类 | 擦除工具 | 橡皮、修正带等 |
| 文具盒类 | 收纳用品 | 笔袋、文具盒等 |
| 文件夹类 | 文件管理 | 文件夹、档案盒等 |
| 工具类 | 辅助工具 | 剪刀、胶水、订书机等 |
| 墨水类 | 耗材 | 墨水、墨囊等 |

### B. 库存表 (inventory)

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| product_code | VARCHAR(20) | UNIQUE, NOT NULL | 商品编号 |
| product_name | VARCHAR(100) | NOT NULL | 商品名称 |
| category | VARCHAR(20) | NOT NULL | 类别 |
| manufacturer | VARCHAR(80) | NOT NULL | 生产厂家 |
| model | VARCHAR(50) | NOT NULL | 型号 |
| stock_quantity | INT | DEFAULT 0 | 库存数量 |
| unit_price | DECIMAL(10,2) | NOT NULL | 单价 |
| safety_stock | INT | DEFAULT 30 | 安全库存阈值 |
| warning_stock | INT | DEFAULT 20 | 告警库存阈值 |
| created_at | DATETIME | DEFAULT NOW() | 创建时间 |
| updated_at | DATETIME | ON UPDATE NOW() | 更新时间 |

### C. 销售表 (sales)

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| product_code | VARCHAR(20) | FK→inventory | 商品编号 |
| product_name | VARCHAR(100) | NOT NULL | 商品名称（冗余） |
| category | VARCHAR(20) | NOT NULL | 类别（冗余） |
| sale_date | DATE | NOT NULL | 交易日期 |
| quantity | INT | NOT NULL | 销售数量 |
| sale_price | DECIMAL(10,2) | NOT NULL | 售价 |
| total_amount | DECIMAL(12,2) | NOT NULL | 小计 (quantity × sale_price) |
| created_at | DATETIME | DEFAULT NOW() | 记录时间 |

### D. 进货表 (purchases)

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| product_code | VARCHAR(20) | FK→inventory | 商品编号 |
| product_name | VARCHAR(100) | NOT NULL | 商品名称（冗余） |
| category | VARCHAR(20) | NOT NULL | 类别（冗余） |
| quantity | INT | NOT NULL | 进货数量 |
| unit_price | DECIMAL(10,2) | NOT NULL | 进货单价 |
| total_cost | DECIMAL(12,2) | NOT NULL | 小计 (quantity × unit_price) |
| purchase_date | DATE | NOT NULL | 进货日期 |
| created_at | DATETIME | DEFAULT NOW() | 记录时间 |

---

> **文档制作者**: AI 软件开发助手  
> **最后更新**: 2026-05-23
