#include "system/SystemProbe.hpp"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <array>
#include <cstdio>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>

namespace nicx::system {

// ─── Helpers ────────────────────────────────────────────────────────────────

std::string SystemProbe::readFile(const std::string& path) const {
    std::ifstream f(path);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    auto s = ss.str();
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

std::string SystemProbe::execCapture(const std::string& cmd) const {
    std::array<char, 256> buf;
    std::string result;
    FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!pipe) return {};
    while (fgets(buf.data(), buf.size(), pipe))
        result += buf.data();
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

bool SystemProbe::pathExists(const std::string& path) const {
    return std::filesystem::exists(path);
}

bool SystemProbe::isDmcryptDevice(const std::string& device) const {
    // Check if device is backed by dm-crypt via /sys/block
    std::string devname = device;
    if (auto pos = devname.rfind('/'); pos != std::string::npos)
        devname = devname.substr(pos + 1);

    std::string dmPath = "/sys/block/" + devname;
    if (!pathExists(dmPath)) {
        // Try partition parent
        // strip trailing digits for partition → disk
        std::string parent = devname;
        while (!parent.empty() && std::isdigit(parent.back()))
            parent.pop_back();
        dmPath = "/sys/block/" + parent + "/" + devname;
    }

    // Check if any dm device in slaves is a crypt type
    std::string dmType = "/sys/block/" + devname + "/dm/uuid";
    std::string uuid = readFile(dmType);
    return uuid.rfind("CRYPT-", 0) == 0;
}

// ─── OS ─────────────────────────────────────────────────────────────────────

OsInfo SystemProbe::probeOs() const {
    OsInfo info;

    // /etc/os-release
    std::ifstream f("/etc/os-release");
    std::string line;
    while (std::getline(f, line)) {
        auto strip = [](std::string s) {
            if (s.size() >= 2 && s.front() == '"') s = s.substr(1, s.size() - 2);
            return s;
        };
        if (line.rfind("PRETTY_NAME=", 0) == 0) info.name    = strip(line.substr(12));
        if (line.rfind("ID=", 0) == 0)          info.id      = strip(line.substr(3));
        if (line.rfind("VERSION_ID=", 0) == 0)  info.version = strip(line.substr(11));
    }

    // kernel
    struct utsname uts {};
    if (uname(&uts) == 0) {
        info.kernel   = uts.release;
        info.hostname = uts.nodename;
    }

    // init system
    if (pathExists("/run/systemd/private"))
        info.init = "systemd";
    else if (pathExists("/run/openrc"))
        info.init = "openrc";
    else if (pathExists("/run/runit"))
        info.init = "runit";
    else if (pathExists("/run/s6"))
        info.init = "s6";
    else
        info.init = execCapture("ps -p 1 -o comm=");

    return info;
}

// ─── CPU ─────────────────────────────────────────────────────────────────────

CpuInfo SystemProbe::probeCpu() const {
    CpuInfo info;
    info.cores = 0;
    info.threads = 0;

    std::ifstream f("/proc/cpuinfo");
    std::string line;
    bool gotModel = false;

    while (std::getline(f, line)) {
        if (!gotModel && line.rfind("model name", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                info.model = line.substr(pos + 2);
                gotModel = true;
            }
        }
        if (line.rfind("processor", 0) == 0) info.threads++;
        if (line.rfind("cpu cores", 0) == 0 && info.cores == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos)
                info.cores = std::stoi(line.substr(pos + 2));
        }
    }

    struct utsname uts {};
    if (uname(&uts) == 0) info.architecture = uts.machine;

    return info;
}

// ─── Memory ─────────────────────────────────────────────────────────────────

MemoryInfo SystemProbe::probeMemory() const {
    MemoryInfo info{};

    std::ifstream f("/proc/meminfo");
    std::string line;
    uint64_t memTotal = 0, memAvail = 0, swapTotal = 0, swapFree = 0;

    auto parseKb = [](const std::string& l) -> uint64_t {
        auto pos = l.find(':');
        if (pos == std::string::npos) return 0;
        std::istringstream ss(l.substr(pos + 1));
        uint64_t v; ss >> v;
        return v * 1024;
    };

    while (std::getline(f, line)) {
        if (line.rfind("MemTotal:", 0) == 0)     memTotal  = parseKb(line);
        if (line.rfind("MemAvailable:", 0) == 0) memAvail  = parseKb(line);
        if (line.rfind("SwapTotal:", 0) == 0)    swapTotal = parseKb(line);
        if (line.rfind("SwapFree:", 0) == 0)     swapFree  = parseKb(line);
    }

    info.totalBytes     = memTotal;
    info.availableBytes = memAvail;
    info.usedBytes      = memTotal - memAvail;

    if (swapTotal > 0) {
        info.swapTotalBytes = swapTotal;
        info.swapUsedBytes  = swapTotal - swapFree;
    }

    return info;
}

// ─── Boot ────────────────────────────────────────────────────────────────────

BootInfo SystemProbe::probeBoot() const {
    BootInfo info;

    info.efi = pathExists("/sys/firmware/efi");
    info.bootMode = info.efi ? "EFI/UEFI" : "BIOS/Legacy";

    if (pathExists("/boot/grub") || pathExists("/boot/grub2"))
        info.bootloader = "GRUB";
    else if (pathExists("/boot/loader/loader.conf"))
        info.bootloader = "systemd-boot";
    else if (pathExists("/boot/EFI/refind") || pathExists("/boot/efi/EFI/refind"))
        info.bootloader = "rEFInd";
    else if (pathExists("/boot/syslinux"))
        info.bootloader = "Syslinux";
    else
        info.bootloader = "unknown";

    return info;
}

// ─── Disks ───────────────────────────────────────────────────────────────────

std::vector<DiskPartition> SystemProbe::probeDisks() const {
    std::vector<DiskPartition> parts;

    std::ifstream mounts("/proc/mounts");
    std::string line;

    while (std::getline(mounts, line)) {
        std::istringstream ss(line);
        std::string device, mountPoint, fs, opts;
        ss >> device >> mountPoint >> fs >> opts;

        // Skip pseudo and snap filesystems
        if (fs == "tmpfs" || fs == "devtmpfs" || fs == "sysfs" ||
            fs == "proc" || fs == "cgroup" || fs == "cgroup2" ||
            fs == "pstore" || fs == "efivarfs" || fs == "bpf" ||
            fs == "tracefs" || fs == "debugfs" || fs == "hugetlbfs" ||
            fs == "mqueue" || fs == "fusectl" || fs == "configfs" ||
            fs == "securityfs" || fs == "autofs" || fs == "ramfs" ||
            fs == "squashfs" ||
            device == "none" || device.rfind("systemd-", 0) == 0 ||
            mountPoint.rfind("/snap/", 0) == 0)
            continue;

        struct statvfs st {};
        if (statvfs(mountPoint.c_str(), &st) != 0) continue;

        DiskPartition p;
        p.device      = device;
        p.mountPoint  = mountPoint;
        p.filesystem  = fs;
        p.totalBytes  = static_cast<uint64_t>(st.f_blocks) * st.f_frsize;
        p.availableBytes = static_cast<uint64_t>(st.f_bavail) * st.f_frsize;
        p.usedBytes   = p.totalBytes - static_cast<uint64_t>(st.f_bfree) * st.f_frsize;
        p.encrypted   = isDmcryptDevice(device);

        if (p.totalBytes > 0)
            parts.push_back(std::move(p));
    }

    return parts;
}

// ─── Collect ────────────────────────────────────────────────────────────────

SystemInfo SystemProbe::collect() const {
    return SystemInfo{
        .os         = probeOs(),
        .cpu        = probeCpu(),
        .memory     = probeMemory(),
        .boot       = probeBoot(),
        .partitions = probeDisks(),
    };
}

} // namespace nicx::system
