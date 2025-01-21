#ifndef __JSON_HPP
#define __JSON_HPP

#include <type_traits>
#include <string_view>
#include <tuple>
#include <cctype>
#include <charconv>
#include <sstream>
#include <algorithm>

namespace json
{

constexpr struct Boolean {} boolean;
constexpr struct Number {} number;
constexpr struct String {} string;

template <typename T>
struct array { const T element; };

template <typename T>
array(const T&) -> array<T>;

template <typename T>
struct object { const T element; };

template <typename T>
object(const T&) -> object<T>;

template <typename T>
struct is_descriptor : std::false_type {};

template <typename T>
constexpr bool is_descriptor_v = is_descriptor<T>::value;

template <> struct is_descriptor<Boolean> : std::true_type {};
template <> struct is_descriptor<Number> : std::true_type {};
template <> struct is_descriptor<String> : std::true_type {};
template <typename T> struct is_descriptor<array<T>> : std::bool_constant<is_descriptor_v<T>> {};
template <typename T> struct is_descriptor<object<T>> : std::bool_constant<is_descriptor_v<T>> {};

template <typename T, typename D>
struct is_valid_descriptor_for : std::false_type {};

template <typename T> struct is_valid_descriptor_for<T, Boolean> : std::bool_constant<std::is_convertible_v<bool, T> && std::is_convertible_v<T, bool>> {};
template <typename T> struct is_valid_descriptor_for<T, Number> : std::bool_constant<std::is_arithmetic_v<T>> {};
template <> struct is_valid_descriptor_for<char, String> : std::true_type {};
template <int N> struct is_valid_descriptor_for<char[N], String> : std::true_type {};
template <typename T> struct is_valid_descriptor_for<T, String> : std::bool_constant<std::is_convertible_v<std::string, T>> {};

template <typename T, typename D>
constexpr bool is_valid_descriptor_for_v = is_valid_descriptor_for<T, D>::value;

template <typename T>
struct is_trivial : std::false_type {};

template <typename T>
constexpr bool is_trivial_v = is_trivial<T>::value;

template <> struct is_trivial<Boolean> : std::true_type {};
template <> struct is_trivial<Number> : std::true_type {};
template <> struct is_trivial<String> : std::true_type {};

template <typename T, typename E, typename D>
struct field
{
	const std::string_view name;
	E T::* member_ptr;
	D descriptor;
};

template <typename T, typename E, typename D>
field(const char* n, E T::* m, const D& d) -> field<T, E, D>;

template <typename T, typename E, typename D>
field(const std::string_view, E T::* m, const D& d) -> field<T, E, D>;

#ifndef __cpp_lib_type_identity
template<class T>
struct type_identity { using type = T; };
#else
template<class T>
using type_identity = std::type_identity<T>;
#endif

template <typename T>
struct array_value_type : type_identity<typename T::value_type> {};

template <typename T, std::size_t N>
struct array_value_type<T[N]> : type_identity<T> {};

template <typename T>
using array_value_type_t = typename array_value_type<T>::type;

template <typename T>
struct object_value_type : type_identity<typename T::value_type::second_type> {};

template <typename T>
using object_value_type_t = typename object_value_type<T>::type;

namespace literals
{
constexpr std::string_view null{ "null" };
constexpr std::string_view true_{ "true" };
constexpr std::string_view false_{ "false" };
}

template <typename T>
struct string_assigner
{
	void operator()(T& value, const std::string& newValue)
	{
		value = newValue;
	}
};

template <>
struct string_assigner<char>
{
	void operator()(char& value, const std::string& newValue)
	{
		value = newValue.front();
	}
};

template <int N>
struct string_assigner<char[N]>
{
	void operator()(char (&value)[N], const std::string& newValue)
	{
		auto it = std::copy_n(newValue.begin(), std::min(N, static_cast<int>(newValue.size())), std::begin(value));
		if (it < std::end(value))
			*it = '\0';
	}
};

template <typename T, typename E = array_value_type_t<T>>
struct array_inserter
{	
	void operator()(T& values, const E& element) const
	{
		values.push_back(element);
	}
};

template <typename E, int N>
struct array_inserter<E[N]>
{
	void operator()(E (&values)[N], const E& element)
	{
		values[_index++] = element;
	}

private:
	int _index{};
};

template <typename T, typename E = object_value_type_t<T>>
struct object_inserter
{
	void operator()(T& values, const std::string_view key, const E& element) const
	{
		values.emplace(key, element);
	}
};

using iterator = std::string_view::const_iterator;

struct parse_result
{
	iterator it;
	bool success;
};

struct Formatting
{
	bool dense;
	bool newline_elements;
	bool newline_trivial_arrays;
};

constexpr Formatting pretty{ false, true, false };
constexpr Formatting dense{ true, false, false };

namespace impl
{

inline std::string_view string_view(const iterator begin, const iterator end)
{
	return std::string_view{ &*begin, static_cast<std::size_t>(end - begin) };
}

inline bool needs_escaping(const char c, char& escapee)
{
	switch (c)
	{
	case '\0': escapee = '0';  return true;
	case '\"': escapee = '"';  return true;
	case '\\': escapee = '\\'; return true;
	case '/':  escapee = '/';  return true;
	case '\b': escapee = 'b';  return true;
	case '\f': escapee = 'f';  return true;
	case '\n': escapee = 'n';  return true;
	case '\r': escapee = 'r';  return true;
	case '\t': escapee = 't';  return true;
	default:
		if (!std::isprint(c))
		{
			escapee = 'u';
			return true;
		}
		return false;
	}
}

inline char unscape(const char c)
{
	switch (c)
	{
	case '0': return '\0';
	case '"':  return '"';
	case '\\': return '\\';
	case '/':  return '/';
	case 'b':  return '\b';
	case 'f':  return '\f';
	case 'n':  return '\n';
	case 'r':  return '\r';
	case 't':  return '\t';
	default:   return c;
	}
}

template <typename S>
inline void write_escaped_string(std::stringstream& ss, const S& str)
{
	ss << '"';
	for (const char c : str)
	{
		if (char escapee{}; needs_escaping(c, escapee))
		{
			ss << '\\' << escapee;

			if (escapee == 'u')
			{
				char xdigits[5]{};
				std::sprintf(xdigits, "%04x", static_cast<int>(c));
				ss.write(xdigits, 4);
			}
		}
		else
		{
			ss << c;
		}
	}
	ss << '"';
}

struct Stringify
{
	inline Stringify(const Formatting& formatting)
		: _formatting{ formatting }
		, _indent{}
	{
	}

