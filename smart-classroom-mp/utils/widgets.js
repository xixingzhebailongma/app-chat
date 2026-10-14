/**
 * 我的教室 · 可自定义卡片（widget）配置
 * 卡片功能来源 = 设备控制页的模块（场景控制 / 开关设备 / 环境面板 / 传感器 / 今日进出）
 * 用户选择持久化在本地 storage，刷新后保留。
 */
const KEY = 'sc_classroom_widgets'

/* 所有可添加卡片的元信息 */
export const WIDGETS = [
  { type: 'scene',   title: '场景控制', icon: 'play',     desc: '一键上课 / 离开' },
  { type: 'switch',  title: '开关设备', icon: 'control',  desc: '灯、空调、门等' },
  { type: 'panel',   title: '环境面板', icon: 'screen',   desc: '融合屏运行指标' },
  { type: 'sensor',  title: '传感器',   icon: 'sensor',   desc: '温湿度 / 空气质量' },
  { type: 'access',  title: '今日进出', icon: 'door-open', desc: '人脸 / 刷卡记录' },
]

export const WIDGET_MAP = WIDGETS.reduce((m, w) => { m[w.type] = w; return m }, {})

/* 默认卡片：场景控制 + 传感器 + 今日进出（开关/面板按需添加） */
export const DEFAULT_WIDGETS = ['scene', 'sensor', 'access']

export function loadWidgets() {
  try {
    const saved = uni.getStorageSync(KEY)
    if (Array.isArray(saved) && saved.length && saved.every((t) => WIDGET_MAP[t])) {
      return saved.slice()
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
