#pragma once

#include "detail/global.hpp"

ILIAS_SQL_NS_BEGIN

class SqlStmt {
public:
    virtual ~SqlStmt() = default;
    
private:
    SqlStmt(const SqlStmt &)            = delete;
    SqlStmt &operator=(const SqlStmt &) = delete;
    SqlStmt()                           = default;
};

ILIAS_SQL_NS_END