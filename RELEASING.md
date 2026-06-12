# Releasing Sweet Papa Pedals

## The flow (once secrets are set up)

```bash
git tag v0.2.0
git push origin v0.2.0
```

That's a release. GitHub Actions builds macOS (AU+VST3, signed + notarized pkg)
and Windows (VST3, Azure-signed installer), writes AI release notes, and
publishes everything to GitHub Releases. The website's download buttons point
at `releases/latest/download/…` with stable asset names, so they update
automatically — no site change needed.

The website itself redeploys to https://pedal.fofo.dev (Firebase Hosting,
site `fofo-pedals` in project `fofoapps-934be`) on any push to `main` that
touches `site/`.

Everything degrades gracefully: signing/notarization/AI-notes steps simply
skip when their secrets are missing, so the pipeline works (unsigned) from
day one.

## Secrets checklist (GitHub → repo → Settings → Secrets → Actions)

### Firebase site deploys — DONE ✓
- `FIREBASE_SERVICE_ACCOUNT_FOFOAPPS` — set from the FoFoApps admin SA.
  Custom domain `pedal.fofo.dev` is live.

### AI release notes (Vertex AI) — DONE ✓
- `VERTEX_SA_KEY` — the existing `vertext-ai@sweet-papa-technologies` service
  account. Notes are written by **Gemini 3.5 Flash** on Vertex (global
  endpoint, project `sweet-papa-technologies`) from the commit log, with
  GitHub auto-notes appended. Model/project are env vars at the top of the
  `release` job in `.github/workflows/release.yml`.

### Azure Trusted Signing (Windows job) — DONE ✓
- Service principal `sweetpapa-pedals-ci` with the **Artifact Signing
  Certificate Profile Signer** role on account `spt-cert` (rg `spt`),
  profile `forrester-personal`, endpoint `https://eus.codesigning.azure.net/`.
- Secrets set: `AZURE_TENANT_ID`, `AZURE_CLIENT_ID`, `AZURE_CLIENT_SECRET`,
  `TRUSTED_SIGNING_ENDPOINT`, `TRUSTED_SIGNING_ACCOUNT`, `TRUSTED_SIGNING_PROFILE`.
- (Local fallback on Infinity still works via `windows\build.ps1`.)

### Apple signing + notarization — run one script
```bash
./scripts/setup-apple-ci-signing.sh
```
You export the two personal Developer ID certificates yourself in Keychain
Access (the script opens it and tells you exactly which two to select — no
other keys are ever exported). The script then VERIFIES the .p12 contains
only those two identities (hard abort otherwise), uploads the secrets
(`APPLE_CERT_P12`, `APPLE_CERT_PASSWORD`, `APPLE_TEAM_ID`, `APPLE_ID`,
`APPLE_APP_PASSWORD`), and deletes the local file. Before running, create
an app-specific password at https://account.apple.com → Sign-In & Security
→ App-Specific Passwords so you can paste it when prompted.

## Local fallbacks

- macOS: `./scripts/build-installer-macos.sh` (auto-detects your personal
  Developer ID certs, skips Stanford ones; `NOTARY_PROFILE=<profile>` to notarize).
- Windows (on Infinity): `powershell -File windows\build.ps1` — see `windows/README.md`.
- Site: `cd site && pnpm build && firebase deploy --only hosting:pedals --project fofoapps-934be`.

## Versioning

Tag = version. `v0.2.0` → installers named `SweetPapaPedals-0.2.0.pkg` /
`SweetPapaPedals-Setup-0.2.0.exe` plus the stable-name copies the site links
to. Keep `bundle.version` in `site/src/data/catalog.json` roughly in sync so
the site shows the right number.
