# structured-json-cpp
 Compile-time json structure definitions for native read and writing

Ok this is a weird one

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

auto txt = json::stringify(Point{ 3, 4 }, PointDescriptor) // -> { "x": 3, "y": 4 }

// ..

Point point;
json::parse("{ \"x\": 3, \"y\": 4 }", point, PointDescriptor) // -> Point{ 3, 4 }
```

The idea is that we can define the structure of our objects at compile time, and that this structure is composable

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

It's currently super flexible whilst being super brittle (good luck with those error messages), that said it's also super cool, so here are the basics

## Descriptors

There are 5 json types, Booleans, Numbers, Strings, Arrays and Objects, and we have our set of basic descriptors

```c++
json::boolean
json::number
json::string
json::array{ <value-type> }
json::object{ <mapped-type> }
```

Array and Object require a sub-descriptor, and that's the basis for defining our structure. (Note that object does not specify a key type, because json is technically limited to keys being strings only.)

Example of composition:

```c++
json::array{ json::object{ json::number } } // Array of Objects containing String-Number pairs
```

## What types can be used?

**Booleans** can be anything that's assign-able from bool, and is usable in a boolean context.

**Numbers** must be arithmetic types (so anything that passes `std::is_arithmetic` should be good).

**Strings** can be a single character, a static array, or any buildable container of underlying value `char`.

**Arrays** can be a static array or any buildable container as long as the underlying type is a valid descriptor.

**Objects** can be a static array or any buildable container as long as the underlying type is a *std::pair*, where the first type is a valid string descriptor, and the mapped type is any valid descriptor.

All types must be default constructable, or I can't parse them.

### "buildable container"?

if i can call `push_back` or `insert` on it, then it's "buildable" and should work no problem

### What about `null`

Only works if your object is a `std::optional`, I think this is reasonable?

### Field descriptors

As in the example above, you can map names of fields to member pointers, this is the fundemental feature of my approach, I think it's cool

### What if I want to map elements of an array?

You can do that too! Dunno why you would be here:

```c++
struct Person
{
	std::string name;
	int age;
	bool active;
};

constexpr auto PersonDescriptor = std::tuple(
	json::element(&Person::name, json::string),
	json::element(&Person::age, json::number),
	json::element(&Person::active, json::boolean)
);

// ...

auto txt = json::stringify(Person{ "Steve", 25, true }, PointDescriptor) // -> ["Steve",25,true]

Person person;
json::parse("[\"Steve\",25,true]", person, PointDescriptor) // -> Person{ "Steve", 25, true }
```

### What if I want to handle different types programmatically?

then don't use this, idiot