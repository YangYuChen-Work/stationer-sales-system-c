<template>
  <div class="heatmap-container">
    <div v-if="loading" class="loading">加载中...</div>
    <div v-else class="charts-grid">
      <div class="chart-card">
        <h4>品类销售占比</h4>
        <div ref="pieChartRef" style="height: 300px;"></div>
      </div>
      <div class="chart-card">
        <h4>每日销售趋势</h4>
        <div ref="barChartRef" style="height: 300px;"></div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted } from 'vue'
import * as echarts from 'echarts'

const props = defineProps({
  categoryData: {
    type: Array,
    default: () => []
  },
  salesData: {
    type: Array,
    default: () => []
  },
  loading: {
    type: Boolean,
    default: false
  }
})

const pieChartRef = ref(null)
const barChartRef = ref(null)
let pieChart = null
let barChart = null

function initPieChart() {
  if (!pieChartRef.value || !props.categoryData || props.categoryData.length === 0) return
  if (!pieChart) {
    pieChart = echarts.init(pieChartRef.value)
  }
  const data = props.categoryData.map(item => ({
    name: item.category,
    value: item.total_sales
  }))
  pieChart.setOption({
    tooltip: {
      trigger: 'item',
      formatter: '{b}: ¥{c} ({d}%)'
    },
    series: [{
      type: 'pie',
      radius: ['45%', '75%'],
      center: ['50%', '50%'],
      data,
      emphasis: {
        itemStyle: {
          shadowBlur: 10,
          shadowColor: 'rgba(0,0,0,0.2)'
        }
      },
      itemStyle: {
        borderRadius: 4,
        borderColor: '#fff',
        borderWidth: 2
      },
      label: {
        fontSize: 11
      }
    }],
    color: ['#2563eb', '#3b82f6', '#60a5fa', '#93c5fd', '#bfdbfe', '#dbeafe', '#eff6ff', '#f0f5ff']
  })
}

function initBarChart() {
  if (!barChartRef.value || !props.salesData || props.salesData.length === 0) return
  if (!barChart) {
    barChart = echarts.init(barChartRef.value)
  }
  const dates = props.salesData.map(d => d.date ? d.date.slice(5) : '')
  const amounts = props.salesData.map(d => d.total_amount)
  barChart.setOption({
    tooltip: {
      trigger: 'axis',
      formatter: function(params) {
        return params[0].name + '<br/>销售额: ¥' + params[0].value
      }
    },
    xAxis: {
      type: 'category',
      data: dates,
      axisLabel: {
        fontSize: 10,
        rotate: 30
      }
    },
    yAxis: {
      type: 'value',
      axisLabel: {
        formatter: '¥{value}'
      }
    },
    series: [{
      type: 'bar',
      data: amounts,
      itemStyle: {
        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
          { offset: 0, color: '#2563eb' },
          { offset: 1, color: '#93c5fd' }
        ]),
        borderRadius: [4, 4, 0, 0]
      },
      barMaxWidth: 40
    }],
    grid: {
      top: 10,
      right: 10,
      bottom: 30,
      left: 50
    }
  })
}

function handleResize() {
  pieChart?.resize()
  barChart?.resize()
}

watch(
  () => [props.categoryData, props.salesData, props.loading],
  () => {
    if (!props.loading) {
      initPieChart()
      initBarChart()
    }
  },
  { deep: true }
)

onMounted(() => {
  if (!props.loading) {
    initPieChart()
    initBarChart()
  }
  window.addEventListener('resize', handleResize)
})

onUnmounted(() => {
  window.removeEventListener('resize', handleResize)
  pieChart?.dispose()
  barChart?.dispose()
  pieChart = null
  barChart = null
})
</script>

<style scoped>
.heatmap-container {
  margin-top: 16px;
}

.charts-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

.chart-card {
  background: #fff;
  border-radius: 8px;
  padding: 16px;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.06);
}

.chart-card h4 {
  font-size: 14px;
  color: #64748b;
  margin: 0 0 12px 0;
  font-weight: 600;
}

.loading {
  text-align: center;
  padding: 60px;
  color: #94a3b8;
}

@media (max-width: 768px) {
  .charts-grid {
    grid-template-columns: 1fr;
  }
}
</style>
