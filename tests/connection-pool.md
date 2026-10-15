# Connection pool lifetime regression

Run from the repository root with a C17 compiler, pthreads and GCC/Clang
AddressSanitizer and UndefinedBehaviorSanitizer support:

```sh
cc -std=gnu17 -g -O1 -fsanitize=address,undefined \
  -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
  -Iinclude tests/test_connection_pool_lifetime.c src/connection_pool.c \
  -Wl,--gc-sections -pthread -o /tmp/xr-connection-pool-test
ASAN_OPTIONS=detect_leaks=0 /tmp/xr-connection-pool-test
```

The fake driver waits behind a condition variable while another thread removes
its pool entry. Releasing the fake driver then exercises worker completion and
the waiting caller after removal. No hardware, SDK or service is used.

Leak checking is disabled because the process-lifetime pool has no teardown API;
this test checks invalid memory accesses, not pool teardown. On the original
implementation AddressSanitizer reports a heap-use-after-free in worker completion.
