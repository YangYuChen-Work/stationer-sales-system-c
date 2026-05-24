# 文具店销售管理系统 — 设计规格说明书

> **版本**: v1.0 | **日期**: 2026-05-23 | **状态**: 已确认

## 1. 项目概述

文具店销售管理系统，提供库存管理、销售管理、进货管理、智能分析等功能。目标用户为文具店经营者，支持日常销售/进货业务闭环、数据查询与统计分析。

## 2. 技术架构

### 2.1 整体架构

```
浏览器 (Vue3 + Element Plus)
    ↕ HTTP REST API (JSON)
C HTTP 服务器 (Mongoose 嵌入式)
    ↕ SQL
MySQL 8.0 数据库
```

### 2.2 技术选型

| 层 | 技术 | 说明 |
|---|------|------|
| 前端 | Vue 3 + Vite + Element Plus + ECharts | SPA 单页应用，Element Plus 组件库，ECharts 图表 |
| 后端 | C11 + Mongoose 7.x | HTTP API 服务，Mongoose 处理网络层 |
| 数据库 | MySQL 8.0 + libmysqlclient | InnoDB 引擎，UTF-8 编码 |
| 数据格式 | JSON | 前后端通信格式 |
| 编译 | GCC / MinGW-w64 | 跨平台编译 |
| 图表 | ECharts 5.x | 热力图、柱状图、饼图 |

### 2.3 项目目录结构

```
文具店销售管理系统设计/
├── backend/                  # C语言后端
│   ├── src/
│   │   ├── main.c            # 主入口，HTTP服务启动
│   │   ├── db.h / db.c       # 数据库连接池
│   │   ├── api.h / api.c     # 路由分发器
│   │   ├── inventory.h/.c    # 库存CRUD模块
│   │   ├── sales.h/.c        # 销售管理模块
│   │   ├── purchase.h/.c     # 进货管理模块
│   │   ├── stats.h/.c        # 统计分析模块
│   │   ├── alert.h/.c        # 库存预警模块
│   │   ├── suggest.h/.c      # 补货建议模块
│   │   ├── heatmap.h/.c      # 热力分析模块
│   │   └── json.h / json.c   # JSON工具
│   ├── lib/
│   │   └── mongoose.c/.h     # 嵌入式Web服务器
│   ├── Makefile              # Linux编译
│   └── build.bat             # Windows编译脚本
├── frontend/                 # Vue3 前端项目
│   ├── src/
│   │   ├── views/            # 5个功能页面
│   │   │   ├── Inventory.vue     # 库存管理
│   │   │   ├── Sales.vue         # 销售管理
│   │   │   ├── Purchase.vue      # 进货管理
│   │   │   ├── Dashboard.vue     # 统计看板（首页）
│   │   │   └── Search.vue        # 高级搜索
│   │   ├── components/       # 复用组件
│   │   │   ├── AppLayout.vue     # 主布局
│   │   │   ├── InventoryForm.vue # 商品表单
│   │   │   ├── CheckoutDialog.vue# 销售收银弹窗
│   │   │   ├── RestockDialog.vue # 进货弹窗
│   │   │   ├── AlertBadge.vue    # 预警徽标
│   │   │   └── HeatmapChart.vue  # 热力图
│   │   ├── api/              # API 调用封装
│   │   │   └── index.js
│   │   ├── router/
│   │   │   └── index.js
│   │   ├── App.vue
│   │   └── main.js
│   ├── vite.config.js
│   ├── package.json
│   └── index.html
├── database/
│   ├── schema.sql            # 建表脚本
│   └── seed.sql              # 100+ 条模拟数据
├── API接口文档.md
└── 文具店销售管理系统设计系统需求文档.md
```

## 3. 数据库设计

### 3.1 库存表 (inventory)

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

### 3.2 销售表 (sales)

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| product_code | VARCHAR(20) | INDEX | 商品编号 |
| product_name | VARCHAR(100) | NOT NULL | 商品名称 |
| category | VARCHAR(20) | NOT NULL | 类别 |
| sale_date | DATE | NOT NULL, INDEX | 交易日期 |
| quantity | INT | NOT NULL | 销售数量 |
| sale_price | DECIMAL(10,2) | NOT NULL | 售价 |
| total_amount | DECIMAL(12,2) | NOT NULL | 小计 |
| created_at | DATETIME | DEFAULT NOW() | 记录时间 |

