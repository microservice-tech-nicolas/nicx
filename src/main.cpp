#include "core/Application.hpp"
#include <span>

int main(int argc, char* argv[]) {
    nicx::core::Application app(std::span(argv, static_cast<std::size_t>(argc)));
    return app.run();
}
