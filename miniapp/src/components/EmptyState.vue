<template>
  <view class="empty">
    <uni-icons :type="icon" size="44" color="var(--text-4)" />
    <text class="title">{{ title }}</text>
    <text v-if="desc" class="desc">{{ desc }}</text>
  </view>
</template>

<script>
export default {
  name: 'EmptyState',
  props: {
    title: { type: String, default: '暂无数据' },
    desc: { type: String, default: '' },
    icon: { type: String, default: 'circle' },
  },
}
</script>

<style scoped>
.empty {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 90rpx 40rpx;
}
.title {
  margin-top: 24rpx;
  font-size: 28rpx;
  font-weight: 500;
  color: var(--text-3);
}
.desc {
  margin-top: 8rpx;
  font-size: 26rpx;
  font-weight: 400;
  color: var(--text-3);
  text-align: center;
}
</style>
