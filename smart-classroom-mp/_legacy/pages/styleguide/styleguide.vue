<template>
  <view class="page">
    <nav-bar title="组件总览" theme="white" :back="true" />

    <!-- ① 设计 Token：色板 -->
    <view class="sg-hint">品牌色 · 语义色 · 点缀色全部取自 uni.scss，组件零硬编码</view>
    <view class="card sg-card">
      <view class="sg-row">
        <view v-for="c in brandColors" :key="c.name" class="swatch">
          <view class="swatch__c" :style="{ background: c.value }" />
          <text class="swatch__n">{{ c.name }}</text>
        </view>
      </view>
      <view class="sg-row">
        <view v-for="c in semColors" :key="c.name" class="swatch">
          <view class="swatch__c" :style="{ background: c.value }" />
          <text class="swatch__n">{{ c.name }}</text>
        </view>
      </view>
      <view class="sg-row">
        <view v-for="c in envColors" :key="c.name" class="swatch">
          <view class="swatch__c swatch__c--sm" :style="{ background: c.value }" />
          <text class="swatch__n">{{ c.name }}</text>
        </view>
      </view>
    </view>

    <!-- ② 状态圆点 -->
    <view class="sg-title">状态圆点</view>
    <view class="card sg-card sg-dots">
      <status-dot status="online" />
      <status-dot status="reconnecting" />
      <status-dot status="offline" />
    </view>

    <!-- ③ 开关 -->
    <view class="sg-title">开关 · 点击进入 pending（900ms 模拟回执）</view>
    <view class="card sg-card">
      <view class="line">
        <text class="line__t">可交互开关（点击体验手感）</text>
        <ui-switch :value="demoSwitch" :pending="demoPending" @switch="onDemoSwitch" />
      </view>
      <view class="line">
        <text class="line__t">开启态</text>
        <ui-switch :value="true" />
      </view>
      <view class="line">
        <text class="line__t">pending（位置不动 + 转圈）</text>
        <ui-switch :value="true" :pending="true" />
      </view>
      <view class="line line--last">
        <text class="line__t">离线置灰（不可点）</text>
        <ui-switch :value="false" :disabled="true" />
      </view>
    </view>

    <!-- ④ 设备列表项 -->
    <view class="sg-title">设备列表项 · 离线行点击有 toast</view>
    <view class="sg-pad">
      <device-row
        v-for="d in demoDevices"
        :key="d.device_id"
        :device="d"
        @toggle="onDeviceToggle"
        @offline-tap="onOfflineTap"
      />
    </view>

    <!-- ⑤ 环境指标卡（实时刷新演示，观察数字不抖动） -->
    <view class="sg-title">
      环境卡 · 数值每 2.5s 静默更新（高亮闪动 / 固定宽度不跳），人体感应不支持故不渲染
    </view>
    <view class="sg-envgrid">
      <env-card
        v-for="m in metrics"
        :key="m.key"
        :metric-key="m.key"
        :value="m.value"
        :alarm="m.alarm"
      />
    </view>

    <!-- ⑥ 告警卡 -->
    <view class="sg-title">告警卡片 · 三种状态</view>
    <view class="sg-pad">
      <alarm-card :alarm="a" v-for="a in alarms" :key="a.id" />
    </view>

    <!-- ⑦ 弹窗 -->
    <view class="sg-title">二次确认弹窗（全局禁用 alert/confirm）</view>
    <view class="card sg-card sg-btns">
      <view class="c-btn c-btn--ghost sg-btn" hover-class="press" @tap="showNormal = true">
        普通确认（场景设备清单）
      </view>
      <view class="c-btn c-btn--danger sg-btn" hover-class="press" @tap="showDanger = true">
        危险确认（强制开门）
      </view>
    </view>

    <!-- ⑧ 骨架屏 -->
    <view class="sg-title">骨架屏 · 形状/数量与真实布局一致</view>
    <view class="card sg-card sg-btns">
      <view
        v-for="t in skTypes"
        :key="t.key"
        class="c-btn c-btn--ghost sg-btn"
        hover-class="press"
        @tap="toggleSk(t.key)"
      >{{ skOn[t.key] ? '隐藏' : '查看' }}{{ t.label }}骨架</view>
    </view>

    <skeleton v-if="skOn.env" type="env" :count="4" />
    <skeleton v-if="skOn.device" type="device" :count="3" />
    <skeleton v-if="skOn.alarm" type="alarm" :count="3" />

    <!-- ⑨ 图标总览 -->
    <view class="sg-title">本地 SVG 线性图标（选中态切换填充版）</view>
    <view class="card sg-card sg-icons">
      <view v-for="n in iconNames" :key="n" class="iconcell">
        <app-icon :name="n" :size="40" />
        <text class="iconcell__t">{{ n }}</text>
      </view>
    </view>

    <!-- TabBar 预览控制台 -->
    <view class="sg-title">自定义 TabBar（页面底部为实时预览）</view>
    <view class="card sg-card sg-btns">
      <view class="sg-seg">
        <view
          class="sg-seg__item" :class="{ on: role === 'teacher' }"
          hover-class="press" @tap="switchRole('teacher')"
        >教师 3 tab</view>
        <view
          class="sg-seg__item" :class="{ on: role === 'admin' }"
          hover-class="press" @tap="switchRole('admin')"
        >管理员 4 tab</view>
      </view>
      <view class="sg-seg">
        <view class="c-btn c-btn--ghost sg-btn" hover-class="press" @tap="decBadge">角标 -1</view>
        <view class="c-btn c-btn--ghost sg-btn" hover-class="press" @tap="incBadge">角标 +1（试 99+）</view>
      </view>
    </view>

    <view style="height: 260rpx" />

    <!-- 底部实时 TabBar（emit 模式，切换不重载） -->
    <tab-bar :role="role" :current="tabCurrent" :badge="{ alarm: badge }" @change="onTabChange" />

    <!-- 角色切换：整页白底转场遮盖，tabBar 在转场期间完成替换 -->
    <view v-if="covering" class="cover">
      <text class="cover__t">智慧教室</text>
    </view>

    <!-- 普通确认弹窗 -->
    <confirm-modal
      :visible="showNormal"
      title="切换到上课模式？"
      header-icon="play"
      confirm-text="确认切换"
      @confirm="showNormal = false"
      @cancel="showNormal = false"
    >
      <view class="chk">
        <text class="chk__desc">以下设备将被联动控制：</text>
        <view v-for="d in checklist" :key="d.device_id" class="chk__item">
          <view class="chk__icon">
            <app-icon :name="d.icon" :size="34" color="#2F6BFF" />
          </view>
          <text class="chk__name">{{ d.name }}</text>
          <text class="tag tag--blue">{{ d.action }}</text>
        </view>
      </view>
    </confirm-modal>

    <!-- 危险确认弹窗：红色图标 + 红色按钮 + 后果文案 + 遮罩不可误关 -->
    <confirm-modal
      :visible="showDanger"
      title="确认强制开门？"
      danger
      header-icon="warning"
      :mask-closable="false"
      confirm-text="强制开门"
      content="A101 教室门锁将立即解除。教室内人员与设备将处于无门禁保护状态，操作会被记录。"
      @confirm="showDanger = false"
      @cancel="showDanger = false"
    />
  </view>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted } from 'vue'

