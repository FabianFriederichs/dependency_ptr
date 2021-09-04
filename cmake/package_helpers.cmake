include(CMakePackageConfigHelpers)
include(CMakeParseArguments)

function(make_package)
    # process arguments
    set(MY_PACKAGE_OPTIONS
        HEADER_ONLY 
    )
    set(MY_PACKAGE_ONE_VALUE_PARAMS
        NAME
        TARGETS
        RUNTIME_DESTINATION
        LIBRARY_DESTINATION
        ARCHIVE_DESTINATION
        INCLUDE_DESTINATION
        CONFIG_DESTINATION
        VERSION_MAJOR
        VERSION_MINOR
        VERSION_PATCH
        COMPATIBILITY
    )
    set(MY_PACKAGE_MULTI_VALUE_PARAMS
        PUBLIC_INCLUDE_DIRECTORIES
        DEPENDENCIES
    )
    cmake_parse_arguments(
        MY_PACKAGE
        "${MY_PACKAGE_OPTIONS}"
        "${MY_PACKAGE_ONE_VALUE_PARAMS}"
        "${MY_PACKAGE_MULTI_VALUE_PARAMS}"
        ${ARGN}
    )
    # version variable
    set(MY_PACKAGE_VERSION ${MY_PACKAGE_VERSION_MAJOR}.${MY_PACKAGE_VERSION_MINOR}.${MY_PACKAGE_VERSION_PATCH})
    # install target
    install(TARGETS ${MY_PACKAGE_TARGETS}
        EXPORT "${MY_PACKAGE_NAME}Targets"
        RUNTIME DESTINATION ${MY_PACKAGE_RUNTIME_DESTINATION}
        LIBRARY DESTINATION ${MY_PACKAGE_LIBRARY_DESTINATION}
        ARCHIVE DESTINATION ${MY_PACKAGE_ARCHIVE_DESTINATION}
    )
    # install include directory
    install(DIRECTORY "${MY_PACKAGE_PUBLIC_INCLUDE_DIRECTORIES}" DESTINATION "${MY_PACKAGE_INCLUDE_DESTINATION}")
    # export targets
    install(EXPORT "${MY_PACKAGE_NAME}Targets" FILE "${MY_PACKAGE_NAME}Targets.cmake" DESTINATION "${MY_PACKAGE_CONFIG_DESTINATION}")    
    # create package
    # create config file template
    set(package_file_config_template_string "
# package header
set(@MY_PACKAGE_NAME@_VERSION @MY_PACKAGE_VERSION@)

@PACKAGE_INIT@

# import targets
include(\"\${CMAKE_CURRENT_LIST_DIR}/@MY_PACKAGE_NAME@Targets.cmake\")

# package dependencies
@MY_PACKAGE_DEPENDENCY_STRINGS@

# check components
check_required_components(@MY_PACKAGE_NAME@)
")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}Config.cmake.in" "${package_file_config_template_string}")
    list(LENGTH MY_PACKAGE_DEPENDENCIES num_dependencies)
    if(${num_dependencies} GREATER_EQUAL 0)
        set(MY_PACKAGE_DEPENDENCY_STRINGS "include(CMakeFindDependencyMacro)")
        # append all dependencies
        foreach(dep IN ITEMS ${MY_PACKAGE_DEPENDENCIES})
            string(CONCAT MY_PACKAGE_DEPENDENCY_STRINGS ${MY_PACKAGE_DEPENDENCY_STRINGS} "\nfind_dependency(${dep})")
        endforeach()
    endif()
    # configure config file
    configure_package_config_file("${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}Config.cmake"
        INSTALL_DESTINATION "${MY_PACKAGE_CONFIG_DESTINATION}"
    )
    # versioning
    if(NOT MY_PACKAGE_HEADER_ONLY)
        set_target_properties(${MY_PACKAGE_TARGETS}
            PROPERTIES
                VERSION "${MY_PACKAGE_VERSION}"
                SOVERSION ${MY_PACKAGE_VERSION_MAJOR}
        )
    endif()
    if(MY_PACKAGE_HEADER_ONLY)
        write_basic_package_version_file(
                "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}ConfigVersion.cmake"
                VERSION "${MY_PACKAGE_VERSION}"
                COMPATIBILITY ${MY_PACKAGE_COMPATIBILITY}
                ARCH_INDEPENDENT
        )
    else()
        write_basic_package_version_file(
                "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}ConfigVersion.cmake"
                VERSION "${MY_PACKAGE_VERSION}"
                COMPATIBILITY ${MY_PACKAGE_COMPATIBILITY}
        )
    endif()
    # install package files
    install(
        FILES 
            "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/${MY_PACKAGE_NAME}ConfigVersion.cmake"
        DESTINATION
            "${MY_PACKAGE_CONFIG_DESTINATION}"
    )
endfunction()