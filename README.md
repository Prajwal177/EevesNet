# EevesNet

EevesNet is a Windows desktop network monitor built with C++20, Qt 6, and Npcap. It captures packets on a selected network adapter and presents packet summaries, header details, and a bounded hex/ASCII payload view. The Dashboard embeds an interactive Leaflet map using Esri's ArcGIS World Street Map tiles when Qt WebEngine is installed.

> **Project status:** This is an actively developed proof of concept. Live capture requires a compatible Npcap installation and may require elevated permissions. GeoIP enrichment and traffic visualization on the map are not yet connected to the capture stream.

## Features

- Enumerate network adapters and start/stop packet capture.
- Parse Ethernet, IPv4, IPv6, TCP, UDP, and ICMP headers where supported.
- Identify common application protocols such as HTTP, HTTPS, and QUIC from packet metadata and payload signatures.
- Annotate packet endpoints that match the selected adapter's local IP addresses.
- Inspect captured TCP/UDP application payloads as hex and ASCII (display limited to 4 KiB per packet).
- Browse an interactive Leaflet/ArcGIS basemap on the Dashboard when Qt WebEngine is available.
- Batch packet updates to keep the UI responsive under heavier traffic.

Payload bytes may contain encrypted data (for example, HTTPS/TLS); EevesNet does not decrypt them. Capturing network traffic can expose sensitive information. Use only on systems and networks you own or are authorized to monitor.

## Requirements

- Windows 10 or later
- Visual Studio with the **Desktop development with C++** workload and MSVC
- CMake 3.21 or later (the project currently requires 3.21)
- Qt 6.5 or later with matching MSVC architecture; required modules: Network and Widgets
- Qt WebEngineWidgets for the embedded Leaflet Dashboard map (optional at configure time)
- Npcap runtime/driver and SDK for packet capture
- vcpkg with the manifest dependency `libmaxminddb`
- GeoLite2 City MMDB database for future/local GeoIP enrichment features

The Qt kit, compiler, vcpkg triplet, Npcap libraries, and build architecture must match (for example, x64 throughout). The Qt WebEngine component should be installed for the same Qt version and MSVC kit using Qt Maintenance Tool. Without it, the application builds with a Dashboard notice instead of the embedded map.

## Configure and build

Open **Developer PowerShell for Visual Studio** with the x64 MSVC tools available. Install Ninja and ensure it is on `PATH`. Set `VCPKG_ROOT` to a full vcpkg checkout (the vcpkg toolchain file must exist under `scripts/buildsystems`). Then configure using paths for your local Qt and Npcap SDK installations:

```powershell
$env:VCPKG_ROOT = "C:/path/to/vcpkg"

cmake -S . -B out/build/x64-Debug -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.12.0/msvc2022_64" `
  -DNPCAP_SDK_ROOT="C:/path/to/npcap-sdk" `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build out/build/x64-Debug
```

Replace the example paths with the locations on your machine. The Qt kit, MSVC compiler, vcpkg triplet, Npcap libraries, and build architecture must all be x64 (or all match another supported architecture). The manifest installs `libmaxminddb`; its CMake toolchain must be active during the first configure. If CMake cannot locate Qt, set `Qt6_DIR` to the kit's `lib/cmake/Qt6` directory. If Npcap SDK discovery fails, CMake warns and capture support is disabled; set `NPCAP_SDK_ROOT` to the extracted SDK root. The Npcap runtime/driver is required at run time for capture.

The embedded Leaflet dashboard is enabled only when the matching **Qt WebEngineWidgets** component is installed for the selected Qt kit. Install it through Qt Maintenance Tool before configuring to display the map; without WebEngine the app builds but displays a notice in the Dashboard. To build without Npcap discovery, add `-DEEVESNET_ENABLE_NPCAP=OFF`.

## Run

After the build succeeds, run the executable from the same PowerShell session:

```powershell
& .\out\build\x64-Debug\EevesNet.exe
```

For a Release configuration, use the Release build directory and its executable path instead. CMake runs `windeployqt` after linking to copy Qt runtime dependencies beside the executable. If Npcap is installed and capture support was enabled at configure time, select an adapter and choose **Start capture**. Depending on the local Npcap configuration, administrator permissions may be required. Use **Stop capture** to end capture.

The map uses the ArcGIS World Street Map tile service and Leaflet assets from a CDN, so it requires internet access. It does not require a MapTiler API key. Esri attribution is displayed on the map.

## Limitations

- Packet capture and decoding support is a proof of concept; traffic may be dropped if the system cannot keep up.
- Application-protocol labels are heuristic. Encrypted application traffic is not decoded.
- The map is a basemap only; captured endpoints are not currently plotted on it.
- GeoIP city/country lookup, process attribution, reverse DNS, and recording/replay are not currently integrated into this C++ application.
- The embedded map requires Qt WebEngine and an internet connection; it does not work offline.
