<template>
  <div class="purchase">
    <div class="page-header">
      <h2>进货管理</h2>
      <el-button type="primary" @click="openRestockDialog">新增进货</el-button>
    </div>

    <!-- 筛选栏 -->
    <div class="table-card" style="margin-bottom: 16px;">
      <el-row :gutter="12" align="middle">
        <el-col :span="5">
          <el-input v-model="filters.keyword" placeholder="搜索商品名称或编号" clearable @keyup.enter="handleSearch" />
        </el-col>
        <el-col :span="4">
          <el-date-picker v-model="filters.dateRange" type="daterange" range-separator="至"
            start-placeholder="开始日期" end-placeholder="结束日期" format="YYYY-MM-DD" value-format="YYYY-MM-DD"
            style="width:100%" unlink-panels />
        </el-col>
        <el-col :span="2">
          <el-button type="primary" @click="handleSearch" :loading="loading">搜索</el-button>
        </el-col>
      </el-row>
    </div>

    <!-- 表格 -->
    <div class="table-card">
      <el-table :data="tableData" v-loading="loading" style="width:100%" empty-text="暂无进货记录" stripe>
        <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
        <el-table-column prop="product_code" label="编号" width="100" />
        <el-table-column prop="category" label="类别" width="90" />
        <el-table-column prop="quantity" label="数量" width="80" align="center" />
        <el-table-column label="进价" width="90" align="right">
          <template #default="{ row }">¥{{ row.unit_price?.toFixed(2) }}</template>
        </el-table-column>
        <el-table-column label="小计" width="100" align="right">
          <template #default="{ row }">¥{{ row.total_cost?.toFixed(2) }}</template>
        </el-table-column>
        <el-table-column prop="purchase_date" label="日期" width="110" />
      </el-table>

      <div style="display:flex;justify-content:flex-end;margin-top:16px;">
        <el-pagination
          v-model:current-page="pagination.page"
          v-model:page-size="pagination.pageSize"
          :total="pagination.total"
          :page-sizes="[10, 20, 50, 100]"
          layout="total, sizes, prev, pager, next, jumper"
          @size-change="fetchList"
          @current-change="fetchList"
        />
      </div>
    </div>

    <!-- 补货入库弹窗 -->
    <el-dialog v-model="restockVisible" title="补货入库" width="700px" :close-on-click-modal="false" @close="resetRestock">
      <div v-if="restockItems.length === 0" style="text-align:center;padding:20px;color:#999;">
        请添加进货商品
      </div>
      <div v-else>
        <div style="margin-bottom:12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
          <el-select v-model="restockProduct" filterable placeholder="搜索并选择商品" style="width:220px"
            @change="onRestockProductSelect">
            <el-option v-for="p in inventoryList" :key="p.product_code" :label="p.product_code + ' ' + p.product_name"
              :value="p.product_code" />
          </el-select>
          <el-input-number v-model="restockQty" :min="1" placeholder="数量" style="width:100px" />
          <el-input-number v-model="restockCostPrice" :min="0" :precision="2" :step="0.5" placeholder="进价" style="width:120px" />
          <el-button type="primary" @click="addRestockItem" :disabled="!restockProduct || restockQty < 1">添加</el-button>
        </div>
        <el-table :data="restockItems" size="small" style="margin-bottom:16px;">
          <el-table-column prop="product_code" label="编号" width="90" />
          <el-table-column prop="product_name" label="商品名称" min-width="130" show-overflow-tooltip />
          <el-table-column prop="quantity" label="数量" width="60" align="center" />
          <el-table-column label="进价" width="90" align="right">
            <template #default="{ row }">¥{{ row.unit_price?.toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="小计" width="100" align="right">
            <template #default="{ row }">¥{{ (row.quantity * row.unit_price).toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="操作" width="60" align="center">
            <template #default="{ $index }">
              <el-button type="danger" link size="small" @click="restockItems.splice($index, 1)">移除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div style="text-align:right;font-size:18px;font-weight:700;margin-bottom:12px;">
          合计成本: ¥{{ restockTotal.toFixed(2) }}
        </div>
      </div>

      <template #footer>
        <el-button @click="restockVisible = false">关闭</el-button>
        <el-button type="primary" @click="handleRestock" :loading="restockLoading" :disabled="restockItems.length === 0">
          确认入库
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { purchaseApi, inventoryApi } from '@/api/index.js'
import { ElMessage } from 'element-plus'

