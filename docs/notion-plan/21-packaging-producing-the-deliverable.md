# packaging/ — producing the deliverable

Source page id: 90f7cc5cab104f47a2ed0c10d1dabfac

---

<callout icon="📦">
	**Purpose.** Turning a build into the thing that lands on the shop machine — installed without elevation, updated without an administrator, and reversible when an update goes wrong.
</callout>
## Files
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`stage.cmake`</td>
<td>Assemble the version folder: executables, Qt runtime, plugins, app-local runtime, licences</td>
</tr>
<tr>
<td>`prune.txt`</td>
<td>The explicit list of what the deployment tool over-collects and we remove. **Verified by the packaging test**, never by hope</td>
</tr>
<tr>
<td>`manifest.py`</td>
<td>Version, every file, every hash, total size. Signed afterwards</td>
</tr>
<tr>
<td>`sign.ps1`</td>
<td>Automated signing of our executables and the manifest with the self-signed certificate</td>
</tr>
<tr>
<td>`installer.iss`</td>
<td>Per-user install, no elevation, one uninstall entry, no file associations, no startup entry unless asked</td>
</tr>
<tr>
<td>`updater/`</td>
<td>A tiny separate executable: verify the signature, unpack the new version folder, repoint, restart, and **revert after two failed starts**</td>
</tr>
<tr>
<td>`certificate/README.md`</td>
<td>How the certificate is generated, where it is stored, and the honest limits below</td>
</tr>
</table>
## The installed footprint
```plain text
SquiFlow\
  current -> versions\<v>      a directory junction, no elevation needed
  versions\<v>\               exactly two kept, older ones removed
  squiflow-updater.exe
ProgramData\SquiFlow\
  data\squiflow.db             machine-wide, so any Windows account sees the shop
  cache\thumbnails\  logs\  secrets\
```
**The program folder is never written to at run time.** One registry entry. No telemetry, no crash reporting to anyone, no background installer service.
## Honest limits of self-signed signing
- Warnings disappear **only** on machines where the certificate is installed as a trusted publisher — which is fine here, because the machines are known.
- A self-signed certificate **never earns reputation** with the operating system's download screening. First-run friction on a fresh machine is expected, and installing the certificate is part of setting a machine up.
- The certificate's private key is a build secret. **If it leaks, updates can be forged** — rotation must be written down before the first release, not after.
## Update rules
- Notify, then the person clicks. **Never silent, never forced.**
- **Blocked while the outbox is not empty** — unsent work must not be carried across a version change.
- Downloads come through the shop server, so the two machines fetch from the private release once, not twice.
## Done when
A fresh machine installs without a prompt for an administrator, updates from the previous version, and reverts automatically when handed a deliberately broken build.
