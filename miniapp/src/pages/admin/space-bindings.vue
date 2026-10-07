<template>
  <view class="bindings page-pad">
    <!-- 标题栏 -->
    <view class="section-title">
      <text class="t">教师与教室</text>
      <text class="more more-action" @tap="openBind(null)">+ 绑定</text>
    </view>

    <!-- 教师列表 -->
    <view v-if="teachers.length" class="list-group">
      <view v-for="t in teachers" :key="t.user_id" class="list-cell">
        <view class="t-main">
          <view class="t-head">
            <view class="t-info">
              <text class="t-name">{{ t.name }}</text>
              <text class="t-id">{{ t.user_id }}</text>
            </view>
            <button class="bind-btn" @tap="openBind(t)">绑定</button>
          </view>
          <view class="t-spaces">
            <view v-if="t.space_ids.length" class="chips">
              <view v-for="sid in t.space_ids" :key="sid" class="space-chip">
                <text class="chip-label">{{ spaceName(sid) }}</text>
                <view class="chip-remove" @tap="unbind(t, sid)">
                  <uni-icons type="closeempty" size="16" color="var(--danger)" />
                </view>
              </view>
            </view>
            <text v-else class="t-empty">暂未绑定教室</text>
          </view>
        </view>
      </view>
    </view>
    <EmptyState v-else title="暂无教师" desc="教师名单由 user_roles 同步" icon="person" />

    <!-- 绑定弹层 -->
    <view v-if="sheetVisible" class="mask" @tap="sheetVisible = false"></view>
    <view class="sheet" :class="{ show: sheetVisible }">
      <view class="sheet-head">
        <text class="sheet-title">绑定教室</text>
        <view class="sheet-close" @tap="sheetVisible = false">
          <uni-icons type="close" size="18" color="var(--text-4)" />
        </view>
      </view>
      <picker
        mode="selector"
        :range="teacherTabs"
        range-key="label"
        @change="onTeacherChange"
      >
        <view class="pick card">
          <text class="pick-label">{{ teacherLabel }}</text>
          <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
        </view>
      </picker>
      <picker
        mode="selector"
        :range="unboundSpaces"
        range-key="label"
        @change="onSheetSpaceChange"
      >
        <view class="pick card">
          <text class="pick-label">{{ sheetSpaceLabel }}</text>
          <uni-icons type="arrow-down" size="16" color="var(--text-3)" />
        </view>
      </picker>
      <button
        class="btn btn-primary"
        :disabled="submitting || !canSubmit"
        @tap="confirmBind"
      >确认绑定</button>
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
      teachers: [],
      sheetVisible: false,
      sheetTeacherId: '',
      sheetSpaceId: '',
      submitting: false,
      redirecting: false,
    }
  },
  computed: {
    teacherTabs() {
      return this.teachers.map((t) => ({ key: t.user_id, label: t.name }))
    },
    teacherLabel() {
      const t = this.teacherTabs.find((x) => x.key === this.sheetTeacherId)
      return t ? t.label : '选择教师'
    },
    unboundSpaces() {
      const t = this.teachers.find((x) => x.user_id === this.sheetTeacherId)
      const bound = t ? t.space_ids : []
      return this.spaces
        .filter((s) => !bound.includes(s.space_id))
        .map((s) => ({ key: s.space_id, label: s.name }))
    },
    sheetSpaceLabel() {
      const s = this.unboundSpaces.find((x) => x.key === this.sheetSpaceId)
      return s ? s.label : '选择教室'
    },
    canSubmit() {
      return !!(this.sheetTeacherId && this.sheetSpaceId)
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
    spaceName(id) {
      const s = this.spaces.find((x) => x.space_id === id)
      return s ? s.name : id
    },
    async load() {
      try {
        const [spaces, bindings] = await Promise.all([api.spaces(), api.spaceBindings()])
        this.spaces = spaces
        this.teachers = bindings.teachers || []
      } catch (e) {
        uni.showToast({ title: e.message || '加载失败', icon: 'none' })
      } finally {
        uni.stopPullDownRefresh()
      }
    },
    openBind(t) {
      this.sheetTeacherId = t ? t.user_id : (this.teachers[0] ? this.teachers[0].user_id : '')
      this.sheetSpaceId = ''
      this.submitting = false
      this.sheetVisible = true
    },
    onTeacherChange(e) {
      const idx = Number(e.detail.value)
      const t = this.teacherTabs[idx]
      if (t) {
        this.sheetTeacherId = t.key
        this.sheetSpaceId = ''
      }
    },
    onSheetSpaceChange(e) {
      const idx = Number(e.detail.value)
      const s = this.unboundSpaces[idx]
      if (s) this.sheetSpaceId = s.key
    },
    async confirmBind() {
      if (this.submitting || !this.canSubmit) return
      this.submitting = true
      try {
        await api.bindSpace(this.sheetTeacherId, this.sheetSpaceId)
        this.sheetVisible = false
        uni.showToast({ title: '已绑定', icon: 'success' })
        this.load()
      } catch (e) {
        uni.showToast({ title: e.message || '绑定失败', icon: 'none' })
      } finally {
        this.submitting = false
      }
    },
    unbind(t, sid) {
      uni.showModal({
        title: '解绑教室',
        content: `确认将 ${t.name} 与「${this.spaceName(sid)}」解绑？`,
        success: async (res) => {
          if (!res.confirm) return
          try {
            await api.unbindSpace(t.user_id, sid)
            uni.showToast({ title: '已解绑', icon: 'success' })
            this.load()
          } catch (e) {
            uni.showToast({ title: e.message || '解绑失败', icon: 'none' })
          }
        },
      })
    },
  },
}
</script>

<style scoped>
.bindings {
  padding-top: 24rpx;
  padding-left: 24rpx;
  padding-right: 24rpx;
}

.more-action {
  color: var(--primary);
}

.list-group {
  margin-top: 4rpx;
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
.t-id {
  margin-top: 4rpx;
  font-size: 22rpx;
  color: var(--text-3);
}
.bind-btn {
  height: 60rpx;
  padding: 0 28rpx;
  margin: 0;
  font-size: 26rpx;
  line-height: 60rpx;
  color: var(--primary);
  background: var(--primary-soft);
  border: 1rpx solid var(--primary-border);
  border-radius: 8rpx;
}
.bind-btn::after {
  border: none;
}

.t-spaces {
  margin-top: 16rpx;
}
.chips {
  display: flex;
  flex-wrap: wrap;
}
.space-chip {
  display: inline-flex;
  align-items: center;
  height: 48rpx;
  padding: 0 8rpx 0 18rpx;
  margin: 0 14rpx 12rpx 0;
  background: var(--info-bg);
  border: 1rpx solid var(--info-border);
  border-radius: 8rpx;
}
.chip-label {
  font-size: 24rpx;
  color: var(--text-2);
}
.chip-remove {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 40rpx;
  height: 40rpx;
  margin-left: 6rpx;
}
.t-empty {
  font-size: 24rpx;
  color: var(--text-4);
}

/* 弹层 */
.mask {
  position: fixed;
  left: 0;
  right: 0;
  top: 0;
  bottom: 0;
  z-index: 1000;
  background: rgba(0, 0, 0, 0.5);
}
.sheet {
  position: fixed;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1001;
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
.pick {
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 80rpx;
  padding: 0 24rpx;
  margin-bottom: 16rpx;
}
.pick-label {
  font-size: 28rpx;
  color: var(--text-1);
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
