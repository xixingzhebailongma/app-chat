<template>
  <view class="spaces page-pad">
    <!-- 标题栏 -->
    <view class="section-title">
      <text class="t">教室管理</text>
      <text class="more more-action" @tap="openCreate">+ 新建教室</text>
    </view>
    <text class="hint">点教室可进入详情：编辑教室信息、查看并控制该教室的设备</text>

    <!-- 搜索 -->
    <view class="search">
      <view class="search-ico"><uni-icons type="search" size="18" color="var(--text-3)" /></view>
      <input
        v-model="keyword"
        class="search-input"
        placeholder="搜索教室名称 / 编号"
        placeholder-class="ph"
      />
      <view v-if="keyword" class="search-clear" @tap="keyword = ''"><uni-icons type="clear" size="18" color="var(--text-4)" /></view>
    </view>

    <!-- 教室列表：点整行进入教室详情（设备控制 + 信息编辑） -->
    <view v-if="filteredSpaces.length" class="list-group">
      <view
        v-for="s in filteredSpaces"
        :key="s.space_id"
        class="list-cell space-cell"
        :class="{ 'is-off': !s.enabled }"
        hover-class="cell-hover"
        @tap="openDetail(s)"
      >
        <view class="s-main">
          <view class="s-head">
            <view class="s-info">
              <text class="s-name">{{ s.name }}</text>
              <text class="s-sub">{{ typeName(s.type) }} · {{ s.space_id }}</text>
            </view>
            <view class="s-enable" @tap.stop>
              <switch class="s-switch" :checked="s.enabled" color="#245e70" @change="onToggle(s, $event)" />
            </view>
          </view>
          <view class="s-foot">
            <view class="act-btn" @tap.stop="openEdit(s)">
              <uni-icons type="compose" size="14" color="var(--text-2)" />
              <text class="act-label">编辑</text>
            </view>
            <text class="s-hint">点击进入设备管理</text>
          </view>
        </view>
        <uni-icons type="arrow-right" size="14" color="var(--text-4)" />
      </view>
    </view>
    <EmptyState v-else-if="keyword" title="未找到匹配教室" desc="请调整关键词后重试" />
    <EmptyState v-else title="暂无教室" desc="点右上角「新建教室」添加" icon="flag" />

    <!-- 新建/编辑弹层 -->
    <ActionSheet :visible="sheetVisible" :title="editing ? '编辑教室' : '新建教室'" @close="sheetVisible = false">
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
    </ActionSheet>
  </view>
</template>

<script>
import { api } from '../../api/index'
import { store, isLoggedIn } from '../../store/index'
import EmptyState from '../../components/EmptyState.vue'
import ActionSheet from '../../components/ActionSheet.vue'

export default {
  components: { EmptyState, ActionSheet },
  data() {
    return {
      store,
      spaces: [],
      typeDefs: [],
      keyword: '',
      sheetVisible: false,
      editing: null,
      form: { name: '', type: '' },
      submitting: false,
      redirecting: false,
    }
  },
  computed: {
    filteredSpaces() {
      const k = this.keyword.trim().toLowerCase()
      if (!k) return this.spaces
      return this.spaces.filter(
        (s) => (s.name || '').toLowerCase().includes(k) || (s.space_id || '').toLowerCase().includes(k)
      )
    },
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
    /* 进入教室详情：查看设备并可控制开关，教室信息也在那里编辑 */
    openDetail(s) {
      uni.navigateTo({ url: '/pages/admin/space-detail?space_id=' + encodeURIComponent(s.space_id) })
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
    async onToggle(s, e) {
      const enabled = !!e.detail.value
      try {
        await api.updateSpace(s.space_id, { enabled })
        s.enabled = enabled
      } catch (err) {
        uni.showToast({ title: err.message || '操作失败', icon: 'none' })
      }
    },
  },
}
</script>

<style scoped>
.spaces {
  padding-top: 24rpx;
  padding-left: 36rpx;
  padding-right: 36rpx;
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
.search {
  display: flex;
  align-items: center;
  margin: 4rpx 0 16rpx;
  padding: 0 28rpx;
  height: 80rpx;
  background: #fff;
  border: 1rpx solid var(--border);
  border-radius: 12rpx;
}
.search-ico {
  display: flex;
  align-items: center;
  margin-right: 12rpx;
}
.search-input {
  flex: 1;
  height: 80rpx;
  font-size: 28rpx;
  color: var(--text-1);
}
.search-clear {
  display: flex;
  align-items: center;
  padding: 8rpx;
}

.list-group {
  margin-top: 4rpx;
}
.space-cell {
  align-items: center;
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
.s-enable {
  display: flex;
  align-items: center;
  flex-shrink: 0;
}
.s-switch {
  transform: scale(0.8);
  transform-origin: right center;
}
.s-foot {
  display: flex;
  align-items: center;
  margin-top: 16rpx;
}
.s-hint {
  flex: 1;
  text-align: right;
  font-size: 22rpx;
  color: var(--text-4);
}
.act-btn {
  display: flex;
  align-items: center;
  height: 56rpx;
  padding: 0 18rpx;
  margin: 0 12rpx 0 0;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 12rpx;
}
.act-label {
  margin-left: 6rpx;
  font-size: 24rpx;
  color: var(--text-2);
}
.act-label-danger {
  color: var(--danger);
}

/* 表单 */
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
  border-radius: 12rpx;
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
