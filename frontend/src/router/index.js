import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    component: () => import('@/components/AppLayout.vue'),
    redirect: '/dashboard',
    children: [
      { path: 'dashboard', name: 'Dashboard', component: () => import('@/views/Dashboard.vue'), meta: { title: '首页看板' } },
      { path: 'inventory', name: 'Inventory', component: () => import('@/views/Inventory.vue'), meta: { title: '库存管理' } },
      { path: 'sales', name: 'Sales', component: () => import('@/views/Sales.vue'), meta: { title: '销售管理' } },
      { path: 'purchase', name: 'Purchase', component: () => import('@/views/Purchase.vue'), meta: { title: '进货管理' } },
      { path: 'search', name: 'Search', component: () => import('@/views/Search.vue'), meta: { title: '高级搜索' } },
    ]
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
