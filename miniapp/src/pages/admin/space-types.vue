<template>
  <view class="types page-pad">
    <!-- 标题栏 -->
    <view class="section-title">
      <text class="t">空间类型</text>
      <text class="more more-action" @tap="openCreate">+ 新增类型</text>
    </view>
    <text class="hint">开关控制类型启停；「停用」即删除，已关联教室不受影响</text>

    <!-- 类型列表 -->
    <view v-if="types.length" class="list-group">
      <view v-for="(t, i) in types" :key="t.code" class="list-cell type-cell" :class="{ 'is-off': !t.enabled }">
        <view class="t-main">
          <view class="t-head">
            <view class="t-info">
              <text class="t-name">{{ t.name }}</text>
              <text class="t-code">{{ t.code }}</text>
            </view>
            <view class="t-enable">
              <text class="t-enable-label">{{ t.enabled ? '已启用' : '已停用' }}</text>
              <switch class="t-switch" :checked="t.enabled" color="#409eff" @change="onToggle(t, $event)" />
            </view>
          </view>
          <view class="t-actions">
            <view class="act-btn" @tap="openEdit(t)">
              <uni-icons type="compose" size="14" color="var(--text-2)" />
              <text class="act-label">编辑</text>
            </view>
            <view class="act-btn" @tap="removeType(t)">
              <uni-icons type="trash" size="14" color="var(--danger)" />
              <text class="act-label act-label-danger">停用</text>
            </view>
          </view>
        </view>
      </view>
    </view>
    <EmptyState v-else title="暂无空间类型" desc="点右上角「新增类型」添加" icon="flag" />

    <!-- 新增/编辑弹层 -->
    <view v-if="sheetVisible" class="mask" @tap="sheetVisible = false"></view>
    <view class="sheet" :class="{ show: sheetVisible }">
      <view class="sheet-head">
        <text class="sheet-title">{{ editing ? '编辑类型' : '新增类型' }}</text>
        <view class="sheet-close" @tap="sheetVisible = false">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <view class="field-row">
        <text class="field-label">类型 code</text>
        <input v-model="form.code" class="field" placeholder="如 lab / dorm" placeholder-class="ph" :disabled="!!editing" />
      </view>
      <view class="field-row">
        <text class="field-label">中文名</text>
        <input v-model="form.name" class="field" placeholder="如 实验室 / 宿舍" placeholder-class="ph" />
      </view>
      <button class="btn btn-primary" :disabled="submitting || !canSubmit" @tap="confirm">保存</button>
    </view>
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import EmptyState from '../../components/EmptyState.vue'

export default {
  components: { EmptyState },
  data() {
    return {
      store,
      types: [],
      sheetVisible: false,
      editing: null,
      form: { code: '', name: '' },
      submitting: false,
      redirecting: false,
    }
  },
  computed: {
    canSubmit() {
      return !!(this.form.code.trim() && this.form.name.trim())
    },
  },
  onShow() {
    if (!isLoggedIn()) {
      uni.reLaunch({ url: '/pages/login/login' })
      return
    }
    // 仅管理员可用；误入回跳首页。
    if (store.role !== 'admin' && !this.redirecting) {
      this.redirecting = true
      uni.reLaunch({ url: '/pages/home/home' })
      return
    }
    this.load()
  },
  onPullDownRefresh() {
    this.load()
  },
  methods: {
    async load() {
      try {
        this.types = await api.spaceTypes()
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    openCreate() {
      this.editing = null
      this.form = { code: '', name: '' }
      this.submitting = false
      this.sheetVisible = true
    },
    openEdit(t) {
      this.editing = t
      this.form = { code: t.code, name: t.name }
      this.submitting = false
      this.sheetVisible = true
    },
    async confirm() {
      if (this.submitting || !this.canSubmit) return
      this.submitting = true
      try {
        if (this.editing) {
          await api.updateSpaceType(this.editing.code, { name: this.form.name.trim() })
        } else {
          await api.createSpaceType({ code: this.form.code.trim(), name: this.form.name.trim() })
        }
        this.sheetVisible = false
        uni.showToast({ title: '已保存', icon: 'success' })
        this.load()
      } catch (e) {
        uni.showToast({ title: e.message || '保存失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
    async onToggle(t, e) {
      const enabled = !!e.detail.value
      try {
        await api.updateSpaceType(t.code, { enabled })
        t.enabled = enabled
      } catch (err) {
        uni.showToast({ title: err.message || '操作失败', icon: 'none' })
      }
    },
    removeType(t) {
      uni.showModal({
        title: '停用类型',
        content: `确定停用「${t.name}」？停用后仅不再作为新类型可选，已关联教室不受影响。`,
        success: async (res) => {
          if (!res.confirm) return
          try {
            await api.disableSpaceType(t.code)
            t.enabled = false
            uni.showToast({ title: '已停用', icon: 'success' })
          } catch (e) {
            uni.showToast({ title: e.message || '停用失败', icon: 'none' })
          }
        },
      })
    },
  },
}
</script>

<style scoped>
.types {
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}

.more-action {
  color: var(--primary);
}
.hint {
  display: block;
  margin: 8rpx 0 12rpx;
  font-size: 22rpx;
  color: var(--text-4);
  line-height: 1.5;
}

.list-group {
  margin-top: 4rpx;
}
.type-cell {
  flex-direction: column;
  align-items: stretch;
}
.type-cell.is-off .t-name {
  color: var(--text-4);
}
.t-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.t-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.t-info {
  display: flex;
  flex-direction: column;
}
.t-name {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
}
.t-code {
  margin-top: 4rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.t-enable {
  display: flex;
  align-items: center;
}
.t-enable-label {
  margin-right: 8rpx;
  font-size: 24rpx;
  color: var(--text-3);
}
.t-switch {
  transform: scale(0.8);
  transform-origin: right center;
}
.t-actions {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  margin-top: 16rpx;
}
.act-btn {
  display: flex;
  align-items: center;
  height: 56rpx;
  padding: 0 18rpx;
  margin: 0 12rpx 8rpx 0;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 8rpx;
}
.act-btn.disabled {
  opacity: 0.4;
}
.act-label {
  margin-left: 6rpx;
  font-size: 24rpx;
  color: var(--text-2);
}
.act-label-danger {
  color: var(--danger);
}

/* 弹层 */
.mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  /* 低于 uni-app picker 弹层(999)，否则弹层内嵌的选择器会被 sheet 遮挡 */
  z-index: 990;
  background: rgba(0, 0, 0, 0.5);
}
.sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 991;
  background: #fff;
  border-radius: 16rpx 16rpx 0 0;
  padding: 32rpx 32rpx calc(32rpx + env(safe-area-inset-bottom));
  transform: translateY(100%);
  transition: transform 0.25s ease;
}
.sheet.show {
  transform: translateY(0);
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
  font-size: 24rpx;
  color: var(--text-3);
}
.field {
  height: 92rpx;
  padding: 0 28rpx;
  background: var(--bg);
  border: 1rpx solid var(--border);
  border-radius: 8rpx;
  font-size: 30rpx;
}
.ph {
  color: var(--text-4);
}
.btn {
  margin-top: 8rpx;
}
.btn-primary {
  background: var(--primary);
  color: #fff;
}
.btn-primary[disabled] {
  background: var(--primary-soft);
  color: var(--text-4);
}
.btn-primary::after {
  border: none;
}
</style>
