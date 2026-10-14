<template>
  <view></view>
</template>

<script setup>
/**
 * 角色分发入口：登录后按角色跳转对应首页
 */
import { onShow } from '@dcloudio/uni-app'
import { store } from '../../store'
import { roleHome } from '../../utils/permission'

onShow(() => {
  if (!store.token) {
    uni.reLaunch({ url: '/pages/login/login' })
    return
  }
  uni.reLaunch({ url: roleHome(store.user) })
})
</script>
