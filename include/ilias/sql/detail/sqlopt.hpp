#pragma once

#include "detail/global.hpp"

ILIAS_SQL_NS_BEGIN

class ISqlContext;
namespace sqlopt {

class OptionBase {
public:
    virtual auto setopt(ISqlContext *sql) const -> int = 0;
    virtual auto getopt(ISqlContext *sql) -> int       = 0;
};

} // namespace sqlopt
ILIAS_SQL_NS_END
