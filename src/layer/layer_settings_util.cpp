/*
 * Copyright (c) 2023 Valve Corporation
 * Copyright (c) 2023 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Authors:
 * - Christophe Riccio <christophe@lunarg.com>
 */

#include "layer_settings_util.hpp"

#include <sstream>
#include <regex>
#include <cstdlib>
#include <cassert>

namespace vl {

const VkLayerSettingsCreateInfoEXT *FindSettingsInChain(const void *next) {
    const VkBaseOutStructure *current = reinterpret_cast<const VkBaseOutStructure *>(next);
    const VkLayerSettingsCreateInfoEXT *found = nullptr;
    while (current) {
        if (VK_STRUCTURE_TYPE_LAYER_SETTINGS_EXT == current->sType) {
            found = reinterpret_cast<const VkLayerSettingsCreateInfoEXT *>(current);
            current = nullptr;
        } else {
            current = current->pNext;
        }
    }
    return found;
}

std::vector<std::string> Split(const std::string &value, char delimiter) {
    std::vector<std::string> result;

    std::string parse = value;

    std::size_t start = 0;
    std::size_t end = parse.find(delimiter);
    while (end != std::string::npos) {
        result.push_back(parse.substr(start, end - start));
        start = end + 1;
        end = parse.find(delimiter, start);
    }

    const std::string last = parse.substr(start, end);
    if (!last.empty()) {
        result.push_back(last);
    }

    return result;
}

std::string GetFileSettingName(const char *pLayerName, const char *pSettingName) {
    assert(pLayerName != nullptr);
    assert(pSettingName != nullptr);

    std::stringstream settingName;
    settingName << vl::ToLower(TrimPrefix(pLayerName)) << "." << pSettingName;

    return settingName.str();
}

std::string GetEnvSettingName(const char *layer_key, const char *setting_key, TrimMode trim_mode) {
    std::stringstream result;

#if defined(__ANDROID__)
    switch (trim_mode) {
        default:
        case TRIM_NONE: {
            result << "debug.vulkan." << GetFileSettingName(layer_key, setting_key);
            break;
        }
        case TRIM_VENDOR: {
            result << "debug.vulkan." << GetFileSettingName(TrimVendor(layer_key).c_str(), setting_key);
            break;
        }
        case TRIM_NAMESPACE: {
            result << "debug.vulkan." << setting_key;
            break;
        }
    }
#else
    switch (trim_mode) {
        default:
        case TRIM_NONE: {
            result << "VK_" << vl::ToUpper(TrimPrefix(layer_key)) << "_" << vl::ToUpper(setting_key);
            break;
        }
        case TRIM_VENDOR: {
            result << "VK_" << vl::ToUpper(TrimVendor(layer_key)) << "_" << vl::ToUpper(setting_key);
            break;
        }
        case TRIM_NAMESPACE: {
            result << "VK_" << vl::ToUpper(setting_key);
            break;
        }
    }

#endif
    return result.str();
}

char GetEnvDelimiter() {
#ifdef WIN32 // a define is necessary because ':' is used for disk drives on Windows path
    return ';';
#else
    return ':';
#endif
}

char FindDelimiter(const std::string& s) {
    if (s.find(',') != std::string::npos) {
        return ',';
    } else if (s.find(GetEnvDelimiter()) != std::string::npos) {
        return GetEnvDelimiter();
    } else {
        return ',';
    }
}

std::string TrimWhitespace(const std::string &s) {
    const char *whitespace = " \t\f\v\n\r";

    const auto trimmed_beg = s.find_first_not_of(whitespace);
    if (trimmed_beg == std::string::npos) return "";

    const auto trimmed_end = s.find_last_not_of(whitespace);
    assert(trimmed_end != std::string::npos && trimmed_beg <= trimmed_end);

    return s.substr(trimmed_beg, trimmed_end - trimmed_beg + 1);
}

std::string TrimPrefix(const std::string &layer_key) {
    std::string key{};
    if (layer_key.find("VK_LAYER_") == 0) {
        std::size_t prefix = std::strlen("VK_LAYER_");
        key = layer_key.substr(prefix, layer_key.size() - prefix);
    } else {
        key = layer_key;
    }
    return key;
}

std::string TrimVendor(const std::string &layer_key) {
    static const char *separator = "_";

    const std::string &namespace_key = TrimPrefix(layer_key);

    const auto trimmed_beg = namespace_key.find_first_of(separator);
    if (trimmed_beg == std::string::npos) return namespace_key;

    assert(namespace_key.find_last_not_of(separator) != std::string::npos &&
           trimmed_beg <= namespace_key.find_last_not_of(separator));

    return namespace_key.substr(trimmed_beg + 1, namespace_key.size());
}

std::string ToLower(const std::string &s) {
    std::string result = s;
    for (auto &c : result) {
        c = (char)std::tolower(c);
    }
    return result;
}

std::string ToUpper(const std::string &s) {
    std::string result = s;
    for (auto &c : result) {
        c = (char)std::toupper(c);
    }
    return result;
}

VkFrameset ToFrameSet(const std::string &s) {
    assert(IsFrameSets(s));

    VkFrameset frameset{0, 1, 1};

    const std::vector<std::string> &frameset_split = vl::Split(s, '-');
    if (frameset_split.size() >= 1) {
        frameset.first = static_cast<std::uint32_t>(std::atoll(frameset_split[0].c_str()));
    }
    if (frameset_split.size() >= 2) {
        frameset.count = static_cast<std::uint32_t>(std::atoll(frameset_split[1].c_str()));
    }
    if (frameset_split.size() >= 3) {
        frameset.step = static_cast<std::uint32_t>(std::atoll(frameset_split[2].c_str()));
    }

    return frameset;
}

std::vector<VkFrameset> ToFrameSets(const std::string &s) {
    std::vector<std::string> tokens = Split(s, FindDelimiter(s));

    std::vector<VkFrameset> results;
    results.resize(tokens.size());
    for (std::size_t i = 0, n = tokens.size(); i < n; ++i) {
        results[i] = ToFrameSet(tokens[i]);
    }

    return results;
}

bool IsFrameSets(const std::string &s) {
    static const std::regex FRAME_REGEX("^([0-9]+([-][0-9]+){0,2})(,([0-9]+([-][0-9]+){0,2}))*$");

    return std::regex_search(s, FRAME_REGEX);
}

bool IsInteger(const std::string &s) {
    static const std::regex FRAME_REGEX("^-?([0-9]*|0x[0-9|a-z|A-Z]*)$");

    return std::regex_search(s, FRAME_REGEX);
}

bool IsFloat(const std::string &s) {
    static const std::regex FRAME_REGEX("^-?[0-9]*([.][0-9]*f?)?$");

    return std::regex_search(s, FRAME_REGEX);
}

std::string Format(const char *message, ...) {
    std::size_t const STRING_BUFFER(4096);

    assert(message != nullptr);
    assert(strlen(message) >= 1 && strlen(message) < STRING_BUFFER);

    char buffer[STRING_BUFFER];
    va_list list;

    va_start(list, message);
    vsnprintf(buffer, STRING_BUFFER, message, list);
    va_end(list);

    return buffer;
}

}  // namespace vl
