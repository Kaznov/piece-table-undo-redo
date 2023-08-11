#include <iostream>
#include <sstream>

struct Logger {
    std::ostringstream ss;

    template<typename T>
    Logger& operator<<(T const& x) {
        ss << x;
        return *this;
    }

    template<>
    Logger& operator<<(bool const& x) {
        ss << std::boolalpha << x;
        return *this;
    }

    template<>
    Logger& operator<<(void* const& x) {
        if (x == nullptr) ss << "nullptr"; else ss << x;
        return *this;
    }
    ~Logger() { std::cout << ss.str() << std::endl; }
};

struct EmptyLogger {
    template <typename T>
    EmptyLogger& operator<<(T const&) {
        return *this;
    }
};
