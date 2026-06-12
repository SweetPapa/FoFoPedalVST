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
- `FIREBASE_SERVICE_ACCOUNT_FOFOAPPS` — already set from the local
  `fofoapps-sa.json` admin service account.

### Apple signing + notarization (macOS job)
| Secret | What it is |
|---|---|
| `APPLE_CERT_P12` | Base64 of a .p12 export containing BOTH personal certs: **Developer ID Application: Forrester Terry (6Y5SZ2K5XY)** and **Developer ID Installer: Forrester Terry (6Y5SZ2K5XY)** with their private keys |
| `APPLE_CERT_PASSWORD` | The password you set on that .p12 |
| `APPLE_ID` | Your Apple ID email |
| `APPLE_TEAM_ID` | `6Y5SZ2K5XY` |
| `APPLE_APP_PASSWORD` | App-specific password from appleid.apple.com → Sign-In & Security → App-Specific Passwords |

To export the .p12 (one time, on this Mac):
1. Keychain Access → My Certificates → ⌘-click select both *Forrester Terry (6Y5SZ2K5XY)* "Developer ID" entries (Application **and** Installer, with their keys).
2. File → Export Items… → .p12, set a password.
3. `base64 -i certs.p12 | pbcopy` → paste into the `APPLE_CERT_P12` secret.

### Azure Trusted Signing (Windows job)
Create an App Registration (service principal) in Entra ID and give it the
**Trusted Signing Certificate Profile Signer** role on your signing account,
then set:

| Secret | What it is |
|---|---|
| `AZURE_TENANT_ID` / `AZURE_CLIENT_ID` / `AZURE_CLIENT_SECRET` | The service principal credentials |
| `TRUSTED_SIGNING_ENDPOINT` | e.g. `https://eus.codesigning.azure.net/` |
| `TRUSTED_SIGNING_ACCOUNT` | Your Trusted Signing account name |
| `TRUSTED_SIGNING_PROFILE` | Your certificate profile name |

(Your existing local setup on Infinity keeps working via `windows\build.ps1`
as a fallback; CI uses the same Azure account through the official action.)

### AI release notes (optional)
| Secret | What it is |
|---|---|
| `ANTHROPIC_API_KEY` | Any Claude API key — release notes are written by `claude-sonnet-4-6` from the commit log. Without it, GitHub's auto-generated notes are used alone. |

## One-time manual steps (outside GitHub)

1. **DNS / custom domain**: Firebase Console → Hosting → site `fofo-pedals` →
   *Add custom domain* → `pedal.fofo.dev`. It will give you an A record (or
   CNAME) + a TXT verification record to add at your DNS host. SSL provisions
   automatically after that (can take an hour).
2. **App-specific password** (Apple) — see table above; only you can create it.
3. **Azure service principal** — see above; needs your Azure portal access.

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
