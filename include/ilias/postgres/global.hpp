#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#ifndef ILIAS_POSTGRES_NAMESPACE
#define ILIAS_POSTGRES_NAMESPACE postgres
#endif

#define ILIAS_POSTGRES_COMPLETE_NAMESPACE ilias::ILIAS_POSTGRES_NAMESPACE
#define ILIAS_POSTGRES_USE_NAMESPACE using namespace ILIAS_POSTGRES_COMPLETE_NAMESPACE;

#define ILIAS_POSTGRES_NS_BEGIN                                                                                        \
    namespace ilias {                                                                                        \
    namespace ILIAS_POSTGRES_NAMESPACE {

#define ILIAS_POSTGRES_NS_END                                                                                          \
    }                                                                                                                  \
    }
