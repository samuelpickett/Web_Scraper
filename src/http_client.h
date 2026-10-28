#pragma once

#include <string>

// Fetches one English Wikipedia article as HTML. Throws std::runtime_error on failure.
std::string fetch_article_html(const std::string& page_title, const std::string& contact);

// Produces the public article URL, using UTF-8 percent encoding for the title.
std::string article_url(const std::string& page_title);
