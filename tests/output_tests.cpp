#include "../src/article_output.h"

#include <nlohmann/json.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

int main() {
    const std::filesystem::path test_folder = std::filesystem::current_path() / "tests" / "temporary_output";
    const std::filesystem::path output = article_output_path(test_folder, L" Ada Lovelace.JSON ");
    assert(output == test_folder / "Scrapped_Pages" / "Ada Lovelace.json");

    for (const std::wstring invalid : {L"", L"..", L"CON", L"../other", L"page:name"}) {
        bool rejected = false;
        try {
            (void)article_output_path(test_folder, invalid);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }

    Article article{"Ada Lovelace", {"Lead paragraph."},
                    {{"History", {"Direct paragraph."}, {{"Early years", {"Child paragraph."}, {}}}}}};
    save_article_json(article, output);
    std::ifstream file(output, std::ios::binary);
    assert(file);
    const nlohmann::json saved = nlohmann::json::parse(file);
    assert(saved.at("title") == "Ada Lovelace");
    assert(saved.at("lead_paragraphs").at(0) == "Lead paragraph.");
    assert(saved.at("sections").at(0).at("paragraphs").at(0) == "Direct paragraph.");
    assert(saved.at("sections").at(0).at("subsections").at(0).at("paragraphs").at(0) == "Child paragraph.");
    assert(saved.at("source_url") == "https://en.wikipedia.org/wiki/Ada%20Lovelace");
    file.close();
    std::filesystem::remove(output);
    std::filesystem::remove(output.parent_path());
    std::filesystem::remove(test_folder);
}
