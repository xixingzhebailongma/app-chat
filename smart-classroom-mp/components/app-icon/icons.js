/**
 * 本地单色线性图标库（24×24 viewBox，stroke 1.8，圆头线帽）
 * 规则：
 * - 统一通过 <app-icon> 以 data-URI 内联渲染，不依赖网络图片、不用 emoji；
 * - 颜色占位符 {{C}} 由组件按 props.color 替换；
 * - tab 图标提供 -fill 填充版本，选中态在线性/填充间切换。
 */
export const ICONS = {
  /* ---- 通用 ---- */
  back: "<path d='M15 5 8 12l7 7'/>",
  close: "<path d='M6 6l12 12M18 6 6 18'/>",
  check: "<path d='m5 12.5 4.5 4.5L19 7.5'/>",
  'chevron-right': "<path d='m9 6 6 6-6 6'/>",
  'arrow-right': "<path d='M4 12h15m0 0-5.5-5.5M19 12l-5.5 5.5'/>",
  refresh: "<path d='M21 12a9 9 0 1 1-2.64-6.36'/><path d='M21 3v6h-6'/>",
  plus: "<path d='M12 5v14M5 12h14'/>",
  search: "<circle cx='11' cy='11' r='7'/><path d='m20 20-3.5-3.5'/>",
  filter: "<path d='M4 5h16l-6.2 7.4V18l-3.6 1.8v-7.4Z'/>",
  edit: "<path d='M4 20h4L19.5 8.5a2.1 2.1 0 0 0-3-3L5 17v3Z'/><path d='M14.5 7.5l3 3'/>",
  send: "<path d='M21 3 10.5 13.5'/><path d='M21 3 14 21l-3.5-7.5L3 10Z'/>",
  clock: "<circle cx='12' cy='12' r='8.5'/><path d='M12 7.5V12l3 2'/>",
  warning: "<path d='M12 4 2.8 19.5a1 1 0 0 0 .86 1.5h16.68a1 1 0 0 0 .86-1.5Z'/><path d='M12 10v4.5'/><path d='M12 17.6h.01'/>",
  info: "<circle cx='12' cy='12' r='9'/><path d='M12 11v5'/><path d='M12 7.8h.01'/>",
  logout: "<path d='M14 4H7a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h7'/><path d='M17 8.5 20.5 12 17 15.5'/><path d='M20 12H10'/>",
  lock: "<rect x='5.5' y='10.5' width='13' height='9.5' rx='2.5'/><path d='M8.5 10.5V8a3.5 3.5 0 0 1 7 0v2.5'/><circle cx='12' cy='15.2' r='1.4' fill='{{C}}' stroke='none'/>",
  'door-open': "<path d='M6 20.5V4.2L15 3v17.5'/><path d='M5 20.5h15'/><path d='M17.5 11.5h.01'/>",
  room: "<path d='M6 20.5V4.2L18 3v17.5'/><path d='M5 20.5h14'/><circle cx='14.2' cy='12.2' r='1' fill='{{C}}' stroke='none'/>",

  /* ---- tab：线性 ---- */
  home: "<path d='M4 10.8 12 4l8 6.8'/><path d='M6.2 9.6V19a1 1 0 0 0 1 1h9.6a1 1 0 0 0 1-1V9.6'/><path d='M10 20v-5h4v5'/>",
  control: "<path d='M4 7h9M19 7h1'/><circle cx='15' cy='7' r='2.2'/><path d='M4 17h3.5M12.5 17H20'/><circle cx='9.5' cy='17' r='2.2'/>",
  bell: "<path d='M6 9.5a6 6 0 1 1 12 0c0 4.2 1.7 5.6 1.7 5.6H4.3S6 13.7 6 9.5Z'/><path d='M10 18.5a2 2 0 0 0 4 0'/>",
  grid: "<rect x='4' y='4' width='7' height='7' rx='2'/><rect x='13' y='4' width='7' height='7' rx='2'/><rect x='4' y='13' width='7' height='7' rx='2'/><rect x='13' y='13' width='7' height='7' rx='2'/>",
  user: "<circle cx='12' cy='8.5' r='3.6'/><path d='M5 20a7 7 0 0 1 14 0'/>",

  /* ---- tab：填充（选中态） ---- */
  'home-fill': "<path d='M11.35 3.75a1 1 0 0 1 1.3 0l8 6.6a1 1 0 0 1-1.28 1.54l-.37-.3V19A1.5 1.5 0 0 1 17.5 20.5h-11A1.5 1.5 0 0 1 5 19v-7.4l-.4.32a1 1 0 1 1-1.24-1.58Z' fill='{{C}}' stroke='none'/>",
  'control-fill': "<path d='M4 7h8.5M19.5 7h1'/><circle cx='14.5' cy='7' r='2.5' fill='{{C}}' stroke='none'/><path d='M4 17h3M12 17h8'/><circle cx='9' cy='17' r='2.5' fill='{{C}}' stroke='none'/>",
  'bell-fill': "<path d='M6 9.5a6 6 0 1 1 12 0c0 4.2 1.7 5.6 1.7 5.6H4.3S6 13.7 6 9.5Z' fill='{{C}}' stroke='none'/><path d='M10 18.5a2 2 0 0 0 4 0' fill='{{C}}' stroke='none'/>",
  'grid-fill': "<rect x='4' y='4' width='7' height='7' rx='2' fill='{{C}}' stroke='none'/><rect x='13' y='4' width='7' height='7' rx='2' fill='{{C}}' stroke='none'/><rect x='4' y='13' width='7' height='7' rx='2' fill='{{C}}' stroke='none'/><rect x='13' y='13' width='7' height='7' rx='2' fill='{{C}}' stroke='none'/>",
  'user-fill': "<circle cx='12' cy='8.5' r='3.6' fill='{{C}}' stroke='none'/><path d='M5 20a7 7 0 0 1 14 0Z' fill='{{C}}' stroke='none'/>",

  /* ---- 设备类型 ---- */
  screen: "<rect x='3' y='4.5' width='18' height='12' rx='2.5'/><path d='M9 20.5h6M12 16.5v4'/>",
  bulb: "<path d='M9.5 16.5h5M10.2 19h3.6'/><path d='M12 3.5a5.5 5.5 0 0 0-3.2 10c.45.32.7.85.7 1.4v.6h5v-.6c0-.55.25-1.08.7-1.4A5.5 5.5 0 0 0 12 3.5Z'/>",
  ac: "<rect x='3' y='5' width='18' height='10' rx='2.5'/><path d='M10.2 8v3.2M8.6 9.6h3.2M15.2 8.4l2.2 2.2M17.4 8.4l-2.2 2.2'/><path d='M6.5 18.5h11M8.5 21h7'/>",
  projector: "<rect x='3' y='8' width='18' height='9' rx='2.2'/><circle cx='8.2' cy='12.5' r='2.1'/><path d='M13.5 11h4M13.5 14h2.8'/><path d='M8 17v2.2M16 17v2.2'/>",
  sensor: "<circle cx='12' cy='12' r='1.7' fill='{{C}}' stroke='none'/><path d='M8.8 8.8a4.5 4.5 0 0 0 0 6.4M15.2 8.8a4.5 4.5 0 0 1 0 6.4M6.3 6.3a8 8 0 0 0 0 11.4M17.7 6.3a8 8 0 0 1 0 11.4'/>",
  codec: "<rect x='7' y='7' width='10' height='10' rx='2'/><path d='M10 3v3M14 3v3M10 18v3M14 18v3M3 10h3M3 14h3M18 10h3M18 14h3'/>",

  /* ---- 环境指标 ---- */
  temperature: "<path d='M10 13.4V5.6a2 2 0 1 1 4 0v7.8a4 4 0 1 1-4 0Z'/><circle cx='12' cy='16.6' r='1.6' fill='{{C}}' stroke='none'/>",
  humidity: "<path d='M12 4s6 6.4 6 10.3a6 6 0 0 1-12 0C6 10.4 12 4 12 4Z'/>",
  co2: "<path d='M7 18.5h10a4.2 4.2 0 0 0 .9-8.3 5.6 5.6 0 0 0-10.8 1.1A3.6 3.6 0 0 0 7 18.5Z'/>",
  sunlight: "<circle cx='12' cy='12' r='4'/><path d='M12 2.8v2.2M12 19v2.2M4.9 4.9l1.6 1.6M17.5 17.5l1.6 1.6M2.8 12H5M19 12h2.2M4.9 19.1l1.6-1.6M17.5 6.5l1.6-1.6'/>",
  pm: "<circle cx='7' cy='9.5' r='2' fill='{{C}}' stroke='none'/><circle cx='14.5' cy='7.5' r='1.5' fill='{{C}}' stroke='none'/><circle cx='16' cy='14.5' r='1.2' fill='{{C}}' stroke='none'/><path d='M4 18c2-1.6 4-1.6 6 0s4 1.6 6 0'/>",
  motion: "<circle cx='12' cy='7' r='2'/><path d='M12 9v4.5M12 10.6 9.5 12.2M12 10.6l2.2 1.2M12 13.5 9.2 18M12 13.5 14.8 18'/>",

  /* ---- 认证方式 / 场景 ---- */
  face: "<rect x='4' y='5' width='16' height='14' rx='3.5'/><circle cx='9.2' cy='10.8' r='.8' fill='{{C}}' stroke='none'/><circle cx='14.8' cy='10.8' r='.8' fill='{{C}}' stroke='none'/><path d='M9.3 14.6c.7.7 1.7 1 2.7 1s2-.3 2.7-1'/>",
  card: "<rect x='3' y='6' width='18' height='12' rx='2.5'/><path d='M3 10h18'/><path d='M7 14.5h4'/>",
  play: "<path d='M9 7.2v9.6L18 12Z'/>",
  power: "<path d='M12 4v8'/><path d='M7.4 6.8a7 7 0 1 0 9.2 0'/>",

  /* ---- 状态插画用 ---- */
  'empty-box': "<path d='M4 8l8-4 8 4-8 4-8-4Z'/><path d='M6 10.5V17l6 3 6-3v-6.5'/><path d='M14 6.5 18 4.5'/>",
  wifi: "<path d='M5 12.2a12 12 0 0 1 14 0'/><path d='M8 15.7a7 7 0 0 1 8 0'/><path d='M11 18.9a2 2 0 0 1 2 0'/><circle cx='12' cy='21.2' r='.8' fill='{{C}}' stroke='none'/>",
  'wifi-off': "<path d='M3 3l18 18'/><path d='M9.6 5.6A11 11 0 0 1 22 9'/><path d='M15 10.5a7 7 0 0 1 2 1.6'/><path d='M2 9a11 11 0 0 1 4.3-2.8M6 12.6a7 7 0 0 0-2 1.6'/><path d='M9.5 16.1a3 3 0 0 1 5 0'/><circle cx='12' cy='19' r='.6' fill='{{C}}' stroke='none'/>",
  'bell-ring': "<path d='M6 9.5a6 6 0 1 1 12 0c0 4.2 1.7 5.6 1.7 5.6H4.3S6 13.7 6 9.5Z'/><path d='M10 18.5a2 2 0 0 0 4 0'/><path d='M2.8 10a7.5 7.5 0 0 0-.8 3.4M21.2 10a7.5 7.5 0 0 1 .8 3.4'/>",
  restart: "<path d='M3 12a9 9 0 1 0 2.64-6.36'/><path d='M3 3v6h6'/>",
}
