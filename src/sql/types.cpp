#include "ilias/sql/types.hpp"

#include <sstream>
#include <regex>
#include <chrono>

ILIAS_SQL_NS_BEGIN
auto SqlDate::toLocalString() const -> std::string {
    if (type == kErrorTime) {
        return "invalid_time";
    }

    // 步骤 A: 将存储的UTC时间组件转换回 time_t 时间戳
    std::tm t_utc {};
    t_utc.tm_year  = year - 1900;
    t_utc.tm_mon   = month - 1;
    t_utc.tm_mday  = day;
    t_utc.tm_hour  = hour;
    t_utc.tm_min   = minute;
    t_utc.tm_sec   = second;
    t_utc.tm_isdst = 0;

#ifdef _WIN32
    time_t seconds_since_epoch = _mkgmtime(&t_utc);
#else
    time_t seconds_since_epoch = timegm(&t_utc);
#endif

    if (seconds_since_epoch == -1) {
        return "invalid_time";
    }

    // 步骤 B: 将 time_t 时间戳转换为本地时区的 tm 结构
    std::tm tm_local {};
#ifdef _WIN32
    localtime_s(&tm_local, &seconds_since_epoch);
#else
    localtime_r(&seconds_since_epoch, &tm_local);
#endif

    // 步骤 C: 使用本地时间的 tm 结构来格式化字符串
    std::ostringstream oss;
    oss << std::setfill('0');

    auto print_us = [&](std::ostringstream &s) {
        if (microsecond > 0) {
            s << '.' << std::setw(6) << microsecond;
        }
    };

    switch (type) {
        case kDateTime:
            oss << std::setw(4) << tm_local.tm_year + 1900 << '-' << std::setw(2) << tm_local.tm_mon + 1 << '-'
                << std::setw(2) << tm_local.tm_mday << ' ' << std::setw(2) << tm_local.tm_hour << ':' << std::setw(2)
                << tm_local.tm_min << ':' << std::setw(2) << tm_local.tm_sec;
            print_us(oss);
            break;
        case kDate:
            // 日期通常不涉及时区转换，但为保持一致性也使用local
            oss << std::setw(4) << tm_local.tm_year + 1900 << '-' << std::setw(2) << tm_local.tm_mon + 1 << '-'
                << std::setw(2) << tm_local.tm_mday;
            break;
        case kTime:
            // 时间同样转换为本地时间
            oss << std::setw(2) << tm_local.tm_hour << ':' << std::setw(2) << tm_local.tm_min << ':' << std::setw(2)
                << tm_local.tm_sec;
            print_us(oss);
            break;
        default:
            return "invalid_time";
    }
    return oss.str();
}

auto SqlDate::toUTCString() const -> std::string {
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
    auto tp = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(timestamp);
    setTime(tp);
}

auto SqlDate::setTime(int year_, int month_, int day_, int hour_, int minute_, int second_, int microsecond_,
                      int time_zone_offset_minutes) -> void {
    clear();

    // 1. 使用 std::chrono::year_month_day 验证日期组件的有效性
    const std::chrono::year_month_day ymd {std::chrono::year {year_},
                                           std::chrono::month {static_cast<unsigned int>(month_)},
                                           std::chrono::day {static_cast<unsigned int>(day_)}};

    // 2. 检查所有时间组件的范围是否有效
    if (!ymd.ok() || hour_ < 0 || hour_ > 23 || minute_ < 0 || minute_ > 59 || second_ < 0 || second_ > 59 ||
        microsecond_ < 0 || microsecond_ > 999999) {
        ILIAS_ERROR("sql", "error date time set {}-{}-{} {}:{}:{}", year_, month_, day_, hour_, minute_, second_);
        return;
    }

    // 3. 从输入组件构造一个 "本地" 时间点 (local_time)
    // local_time 代表一个未指定时区的时间
    auto local_tp = std::chrono::local_days {ymd} + std::chrono::hours {hour_} + std::chrono::minutes {minute_} +
                    std::chrono::seconds {second_} + std::chrono::microseconds {microsecond_};

    // 4. 应用时区偏移量，将本地时间转换为 UTC 时间 (sys_time)
    // 从本地时间转为 UTC 时间，需要减去偏移量
    const std::chrono::minutes                       offset {time_zone_offset_minutes};
    std::chrono::sys_time<std::chrono::microseconds> utc_tp {(local_tp - offset).time_since_epoch()};

    // 5. 将 UTC 时间点分解为年月日和时分秒
    auto                        utc_dp = std::chrono::floor<std::chrono::days>(utc_tp);
    std::chrono::year_month_day utc_ymd {utc_dp};
    auto                        time_of_day = utc_tp - utc_dp;
    std::chrono::hh_mm_ss       hms {time_of_day};

    // 6. 将最终的 UTC 时间赋值给成员变量
    year        = static_cast<int>(utc_ymd.year());
    month       = static_cast<unsigned int>(utc_ymd.month());
    day         = static_cast<unsigned int>(utc_ymd.day());
    hour        = hms.hours().count();
    minute      = hms.minutes().count();
    second      = hms.seconds().count();
    microsecond = static_cast<uint32_t>(hms.subseconds().count());
    type        = kDateTime;
}

auto SqlDate::setDate(int _year, int _month, int _day) -> void {
    clear();
    if (_year < 0 || _month < 1 || _month > 12 || _day < 1 || _day > 31) {
        ILIAS_ERROR("sql", "error date set {}-{}-{}", _year, _month, _day);
        return;
    }
    this->year  = _year;
    this->month = _month;
    this->day   = _day;
    type        = kDate;
}

auto SqlDate::setTime(int _hour, int _minute, int _second) -> void {
    clear();
    if (_hour < 0 || _hour > 23 || _minute < 0 || _minute > 59 || _second < 0 || _second > 59) {
        ILIAS_ERROR("sql", "error time set {}:{}:{}", _hour, _minute, _second);
        return;
    }
    this->hour   = _hour;
    this->minute = _minute;
    this->second = _second;
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

auto SqlDate::fromUTCString(std::string_view str) -> void {
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

auto SqlDate::fromLocalString(std::string_view str) -> void {
    fromUTCString(str);
    if (type == kErrorTime) {
        return;
    }
    // 转换为UTC时间
    auto tm = static_cast<struct tm>(*this);
    auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    setTime(tp);
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

auto SqlDate::now() -> SqlDate {
    SqlDate date;
    date.setTime(std::chrono::system_clock::now());
    return date;
}

ILIAS_SQL_NS_END