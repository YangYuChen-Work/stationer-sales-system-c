<template>
  <div class="sales">
    <div class="page-header">
      <h2>销售管理</h2>
      <el-button type="primary" @click="openCheckoutDialog">新增销售</el-button>
    </div>

    <el-tabs v-model="activeTab" @tab-change="onTabChange">
      <!-- 销售记录列表 -->
      <el-tab-pane label="销售记录" name="list">
        <!-- 筛选栏 -->
        <div class="table-card" style="margin-bottom: 16px;">
          <el-row :gutter="12" align="middle">
            <el-col :span="5">
              <el-input v-model="filters.keyword" placeholder="搜索商品名称或编号" clearable @keyup.enter="handleSearch" />
            </el-col>
            <el-col :span="3">
              <el-date-picker v-model="filters.dateRange" type="daterange" range-separator="至"
                start-placeholder="开始日期" end-placeholder="结束日期" format="YYYY-MM-DD" value-format="YYYY-MM-DD"
                style="width:100%" unlink-panels />
            </el-col>
            <el-col :span="3">
              <el-select v-model="filters.category" placeholder="选择类别" clearable style="width:100%">
                <el-option v-for="c in categoryOptions" :key="c" :label="c" :value="c" />
              </el-select>
            </el-col>
            <el-col :span="2">
              <el-button @click="toggleSortOrder">
                {{ sortOrder === 'desc' ? '降序 ↓' : '升序 ↑' }}
              </el-button>
            </el-col>
            <el-col :span="2">
              <el-button type="primary" @click="handleSearch" :loading="loading">搜索</el-button>
            </el-col>
          </el-row>
        </div>

        <!-- 表格 -->
        <div class="table-card">
          <el-table :data="tableData" v-loading="loading" style="width:100%" empty-text="暂无销售记录" stripe>
            <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
            <el-table-column prop="product_code" label="编号" width="100" />
            <el-table-column prop="category" label="类别" width="90" />
            <el-table-column prop="sale_date" label="日期" width="110" sortable />
            <el-table-column prop="quantity" label="数量" width="80" align="center" />
            <el-table-column label="售价" width="90" align="right">
              <template #default="{ row }">¥{{ row.sale_price?.toFixed(2) }}</template>
            </el-table-column>
            <el-table-column label="小计" width="100" align="right">
              <template #default="{ row }">¥{{ row.total_amount?.toFixed(2) }}</template>
            </el-table-column>
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
      </el-tab-pane>

      <!-- 收银结算 Tab（已整合到弹窗中） -->
      <el-tab-pane label="收银结算" name="checkout" />
    </el-tabs>

    <!-- 收银结算弹窗 -->
    <el-dialog v-model="checkoutVisible" title="收银结算" width="700px" :close-on-click-modal="false" @close="resetCheckout">
      <div v-if="checkoutItems.length === 0" style="text-align:center;padding:20px;color:#999;">
        请添加销售商品
      </div>
      <div v-else>
        <div style="margin-bottom:12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap;">
          <el-select v-model="checkoutProduct" filterable placeholder="搜索并选择商品" style="width:220px"
            @change="onCheckoutProductSelect">
            <el-option v-for="p in inventoryList" :key="p.product_code" :label="p.product_code + ' ' + p.product_name"
              :value="p.product_code" />
          </el-select>
          <el-input-number v-model="checkoutQty" :min="1" placeholder="数量" style="width:100px" />
          <el-input-number v-model="checkoutPrice" :min="0" :precision="2" :step="0.5" placeholder="售价" style="width:120px" />
          <el-button type="primary" @click="addCheckoutItem" :disabled="!checkoutProduct || checkoutQty < 1">添加</el-button>
        </div>
        <el-table :data="checkoutItems" size="small" style="margin-bottom:16px;">
          <el-table-column prop="product_code" label="编号" width="90" />
          <el-table-column prop="product_name" label="商品名称" min-width="130" show-overflow-tooltip />
          <el-table-column prop="quantity" label="数量" width="60" align="center" />
          <el-table-column label="售价" width="90" align="right">
            <template #default="{ row }">¥{{ row.sale_price?.toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="小计" width="100" align="right">
            <template #default="{ row }">¥{{ (row.quantity * row.sale_price).toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="操作" width="60" align="center">
            <template #default="{ $index }">
              <el-button type="danger" link size="small" @click="checkoutItems.splice($index, 1)">移除</el-button>
            </template>
          </el-table-column>
        </el-table>
        <div style="text-align:right;font-size:18px;font-weight:700;margin-bottom:12px;">
          合计: ¥{{ checkoutTotal.toFixed(2) }}
        </div>
      </div>

      <!-- 结算成功结果 -->
      <div v-if="checkoutResult" style="margin-top:16px;padding:12px;background:#f0f9ff;border-radius:8px;">
        <div style="font-weight:600;margin-bottom:8px;">结算成功</div>
        <el-table :data="checkoutResult.sale_records" size="small">
          <el-table-column prop="product_name" label="商品" />
          <el-table-column prop="quantity" label="数量" width="60" align="center" />
          <el-table-column label="金额" width="90" align="right">
            <template #default="{ row }">¥{{ row.total_amount?.toFixed(2) }}</template>
          </el-table-column>
        </el-table>
        <div style="text-align:right;font-weight:700;margin-top:8px;">
          总计: ¥{{ checkoutResult.total_amount?.toFixed(2) }}
        </div>
      </div>

      <template #footer>
        <el-button @click="checkoutVisible = false">关闭</el-button>
        <el-button type="primary" @click="handleCheckout" :loading="checkoutLoading" :disabled="checkoutItems.length === 0">
          结算
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { salesApi, inventoryApi } from '@/api/index.js'
import { ElMessage } from 'element-plus'

