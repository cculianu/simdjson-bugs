#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <list>
#include <memory>
#include <string_view>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "simdjson/singleheader/simdjson.h"

const std::vector<std::string_view> files = {
    "block556034.json",
    "mempool10k.json",
    "semanticscholar-corpus.json",
    "user.json",
};

const std::vector<std::string_view> dirs2try = { ".", "jsondata", "../jsondata", "../simdjson-bugs/jsondata", };

using Buffer = std::string;

Buffer slurpFile(const std::string_view &name) {
    Buffer ret;
    for (const auto & dir : dirs2try) {
        auto *f = std::fopen((std::string{dir} + "/" + std::string{name}).c_str(), "rb");
        if (f) {
            struct AutoCloser {
                void operator()(std::FILE *f) const noexcept {
                    std::fclose(f);
                }
            };
            const std::unique_ptr<std::FILE, AutoCloser> holder{f};
            if (std::fseek(f, 0, SEEK_END))
                continue;
            auto size = std::ftell(f);
            if (size <= 0)
                continue;
            if (std::fseek(f, 0, SEEK_SET))
                continue;
            ret.resize(size, 0);
            if (auto res = std::fread(&ret[0], sizeof(ret[0]), ret.size(), f); res != ret.size())
                continue;
            return ret;
        }
    }
    throw std::runtime_error("Unable to find and/or open: \"" + std::string{name} + "\"");
}

void banner(const std::string_view &text)
{
    std::cout << "\n" << std::string(80, '=') << "\n";
    std::cout << text << "\n";
    std::cout << std::string(80, '-') << "\n";
}

void wtf()
{
    banner("Weird results -- observe same string serialized for *ALL*\n"
           ".. despite them all having come from different documents. Wtf?!");
    std::vector<std::pair<std::string, simdjson::dom::element>> results;
    simdjson::dom::parser parser; // it appears simdjson doesn't like this object to be re-used
    for (const auto & f : files) {
        auto buf = slurpFile(f);
        std::cout << "Read \"" << f << "\" - size: " << buf.size() << "\n";
        simdjson::dom::element parsed;
        auto error = parser.parse(buf).get(parsed);
        if (error) throw std::runtime_error(std::string{"Failed to parse "} + std::string{f});
        results.emplace_back(std::string{f}, std::move(parsed));
    }
    for (const auto & [name, parsed] : results) {
        std::cout << "File: \"" << name << "\"; serializing...\n";
        std::cout << "First 100 bytes, reserialized: " << simdjson::to_string(parsed).substr(0, 100) << " ...\n";
        std::cout << std::string(80, '-') << "\n";
    }
}

void crashy()
{
    banner("CRASHY!");
    // With this it crashes:
    std::vector<std::tuple<std::string, simdjson::dom::parser, simdjson::dom::element>> results;
    for (const auto & f : files) {
        auto buf = slurpFile(f);
        std::cout << "Read \"" << f << "\" - size: " << buf.size() << "\n";
        simdjson::dom::parser parser;
        simdjson::dom::element parsed;
        auto error = parser.parse(buf).get(parsed);
        if (error) throw std::runtime_error(std::string{"Failed to parse "} + std::string{f});
        results.emplace_back(std::string{f}, std::move(parser), std::move(parsed));
    }
    for (const auto & [name, parser, parsed] : results) {
        std::cout << "File: \"" << name << "\"; serializing...\n";
        std::cout << "First 100 bytes, reserialized: " << simdjson::to_string(parsed).substr(0, 100) << " ...\n";
        std::cout << std::string(80, '-') << "\n";
    }
}

void crashy2()
{
    banner("If the dom results outlive the parser -- also we crash! WHY?!");
    std::list<std::pair<std::string, simdjson::dom::element>> results;
    for (const auto & f : files) {
        auto buf = slurpFile(f);
        std::cout << "Read \"" << f << "\" - size: " << buf.size() << "\n";
        results.emplace_back();
        auto & [name, parsed] = results.back();
        name = f; // assign
        simdjson::dom::parser parser;
        auto error = parser.parse(buf).get(parsed);
        if (error) throw std::runtime_error(std::string{"Failed to parse "} + std::string{f});
    }
    // at this point the parser used for each result is dead.
    for (const auto & [name, parsed] : results) {
        std::cout << "File: \"" << name << "\"; serializing...\n";
        std::cout << "First 100 bytes, reserialized: " << simdjson::to_string(parsed).substr(0, 100) << " ...\n";
        std::cout << std::string(80, '-') << "\n";
    }
}


void notcrashy()
{
    banner("NOT CRASHY! (works as one would expect)");
    // If we do this it works:
    std::list<std::tuple<std::string, simdjson::dom::parser, simdjson::dom::element>> results;
    for (const auto & f : files) {
        auto buf = slurpFile(f);
        std::cout << "Read \"" << f << "\" - size: " << buf.size() << "\n";
        results.emplace_back();
        auto & [fname, parser, parsed] = results.back();
        fname = f; // assign
        auto error = parser.parse(buf).get(parsed);
        if (error) throw std::runtime_error(std::string{"Failed to parse "} + std::string{f});

    }
    std::cout << std::string(80, '-') << "\n";
    for (const auto & [name, parser, parsed] : results) {
        std::cout << "File: \"" << name << "\"; serializing...\n";
        std::cout << "First 100 bytes, reserialized: " << simdjson::to_string(parsed).substr(0, 100) << " ...\n";
        std::cout << std::string(80, '-') << "\n";
    }

}

int main() {
    // this succeeds as expected
    notcrashy();

    // weird results -- why the same string printed for all?!?!
    wtf();

    // this has a sigsegv on my Darwin macOS Mojave system with compiler: AppleClang 11.0.0.11000033
    // also crashes on Linux G++ 8.3.0.  I actually couldn't find a system it *doesn't* crash on!
    crashy();

    // (not reached unless you comment-out crashy() above)
    // this also crashes.. if the DOM results outlive the parser. BOOM!
    crashy2();

    return EXIT_SUCCESS;
}