	template <typename T, typename D>
	std::string operator()(const T& value, const D& desc)
	{
		write(value, desc);
		return _ss.str();
	}

private:

	void spacing(const bool modifier = false)
	{
		if (_formatting.newline_elements && !modifier)
		{
			_ss << '\n';

			for (int i = 0; i < _indent; ++i)
				_ss.put('\t');
		}
		else if (!_formatting.dense)
		{
			_ss << ' ';
		}
	}

	template <typename T>
	void write(const T& value, const Boolean&)
	{
		_ss << (value ? literals::true_ : literals::false_);
	}

	template <typename T>
	void write(const T& value, const Number&)
	{
		_ss << value;
	}

	void write(const char value, const String&)
	{
		write_escaped_string(_ss, std::string_view{ &value, 1 });
	}

	template <typename T>
	void write(const T& value, const String&)
	{
		write_escaped_string(_ss, value);
	}

	template <typename T, typename D>
	void write(const T& value, const array<D>& desc)
	{
		_ss << '[';

		bool first = true;

		for (const auto& element : value)
		{
			if (first)
			{
				_indent++;
				spacing(is_trivial_v<D> && !_formatting.newline_trivial_arrays);
			}
			else
			{
				_ss << ',';
				spacing(is_trivial_v<D> && !_formatting.newline_trivial_arrays);
			}
			
			write(element, desc.element);
			first = false;
		}

		if (!first)
		{
			_indent--;
			spacing(is_trivial_v<D> && !_formatting.newline_trivial_arrays);
		}

		_ss << ']';
	}

