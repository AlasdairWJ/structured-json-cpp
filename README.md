# structured-json-cpp
 **(c++17)** Single-file compile-time json structure definitions for native read and writing

A descriptor provides description of a structure that can be used to read/write JSON

```c++
struct Point
{
	int x, y;
};

constexpr auto PointDescriptor = std::tuple(
	json::field("x", &Point::x, json::number),
	json::field("y", &Point::y, json::number)
);

// ...

std::cout << json::stringify(Point{ 3, 4 }, PointDescriptor); // -> {"x":3,"y":4}

// ..

Point point;
json::parse("{ \"x\": 3, \"y\": 4 }", point, PointDescriptor) // point == Point{ 3, 4 }
```

## Basics

We write JSON using `json::stringify( <object>, <descriptor>[, <formatting>]) -> std::string`

We read JSON use `json::parse(std::string_view, <object>, <descriptor>) -> bool`

A descriptor informs how an object should be represented as JSON.

There are descriptors for the trivial JSON types (`boolean`, `number`, and `string`)

```c++
std::cout << json::stringify(true, json::boolean); // -> true
std::cout << json::stringify(123, json::number); // 123
std::cout << json::stringify("hello", json::string); // -> "hello"
```

The non-trivial types `json::array` and `json::object` must define a descriptor for their elements

```c++
int sequence[]{ 2, 3, 5, 7 };
std::cout << json::stringify(sequence, json::array{ json::number });
// -> [2,3,5,7]

std::map<std::string, int> ages{ { "Joe", 21 }, { "Anne", 23 }, { "Sam", 19 } };
std::cout << json::stringify(ages, json::object{ json::number });
// -> {"Anne":23,"Joe":21,"Sam":19}
```

Finally, tuple's can be used to map native structures by defining a set of `json::field`s, each composed of a key, a member pointer, and a descriptor for that member

```c++
struct Point
{
	int x, y;
};

constexpr auto PointDescriptor = std::tuple(
	json::field("x", &Point::x, json::number),
	json::field("y", &Point::y, json::number)
);
```

This allow descriptors to be defined in a nested manor

```c++
struct Line
{
	Point a, b;
};

constexpr auto LineDescriptor = std::tuple(
	json::field("a", &Line::a, PointDescriptor),
	json::field("b", &Line::b, PointDescriptor)
);
```

## What types can be used for what descriptors?

 - **Boolean** be convertible to/from `bool`
 - **Number** must be an arithmetic type
 - **String** must be `char`, `char[]`, or convertible from `std::string`
 - **Array** must either be a static array, support `push_back`, or have a custom `json::array_inserter` defined for it
 - **Object** must either support `emplace`, or have a custom `json::object_inserter` defined for it

An example of a `json::array_inserter` for a `std::set` could be defined like so 

```c++
template <typename E>
struct json::array_inserter<std::set<E>>
{	
	void operator()(std::set<E>& values, const E& element) const
	{
		values.insert(element);
	}
};
```

`json::object_inserters`s are similar

```c++
template <>
struct json::object_inserter<T>
{	
	void operator()(T& values, const std::string_view key, const E& element) const
	{
		// ...
	}
};
```

## Formatting

We can pretty-print JSON by passing `json::pretty` to `json::stringify`

```c++
std::map<std::string, int> ages{ { "Joe", 21 }, { "Anne", 23 }, { "Sam", 19 } };
std::cout << json::stringify(ages, json::object{ json::number }, json::pretty);
// -> {
//        "Anne": 23,
//        "Joe": 21,
//        "Sam": 19
//    }
```

 or we can define our own `json::Formatting` object:

```c++
struct json::Formatting
{
	bool dense;
	bool newline_elements;
	bool newline_trivial_arrays;
};
```

 - `dense`: whether to include spaces between elements on the same line, or after a colon
 - `newline_elements`: whether to insert a new line between elements of an object, or elements of non-trivial arrays.
 - `newline_trivial_arrays`: if `newline_elements` is set, whether arrays of `boolean`/`number`/`string` also a get new line per element

## Todo:

 - Support `std::optional` for `null`
 - Explicit type validation of descriptors to improve error messages
 - Support some form of dynamic content (i.e. how to read into a variant based on type?)
