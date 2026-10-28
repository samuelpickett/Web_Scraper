#include "article_parser.h"
#include "http_client.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {

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

void write_json(const std::filesystem::path& output_path, const nlohmann::json& json) {
    const std::filesystem::path temporary_path = output_path.string() + ".tmp";
    try {
        std::ofstream file(temporary_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            throw std::runtime_error("Could not open the output file for writing.");
        }
        file << json.dump(2, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
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

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: web_scraper <Wikipedia page title> <output.json> <contact email or website>\n";
        return 1;
    }

    try {
        const std::string title = argv[1];
        if (title.empty()) {
            throw std::runtime_error("The page title cannot be empty.");
        }
        const std::filesystem::path output_path = argv[2];
        if (output_path.empty()) {
            throw std::runtime_error("The output path cannot be empty.");
        }
        const Article article = parse_article(fetch_article_html(title, argv[3]));
        write_json(output_path, article_json(article));
        std::cout << "Saved '" << article.title << "' to " << output_path.string() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 2;
    }
}
