<template>
  <view class="spaces page-pad">
    <!-- 标题栏 -->
    <view class="section-title">
      <text class="t">教室管理</text>
      <text class="more more-action" @tap="openCreate">+ 新建教室</text>
    </view>
    <text class="hint">新建教室后设备页即出现对应类型 tab（暂无设备，等边侧装好设备后自动出现）</text>

    <!-- 教室列表 -->
    <view v-if="spaces.length" class="list-group">
      <view v-for="s in spaces" :key="s.space_id" class="list-cell space-cell" :class="{ 'is-off': !s.enabled }">
        <view class="s-main">
          <view class="s-head">
            <view class="s-info">
              <text class="s-name">{{ s.name }}</text>
              <text class="s-sub">{{ typeName(s.type) }} · {{ s.space_id }}</text>
            </view>
            <StatusTag :text="s.enabled ? '启用' : '已停用'" :tone="s.enabled ? 'success' : 'neutral'" />
          </view>
          <view class="s-actions">
            <view class="act-btn" @tap="openEdit(s)">
              <uni-icons type="compose" size="14" color="var(--text-2)" />
              <text class="act-label">编辑</text>
            </view>
            <view class="act-btn" @tap="removeSpace(s)">
              <uni-icons type="trash" size="14" color="var(--danger)" />
              <text class="act-label act-label-danger">停用</text>
            </view>
          </view>
        </view>
      </view>
    </view>
    <EmptyState v-else title="暂无教室" desc="点右上角「新建教室」添加" icon="flag" />

    <!-- 新建/编辑弹层 -->
    <view v-if="sheetVisible" class="mask" @tap="sheetVisible = false"></view>
    <view class="sheet" :class="{ show: sheetVisible }">
      <view class="sheet-head">
        <text class="sheet-title">{{ editing ? '编辑教室' : '新建教室' }}</text>
        <view class="sheet-close" @tap="sheetVisible = false">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <view class="field-row">
        <text class="field-label">教室名</text>
        <input v-model="form.name" class="field" placeholder="如 共用教室 / A201" placeholder-class="ph" />
      </view>
      <view class="field-row">
        <text class="field-label">类型</text>
        <picker mode="selector" :range="typeOptions" range-key="label" @change="onTypeChange">
          <view class="field pick-field">
            <text :class="form.type ? 'pick-val' : 'ph'">{{ typeLabel }}</text>
            <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
          </view>
        </picker>
      </view>
      <button class="btn btn-primary" :disabled="submitting || !canSubmit" @tap="confirm">保存</button>
    </view>
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import StatusTag from '../../components/StatusTag.vue'
import EmptyState from '../../components/EmptyState.vue'

export default {
  components: { StatusTag, EmptyState },
  data() {
    return {
      store,
      spaces: [],
      typeDefs: [],
      sheetVisible: false,
      editing: null,
      form: { name: '', type: '' },
      submitting: false,
      redirecting: false,
    }
  },
  computed: {
    typeOptions() {
      return this.typeDefs.map((t) => ({ key: t.code, label: t.name }))
    },
    typeLabel() {
      const t = this.typeDefs.find((x) => x.code === this.form.type)
      return t ? t.name : '请选择类型'
    },
    canSubmit() {
      return !!(this.form.name.trim() && this.form.type)
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
    typeName(code) {
      const t = this.typeDefs.find((x) => x.code === code)
      return t ? t.name : code
    },
    async load() {
      try {
        const [spaces, typeDefs] = await Promise.all([api.adminSpaces(), api.spaceTypes()])
        this.spaces = spaces || []
        this.typeDefs = typeDefs || []
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    openCreate() {
      this.editing = null
      this.form = { name: '', type: '' }
      this.submitting = false
      this.sheetVisible = true
    },
    openEdit(s) {
      this.editing = s
      this.form = { name: s.name, type: s.type }
      this.submitting = false
      this.sheetVisible = true
    },
    onTypeChange(e) {
      const idx = Number(e.detail.value)
      const t = this.typeOptions[idx]
      if (t) this.form.type = t.key
    },
    async confirm() {
      if (this.submitting || !this.canSubmit) return
      this.submitting = true
      try {
        if (this.editing) {
          await api.updateSpace(this.editing.space_id, { name: this.form.name.trim(), type: this.form.type })
        } else {
          await api.createSpace({ name: this.form.name.trim(), type: this.form.type })
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
    removeSpace(s) {
      uni.showModal({
        title: '停用教室',
        content: `确定停用「${s.name}」？停用后设备页不再显示。`,
        success: async (res) => {
          if (!res.confirm) return
          try {
            await api.disableSpace(s.space_id)
            s.enabled = false
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
.spaces {
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
.space-cell {
  flex-direction: column;
  align-items: stretch;
}
.space-cell.is-off .s-name {
  color: var(--text-4);
}
.s-main {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.s-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.s-info {
  flex: 1;
  min-width: 0;
  display: flex;
  flex-direction: column;
}
.s-name {
  font-size: 30rpx;
  font-weight: 600;
  color: var(--text-1);
}
.s-sub {
  margin-top: 4rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.s-actions {
  display: flex;
  align-items: center;
  margin-top: 16rpx;
}
.act-btn {
  display: flex;
  align-items: center;
  height: 56rpx;
  padding: 0 18rpx;
  margin: 0 12rpx 0 0;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 8rpx;
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
.pick-field {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
.pick-val {
  font-size: 30rpx;
  color: var(--text-1);
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
