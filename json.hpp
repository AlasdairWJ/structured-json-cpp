#ifndef __JSON_HPP
#define __JSON_HPP

#include <string>
#include <tuple>
#include <optional>

namespace json
{

struct boolean_t {} boolean;

struct number_t {} number;

struct string_t {} string;

template <typename TDesc>
struct array
{
	TDesc value_descriptor;
};

template <typename TDesc>
struct object
{
	TDesc value_descriptor;
};

// =====

template <typename T>
struct underlying_value_type;

template <std::ranges::range T>
struct underlying_value_type<T>
{
	using type = std::ranges::range_value_t<T>;
};

template <typename T, std::size_t N>
struct underlying_value_type<T[N]>
{
	using type = T;
};

template <typename T>
using underlying_value_type_t = typename underlying_value_type<T>::type;

// =====

template <typename T>
constexpr bool is_pair_v = false;

template <typename TKey, typename TValue>
constexpr bool is_pair_v<std::pair<TKey, TValue>> = true;

// =====

template <typename T>
concept BackInsertable = requires(T& t, const T::value_type& u) { { t.push_back(u) }; };

template <typename T>
concept Insertable = requires(T& t, const T::value_type& u) { { t.insert(t.end(), u) }; };

template <typename T>
concept Buildable =  BackInsertable<T> || Insertable<T>;

template <typename T>
concept BuildableRange = std::ranges::range<T> && (Buildable<T>);

// =====

template <typename T>
concept Boolean = requires(T t, bool b) { { t = b }; } && requires(T t) { { t ? 0 : 1 }; };

template <typename T>
concept Number = std::is_arithmetic_v<T>;

template <typename T>
concept String = std::is_same_v<T, char> || std::is_same_v<T, const char*> || (std::is_bounded_array_v<T> || BuildableRange<T>) && std::is_same_v<underlying_value_type_t<T>, char>;

template <typename T>
concept Array = std::is_bounded_array_v<T> || BuildableRange<T>;

template <typename T>
concept Object = (std::is_bounded_array_v<T> || BuildableRange<T>) && is_pair_v<underlying_value_type_t<T>>;

// =====

namespace literals
{
constexpr std::string null{ "null" };
constexpr std::string true_{ "true" };
constexpr std::string false_{ "false" };
}

// =====

template <typename T>
constexpr bool is_valid_descriptor_v = false;

template <typename T>
concept Descriptor = is_valid_descriptor_v<T>;

template <> constexpr bool is_valid_descriptor_v<boolean_t> = true;
template <> constexpr bool is_valid_descriptor_v<number_t> = true;
template <> constexpr bool is_valid_descriptor_v<string_t> = true;

template <typename TValue>
constexpr bool is_valid_descriptor_v<array<TValue>> = is_valid_descriptor_v<TValue>;

template <typename TValue>
constexpr bool is_valid_descriptor_v<object<TValue>> = is_valid_descriptor_v<TValue>;

template <typename TValue>
constexpr bool is_valid_descriptor_v<std::optional<TValue>> = is_valid_descriptor_v<TValue>;

// =====

template <typename TStructure, typename TValue, Descriptor TDescriptor>// , std::integral TSize = std::size_t
struct field
{
	const char* name;
	TValue TStructure::* member_ptr;
	TDescriptor descriptor;
};

template <typename T>
constexpr bool is_field_list_v = false;

template <typename T, typename... TValues, Descriptor... TDescriptors>
constexpr bool is_field_list_v<std::tuple<field<T, TValues, TDescriptors>...>> = true;

template <typename T, typename... TValues, Descriptor... TDescriptors>
constexpr bool is_field_list_v<const std::tuple<field<T, TValues, TDescriptors>...>> = true;

template <typename T> requires (is_field_list_v<T>) 
constexpr bool is_valid_descriptor_v<T> = true;

// =====

template <typename TContainer, typename TValue, Descriptor TDescriptor>
struct element
{
	TValue TContainer::* member_ptr;
	TDescriptor descriptor;
};

template <typename T>
constexpr bool is_element_list_v = false; // is T a list of elements of TOf ?

template <typename T, typename... TValues, typename... TDescriptors>
constexpr bool is_element_list_v<std::tuple<element<T, TValues, TDescriptors>...>> = true;

template <typename T, typename... TValues, typename... TDescriptors>
constexpr bool is_element_list_v<const std::tuple<element<T, TValues, TDescriptors>...>> = true;

template <typename T> requires (is_element_list_v<T>) 
constexpr bool is_valid_descriptor_v<T> = true;

// =====

template <typename T>
constexpr bool is_optional_v = false;

template <typename T>
constexpr bool is_optional_v<std::optional<T>> = true;

template <typename T>
concept Mandatory = !is_optional_v<T>;

} // json

#endif // __JSON_HPP