#ifndef __JSON_STRINGIFIER_HPP
#define __JSON_STRINGIFIER_HPP

#include <sstream>
#include <charconv>

#include "json.hpp"

namespace json
{

namespace
{
template <typename T>
constexpr bool is_trivial_value_v = false;

template <> constexpr bool is_trivial_value_v<boolean_t> = true;
template <> constexpr bool is_trivial_value_v<number_t> = true;
template <> constexpr bool is_trivial_value_v<string_t> = true;
}

struct stringifier
{
public:
	bool dense{}; // removes spacing around elements in arrays and objects
	bool pretty{}; // adds newlines to arrays or objects containing arrays or objects

	std::string operator()(const auto& value, const auto& descriptor)
	{
		indent = 0;
		std::stringstream ss;
		stringify(ss, value, descriptor);
		return ss.str();
	}

	void operator()(std::ostream& os, const auto& value, const auto& descriptor)
	{
		indent = 0;
		stringify(os, value, descriptor);
	}

private:
	int indent{};

	inline void do_indent(std::ostream& os)
	{
		for (int i = 0; i < indent; ++i)
			os << '\t';
	}

	template <typename T, Descriptor TDesc>
	void stringify(std::ostream& os, const std::optional<T>& value, const TDesc& desc)
	{
		if (value)
		{
			stringify(os, *value, desc);
		}
		else
		{
			os << literals::null;
		}
	}

	void stringify(std::ostream& os, const Boolean auto& value, const boolean_t&)
	{
		os << (value ? literals::true_ : literals::false_);
	}

	void stringify(std::ostream& os, const Number auto& value, const number_t&)
	{
		os << value;
	}

	inline bool needs_escaping(const char& c, char& escapee)
	{
		switch (c)
		{
		case '\"':  escapee = '"';  return true;
		case '\\': escapee = '\\'; return true;
		case '/':  escapee = '/';  return true;
		case '\b':  escapee = 'b'; return true;
		case '\f':  escapee = 'f'; return true;
		case '\n':  escapee = 'n'; return true;
		case '\r':  escapee = 'r'; return true;
		case '\t':  escapee = 't'; return true;
		default:
			if (!std::isprint(c))
			{
				escapee = 'u';
				return true;
			}
			return false;
		}
	}

	inline void stringify_string(std::ostream& os, const char c)
	{
		if (char escapee{}; needs_escaping(c, escapee))
		{
			os << '\\' << escapee;

			if (escapee == 'u')
			{
				char xdigits[5]{};
				std::to_chars(xdigits, xdigits + 5, 0x10000u | c, 16);
				os.write(xdigits + 1, 4);
			}
		}
		else
		{
			os << c;
		}
	}

	inline void stringify_string(std::ostream& os, const char* value)
	{
		for (; *value != '\0'; ++value)
			stringify_string(os, *value);
	}

	template <typename T> requires (!std::is_same_v<T, char>)
	void stringify_string(std::ostream& os, const T& value)
	{
		for (const char& c : value)
		{
			if constexpr (std::is_bounded_array_v<T>)
			{
				if (c == '\0')
					break;
			}

			stringify_string(os, c);
		}
	}
	
	void stringify(std::ostream& os, const String auto& value, const string_t&)
	{
		os << '"';
		stringify_string(os, value);
		os << '"';
	}

	// only newline non-trivial value types
	template <typename TValue>
	void stringify(std::ostream& os, const Array auto& values, const array<TValue>& descriptor)
	{
		os << '[';

		if (std::ranges::empty(values))
		{
			os << "]";
			return;
		}

		bool first = true;

		if constexpr (is_trivial_value_v<TValue>)
		{
			if (!dense)
			{
				os << ' ';
			}
		}
		else 
		{
			if (pretty)
			{
				os << '\n';

				indent++;
				do_indent(os);
			}
		}

		for (const auto& value : values)
		{
			if (!first)
			{
				os << ',';

				if constexpr (is_trivial_value_v<TValue>)
				{
					if (!dense)
					{
						os << ' ';
					}
				}
				else
				{
					if (pretty)
					{
						os << '\n';
						do_indent(os);
					}
					else if (!dense)
					{
						os << ' ';
					}
				}
			}

			stringify(os, value, descriptor.value_descriptor);
			first = false;
		}

		if constexpr (is_trivial_value_v<TValue>)
		{
			if (!dense)
			{
				os << ' ';
			}
		}
		else
		{
			if (pretty)
			{
				os << '\n';

				indent--;
				do_indent(os);
			}
		}
		
		os << ']';
	}

