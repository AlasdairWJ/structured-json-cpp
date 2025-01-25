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

constexpr struct Custom {} custom;

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

template <typename T, typename D>
struct is_valid_descriptor_for : std::false_type {};

template <typename T, typename D>
constexpr bool is_valid_descriptor_for_v = is_valid_descriptor_for<T, D>::value;

template <typename T>
struct is_valid_descriptor_for<T, Boolean> : std::bool_constant<std::is_convertible_v<bool, T> && std::is_convertible_v<T, bool>> {};

template <typename T>
struct is_valid_descriptor_for<T, Number> : std::bool_constant<std::is_arithmetic_v<T>> {};

template <>
struct is_valid_descriptor_for<char, String> : std::true_type {};

template <int N>
struct is_valid_descriptor_for<char[N], String> : std::true_type {};

template <typename T>
struct is_valid_descriptor_for<T, String> : std::bool_constant<std::is_convertible_v<std::string, T>> {};

template <typename T, typename D>
struct is_valid_descriptor_for<T, array<D>> : std::bool_constant<
	is_valid_descriptor_for_v<array_value_type_t<T>, D>
> {};

template <typename T, typename D>
struct is_valid_descriptor_for<T, object<D>> : std::bool_constant<
	is_valid_descriptor_for_v<object_value_type_t<T>, D>
> {};

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

namespace literals
{
constexpr std::string_view null{ "null" };
constexpr std::string_view true_{ "true" };
constexpr std::string_view false_{ "false" };
}

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

struct Formatting
{
	bool dense;
	bool newline_elements;
	bool newline_trivial_arrays;
};

constexpr Formatting pretty{ false, true, false };
constexpr Formatting dense{ true, false, false };

struct Stringifier
{
	inline Stringifier(std::ostream& os, const Formatting& formatting)
		: _os{ os }
		, _formatting{ formatting }
		, _indent{}
	{
	}

	template <typename T, typename D>
	auto& operator()(const T& value, const D& desc)
	{
		write(value, desc);
		return *this;
	}

private:

	void spacing(const bool modifier = false)
	{
		if (_formatting.newline_elements && !modifier)
		{
			_os << '\n';

			for (int i = 0; i < _indent; ++i)
				_os.put('\t');
		}
		else if (!_formatting.dense)
		{
			_os << ' ';
		}
	}

	template <typename T>
	void write(const T& value, const Boolean&)
	{
		_os << (value ? literals::true_ : literals::false_);
	}

	template <typename T>
	void write(const T& value, const Number&)
	{
		_os << value;
	}

	static inline bool needs_escaping(const char c, char& escapee)
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

	template <typename T>
	void write(const T& value, const String&)
	{
		_os << '"';
		for (const char c : value)
		{
			if (char escapee{}; impl::needs_escaping(c, escapee))
			{
				_os << '\\' << escapee;

				if (escapee == 'u')
				{
					char xdigits[5]{};
					std::sprintf(xdigits, "%04x", static_cast<int>(c));
					_os.write(xdigits, 4);
				}
			}
			else
			{
				_os << c;
			}
		}
		_os << '"';
	}

	inline void write(const char value, const String&)
	{
		write(std::string_view{ &value, 1 }, string);
	}

	template <typename T, typename D>
	void write(const T& value, const array<D>& desc)
	{
		_os << '[';

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
				_os << ',';
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

		_os << ']';
	}

	template <typename T, typename D>
	void write(const T& value, const object<D>& desc)
	{
		_os << '{';

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
				_os << ',';
				spacing();
			}
			
			write(key, string);
			_os << ':';

			if (!_formatting.dense)
				_os << ' ';

			write(element, desc.element);
			first = false;
		}

		if (!first)
		{
			_indent--;
			spacing();
		}

		_os << '}';
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
			_os << ',';
			spacing();
		}

		const auto [name, mptr, d] = std::get<I>(desc);

		write(name, string);

		_os << ':';

		if (!_formatting.dense)
			_os << ' ';

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
		_os << '{';
		
		if constexpr (sizeof... (Fs) != 0)
		{
			write_field<0>(value, desc);
		}
		
		_os << '}';
	}

	Formatting _formatting;
	int _indent;
	bool _first;
	std::ostream& _os;
};

