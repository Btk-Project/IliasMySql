#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#ifndef ILIAS_MYSQL_NAMESPACE
#define ILIAS_MYSQL_NAMESPACE mysql
#endif

#define ILIAS_MYSQL_COMPLETE_NAMESPACE ILIAS_NAMESPACE::ILIAS_MYSQL_NAMESPACE
#define ILIAS_MYSQL_USE_NAMESPACE using namespace ILIAS_MYSQL_COMPLETE_NAMESPACE;

#define ILIAS_MYSQL_NS_BEGIN                                                                                             \
    namespace ILIAS_NAMESPACE {                                                                                        \
    namespace ILIAS_MYSQL_NAMESPACE {

#define ILIAS_MYSQL_NS_END                                                                                               \
    }                                                                                                                  \
    }