### 3.3 进货表 (purchases)

| 字段 | 类型 | 约束 | 说明 |
|------|------|------|------|
| id | INT | PK, AUTO_INCREMENT | 主键 |
| product_code | VARCHAR(20) | INDEX | 商品编号 |
| product_name | VARCHAR(100) | NOT NULL | 商品名称 |
| category | VARCHAR(20) | NOT NULL | 类别 |
| quantity | INT | NOT NULL | 进货数量 |
| unit_price | DECIMAL(10,2) | NOT NULL | 进货单价 |
| total_cost | DECIMAL(12,2) | NOT NULL | 小计 |
| purchase_date | DATE | NOT NULL | 进货日期 |
| created_at | DATETIME | DEFAULT NOW() | 记录时间 |

## 4. API 端点清单（18个）

### 4.1 库存管理（5个）
- `GET /api/inventory` — 库存列表
- `GET /api/inventory/:code` — 按编号查询
- `POST /api/inventory` — 新增商品
- `PUT /api/inventory/:code` — 修改商品
- `DELETE /api/inventory/:code` — 删除商品

### 4.2 销售管理（4个）
- `GET /api/sales` — 销售列表
- `GET /api/sales/sorted` — 按日期排序
- `POST /api/sales` — 新增记录
- `POST /api/sales/checkout` — 销售流程（扣库存）

### 4.3 进货管理（3个）
- `GET /api/purchases` — 进货列表
- `POST /api/purchases` — 新增记录
- `POST /api/purchases/restock` — 进货流程（增库存）

### 4.4 查询与统计（4个）
- `GET /api/search` — 多条件组合查询
- `GET /api/stats/overview` — 综合统计
- `GET /api/stats/eraser` — 橡皮专项统计

### 4.5 智能功能（4个）
- `GET /api/alerts` — 库存预警
- `GET /api/suggestions` — 补货建议
- `GET /api/heatmap/category` — 品类热度
- `GET /api/heatmap/sales` — 时段热力图

## 5. 前端页面设计

| 页面 | 路由 | 说明 |
|------|------|------|
| 首页看板 | / | 综合统计概览 + 预警列表 + 热力图 |
| 库存管理 | /inventory | 商品表 + CRUD弹窗 + 编号查询 |
| 销售管理 | /sales | 销售记录 + 日期排序 + 收银流程 |
| 进货管理 | /purchases | 进货记录 + 补货流程 |
| 高级搜索 | /search | 多条件组合查询表单 |

## 6. 核心业务流程

### 6.1 销售流程
```
收银台选择商品 + 数量 → 校验库存充足 → 扣减库存 → 生成销售记录 → 返回小票
```
事务保证：库存扣减与销售记录在同一 MySQL 事务中。

### 6.2 进货流程
```
选择商品 + 数量 + 进价 → 增加库存 → 生成进货记录 → 更新库存金额
```
事务保证：库存增加与进货记录在同一 MySQL 事务中。

### 6.3 智能预警
```
定时/实时计算: 当前库存 vs 安全阈值 → 三级分类(安全/警告/危险) → 看板展示
```

### 6.4 补货建议
```
计算规则: 日均销量 = 近30天总销量 / 30
建议采购量 = MAX(0, 日均销量 × 30 × 1.2 - 当前库存)
紧急程度: 库存天数 < 7 → high | < 30 → medium | ≥ 30 → low
```

## 7. 模拟数据

- inventory: 30 条（8 个类别，5 个厂家）
- sales: 50+ 条（近一个月，含跨品类数据）
- purchases: 20+ 条（近三个月）

## 8. 编码规范

- C 代码遵循 C11 标准，使用 GCC/MinGW-w64 编译
- 每个 .c 文件有对应 .h 头文件
- 函数命名使用 `snake_case`，模块前缀（如 `inv_`, `sales_`, `db_`）
- 关键逻辑有中文注释说明
- 前端 Vue 3 Composition API，`<script setup>` 语法
- 每个 Vue 组件有清晰的职责边界