	template <typename TValue, Descriptor TValueDesc>
	void stringify_key_pair_value(std::ostream& os, const String auto& key, const TValue& value, const TValueDesc& value_descriptor)
	{
		stringify(os, key, string);

		os << ':';

		if (!dense)
		{
			os << ' ';
		}

		stringify(os, value, value_descriptor);
	}

	template <Descriptor TValue>
	void stringify(std::ostream& os, const auto& values, const object<TValue>& descriptor)
	{
		os << '{';

		if (std::ranges::empty(values))
		{
			os << "}";
			return;
		}

		if constexpr (!is_trivial_value_v<TValue>)
		{
			if (pretty)
			{
				os << '\n';

				indent++;
				do_indent(os);
			}
		}
		else
		{
			if (!dense)
			{
				os << ' ';
			}
		}

		bool first = true;

		for (const auto& [key, value] : values)
		{
			if (!first)
			{
				os << ',';

				if constexpr (is_trivial_value_v<TValue>)
				{
					if (!dense)
					{
						os << ' ';
					}
				}
				else
				{
					if (pretty)
					{
						os << '\n';
						do_indent(os);
					}
					else if (!dense)
					{
						os << ' ';
					}
				}
			}

			stringify_key_pair_value(os, key, value, descriptor.value_descriptor);
			first = false;
		}

		if constexpr (is_trivial_value_v<TValue>)
		{
			if (!dense)
			{
				os << ' ';
			}
		}
		else
		{
			if (pretty)
			{
				os << '\n';

				indent--;
				do_indent(os);
			}
		}
		
		os << '}';
	}

	template <int Index, typename T, typename TFields> requires (is_field_list_v<TFields>)
	void stringify_fields(std::ostream& os, const T& value, const TFields& fields)
	{
		const auto& field = std::get<Index>(fields);
		const auto& member_ptr = field.member_ptr;

		stringify_key_pair_value(os, field.name, value.*member_ptr, field.descriptor);

		if constexpr (Index + 1 < std::tuple_size_v<TFields>)
		{
			os << ',';

			if (pretty)
			{
				os << '\n';
				do_indent(os);
			}
			else if (!dense)
			{
				os << ' ';
			}

			stringify_fields<Index + 1>(os, value, fields);
		}
	}

	template <typename T, typename TFields> requires (is_field_list_v<TFields>)
	void stringify(std::ostream& os, const T& value, const TFields& fields)
	{
		os << '{';
			
		if constexpr (std::tuple_size_v<TFields> != 0)
		{
			if (pretty)
			{
				os << '\n';

				indent++;
				do_indent(os);
			}
			else if (!dense)
			{
				os << ' ';
			}

			stringify_fields<0>(os, value, fields);
		}

		if (pretty)
		{
			os << '\n';

			indent--;
			do_indent(os);
		}
		else if (!dense)
		{
			os << ' ';
		}
			
		os << '}';
	}

	template <int Index, typename TElements> requires (is_element_list_v<TElements>)
	void stringify_elements(std::ostream& os, const auto& value, const TElements& elements)
	{
		const auto& element = std::get<Index>(elements);
		const auto& member_ptr = element.member_ptr;
		const auto& member_value = value.*member_ptr;

		stringify(os, member_value, element.descriptor);

		if constexpr (Index + 1 < std::tuple_size_v<TElements>)
		{
			os << ',';

			stringify_elements<Index + 1>(os, value, elements);
		}
	}

	template <typename TElements> requires (is_element_list_v<TElements>)
	void stringify(std::ostream& os, const auto& value, const TElements& elements)
	{
		os << '[';

		if constexpr (std::tuple_size_v<TElements> != 0)
		{
			stringify_elements<0>(os, value, elements);
		}

		os << ']';
	}
};

} // json

#endif // __JSON_STRINGIFIER_HPP