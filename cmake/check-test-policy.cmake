cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED TPMKIT_SOURCE_DIR)
    message(FATAL_ERROR "TPMKIT_SOURCE_DIR is required.")
endif()

file(GLOB_RECURSE test_sources
    "${TPMKIT_SOURCE_DIR}/tests/*.cpp"
)

set(policy_failures "")

foreach(test_source IN LISTS test_sources)
    file(READ "${test_source}" content)

    if(test_source MATCHES "/tests/integration/.+(header_self_contained|headers_smoke)\\.cpp$")
        list(APPEND policy_failures
            "${test_source}: compile-only header smoke tests must live outside tests/integration/")
    endif()

    if(content MATCHES "deferred_to_task|GTEST_SKIP\\([^\\n]*deferred|deferred coverage")
        list(APPEND policy_failures
            "${test_source}: deferred test placeholders are forbidden")
    endif()

    if(test_source MATCHES "/tests/(unit|contract|integration)/" AND
       NOT test_source MATCHES "/tests/interop/" AND
       content MATCHES "GTEST_SKIP\\(")
        list(APPEND policy_failures
            "${test_source}: GTEST_SKIP is forbidden for unit, contract, and integration tests")
    endif()

    if(test_source MATCHES "/tests/contract/" AND content MATCHES "(^|\\n)[ \t]*TEST(_F)?\\(")
        list(APPEND policy_failures
            "${test_source}: contract tests must use TEST_P with INSTANTIATE_TEST_SUITE_P")
    endif()

    if(test_source MATCHES "/tests/contract/" AND
       content MATCHES "(^|\\n)[ \t]*TEST_P\\(" AND
       NOT content MATCHES "INSTANTIATE_TEST_SUITE_P")
        list(APPEND policy_failures
            "${test_source}: TEST_P contract tests require INSTANTIATE_TEST_SUITE_P")
    endif()

    file(STRINGS "${test_source}" lines)
    set(state "search")
    set(test_line 0)
    set(line_number 0)

    foreach(line IN LISTS lines)
        math(EXPR line_number "${line_number} + 1")

        if(state STREQUAL "search")
            if(line MATCHES "^[ \t]*TEST(_F|_P)?\\(")
                set(state "await_brace")
                set(test_line "${line_number}")
            endif()
        elseif(state STREQUAL "await_brace")
            if(line MATCHES "^[ \t]*\\{")
                set(state "await_comment")
            endif()
        elseif(state STREQUAL "await_comment")
            if(line MATCHES "^[ \t]*$")
                continue()
            endif()

            if(NOT line MATCHES "^[ \t]*// ")
                list(APPEND policy_failures
                    "${test_source}:${test_line}: first statement in each TEST must be a short // comment")
            elseif(line MATCHES "^[ \t]*// (Arrange|Act|Assert|Given|When|Then)")
                list(APPEND policy_failures
                    "${test_source}:${test_line}: top TEST comment must state the behavior under test")
            endif()

            set(state "search")
        endif()
    endforeach()
endforeach()

if(policy_failures)
    list(JOIN policy_failures "\n" formatted_failures)
    message(FATAL_ERROR "Test policy check failed:\n${formatted_failures}")
endif()
