#pragma once

#include "output/IRenderer.hpp"

namespace nicx::output {

// Concrete renderer: ANSI colors, aligned columns, pretty output.
class TerminalRenderer final : public IRenderer {
public:
    explicit TerminalRenderer(bool color = true);

    void header(std::string_view text) override;
    void section(std::string_view title) override;
    void row(std::string_view key, std::string_view value) override;
    void row(std::string_view key, std::string_view value, std::string_view status) override;
    void blank() override;
    void info(std::string_view text) override;
    void warn(std::string_view text) override;
    void error(std::string_view text) override;
    void success(std::string_view text) override;
    void rule() override;
    void pkg(std::string_view name, std::string_view version,
             std::string_view description, bool installed) override;
    void trackedPkg(std::string_view name, std::string_view pm,
                    std::string_view method, std::string_view notes) override;

private:
    bool m_color;
    static constexpr int KEY_WIDTH = 22;

    std::string colorize(std::string_view text, std::string_view code) const;
    std::string statusColor(std::string_view status) const;
};

} // namespace nicx::output
