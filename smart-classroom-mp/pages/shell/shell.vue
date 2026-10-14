<template>
  <view class="shell">
    <!-- 教师端 3 视图（v-show 缓存：切换不销毁、不重载、状态保留） -->
    <template v-if="isTeacherRole">
      <classroom-view v-show="cur === 'classroom'" ref="vClassroom" />
      <control-view v-show="cur === 'control'" ref="vControl" />
      <alarm-view v-show="cur === 'alarm'" ref="vAlarm" />
      <profile-view v-show="cur === 'mine'" ref="vMine" />
    </template>

    <!-- 管理员端 4 视图（control tab = P7 远程控制） -->
    <template v-else>
      <overview-view v-show="cur === 'overview'" ref="vOverview" />
      <alarm-view v-show="cur === 'alarm'" ref="vAlarm" />
      <remote-view v-show="cur === 'control'" ref="vControl" />
      <profile-view v-show="cur === 'mine'" ref="vMine" />
    </template>

    <tab-bar
      :role="role"
      :current="cur"
      :badge="{ alarm: store.pendingAlarmCount }"
      @change="onTabChange"
    />
  </view>
</template>

<script setup>
/**
 * 壳层页：全部 tab 页收敛为内部 v-show 视图
 * - 切 tab = 切显隐：不销毁页面、不重跑 onLoad、列表滚动状态保留 → 切换丝滑
 * - 视图协议：onEnter(tab 激活 / 页面 onShow) / onRefresh(下拉) / onLoadMore(触底)
 * - 下拉刷新与触底加载由壳层统一接收，派发给当前激活视图
 * - 子页面(告警详情/教室详情等)通过 gotoTab() → uni.$emit('shell:goto') 回切指定 tab
 */
import { ref, computed, nextTick } from 'vue'
import { onLoad, onShow, onReady, onUnload, onPullDownRefresh, onReachBottom } from '@dcloudio/uni-app'
import { store } from '../../store'
import { ensureWs } from '../../services/ws'
import { roleTabs, requireAuth } from '../../utils/permission'
import classroomView from '../../components/views/classroom-view.vue'
import controlView from '../../components/views/control-view.vue'
import alarmView from '../../components/views/alarm-view.vue'
import overviewView from '../../components/views/overview-view.vue'
import remoteView from '../../components/views/remote-view.vue'
import profileView from '../../components/views/profile-view.vue'

const vClassroom = ref(null)
const vControl = ref(null)
const vAlarm = ref(null)
const vOverview = ref(null)
const vMine = ref(null)

const role = computed(() => (store.user && store.user.role) || 'teacher')
const isTeacherRole = computed(() => role.value === 'teacher')
const tabs = computed(() => roleTabs(store.user))

const cur = ref('')

/* 当前激活视图实例（key → ref） */
function activeView() {
  const map = {
    classroom: vClassroom,
    control: vControl,
    alarm: vAlarm,
    overview: vOverview,
    mine: vMine,
  }
  const r = map[cur.value]
  return r ? r.value : null
}

function callActive(fn, extra) {
  const inst = activeView()
  if (inst && typeof inst[fn] === 'function') inst[fn](extra)
}

function switchTab(key, extra) {
  const valid = tabs.value.some((t) => t.key === key)
  const next = valid ? key : tabs.value[0].key
  const force = !!extra
  if (cur.value === next && !force) {
    if (force) nextTick(() => callActive('onEnter', extra))
    return
  }
  cur.value = next
  uni.pageScrollTo({ scrollTop: 0, duration: 0 }) // 切 tab 回到顶部
  nextTick(() => callActive('onEnter', extra))
}

function onTabChange({ key }) {
  switchTab(key)
}

/* 子页面回切：gotoTab() 派发 */
function onGoto(payload) {
  if (payload && payload.key) switchTab(payload.key, payload)
}

onLoad((options) => {
  if (!requireAuth()) return
  cur.value = (options && options.tab) || tabs.value[0].key
  uni.$on('shell:goto', onGoto)
})

/* 首次加载交给 onReady：onShow 早于子视图挂载（ref 尚未就绪），
   直接 callActive 会落空导致首屏一直停留在 loading。onReady 在首屏
   渲染完成后触发，ref 必定就绪；用 entered 标记避免与 onShow 重复拉取。 */
let entered = false

onShow(() => {
  if (!requireAuth()) return
  ensureWs()
  if (!entered) return
  callActive('onEnter')
})

onReady(() => {
  if (!requireAuth()) return
  entered = true
  callActive('onEnter')
})

onUnload(() => {
  uni.$off('shell:goto', onGoto)
})

onPullDownRefresh(() => {
  callActive('onRefresh')
})
onReachBottom(() => {
  callActive('onLoadMore')
})
</script>

<style lang="scss" scoped>
.shell { min-height: 100vh; }
</style>
