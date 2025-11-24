#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#ifndef ILIAS_SQL_NAMESPACE
#define ILIAS_SQL_NAMESPACE sql
#endif

#define ILIAS_SQL_COMPLETE_NAMESPACE ILIAS_NAMESPACE::ILIAS_SQL_NAMESPACE
#define ILIAS_SQL_USE_NAMESPACE using namespace ILIAS_SQL_COMPLETE_NAMESPACE;

#define ILIAS_SQL_NS_BEGIN                                                                                             \
    namespace ILIAS_NAMESPACE {                                                                                        \
    namespace ILIAS_SQL_NAMESPACE {

#define ILIAS_SQL_NS_END                                                                                               \
    }                                                                                                                  \
    }