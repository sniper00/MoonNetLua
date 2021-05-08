#pragma once
#include <cstdint>
#include <unordered_map>
#include "string.hpp"
#include "buffer_view.hpp"
#include "utils.hpp"

namespace moon
{
    namespace http
    {
        using  case_insensitive_multimap = std::unordered_multimap<std::string, std::string, moon::ihash_string_view_functor_t, moon::iequal_string_view_functor_t>;
        using  case_insensitive_multimap_view = std::unordered_multimap<std::string_view, std::string_view, moon::ihash_string_view_functor_t, moon::iequal_string_view_functor_t>;

        class percent {
        public:
            /// Returns percent-encoded string
            static std::string encode(std::string_view value) noexcept {
                static auto hex_chars = "0123456789ABCDEF";

                std::string result;
                result.reserve(value.size()); // Minimum size of result

                for (const auto &chr : value) {
                    if (!((chr >= '0' && chr <= '9') || (chr >= 'A' && chr <= 'Z') || (chr >= 'a' && chr <= 'z') || chr == '-' || chr == '.' || chr == '_' || chr == '~'))
                        result += std::string("%") + hex_chars[static_cast<unsigned char>(chr) >> 4] + hex_chars[static_cast<unsigned char>(chr) & 15];
                    else
                        result += chr;
                }

                return result;
            }

            /// Returns percent-decoded string
            static std::string decode(std::string_view value) noexcept {
                std::string result;
                result.reserve(value.size() / 3 + (value.size() % 3)); // Minimum size of result
                std::errc ec;
                for (std::size_t i = 0; i < value.size(); ++i) {
                    auto &chr = value[i];
                    if (chr == '%' && i + 2 < value.size()) {
                        auto hex = value.substr(i + 1, 2);
                        auto decoded_chr = moon::string_convert<uint8_t>(hex, ec, 16);
                        result += static_cast<char>(decoded_chr);
                        i += 2;
                    }
                    else if (chr == '+')
                        result += ' ';
                    else
                        result += chr;
                }

                return result;
            }
        };

        /// Query string creation and parsing
        class query_string {
        public:
            /// Returns query string created from given field names and values
            static std::string create(const case_insensitive_multimap &fields) noexcept {
                std::string result;

                bool first = true;
                for (auto &field : fields) {
                    result += (!first ? "&" : "") + field.first + '=' + percent::encode(field.second);
                    first = false;
                }

                return result;
            }

            /// Returns query keys with percent-decoded values.
            static case_insensitive_multimap parse(std::string_view query_string) noexcept {
                case_insensitive_multimap result;

                if (query_string.empty())
                    return result;

                std::size_t name_pos = 0;
                auto name_end_pos = std::string::npos;
                auto value_pos = std::string::npos;
                for (std::size_t c = 0; c < query_string.size(); ++c) {
                    if (query_string[c] == '&') {
                        auto name = query_string.substr(name_pos, (name_end_pos == std::string::npos ? c : name_end_pos) - name_pos);
                        if (!name.empty()) {
                            auto value = value_pos == std::string::npos ? std::string() : query_string.substr(value_pos, c - value_pos);
                            result.emplace(std::move(name), percent::decode(value));
                        }
                        name_pos = c + 1;
                        name_end_pos = std::string::npos;
                        value_pos = std::string::npos;
                    }
                    else if (query_string[c] == '=') {
                        name_end_pos = c;
                        value_pos = c + 1;
                    }
                }
                if (name_pos < query_string.size()) {
                    auto name = query_string.substr(name_pos, name_end_pos - name_pos);
                    if (!name.empty()) {
                        auto value = value_pos >= query_string.size() ? std::string() : query_string.substr(value_pos);
                        result.emplace(std::move(name), percent::decode(value));
                    }
                }

                return result;
            }
        };

        class http_header {
        public:
            /// Parse header fields
            static case_insensitive_multimap_view parse(std::string_view sv) noexcept {
                case_insensitive_multimap_view result;
                buffer_view br(sv.data(), sv.size());
                std::string_view line = br.readline();
                size_t param_end = 0;
                while ((param_end = line.find(':')) != std::string_view::npos) {
                    size_t value_start = param_end + 1;
                    if (value_start < line.size()) {
                        if (line[value_start] == ' ')
                            value_start++;
                        if (value_start < line.size())
                            result.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start));
                    }
                    line = br.readline();
                }
                return result;
            }
        };

        class request_parser
        {
        public:
            /// Parse request line and header fields
            static bool parse(std::string_view sv, std::string_view& method, std::string_view& path, std::string_view& query_string, std::string_view& http_version, case_insensitive_multimap_view& header) noexcept
            {
                buffer_view br(sv.data(), sv.size());
                auto line = br.readline();

                size_t method_end;
                if ((method_end = line.find(' ')) != std::string_view::npos)
                {
                    method = line.substr(0, method_end);
                    size_t query_start = std::string_view::npos;
                    size_t path_and_query_string_end = std::string_view::npos;
                    for (size_t i = method_end + 1; i < line.size(); ++i)
                    {
                        if (line[i] == '?' && (i + 1) < line.size())
                        {
                            query_start = i + 1;
                        }
                        else if (line[i] == ' ')
                        {
                            path_and_query_string_end = i;
                            break;
                        }
                    }
                    if (path_and_query_string_end != std::string_view::npos)
                    {
                        if (query_start != std::string_view::npos) {
                            path = line.substr(method_end + 1, query_start - method_end - 2);
                            query_string = line.substr(query_start, path_and_query_string_end - query_start);
                        }
                        else
                            path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);
                        size_t protocol_end;
                        if ((protocol_end = line.find('/', path_and_query_string_end + 1)) != std::string_view::npos) {
                            if (line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "HTTP") != 0)
                                return false;
                            http_version = line.substr(protocol_end + 1, line.size() - protocol_end - 1);
                        }
                        else
                            return false;

                        header = http_header::parse(std::string_view{ br.data(),br.size() });
                    }
                    else
                        return false;
                }
                else
                    return false;
                return true;
            }
        };

        class response_parser
        {
        public:
            /// Parse status line and header fields
            static bool parse(std::string_view sv, std::string_view& version, std::string_view&status_code, case_insensitive_multimap_view& header) noexcept
            {
                buffer_view br(sv.data(), sv.size());
                auto line = br.readline();
                std::size_t version_end;
                if (!line.empty() && (version_end = line.find(' ')) != std::string_view::npos) {
                    if (5 < line.size())
                        version = line.substr(5, version_end - 5);
                    else
                        return false;
                    if ((version_end + 1) < line.size())
                        status_code = line.substr(version_end + 1, line.size() - (version_end + 1));
                    else
                        return false;

                    header = http_header::parse(std::string_view{ br.data(),br.size() });
                }
                else
                    return false;
                return true;
            }
        };
    }
   
}