/* ---------- 开关手感演示 ---------- */
const demoSwitch = ref(false)
const demoPending = ref(false)
function onDemoSwitch(next) {
  demoPending.value = true
  setTimeout(() => { demoPending.value = false; demoSwitch.value = next }, 900)
}

/* ---------- 设备行演示 ---------- */
const demoDevices = reactive([
  { device_id: 'screen-01', device_type: 'screen', label: '综合屏 A101-01', online: true, state: 'on', sub: '3 分钟前上报' },
  { device_id: 'ac-01', device_type: 'ac', label: '中央空调', online: true, state: 'off', sub: '刚刚上报' },
  { device_id: 'projector-01', device_type: 'projector', label: '投影仪', online: false, state: 'off', sub: '2 小时前上报' },
])
function onDeviceToggle({ device, next }) {
  device.pending = true
  setTimeout(() => {
    device.pending = false
    device.state = next ? 'on' : 'off'
    device.sub = '刚刚上报'
  }, 900)
}
function onOfflineTap() {
  uni.showToast({ title: '设备离线，暂不可控', icon: 'none' })
}

/* ---------- 环境卡实时刷新演示 ---------- */
const metrics = reactive([
  { key: 'temperature', value: '24.6', alarm: false },
  { key: 'humidity', value: '52', alarm: false },
  { key: 'co2', value: '612', alarm: false },
  { key: 'light', value: '320', alarm: false },
  { key: 'pm25', value: '18', alarm: false },
])
let timer = null
onMounted(() => {
  timer = setInterval(() => {
    metrics[0].value = (23 + Math.random() * 2).toFixed(1)
    metrics[1].value = String(Math.round(48 + Math.random() * 10))
    const co2 = Math.round(500 + Math.random() * 520)
    metrics[2].value = String(co2)
    metrics[2].alarm = co2 > 800
    metrics[3].value = String(Math.round(280 + Math.random() * 120))
    metrics[4].value = String(Math.round(12 + Math.random() * 18))
  }, 2500)
})
onUnmounted(() => clearInterval(timer))