struct Parser
{
	inline Parser(const iterator begin, const iterator end)
		: _it{ begin }
		, _end{ end }
	{
	}

	template <typename T, typename D>
	bool operator()(T& value, const D& desc)
	{
		return parse(value, desc);
	}

	bool ignore();

	inline iterator position() const { return _it; }

private:

	inline bool at_end() const { return _it == _end; }
	inline bool needs(const int n) const { return _it + n <= _end; } 

	inline bool skip_whitespace()
	{
		while (!at_end() && std::isspace(*_it))
			++_it;

		return _it != _end;
	}

	inline bool expect(const char c)
	{
		if (!at_end() && *_it != c)
			return false;

		++_it;

		return true;
	}

	inline bool expect(const std::string_view text)
	{
		for (const char c : text)
		{
			if (!expect(c))
				return false;
		}

		return true;
	}

	template <typename T>
	bool parse(T& value, const Boolean&)
	{
		if (at_end())
			return false;

		switch (*_it)
		{
		case 't':
			if (!expect(literals::true_))
				return false;
			value = true;
			return true;

		case 'f':
			if (!expect(literals::false_))
				return false;
			value = false;
			return true;

		default:
			return false;
		}
	}

	template <typename T>
	bool parse(T& value, const Number&)
	{
		const auto [number_end, ec] = std::from_chars(_it, _end, value);

		if (ec != std::errc{})
			return false;

		_it = number_end;
		return true;
	}

