#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#ifndef ILIAS_POSTGRES_NAMESPACE
#define ILIAS_POSTGRES_NAMESPACE postgres
#endif

#define ILIAS_POSTGRES_COMPLETE_NAMESPACE ILIAS_NAMESPACE::ILIAS_POSTGRES_NAMESPACE
#define ILIAS_POSTGRES_USE_NAMESPACE using namespace ILIAS_POSTGRES_COMPLETE_NAMESPACE;

#define ILIAS_POSTGRES_NS_BEGIN                                                                                        \
    namespace ILIAS_NAMESPACE {                                                                                        \
    namespace ILIAS_POSTGRES_NAMESPACE {

#define ILIAS_POSTGRES_NS_END                                                                                          \
    }                                                                                                                  \
    }
