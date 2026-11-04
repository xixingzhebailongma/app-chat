/**
 * 环境读数「差值阻尼」—— 对齐《晴昼 1.4.2 交付说明》§三 与 qingzhou.html 的 animateNumber。
 *
 * 规则（交付说明逐条）：
 *   1. 数值变化时从「上一次显示值」滚到「新值」，**不回到 0 重滚**；
 *   2. 值未变化不重播（同值短路），动画结束才触发一次微光呼吸；
 *   3. 中途再次变值：从当前正在显示的值接上，不跳变，且不残留旧时间线；
 *   4. 页面隐藏（小程序 onHide）立刻终止，落终值交给页面自身处理；
 *   5. 时长刻意差异化：仪表类（温度/湿度/光照）400ms，弥散类（CO₂/PM2.5）1000ms。
 *      —— 这是调校过的有意差异，不要统一。
 *
 * 关键点（交付说明 §四 3.2）：上次值必须存在 **模块级** Map，不能挂在页面 data、也不能挂在
 * DOM/元素上。演示页里 `#environment` 整体 innerHTML 重写会让挂在元素上的上次值全部丢失，
 * 退化成每次从 0 重滚。小程序里的同构风险是「切教室 / 切 Tab / 页面重建导致 data 复位」，
 * 模块级 Map 与页面生命周期解耦，重进页面仍是「从旧值平滑过渡到新值」。
 *
 * 小程序逻辑层没有 requestAnimationFrame，用 setTimeout 固定 TICK(33ms ≈ 30fps) 逐帧回调，
 * 由页面 setData 更新文案。
 */

/** key -> 当前「已显示」的数值（逐帧更新，便于中途改目标时无跳变地接上） */
const LAST = new Map()
/** key -> 正在跑的帧定时器 */
const TIMERS = new Map()

/** 帧间隔：≈30fps，兼顾流畅度与 setData 压力 */
export const TICK = 33
/** 微光呼吸时长，需与页面 CSS 的 animation-duration 一致 */
export const FLASH_MS = 550

/** 各指标时长（仪表快 / 弥散慢，刻意差异化） */
export const DURATION = {
  temperature: 400,
  humidity: 400,
  light: 400,
  co2: 1000,
  pm25: 1000,
}

export const DEFAULT_DURATION = 400

export function durationOf(key) {
  const d = DURATION[key]
  return typeof d === 'number' ? d : DEFAULT_DURATION
}

/**
 * 清空全部上次值。
 * 仅供「强制重播」这类演示入口使用；正常刷新/切页**不要**调用，
 * 否则会丢掉上一次读数，退化成首帧直接落值。当前小程序内没有强制重播入口，
 * 因此没有任何地方调用它 —— 保留是为了与交付说明 3-3 的接口对齐。
 */
export function clearLastVals() {
  LAST.clear()
}

/** 读取某个指标当前记录的上次值（无记录返回 undefined），主要用于自检与单测 */
export function lastValue(key) {
  return LAST.get(key)
}

/** 取消某个指标正在跑的时间线（保留它当前显示值作为下次的起点） */
export function cancelDamp(key) {
  const timer = TIMERS.get(key)
  if (timer) {
    clearTimeout(timer)
    TIMERS.delete(key)
  }
}

/** 取消全部时间线 */
export function cancelAllDamp() {
  TIMERS.forEach((timer) => clearTimeout(timer))
  TIMERS.clear()
}

/**
 * 把某个指标从上次显示值阻尼到 target。
 * @param {string} key        指标键（temperature / humidity / co2 / light / pm25 …）
 * @param {number} target     目标值
 * @param {object} [options]
 * @param {number} [options.duration] 时长；缺省按 DURATION 表，再缺省 400ms
 * @param {(value:number, final:boolean)=>void} [options.onFrame]
 *        每帧回调，final=true 表示已收敛（页面据此触发微光呼吸）
 * @returns {Function} cancel 闭包（等价于演示页里的 token 守卫）
 */
export function dampValue(key, target, options) {
  const opts = options || {}
  const onFrame = typeof opts.onFrame === 'function' ? opts.onFrame : function () {}
  const duration = opts.duration == null ? durationOf(key) : opts.duration
  const value = Number(target)

  // 缺值 / 非数值（NaN / Infinity）不接受，保持现状
  // （注意 Number(null) === 0、Number('') === 0，必须先挡掉空值再判有限性）
  if (target === null || target === undefined || target === '' || !isFinite(value)) {
    return function () {}
  }

  // 同一指标只允许一条时间线：中途改目标从「当前显示值」接上，避免两条定时器互相打架
  cancelDamp(key)

  const prev = LAST.get(key)
  const has = typeof prev === 'number' && isFinite(prev)

  // 首次出现 / 值未变 / 不需要动画 → 直接落终值，不滚动、不触发微光
  if (!has || prev === value || !(duration > 0)) {
    LAST.set(key, value)
    onFrame(value, false)
    return function () {}
  }

  const from = prev
  const start = Date.now()
  let cancelled = false

  // 首帧同步落地：保证 watcher 触发的这一拍渲染出来的是「旧值」，
  // 否则目标值会先闪现一帧再被打回去（交付说明：首帧不得为 0、不得先跳后滚）
  onFrame(from, false)

  const step = function () {
    if (cancelled) return
    const elapsed = Date.now() - start
    const t = elapsed >= duration ? 1 : elapsed / duration
    // easeOutCubic：与演示页 1 - Math.pow(1 - t, 3) 一致
    const eased = 1 - Math.pow(1 - t, 3)
    const cur = t >= 1 ? value : from + (value - from) * eased
    LAST.set(key, cur)
    if (t >= 1) {
      LAST.set(key, value)
      TIMERS.delete(key)
      onFrame(value, true)
      return
    }
    onFrame(cur, false)
    TIMERS.set(key, setTimeout(step, TICK))
  }

  TIMERS.set(key, setTimeout(step, TICK))

  return function cancel() {
    cancelled = true
    cancelDamp(key)
  }
}

export default { dampValue, cancelDamp, cancelAllDamp, clearLastVals, lastValue, durationOf, DURATION, TICK, FLASH_MS }
