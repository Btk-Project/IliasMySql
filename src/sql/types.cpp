#include "ilias/sql/types.hpp"

#include <sstream>
#include <regex>

ILIAS_SQL_NS_BEGIN
auto SqlDate::toString() const -> std::string {
    std::ostringstream oss;
    oss << std::setfill('0');

    auto print_us = [&](std::ostringstream &s) {
        if (microsecond > 0) {
            s << '.' << std::setw(6) << microsecond;
        }
    };
    switch (type) {
        case kDateTime:
            oss << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day << ' '
                << std::setw(2) << hour << ':' << std::setw(2) << minute << ':' << std::setw(2) << second;
            print_us(oss);
            break;
        case kDate:
            oss << std::setw(4) << year << '-' << std::setw(2) << month << '-' << std::setw(2) << day;
            break;
        case kTime:
            oss << std::setw(2) << hour << ':' << std::setw(2) << minute << ':' << std::setw(2) << second;
            print_us(oss);
            break;
        default:
            return "invalid_time";
    }
    return oss.str();
}

auto SqlDate::toTimestamp() const -> uint64_t {
    if (type == kErrorTime)
        return 0;

    std::tm t {};
    t.tm_year  = year - 1900;
    t.tm_mon   = month - 1;
    t.tm_mday  = day;
    t.tm_hour  = hour;
    t.tm_min   = minute;
    t.tm_sec   = second;
    t.tm_isdst = -1;

#ifdef _WIN32
    time_t seconds_since_epoch = _mkgmtime(&t);
#else
    time_t seconds_since_epoch = timegm(&t);
#endif

    if (seconds_since_epoch == -1) {
        return 0;
    }

    return static_cast<uint64_t>(seconds_since_epoch) * 1000000 + microsecond;
}

auto SqlDate::setTime(std::chrono::system_clock::time_point tp) -> void {
    auto us_since_epoch  = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
    auto sec_since_epoch = std::chrono::duration_cast<std::chrono::seconds>(us_since_epoch);

    time_t   tt     = sec_since_epoch.count();
    std::tm *utc_tm = gmtime(&tt); // 使用 gmtime 获取 UTC 时间

    year        = utc_tm->tm_year + 1900;
    month       = utc_tm->tm_mon + 1;
    day         = utc_tm->tm_mday;
    hour        = utc_tm->tm_hour;
    minute      = utc_tm->tm_min;
    second      = utc_tm->tm_sec;
    microsecond = us_since_epoch.count() % 1000000;
    type        = kDateTime;
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
    if (year_ < 0 || month_ < 1 || month_ > 12 || day_ < 1 || day_ > 31 || hour_ < 0 || hour_ > 23 || minute_ < 0 ||
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
    if (year < 0 || month < 1 || month > 12 || day < 1 || day > 31) {
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

auto SqlDate::setTime(std::string_view str) -> void {
    clear();
    // 使用正则表达式来灵活解析常见的 SQL 时间格式
    // 格式1: YYYY-MM-DD HH:MM:SS.FFFFFF (DateTime)
    static const std::regex re_datetime(R"((\d{4})-(\d{2})-(\d{2})[ T](\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,6}))?.*)");
    // 格式2: YYYY-MM-DD (Date)
    static const std::regex re_date(R"(^(\d{4})-(\d{2})-(\d{2})$)");
    // 格式3: HH:MM:SS.FFFFFF (Time)
    static const std::regex re_time(R"(^(\d{2}):(\d{2}):(\d{2})(?:\.(\d{1,6}))?.*)");

    std::cmatch match;
    std::string s_str(str); // regex需要C-string
    ILIAS_TRACE("ilias-sql", "parser time {}", str);

    if (std::regex_match(s_str.c_str(), match, re_datetime)) {
        type   = kDateTime;
        year   = std::stoul(match[1].str());
        month  = std::stoul(match[2].str());
        day    = std::stoul(match[3].str());
        hour   = std::stoul(match[4].str());
        minute = std::stoul(match[5].str());
        second = std::stoul(match[6].str());
        if (match[7].matched) {
            std::string us_str = match[7].str();
            us_str.resize(6, '0'); // 补全到6位微秒
            microsecond = std::stoul(us_str);
        }
    }
    else if (std::regex_match(s_str.c_str(), match, re_date)) {
        type  = kDate;
        year  = std::stoul(match[1].str());
        month = std::stoul(match[2].str());
        day   = std::stoul(match[3].str());
    }
    else if (std::regex_match(s_str.c_str(), match, re_time)) {
        type   = kTime;
        hour   = std::stoul(match[1].str());
        minute = std::stoul(match[2].str());
        second = std::stoul(match[3].str());
        if (match[7].matched) {
            std::string us_str = match[7].str();
            us_str.resize(6, '0'); // 补全到6位微秒
            microsecond = std::stoul(us_str);
        }
    }
    else {
        type = kErrorTime;
        ILIAS_WARN("sql", "Failed to parse time string: {}", str);
    }
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