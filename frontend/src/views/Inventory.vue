<template>
  <div class="inventory">
    <div class="page-header">
      <h2>库存管理</h2>
      <el-button type="primary" @click="openAddDialog">新增商品</el-button>
    </div>

    <!-- 搜索栏 -->
    <div class="table-card" style="margin-bottom: 16px;">
      <el-row :gutter="12" align="middle">
        <el-col :span="5">
          <el-input
            v-model="filters.keyword"
            placeholder="搜索商品名称或编号"
            clearable
            @keyup.enter="handleSearch"
          />
        </el-col>
        <el-col :span="4">
          <el-select v-model="filters.category" placeholder="选择类别" clearable style="width:100%">
            <el-option v-for="c in categoryOptions" :key="c" :label="c" :value="c" />
          </el-select>
        </el-col>
        <el-col :span="4">
          <el-input
            v-model="filters.productCode"
            placeholder="按编号精确查询"
            clearable
            @keyup.enter="handleSearch"
          />
        </el-col>
        <el-col :span="3">
          <el-button type="primary" @click="handleSearch" :loading="loading">搜索</el-button>
        </el-col>
      </el-row>
    </div>

    <!-- 表格 -->
    <div class="table-card">
      <el-table
        :data="tableData"
        v-loading="loading"
        style="width: 100%"
        empty-text="暂无数据"
        stripe
      >
        <el-table-column prop="product_code" label="编号" width="100" />
        <el-table-column prop="product_name" label="商品名称" min-width="150" show-overflow-tooltip />
        <el-table-column prop="category" label="类别" width="90" />
        <el-table-column prop="manufacturer" label="生产厂家" min-width="120" show-overflow-tooltip />
        <el-table-column prop="model" label="型号" width="100" show-overflow-tooltip />
        <el-table-column prop="stock_quantity" label="库存数量" width="90" align="center">
          <template #default="{ row }">
            <span :style="{ color: row.stock_quantity <= row.warning_stock ? '#dc2626' : row.stock_quantity <= row.safety_stock ? '#d97706' : '' }">
              {{ row.stock_quantity }}
            </span>
          </template>
        </el-table-column>
        <el-table-column label="单价" width="90" align="right">
          <template #default="{ row }">¥{{ row.unit_price?.toFixed(2) }}</template>
        </el-table-column>
        <el-table-column prop="safety_stock" label="安全库存" width="80" align="center" />
        <el-table-column label="操作" width="140" align="center" fixed="right">
          <template #default="{ row }">
            <el-button type="primary" link size="small" @click="openEditDialog(row)">编辑</el-button>
            <el-button type="danger" link size="small" @click="handleDelete(row)">删除</el-button>
          </template>
        </el-table-column>
      </el-table>

      <!-- 分页 -->
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

    <!-- 新增/编辑弹窗 -->
    <el-dialog
      v-model="dialogVisible"
      :title="isEdit ? '编辑商品' : '新增商品'"
      width="560px"
      :close-on-click-modal="false"
      @close="resetForm"
    >
      <el-form ref="formRef" :model="form" :rules="formRules" label-width="90px" label-position="right">
        <el-row :gutter="16">
          <el-col :span="12">
            <el-form-item label="商品编号" prop="product_code">
              <el-input v-model="form.product_code" :disabled="isEdit" placeholder="如 P031" />
            </el-form-item>
          </el-col>
          <el-col :span="12">
            <el-form-item label="商品名称" prop="product_name">
              <el-input v-model="form.product_name" placeholder="商品名称" />
            </el-form-item>
          </el-col>
        </el-row>
        <el-row :gutter="16">
          <el-col :span="12">
            <el-form-item label="类别" prop="category">
              <el-select v-model="form.category" placeholder="选择类别" style="width:100%">
                <el-option v-for="c in categoryOptions" :key="c" :label="c" :value="c" />
              </el-select>
            </el-form-item>
          </el-col>
          <el-col :span="12">
            <el-form-item label="生产厂家" prop="manufacturer">
              <el-input v-model="form.manufacturer" placeholder="生产厂家" />
            </el-form-item>
          </el-col>
        </el-row>
        <el-row :gutter="16">
          <el-col :span="12">
            <el-form-item label="型号" prop="model">
              <el-input v-model="form.model" placeholder="型号" />
            </el-form-item>
          </el-col>
          <el-col :span="12">
            <el-form-item label="库存数量" prop="stock_quantity">
              <el-input-number v-model="form.stock_quantity" :min="0" style="width:100%" />
            </el-form-item>
          </el-col>
        </el-row>
        <el-row :gutter="16">
          <el-col :span="12">
            <el-form-item label="单价" prop="unit_price">
              <el-input-number v-model="form.unit_price" :min="0" :precision="2" :step="0.5" style="width:100%" />
            </el-form-item>
          </el-col>
        </el-row>
        <el-row :gutter="16">
          <el-col :span="12">
            <el-form-item label="安全库存">
              <el-input-number v-model="form.safety_stock" :min="0" style="width:100%" />
            </el-form-item>
          </el-col>
          <el-col :span="12">
            <el-form-item label="预警库存">
              <el-input-number v-model="form.warning_stock" :min="0" style="width:100%" />
            </el-form-item>
          </el-col>
        </el-row>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" @click="handleSave" :loading="saveLoading">
          {{ isEdit ? '保存修改' : '确认新增' }}
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { inventoryApi } from '@/api/index.js'
import { ElMessage, ElMessageBox } from 'element-plus'

