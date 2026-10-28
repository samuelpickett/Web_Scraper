#include "../src/article_parser.h"

#include <cassert>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

int main() {
    std::ifstream file("tests/fixtures/article.html", std::ios::binary);
    assert(file);
    const std::string html(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    const Article article = parse_article(html);

    assert(article.title == "Example & Test");
    assert(article.lead_paragraphs.size() == 2);
    assert(article.lead_paragraphs[0] == "Lead with a link and & an entity.");
    assert(article.lead_paragraphs[1] == "Second lead paragraph with x2.");
    assert(article.sections.size() == 3);
    assert(article.sections[0].heading == "History");
    assert(article.sections[0].paragraphs.size() == 1);
    assert(article.sections[0].paragraphs[0] == "Direct history text.");
    assert(article.sections[0].subsections.size() == 2);
    assert(article.sections[0].subsections[0].heading == "Early years");
    assert(article.sections[0].subsections[0].paragraphs[0] == "Early text with bold words.");
    assert(article.sections[0].subsections[0].subsections[0].heading == "Details");
    assert(article.sections[0].subsections[0].subsections[0].paragraphs[0] == "Detailed text.");
    assert(article.sections[0].subsections[1].heading == "Later years");
    assert(article.sections[0].subsections[1].paragraphs[0] == "Later text.");
    assert(article.sections[1].heading == "Legacy");
    assert(article.sections[1].paragraphs[0] == "Legacy text.");
    assert(article.sections[2].heading == "References");
    assert(article.sections[2].paragraphs.empty());

    for (const std::string invalid : {"", "<html><body><p>Other site</p></body></html>",
                                    "<html><body class='mw-parser-output'><p>No title</p></body></html>",
                                    "<html><head><title>Empty</title></head><body class='mw-parser-output'></body></html>"}) {
        bool rejected = false;
        try {
            (void)parse_article(invalid);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
    }
}