/* ---------- 告警演示 ---------- */
const alarms = [
  { id: 1, status: 'unhandled', type: 'warning', title: 'CO₂ 浓度超标', room: 'A101 教室', desc: 'CO₂ 持续 10 分钟高于 800ppm，当前 926ppm', time: '10:24', notes: [{ text: '短信已送达', tone: 'blue' }] },
  { id: 2, status: 'processing', type: 'sensor', title: '设备离线', room: 'A203 教室', desc: '投影仪已离线超过 30 分钟，正在尝试远程唤醒', time: '09:41', notes: [{ text: '30 分钟冷却期内已合并 2 条', tone: 'orange' }] },
  { id: 3, status: 'resolved', type: 'lock', title: '门未关提醒', room: 'A101 教室', desc: '放学后教室门未关闭，已由管理员现场处理', time: '昨天 18:02', notes: [{ text: '已处理', tone: 'gray' }] },
]

/* ---------- 弹窗 ---------- */
const showNormal = ref(false)
const showDanger = ref(false)
const checklist = [
  { device_id: 1, icon: 'bulb', name: '教室灯光（8 路灯控）', action: '开启' },
  { device_id: 2, icon: 'screen', name: '综合屏', action: '唤醒' },
  { device_id: 3, icon: 'projector', name: '投影仪', action: '开启' },
]

/* ---------- 骨架屏 ---------- */
const skTypes = [
  { key: 'env', label: '环境卡' },
  { key: 'device', label: '设备列表' },
  { key: 'alarm', label: '告警列表' },
]
const skOn = reactive({ env: false, device: false, alarm: false })
function toggleSk(k) { skOn[k] = !skOn[k] }

/* ---------- 色板 ---------- */
const brandColors = [
  { name: '品牌蓝', value: '#2F6BFF' },
  { name: '主色浅底', value: '#EBF1FF' },
  { name: '青绿', value: '#16B98A' },
  { name: '青绿浅底', value: '#E6F8F2' },
]
const semColors = [
  { name: '成功', value: '#16B98A' },
  { name: '警告', value: '#FF8A34' },
  { name: '危险', value: '#F5483B' },
  { name: '离线', value: '#9AA3B2' },
]
const envColors = [
  { name: '温度', value: '#FF7A45' },
  { name: '湿度', value: '#2F9BFF' },
  { name: 'CO₂', value: '#8B6FE8' },
  { name: '光照', value: '#F5B63D' },
  { name: 'PM2.5', value: '#5BBF9A' },
  { name: '人体', value: '#16B98A' },
]

/* ---------- 图标总览 ---------- */
const iconNames = [
  'home', 'home-fill', 'control', 'control-fill', 'bell', 'bell-fill',
  'grid', 'grid-fill', 'user', 'user-fill', 'back', 'chevron-right',
  'screen', 'bulb', 'ac', 'projector', 'sensor', 'lock', 'codec',
  'temperature', 'humidity', 'co2', 'sunlight', 'pm', 'motion',
  'face', 'card', 'warning', 'info', 'clock', 'logout', 'door-open',
]

/* ---------- TabBar 预览 ---------- */
const role = ref('teacher')
const tabCurrent = ref(0)
const badge = ref(3)
const covering = ref(false)

