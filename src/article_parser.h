#pragma once

#include <string>
#include <vector>

struct Section {
    std::string heading;
    std::vector<std::string> paragraphs;
    std::vector<Section> subsections;
};

struct Article {
    std::string title;
    std::vector<std::string> lead_paragraphs;
    std::vector<Section> sections;
};

// Throws std::runtime_error when the response is not a usable Wikipedia article.
Article parse_article(const std::string& html);
