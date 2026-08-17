#include "bar.h"

#include <ctime>
#include <sstream>
#include <iomanip>

namespace qback::domain {

int Date::year() const {
    time_t seconds = static_cast<time_t>(days_since_epoch) * 86400;
    struct tm tm_buf{};
    gmtime_r(&seconds, &tm_buf);
    return tm_buf.tm_year + 1900;
}

int Date::month() const {
    time_t seconds = static_cast<time_t>(days_since_epoch) * 86400;
    struct tm tm_buf{};
    gmtime_r(&seconds, &tm_buf);
    return tm_buf.tm_mon + 1;
}

int Date::day() const {
    time_t seconds = static_cast<time_t>(days_since_epoch) * 86400;
    struct tm tm_buf{};
    gmtime_r(&seconds, &tm_buf);
    return tm_buf.tm_mday;
}

std::string Date::to_string() const {
    time_t seconds = static_cast<time_t>(days_since_epoch) * 86400;
    struct tm tm_buf{};
    gmtime_r(&seconds, &tm_buf);
    std::ostringstream oss;
    oss << std::setfill('0')
        << (tm_buf.tm_year + 1900) << '-'
        << std::setw(2) << (tm_buf.tm_mon + 1) << '-'
        << std::setw(2) << tm_buf.tm_mday;
    return oss.str();
}

}  // namespace qback::domain
