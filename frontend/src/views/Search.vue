<template>
  <div class="search">
    <div class="page-header">
      <h2>高级搜索</h2>
    </div>

    <!-- 搜索类型选择 -->
    <div class="table-card" style="margin-bottom: 16px;">
      <div style="margin-bottom: 12px; font-weight: 600;">搜索类型</div>
      <el-radio-group v-model="searchType" @change="onTypeChange">
        <el-radio-button value="inventory">库存</el-radio-button>
        <el-radio-button value="sales">销售</el-radio-button>
        <el-radio-button value="purchases">进货</el-radio-button>
      </el-radio-group>
    </div>

    <!-- 筛选条件 -->
    <div class="table-card" style="margin-bottom: 16px;">
      <div style="margin-bottom: 12px; font-weight: 600;">筛选条件</div>
      <el-form label-width="90px" label-position="right">
        <el-row :gutter="16">
          <el-col :span="8">
            <el-form-item label="关键词">
              <el-input v-model="form.keyword" placeholder="按名称模糊搜索" clearable />
            </el-form-item>
          </el-col>
          <el-col :span="8">
            <el-form-item label="商品编号">
              <el-input v-model="form.product_code" placeholder="按编号精确搜索" clearable />
            </el-form-item>
          </el-col>
          <el-col :span="8">
            <el-form-item label="类别">
              <el-select v-model="form.category" placeholder="选择类别" clearable style="width:100%">
                <el-option v-for="c in categoryOptions" :key="c" :label="c" :value="c" />
              </el-select>
            </el-form-item>
          </el-col>
        </el-row>

        <!-- 库存类型特有：生产厂家 -->
        <el-row :gutter="16" v-if="searchType === 'inventory'">
          <el-col :span="8">
            <el-form-item label="生产厂家">
              <el-input v-model="form.manufacturer" placeholder="按厂家筛选" clearable />
            </el-form-item>
          </el-col>
        </el-row>

        <!-- 销售/进货类型特有：日期范围 -->
        <el-row :gutter="16" v-if="searchType === 'sales' || searchType === 'purchases'">
          <el-col :span="12">
            <el-form-item label="日期范围">
              <el-date-picker v-model="form.dateRange" type="daterange" range-separator="至"
                start-placeholder="开始日期" end-placeholder="结束日期" format="YYYY-MM-DD" value-format="YYYY-MM-DD"
                style="width:100%" unlink-panels />
            </el-form-item>
          </el-col>
        </el-row>

        <el-row :gutter="16">
          <el-col :span="8">
            <el-form-item label="价格区间">
              <el-input-number v-model="form.min_price" :min="0" :precision="2" placeholder="最低" style="width:calc(50% - 5px)" />
              <span style="padding:0 4px;">-</span>
              <el-input-number v-model="form.max_price" :min="0" :precision="2" placeholder="最高" style="width:calc(50% - 5px)" />
            </el-form-item>
          </el-col>
          <el-col :span="8">
            <el-form-item label="数量区间">
              <el-input-number v-model="form.min_quantity" :min="0" placeholder="最低" style="width:calc(50% - 5px)" />
              <span style="padding:0 4px;">-</span>
              <el-input-number v-model="form.max_quantity" :min="0" placeholder="最高" style="width:calc(50% - 5px)" />
            </el-form-item>
          </el-col>
        </el-row>

        <el-row>
          <el-col :span="24" style="text-align:right;">
            <el-button @click="handleReset">重置</el-button>
            <el-button type="primary" @click="handleSearch" :loading="loading">搜索</el-button>
          </el-col>
        </el-row>
      </el-form>
    </div>

    <!-- 已应用条件 -->
    <div v-if="appliedConditionsText.length > 0" class="table-card" style="margin-bottom: 16px;">
      <div style="font-weight:600;margin-bottom:8px;">已应用条件</div>
      <div style="display:flex;flex-wrap:wrap;gap:6px;">
        <el-tag v-for="(cond, index) in appliedConditionsText" :key="index" size="small" type="info">
          {{ cond }}
        </el-tag>
      </div>
    </div>

    <!-- 搜索结果 -->
    <div class="table-card">
      <div style="font-weight:600;margin-bottom:12px;">
        搜索结果
        <span v-if="!loading && !searched" style="color:#999;font-weight:400;font-size:13px;">
          — 请输入条件并点击搜索
        </span>
      </div>

      <el-table :data="tableData" v-loading="loading" style="width:100%" empty-text="暂无数据" stripe>
        <!-- 库存字段 -->
        <template v-if="searchType === 'inventory'">
          <el-table-column prop="product_code" label="编号" width="100" />
          <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
          <el-table-column prop="category" label="类别" width="90" />
          <el-table-column prop="manufacturer" label="厂家" min-width="110" show-overflow-tooltip />
          <el-table-column prop="model" label="型号" width="90" />
          <el-table-column prop="stock_quantity" label="库存" width="70" align="center" />
          <el-table-column label="单价" width="80" align="right">
            <template #default="{ row }">¥{{ row.unit_price?.toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="更新时间" width="130">
            <template #default="{ row }">{{ row.updated_at?.replace('T', ' ').substring(0, 19) }}</template>
          </el-table-column>
        </template>

        <!-- 销售字段 -->
        <template v-else-if="searchType === 'sales'">
          <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
          <el-table-column prop="product_code" label="编号" width="100" />
          <el-table-column prop="category" label="类别" width="90" />
          <el-table-column prop="sale_date" label="日期" width="110" />
          <el-table-column prop="quantity" label="数量" width="70" align="center" />
          <el-table-column label="售价" width="80" align="right">
            <template #default="{ row }">¥{{ row.sale_price?.toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="小计" width="100" align="right">
            <template #default="{ row }">¥{{ row.total_amount?.toFixed(2) }}</template>
          </el-table-column>
        </template>

        <!-- 进货字段 -->
        <template v-else-if="searchType === 'purchases'">
          <el-table-column prop="product_name" label="商品名称" min-width="140" show-overflow-tooltip />
          <el-table-column prop="product_code" label="编号" width="100" />
          <el-table-column prop="category" label="类别" width="90" />
          <el-table-column prop="purchase_date" label="日期" width="110" />
          <el-table-column prop="quantity" label="数量" width="70" align="center" />
          <el-table-column label="进价" width="80" align="right">
            <template #default="{ row }">¥{{ row.unit_price?.toFixed(2) }}</template>
          </el-table-column>
          <el-table-column label="小计" width="100" align="right">
            <template #default="{ row }">¥{{ row.total_cost?.toFixed(2) }}</template>
          </el-table-column>
        </template>
      </el-table>

      <div style="display:flex;justify-content:flex-end;margin-top:16px;">
        <el-pagination
          v-model:current-page="pagination.page"
          v-model:page-size="pagination.pageSize"
          :total="pagination.total"
          :page-sizes="[10, 20, 50, 100]"
          layout="total, sizes, prev, pager, next, jumper"
          @size-change="doSearch"
          @current-change="doSearch"
        />
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { searchApi } from '@/api/index.js'
import { ElMessage } from 'element-plus'

