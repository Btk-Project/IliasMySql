#include "ilias/sql/types.hpp"

ILIAS_SQL_NS_BEGIN
auto SqlDate::toString() const -> std::string {
    switch (type) {
        case kDateTime:
            return std::to_string(year) + "-" + std::to_string(month) + "-" + std::to_string(day) + " " +
                   std::to_string(hour) + ":" + std::to_string(minute) + ":" + std::to_string(second) + "." +
                   std::to_string(microsecond);
        case kDate:
            return std::to_string(year) + "-" + std::to_string(month) + "-" + std::to_string(day);
        case kTime:
            return std::to_string(hour) + ":" + std::to_string(minute) + ":" + std::to_string(second) + "." +
                   std::to_string(microsecond);
        default:
            return "error time";
    }
}

auto SqlDate::toTimestamp() const -> uint64_t {
    switch (type) {
        case kDateTime:
            return year * 31536000 + month * 2592000 + day * 86400 + hour * 3600 + minute * 60 + second +
                   microsecond / 1000000;
        case kDate:
            return year * 31536000 + month * 2592000 + day * 86400;
        case kTime:
            return hour * 3600 + minute * 60 + second + microsecond / 1000000;
        default:
            return 0;
    }
}

auto SqlDate::setTime(std::chrono::system_clock::time_point tp) -> void {
    auto     tt  = std::chrono::system_clock::to_time_t(tp);
    std::tm *now = gmtime(&tt);
    setTime(now);
}

auto SqlDate::setTime(std::chrono::milliseconds timestamp) -> void {
    auto     milliseconds = std::chrono::milliseconds(timestamp);
    auto     tp           = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(milliseconds);
    auto     tt           = std::chrono::system_clock::to_time_t(tp);
    std::tm *now          = gmtime(&tt);
    setTime(now);
}

auto SqlDate::setTime(int year_, int month_, int day_, int hour_, int minute_, int second_, int microsecond_) -> void {
    clear();
    if (year_ < 0 || month_ < 0 || month_ > 12 || day_ < 0 || day_ > 31 || hour_ < 0 || hour_ > 23 || minute_ < 0 ||
        minute_ > 59 || second_ < 0 || second_ > 59 || microsecond_ < 0 || microsecond_ > 999999) {
        ILIAS_ERROR("sql", "error date time set {}-{}-{} {}:{}:{}", year_, month_, day_, hour_, minute_, second_);
        return;
    }
    year        = year_;
    month       = month_;
    day         = day_;
    hour        = hour_;
    minute      = minute_;
    second      = second_;
    microsecond = microsecond_;
    type        = kDateTime;
}

auto SqlDate::setDate(int year, int month, int day) -> void {
    clear();
    if (year < 0 || month < 0 || month > 12 || day < 0 || day > 31) {
        ILIAS_ERROR("sql", "error date set {}-{}-{}", year, month, day);
        return;
    }
    this->year  = year;
    this->month = month;
    this->day   = day;
    type        = kDate;
}

auto SqlDate::setTime(int hour, int minute, int second) -> void {
    clear();
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        ILIAS_ERROR("sql", "error time set {}:{}:{}", hour, minute, second);
        return;
    }
    this->hour   = hour;
    this->minute = minute;
    this->second = second;
    type         = kTime;
}

auto SqlDate::setTime(struct tm *timeinfo) -> void {
    year        = timeinfo->tm_year;
    month       = timeinfo->tm_mon;
    day         = timeinfo->tm_mday;
    hour        = timeinfo->tm_hour;
    minute      = timeinfo->tm_min;
    second      = timeinfo->tm_sec;
    type        = kDateTime;
    microsecond = 0;
}

auto SqlDate::setTimeType(TimeType type) -> void {
    this->type = type;
}

auto SqlDate::setTime(std::string_view str, std::string_view fmt) -> void {
    if (fmt == "") {
        fmt = "%Y-%m-%d %H:%M:%S";
    }
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(struct tm));
    std::istringstream istr((std::string(str)));
    istr >> std::get_time(&timeinfo, std::string(fmt).c_str());
    timeinfo.tm_year += 1900;
    timeinfo.tm_mon += 1;
    setTime(&timeinfo);
}

void SqlDate::clear() {
    type        = kErrorTime;
    year        = 0;
    month       = 0;
    day         = 0;
    hour        = 0;
    minute      = 0;
    second      = 0;
    microsecond = 0;
}

ILIAS_SQL_NS_END