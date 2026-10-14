/**
 * 主题色 JS 侧单一数据源
 * 与 uni.scss 的设计 Token 成对维护：模板里 app-icon 的 color、
 * uni API 的 confirmColor 等无法直接引用 SCSS 变量的位置，一律从这里取。
 * 改色只改两处：uni.scss（样式）+ 本文件（JS/模板）。
 */

export const COLORS = {
  /* 品牌 */
  primary: '#007AFF',
  primaryDark: '#0056B3',
  primaryLight: '#E5F1FF',

  /* 语义 */
  success: '#34C759',
  warning: '#FF9500',
  danger: '#FF3B30',
  disabled: '#8E8E93',

  /* 中性 */
  page: '#F2F2F7',
  card: '#FFFFFF',
  ink: '#1C1C1E',
  text1: '#000000',
  text2: '#3A3A3C',
  text3: '#8E8E93',
  text4: '#C7C7CC',
  line: '#E5E5EA',

  white: '#FFFFFF',
}

/** 设备分类点缀色（小图标/小圆点专用，禁止大面积铺背景） */
export const CAT_COLORS = {
  light: '#FFCC00',
  climate: '#34C759',
  security: '#FF3B30',
  media: '#AF52DE',
  plug: '#007AFF',
  sensor: '#5856D6',
  other: '#8E8E93',
}

/** 设备分类浅底（图标底块专用） */
export const CAT_COLORS_SOFT = {
  light: '#FFF8D6',
  climate: '#E9F9EE',
  security: '#FFEDEC',
  media: '#F4ECFD',
  plug: '#E5F1FF',
  sensor: '#ECECFB',
  other: '#F2F2F7',
}
