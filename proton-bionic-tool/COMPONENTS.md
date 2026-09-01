# Proton Bionic (Termux) components

This local Steam compatibility tool uses the installed open-source Hangover
Wine/FEX stack and the following unmodified upstream Windows DLL releases:

- DXVK 1.10.3: https://github.com/doitsujin/dxvk/releases/tag/v1.10.3
  - `dxvk-1.10.3.tar.gz`
  - SHA-256: `8d1a3c912761b450c879f98478ae64f6f6639e40ce6848170a0f6b8596fd53c6`
- vkd3d-proton 3.0.1:
  https://github.com/HansKristian-Work/vkd3d-proton/releases/tag/v3.0.1
  - `vkd3d-proton-3.0.1.tar.zst`
  - SHA-256: `3cf2315522af5e43605ef6d3c41dad91387040bf97199934f3f7ab76caaa2f0c`

DXVK provides D3D9 through D3D11. vkd3d-proton provides D3D12. The launcher
installs both x86 and x86_64 DLL sets into each Steam prefix on first use.

The tool uses the local open-source `lsteambridge` proxy to reach the logged-in
native ARM64 Steam client. The bridge is started only for the `proxy` mode and
does not copy credentials into Wine.

DXVK 1.10.3 is the tested Adreno/Turnip default. DXVK 3.0.2 is retained only as
a development artifact outside minimal packages because it requires Vulkan
features, notably `shaderInt64`, that are not exposed by the tested driver.
