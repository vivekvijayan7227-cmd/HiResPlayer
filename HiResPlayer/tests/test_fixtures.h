#pragma once
#include <string>
#include <filesystem>

inline std::wstring get_test_fixture_path(const std::wstring& filename) {
    namespace fs = std::filesystem;
    const fs::path candidates[] = {
        fs::path(L"tests/Fixtures") / filename,
        fs::path(L"tests/fixtures") / filename,
        fs::path(L"HiResPlayer/tests/Fixtures") / filename,
        fs::path(L"HiResPlayer/tests/fixtures") / filename,
        fs::path(L"../tests/Fixtures") / filename,
        fs::path(L"../tests/fixtures") / filename,
        fs::path(L"../../tests/Fixtures") / filename,
        fs::path(L"../../tests/fixtures") / filename,
        fs::path(L"Fixtures") / filename,
        fs::path(L"fixtures") / filename,
        fs::path(L"d:/AudioPlayer/HiResPlayer/tests/Fixtures") / filename,
        fs::path(L"d:/AudioPlayer/HiResPlayer/tests/fixtures") / filename
    };

    for (const auto& p : candidates) {
        std::error_code ec;
        if (fs::exists(p, ec)) {
            return fs::absolute(p, ec).wstring();
        }
    }
    return filename;
}
