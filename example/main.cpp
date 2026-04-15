#include "StaticLambda/StaticLambda.hpp"

#include <print>
#include <optional>

void example1()
{
	// Lambda with capture
	int c = 7;
	StaticLambda<int(int, int)> sl([&](int a, int b) {
		return a + b + c;
	});

	// Raw pointer
	int(*static_func)(int, int) = sl.GetTarget();

	std::println("example1: result = {}", static_func(2, 3)); // 2 + 3 + 7 = 12
}

void example2()
{
	// using std::optional for delayed initialization
	std::optional<StaticLambda<int(int, int)>> opt_sl;

	int c = 7;
	opt_sl.emplace([&](int a, int b) {
		return a + b + c;
	});

	int(*static_func)(int, int) = opt_sl->GetTarget();
	auto result = static_func(2, 3); // 2 + 3 + 7 = 12

	std::println("example2: result = {}", result);
}

int main()
{
	example1();
	example2();
}
