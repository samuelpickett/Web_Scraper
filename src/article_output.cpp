#include "article_output.h"
#include "http_client.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

std::wstring trim(const std::wstring& text) {
    const auto first = std::find_if_not(text.begin(), text.end(), [](wchar_t c) { return std::iswspace(c); });
    if (first == text.end()) {
        return {};
    }
    const auto last = std::find_if_not(text.rbegin(), text.rend(), [](wchar_t c) { return std::iswspace(c); }).base();
    return {first, last};
}

bool ends_with_json(const std::wstring& name) {
    if (name.size() < 5) {
        return false;
    }
    const std::wstring suffix = name.substr(name.size() - 5);
    return std::towlower(suffix[0]) == L'.' && std::towlower(suffix[1]) == L'j' &&
        std::towlower(suffix[2]) == L's' && std::towlower(suffix[3]) == L'o' &&
        std::towlower(suffix[4]) == L'n';
}

bool reserved_windows_name(const std::wstring& name) {
    std::wstring base = name.substr(0, name.find(L'.'));
    std::transform(base.begin(), base.end(), base.begin(), [](wchar_t c) { return std::towupper(c); });
    if (base == L"CON" || base == L"PRN" || base == L"AUX" || base == L"NUL") {
        return true;
    }
    return base.size() == 4 &&
        (base.substr(0, 3) == L"COM" || base.substr(0, 3) == L"LPT") &&
        base[3] >= L'1' && base[3] <= L'9';
}

nlohmann::json section_json(const Section& section) {
    nlohmann::json nested = nlohmann::json::array();
    for (const Section& child : section.subsections) {
        nested.push_back(section_json(child));
    }
    return {{"heading", section.heading},
            {"paragraphs", section.paragraphs},
            {"subsections", nested}};
}

nlohmann::json article_json(const Article& article) {
    nlohmann::json sections = nlohmann::json::array();
    for (const Section& section : article.sections) {
        sections.push_back(section_json(section));
    }
    return {{"title", article.title},
            {"source_url", article_url(article.title)},
            {"lead_paragraphs", article.lead_paragraphs},
            {"sections", sections}};
}

} // namespace

std::filesystem::path article_output_path(const std::filesystem::path& application_folder,
                                        const std::wstring& file_name) {
    std::wstring name = trim(file_name);
    if (ends_with_json(name)) {
        name.resize(name.size() - 5);
    }
    if (name.empty() || name == L"." || name == L".." || name.back() == L'.' ||
        reserved_windows_name(name)) {
        throw std::runtime_error("Enter a valid file name, such as ada_lovelace.");
    }
    for (wchar_t c : name) {
        if (c < 32 || std::wstring(L"\\/:*?\"<>|").find(c) != std::wstring::npos) {
            throw std::runtime_error("File names cannot contain paths or Windows reserved characters.");
        }
    }
    return application_folder / L"Scrapped_Pages" / (name + L".json");
}

void save_article_json(const Article& article, const std::filesystem::path& output_path) {
    std::filesystem::create_directories(output_path.parent_path());
    const std::filesystem::path temporary_path = output_path.wstring() + L".tmp";
    try {
        std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Could not open the output file for writing.");
        }
        file << article_json(article).dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
        file.flush();
        if (!file) {
            throw std::runtime_error("Could not write the complete JSON file.");
        }
        file.close();
        if (!file) {
            throw std::runtime_error("Could not close the JSON file.");
        }
        std::filesystem::rename(temporary_path, output_path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary_path, ignored);
        throw;
    }
}
