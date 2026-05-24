-- =============================================================
-- 文具店销售管理系统 - 数据库建库建表脚本
-- Stationery Store Sales Management System - Database Schema
-- =============================================================
-- 说明 / Instructions:
--   本脚本用于首次初始化系统数据库。
--   使用方法：连接 MySQL 8.0 实例后，在命令行或客户端中执行：
--     mysql -u root -p < schema.sql
--   或在 MySQL 客户端中执行：
--     source /path/to/schema.sql
--   脚本包含 DROP TABLE IF EXISTS 语句，可重复执行而不会报错，
--   但会清空已有数据，生产环境请谨慎使用。
-- =============================================================
-- 环境要求 / Requirements:
--   MySQL 8.0+
--   字符集 utf8mb4
-- =============================================================

-- ---------------------------------------------------------
-- 1. 创建数据库
-- ---------------------------------------------------------
DROP DATABASE IF EXISTS stationery_db;
CREATE DATABASE stationery_db
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE stationery_db;

-- ---------------------------------------------------------
-- 2. 库存表 / Inventory
-- ---------------------------------------------------------
DROP TABLE IF EXISTS inventory;
CREATE TABLE inventory (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '主键ID',
    product_code    VARCHAR(20)     NOT NULL                 COMMENT '商品编号，唯一标识每种商品',
    product_name    VARCHAR(100)    NOT NULL                 COMMENT '商品名称',
    category        VARCHAR(20)     NOT NULL                 COMMENT '商品分类（如：笔类、本册类、办公用品等）',
    manufacturer    VARCHAR(80)     NOT NULL                 COMMENT '生产厂家 / 品牌',
    model           VARCHAR(50)     NOT NULL                 COMMENT '型号规格',
    stock_quantity  INT             NOT NULL DEFAULT 0       COMMENT '当前库存数量',
    unit_price      DECIMAL(10,2)   NOT NULL                 COMMENT '建议零售单价（元）',
    safety_stock    INT             NOT NULL DEFAULT 30      COMMENT '安全库存量，低于此值建议补货',
    warning_stock   INT             NOT NULL DEFAULT 20      COMMENT '预警库存量，低于此值触发紧急告警',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录创建时间',
    updated_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '记录最近更新时间',
    PRIMARY KEY (id),
    UNIQUE KEY uk_product_code (product_code),
    INDEX idx_inventory_category (category),
    INDEX idx_inventory_product_name (product_name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='库存表 - 存储所有文具商品的库存信息';

-- ---------------------------------------------------------
-- 3. 销售表 / Sales
-- ---------------------------------------------------------
DROP TABLE IF EXISTS sales;
CREATE TABLE sales (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '主键ID',
    product_code    VARCHAR(20)     NOT NULL                 COMMENT '商品编号',
    product_name    VARCHAR(100)    NOT NULL                 COMMENT '商品名称',
    category        VARCHAR(20)     NOT NULL                 COMMENT '商品分类',
    sale_date       DATE            NOT NULL                 COMMENT '销售日期',
    quantity        INT             NOT NULL                 COMMENT '销售数量',
    sale_price      DECIMAL(10,2)   NOT NULL                 COMMENT '实际销售单价（元）',
    total_amount    DECIMAL(12,2)   NOT NULL                 COMMENT '销售总金额 = 数量 × 单价（元）',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录创建时间',
    PRIMARY KEY (id),
    INDEX idx_sales_product_code (product_code),
    INDEX idx_sales_category (category),
    INDEX idx_sales_sale_date (sale_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='销售表 - 存储所有销售记录';

-- ---------------------------------------------------------
-- 4. 进货表 / Purchases
-- ---------------------------------------------------------
DROP TABLE IF EXISTS purchases;
CREATE TABLE purchases (
    id              BIGINT          NOT NULL AUTO_INCREMENT  COMMENT '主键ID',
    product_code    VARCHAR(20)     NOT NULL                 COMMENT '商品编号',
    product_name    VARCHAR(100)    NOT NULL                 COMMENT '商品名称',
    category        VARCHAR(20)     NOT NULL                 COMMENT '商品分类',
    quantity        INT             NOT NULL                 COMMENT '进货数量',
    unit_price      DECIMAL(10,2)   NOT NULL                 COMMENT '进货单价（元）',
    total_cost      DECIMAL(12,2)   NOT NULL                 COMMENT '进货总成本 = 数量 × 单价（元）',
    purchase_date   DATE            NOT NULL                 COMMENT '进货日期',
    created_at      DATETIME        NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '记录创建时间',
    PRIMARY KEY (id),
    INDEX idx_purchases_product_code (product_code),
    INDEX idx_purchases_category (category),
    INDEX idx_purchases_purchase_date (purchase_date)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='进货表 - 存储所有进货记录';
