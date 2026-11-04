/**
 * 我的教室 · 可自定义卡片（widget）配置
 * 卡片功能来源 = 设备控制页的模块（场景控制 / 开关设备 / 环境面板 / 传感器）
 * 用户选择持久化在本地 storage，刷新后保留。
 */
const KEY = 'sc_classroom_widgets'

/* 所有可添加卡片的元信息 */
export const WIDGETS = [
  { type: 'scene',   title: '场景控制', icon: 'flag',     desc: '一键上课 / 离开' },
  { type: 'switch',  title: '开关设备', icon: 'flag',     desc: '灯、空调、门等' },
  { type: 'panel',   title: '环境面板', icon: 'flag',     desc: '融合屏运行指标' },
  { type: 'sensor',  title: '传感器',   icon: 'flag',     desc: '温湿度 / 空气质量' },
]

export const WIDGET_MAP = WIDGETS.reduce((m, w) => { m[w.type] = w; return m }, {})

/* 默认卡片：场景控制 + 传感器（开关/面板按需添加） */
export const DEFAULT_WIDGETS = ['scene', 'sensor']

export function loadWidgets() {
  try {
    const saved = uni.getStorageSync(KEY)
    if (Array.isArray(saved) && saved.length) {
      // 过滤掉已下线或未知的卡片类型（如历史上的 access），保留其余用户选择
      const valid = saved.filter((t) => WIDGET_MAP[t])
      if (valid.length) return valid
    }
  } catch (e) { /* ignore */ }
  return DEFAULT_WIDGETS.slice()
}

export function saveWidgets(list) {
  try { uni.setStorageSync(KEY, list.slice()) } catch (e) { /* ignore */ }
}

export function resetWidgets() {
  try { uni.removeStorageSync(KEY) } catch (e) { /* ignore */ }
  return DEFAULT_WIDGETS.slice()
}