const categoryOptions = ['笔类', '本类', '尺类', '橡皮类', '文具盒类', '文件夹类', '工具类', '墨水类']

const activeTab = ref('list')
const loading = ref(false)
const tableData = ref([])
const sortOrder = ref('desc')

const filters = reactive({
  keyword: '',
  dateRange: null,
  category: ''
})

const pagination = reactive({
  page: 1,
  pageSize: 20,
  total: 0
})

// 收银结算
const checkoutVisible = ref(false)
const checkoutLoading = ref(false)
const checkoutItems = ref([])
const checkoutResult = ref(null)

const checkoutProduct = ref('')
const checkoutQty = ref(1)
const checkoutPrice = ref(0)
const inventoryList = ref([])

const checkoutTotal = computed(() => {
  return checkoutItems.value.reduce((sum, item) => sum + item.quantity * item.sale_price, 0)
})

onMounted(() => {
  fetchList()
})

function onTabChange(tab) {
  if (tab === 'checkout') {
    openCheckoutDialog()
    activeTab.value = 'list'
  }
}

async function fetchList() {
  loading.value = true
  try {
    const params = {
      page: pagination.page,
      page_size: pagination.pageSize,
      order: sortOrder.value
    }
    if (filters.keyword) params.keyword = filters.keyword
    if (filters.category) params.category = filters.category
    if (filters.dateRange && filters.dateRange.length === 2) {
      params.start_date = filters.dateRange[0]
      params.end_date = filters.dateRange[1]
    }
    const res = await salesApi.sorted(params)
    const d = res.data
    tableData.value = d.list ?? []
    pagination.total = d.total ?? 0
  } catch (err) {
    ElMessage.error('获取销售记录失败: ' + err.message)
    tableData.value = []
  } finally {
    loading.value = false
  }
}

function handleSearch() {
  pagination.page = 1
  fetchList()
}

function toggleSortOrder() {
  sortOrder.value = sortOrder.value === 'desc' ? 'asc' : 'desc'
  pagination.page = 1
  fetchList()
}

// 收银结算
async function openCheckoutDialog() {
  checkoutVisible.value = true
  checkoutResult.value = null
  // 加载库存列表供选择
  try {
    const res = await inventoryApi.list({ page: 1, page_size: 200 })
    inventoryList.value = res.data?.list ?? []
  } catch {
    inventoryList.value = []
  }
}

function onCheckoutProductSelect(code) {
  const product = inventoryList.value.find(p => p.product_code === code)
  if (product) {
    checkoutPrice.value = product.unit_price ?? 0
  }
}

function addCheckoutItem() {
  if (!checkoutProduct.value || checkoutQty.value < 1) return
  const product = inventoryList.value.find(p => p.product_code === checkoutProduct.value)
  checkoutItems.value.push({
    product_code: checkoutProduct.value,
    product_name: product?.product_name ?? '',
    quantity: checkoutQty.value,
    sale_price: checkoutPrice.value
  })
  checkoutProduct.value = ''
  checkoutQty.value = 1
  checkoutPrice.value = 0
}

function resetCheckout() {
  checkoutItems.value = []
  checkoutResult.value = null
  checkoutProduct.value = ''
  checkoutQty.value = 1
  checkoutPrice.value = 0
}

async function handleCheckout() {
  if (checkoutItems.value.length === 0) {
    ElMessage.warning('请至少添加一件商品')
    return
  }
  checkoutLoading.value = true
  checkoutResult.value = null
  try {
    const payload = {
      items: checkoutItems.value.map(item => ({
        product_code: item.product_code,
        quantity: item.quantity,
        sale_price: item.sale_price
      }))
    }
    const res = await salesApi.checkout(payload)
    checkoutResult.value = res.data
    ElMessage.success('结算成功')
    // 刷新列表
    pagination.page = 1
    fetchList()
  } catch (err) {
    if (err.message && err.message.includes('库存不足')) {
      ElMessage.error('库存不足: ' + err.message)
    } else {
      ElMessage.error('结算失败: ' + err.message)
    }
  } finally {
    checkoutLoading.value = false
  }
}
</script>

<style scoped>
.sales {
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
