# OTA Update System

A complete Over-The-Air (OTA) update system for delivering firmware updates via network with daemon support, automatic decompression, and JSON configuration.

## Quick Start

### 1. Build
```bash
cd /home/maxmaster/temp
mkdir -p build && cd build
cmake ..
make
```

### 2. Start Service (Terminal 1)
```bash
./ota_service /path/to/file_ota_update_2.5.wic.bz2
```

### 3. Start Daemon (Terminal 2)
```bash
./ota_daemon /etc/ota/daemon.json
```

## What It Does

**ota_service** - Server that:
- Loads firmware files from disk
- Extracts version numbers from filenames (e.g., 2.5 from `file_ota_update_2.5.wic.bz2`)
- Serves file chunks via SomeIP RPC (4KB default)
- Verifies integrity with MD5 checksums

**ota_daemon** - Client daemon that:
- Runs continuously in background
- Reads JSON configuration file
- Monitors for updates every N seconds
- Downloads files and verifies checksums
- Automatically decompresses (bzip2, gzip)
- Optionally cleans up compressed files
- Prevents reprocessing of same version

## Configuration

Create a JSON config file (e.g., `/etc/ota/daemon.json`):

```json
{
    "downloadPath": "/var/ota/downloads",
    "decompressionPath": "/var/ota/extracted",
    "chunkSize": 4096,
    "checkIntervalSec": 60,
    "autoDecompress": true,
    "autoCleanup": true
}
```

### Required Fields
- `downloadPath` - Where to save downloaded files
- `decompressionPath` - Where to extract files

### Optional Fields (with defaults)
- `chunkSize` - Bytes per chunk (default: 4096)
- `checkIntervalSec` - Check interval in seconds (default: 60)
- `autoDecompress` - Auto-decompress after download (default: true)
- `autoCleanup` - Delete compressed file after decompression (default: true)

## File Naming

Update files should follow the format:
```
file_ota_update_<MAJOR>.<MINOR>.<EXTENSION>
```

Examples:
- `file_ota_update_1.0.wic.bz2` → Version 1.0 (bzip2)
- `file_ota_update_2.5.wic.gz` → Version 2.5 (gzip)
- `file_ota_update_3.0.wic` → Version 3.0 (uncompressed)

## Dependencies

```bash
# Ubuntu/Debian
sudo apt-get install -y \
  cmake \
  libcommonapi-dev \
  libcommonapi-someip-dev \
  libvsomeip3-dev \
  libssl-dev \
  libbz2-dev \
  zlib1g-dev \
  nlohmann-json3-dev
```

## Directory Structure

```
/home/maxmaster/temp/
├── inc/                          # Headers
│   ├── ota_updater_impl.hpp
│   ├── file_manager.hpp
│   ├── config_manager.hpp
│   └── file_decompressor.hpp
├── service_src/                  # Service code
│   ├── ota_service_main.cpp
│   ├── ota_updater_impl.cc
│   └── file_manager.cc
├── daemon_src/                   # Daemon code
│   ├── ota_daemon_main.cpp
│   ├── config_manager.cc
│   └── file_decompressor.cc
├── build/                        # Build output
│   ├── ota_service               # Service binary
│   └── ota_daemon                # Daemon binary
├── CMakeLists.txt
├── example_daemon_config.json
└── README.md                     # This file
```

## Usage Examples

### Example 1: Simple Download
```bash
# Terminal 1
cd /home/maxmaster/temp/build
./ota_service /tmp/file_ota_update_2.5.wic.bz2

# Terminal 2
cat > /tmp/config.json << 'EOT'
{
    "downloadPath": "/tmp/ota_downloads",
    "decompressionPath": "/tmp/ota_extracted",
    "checkIntervalSec": 10,
    "autoDecompress": true
}
EOT
./ota_daemon /tmp/config.json
```

### Example 2: Systemd Service
Create `/etc/systemd/system/ota-daemon.service`:
```ini
[Unit]
Description=OTA Update Daemon
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=ota
ExecStart=/usr/local/bin/ota_daemon /etc/ota/daemon.json
Restart=on-failure
RestartSec=10

[Install]
WantedBy=multi-user.target
```

Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable ota-daemon
sudo systemctl start ota-daemon
sudo systemctl status ota-daemon
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Service won't start | Check file exists: `ls -la /path/to/firmware.bz2` |
| Daemon can't connect | Ensure service is running: `./ota_service <file>` |
| Config file error | Verify JSON syntax: `cat config.json \| jq .` |
| Decompression fails | Check disk space: `df -h /var/ota/` |
| MD5 mismatch | Network corruption detected; daemon retries next cycle |

## Compression Support

- **bzip2** (.bz2) - Supported via libbz2
- **gzip** (.gz) - Supported via zlib
- **uncompressed** - Pass through without decompression

## Key Features

✅ SomeIP RPC for network transparency  
✅ Chunked file transfer (configurable)  
✅ MD5 checksum verification  
✅ Daemon mode with continuous monitoring  
✅ Automatic decompression  
✅ JSON-based configuration  
✅ Version tracking (prevents reprocessing)  
✅ Systemd integration  
✅ Graceful error handling  

## Build Info

- **Project:** OTA_UpdateSystem
- **C++ Standard:** C++17
- **CMake:** 3.10+
- **Binaries:** `ota_service` (1.5 MB), `ota_daemon` (1.5 MB)
- **Status:** Production-ready ✓

---

For detailed information, see `example_daemon_config.json` for configuration examples.
