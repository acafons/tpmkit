include_guard(GLOBAL)

function(tpmkit_target_accepts_link_options target out_var)
    get_target_property(target_type "${target}" TYPE)
    if(target_type STREQUAL "STATIC_LIBRARY"
       OR target_type STREQUAL "OBJECT_LIBRARY"
       OR target_type STREQUAL "INTERFACE_LIBRARY")
        set("${out_var}" OFF PARENT_SCOPE)
    else()
        set("${out_var}" ON PARENT_SCOPE)
    endif()
endfunction()

function(tpmkit_apply_project_options target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "Cannot apply tpmkit project options to missing target: ${target}")
    endif()

    tpmkit_target_accepts_link_options("${target}" tpmkit_can_use_link_options)

    if(MSVC)
        target_compile_options("${target}" PRIVATE /W4 /permissive-)

        if(tpmkit_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options("${target}"
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wnon-virtual-dtor
                -Wold-style-cast
                -Wcast-align
                -Wunused
                -Woverloaded-virtual
                -Wnull-dereference
                -Wdouble-promotion
                -Wformat=2
        )

        if(tpmkit_WARNINGS_AS_ERRORS)
            target_compile_options("${target}" PRIVATE -Werror)
        endif()

        target_compile_options("${target}"
            PRIVATE
                $<$<CONFIG:Release>:-D_FORTIFY_SOURCE=2>
                $<$<CONFIG:Release>:-fstack-protector-strong>
                $<$<CONFIG:Release>:-fstack-clash-protection>
                $<$<CONFIG:Release>:-fPIE>
        )
        if(tpmkit_can_use_link_options)
            target_link_options("${target}"
                PRIVATE
                    $<$<CONFIG:Release>:-pie>
                    $<$<CONFIG:Release>:-Wl,-z,relro,-z,now,-z,noexecstack>
            )
        endif()

        if(tpmkit_USE_ASAN)
            target_compile_options("${target}" PRIVATE -fsanitize=address -fno-omit-frame-pointer)
            if(tpmkit_can_use_link_options)
                target_link_options("${target}" PRIVATE -fsanitize=address)
            endif()
        endif()

        if(tpmkit_USE_UBSAN)
            target_compile_options("${target}" PRIVATE -fsanitize=undefined -fno-omit-frame-pointer)
            if(tpmkit_can_use_link_options)
                target_link_options("${target}" PRIVATE -fsanitize=undefined)
            endif()
        endif()

        if(tpmkit_USE_TSAN)
            target_compile_options("${target}" PRIVATE -fsanitize=thread -fno-omit-frame-pointer)
            if(tpmkit_can_use_link_options)
                target_link_options("${target}" PRIVATE -fsanitize=thread)
            endif()
        endif()

        if(TPMKIT_COVERAGE)
            target_compile_options("${target}" PRIVATE --coverage -O0 -g)
            if(tpmkit_can_use_link_options)
                target_link_options("${target}" PRIVATE --coverage)
            endif()
        endif()
    endif()
endfunction()

function(tpmkit_configure_library_target target)
    target_compile_features("${target}" PUBLIC cxx_std_17)
    set_target_properties("${target}" PROPERTIES
        CXX_EXTENSIONS OFF
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    tpmkit_apply_project_options("${target}")

    if(NOT BUILD_SHARED_LIBS)
        target_compile_definitions("${target}" PUBLIC TPMKIT_STATIC_DEFINE)
    endif()
endfunction()

function(tpmkit_configure_executable_target target)
    target_compile_features("${target}" PRIVATE cxx_std_17)
    set_target_properties("${target}" PROPERTIES
        CXX_EXTENSIONS OFF
    )
    tpmkit_apply_project_options("${target}")
endfunction()
