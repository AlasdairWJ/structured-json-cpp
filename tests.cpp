#include <iostream>
#include <sstream>
#include <iomanip>
#include <map>

#include "parser.hpp"
#include "stringifier.hpp"

using namespace std::string_literals;

// json string re
//const std::regex string_re{ "^\"(?:[^\\\\]|\\\\(?:\"|\\\\|\\/|b|f|n|r|t|u\\d{1,4}))*\"" }; // 

bool any_failed = false;

bool test(json::stringifier& stringify, const auto& obj, const auto& desc, const std::string& expectation)
{
	const auto result = stringify(obj, desc);

	if (result == expectation)
		return true;

	any_failed = true;

	std::cout << "test failed:\n";
	std::cout << "expectation: " << std::quoted(expectation) << '\n';
	std::cout << "got: " << std::quoted(result) << '\n';

	return false;
}

template <typename T>
concept Comprable = requires(const T& lhs, const T& rhs) { { lhs == rhs }; };

template <Comprable T>
bool test(json::parser& parse, const std::string& text, const auto& desc, const T& expectation)
{
	T value{};
	const bool success = parse(text, value, desc);

	if (success && value == expectation)
		return true;

	any_failed = true;

	std::cout << "test failed:\n";
	std::cout << "when parsing: " << std::quoted(text) << '\n';

	return false;
}

auto quoted(auto value)
{
	std::stringstream ss{};
	ss << std::quoted(value);
	return ss.str();
}

struct Point
{
	int x, y;

	auto operator<=>(const Point&) const = default;
};

constexpr auto PointDescriptor = std::tuple(
	json::field("x", &Point::x, json::number),
	json::field("y", &Point::y, json::number)
);

struct Person
{
	std::string name;
	int age;
	bool active;
	
	auto operator<=>(const Person&) const = default;
};

constexpr auto PersonDescriptor = std::tuple(
	json::element(&Person::name, json::string),
	json::element(&Person::age, json::number),
	json::element(&Person::active, json::boolean)
);

int main(int argc, char const *argv[])
{
//	std::cout << json::Descriptor<decltype(PointDescriptor)> << '\n';

	// stringifier tests
	{
		json::stringifier stringify{};
		stringify.dense = true;

		// bool
		test(stringify, false, json::boolean, "false") &&
		test(stringify, true, json::boolean, "true") &&

		// integer
		test(stringify, 0, json::number, "0") &&
		test(stringify, 123, json::number, "123") &&
		test(stringify, -4567, json::number, "-4567") &&

		// float
		test(stringify, 0.0, json::number, "0") &&
		test(stringify, 1.23, json::number, "1.23") &&
		test(stringify, 4.567, json::number, "4.567") &&
		test(stringify, -100.5, json::number, "-100.5") &&

		// string
		test(stringify, ""s, json::string, quoted("")) &&
		test(stringify, "hello"s, json::string, quoted("hello")) &&
		test(stringify, "\"world\""s, json::string, quoted("\"world\"")) &&
		test(stringify, "this\nthat"s, json::string, "\"this\\nthat\"") &&

		// array
		test(stringify, std::vector<int>{}, json::array{ json::number }, "[]") &&
		test(stringify, std::vector<int>{4,5,6}, json::array{ json::number }, "[4,5,6]") &&
		test(stringify, std::vector<std::string>{"yes","no","maybe"}, json::array{ json::string }, "[\"yes\",\"no\",\"maybe\"]") &&

		//object
		test(stringify, std::map<std::string, int>{}, json::object{ json::number }, "{}") &&

		test(
			stringify,
			std::map<std::string, int>{
				{ "red", 1 },
				{ "green", 8 },
				{ "blue", -914 }
			},
			json::object{ json::number },
			"{\"blue\":-914,\"green\":8,\"red\":1}"
		) &&

		test(stringify, std::optional<int>{}, json::number, "null") &&
		test(stringify, std::optional<int>{1}, json::number, "1");

		// fields
		test(stringify, Point{3,4}, PointDescriptor, "{\"x\":3,\"y\":4}");

		// elements
		test(stringify, Person{ "Steve", 25, true }, PersonDescriptor, "[\"Steve\",25,true]");
	}

	// parser tests
	{
		json::parser parse{};

		// boolean
		test(parse, "false", json::boolean, false) &&
		test(parse, "true", json::boolean, true) &&

		// integer
		test(parse, "0", json::number, 0) &&
		test(parse, "123", json::number, 123) &&
		test(parse, "-4567", json::number, -4567) &&
		test(parse, "281474976710656", json::number, 281474976710656ll) &&

		// float
		test(parse, "0.0", json::number, 0.0) &&
		test(parse, "1.23", json::number, 1.23) &&
		test(parse, "4.567", json::number, 4.567f) &&
		test(parse, "-100.5", json::number, -100.5f) &&

		// string
		test(parse, "\"\""s, json::string, ""s) &&
		test(parse, "\"hello\""s, json::string, "hello"s) &&
		test(parse, "\"\\\"world\\\"\""s, json::string, "\"world\""s) &&
		test(parse, "\"this\\nthat\""s, json::string, "this\nthat"s) &&

		// array
		test(parse, "[]", json::array{ json::number }, std::vector<int>{}) &&
		test(parse, "[4,5,6]", json::array{ json::number }, std::vector<int>{4,5,6}) &&
		test(parse, "[\"yes\",\"no\",\"maybe\"]", json::array{ json::string }, std::vector<std::string>{"yes","no","maybe"}) &&

		// object
		test(parse, "{}", json::object{ json::number }, std::map<std::string, int>{}) &&
		test(parse, "{\"blue\":-914,\"green\":8,\"red\":1}"s, json::object{ json::number }, std::map<std::string, int>{{"red", 1}, {"green", 8}, {"blue", -914}}) &&

		// optional
		test(parse, "null", json::number, std::optional<int>{}) &&
		test(parse, "1", json::number, std::optional<int>{1}) &&

		// fields
		test(parse, "{\"x\":3,\"y\":4}", PointDescriptor, Point{3,4}) &&

		// elements
		test(parse, "[\"Steve\",25,true]", PersonDescriptor, Person{ "Steve", 25, true });

	}

	if (!any_failed)
		std::cout << "all good.";
}