#pragma once

#include <optional>
#include <string>

std::optional<std::string> getETag(const std::string& file);
void setETag(const std::string& file, const std::string& value);
