
#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include "timer.h"

std::string now() {

    auto now = std::chrono::system_clock::now();

    // Convert to time_t (seconds)
    std::time_t t = std::chrono::system_clock::to_time_t(now);

    // Extract milliseconds
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    // Convert to local time
    std::tm localTime = *std::localtime(&t);

    // Use a stringstream to format output
    std::ostringstream oss;
    oss << std::put_time(&localTime, "%H:%M:%S") << "." << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}
