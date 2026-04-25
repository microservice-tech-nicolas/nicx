#include "commands/GpgCommand.hpp"
#include "core/CommandRegistry.hpp"
#include "output/TerminalRenderer.hpp"
#include <cstdlib>
#include <array>
#include <cstdio>
#include <sstream>
#include <string>

namespace nicx::commands {

// ─── Self-registration ───────────────────────────────────────────────────────

static core::CommandRegistrar s_reg{
    "gpg",
    [] {
        return std::make_unique<GpgCommand>(
            std::make_unique<output::TerminalRenderer>());
    }
};

// ─── Helpers ─────────────────────────────────────────────────────────────────

static std::string capture(const std::string& cmd) {
    std::array<char, 512> buf{};
    std::string out;
    FILE* p = popen(cmd.c_str(), "r");
    if (!p) return {};
    while (fgets(buf.data(), buf.size(), p)) out += buf.data();
    pclose(p);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

static bool inPath(const char* bin) {
    return std::system((std::string("command -v ") + bin + " >/dev/null 2>&1").c_str()) == 0;
}

// ─── Construction ─────────────────────────────────────────────────────────────

GpgCommand::GpgCommand(std::unique_ptr<output::IRenderer> renderer)
    : m_renderer(std::move(renderer)) {}

std::string_view GpgCommand::name()        const { return "gpg"; }
std::string_view GpgCommand::description() const { return "GPG key management"; }

std::vector<std::string_view> GpgCommand::subcommands() const {
    return {"setup", "list", "status", "export", "import"};
}

int GpgCommand::execute(std::span<const std::string_view> args) {
    if (args.empty()) { printHelp(); return 0; }

    auto sub = args[0];
    if (sub == "setup")  return runSetup();
    if (sub == "list")   return runList();
    if (sub == "status") return runStatus();
    if (sub == "export") {
        if (args.size() < 2) { m_renderer->error("export requires a key ID"); return 1; }
        return runExport(args[1]);
    }
    if (sub == "import") {
        if (args.size() < 2) { m_renderer->error("import requires a file path"); return 1; }
        return runImport(args[1]);
    }

    m_renderer->error(std::string("unknown subcommand: ") + std::string(sub));
    printHelp();
    return 1;
}

void GpgCommand::printHelp() const {
    m_renderer->section("nicx gpg — GPG key management");
    m_renderer->row("setup",         "Ensure gpg is installed; guide key generation");
    m_renderer->row("list",          "List available secret keys");
    m_renderer->row("status",        "Show which key is configured for git signing / pass");
    m_renderer->row("export <id>",   "Export a public key to stdout");
    m_renderer->row("import <file>", "Import a key from file");
}

// ─── Subcommands ─────────────────────────────────────────────────────────────

int GpgCommand::runSetup() {
    m_renderer->section("GPG Setup");

    if (!inPath("gpg")) {
        m_renderer->warn("gpg is not installed");
        m_renderer->info("Install it with: nicx pkg install gnupg2");
        return 1;
    }
    m_renderer->row("gpg", "installed");

    std::string keys = capture("gpg --list-secret-keys --keyid-format LONG 2>/dev/null");
    if (keys.empty() || keys.find("sec") == std::string::npos) {
        m_renderer->warn("No GPG secret keys found");
        m_renderer->info("Generate a key with:");
        m_renderer->info("  gpg --full-generate-key");
        m_renderer->info("Recommended: RSA 4096. After generation run 'nicx gpg status'.");
        return 0;
    }

    m_renderer->success("GPG keys present — run 'nicx gpg list' to see them");
    return 0;
}

int GpgCommand::runList() {
    m_renderer->section("GPG Secret Keys");

    if (!inPath("gpg")) { m_renderer->error("gpg is not installed"); return 1; }

    std::string out = capture("gpg --list-secret-keys --keyid-format LONG 2>/dev/null");
    if (out.empty() || out.find("sec") == std::string::npos) {
        m_renderer->info("No secret keys found. Run 'nicx gpg setup'.");
        return 0;
    }

    std::istringstream ss(out);
    std::string line;
    while (std::getline(ss, line))
        if (!line.empty()) m_renderer->info(line);

    return 0;
}

int GpgCommand::runStatus() {
    m_renderer->section("GPG Status");

    if (!inPath("gpg")) { m_renderer->row("gpg", "NOT installed"); return 1; }
    m_renderer->row("gpg", "installed");

    std::string signingKey = capture("git config --global user.signingkey 2>/dev/null");
    m_renderer->row("git signing key", signingKey.empty() ? "not configured" : signingKey);

    std::string gpgSign = capture("git config --global commit.gpgsign 2>/dev/null");
    m_renderer->row("commit.gpgsign", gpgSign.empty() ? "false" : gpgSign);

    std::string gpgId = capture("cat ~/.password-store/.gpg-id 2>/dev/null");
    m_renderer->row("pass gpg-id", gpgId.empty() ? "not configured" : gpgId);

    std::string keyCount = capture(
        "gpg --list-secret-keys --keyid-format LONG 2>/dev/null | grep -c '^sec' || true");
    m_renderer->row("secret keys", keyCount.empty() ? "0" : keyCount);

    return 0;
}

int GpgCommand::runExport(std::string_view keyId) {
    m_renderer->section("Export GPG Public Key");
    if (!inPath("gpg")) { m_renderer->error("gpg is not installed"); return 1; }
    m_renderer->info("Exporting key: " + std::string(keyId));
    return std::system(("gpg --armor --export " + std::string(keyId)).c_str());
}

int GpgCommand::runImport(std::string_view file) {
    m_renderer->section("Import GPG Key");
    if (!inPath("gpg")) { m_renderer->error("gpg is not installed"); return 1; }
    m_renderer->info("Importing from: " + std::string(file));
    return std::system(("gpg --import " + std::string(file)).c_str());
}

} // namespace nicx::commands
