# docs/CMakeLists.txt - Optional Doxygen-based API documentation.
#
# Only added when RFLCPP_BUILD_DOCS=ON. When Doxygen isn't available we
# still define an `rflcpp_docs` target that just prints a friendly note,
# so the top-level Makefile's `make docs` shortcut keeps working.

find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    set(DOXYGEN_PROJECT_NAME       "rflcpp")
    set(DOXYGEN_PROJECT_NUMBER     "${PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF      "${PROJECT_DESCRIPTION}")
    set(DOXYGEN_OUTPUT_DIRECTORY   "${CMAKE_CURRENT_BINARY_DIR}")
    set(DOXYGEN_GENERATE_HTML      YES)
    set(DOXYGEN_GENERATE_LATEX     NO)
    set(DOXYGEN_EXTRACT_ALL        YES)
    set(DOXYGEN_QUIET              YES)
    set(DOXYGEN_RECURSIVE          YES)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_CURRENT_SOURCE_DIR}/index.md")

    doxygen_add_docs(rflcpp_docs
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating rflcpp documentation with Doxygen"
    )
else()
    add_custom_target(rflcpp_docs
        COMMAND ${CMAKE_COMMAND} -E echo
            "Doxygen not found - skipping HTML docs (the Markdown files in docs/ are still usable)."
    )
endif()
