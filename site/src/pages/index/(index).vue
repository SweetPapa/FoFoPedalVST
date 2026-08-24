<template>
  <q-page class="sp-home">
    <!-- ── Hero ─────────────────────────────────────────────────────────── -->
    <section class="sp-hero">
      <div class="sp-hero-inner">
        <h1 class="sp-hero-title">
          Seven <em>free</em> pedals for<br />indie-rock production.
        </h1>
        <p class="sp-hero-sub">
          A vocal doubler, a reverb that never swamps, tape wobble, touch-responsive dirt,
          a one-knob dream machine, a grunge-and-metal amp in a box, and a twelve-character multi-FX.
          <strong>VST3 + AU · macOS &amp; Windows · actually free.</strong>
        </p>
        <div class="sp-hero-cta">
          <q-btn
            v-if="hasBundleLink"
            class="sp-btn-primary" unelevated no-caps size="lg"
            :href="bundle.downloads.mac" target="_blank" rel="noopener"
            label="Download for macOS" icon="download"
          />
          <q-btn
            v-if="hasBundleLinkWin"
            class="sp-btn-secondary" outline no-caps size="lg"
            :href="bundle.downloads.win" target="_blank" rel="noopener"
            label="Download for Windows"
          />
          <q-btn
            class="sp-btn-secondary" flat no-caps size="lg"
            href="#pedals" label="Meet the pedals ↓"
          />
        </div>
        <p class="sp-hero-note">
          One installer, all seven pedals, v{{ bundle.version }}. No account, no email, no trial timer.
        </p>
      </div>
    </section>

    <!-- ── Pedal grid ───────────────────────────────────────────────────── -->
    <section id="pedals" class="sp-section">
      <h2 class="sp-section-title">The pedals</h2>
      <p class="sp-section-sub">
        Each one is a single opinionated sound with a handful of knobs that all matter.
        Mix hygiene — filtered sends, pre-delay, ducking — is built in, not a settings page.
      </p>
      <div class="sp-grid">
        <PedalCard v-for="p in pedals" :key="p.slug" :pedal="p" />
      </div>
    </section>

    <!-- ── Install ──────────────────────────────────────────────────────── -->
    <section id="install" class="sp-section sp-install">
      <h2 class="sp-section-title">Install in one minute</h2>
      <div class="sp-install-cols">
        <div class="sp-install-card">
          <h3>macOS</h3>
          <p>{{ site.installNotes.mac }}</p>
        </div>
        <div class="sp-install-card">
          <h3>Windows</h3>
          <p>{{ site.installNotes.win }}</p>
        </div>
      </div>
      <p class="sp-install-daws">
        Works in GarageBand, Logic Pro, Ableton Live, FL Studio, Reaper, Cubase, Studio One
        and anything else that loads VST3 or Audio Units.
      </p>
    </section>

    <!-- ── Why / SEO copy ──────────────────────────────────────────────── -->
    <section class="sp-section sp-why">
      <h2 class="sp-section-title">Why these exist</h2>
      <p>
        Most free plugins are either feature racks you have to engineer, or toys that fall apart
        in a mix. These seven are built the way the beloved character pedals are built: one clear
        identity each, a handful of knobs that all do something, defaults that sound right on insert,
        and wet paths that stay out of your vocal's way. Made by
        <a :href="site.portfolio.url" target="_blank" rel="noopener">{{ site.company }}</a>
        for real indie-rock and bedroom-pop sessions — and shared free while the premium line grows.
      </p>
    </section>
  </q-page>
</template>

<script setup lang="ts">
import { useMeta } from 'quasar';
import catalog from '@/data/catalog.json';
import PedalCard from '@/components/PedalCard.vue';

const site = catalog.site;
const bundle = catalog.bundle;
const pedals = catalog.pedals;

const hasBundleLink = !bundle.downloads.mac.startsWith('PASTE_');
const hasBundleLinkWin = !bundle.downloads.win.startsWith('PASTE_');

const title = `${site.name} — ${site.tagline} (free VST3/AU)`;

useMeta({
  title,
  meta: {
    description: { name: 'description', content: site.description },
    keywords: { name: 'keywords', content: site.keywords.join(', ') },
    ogTitle: { property: 'og:title', content: title },
    ogDescription: { property: 'og:description', content: site.description },
    ogType: { property: 'og:type', content: 'website' },
    ogUrl: { property: 'og:url', content: site.url },
    ogImage: { property: 'og:image', content: `${site.url}/og.png` },
    twitterCard: { name: 'twitter:card', content: 'summary_large_image' }
  },
  link: { canonical: { rel: 'canonical', href: site.url } },
  script: {
    jsonld: {
      type: 'application/ld+json',
      innerHTML: JSON.stringify({
        '@context': 'https://schema.org',
        '@type': 'ItemList',
        name: site.name,
        description: site.description,
        itemListElement: pedals.map((p, i) => ({
          '@type': 'SoftwareApplication',
          position: i + 1,
          name: p.name,
          description: p.short,
          url: `${site.url}/pedals/${p.slug}`,
          applicationCategory: 'MultimediaApplication',
          operatingSystem: 'macOS, Windows',
          offers: { '@type': 'Offer', price: '0', priceCurrency: 'USD' }
        }))
      })
    }
  }
});
</script>