	template <typename T, typename D>
	void write(const T& value, const object<D>& desc)
	{
		_ss << '{';

		bool first = true;

		for (const auto& [key, element] : value)
		{
			if (first)
			{
				_indent++;
				spacing();
			}
			else
			{
				_ss << ',';
				spacing();
			}
			
			_ss << '"';
			write_escaped_string(_ss, key);
			_ss << '"' << ':';

			if (!_formatting.dense)
				_ss << ' ';

			write(element, desc.element);
			first = false;
		}

		if (!first)
		{
			_indent--;
			spacing();
		}

		_ss << '}';
	}

	template <std::size_t I, typename T, typename... Fs>
	void write_field(const T& value, const std::tuple<Fs...>& desc)
	{
		if constexpr (I == 0)
		{
			_indent++;
			spacing();
		}
		else
		{
			_ss << ',';
			spacing();
		}

		const auto [name, mptr, d] = std::get<I>(desc);

		write_escaped_string(_ss, name);

		_ss << ':';

		if (!_formatting.dense)
			_ss << ' ';

		write(value.*mptr, d);

		if constexpr (I + 1 < sizeof...(Fs))
		{
			write_field<I + 1>(value, desc);
		}
		else
		{
			_indent--;
			spacing();
		}
	}

	template <typename T, typename... Fs>
	void write(const T& value, const std::tuple<Fs...>& desc)
	{
		_ss << '{';
		
		if constexpr (sizeof... (Fs) != 0)
		{
			write_field<0>(value, desc);
		}
		
		_ss << '}';
	}

