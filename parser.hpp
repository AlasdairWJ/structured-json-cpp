#ifndef __JSON_PARSER_HPP
#define __JSON_PARSER_HPP

#include <type_traits>
#include <charconv>
#include <cctype>
#include <regex>
#include <array>
#include <span>

#include "json.hpp"

namespace json
{

namespace
{
const std::regex null_re{ "^null" };
const std::regex bool_re{ "^true|false" };
const std::regex number_re{ "^[+-]?(?:0|[1-9]\\d*)(?:\\.\\d+)?(?:[eE][+-]?\\d+)?" };
//const std::regex string_re{ "^\"(?:[^\\\\]|\\\\(?:\"|\\\\|\\/|b|f|n|r|t|u\\d{1,4}))*\"" };

auto get_inserter_iterator(BackInsertable auto& container) { return std::back_inserter(container); }

template <typename T> requires (Insertable<T> && !BackInsertable<T>)
auto get_inserter_iterator(T& container) { return std::inserter(container, container.end()); }

template <typename T, std::size_t N>
auto get_inserter_iterator(T (&arr)[N]) { return &arr[0]; }

auto get_inserter_iterator(char& c) { return &c; }

template <typename T>
constexpr std::size_t extent_v = std::dynamic_extent;

template <typename T, std::size_t N>
constexpr std::size_t extent_v<T[N]> = N;

template <typename T, std::size_t N>
constexpr std::size_t extent_v<std::array<T, N>> = N;

template <typename T>
constexpr std::size_t string_extent_v = extent_v<T>;

template <>
constexpr std::size_t string_extent_v<char> = 1;
}

struct parser
{
public:
	bool terminate_char_arrays = true;

	bool operator()(const std::string& line, auto& value, const auto& descriptor)
	{
		auto result = parse(line.cbegin(), line.cend(), value, descriptor);

		if (!result.success)
		{
			std::cout << "fail, dist: " << (result.it - line.begin()) << '\n';
		}

		return result.success;
	}

private:
	struct parse_result
	{
		std::string::const_iterator it;
		bool success;
	};

	using iterator = std::string::const_iterator;

	template <typename T, typename TDesc>
	parse_result parse(iterator begin, const iterator end, std::optional<T>& value, const TDesc& descriptor)
	{
		if (std::smatch match; std::regex_search(begin, end, null_re))
		{
			value.reset();
			return parse_result{ match[0].second, true };
		}
		
		return parse(begin, end, value.emplace(), descriptor);
	}

	parse_result parse(const iterator begin, const iterator end, Mandatory auto& value, const boolean_t&)
	{
		return parse_boolean(begin, end, value);
	}

	parse_result parse(const iterator begin, const iterator end, Mandatory auto& value, const number_t&)
	{
		return parse_number(begin, end, value);
	}

	parse_result parse(const iterator begin, const iterator end, Mandatory auto& value, const string_t&)
	{
		return parse_string(begin, end, value);
	}

	template <typename TValueDesc>
	parse_result parse(const iterator begin, const iterator end, Mandatory auto& value, const array<TValueDesc>& descriptor)
	{
		return parse_array(begin, end, value, descriptor.value_descriptor);
	}

	template <typename TValueDesc>
	parse_result parse(const iterator begin, const iterator end, Mandatory auto& value, const object<TValueDesc>& descriptor)
	{
		return parse_object(begin, end, value, descriptor.value_descriptor);
	}

	template <typename T, typename TFields> requires (is_field_list_v<TFields>)
	parse_result parse(const iterator begin, const iterator end, T& value, const TFields& fields)
	{
		return parse_fields(begin, end, value, fields);
	}

	template <typename T, typename TElements> requires (is_element_list_v<TElements>)
	parse_result parse(const iterator begin, const iterator end, T& value, const TElements& fields)
	{
		return parse_elements(begin, end, value, fields);
	}

