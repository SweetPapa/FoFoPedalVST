<template>
  <q-layout view="hHh lpR fFf" class="sp-site">
    <div class="sp-flow" aria-hidden="true" />
    <q-header class="sp-header">
      <q-toolbar class="sp-toolbar">
        <router-link to="/" class="sp-logo">
          <span class="sp-logo-mark">●</span>
          <span class="sp-logo-text">Sweet Papa <b>Pedals</b></span>
        </router-link>
        <q-space />
        <a class="sp-nav-link" href="/#pedals">Pedals</a>
        <a class="sp-nav-link" href="/#install">Install</a>
        <a class="sp-nav-link" :href="site.github" target="_blank" rel="noopener">GitHub</a>
        <a class="sp-nav-link sp-nav-portfolio" :href="site.portfolio.url" target="_blank" rel="noopener">
          {{ site.portfolio.label }} ↗
        </a>
      </q-toolbar>
    </q-header>

    <q-page-container>
      <!-- key by path: /pedals/:slug reuses the same component, and the pedal
           page resolves its catalog entry + meta at setup — remount on
           param change so navigating pedal→pedal actually loads the new one -->
      <router-view :key="$route.path" />
    </q-page-container>

    <footer class="sp-footer">
      <div class="sp-footer-inner">
        <div>
          <div class="sp-footer-brand">Sweet Papa Pedals</div>
          <div class="sp-footer-dim">{{ site.tagline }} · free, forever</div>
        </div>
        <div class="sp-footer-links">
          <a :href="site.portfolio.url" target="_blank" rel="noopener">{{ site.portfolio.label }}</a>
          <a :href="site.github" target="_blank" rel="noopener">source on GitHub</a>
          <router-link to="/">all pedals</router-link>
        </div>
        <div class="sp-footer-dim">
          © {{ new Date().getFullYear() }} {{ site.company }} · VST is a trademark of Steinberg Media Technologies GmbH
        </div>
      </div>
    </footer>
  </q-layout>
</template>

<script setup lang="ts">
import catalog from '@/data/catalog.json';
const site = catalog.site;
</script>
