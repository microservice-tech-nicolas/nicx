#pragma once

#include <string_view>
#include <vector>
#include <utility>

namespace nicx::output {

// Abstract renderer — DIP: commands depend on this, not on a concrete terminal impl.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual void header(std::string_view text) = 0;
    virtual void section(std::string_view title) = 0;
    virtual void row(std::string_view key, std::string_view value) = 0;
    virtual void row(std::string_view key, std::string_view value, std::string_view status) = 0;
    virtual void blank() = 0;
    virtual void info(std::string_view text) = 0;
    virtual void warn(std::string_view text) = 0;
    virtual void error(std::string_view text) = 0;
    virtual void success(std::string_view text) = 0;
    virtual void rule() = 0;
};

} // namespace nicx::output