function onTabChange({ index }) { tabCurrent.value = index }
function incBadge() { badge.value = badge.value >= 99 ? 120 : badge.value + 1 }
function decBadge() { badge.value = Math.max(0, badge.value - 1) }
function switchRole(r) {
  if (r === role.value) return
  covering.value = true
  setTimeout(() => {
    role.value = r
    tabCurrent.value = 0
  }, 280)
  setTimeout(() => { covering.value = false }, 560)
}
</script>

<style lang="scss" scoped>
.page { min-height: 100vh; padding-bottom: 40rpx; }

.sg-title {
  margin: $sp-28 $page-pad 0;
  font-size: $fs-body-sm;
  color: $text-2;
  line-height: 1.5;
}
.sg-hint {
  margin: $sp-20 $page-pad 0;
  font-size: $fs-aux;
  color: $text-3;
  line-height: 1.5;
}
.sg-card { margin-top: $sp-16; }
.sg-pad { padding: $sp-20 $page-pad 0; }

/* 色板 */
.sg-row {
  display: flex;
  gap: $sp-16;
  margin-bottom: $sp-16;
  &:last-child { margin-bottom: 0; }
}
.swatch {
  flex: 1;
  @include flex(flex-start, flex-start, column);
  gap: 8rpx;
}
.swatch__c {
  width: 100%;
  height: 72rpx;
  border-radius: $r-icon;
  box-shadow: inset 0 0 0 1rpx rgba(31, 56, 122, 0.06);
}
.swatch__n { font-size: $fs-tag; color: $text-3; }

/* 状态圆点 */
.sg-dots {
  @include flex(space-around, center);
  .sd { gap: 10rpx; }
}

/* 行 */
.line {
  @include flex(space-between, center);
  min-height: 96rpx;
  border-bottom: 1rpx solid $line;
}
.line--last { border-bottom: none; }
.line__t { font-size: $fs-body; color: $text-2; }

/* 环境卡网格：2 列 */
.sg-envgrid {
  display: flex;
  flex-wrap: wrap;
  justify-content: space-between;
  padding: $sp-16 $page-pad 0;
}
.sg-envgrid > view { width: 334rpx; margin-bottom: $sp-20; }

/* 按钮组 */
.sg-btns {
  @include flex(center, stretch, column);
  gap: $sp-16;
}
.sg-btn { width: 100%; }

/* 图标总览 */
.sg-icons {
  display: flex;
  flex-wrap: wrap;
}
.iconcell {
  width: 25%;
  @include flex(center, center, column);
  gap: 8rpx;
  padding: $sp-16 0;
}
.iconcell__t { font-size: 18rpx; color: $text-3; }

/* 分段（预览页内用） */
.sg-seg {
  display: flex;
  background: $disabled-light;
  border-radius: $r-btn;
  padding: 6rpx;
  width: 100%;
  gap: 6rpx;
}
.sg-seg__item {
  flex: 1;
  height: 64rpx;
  line-height: 64rpx;
  text-align: center;
  font-size: $fs-aux;
  color: $text-2;
  border-radius: $r-btn;
  transition: all $t-toggle $ease-standard;
}
.sg-seg__item.on {
  background: #fff;
  color: $primary;
  font-weight: 600;
  box-shadow: 0 2rpx 8rpx rgba(31, 56, 122, 0.08);
}

/* 弹窗内设备清单 */
.chk {
  margin-top: $sp-20;
  background: $bg-page;
  border-radius: $r-icon;
  padding: $sp-20;
}
.chk__desc { font-size: $fs-aux; color: $text-3; }
.chk__item {
  @include flex(flex-start, center);
  gap: $sp-12;
  margin-top: $sp-16;
}
.chk__icon {
  width: 56rpx;
  height: 56rpx;
  border-radius: 12rpx;
  background: $primary-light;
  @include flex(center, center);
  flex: none;
}
.chk__name { flex: 1; font-size: $fs-body; color: $text-1; }

/* 角色切换整页遮盖 */
.cover {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 2000;
  background: $primary-gradient;
  @include flex(center, center);
  animation: sc-mask-in 280ms $ease-standard;
}
.cover__t {
  color: #fff;
  font-size: 44rpx;
  font-weight: 700;
  letter-spacing: 4rpx;
}
</style>
