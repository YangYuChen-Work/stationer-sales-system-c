<template>
  <span :class="['alert-badge', level]">
    <span v-if="level === 'danger'">⚠ 危险</span>
    <span v-else-if="level === 'warning'">⚡ 警告</span>
    <span v-else>✓ 正常</span>
    <span v-if="count !== undefined" class="count">({{ count }})</span>
  </span>
</template>

<script setup>
defineProps({
  level: {
    type: String,
    default: 'safe',
    validator: (value) => ['danger', 'warning', 'safe'].includes(value)
  },
  count: {
    type: Number,
    default: undefined
  }
})
</script>

<style scoped>
.alert-badge {
  display: inline-flex;
  align-items: center;
  gap: 4px;
  padding: 2px 10px;
  border-radius: 12px;
  font-size: 12px;
  font-weight: 500;
}

.alert-badge.danger {
  background: #fef2f2;
  color: #dc2626;
}

.alert-badge.warning {
  background: #fffbeb;
  color: #d97706;
}

.alert-badge.safe {
  background: #f0fdf4;
  color: #059669;
}

.count {
  font-weight: 400;
  opacity: 0.7;
}
</style>