const categoryOptions = ['笔类', '本类', '尺类', '橡皮类', '文具盒类', '文件夹类', '工具类', '墨水类']

const searchType = ref('inventory')
const loading = ref(false)
const searched = ref(false)
const tableData = ref([])
const appliedConditions = reactive({})

const form = reactive({
  keyword: '',
  product_code: '',
  category: '',
  manufacturer: '',
  dateRange: null,
  min_price: null,
  max_price: null,
  min_quantity: null,
  max_quantity: null
})

const pagination = reactive({
  page: 1,
  pageSize: 20,
  total: 0
})

const appliedConditionsText = computed(() => {
  const texts = []
  const map = appliedConditions
  if (!map || Object.keys(map).length === 0) return texts
  const labelMap = {
    type: '类型',
    keyword: '关键词',
    product_code: '编号',
    category: '类别',
    manufacturer: '厂家',
    start_date: '开始日期',
    end_date: '结束日期',
    min_price: '最低价格',
    max_price: '最高价格',
    min_quantity: '最低数量',
    max_quantity: '最高数量'
  }
  for (const [key, val] of Object.entries(map)) {
    if (val === null || val === undefined || val === '') continue
    const label = labelMap[key] || key
    if (key === 'type') {
      const typeMap = { inventory: '库存', sales: '销售', purchases: '进货' }
      texts.push(label + ': ' + (typeMap[val] || val))
    } else {
      texts.push(label + ': ' + val)
    }
  }
  return texts
})

onMounted(() => {
  // 不做初始搜索，等待用户点击搜索
})

function onTypeChange() {
  form.product_code = ''
  form.category = ''
  form.manufacturer = ''
  form.dateRange = null
  form.min_price = null
  form.max_price = null
  form.min_quantity = null
  form.max_quantity = null
  tableData.value = []
  pagination.total = 0
  searched.value = false
  Object.keys(appliedConditions).forEach(k => delete appliedConditions[k])
}

function handleSearch() {
  pagination.page = 1
  doSearch()
}

async function doSearch() {
  loading.value = true
  try {
    const params = {
      type: searchType.value,
      page: pagination.page,
      page_size: pagination.pageSize
    }
    if (form.keyword) params.keyword = form.keyword
    if (form.product_code) params.product_code = form.product_code
    if (form.category) params.category = form.category
    if (form.manufacturer && searchType.value === 'inventory') params.manufacturer = form.manufacturer
    if (form.dateRange && form.dateRange.length === 2) {
      params.start_date = form.dateRange[0]
      params.end_date = form.dateRange[1]
    }
    if (form.min_price !== null && form.min_price !== undefined) params.min_price = form.min_price
    if (form.max_price !== null && form.max_price !== undefined) params.max_price = form.max_price
    if (form.min_quantity !== null && form.min_quantity !== undefined) params.min_quantity = form.min_quantity
    if (form.max_quantity !== null && form.max_quantity !== undefined) params.max_quantity = form.max_quantity

    const res = await searchApi.query(params)
    const d = res.data
    tableData.value = d.list ?? []
    pagination.total = d.total ?? 0
    searched.value = true

    // 更新已应用条件
    Object.keys(appliedConditions).forEach(k => delete appliedConditions[k])
    if (d.conditions) {
      Object.assign(appliedConditions, d.conditions)
    }
  } catch (err) {
    ElMessage.error('搜索失败: ' + err.message)
    tableData.value = []
    pagination.total = 0
  } finally {
    loading.value = false
  }
}

function handleReset() {
  form.keyword = ''
  form.product_code = ''
  form.category = ''
  form.manufacturer = ''
  form.dateRange = null
  form.min_price = null
  form.max_price = null
  form.min_quantity = null
  form.max_quantity = null
  tableData.value = []
  pagination.page = 1
  pagination.total = 0
  searched.value = false
  Object.keys(appliedConditions).forEach(k => delete appliedConditions[k])
}
</script>

<style scoped>
.search {
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
