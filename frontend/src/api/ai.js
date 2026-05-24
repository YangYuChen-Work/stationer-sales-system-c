import http from './index'

// AI 功能 API 封装 — 后续接入大模型时启用
export const aiApi = {
  ask:       (question) => http.post('/ai/ask', { question }),
  forecast:  (params)   => http.get('/ai/forecast', { params }),
  recommend: (params)   => http.get('/ai/recommend', { params }),
}
