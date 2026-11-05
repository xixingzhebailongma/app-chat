<template>
  <view class="cv page-pad" @tap="onBackdropTap">
    <!-- 首屏 Hero：caps 小标 + 大标题 + 在线率圆环（全页唯一视觉焦点） -->
    <view class="cv__hero">
      <view class="cv__hero-glow" />
      <view class="cv__hero-top">
        <text class="cv__caps">SPATIAL OVERVIEW</text>
        <text class="cv__caps-sep">·</text>
        <text class="cv__caps-dim">{{ todayText }}</text>
        <view class="cv__avatar-sm" hover-class="cv__avatar-sm--press" @tap.stop="goProfile">
          <text class="cv__avatar-sm-text">{{ (store.name || '师').slice(0, 1) }}</text>
          <view class="cv__avatar-dot" :class="'cv__avatar-dot--' + wsTone" />
        </view>
      </view>

      <view class="cv__hero-main">
        <view class="cv__hero-left">
          <text class="cv__name">{{ currentSpace ? currentSpace.name : '我的教室' }}</text>
          <view class="cv__hero-run">
            <view class="cv__hero-run-dot" />
            <text class="cv__hero-run-text">{{ onlineCount }}/{{ devices.length }} 台设备在线</text>
          </view>
          <text class="cv__hero-sync">{{ wsText }} · 同步于 {{ syncText }}</text>
        </view>
        <view class="cv__hero-ring">
          <view class="cv__hero-ring-arc" :style="{ '--p': onlinePct + '%' }">
            <view class="cv__hero-ring-hole">
              <text class="cv__hero-ring-num num">{{ onlinePct }}<text class="cv__hero-ring-pct">%</text></text>
              <text class="cv__hero-ring-cap">在线率</text>
            </view>
          </view>
        </view>
      </view>
      <view class="cv__hero-rule" />
    </view>

    <!-- 教室切换：原生 selector 下拉，与「设备控制」的空间选择保持一致 -->
    <picker
      mode="selector"
      :range="spaceTabs"
      range-key="label"
      :value="spaceIndex"
      @change="onSpaceChange"
    >
      <view class="cv__space-bar" hover-class="cv__space-bar--press" :hover-stay-time="60">
        <text class="cv__space-bar-caps">CLASSROOM</text>
        <text class="cv__space-bar-name">{{ currentSpace ? currentSpace.name : '请选择教室' }}</text>
        <text class="cv__space-bar-count">{{ spaces.length }} 间</text>
        <uni-icons type="arrow-down" size="14" color="var(--brand)" />
      </view>
    </picker>

    <!-- 空间概览：设备按区归组，点阵直读在线状态（只读 devices，零接口改动） -->
    <view v-if="zones.length" class="cv__zones">
      <view class="cv__zones-head">
        <view class="cv__zones-title-wrap">
          <text class="cv__zones-caps">BY ZONE</text>
          <text class="cv__zones-title">空间概览</text>
        </view>
        <text class="cv__zones-sub">{{ onlineCount }}/{{ devices.length }} 在线</text>
      </view>
      <view v-for="z in zones" :key="z.key" class="cv__zone-row">
        <text class="cv__zone-label">{{ z.label }}</text>
        <view class="cv__zone-dots">
          <view
            v-for="d in z.dots"
            :key="d.id"
            class="cv__zone-dot"
            :class="{ 'cv__zone-dot--on': d.online }"
          />
          <text v-if="z.more > 0" class="cv__zone-more">+{{ z.more }}</text>
        </view>
        <text class="cv__zone-count">{{ z.online }}/{{ z.total }}</text>
      </view>
    </view>

    <!-- 订阅引导条：轻量解释价值，用户点击后才调起授权，不自动弹 -->
    <view v-if="showSubGuide" class="cv__subguide">
      <view class="cv__subguide-icon">
        <uni-icons type="notification" size="20" color="#245e70" />
      </view>
      <text class="cv__subguide-text">开启告警推送，教室异常第一时间通知您</text>
      <text class="cv__subguide-btn" @tap="requestSub">去开启</text>
      <view class="cv__subguide-close" @tap="dismissSubGuide">
        <uni-icons type="closeempty" size="14" color="var(--text-3)" />
      </view>
    </view>

    <!-- 可自定义卡片区 -->
    <view
      v-for="w in widgets"
      :key="w"
      class="card cv__card"
      :class="{ 'cv__card--edit': editMode }"
      @tap.stop
      @longpress="editMode = true"
    >
      <view v-if="editMode" class="cv__w-del" @tap.stop="removeWidget(w)">
        <text class="cv__w-del-minus">−</text>
      </view>

      <view class="cv__card-eyebrow">
        <text class="cv__card-caps">{{ w === 'sensor' ? 'ENVIRONMENT' : w === 'switch' ? 'SWITCHES' : w === 'panel' ? 'TERMINALS' : 'SCENES' }}</text>
        <text v-if="w === 'sensor'" class="cv__card-count">{{ envReadings.length }} 项指标 · 示例数据</text>
        <text v-else-if="w === 'switch'" class="cv__card-count">{{ switchDevices.length }} 台可控</text>
        <text v-else-if="w === 'panel'" class="cv__card-count">{{ panelDevices.length }} 台终端</text>
        <text v-else class="cv__card-count">{{ fixedScenes.length + customScenes.length }} 个场景</text>
      </view>

      <view class="card__title">
        {{ widgetTitle(w) }}
        <view v-if="w === 'scene'" class="cv__scene-add" hover-class="cv__scene-add--press" @tap.stop="openCreateScene">
          <view class="cv__scene-add-bar" />
          <view class="cv__scene-add-bar cv__scene-add-bar--v" />
        </view>
      </view>

      <!-- 场景控制：蓝色方块网格（参考图底部 Mode/Control/Set 三蓝块） -->
      <view v-if="w === 'scene'" class="cv__scene-grid">
        <view
          v-for="s in scenes"
          :key="s.scene_id"
          class="cv__scene-tile"
          :class="{ 'cv__scene-tile--on': isSceneActive(s) }"
          hover-class="cv__scene-tile--press"
          :hover-stay-time="60"
          @tap="runScene(s)"
          @longpress.stop="onSceneLongPress(s)"
        >
          <view class="cv__scene-tile-ic">
            <view class="cv__glyph" :class="s.kind === 'all_off' ? 'cv__glyph--out' : 'cv__glyph--power'" />
          </view>
          <text class="cv__scene-tile-name">{{ s.name }}</text>
          <text class="cv__scene-tile-desc">{{ sceneDesc(s) }}</text>
        </view>
        <view class="cv__scene-tile cv__scene-tile--add" hover-class="cv__scene-tile--press" @tap.stop="openCreateScene">
          <view class="cv__scene-tile-ic cv__scene-tile-ic--add">
            <uni-icons type="plusempty" size="20" color="#245e70" />
          </view>
          <text class="cv__scene-tile-name cv__scene-tile-name--add">新建场景</text>
        </view>
        <view v-if="!scenes.length" class="cv__empty-line">暂无场景</view>
      </view>

      <!-- 开关设备：两列蓝白磁贴，整卡点按切换（参考图 Light bulbs / Smart TV 磁贴） -->
      <view v-else-if="w === 'switch'" class="cv__dev-grid">
        <view
          v-for="d in switchDevices"
          :key="d.device_id"
          class="cv__dev-card"
          :class="{ 'cv__dev-card--on': d.app === 'on' && d.status === 'online', 'cv__dev-card--off': d.status !== 'online' }"
          hover-class="cv__dev-card--press"
          :hover-stay-time="60"
          @tap="onDeviceCardTap(d)"
        >
          <view class="cv__dev-card-top">
            <view class="cv__dev-card-ic">
              <uni-icons :type="deviceTypeIcon(d.device_type, d.zb_type)" size="20" :color="d.app === 'on' && d.status === 'online' ? '#ffffff' : 'var(--text-2)'" />
            </view>
            <view class="cv__dev-tgl" :class="{ 'cv__dev-tgl--on': d.app === 'on' && d.status === 'online' }">
              <view class="cv__dev-tgl-knob" />
            </view>
          </view>
          <text class="cv__dev-card-name">{{ d.label }}</text>
          <text class="cv__dev-card-sub">{{ devSub(d) }}</text>
        </view>
        <view v-if="!switchDevices.length" class="cv__empty-line">暂无开关设备</view>
      </view>

      <!-- 环境面板（fuhe 类：设备行 + CPU/MEM/UPTIME 运行指标条） -->
      <template v-else-if="w === 'panel'">
        <view v-for="d in panelDevices" :key="d.device_id" class="cv__panel-item">
          <view class="cv__dev-row" :class="{ 'cv__dev-row--off': d.status !== 'online' }">
            <view class="cv__dev-icon" :style="d.status === 'online' ? { background: iconSoft(d) } : {}">
              <uni-icons :type="deviceTypeIcon(d.device_type, d.zb_type)" size="20" :color="d.status === 'online' ? iconColor(d) : 'var(--text-4)'" />
            </view>
            <view class="cv__dev-main">
              <text class="cv__dev-name">{{ d.label }}</text>
              <text class="cv__dev-sub">{{ devSub(d) }}</text>
            </view>
            <switch
              class="cv__dev-switch"
              :checked="d.app === 'on'"
              :disabled="d.status !== 'online'"
              color="#245e70"
              @change="onDeviceToggle(d, $event)"
            />
          </view>
          <view v-if="panelInfo(d)" class="cv__panel-info">
            <view v-for="(v, k) in panelInfo(d)" :key="k" class="cv__panel-cell">
              <text class="cv__panel-k">{{ k }}</text>
              <text class="cv__panel-v">{{ v }}</text>
            </view>
          </view>
        </view>
        <view v-if="!panelDevices.length" class="cv__empty-line">暂无环境面板设备</view>
      </template>

      <!-- 传感器：主卡圆环仪表 + 辅卡数值条（读数分主次） -->
      <view v-else-if="w === 'sensor'">
        <view v-if="envReadings.length" class="cv__gauge-grid">
          <view
            v-for="(r, i) in envReadings"
            :key="r.key"
            class="cv__gauge"
            :class="{ 'cv__gauge--lead': i === 0, 'cv__gauge--wide': i > 0 && i === envReadings.length - 1 && envReadings.length % 2 === 0 }"
            hover-class="cv__gauge--press"
            :hover-stay-time="60"
          >
            <!-- 主卡：圆环仪表（弧长由 --p 驱动，值来自阻尼后的 envAnim） -->
            <view v-if="i === 0" class="cv__gauge-arc" :style="{ '--p': (envPct[r.key] || 0) + '%' }">
              <view class="cv__gauge-hole">
                <text
                  class="cv__gauge-num num"
                  :class="{ 'cv__gauge-num--alarm': r.alarm, 'cv__gauge-num--flash': r.flashing }"
                >{{ envAnim[r.key] || r.display }}</text>
                <text v-if="r.unit" class="cv__gauge-unit">{{ r.unit }}</text>
              </view>
            </view>
            <view v-if="i === 0" class="cv__gauge-lead-main">
              <text class="cv__gauge-label">{{ r.label }}</text>
              <text class="cv__gauge-lead-sub" :class="{ 'cv__gauge-lead-sub--alarm': r.alarm }">
                {{ r.alarm ? '超出示例阈值' : '示例读数 · 非真实采集' }}
              </text>
            </view>

            <!-- 辅卡：标签 + 数值 + 进度条 -->
            <template v-else>
              <view class="cv__gauge-head">
                <text class="cv__gauge-label">{{ r.label }}</text>
                <view v-if="r.alarm" class="cv__gauge-warn">
                  <uni-icons type="info-filled" size="14" color="var(--danger)" />
                </view>
              </view>
              <view class="cv__gauge-value">
                <text
                  class="cv__gauge-num num"
                  :class="{ 'cv__gauge-num--alarm': r.alarm, 'cv__gauge-num--flash': r.flashing }"
                >{{ envAnim[r.key] || r.display }}</text>
                <text v-if="r.unit" class="cv__gauge-unit">{{ r.unit }}</text>
              </view>
              <view class="cv__gauge-bar">
                <view class="cv__gauge-bar-fill" :style="{ width: (envPct[r.key] || 0) + '%' }" />
              </view>
            </template>
          </view>
        </view>
        <view v-else class="cv__empty-line">教室内暂无在线传感器</view>
      </view>

    </view>

    <!-- 底部蓝色快捷横幅（参考图 Quick remote access） -->
    <view class="cv__banner" hover-class="cv__banner--press" @tap.stop="goDevices">
      <view class="cv__banner-main">
        <text class="cv__banner-title">设备控制</text>
        <text class="cv__banner-desc">前往设备页逐台精细控制与查看状态</text>
      </view>
      <view class="cv__banner-btn">
        <uni-icons type="right" size="16" color="#ffffff" />
      </view>
    </view>

    <!-- 添加卡片入口 -->
    <view v-if="widgetOptions.length" class="card cv__add-card" @tap.stop="showPicker = true">
      <view class="cv__add-ring">
        <text class="cv__add-plus">+</text>
      </view>
      <text class="cv__add-text">添加卡片</text>
    </view>

    <!-- 编辑/完成按钮 -->
    <view class="cv__edit-bar" @tap.stop>
      <view class="cv__edit-action" @tap="editMode = !editMode">
        {{ editMode ? '完成' : '自定义卡片' }}
      </view>
    </view>

    <!-- 添加卡片弹窗 -->
    <view v-if="showPicker" class="cv__mask" @tap="showPicker = false">
      <view class="cv__sheet" @tap.stop>
        <view class="cv__sheet-title">添加卡片</view>
        <view
          v-for="opt in widgetOptions"
          :key="opt.type"
          class="cv__opt"
          :class="{ 'cv__opt--off': opt.disabled, 'cv__opt--on': opt.added }"
          @tap="onPick(opt)"
        >
          <uni-icons type="flag" size="22" :color="opt.disabled ? 'var(--text-3)' : 'var(--primary)'" />
          <view class="cv__opt-main">
            <text class="cv__opt-name">{{ opt.title }}</text>
            <text class="cv__opt-desc">{{ opt.desc }}</text>
          </view>
          <text class="cv__opt-state">{{ opt.added ? '已添加' : (opt.disabled ? '暂无设备' : '可添加') }}</text>
        </view>
        <view class="cv__sheet-cancel" @tap="showPicker = false">取消</view>
      </view>
    </view>

    <AppTabBar current="classroom" />

    <!-- 场景编辑弹层 -->
    <view v-if="sceneSheetVisible" class="mask" @tap="sceneSheetVisible = false"></view>
    <view class="sheet" :class="{ show: sceneSheetVisible }">
      <view class="sheet-head">
        <text class="sheet-title">{{ editingScene ? '编辑场景' : '新建场景' }}</text>
        <view class="sheet-close" @tap="sceneSheetVisible = false">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <view class="field-row">
        <text class="field-label">场景名</text>
        <input v-model="sceneForm.name" class="field" placeholder="如 午休模式 / 考试模式" placeholder-class="ph" />
      </view>
      <view class="section-title"><text class="t">设备状态</text></view>
      <text class="sd-hint">仅开关类设备可加入场景；门禁、传感器、编码器不纳入</text>
      <view v-if="sceneControllableDevices.length" class="list-group scene-device-list">
        <view v-for="d in sceneControllableDevices" :key="d.device_id" class="list-cell list-cell-center">
          <text class="sd-label">{{ d.label }}</text>
          <switch :checked="!!sceneForm.states[d.device_id]" color="#245e70" @change="onSceneDeviceToggle(d, $event)" />
        </view>
      </view>
      <text v-else class="sd-empty">该教室暂无开关类设备</text>
      <view class="sheet-footer">
        <button class="btn btn-primary" :disabled="submitting" @tap="saveScene">确认保存</button>
      </view>
    </view>
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import { deviceTypeLabel, formatTime, spaceTypeLabel } from '../../utils/format'
import { deviceTypeIcon } from '../../utils/icon'
import { controlErrorMessage } from '../../utils/controlError'
import { parseSensorApp } from '../../utils/env'
import { dampValue, durationOf, FLASH_MS } from '../../utils/envDamp'
import { connectWs, closeWs } from '../../utils/ws'
import { WIDGETS, WIDGET_MAP, loadWidgets, saveWidgets } from '../../utils/widgets'
import AppTabBar from '../../components/AppTabBar.vue'
import config from '../../config'
import { requestSubscribe, subscriptionActive, promptDismissed, dismissPrompt } from '../../utils/subscribe'