const categoryOptions = ['笔类', '本类', '尺类', '橡皮类', '文具盒类', '文件夹类', '工具类', '墨水类']

const loading = ref(false)
const tableData = ref([])

const filters = reactive({
  keyword: '',
  category: '',
  productCode: ''
})

const pagination = reactive({
  page: 1,
  pageSize: 20,
  total: 0
})

// 弹窗
const dialogVisible = ref(false)
const isEdit = ref(false)
const formRef = ref(null)
const saveLoading = ref(false)
const editProductCode = ref('')

const form = reactive({
  product_code: '',
  product_name: '',
  category: '',
  manufacturer: '',
  model: '',
  stock_quantity: 0,
  unit_price: 0,
  safety_stock: 30,
  warning_stock: 20
})

const formRules = {
  product_code: [
    { required: true, message: '请输入商品编号', trigger: 'blur' }
  ],
  product_name: [
    { required: true, message: '请输入商品名称', trigger: 'blur' }
  ],
  category: [
    { required: true, message: '请选择类别', trigger: 'change' }
  ],
  manufacturer: [
    { required: true, message: '请输入生产厂家', trigger: 'blur' }
  ],
  model: [
    { required: true, message: '请输入型号', trigger: 'blur' }
  ],
  stock_quantity: [
    { required: true, message: '请输入库存数量', trigger: 'blur' }
  ],
  unit_price: [
    { required: true, message: '请输入单价', trigger: 'blur' }
  ]
}

onMounted(() => {
  fetchList()
})

async function fetchList() {
  loading.value = true
  try {
    const params = { page: pagination.page, page_size: pagination.pageSize }
    if (filters.keyword) params.keyword = filters.keyword
    if (filters.category) params.category = filters.category
    if (filters.productCode) {
      // 按编号精确查询 — 直接用 get 接口
      try {
        const res = await inventoryApi.get(filters.productCode)
        tableData.value = [res.data]
        pagination.total = 1
        loading.value = false
        return
      } catch (err) {
        // 如果编号不存在，表格为空
        tableData.value = []
        pagination.total = 0
        loading.value = false
        return
      }
    }
    const res = await inventoryApi.list(params)
    const d = res.data
    tableData.value = d.list ?? []
    pagination.total = d.total ?? 0
  } catch (err) {
    ElMessage.error('获取库存列表失败: ' + err.message)
    tableData.value = []
  } finally {
    loading.value = false
  }
}

function handleSearch() {
  pagination.page = 1
  fetchList()
}

// 新增
function openAddDialog() {
  isEdit.value = false
  editProductCode.value = ''
  resetFormData()
  dialogVisible.value = true
}

// 编辑
function openEditDialog(row) {
  isEdit.value = true
  editProductCode.value = row.product_code
  form.product_code = row.product_code
  form.product_name = row.product_name
  form.category = row.category
  form.manufacturer = row.manufacturer
  form.model = row.model
  form.stock_quantity = row.stock_quantity
  form.unit_price = row.unit_price
  form.safety_stock = row.safety_stock
  form.warning_stock = row.warning_stock
  dialogVisible.value = true
}

function resetFormData() {
  form.product_code = ''
  form.product_name = ''
  form.category = ''
  form.manufacturer = ''
  form.model = ''
  form.stock_quantity = 0
  form.unit_price = 0
  form.safety_stock = 30
  form.warning_stock = 20
  formRef.value?.resetFields()
}

function resetForm() {
  resetFormData()
  formRef.value?.clearValidate()
}

async function handleSave() {
  const valid = await formRef.value.validate().catch(() => false)
  if (!valid) return

  saveLoading.value = true
  try {
    const payload = {
      product_code: form.product_code,
      product_name: form.product_name,
      category: form.category,
      manufacturer: form.manufacturer,
      model: form.model,
      stock_quantity: form.stock_quantity,
      unit_price: form.unit_price,
      safety_stock: form.safety_stock,
      warning_stock: form.warning_stock
    }

    if (isEdit.value) {
      await inventoryApi.update(editProductCode.value, payload)
      ElMessage.success('商品信息更新成功')
    } else {
      await inventoryApi.create(payload)
      ElMessage.success('新增商品成功')
    }
    dialogVisible.value = false
    fetchList()
  } catch (err) {
    ElMessage.error((isEdit.value ? '更新' : '新增') + '失败: ' + err.message)
  } finally {
    saveLoading.value = false
  }
}

// 删除
async function handleDelete(row) {
  try {
    await ElMessageBox.confirm(
      `确定要删除商品「${row.product_name}」(${row.product_code}) 吗？删除后无法恢复。`,
      '删除确认',
      { confirmButtonText: '确定', cancelButtonText: '取消', type: 'warning' }
    )
  } catch {
    return
  }

  try {
    await inventoryApi.delete(row.product_code)
    ElMessage.success('商品删除成功')
    fetchList()
  } catch (err) {
    ElMessage.error('删除失败: ' + err.message)
  }
}
</script>

<style scoped>
.inventory {
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
