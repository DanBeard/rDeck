# rDeck Test Suite

Unit and integration tests for rDeck using the Unity test framework.

## Running Tests

```bash
# Run all unit tests
pio test -e test_native

# Run specific test
pio test -e test_native --filter "test_lxmf"

# Verbose output
pio test -e test_native -v
```

## Directory Structure

```
test/
  test_common/        # Shared test utilities and helpers
    test_helpers.h    # Byte utilities, custom assertions
  unit/               # Unit tests (no hardware/SDL required)
    test_lxmf/        # LXMF message and conversation tests
  integration/        # Integration tests (requires emulator)
```

## Writing Tests

Tests use the [Unity](https://github.com/ThrowTheSwitch/Unity) framework.

```cpp
#include <unity.h>

void setUp(void) {
    // Run before each test
}

void tearDown(void) {
    // Run after each test
}

void test_example(void) {
    TEST_ASSERT_EQUAL(1, 1);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_example);
    return UNITY_END();
}
```

## Test Naming Convention

- Test files: `test_<component>.cpp`
- Test functions: `test_<what_is_being_tested>()`
