# Building & signing the Windows release (run on Infinity)

One command produces `dist\SweetPapaPedals-Setup-<version>.exe`, signed and SmartScreen-friendly.

## One-time setup

1. **Visual Studio 2022** — install with the *Desktop development with C++* workload.
2. **CMake ≥ 3.22** and **Node 18+** on PATH.
3. **Inno Setup 6** — <https://jrsoftware.org/isinfo.php> (free).
4. **Azure Trusted Signing hookup** (you already have the Azure side set up):
   - Install the [Trusted Signing dlib package](https://www.nuget.org/packages/Microsoft.Trusted.Signing.Client) (NuGet → extract) or `dotnet tool` equivalent. Note the path to `Azure.CodeSigning.Dlib.dll` (x64).
   - Create a `metadata.json` next to it:
     ```json
     {
       "Endpoint": "https://<region>.codesigning.azure.net/",
       "CodeSigningAccountName": "<your-account>",
       "CertificateProfileName": "<your-profile>"
     }
     ```
   - Authenticate however you normally do for Azure (e.g. `az login`, or set `AZURE_TENANT_ID` / `AZURE_CLIENT_ID` / `AZURE_CLIENT_SECRET` for a service principal). `signtool` picks credentials up via DefaultAzureCredential.
   - `signtool.exe` comes with the Windows SDK (installed with VS). Needs the **10.0.22621+** SDK for dlib support.

## Build + sign

```powershell
cd <repo>
$env:AZURE_CODESIGN_DLIB = "C:\tools\TrustedSigning\bin\x64\Azure.CodeSigning.Dlib.dll"
$env:AZURE_CODESIGN_META = "C:\tools\TrustedSigning\metadata.json"
powershell -ExecutionPolicy Bypass -File windows\build.ps1
```

The script: builds the seven React UIs → CMake/MSVC Release build of all seven VST3s → signs each plugin binary → compiles the Inno Setup installer → signs the installer.

Leave the two env vars unset to produce an unsigned installer for local testing.

## Notes

- **Do VST3s need signing?** No — DAWs load unsigned VST3s fine. The *installer* is what SmartScreen judges; signing it with Trusted Signing removes the "unrecognized app" wall. Signing the plugin DLLs too is free once it works, so the script does both.
- SmartScreen reputation builds over downloads even when signed — early downloaders may still see a milder prompt for a while.
- Standalone .exe builds also exist (`<PLUGIN>_Standalone` targets) but aren't shipped in the installer; DAW plugins are the product.
- AAX/Pro Tools is out of scope (requires Avid signing/PACE).