	Formatting _formatting;
	int _indent;
	std::stringstream _ss;
};

inline void skip_whitespace(iterator& it, const iterator end)
{
	while (it != end && std::isspace(*it))
		++it;
}

inline bool expect(const iterator begin, const iterator end, const char c)
{
	return begin != end && *begin == c;
}

inline parse_result expect(iterator begin, const iterator end, const std::string_view text)
{
	for (auto it = text.begin(); it != text.end(); ++it, ++begin)
	{
		if (!expect(begin, end, *it))
			return parse_result{ begin, false };
	}

	return parse_result{ begin, true };
}

parse_result parse_escaped_string(iterator it, const iterator end, std::string& str)
{
	if (!expect(it, end, '"'))
		return parse_result{ it, false };

	for (++it; it != end; )
	{
		if (const char c = *(it++); c == '"')
		{
			return parse_result{ it, true };
		}
		else if (c == '\\')
		{
			if (it == end)
				return parse_result{ it, false };

			if (const char e = *(it++); e == 'u')
			{
				if (it + 4 > end)
					return parse_result{ end, false };

				const char hex[4]{ *(it++), *(it++), *(it++), *(it++) };

				int value;
				std::from_chars(std::begin(hex), std::end(hex), value, 16);
				str.push_back(static_cast<char>(value));
			}
			else
			{
				str.push_back(unscape(e));
			}
		}
		else
		{
			str.push_back(c);
		}
	}

	return parse_result{ it, false };
}

template <typename T, typename D>
parse_result parse(iterator it, const iterator end, T& value, const array<D>& desc);

template <typename T, typename D>
parse_result parse(iterator it, const iterator end, T& value, const object<D>& desc);

template <typename T, typename... Fs>
parse_result parse(iterator it, const iterator end, T& value, const std::tuple<Fs...>& desc);

template <typename T>
parse_result parse(const iterator begin, const iterator end, T& value, const Boolean&)
{
	if (const auto [it, ok] = expect(begin, end, literals::true_); ok)
	{
		value = true;
		return parse_result{ it, true };
	}

	if (const auto [it, ok] = expect(begin, end, literals::false_); ok)
	{
		value = false;
		return parse_result{ it, true };
	}

	return parse_result{ begin, false };
}

template <typename T>
parse_result parse(const iterator begin, const iterator end, T& value, const Number&)
{
	const char* pBegin = &*begin;
	const auto [pEnd, ec] = std::from_chars(pBegin, pBegin + (end - begin), value);

	if (ec == std::errc{})
	{
		return parse_result{ begin + (pEnd - pBegin), true };
	}
	else
	{
		return parse_result{ begin, false };
	}
}

template <typename T>
parse_result parse(const iterator begin, const iterator end, T& value, const String&)
{
	std::string str;
	const auto result = parse_escaped_string(begin, end, str);
	
	if (result.success)
	{
		string_assigner<T>{}(value, str);
	}

	return result;
}

template <typename T, typename D>
parse_result parse(iterator it, const iterator end, T& value, const array<D>& desc)
{
	if (!expect(it, end, '['))
		return parse_result{ it, false };

	skip_whitespace(++it, end);

	array_inserter<T> inserter{};

	while (it != end && *it != ']')
	{
		array_value_type_t<T> element;
		const auto [element_it, element_ok] = parse(it, end, element, desc.element);

		if (!element_ok)
			return parse_result{ element_it, false };

		inserter(value, element);

		it = element_it;

		skip_whitespace(it, end);

		if (it == end || (*it != ',' && *it != ']'))
			return parse_result{ it, false };
		
		if (*it == ',')
			skip_whitespace(++it, end);
	}

	return parse_result{ it + 1, true };
}

template <typename T, typename D>
parse_result parse(iterator it, const iterator end, T& value, const object<D>& desc)
{
	if (!expect(it, end, '{'))
		return parse_result{ it, false };

	skip_whitespace(++it, end);

	object_inserter<T> inserter;

	while (it != end && *it != '}')
	{
		std::string key;
		const auto [key_it, key_ok] = parse_escaped_string(it, end, key);

		if (!key_ok)
			return parse_result{ key_it, false };

		it = key_it;

		skip_whitespace(it, end);

		if (!expect(it, end, ':'))
			return parse_result{ it, false };

		skip_whitespace(++it, end);

		object_value_type_t<T> element;
		const auto [element_it, element_ok] = parse(it, end, element, desc.element);

		if (!element_ok)
			return parse_result{ element_it, false };

		inserter(value, key, element);

		it = element_it;

		skip_whitespace(it, end);

		if (it == end || (*it != ',' && *it != '}'))
			return parse_result{ it, false };

		if (*it == ',')
			skip_whitespace(++it, end);
	}

	return parse_result{ it + 1, true };
}

template <std::size_t I, typename T, typename... Fs>
parse_result parse_field(const iterator begin, const iterator end, const std::string_view key, T& value, const std::tuple<Fs...>& desc)
{
	const auto [name, mptr, d] = std::get<I>(desc);

	if (name == key)
	{
		return parse(begin, end, value.*mptr, d);
	}
	else
	{
		if constexpr (I + 1 < sizeof...(Fs))
		{
			return parse_field<I + 1>(begin, end, key, value, desc);
		}
		else
		{
			return parse_result{ begin, false };
		}
	}
}

template <typename T, typename... Fs>
parse_result parse(iterator it, const iterator end, T& value, const std::tuple<Fs...>& desc)
{
	if (!expect(it, end, '{'))
		return parse_result{ it, false };

	skip_whitespace(++it, end);

	while (it != end && *it != '}')
	{
		std::string key;
		const auto [key_it, key_ok] = parse_escaped_string(it, end, key);

		if (!key_ok)
			return parse_result{ key_it, false };

		it = key_it;

		skip_whitespace(it, end);

		if (!expect(it, end, ':'))
			return parse_result{ it, false };

		skip_whitespace(++it, end);

		const auto [element_it, element_ok] = parse_field<0>(it, end, key, value, desc);

		if (!element_ok)
			return parse_result{ element_it, false };

		it = element_it;

		skip_whitespace(it, end);

		if (it == end || (*it != ',' && *it != '}'))
			return parse_result{ it, false };

		if (*it == ',')
			skip_whitespace(++it, end);
	}

	return parse_result{ it + 1, true };
}

} // impl

template <typename T, typename D>
std::string stringify(const T& value, const D& desc, const Formatting& formatting = dense)
{
	return impl::Stringify{ formatting }(value, desc);
}

template <typename T, typename D>
parse_result parse(const std::string_view line, T& value, const D& desc)
{
	return impl::parse(line.begin(), line.end(), value, desc);
}

} // json

#endif // __JSON_HPP 