const loading = ref(false)
const tableData = ref([])

const filters = reactive({
  keyword: '',
  dateRange: null
})

const pagination = reactive({
  page: 1,
  pageSize: 20,
  total: 0
})

// 补货入库
const restockVisible = ref(false)
const restockLoading = ref(false)
const restockItems = ref([])

const restockProduct = ref('')
const restockQty = ref(1)
const restockCostPrice = ref(0)
const inventoryList = ref([])

const restockTotal = computed(() => {
  return restockItems.value.reduce((sum, item) => sum + item.quantity * item.unit_price, 0)
})

onMounted(() => {
  fetchList()
})

async function fetchList() {
  loading.value = true
  try {
    const params = { page: pagination.page, page_size: pagination.pageSize }
    if (filters.keyword) params.keyword = filters.keyword
    if (filters.dateRange && filters.dateRange.length === 2) {
      params.start_date = filters.dateRange[0]
      params.end_date = filters.dateRange[1]
    }
    const res = await purchaseApi.list(params)
    const d = res.data
    tableData.value = d.list ?? []
    pagination.total = d.total ?? 0
  } catch (err) {
    ElMessage.error('获取进货记录失败: ' + err.message)
    tableData.value = []
  } finally {
    loading.value = false
  }
}

function handleSearch() {
  pagination.page = 1
  fetchList()
}

// 补货入库
async function openRestockDialog() {
  restockVisible.value = true
  // 加载库存列表供选择
  try {
    const res = await inventoryApi.list({ page: 1, page_size: 200 })
    inventoryList.value = res.data?.list ?? []
  } catch {
    inventoryList.value = []
  }
}

function onRestockProductSelect(code) {
  const product = inventoryList.value.find(p => p.product_code === code)
  if (product) {
    restockCostPrice.value = product.unit_price ?? 0
  }
}

function addRestockItem() {
  if (!restockProduct.value || restockQty.value < 1) return
  const product = inventoryList.value.find(p => p.product_code === restockProduct.value)
  restockItems.value.push({
    product_code: restockProduct.value,
    product_name: product?.product_name ?? '',
    quantity: restockQty.value,
    unit_price: restockCostPrice.value
  })
  restockProduct.value = ''
  restockQty.value = 1
  restockCostPrice.value = 0
}

function resetRestock() {
  restockItems.value = []
  restockProduct.value = ''
  restockQty.value = 1
  restockCostPrice.value = 0
}

async function handleRestock() {
  if (restockItems.value.length === 0) {
    ElMessage.warning('请至少添加一件商品')
    return
  }
  restockLoading.value = true
  try {
    const payload = {
      items: restockItems.value.map(item => ({
        product_code: item.product_code,
        quantity: item.quantity,
        unit_price: item.unit_price
      }))
    }
    await purchaseApi.restock(payload)
    ElMessage.success('进货完成，库存已更新')
    restockVisible.value = false
    resetRestock()
    pagination.page = 1
    fetchList()
  } catch (err) {
    ElMessage.error('入库失败: ' + err.message)
  } finally {
    restockLoading.value = false
  }
}
</script>

<style scoped>
.purchase {
  width: 100%;
}

.page-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
}

.page-header h2 {
  font-size: 20px;
  font-weight: 600;
}

.table-card {
  background: #fff;
  border-radius: 8px;
  box-shadow: 0 1px 3px rgba(0,0,0,0.06);
  padding: 20px;
}
</style>
