import axios from 'axios'

const http = axios.create({
  baseURL: '/api',
  timeout: 10000,
  headers: { 'Content-Type': 'application/json' }
})

// Response interceptor
http.interceptors.response.use(
  res => res.data,
  err => {
    console.error('API Error:', err)
    return Promise.reject(err)
  }
)

// ========== 库存 API ==========
export const inventoryApi = {
  list:   (params) => http.get('/inventory', { params }),
  get:    (code)   => http.get(`/inventory/${code}`),
  create: (data)   => http.post('/inventory', data),
  update: (code, data) => http.put(`/inventory/${code}`, data),
  delete: (code)   => http.delete(`/inventory/${code}`),
}

// ========== 销售 API ==========
export const salesApi = {
  list:     (params) => http.get('/sales', { params }),
  sorted:   (params) => http.get('/sales/sorted', { params }),
  create:   (data)   => http.post('/sales', data),
  checkout: (data)   => http.post('/sales/checkout', data),
}

// ========== 进货 API ==========
export const purchaseApi = {
  list:    (params) => http.get('/purchases', { params }),
  create:  (data)   => http.post('/purchases', data),
  restock: (data)   => http.post('/purchases/restock', data),
}

// ========== 搜索 API ==========
export const searchApi = {
  query: (params) => http.get('/search', { params }),
}

// ========== 统计 API ==========
export const statsApi = {
  overview: ()     => http.get('/stats/overview'),
  eraser:   ()     => http.get('/stats/eraser'),
}

// ========== 预警 + 建议 API ==========
export const alertApi = {
  list: (params) => http.get('/alerts', { params }),
}

export const suggestApi = {
  list: (params) => http.get('/suggestions', { params }),
}

// ========== 热力图 API ==========
export const heatmapApi = {
  category: (params) => http.get('/heatmap/category', { params }),
  sales:    (params) => http.get('/heatmap/sales', { params }),
}

export default http
