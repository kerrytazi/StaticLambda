# StaticLambda
Helps convert C++ lambdas with captures to function pointers.

## Usage Example
https://github.com/kerrytazi/StaticLambda/blob/f2e7b7d2cfcc6bb97764405155f8a6c1f355564d/example/main.cpp#L8-L17

## How to add via cmake
Can be added via simple [add_subdirectory](https://cmake.org/cmake/help/latest/command/add_subdirectory.html)/[FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)
```cmake
include(FetchContent)

FetchContent_Declare(
	StaticLambda
	GIT_REPOSITORY https://github.com/kerrytazi/StaticLambda.git
)
FetchContent_MakeAvailable(StaticLambda)

target_link_libraries(MyTarget PRIVATE StaticLambda)
```

## Used by
- [DetourLambda](https://github.com/kerrytazi/DetourLambda)

## License
StaticLambda is licensed under the MIT license.
