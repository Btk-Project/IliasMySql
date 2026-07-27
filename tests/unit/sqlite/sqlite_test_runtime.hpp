#pragma once

#include <cstdio>

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <sqlite3.h>

#include "ilias/sql/global/config.h"
#include "ilias/sqlite/sqlite.hpp"

#if defined(ENABLE_SQLCIPHER_PLUGINS)
#include <openssl/crypto.h>
#endif

namespace sqlite_test {

// SQLite and SQLCipher own process-wide state in addition to per-connection
// handles. Keep this cleanup at the test executable boundary: calling it from
// a connection destructor would race with other live connections, and calling
// OPENSSL_cleanup() from the library would affect unrelated OpenSSL users in
// the host process.
inline auto runAllTests() -> int {
    int result = 0;
    {
        ilias::PlatformContext ioContext;
        ioContext.install();
        result = RUN_ALL_TESTS();
    }

    const int shutdownResult = ILIAS_SQLITE_COMPLETE_NAMESPACE::shutdownRuntime();
    if (shutdownResult != SQLITE_OK) {
        std::fprintf(stderr, "sqlite3_shutdown() failed with code %d\n", shutdownResult);
        result = 1;
    }
    const int repeatedShutdownResult = ILIAS_SQLITE_COMPLETE_NAMESPACE::shutdownRuntime();
    if (repeatedShutdownResult != shutdownResult) {
        std::fprintf(stderr, "repeated SQLite shutdown changed result from %d to %d\n", shutdownResult,
                     repeatedShutdownResult);
        result = 1;
    }

#if defined(ENABLE_SQLCIPHER_PLUGINS)
    // SQLCipher intentionally does not tear down OpenSSL's process-wide state,
    // because OpenSSL may be shared by its host. This test executable owns its
    // process and can perform the terminal cleanup after SQLCipher is shut down.
    OPENSSL_cleanup();
#endif

    return result;
}

} // namespace sqlite_test
