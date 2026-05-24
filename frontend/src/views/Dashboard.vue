<template>
  <div class="dashboard">
    <div class="page-header">
      <h2>首页分析看板</h2>
    </div>

    <!-- 统计卡片 -->
    <div class="stat-cards">
      <div class="stat-card">
        <div class="label">今日销售额</div>
        <div class="value">{{ overview.sales_summary?.today_amount != null ? '¥' + overview.sales_summary.today_amount.toFixed(2) : '--' }}</div>
      </div>
      <div class="stat-card">
        <div class="label">今日订单数</div>
        <div class="value">{{ overview.sales_summary?.today_count ?? '--' }}</div>
      </div>
      <div class="stat-card">
        <div class="label">库存商品数</div>
        <div class="value">{{ overview.inventory_summary?.total_products ?? '--' }}</div>
      </div>
      <div class="stat-card">
        <div class="label">预警商品数</div>
        <div class="value warning">{{ alertTotal }}</div>
      </div>
    </div>

    <el-row :gutter="16" style="margin-bottom: 20px;">
      <!-- 库存预警列表 -->
      <el-col :span="14">
        <div class="table-card">
          <div class="page-header">
            <h2>库存预警列表</h2>
          </div>
          <el-table
            :data="warningList"
            v-loading="warningLoading"
            style="width: 100%"
            empty-text="暂无预警商品"
            size="small"
          >
            <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
            <el-table-column prop="product_code" label="编号" width="90" />
            <el-table-column prop="category" label="类别" width="90" />
            <el-table-column prop="stock_quantity" label="当前库存" width="90" align="center" />
            <el-table-column prop="safety_stock" label="安全阈值" width="90" align="center" />
            <el-table-column label="状态" width="120" align="center">
              <template #default="{ row }">
                <el-tag :type="row.level === 'danger' ? 'danger' : 'warning'" size="small">
                  {{ row.level === 'danger' ? '严重缺货' : '库存偏低' }}
                </el-tag>
              </template>
            </el-table-column>
          </el-table>
        </div>
      </el-col>

      <!-- 销售排行 TOP10 -->
      <el-col :span="10">
        <div class="table-card">
          <div class="page-header">
            <h2>销售排行 TOP10</h2>
          </div>
          <el-table
            :data="salesRanking"
            v-loading="overviewLoading"
            style="width: 100%"
            empty-text="暂无销售数据"
            size="small"
          >
            <el-table-column label="排名" width="60" align="center">
              <template #default="{ row }">
                <el-tag :type="row.rank <= 3 ? 'danger' : 'info'" size="small" effect="dark">
                  {{ row.rank }}
                </el-tag>
              </template>
            </el-table-column>
            <el-table-column prop="product_name" label="商品名称" min-width="130" show-overflow-tooltip />
            <el-table-column prop="total_sold" label="销量" width="70" align="center" />
            <el-table-column label="销售额" width="90" align="right">
              <template #default="{ row }">
                ¥{{ row.total_amount?.toFixed(2) }}
              </template>
            </el-table-column>
          </el-table>
        </div>
      </el-col>
    </el-row>

    <!-- 热力图区域 -->
    <HeatmapChart
      :categoryData="categoryHeatData"
      :salesData="salesHeatData"
      :loading="heatmapLoading"
    />
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { statsApi, heatmapApi, alertApi } from '@/api/index.js'
import { ElMessage } from 'element-plus'
import HeatmapChart from '@/components/HeatmapChart.vue'

const overview = reactive({
  inventory_summary: null,
  sales_summary: null,
  purchase_summary: null,
  sales_ranking: [],
  inventory_warnings: []
})
const overviewLoading = ref(false)
const salesRanking = ref([])
const alertTotal = ref(0)

const warningList = ref([])
const warningLoading = ref(false)

const categoryHeatData = ref(null)
const salesHeatData = ref(null)
const heatmapLoading = ref(false)

onMounted(async () => {
  await Promise.all([
    fetchOverview(),
    fetchAlerts(),
    fetchHeatmapCategory(),
    fetchHeatmapSales()
  ])
})

async function fetchOverview() {
  overviewLoading.value = true
  try {
    const res = await statsApi.overview()
    const d = res.data
    if (d.inventory_summary) overview.inventory_summary = d.inventory_summary
    if (d.sales_summary) overview.sales_summary = d.sales_summary
    if (d.purchase_summary) overview.purchase_summary = d.purchase_summary
    if (d.sales_ranking) {
      overview.sales_ranking = d.sales_ranking
      salesRanking.value = d.sales_ranking
    }
    if (d.inventory_warnings) overview.inventory_warnings = d.inventory_warnings
  } catch (err) {
    ElMessage.error('获取概览数据失败: ' + err.message)
  } finally {
    overviewLoading.value = false
  }
}

async function fetchAlerts() {
  warningLoading.value = true
  try {
    const res = await alertApi.list({ level: 'all' })
    const d = res.data
    alertTotal.value = d.total_alerts ?? 0
    warningList.value = d.list ?? []
  } catch (err) {
    ElMessage.error('获取预警数据失败: ' + err.message)
  } finally {
    warningLoading.value = false
  }
}

async function fetchHeatmapCategory() {
  try {
    const res = await heatmapApi.category()
    categoryHeatData.value = res.data?.categories ?? []
  } catch (err) {
    ElMessage.error('获取品类热度数据失败: ' + err.message)
  }
}

async function fetchHeatmapSales() {
  heatmapLoading.value = true
  try {
    const res = await heatmapApi.sales({ dimension: 'daily' })
    salesHeatData.value = res.data?.heatmap_data ?? []
  } catch (err) {
    ElMessage.error('获取销售热力数据失败: ' + err.message)
  } finally {
    heatmapLoading.value = false
  }
}
</script>

<style scoped>
.dashboard {
  width: 100%;
}

.stat-cards {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 16px;
  margin-bottom: 24px;
}

.stat-card {
  background: #fff;
  border-radius: 8px;
  padding: 20px;
  box-shadow: 0 1px 3px rgba(0,0,0,0.06);
}

.stat-card .label {
  font-size: 13px;
  color: #64748b;
  margin-bottom: 8px;
}

.stat-card .value {
  font-size: 28px;
  font-weight: 700;
  color: #1e293b;
}

.stat-card .value.warning {
  color: #d97706;
}

.table-card {
  background: #fff;
  border-radius: 8px;
  box-shadow: 0 1px 3px rgba(0,0,0,0.06);
  padding: 20px;
  margin-bottom: 20px;
}

.page-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.page-header h2 {
  font-size: 16px;
  font-weight: 600;
}
</style>