// 当前教室持久化：切 Tab / 重进页面 / 重启小程序后仍保留用户选择的教室；
// 若该教室已被解绑或禁用，load() 回退到授权列表的第一个。
const CURRENT_SPACE_KEY = 'sc_classroom_current_space'
function loadCurrentSpace() {
  try { return uni.getStorageSync(CURRENT_SPACE_KEY) || '' } catch (e) { return '' }
}
function saveCurrentSpace(id) {
  try { uni.setStorageSync(CURRENT_SPACE_KEY, id || '') } catch (e) { /* ignore */ }
}

export default {
  components: { AppTabBar },
  data() {
    return {
      store,
      spaces: [],
      current: '',
      devices: [],
      env: null,
      /* 读数变化时高亮的指标键（微光呼吸结束后移除） */
      flashKeys: [],
      /* P1 差值阻尼：envAnim[key] = 当前正在显示的数值文案，只有跑过动画的指标才有键 */
      envAnim: {},
      redirecting: false,
      scenes: [],
      activeSceneId: '',
      sceneSheetVisible: false,
      editingScene: null,
      sceneForm: { name: '', states: {} },
      submitting: false,
      widgets: loadWidgets(),
      editMode: false,
      showPicker: false,
      wsConnected: true,
      spaceSwitching: false,
      showSubGuide: false,
    }
  },
  computed: {
    currentSpace() {
      return this.spaces.find((s) => s.space_id === this.current) || this.spaces[0] || null
    },
    // 下拉选项：名称 + 类型，与设备控制页的展示口径一致
    spaceTabs() {
      return this.spaces.map((s) => ({
        key: s.space_id,
        label: s.name + ' · ' + spaceTypeLabel(s.type),
      }))
    },
    spaceIndex() {
      const i = this.spaceTabs.findIndex((t) => t.key === this.current)
      return i < 0 ? 0 : i
    },
    onlineCount() {
      return this.devices.filter((d) => d.status === 'online').length
    },
    // 首屏在线率（圆环用；纯展示派生，不参与任何业务判断）
    onlinePct() {
      const total = this.devices.length
      if (!total) return 0
      return Math.round((this.onlineCount / total) * 100)
    },
    // 空间概览分组：把在线状态摊成点阵（只读 devices，零接口改动）
    zones() {
      const buckets = { switch: [], sensor: [], fuhe: [], other: [] }
      this.devices.forEach((d) => {
        if (d.zb_type === 'switch') buckets.switch.push(d)
        else if (d.zb_type === 'sensor') buckets.sensor.push(d)
        else if (String(d.device_type || '').startsWith('fuhe')) buckets.fuhe.push(d)
        else buckets.other.push(d)
      })
      const groups = [
        { key: 'switch', label: '照明与开关' },
        { key: 'sensor', label: '环境与传感' },
        { key: 'fuhe', label: '终端与屏' },
        { key: 'other', label: '其他设备' },
      ]
      return groups
        .filter((g) => buckets[g.key].length > 0)
        .map((g) => {
          const items = buckets[g.key]
          const shown = items.slice(0, 12)
          return {
            key: g.key,
            label: g.label,
            total: items.length,
            online: items.filter((d) => d.status === 'online').length,
            more: items.length - shown.length,
            dots: shown.map((d, i) => ({
              id: d.device_id || g.key + i,
              online: d.status === 'online',
            })),
          }
        })
    },
    // 顶部日期文案：如「10月15日 周三」
    todayText() {
      const d = new Date()
      const week = ['周日', '周一', '周二', '周三', '周四', '周五', '周六'][d.getDay()]
      return (d.getMonth() + 1) + '月' + d.getDate() + '日 ' + week
    },
    switchDevices() {
      return this.devices.filter((d) => this.isSwitchable(d))
    },
    panelDevices() {
      return this.devices.filter((d) => String(d.device_type).startsWith('fuhe'))
    },
    sensorDevices() {
      return this.devices.filter(
        (d) => d.zb_type === 'sensor' || (!this.isSwitchable(d) && !String(d.device_type).startsWith('fuhe'))
      )
    },
    sceneControllableDevices() {
      return this.devices.filter((d) => this.isSwitchable(d))
    },
    // smart scene-bar 结构：大按钮行固定只有内置两个场景（开启/离开），
    // 自定义场景一律走下方 chips，保证大按钮永远 1:1 不被挤压
    fixedScenes() {
      return this.scenes.filter((s) => s.kind !== 'custom').slice(0, 2)
    },
    customScenes() {
      return this.scenes.filter((s) => s.kind === 'custom')
    },
    /* 传感器读数（对齐 smart env-grid：只渲染 app JSON 里实际存在的字段） */
    envReadings() {
      const merged = {}
      const meta = {
        temperature: { label: '温度', unit: '℃', decimals: 1 },
        humidity: { label: '湿度', unit: '%RH', decimals: 0 },
        co2: { label: 'CO₂', unit: 'ppm', decimals: 0 },
        light: { label: '光照', unit: 'lux', decimals: 0 },
        pm25: { label: 'PM2.5', unit: 'μg/m³', decimals: 0 },
        pir: { label: '人体感应', unit: '', boolean: true },
      }
      this.sensorDevices.forEach((d) => {
        const o = parseSensorApp(d)
        if (!o) return
        Object.keys(o).forEach((k) => { if (meta[k] && o[k] != null) merged[k] = o[k] })
      })
      return Object.keys(meta)
        .filter((k) => k in merged)
        .map((k) => {
          const m = meta[k]
          const v = merged[k]
          return {
            key: k,
            label: m.label,
            unit: m.unit,
            /* 原始数值与小数位：只给差值阻尼用（P1），display 仍是未动画时的兜底文案 */
            raw: v,
            decimals: m.decimals || 0,
            isBool: !!m.boolean,
            display: m.boolean ? (v ? '有人' : '无人') : Number(v).toFixed(m.decimals || 0),
            alarm: this.envAlarm(k, v),
            flashing: this.flashKeys.indexOf(k) >= 0,
          }
        })
    },
    /* 读数原始值签名：只用于驱动阻尼动画，不参与渲染 */
    envRawSig() {
      return this.envReadings.map((r) => r.key + '=' + r.raw).join('|')
    },
    /* 仪表弧长 / 进度条占比：取阻尼后的显示值，让弧长与数字同步收敛 */
    envPct() {
      const scale = {
        temperature: [0, 40],
        humidity: [0, 100],
        co2: [400, 2000],
        pm25: [0, 150],
        light: [0, 2000],
      }
      const out = {}
      this.envReadings.forEach((r) => {
        if (r.isBool) {
          out[r.key] = r.raw ? 100 : 0
          return
        }
        const shown = this.envAnim[r.key]
        const v = Number(shown === undefined || shown === '' ? r.raw : shown)
        const s = scale[r.key]
        if (!s || !isFinite(v)) {
          out[r.key] = 0
          return
        }
        const pct = ((v - s[0]) / (s[1] - s[0])) * 100
        out[r.key] = Math.max(0, Math.min(100, Math.round(pct)))
      })
      return out
    },
    syncText() {
      const list = this.devices.filter((d) => d.last_seen)
      if (!list.length) return formatTime(Date.now())
      const latest = list.slice().sort((a, b) => new Date(b.last_seen) - new Date(a.last_seen))[0].last_seen
      return formatTime(latest)
    },
    wsText() {
      return this.wsConnected ? '在线' : '离线'
    },
    wsTone() {
      return this.wsConnected ? 'on' : 'off'
    },
    widgetOptions() {
      return WIDGETS.map((w) => ({
        ...w,
        added: this.widgets.includes(w.type),
        disabled: this.widgets.includes(w.type) || !this.widgetAvailable(w.type),
      }))
    },
  },
  watch: {
    /* 读数一变就跑差值阻尼（P1）。必须**同步**跑：runEnvDamp 的第一帧会把
     * envAnim 写回「旧值」，抢在本次渲染之前落地，否则目标值会先闪一帧再被打回去 */
    envRawSig() {
      this.runEnvDamp()
    },
  },
  created() {
    /* 动画句柄放实例上但不进 data：不需要参与渲染，避免无谓 setData */
    this._dampers = {}
    this._flashTimers = {}
    this._pageHidden = false
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    if (store.role === 'admin' && !this.redirecting) {
      this.redirecting = true
      uni.reLaunch({ url: '/pages/home/home' })
      return
    }
    this._pageHidden = false
    /* 回到前台先补一拍：页隐藏期间读数变更被丢弃了，这里从上次显示值接上 */
    this.runEnvDamp()
    this.initSubGuide()
    this.load()
  },
  onHide() {
    /* 页面不可见：立刻终止阻尼时间线，避免后台空转 setData */
    this._pageHidden = true
    this.cancelEnvDamp()
    this.flashKeys = []
    closeWs()
  },
  onUnload() {
    this._pageHidden = true
    this.cancelEnvDamp()
    closeWs()
  },
  onPullDownRefresh() {
    this.fetchDevices()
  },
  methods: {
    spaceTypeLabel,
    deviceType: deviceTypeLabel,
    deviceTypeIcon,
    widgetTitle(t) { return (WIDGET_MAP[t] && WIDGET_MAP[t].title) || t },
    /* 阈值（对齐 smart utils/sensor.js 的 levelOf） */
    envAlarm(k, v) {
      const n = Number(v)
      if (Number.isNaN(n)) return false
      if (k === 'temperature') return n >= 30
      if (k === 'humidity') return n < 30 || n > 70
      if (k === 'co2') return n >= 1500
      if (k === 'pm25') return n >= 75
      return false
    },
    widgetAvailable(t) {
      if (t === 'scene') return true
      if (t === 'switch') return this.switchDevices.length > 0
      if (t === 'panel') return this.panelDevices.length > 0
      if (t === 'sensor') return this.sensorDevices.length > 0
      return false
    },
    addWidget(t) {
      if (!this.widgets.includes(t)) {
        this.widgets.push(t)
        saveWidgets(this.widgets)
      }
    },
    removeWidget(t) {
      this.widgets = this.widgets.filter((x) => x !== t)
      saveWidgets(this.widgets)
    },
    onBackdropTap() {
      // 点击卡片外的空白区域：退出编辑模式并关闭添加弹窗
      if (this.editMode) this.editMode = false
      if (this.showPicker) this.showPicker = false
    },
    onPick(opt) {
      if (opt.disabled) return
      this.addWidget(opt.type)
    },
    /* 订阅引导条：仅真实模式 + 已配模板 + 未订阅 + 未关闭时显示（与 smart 端一致，点击才拉起授权） */
    async initSubGuide() {
      if (config.USE_MOCK) return
      if (!(config.SUBSCRIBE_TMPL_IDS || []).length) return
      if (promptDismissed()) return
      if (uni.getStorageSync('sc_sub_guide_dismissed') === 'yes') return
      const active = await subscriptionActive()
      if (!active) this.showSubGuide = true
    },
    requestSub() {
      requestSubscribe().then((res) => {
        if (res === 'accepted') {
          this.dismissSubGuide()
          uni.showToast({ title: '已开启推送', icon: 'none' })
        } else if (res === 'rejected') {
          uni.showToast({ title: '已拒绝，可稍后在「我的」中再次开启', icon: 'none' })
        } else if (res === 'failed' || res === 'banned') {
          uni.showToast({ title: '授权未完成', icon: 'none' })
        }
      })
    },
    dismissSubGuide() {
      this.showSubGuide = false
      dismissPrompt()
      try { uni.setStorageSync('sc_sub_guide_dismissed', 'yes') } catch (e) { /* ignore */ }
    },
    /* 设备行辅助：图标色 / 浅底（统一蓝色体系） */
    iconColor(d) {
      return 'var(--brand)'
    },
    iconSoft(d) {
      return 'var(--brand-soft)'
    },
    devSub(d) {
      if (d.status !== 'online') return '设备已离线'
      return d.last_seen ? '上报 ' + formatTime(d.last_seen) : '在线'
    },
    panelInfo(d) {
      try {
        if (!d || !d.info) return null
        const o = typeof d.info === 'string' ? JSON.parse(d.info) : d.info
        return o && Object.keys(o).length ? o : null
      } catch (e) {
        return null
      }
    },
    isSwitchable(d) {
      if (d.device_type === 'zigbee') return d.zb_type === 'switch' && d.zb_role !== 'door'
      return d.device_type === 'fuhe-screen' || d.device_type === 'fuhe-board'
    },
    // 场景磁贴的「当前生效」态：以服务端 active_scene_id 为准；
    // 尚未执行过任何场景时，把 all_on 视为主操作（纯展示派生，不参与下发判断）
    isSceneActive(s) {
      if (this.activeSceneId) return this.activeSceneId === s.scene_id
      return s.kind === 'all_on'
    },
    sceneDesc(s) {
      if (s.kind === 'all_on') return '全部开启'
      if (s.kind === 'all_off') return '全部关闭'
      const on = (s.device_states || []).filter((x) => x.command === 'on').map((x) => this.deviceLabel(x.device_id))
      if (on.length) return '开 ' + on.join('、')
      return '全部关闭'
    },
    deviceLabel(id) {
      const d = this.devices.find((x) => x.device_id === id)
      return d ? d.label : id
    },
    async runScene(scene) {
      if (!this.current) {
        uni.showToast({ title: '请先选择教室', icon: 'none' })
        return
      }
      try {
        const res = await api.executeScene(scene.scene_id)
        if (res && res.warning) {
          uni.showToast({ title: res.warning, icon: 'none' })
          return
        }
        const n = res && res.applied != null ? res.applied : 0
        this.activeSceneId = scene.scene_id
        uni.showToast({ title: `已下发 ${n} 台设备`, icon: 'none' })
        this.fetchDevices()
      } catch (e) {
        uni.showToast({ title: controlErrorMessage(e) || '下发失败', icon: 'none' })
      }
    },
    openCreateScene() {
      this.editingScene = null
      this.sceneForm = { name: '', states: {} }
      this.submitting = false
      this.sceneSheetVisible = true
    },
    openEditScene(scene) {
      this.editingScene = scene
      const states = {}
      ;(scene.device_states || []).forEach((s) => { states[s.device_id] = s.command === 'on' })
      this.sceneForm = { name: scene.name, states }
      this.submitting = false
      this.sceneSheetVisible = true
    },
    onSceneLongPress(scene) {
      uni.showActionSheet({
        itemList: ['编辑场景', '删除场景'],
        success: (res) => {
          if (res.tapIndex === 0) this.openEditScene(scene)
          else if (res.tapIndex === 1) this.removeScene(scene)
        },
      })
    },
    onSceneDeviceToggle(d, e) {
      this.sceneForm.states[d.device_id] = !!e.detail.value
    },
    async saveScene() {
      if (this.submitting) return
      if (!this.sceneForm.name.trim()) {
        uni.showToast({ title: '请输入场景名', icon: 'none' })
        return
      }
      this.submitting = true
      try {
        const deviceStates = this.sceneControllableDevices.map((d) => ({
          device_id: d.device_id,
          device_type: d.device_type,
          command: this.sceneForm.states[d.device_id] ? 'on' : 'off',
        }))
        if (this.editingScene) {
          await api.updateScene(this.editingScene.scene_id, { name: this.sceneForm.name.trim(), device_states: deviceStates })
        } else {
          await api.createScene({ space_id: this.current, name: this.sceneForm.name.trim(), device_states: deviceStates })
        }
        this.sceneSheetVisible = false
        uni.showToast({ title: '已保存', icon: 'success' })
        this.loadScenes()
      } catch (e) {
        uni.showToast({ title: e.message || '保存失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
    removeScene(scene) {
      uni.showModal({
        title: '删除场景',
        content: `确定删除「${scene.name}」？`,
        success: async (res) => {
          if (!res.confirm) return
          try {
            await api.deleteScene(scene.scene_id)
            if (this.activeSceneId === scene.scene_id) this.activeSceneId = ''
            uni.showToast({ title: '已删除', icon: 'success' })
            this.loadScenes()
          } catch (e) {
            uni.showToast({ title: e.message || '删除失败', icon: 'none' })
          }
        },
      })
    },
    async load() {
      try {
        const spaces = await api.spaces()
        this.spaces = spaces
        if (!this.current || !spaces.some((s) => s.space_id === this.current)) {
          // 优先恢复上次选择的教室；已失效（解绑/禁用）则回退授权列表第一个
          const saved = loadCurrentSpace()
          this.current = spaces.some((s) => s.space_id === saved)
            ? saved
            : (spaces.length ? spaces[0].space_id : '')
          saveCurrentSpace(this.current)
        }
        await Promise.all([this.fetchDevices(), this.loadScenes()])
        this.openWs()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
        uni.stopPullDownRefresh()
      }
    },
    async loadScenes() {
      if (!this.current) {
        this.scenes = []
        this.activeSceneId = ''
        return
      }
      try {
        const res = await api.scenes(this.current)
        this.scenes = (res && res.scenes) || []
        this.activeSceneId = (res && res.active_scene_id) || ''
      } catch (e) {
        uni.showToast({ title: e.message || '场景加载失败', icon: 'none' })
      }
    },
    async fetchDevices() {
      if (!this.current) {
        this.devices = []
        this.env = null
        uni.stopPullDownRefresh()
        return
      }
      try {
        const r = await api.devices(this.current)
        const ds = r.devices || []
        this.devices = ds
        const sensor = ds.find(
          (d) => d.device_type === 'zigbee' && d.zb_type === 'sensor' && parseSensorApp(d)
        )
        this.env = sensor ? parseSensorApp(sensor) : null
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    onSpaceChange(e) {
      const tab = this.spaceTabs[Number(e.detail.value)]
      const s = tab && this.spaces.find((x) => x.space_id === tab.key)
      if (s) this.switchSpace(s)
    },
    async switchSpace(s) {
      if (!s || this.spaceSwitching || s.space_id === this.current) return
      this.spaceSwitching = true
      // 重置上一个教室的瞬时态，避免编辑模式 / 场景弹层串到新教室
      this.editMode = false
      this.showPicker = false
      this.activeSceneId = ''
      this.sceneSheetVisible = false
      this.current = s.space_id
      saveCurrentSpace(this.current)
      try {
        // 全量重载：设备（开关/面板）+ 环境传感器、场景，
        // 并重连实时通道——与新进入页面完全一致，页面全部功能保留
        await Promise.all([this.fetchDevices(), this.loadScenes()])
        this.openWs()
        uni.showToast({ title: '已切换至 ' + s.name, icon: 'success' })
      } catch (e) {
        uni.showToast({ title: e.message || '切换失败', icon: 'none' })
      } finally {
        this.spaceSwitching = false
      }
    },
    async onDeviceToggle(d, e) {
      const cmd = e.detail.value ? 'on' : 'off'
      try {
        await api.control({ space_id: this.current, device_type: d.device_type, device_id: d.device_id, command: cmd })
        d.app = cmd
        this.activeSceneId = ''
      } catch (err) {
        uni.showToast({ title: controlErrorMessage(err) || '控制失败', icon: 'none' })
      }
    },
    // 两列磁贴整卡点按：离线不可控，在线则在 on/off 之间翻转
    onDeviceCardTap(d) {
      if (d.status !== 'online') {
        uni.showToast({ title: '设备已离线', icon: 'none' })
        return
      }
      this.onDeviceToggle(d, { detail: { value: d.app !== 'on' } })
    },
    goProfile() {
      uni.reLaunch({ url: '/pages/profile/profile' })
    },
    goDevices() {
      uni.reLaunch({ url: '/pages/devices/devices' })
    },
    openWs() {
      connectWs(this.current, {
        onMessage: (msg) => this.applyWsMessage(msg),
        onClose: () => { this.wsConnected = false },
      })
      this.wsConnected = true
    },
    applyWsMessage(msg) {
      if (!msg) return
      if (msg.type === 'device_update') {
        const idx = this.devices.findIndex((d) => d.device_id === msg.device_id)
        if (idx >= 0) {
          this.devices[idx] = { ...this.devices[idx], status: msg.status, app: msg.app }
          if (this.devices[idx].device_type === 'zigbee' && this.devices[idx].zb_type === 'sensor') {
            this.env = parseSensorApp(this.devices[idx])
            /* 微光不再整片闪：改由 envRawSig → runEnvDamp 在数值收敛时逐指标触发 */
          }
        }
      }
    },
    /* ============================================================
     * P1 · 环境读数差值阻尼（对齐《晴昼 1.4.2 交付说明》§三 / qingzhou animateNumber）
     * 上次值存在 utils/envDamp.js 的模块级 Map 里 —— 切教室、切 Tab 重建页面也不会丢，
     * 所以每次变化都是「从旧值滚到新值」，不会退回 0 重滚。
     * ============================================================ */
    runEnvDamp() {
      if (this._pageHidden) return
      const alive = {}
      this.envReadings.forEach((r) => {
        alive[r.key] = true
        if (r.isBool) {
          /* 人体感应是布尔态（有人/无人），不做数值阻尼 */
          this.setEnvAnim(r.key, r.display)
          return
        }
        this._dampers[r.key] = dampValue(r.key, r.raw, {
          duration: durationOf(r.key),
          onFrame: (val, final) => {
            this.setEnvAnim(r.key, Number(val).toFixed(r.decimals))
            if (final) this.flashEnvKey(r.key)
          },
        })
      })
      /* 指标消失（换教室 / 传感器离线）：收掉它的时间线与残留文案 */
      Object.keys(this._dampers).forEach((k) => {
        if (alive[k]) return
        this._dampers[k]()
        delete this._dampers[k]
        clearTimeout(this._flashTimers[k])
        delete this._flashTimers[k]
        if (this.envAnim[k] !== undefined) {
          const next = { ...this.envAnim }
          delete next[k]
          this.envAnim = next
        }
      })
    },
    setEnvAnim(key, text) {
      if (this.envAnim[key] === text) return
      this.envAnim = { ...this.envAnim, [key]: text }
    },
    /* 微光呼吸：先摘类、下一拍再加回，等价于演示页 remove → 强制回流 → add（重启动画） */
    flashEnvKey(key) {
      if (this._pageHidden) return
      const timers = this._flashTimers
      clearTimeout(timers[key])
      const add = () => {
        if (this.flashKeys.indexOf(key) < 0) this.flashKeys = this.flashKeys.concat(key)
        timers[key] = setTimeout(() => {
          this.flashKeys = this.flashKeys.filter((k) => k !== key)
          delete timers[key]
        }, FLASH_MS)
      }
      if (this.flashKeys.indexOf(key) >= 0) {
        this.flashKeys = this.flashKeys.filter((k) => k !== key)
        this.$nextTick(add)
      } else {
        add()
      }
    },
    cancelEnvDamp() {
      Object.keys(this._dampers).forEach((k) => {
        this._dampers[k]()
        delete this._dampers[k]
      })
      Object.keys(this._flashTimers).forEach((k) => {
        clearTimeout(this._flashTimers[k])
        delete this._flashTimers[k]
      })
    },
  },
}
</script>

<style scoped>

/* ============================================================
 * 教师端 · 我的教室（classroom）
 * 视觉基准：智能家居 App 参考图
 *   底色 var(--bg-page) 浅灰蓝 · 纯白大圆角卡 · 主色 var(--brand) 皇家蓝
 *   蓝卡承载「已开启 / 主操作」，白卡承载「未开启 / 次级」
 * 规则：仅重写样式，模板结构与 script 一行未动
 * ============================================================ */

.cv {
  padding-top: 12rpx;
  padding-bottom: calc(240rpx + env(safe-area-inset-bottom));
  /* 晴昼：环境光双径向叠层（对齐 qingzhou body） */
  background:
    radial-gradient(ellipse at 30% -10%, var(--ambient-a), transparent 55%),
    radial-gradient(ellipse at 95% 70%, var(--ambient-b), transparent 52%),
    var(--bg-page);
}

/* ---------- 首屏 Hero：caps 小标 + 大标题 + 在线率圆环 ---------- */
.cv__hero {
  margin: 12rpx 32rpx 10rpx;
  position: relative;
  overflow: hidden;
  padding: 26rpx 32rpx 0;
  border-radius: 36rpx;
  background: linear-gradient(150deg, var(--bg-card) 0%, var(--bg-card-end) 60%, var(--brand-soft) 100%);
  box-shadow: var(--shadow-panel);
}
.cv__hero-glow {
  position: absolute;
  top: -130rpx;
  right: -90rpx;
  width: 360rpx;
  height: 360rpx;
  border-radius: 50%;
  background: radial-gradient(circle, rgba(36, 94, 112, 0.16) 0%, rgba(36, 94, 112, 0) 68%);
  pointer-events: none;
}
.cv__hero-main {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20rpx;
  padding: 14rpx 0 22rpx;
}
.cv__hero-left {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.cv__hero-top {
  position: relative;
  display: flex;
  align-items: center;
  gap: 10rpx;
}
.cv__caps {
  font-size: 20rpx;
  font-weight: 600;
  letter-spacing: 0.18em;
  color: var(--brand);
}
.cv__caps-sep {
  font-size: 20rpx;
  color: var(--text-4);
}
.cv__caps-dim {
  flex: 1;
  font-size: 20rpx;
  color: var(--text-3);
}
.cv__avatar-sm {
  position: relative;
  width: 56rpx;
  height: 56rpx;
  border-radius: 50%;
  background: var(--brand-grad);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  box-shadow: 0 8rpx 18rpx rgba(36, 94, 112, 0.26);
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__avatar-sm--press {
  transform: scale(0.93);
}
.cv__avatar-sm-text {
  color: #FFFFFF;
  font-size: 26rpx;
  font-weight: 600;
}
.cv__name {
  margin-top: 8rpx;
  font-size: 54rpx;
  font-weight: 700;
  line-height: 1.15;
  color: var(--text-1);
  letter-spacing: 0.5rpx;
}
.cv__hero-run {
  display: flex;
  align-items: center;
  gap: 12rpx;
  margin-top: 18rpx;
}
.cv__hero-run-dot {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
  background: var(--success);
  box-shadow: 0 0 0 6rpx rgba(15, 112, 56, 0.14);
  flex-shrink: 0;
}
.cv__hero-run-text {
  font-size: 26rpx;
  font-weight: 500;
  color: var(--text-1);
}
.cv__hero-sync {
  margin-top: 8rpx;
  font-size: 22rpx;
  color: var(--text-3);
}

/* 在线率圆环：conic-gradient + --p，退化时仅显示轨道 */
.cv__hero-ring {
  flex-shrink: 0;
}
.cv__hero-ring-arc {
  width: 156rpx;
  height: 156rpx;
  border-radius: 50%;
  background-color: var(--gauge-track);
  background-image: conic-gradient(from 180deg, var(--gauge-start), var(--gauge-end) var(--p), var(--gauge-track) var(--p));
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: 0 10rpx 26rpx rgba(36, 94, 112, 0.16), 0 0 0 12rpx rgba(36, 94, 112, 0.05),
    0 0 34rpx rgba(36, 94, 112, 0.14);
}
.cv__hero-ring-hole {
  width: 122rpx;
  height: 122rpx;
  border-radius: 50%;
  background: var(--bg-card);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}
.cv__hero-ring-num {
  font-size: 42rpx;
  font-weight: 700;
  color: var(--brand);
  line-height: 1.1;
}
.cv__hero-ring-pct {
  font-size: 22rpx;
  font-weight: 600;
  color: var(--brand);
}
.cv__hero-ring-cap {
  font-size: 18rpx;
  color: var(--text-3);
  margin-top: 2rpx;
  letter-spacing: 0.08em;
}
.cv__hero-rule {
  position: relative;
  height: 1rpx;
  margin: 0 -32rpx;
  background: linear-gradient(90deg, rgba(36, 94, 112, 0), rgba(36, 94, 112, 0.45), rgba(36, 94, 112, 0));
}
.cv__avatar-dot {
  position: absolute;
  right: -2rpx;
  bottom: -2rpx;
  width: 20rpx;
  height: 20rpx;
  border-radius: 50%;
  border: 3rpx solid #FFFFFF;
}
.cv__avatar-dot--on {
  background: var(--success);
}
.cv__avatar-dot--off {
  background: var(--text-4);
}

/* ---------- 教室下拉入口（替代原横向标签行） ---------- */
.cv__space-bar {
  display: flex;
  align-items: center;
  gap: 16rpx;
  margin-top: 4rpx;
  padding: 22rpx 26rpx;
  border-radius: 24rpx;
  background: var(--bg-card);
  border: 1rpx solid var(--border-light);
  box-shadow: 0 6rpx 18rpx rgba(96, 81, 51, 0.05);
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__space-bar--press {
  transform: scale(0.985);
}
.cv__space-bar-caps {
  flex-shrink: 0;
  font-size: 18rpx;
  letter-spacing: 2rpx;
  color: var(--text-4);
}
.cv__space-bar-name {
  flex: 1;
  min-width: 0;
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__space-bar-count {
  flex-shrink: 0;
  font-size: 22rpx;
  color: var(--text-3);
}

/* ---------- 订阅引导条（蓝色调） ---------- */
.cv__subguide {
  display: flex;
  align-items: center;
  margin: 12rpx 32rpx 0;
  background: linear-gradient(135deg, var(--brand-soft) 0%, var(--bg-soft) 100%);
  border: 1rpx solid rgba(36, 94, 112, 0.16);
  border-radius: 24rpx;
  padding: 18rpx 22rpx;
  box-shadow: none;
}
.cv__subguide-icon {
  margin-right: 16rpx;
  display: flex;
  align-items: center;
}
.cv__subguide-text {
  flex: 1;
  color: var(--text-1);
  font-size: 26rpx;
}
.cv__subguide-btn {
  color: var(--brand);
  font-weight: 600;
  font-size: 26rpx;
  padding: 0 12rpx;
  flex-shrink: 0;
}
.cv__subguide-close {
  display: flex;
  align-items: center;
  padding: 0 6rpx 0 12rpx;
  flex-shrink: 0;
}

/* ---------- 通用卡片 ---------- */
.cv__card {
  margin: 0 32rpx 24rpx;
  padding: 32rpx;
  position: relative;
  box-shadow: 0 12rpx 32rpx rgba(96, 81, 51, 0.06);
}
.cv__card--edit {
  border: 3rpx dashed rgba(36, 94, 112, 0.42);
}

/* 编辑态删除按钮 */
.cv__w-del {
  position: absolute;
  top: -10rpx;
  right: -10rpx;
  width: 44rpx;
  height: 44rpx;
  border-radius: 50%;
  background: var(--danger);
  z-index: 10;
  display: flex;
  align-items: center;
  justify-content: center;
  box-shadow: 0 6rpx 14rpx rgba(198, 40, 40, 0.32);
}
.cv__w-del-minus {
  color: #FFFFFF;
  font-size: 34rpx;
  line-height: 1;
  font-weight: 400;
}

/* 卡片标题：左侧蓝色短条 */
.card__title {
  display: flex;
  align-items: center;
  justify-content: space-between;
  font-size: 32rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-bottom: 24rpx;
  position: relative;
  padding-left: 22rpx;
}
.card__title::before {
  content: '';
  position: absolute;
  left: 0;
  top: 50%;
  transform: translateY(-50%);
  width: 8rpx;
  height: 30rpx;
  border-radius: 4rpx;
  background: var(--brand);
}
.cv__scene-add {
  margin-left: auto;
  width: 48rpx;
  height: 48rpx;
  border-radius: 50%;
  background: rgba(36, 94, 112, 0.10);
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
  transition: transform 160ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__scene-add--press {
  transform: scale(0.88);
}
.cv__scene-add-bar {
  position: absolute;
  width: 22rpx;
  height: 3rpx;
  border-radius: 2rpx;
  background: var(--brand);
}
.cv__scene-add-bar--v {
  transform: rotate(90deg);
}

/* ---------- 场景：两列蓝色磁贴 ---------- */
.cv__scene-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20rpx;
}
.cv__scene-tile {
  position: relative;
  min-height: 200rpx;
  display: flex;
  flex-direction: column;
  padding: 26rpx;
  border-radius: 28rpx;
  background: linear-gradient(145deg, var(--bg-card), var(--bg-card-end));
  border: 1rpx solid var(--border-light);
  box-shadow: 0 8rpx 20rpx rgba(96, 81, 51, 0.06);
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1), box-shadow 200ms ease, background 200ms ease;
}
.cv__scene-tile--press {
  transform: scale(0.97);
}
/* 当前生效的场景（服务端 active_scene_id）才是实心青墨，其余一律浅底次级 */
.cv__scene-tile--on {
  background: var(--brand-grad);
  border-color: transparent;
  box-shadow: 0 14rpx 28rpx rgba(36, 94, 112, 0.26);
}
.cv__scene-tile-ic {
  width: 64rpx;
  height: 64rpx;
  border-radius: 20rpx;
  background: rgba(36, 94, 112, 0.10);
  color: var(--brand);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}
.cv__scene-tile--on .cv__scene-tile-ic {
  background: rgba(255, 255, 255, 0.22);
  color: #FFFFFF;
}
/* 场景符号：纯 CSS 描边（电源 / 离开箭头），替掉 emoji 感的图标 */
.cv__glyph {
  position: relative;
  width: 26rpx;
  height: 26rpx;
}
.cv__glyph--power {
  border: 2rpx solid currentColor;
  border-top-color: transparent;
  border-radius: 50%;
}
.cv__glyph--power::before {
  content: '';
  position: absolute;
  top: -6rpx;
  left: 50%;
  transform: translateX(-50%);
  width: 2rpx;
  height: 12rpx;
  background: currentColor;
}
.cv__glyph--out::before {
  content: '';
  position: absolute;
  top: 50%;
  left: 1rpx;
  width: 20rpx;
  height: 2rpx;
  transform: translateY(-50%);
  background: currentColor;
}
.cv__glyph--out::after {
  content: '';
  position: absolute;
  top: 50%;
  right: 1rpx;
  width: 9rpx;
  height: 9rpx;
  border-top: 2rpx solid currentColor;
  border-right: 2rpx solid currentColor;
  transform: translateY(-50%) rotate(45deg);
}
.cv__scene-tile-name {
  margin-top: auto;
  color: var(--text-1);
  font-size: 30rpx;
  font-weight: 600;
  line-height: 1.4;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__scene-tile-desc {
  margin-top: 6rpx;
  color: var(--text-3);
  font-size: 22rpx;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__scene-tile--on .cv__scene-tile-name {
  color: #FFFFFF;
}
.cv__scene-tile--on .cv__scene-tile-desc {
  color: rgba(255, 255, 255, 0.72);
}
.cv__scene-tile--add {
  background: var(--bg-card);
  border: 2rpx dashed rgba(36, 94, 112, 0.32);
  box-shadow: none;
  align-items: center;
  justify-content: center;
}
.cv__scene-tile-ic--add {
  background: rgba(36, 94, 112, 0.10);
}
.cv__scene-tile-name--add {
  margin-top: 16rpx;
  color: var(--brand);
}

/* ---------- 开关设备：两列磁贴（开 = 蓝卡，关 = 白卡） ---------- */
.cv__dev-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20rpx;
}
.cv__dev-card {
  display: flex;
  flex-direction: column;
  min-height: 196rpx;
  padding: 24rpx;
  border-radius: 28rpx;
  background: var(--bg-card);
  border: 1rpx solid var(--border);
  box-shadow: 0 6rpx 16rpx rgba(96, 81, 51, 0.05);
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__dev-card--press {
  transform: scale(0.97);
}
.cv__dev-card--on {
  background: var(--brand-grad);
  border-color: transparent;
  box-shadow: 0 14rpx 28rpx rgba(36, 94, 112, 0.26);
}
.cv__dev-card--off {
  opacity: 0.5;
}
.cv__dev-card-top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12rpx;
}
.cv__dev-card-ic {
  width: 60rpx;
  height: 60rpx;
  border-radius: 18rpx;
  background: var(--bg-soft);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}
.cv__dev-card--on .cv__dev-card-ic {
  background: rgba(255, 255, 255, 0.22);
}
.cv__dev-tgl {
  position: relative;
  width: 76rpx;
  height: 44rpx;
  border-radius: 999rpx;
  background: var(--line-strong);
  transition: background 240ms;
  flex-shrink: 0;
}
.cv__dev-tgl-knob {
  position: absolute;
  top: 4rpx;
  left: 4rpx;
  width: 36rpx;
  height: 36rpx;
  border-radius: 50%;
  background: var(--bg-card);
  box-shadow: 0 2rpx 6rpx rgba(0, 0, 0, 0.18);
  transition: transform 240ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__dev-tgl--on {
  background: rgba(255, 255, 255, 0.34);
}
.cv__dev-tgl--on .cv__dev-tgl-knob {
  transform: translateX(32rpx);
}
.cv__dev-card-name {
  margin-top: auto;
  font-size: 28rpx;
  font-weight: 600;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__dev-card--on .cv__dev-card-name {
  color: #FFFFFF;
}
.cv__dev-card-sub {
  margin-top: 8rpx;
  font-size: 22rpx;
  color: var(--text-3);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__dev-card--on .cv__dev-card-sub {
  color: rgba(255, 255, 255, 0.74);
}

/* ---------- 环境面板：设备行 + 运行指标条 ---------- */
.cv__panel-item {
  margin-bottom: 12rpx;
}
.cv__dev-row {
  display: flex;
  align-items: center;
  gap: 20rpx;
  min-height: 120rpx;
  padding: 20rpx 0;
  border-bottom: 1rpx solid var(--bg-soft);
  transition: opacity 200ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__dev-row:last-child {
  border-bottom: none;
}
.cv__dev-row--off {
  opacity: 0.55;
}
.cv__dev-icon {
  width: 72rpx;
  height: 72rpx;
  border-radius: 20rpx;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  background: var(--bg-soft);
}
.cv__dev-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 8rpx;
}
.cv__dev-name {
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-1);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__dev-sub {
  font-size: 24rpx;
  color: var(--text-3);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.cv__dev-switch {
  transform: scale(0.7);
  transform-origin: right center;
  flex-shrink: 0;
}
.cv__panel-info {
  background: var(--bg-soft);
  border-radius: 18rpx;
  padding: 18rpx 24rpx;
  margin-top: 12rpx;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 12rpx 40rpx;
}
.cv__panel-cell {
  display: flex;
  align-items: baseline;
}
.cv__panel-k {
  font-size: 22rpx;
  color: var(--text-3);
  margin-right: 10rpx;
  text-transform: uppercase;
}
.cv__panel-v {
  font-size: 26rpx;
  font-weight: 500;
  color: var(--text-1);
}

/* 卡片眉标：caps 英文小标 + 右侧数据量（统一版式语法） */
.cv__card-eyebrow {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin: -8rpx 0 4rpx;
}
.cv__card-caps {
  font-size: 18rpx;
  letter-spacing: 3rpx;
  color: var(--text-4);
}
.cv__card-count {
  font-size: 20rpx;
  color: var(--text-3);
}

/* ---------- 空间概览：三行分区 + 在线点阵 ---------- */
.cv__zones {
  margin: 6rpx 32rpx 0;
  padding: 22rpx 26rpx;
  border-radius: 26rpx;
  border: 1rpx solid var(--border-light);
  background: linear-gradient(145deg, var(--bg-card), var(--bg-card-end));
  box-shadow: 0 10rpx 24rpx rgba(96, 81, 51, 0.06);
}
.cv__zones-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
}
.cv__zones-title-wrap {
  display: flex;
  align-items: baseline;
  gap: 12rpx;
}
.cv__zones-caps {
  font-size: 18rpx;
  letter-spacing: 3rpx;
  color: var(--text-4);
}
.cv__zones-title {
  font-size: 26rpx;
  font-weight: 600;
  letter-spacing: 1rpx;
  color: var(--text-1);
}
.cv__zones-sub {
  font-size: 22rpx;
  color: var(--text-3);
}
.cv__zone-row {
  display: flex;
  align-items: center;
  gap: 16rpx;
  margin-top: 10rpx;
  padding: 10rpx 14rpx;
  border-radius: 16rpx;
  background: var(--zone-face);
}
.cv__zone-label {
  width: 150rpx;
  flex-shrink: 0;
  font-size: 22rpx;
  color: var(--text-2);
}
.cv__zone-dots {
  flex: 1;
  min-width: 0;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8rpx;
}
.cv__zone-dot {
  width: 14rpx;
  height: 14rpx;
  border-radius: 50%;
  border: 1rpx solid var(--text-4);
  background: transparent;
}
.cv__zone-dot--on {
  border-color: var(--success);
  background: var(--success);
}
.cv__zone-more {
  font-size: 20rpx;
  color: var(--text-3);
}
.cv__zone-count {
  flex-shrink: 0;
  font-size: 22rpx;
  color: var(--text-3);
}

/* ---------- 环境读数：主卡圆环仪表 + 辅卡数值条（读数分主次） ---------- */
.cv__gauge-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 20rpx;
}
.cv__gauge {
  position: relative;
  background: linear-gradient(145deg, var(--bg-card), var(--bg-card-end));
  border-radius: 26rpx;
  border: 1rpx solid var(--border-light);
  box-shadow: 0 8rpx 20rpx rgba(96, 81, 51, 0.06);
  padding: 22rpx 24rpx;
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__gauge--press {
  transform: scale(0.97);
}
/* 主卡：占满两列，圆环 + 文案横排 */
.cv__gauge--lead {
  grid-column: span 2;
  display: flex;
  align-items: center;
  gap: 28rpx;
  padding: 26rpx 28rpx;
}
/* 辅卡落单时占满两列，避免右侧留空洞 */
.cv__gauge--wide {
  grid-column: span 2;
  padding: 24rpx 28rpx;
}
.cv__gauge--wide .cv__gauge-num {
  font-size: 48rpx;
}
.cv__gauge-arc {
  position: relative;
  width: 168rpx;
  height: 168rpx;
  border-radius: 50%;
  flex-shrink: 0;
  background-color: var(--gauge-track);
  background-image: conic-gradient(from 180deg, var(--gauge-start), var(--gauge-end) var(--p), var(--gauge-track) var(--p));
  box-shadow: 0 8rpx 22rpx rgba(36, 94, 112, 0.14);
  display: flex;
  align-items: center;
  justify-content: center;
}
.cv__gauge-hole {
  width: 132rpx;
  height: 132rpx;
  border-radius: 50%;
  background: var(--bg-card);
  border: 1rpx solid var(--border-light);
  box-shadow: inset 0 2rpx 8rpx rgba(96, 81, 51, 0.06);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
}
.cv__gauge-lead-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.cv__gauge-label {
  font-size: 26rpx;
  font-weight: 600;
  color: var(--text-2);
}
.cv__gauge-lead-sub {
  margin-top: 10rpx;
  font-size: 22rpx;
  line-height: 1.5;
  color: var(--text-3);
}
.cv__gauge-lead-sub--alarm {
  color: var(--danger);
}
/* 辅卡：标签 + 数值 + 4rpx 进度条 */
.cv__gauge-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.cv__gauge-warn {
  display: flex;
  align-items: center;
}
.cv__gauge-value {
  display: flex;
  align-items: flex-end;
  gap: 6rpx;
  margin-top: 12rpx;
}
.cv__gauge-num {
  font-size: 44rpx;
  font-weight: 700;
  line-height: 1.1;
  color: var(--text-1);
}
.cv__gauge--lead .cv__gauge-num {
  font-size: 46rpx;
  color: var(--brand);
}
/* 告警红必须压过主卡的品牌色（同权重、后出现者胜） */
.cv__gauge .cv__gauge-num--alarm {
  color: var(--danger);
}
.cv__gauge-unit {
  font-size: 22rpx;
  font-weight: 400;
  color: var(--text-3);
  line-height: 1.5;
}
.cv__gauge-bar {
  height: 4rpx;
  border-radius: 2rpx;
  background: var(--gauge-track);
  margin-top: 16rpx;
  overflow: hidden;
}
.cv__gauge-bar-fill {
  height: 100%;
  border-radius: 2rpx;
  background: var(--brand-grad);
  transition: width 220ms linear;
}
/* 晴昼 1.4.2 §三：数值收敛时的微光呼吸（对齐 qingzhou .qd142-num-flash） */
.cv__gauge-num--flash {
  animation: qd142-num-glow 550ms ease-out;
}
@media (prefers-reduced-motion: reduce) {
  .cv__gauge-num--flash {
    animation: none;
  }
}
/* 只在 35% 给值：0% / 100% 隐式取基础值，告警红等基础色能自然回位 */
@keyframes qd142-num-glow {
  35% {
    text-shadow: 0 0 12rpx var(--brand);
    color: var(--brand);
  }
}

/* 空状态行 */
.cv__empty-line {
  text-align: center;
  color: var(--text-3);
  font-size: 26rpx;
  padding: 24rpx 0;
}

/* ---------- 底部蓝色横幅 ---------- */
.cv__banner {
  display: flex;
  align-items: center;
  gap: 20rpx;
  margin: 0 32rpx 24rpx;
  padding: 30rpx 32rpx;
  border-radius: 36rpx;
  border: 1rpx solid var(--border-light);
  background: linear-gradient(140deg, var(--bg-card), var(--bg-card-end));
  box-shadow: var(--shadow-panel);
  transition: transform 150ms cubic-bezier(0.25, 0.1, 0.25, 1);
}
.cv__banner--press {
  transform: scale(0.98);
}
.cv__banner-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 8rpx;
}
.cv__banner-title {
  color: var(--text-1);
  font-size: 32rpx;
  font-weight: 600;
}
.cv__banner-desc {
  color: var(--text-3);
  font-size: 24rpx;
}
/* 全页唯一的实心青墨块：作为「去操作」的行动焦点 */
.cv__banner-btn {
  width: 68rpx;
  height: 68rpx;
  border-radius: 50%;
  background: var(--brand);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  box-shadow: 0 10rpx 22rpx rgba(36, 94, 112, 0.28);
}

/* ---------- 添加卡片入口 ---------- */
.cv__add-card {
  margin: 0 32rpx 24rpx;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 14rpx;
  padding: 40rpx 0;
  border: 2rpx dashed rgba(36, 94, 112, 0.28) !important;
  background: var(--bg-card) !important;
  box-shadow: none !important;
}
.cv__add-ring {
  width: 72rpx;
  height: 72rpx;
  border-radius: 50%;
  border: 2rpx dashed rgba(36, 94, 112, 0.34);
  background: transparent;
  display: flex;
  align-items: center;
  justify-content: center;
}
.cv__add-plus {
  font-size: 42rpx;
  color: var(--brand);
  font-weight: 400;
  line-height: 1;
}
.cv__add-text {
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
}

/* ---------- 自定义卡片按钮 ---------- */
.cv__edit-bar {
  display: flex;
  justify-content: center;
  margin: 8rpx 32rpx 0;
}
.cv__edit-action {
  display: flex;
  align-items: center;
  justify-content: center;
  height: 76rpx;
  padding: 0 44rpx;
  border-radius: 999rpx;
  background: var(--brand);
  color: #FFFFFF;
  font-size: 28rpx;
  font-weight: 600;
  border: none;
  box-shadow: 0 12rpx 24rpx rgba(36, 94, 112, 0.26);
}

/* ---------- 弹层：添加卡片 ---------- */
.cv__mask {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(17, 24, 39, 0.42);
  z-index: 900;
  display: flex;
  align-items: flex-end;
}
.cv__sheet {
  background: var(--bg-card);
  border-radius: 32rpx 32rpx 0 0;
  padding: 32rpx 32rpx calc(32rpx + env(safe-area-inset-bottom));
  width: 100%;
  max-height: 78vh;
  overflow-y: auto;
}
.cv__sheet-title {
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
  margin-bottom: 16rpx;
}
.cv__opt {
  display: flex;
  align-items: center;
  gap: 20rpx;
  padding: 24rpx 0;
  border-bottom: 1rpx solid var(--bg-soft);
}
.cv__opt--off {
  opacity: 0.5;
}
.cv__opt-main {
  flex: 1;
  display: flex;
  flex-direction: column;
  min-width: 0;
}
.cv__opt-name {
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-1);
}
.cv__opt-desc {
  font-size: 22rpx;
  color: var(--text-3);
  margin-top: 4rpx;
}
.cv__opt-state {
  font-size: 24rpx;
  font-weight: 400;
  color: var(--text-3);
}
.cv__opt--on .cv__opt-state {
  color: var(--brand);
}
.cv__sheet-cancel {
  text-align: center;
  margin-top: 20rpx;
  padding: 20rpx 0;
  color: var(--text-2);
  font-size: 28rpx;
  font-weight: 500;
  background: var(--bg-soft);
  border-radius: 999rpx;
}

/* ---------- 弹层：场景编辑 ---------- */
.mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  z-index: 1000;
  background: rgba(17, 24, 39, 0.45);
}
.sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1001;
  background: var(--bg-card);
  border-radius: 32rpx 32rpx 0 0;
  padding: 32rpx 32rpx 0;
  transform: translateY(100%);
  transition: transform 280ms cubic-bezier(0.25, 0.1, 0.25, 1);
  max-height: 85vh;
  overflow-y: auto;
}
.sheet.show {
  transform: translateY(0);
}
.sheet-footer {
  position: sticky;
  bottom: 0;
  background: var(--bg-card);
  padding: 16rpx 0 calc(24rpx + env(safe-area-inset-bottom));
}
.sheet-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 16rpx;
}
.sheet-title {
  font-size: 34rpx;
  font-weight: 600;
  color: var(--text-1);
}
.sheet-close {
  display: flex;
  align-items: center;
  padding: 8rpx;
}
.field-row {
  margin-bottom: 20rpx;
}
.field-label {
  display: block;
  margin-bottom: 12rpx;
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
}
.field {
  height: 92rpx;
  padding: 0 28rpx;
  background: var(--bg-soft);
  border: 1rpx solid var(--border);
  border-radius: 20rpx;
  font-size: 28rpx;
}
.ph {
  color: var(--text-4);
}
.section-title {
  margin-top: 16rpx;
}
.section-title .t {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-2);
}
.sd-hint {
  display: block;
  margin: 4rpx 0 8rpx;
  font-size: 24rpx;
  font-weight: 400;
  color: var(--text-3);
}
.scene-device-list .list-cell {
  padding: 12rpx 0;
}
.sd-label {
  flex: 1;
  font-size: 28rpx;
  font-weight: 400;
  color: var(--text-1);
}
.sd-empty {
  display: block;
  padding: 24rpx 0;
  font-size: 26rpx;
  color: var(--text-3);
  text-align: center;
}
.btn {
  margin-top: 8rpx;
}
.btn-primary {
  background: var(--brand);
  color: #FFFFFF;
  box-shadow: 0 12rpx 24rpx rgba(36, 94, 112, 0.24);
}
.btn-primary[disabled] {
  background: rgba(36, 94, 112, 0.14);
  color: var(--text-3);
  box-shadow: none;
}
.btn-primary::after {
  border: none;
}
</style>
