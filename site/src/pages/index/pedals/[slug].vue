<template>
  <q-page v-if="pedal" class="sp-pedal-page" :style="{ '--accent': pedal.accent }">
    <section class="sp-pedal-hero">
      <div class="sp-pedal-hero-inner">
        <router-link to="/" class="sp-crumb">← all pedals</router-link>
        <div class="sp-pedal-cat">{{ pedal.category }} · free VST3 / AU</div>
        <h1 class="sp-pedal-name">{{ pedal.name }}</h1>
        <p class="sp-pedal-tagline">{{ pedal.tagline }}</p>

        <div class="sp-pedal-cta">
          <q-btn
            class="sp-btn-primary" unelevated no-caps size="lg" icon="download"
            :href="macLink" target="_blank" rel="noopener"
            :label="macReady ? 'Download for macOS' : 'macOS — coming right up'"
            :disable="!macReady"
          />
          <q-btn
            class="sp-btn-secondary" outline no-caps size="lg"
            :href="winLink" target="_blank" rel="noopener"
            :label="winReady ? 'Download for Windows' : 'Windows — coming right up'"
            :disable="!winReady"
          />
        </div>
        <p class="sp-pedal-note">
          Ships in the free Sweet Papa Pedals bundle (v{{ bundle.version }}) — one installer, all six pedals.
        </p>

        <figure class="sp-pedal-shot">
          <img :src="pedal.image" :alt="`${pedal.name} plugin user interface — ${pedal.tagline}`" />
        </figure>
      </div>
    </section>

    <section class="sp-section sp-pedal-body">
      <div class="sp-pedal-cols">
        <div class="sp-pedal-desc">
          <p v-for="(para, i) in pedal.long" :key="i">{{ para }}</p>

          <h2>Good for</h2>
          <ul>
            <li v-for="g in pedal.goodFor" :key="g">{{ g }}</li>
          </ul>
        </div>

        <aside class="sp-pedal-panel">
          <h2>Controls</h2>
          <dl class="sp-controls">
            <template v-for="c in pedal.controls" :key="c.name">
              <dt :style="{ color: pedal.accent }">{{ c.name }}</dt>
              <dd>{{ c.desc }}</dd>
            </template>
          </dl>

          <template v-if="pedal.modes.length">
            <h2>{{ pedal.modes.length > 4 ? 'Characters' : 'Modes' }}</h2>
            <div class="sp-modes">
              <span v-for="m in pedal.modes" :key="m" class="sp-mode-chip">{{ m }}</span>
            </div>
          </template>
        </aside>
      </div>

      <div class="sp-pedal-others">
        <h2>The other pedals</h2>
        <div class="sp-grid">
          <PedalCard v-for="p in others" :key="p.slug" :pedal="p" />
        </div>
      </div>
    </section>
  </q-page>

  <q-page v-else class="sp-home sp-section">
    <h1>Pedal not found</h1>
    <router-link to="/">← all pedals</router-link>
  </q-page>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useRoute } from 'vue-router';
import { useMeta } from 'quasar';
import catalog from '@/data/catalog.json';
import PedalCard from '@/components/PedalCard.vue';

const route = useRoute();
const site = catalog.site;
const bundle = catalog.bundle;

const pedal = computed(() =>
  catalog.pedals.find((p) => p.slug === String(route.params.slug))
).value;

const others = catalog.pedals.filter((p) => p.slug !== pedal?.slug);

const macReady = !bundle.downloads.mac.startsWith('PASTE_');
const winReady = !bundle.downloads.win.startsWith('PASTE_');
const macLink = macReady ? bundle.downloads.mac : undefined;
const winLink = winReady ? bundle.downloads.win : undefined;

if (pedal) {
  const title = `${pedal.name} — ${pedal.tagline} | free ${pedal.category} VST/AU`;
  const desc = pedal.short;
  const url = `${site.url}/pedals/${pedal.slug}`;

  useMeta({
    title,
    meta: {
      description: { name: 'description', content: desc },
      keywords: { name: 'keywords', content: pedal.keywords.join(', ') },
      ogTitle: { property: 'og:title', content: title },
      ogDescription: { property: 'og:description', content: desc },
      ogType: { property: 'og:type', content: 'website' },
      ogUrl: { property: 'og:url', content: url },
      ogImage: { property: 'og:image', content: `${site.url}${pedal.image}` },
      twitterCard: { name: 'twitter:card', content: 'summary_large_image' }
    },
    link: { canonical: { rel: 'canonical', href: url } },
    script: {
      jsonld: {
        type: 'application/ld+json',
        innerHTML: JSON.stringify({
          '@context': 'https://schema.org',
          '@type': 'SoftwareApplication',
          name: pedal.name,
          description: desc,
          url,
          applicationCategory: 'MultimediaApplication',
          applicationSubCategory: pedal.category,
          operatingSystem: 'macOS, Windows',
          softwareVersion: bundle.version,
          offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' },
          publisher: { '@type': 'Organization', name: site.company, url: site.portfolio.url }
        })
      }
    }
  });
}
</script>
