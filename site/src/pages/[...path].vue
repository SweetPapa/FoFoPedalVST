<template>
  <div class="sp-404">
    <div>
      <div class="sp-404-code">404</div>
      <div class="sp-404-text">That pedal isn't on the board.</div>
      <q-btn class="sp-btn-primary q-mt-xl" unelevated no-caps to="/" label="← Back to the pedals" />
    </div>
  </div>
</template>

<style lang="scss" scoped>
.sp-404 {
  min-height: 100vh; display: flex; align-items: center; justify-content: center;
  text-align: center; background: var(--bg); color: var(--ink);
}
.sp-404-code { font-size: 20vh; font-weight: 800; color: var(--brand); line-height: 1; }
.sp-404-text { font-size: 20px; opacity: 0.6; }
</style>
