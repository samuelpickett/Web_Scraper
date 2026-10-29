#pragma once

#include "article_parser.h"

#include <filesystem>
#include <string>

// Keeps user-chosen names inside Scrapped_Pages and always gives them a .json extension.
std::filesystem::path article_output_path(const std::filesystem::path& application_folder,
                                        const std::wstring& file_name);

// Creates Scrapped_Pages on demand and replaces the output only after a complete write.
void save_article_json(const Article& article, const std::filesystem::path& output_path);