	parse_result parse_boolean(const iterator begin, const iterator end, auto& value)
	{
		if (std::smatch match; std::regex_search(begin, end, match, bool_re))
		{
			value = match.str() == literals::true_;
			return parse_result{ match[0].second, true };
		}

		return parse_result{ begin, false };
	}

	parse_result parse_number(const iterator begin, const iterator end, auto& value)
	{
		if (std::smatch match; std::regex_search(begin, end, match, number_re))
		{
			const char* pBegin = &*begin;
			return parse_result{
				match[0].second,
				std::from_chars(pBegin, pBegin + (match[0].second - begin), value).ec == std::errc{}
			};
		}

		return parse_result{ begin, false };
	}

	template <typename T>
	parse_result parse_string(iterator it, const iterator end, T& value)
	{
		if (*it != '"' || ++it == end)
			return parse_result{ it, false };

		auto str_it = get_inserter_iterator(value);
		const bool needs_terminating = std::is_bounded_array_v<T> && terminate_char_arrays;

		for (std::size_t n{ needs_terminating }; it != end && *it != '"'; n++, ++it)
		{
			int value{};

			if (*it == '\\')
			{
				if (++it; it == end)
					return parse_result{ it, false };

				switch (*it)
				{
				case '"':  value = '"';  break;
				case '\\': value = '\\'; break;
				case '/':  value = '/';  break;
				case 'b':  value = '\b'; break;
				case 'f':  value = '\f'; break;
				case 'n':  value = '\n'; break;
				case 'r':  value = '\r'; break;
				case 't':  value = '\t'; break;
				case 'u': {
					++it;
					int n{};
					for (auto hex_it = it; n < 4 && hex_it != end && std::isxdigit(*hex_it); n++)
						++hex_it;
					if (n != 4)
						return parse_result{ it, false };
					std::from_chars(&*it, &*it + 4, value, 16);
					it += 3;
					break;
				}
				default:
					return parse_result{ it, false };
				}
			}
			else
			{
				value = *it;
			}

			if (n < string_extent_v<T>)
			{
				*str_it = value;
				++str_it;
			}
		}

		if (needs_terminating)
		{
			*str_it = '\0';
		}

		if (it == end || *it != '"')
			return parse_result{ it, false };

		return parse_result{ ++it, true };
	}

	bool skip_whitespace(iterator& it, const iterator end)
	{
		while (it != end && std::isspace(*it))
			++it;

		return it != end;
	}

	template <typename T>
	parse_result parse_array(iterator it, const iterator end, T& value, const auto& value_type_descriptor)
	{
		if (*it != '[')
			return parse_result{ it, false };

		if (!skip_whitespace(++it, end))
			return parse_result{ it, false };
		
		auto value_it = get_inserter_iterator(value);
		
		for (int n{}; it != end && *it != ']'; ++value_it, n++)
		{
			underlying_value_type_t<T> element{};
			const auto result = parse(it, end, element, value_type_descriptor);

			if (n < extent_v<T>)
			{
				*value_it = element;
			}

			if (!result.success)
				return result;

			if (!skip_whitespace(it = result.it, end))
				return parse_result{ it, false };

			if (*it == ',')
				skip_whitespace(++it, end);
		}

		if (it == end)
			return parse_result{ it, false };

		return parse_result{ it + 1, true };
	}