	static inline char unscape(const char c)
	{
		switch (c)
		{
		case '0':  return '\0';
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

	inline bool parse(std::string& value, const String&)
	{
		if (!expect('"'))
			return false;

		for (;; ++_it)
		{
			if (at_end())
				return false;

			const char c = *_it;

			if (c == '"')
			{
				++_it;
				return true;
			}
			
			if (c != '\\')
			{
				value.push_back(c);
				continue;
			}

			if (++_it; at_end())
				return false;

			const char e = *_it;

			if (e != 'u')
			{
				value.push_back(unscape(e));
			}
			else
			{
				if (!needs(4))
					return false;

				const char hex[4]{ *(++_it), *(++_it), *(++_it), *(++_it) };

				int ival;
				std::from_chars(std::begin(hex), std::end(hex), ival, 16);
				value.push_back(static_cast<char>(ival));
			}
		}
	}

	inline bool parse(char& value, const String&)
	{
		std::string str;
		if (!parse(str, string))
			return false;
		value = !str.empty() ? str.front() : '\0';
		return true;
	}

	template <typename T>
	bool parse(T& value, const String&)
	{
		std::string str;
		if (!parse(str, string))
			return false;
		value = str;
		return true;
	}

	template <typename T, typename D>
	bool parse(T& value, const array<D>& desc)
	{
		if (!expect('[') || !skip_whitespace())
			return false;

		if (*_it == ']')
		{
			_it++;
			return true;
		}

		array_inserter<T> inserter{};

		for (;;)
		{
			array_value_type_t<T> element;

			if (!parse(element, desc.element))
				return false;

			inserter(value, element);

			if (!skip_whitespace())
				return false;

			if (*_it == ']')
			{
				_it++;
				return true;
			}

			if (*_it != ',')
				return false;

			++_it;

			if (!skip_whitespace())
				return false;
		}
	}

	template <typename T, typename D>
	bool parse(T& value, const object<D>& desc)
	{
		if (!expect('{') || !skip_whitespace())
			return false;

		if (*_it == '}')
		{
			_it++;
			return true;
		}

		object_inserter<T> inserter{};

		for (;;)
		{
			std::string key;

			if (
				!parse(key, string) ||
				!skip_whitespace() ||
				!expect(':') ||
				!skip_whitespace()
			)
				return false;

			object_value_type_t<T> element;

			if (!parse(element, desc.element))
				return false;

			inserter(value, key, element);

			if (!skip_whitespace())
				return false;

			if (*_it == '}')
			{
				_it++;
				return true;
			}

			if (*_it != ',')
				return false;

			++_it;

			if (!skip_whitespace())
				return false;
		}
	}

	template <std::size_t I, typename T, typename... Fs>
	bool parse_field(const std::string_view key, T& value, const std::tuple<Fs...>& desc)
	{
		const auto [name, mptr, d] = std::get<I>(desc);

		if (name == key)
		{
			return parse(value.*mptr, d);
		}
		else
		{
			if constexpr (I + 1 < sizeof...(Fs))
			{
				return parse_field<I + 1>(key, value, desc);
			}
			else
			{
				return ignore();
			}
		}
	}

	template <typename T, typename... Fs>
	bool parse(T& value, const std::tuple<Fs...>& desc)
	{
		if (!expect('{') || !skip_whitespace())
			return false;

		if (*_it == '}')
		{
			_it++;
			return true;
		}

		for (;;)
		{
			std::string key;

			if (
				!parse(key, string) ||
				!skip_whitespace() ||
				!expect(':') ||
				!skip_whitespace() ||
				!parse_field<0>(key, value, desc) ||
				!skip_whitespace()
			)
				return false;

			if (*_it == '}')
			{
				_it++;
				return true;
			}

			if (*_it != ',')
				return false;

			++_it;

			if (!skip_whitespace())
				return false;
		}
	}

	inline bool ignore_number()
	{
		double x;
		return parse(x, number);
	}

	inline bool ignore_string()
	{
		if (!expect('"'))
			return false;

		for (;; ++_it)
		{
			if (at_end())
				return false;

			const char c = *_it;

			if (c == '"')
			{
				++_it;
				return true;
			}
			
			if (c != '\\')
				continue;

			if (++_it; at_end())
				return false;

			if (*_it == 'u')
			{
				if (!needs(4))
					return false;

				_it += 4;
			}
		}
	}

	inline bool ignore_array()
	{
		if (!expect('[') || !skip_whitespace())
			return false;

		if (*_it == ']')
		{
			_it++;
			return true;
		}

		for (;;)
		{
			if (
				!skip_whitespace() ||
				!ignore() ||
				!skip_whitespace()
			)
				return false;

			if (*_it == ']')
			{
				_it++;
				return true;
			}

			if (*_it != ',')
				return false;

			++_it;
		}
	}

	inline bool ignore_object()
	{
		if (!expect('{') || !skip_whitespace())
			return false;

		if (*_it == '}')
		{
			_it++;
			return true;
		}

		for (;;)
		{
			if (
				!skip_whitespace() ||
				!ignore_string() ||
				!skip_whitespace() ||
				!expect(':') ||
				!skip_whitespace() ||
				!ignore() ||
				!skip_whitespace()
			)
				return false;

			if (*_it == '}')
			{
				_it++;
				return true;
			}

			if (*_it != ',')
				return false;

			++_it;
		}
	}

	iterator _it;
	const iterator _end;
};

inline bool Parser::ignore()
{
	if (at_end())
		return false;

	switch (const char c = *_it; c)
	{
	case 'n':
		return expect(literals::null);
	case 't':
		return expect(literals::true_);
	case 'f':
		return expect(literals::false_);
	case '-': case '0':
	case '1': case '2': case '3':
	case '4': case '5': case '6':
	case '7': case '8': case '9':
		return ignore_number();
	case '"':
		return ignore_string();
	case '[':
		return ignore_array();
	case '{':
		return ignore_object();
	default:
		return false;
	}
}

template <typename T, typename D>
std::string stringify(const T& value, const D& desc, const Formatting& formatting = dense)
{
	std::stringstream ss;
	Stringifier{ ss, formatting }(value, desc);
	return ss.str();
}

template <typename T, typename D>
bool parse(const std::string_view line, T& value, const D& desc)
{
	return Parser{ line.begin(), line.end() }(value, desc);
}

} // json

#endif // __JSON_HPP 