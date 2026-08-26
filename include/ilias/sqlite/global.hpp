#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#ifndef ILIAS_SQLITE_NAMESPACE
#define ILIAS_SQLITE_NAMESPACE sqlite
#endif

#define ILIAS_SQLITE_COMPLETE_NAMESPACE ilias::ILIAS_SQLITE_NAMESPACE
#define ILIAS_SQLITE_USE_NAMESPACE using namespace ILIAS_SQLITE_COMPLETE_NAMESPACE;

#define ILIAS_SQLITE_NS_BEGIN                                                                                          \
    namespace ilias {                                                                                                  \
    namespace ILIAS_SQLITE_NAMESPACE {

#define ILIAS_SQLITE_NS_END                                                                                            \
    }                                                                                                                  \
    }