	template <typename T>
	parse_result parse_object(iterator it, const iterator end, T& value, const auto& value_type_descriptor)
	{
		using value_type = underlying_value_type_t<T>;
		using key_type = std::remove_const_t<typename value_type::first_type>;
		using mapped_type = value_type::second_type;

		if (*it != '{')
			return parse_result{ it, false };

		if (!skip_whitespace(++it, end))
			return parse_result{ it, false };

		auto value_it = get_inserter_iterator(value);

		for (std::size_t n{}; it != end && *it != '}'; ++value_it, n++)
		{
			key_type key{};

			const auto key_result = parse(it, end, key, string);

			if (!key_result.success)
				return key_result;

			if (!skip_whitespace(it = key_result.it, end))
				return parse_result{ it, false };

			if (*it != ':')
				return parse_result{ it, false };

			if (!skip_whitespace(++it, end))
				return parse_result{ it, false };

			mapped_type value{}; 

			const auto value_result = parse(it, end, value, value_type_descriptor);

			if (!value_result.success)
				return value_result;

			if (n < extent_v<T>)
			{
				*value_it = value_type{ key, value };
			}

			if (!skip_whitespace(it = value_result.it, end))
				return parse_result{ it, false };

			if (*it == ',')
				skip_whitespace(++it, end);
		}

		if (it == end)
			return parse_result{ it, false };

		return parse_result{ it + 1, true };
	}

	template <int Index, typename T, typename TFields> requires (is_field_list_v<TFields>)
	parse_result parse_field(const iterator begin, const iterator end, const std::string& field_name, T& value, const TFields& fields)
	{
		const auto& field = std::get<Index>(fields);
		if (field.name == field_name)
		{
			const auto& member_ptr = field.member_ptr;
			return parse(begin, end, value.*member_ptr, field.descriptor);
		}
		else
		{
			if constexpr (Index + 1 < std::tuple_size_v<TFields>)
			{
				return parse_field<Index + 1>(begin, end, field_name, value, fields);
			}
			else
			{
				return parse_result{ begin, false };
			}
		}
	}

	template <typename T, typename TFields> requires (is_field_list_v<TFields>)
	parse_result parse_fields(iterator it, const iterator end, T& value, const TFields& fields)
	{
		if (*it != '{')
			return parse_result{ it, false };

		if (!skip_whitespace(++it, end))
			return parse_result{ it, false };

		while (it != end && *it != '}')
		{
			std::string key;
			const auto key_result = parse(it, end, key, string);

			if (!key_result.success)
				return key_result;

			if (!skip_whitespace(it = key_result.it, end))
				return parse_result{ it, false };

			if (*it != ':')
				return parse_result{ it, false };

			if (!skip_whitespace(++it, end))
				return parse_result{ it, false };

			const auto value_result = parse_field<0>(it, end, key, value, fields);

			if (!value_result.success)
				return value_result;

			if (!skip_whitespace(it = value_result.it, end))
				return parse_result{ it, false };

			if (*it == ',')
				skip_whitespace(++it, end);
		}

		if (it == end)
			return parse_result{ it, false };

		return parse_result{ it + 1, true };
	}

	template <int Index, typename T, typename TElements> requires (is_element_list_v<TElements>)
	parse_result parse_element(iterator it, const iterator end, T& value, const TElements& elements)
	{
		const auto& element = std::get<Index>(elements);
		const auto& member_ptr = element.member_ptr;
		
		const auto result = parse(it, end, value.*member_ptr, element.descriptor);

		if (!result.success)
			return result;

		if (!skip_whitespace(it = result.it, end))
			return parse_result{ it, false };

		if constexpr (Index + 1 < std::tuple_size_v<TElements>)
		{
			if (*it != ',')
				return parse_result{ it, false };

			if (!skip_whitespace(++it, end))
				return parse_result{ it, false };

			return parse_element<Index + 1>(it, end, value, elements);
		}
		else
		{
			if (*it != ']')
				return parse_result{ it, false };

			return parse_result{ it + 1, true };
		}
	}
	template <typename T, typename TElements> requires (is_element_list_v<TElements>)
	parse_result parse_elements(iterator it, const iterator end, T& value, const TElements& elements)
	{
		if (*it != '[')
			return parse_result{ it, false };

		if (!skip_whitespace(++it, end))
			return parse_result{ it, false };

		return parse_element<0>(it, end, value, elements);
	}
};

} // json

#endif // __JSON_PARSER_HPP