<template>
  <router-link :to="`/pedals/${pedal.slug}`" class="sp-card" :style="cardStyle">
    <div class="sp-card-shot">
      <img :src="pedal.image" :alt="`${pedal.name} plugin interface — ${pedal.tagline}`" loading="lazy" />
    </div>
    <div class="sp-card-cat">{{ pedal.category }}</div>
    <h3 class="sp-card-name" :style="{ color: pedal.accent }">{{ pedal.name }}</h3>
    <p class="sp-card-tag">{{ pedal.tagline }}</p>
    <p class="sp-card-short">{{ pedal.short }}</p>
    <div class="sp-card-foot">
      <span class="sp-card-free">{{ pedal.status === 'free' ? 'FREE' : pedal.status.toUpperCase() }}</span>
      <span class="sp-card-more">details + download →</span>
    </div>
  </router-link>
</template>

<script setup lang="ts">
import { computed } from 'vue';

const props = defineProps<{
  pedal: {
    slug: string; name: string; tagline: string; accent: string;
    status: string; category: string; short: string; image: string;
  };
}>();

const cardStyle = computed(() => ({
  '--accent': props.pedal.accent
}));
</script>
