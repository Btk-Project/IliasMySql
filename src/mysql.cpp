#include "ilias/sql/driver_registry.hpp"

#include "ilias/mysql/detail/mysql.hpp"
#include "ilias/mysql/detail/sqlresultp.hpp"
#include "ilias/mysql/detail/sqlopt.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

class MysqlResultSet : public IResultSet {
public:
    MysqlResultSet(const MysqlResultSet &)            = delete;
    MysqlResultSet(MysqlResultSet &&)                 = default;
    MysqlResultSet &operator=(const MysqlResultSet &) = delete;
    MysqlResultSet &operator=(MysqlResultSet &&)      = default;
    MysqlResultSet(std::unique_ptr<detail::SqlResultBase> imp);
    virtual ~MysqlResultSet();
    auto next() -> IoTask<bool> override;
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;
    auto getValue(size_t index) -> IoResult<SqlValue> override;
    auto getValue(std::string_view name) -> IoResult<SqlValue> override;

private:
    std::unique_ptr<detail::SqlResultBase> mImp;
};
MysqlResultSet::~MysqlResultSet() {
}

MysqlResultSet::MysqlResultSet(std::unique_ptr<detail::SqlResultBase> imp) : mImp(std::move(imp)) {
}

auto MysqlResultSet::next() -> IoTask<bool> {
    return mImp->next();
}
auto MysqlResultSet::columnCount() const -> size_t {
    return mImp->countFields();
}
auto MysqlResultSet::columnName(size_t index) const -> std::string_view {
    return mImp->fieldName(index);
}
auto MysqlResultSet::getValue(size_t index) -> IoResult<SqlValue> {
    co_return co_await mImp->get(index);
}
auto MysqlResultSet::getValue(std::string_view name) -> IoResult<SqlValue> {
    co_return co_await mImp->get(name);
}

ILIAS_MYSQL_NS_END