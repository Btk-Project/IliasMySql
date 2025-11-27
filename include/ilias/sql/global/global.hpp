#pragma once

#include <ilias/task.hpp>
#include <ilias/detail/config.hpp>
#include <ilias/defines.hpp>

#include "config.h"

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

#ifdef _WIN32
#define ILIAS_SQL_DECL_EXPORT __declspec(dllexport)
#define ILIAS_SQL_DECL_IMPORT __declspec(dllimport)
#define ILIAS_SQL_DECL_LOCAL
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define ILIAS_SQL_DECL_EXPORT __attribute__((visibility("default")))
#define ILIAS_SQL_DECL_IMPORT __attribute__((visibility("default")))
#define ILIAS_SQL_DECL_LOCAL  __attribute__((visibility("hidden")))
#else
#define ILIAS_SQL_DECL_EXPORT
#define ILIAS_SQL_DECL_IMPORT
#define ILIAS_SQL_DECL_LOCAL
#endif

#ifndef ILIAS_SQL_STATIC
#ifdef ILIAS_SQL_LIBRARY
#define ILIAS_SQL_API ILIAS_SQL_DECL_EXPORT
#else
#define ILIAS_SQL_API ILIAS_SQL_DECL_IMPORT
#endif
#else
#define ILIAS_SQL_API
#endif