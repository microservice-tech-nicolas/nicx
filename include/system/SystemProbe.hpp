#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace nicx::system {

struct CpuInfo {
    std::string model;
    int cores;
    int threads;
    std::string architecture;
};

struct MemoryInfo {
    uint64_t totalBytes;
    uint64_t availableBytes;
    uint64_t usedBytes;
    std::optional<uint64_t> swapTotalBytes;
    std::optional<uint64_t> swapUsedBytes;
};

struct DiskPartition {
    std::string device;
    std::string mountPoint;
    std::string filesystem;
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t availableBytes;
    bool encrypted;
};

struct BootInfo {
    std::string bootloader;   // grub, systemd-boot, refind, unknown
    bool efi;
    std::string bootMode;     // EFI / BIOS
};

struct OsInfo {
    std::string name;
    std::string id;           // fedora, arch, nixos, ubuntu...
    std::string version;
    std::string kernel;
    std::string hostname;
    std::string init;         // systemd, openrc, runit...
};

struct SystemInfo {
    OsInfo os;
    CpuInfo cpu;
    MemoryInfo memory;
    BootInfo boot;
    std::vector<DiskPartition> partitions;
};

// Single Responsibility: only probes — no formatting, no output.
class SystemProbe {
public:
    SystemInfo collect() const;

private:
    OsInfo probeOs() const;
    CpuInfo probeCpu() const;
    MemoryInfo probeMemory() const;
    BootInfo probeBoot() const;
    std::vector<DiskPartition> probeDisks() const;

    std::string readFile(const std::string& path) const;
    std::string execCapture(const std::string& cmd) const;
    bool pathExists(const std::string& path) const;
    bool isDmcryptDevice(const std::string& device) const;
};

} // namespace nicx::system
