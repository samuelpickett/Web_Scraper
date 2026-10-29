#include "article_parser.h"

#include <gumbo.h>

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using ParsedHtml = std::unique_ptr<GumboOutput, void (*)(GumboOutput*)>;

void destroy_html(GumboOutput* output) {
    if (output != nullptr) {
        gumbo_destroy_output(&kGumboDefaultOptions, output);
    }
}

bool is_element(const GumboNode* node) {
    return node != nullptr && node->type == GUMBO_NODE_ELEMENT;
}

std::string tag_name(const GumboNode* node) {
    if (!is_element(node)) {
        return {};
    }
    return gumbo_normalized_tagname(node->v.element.tag);
}

const GumboVector* children(const GumboNode* node) {
    if (node->type == GUMBO_NODE_DOCUMENT) {
        return &node->v.document.children;
    }
    if (is_element(node)) {
        return &node->v.element.children;
    }
    return nullptr;
}

const GumboNode* find_first(const GumboNode* node, const std::string& wanted) {
    if (tag_name(node) == wanted) {
        return node;
    }
    const GumboVector* list = children(node);
    if (list == nullptr) {
        return nullptr;
    }
    for (unsigned int i = 0; i < list->length; ++i) {
        if (const GumboNode* found = find_first(static_cast<const GumboNode*>(list->data[i]), wanted)) {
            return found;
        }
    }
    return nullptr;
}

std::string attribute(const GumboNode* node, const char* name) {
    if (!is_element(node)) {
        return {};
    }
    const GumboAttribute* value = gumbo_get_attribute(&node->v.element.attributes, name);
    return value == nullptr ? std::string{} : value->value;
}

bool has_class(const GumboNode* node, const std::string& wanted) {
    const std::string classes = attribute(node, "class");
    std::size_t start = 0;
    while (start < classes.size()) {
        while (start < classes.size() && std::isspace(static_cast<unsigned char>(classes[start]))) {
            ++start;
        }
        std::size_t end = start;
        while (end < classes.size() && !std::isspace(static_cast<unsigned char>(classes[end]))) {
            ++end;
        }
        if (classes.substr(start, end - start) == wanted) {
            return true;
        }
        start = end;
    }
    return false;
}

bool skip_element(const GumboNode* node) {
    const std::string name = tag_name(node);
    if (name == "table" || name == "figure" || name == "nav" || name == "aside" ||
        name == "ol" || name == "ul" || name == "dl" || name == "script" ||
        name == "style" || name == "footer" || name == "header") {
        return true;
    }
    if (name == "sup" && (has_class(node, "reference") || has_class(node, "mw-ref"))) {
        return true;
    }
    for (const char* class_name : {"infobox", "sidebar", "navbox", "hatnote", "thumb",
                                "toc", "reflist", "metadata", "shortdescription",
                                "mw-references-wrap", "mw-editsection", "mw-empty-elt"}) {
        if (has_class(node, class_name)) {
            return true;
        }
    }
    return false;
}

void append_text(const GumboNode* node, std::string& text) {
    if (node->type == GUMBO_NODE_TEXT || node->type == GUMBO_NODE_WHITESPACE ||
        node->type == GUMBO_NODE_CDATA) {
        text += node->v.text.text;
        return;
    }
    if (skip_element(node)) {
        return;
    }
    if (tag_name(node) == "br") {
        text += ' ';
        return;
    }
    const GumboVector* list = children(node);
    if (list != nullptr) {
        for (unsigned int i = 0; i < list->length; ++i) {
            append_text(static_cast<const GumboNode*>(list->data[i]), text);
        }
    }
}

std::string normalize_space(const std::string& input) {
    std::string result;
    bool pending_space = false;
    for (std::size_t i = 0; i < input.size(); ++i) {
        const unsigned char byte = static_cast<unsigned char>(input[i]);
        // Wikipedia uses nonbreaking and narrow nonbreaking spaces in prose.
        if (std::isspace(byte) ||
            (i + 1 < input.size() && byte == 0xC2 && static_cast<unsigned char>(input[i + 1]) == 0xA0) ||
            (i + 2 < input.size() && byte == 0xE2 && static_cast<unsigned char>(input[i + 1]) == 0x80 &&
            static_cast<unsigned char>(input[i + 2]) == 0xAF)) {
            pending_space = !result.empty();
            if (byte == 0xC2) {
                ++i;
            } else if (byte == 0xE2) {
                i += 2;
            }
            continue;
        }
        if (pending_space) {
            result += ' ';
            pending_space = false;
        }
        result += input[i];
    }
    return result;
}

std::string visible_text(const GumboNode* node) {
    std::string text;
    append_text(node, text);
    return normalize_space(text);
}

int heading_level(const GumboNode* node) {
    const std::string name = tag_name(node);
    if (name.size() == 2 && name[0] == 'h' && name[1] >= '2' && name[1] <= '6') {
        return name[1] - '0';
    }
    return 0;
}

struct HeadingLocation {
    int level;
    Section* section;
};

void collect_article(const GumboNode* node, Article& article,
                    std::vector<HeadingLocation>& headings) {
    if (skip_element(node)) {
        return;
    }

    const int level = heading_level(node);
    if (level != 0) {
        const std::string heading = visible_text(node);
        if (!heading.empty()) {
            while (!headings.empty() && headings.back().level >= level) {
                headings.pop_back();
            }
            std::vector<Section>& destination = headings.empty()
                ? article.sections : headings.back().section->subsections;
            destination.push_back(Section{heading, {}, {}});
            headings.push_back({level, &destination.back()});
        }
        return;
    }

    if (tag_name(node) == "p") {
        const std::string paragraph = visible_text(node);
        if (!paragraph.empty()) {
            if (headings.empty()) {
                article.lead_paragraphs.push_back(paragraph);
            } else {
                headings.back().section->paragraphs.push_back(paragraph);
            }
        }
        return;
    }

    const GumboVector* list = children(node);
    if (list != nullptr) {
        for (unsigned int i = 0; i < list->length; ++i) {
            collect_article(static_cast<const GumboNode*>(list->data[i]), article, headings);
        }
    }
}

std::size_t paragraph_count(const std::vector<Section>& sections) {
    std::size_t count = 0;
    for (const Section& section : sections) {
        count += section.paragraphs.size() + paragraph_count(section.subsections);
    }
    return count;
}

} // namespace

Article parse_article(const std::string& html) {
    if (html.empty()) {
        throw std::runtime_error("The response was empty.");
    }
    ParsedHtml parsed(gumbo_parse_with_options(&kGumboDefaultOptions, html.data(), html.size()),
                    destroy_html);
    if (parsed == nullptr || parsed->root == nullptr) {
        throw std::runtime_error("The HTML could not be parsed.");
    }

    const GumboNode* body = find_first(parsed->root, "body");
    if (body == nullptr || !has_class(body, "mw-parser-output")) {
        throw std::runtime_error("The response does not contain Wikipedia article content.");
    }
    const GumboNode* title_node = find_first(parsed->root, "title");
    const std::string title = title_node == nullptr ? std::string{} : visible_text(title_node);
    if (title.empty()) {
        throw std::runtime_error("The article title is missing.");
    }

    Article article{title, {}, {}};
    std::vector<HeadingLocation> headings;
    collect_article(body, article, headings);
    if (article.lead_paragraphs.empty() && paragraph_count(article.sections) == 0) {
        throw std::runtime_error("No article paragraphs were found.");
    }
    return article;
}
