# This script reads Qt configure arguments from config.opt,
# translates the arguments to CMake arguments and calls CMake.
#
# This file is to be used in CMake script mode with the following variables set:
# OPTFILE: A text file containing the options that were passed to configure
#          with one option per line.
# MODULE_ROOT: The source directory of the module to be built.
#              If empty, qtbase/top-level is assumed.
# TOP_LEVEL: TRUE, if this is a top-level build.

include(${CMAKE_CURRENT_LIST_DIR}/QtFeatureCommon.cmake)

cmake_policy(SET CMP0007 NEW)
cmake_policy(SET CMP0057 NEW)

set(cmake_args "")
macro(push)
    list(APPEND cmake_args ${ARGN})
endmacro()

macro(pop_path_argument)
    list(POP_FRONT configure_args path)
    string(REGEX REPLACE "^\"(.*)\"$" "\\1" path "${path}")
    file(TO_CMAKE_PATH "${path}" path)
endmacro()

if("${MODULE_ROOT}" STREQUAL "")
    # If MODULE_ROOT is not set, assume that we want to build qtbase or top-level.
    get_filename_component(MODULE_ROOT ".." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
else()
    file(TO_CMAKE_PATH "${MODULE_ROOT}" MODULE_ROOT)
endif()
set(configure_filename "configure.cmake")
set(commandline_filename "qt_cmdline.cmake")
if(TOP_LEVEL)
    get_filename_component(MODULE_ROOT "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_LIST_DIR}")
    file(GLOB commandline_files "${MODULE_ROOT}/*/${commandline_filename}")
    if(EXISTS "${MODULE_ROOT}/${commandline_filename}")
        list(PREPEND commandline_files "${MODULE_ROOT}/${commandline_filename}")
    endif()
else()
    set(commandline_files "${MODULE_ROOT}/${commandline_filename}")
endif()
file(STRINGS "${OPTFILE}" configure_args)
list(FILTER configure_args EXCLUDE REGEX "^[ \t]*$")
list(TRANSFORM configure_args STRIP)
unset(generator)
set(auto_detect_generator TRUE)
unset(device_options)
set_property(GLOBAL PROPERTY UNHANDLED_ARGS "")
while(configure_args)
    list(POP_FRONT configure_args arg)
    if(arg STREQUAL "-cmake")
        # ignore
    elseif(arg STREQUAL "-cmake-generator")
        list(POP_FRONT configure_args generator)
    elseif(arg STREQUAL "-cmake-use-default-generator")
        set(auto_detect_generator FALSE)
    elseif(arg STREQUAL "-skip")
        list(POP_FRONT configure_args qtrepo)
        push("-DBUILD_${qtrepo}=OFF")
    elseif(arg STREQUAL "-hostprefix")
        message(FATAL_ERROR "${arg} is not supported in the CMake build.")
    elseif(arg STREQUAL "-external-hostbindir")
        # This points to the bin directory of the Qt installation.
        # This can be multiple levels deep and we cannot deduce the QT_HOST_PATH safely.
        message(FATAL_ERROR "${arg} is not supported anymore. Use -qt-host-path <dir> instead.")
    elseif(arg STREQUAL "-qt-host-path")
        pop_path_argument()
        push("-DQT_HOST_PATH=${path}")
    elseif(arg MATCHES "^-host.*dir")
        message(FATAL_ERROR "${arg} is not supported anymore.")
    elseif(arg STREQUAL "--")
        # Everything after this argument will be passed to CMake verbatim.
        push(${configure_args})
        break()
    else()
        set_property(GLOBAL APPEND PROPERTY UNHANDLED_ARGS "${arg}")
    endif()
endwhile()


####################################################################################################
# Define functions/macros that are called in configure.cmake files
#
# Every function that's called in a configure.cmake file must be defined here.
# Most are empty stubs.
####################################################################################################

set_property(GLOBAL PROPERTY COMMANDLINE_KNOWN_FEATURES "")

function(qt_feature feature)
    set_property(GLOBAL APPEND PROPERTY COMMANDLINE_KNOWN_FEATURES "${feature}")
endfunction()

macro(defstub name)
    function(${name})
    endfunction()
endmacro()

defstub(qt_add_qmake_lib_dependency)
defstub(qt_config_compile_test)
defstub(qt_config_compile_test_machine_tuple)
defstub(qt_config_compile_test_x86simd)
defstub(qt_config_compiler_supports_flag_test)
defstub(qt_config_linker_supports_flag_test)
defstub(qt_configure_add_report_entry)
defstub(qt_configure_add_summary_build_mode)
defstub(qt_configure_add_summary_build_parts)
defstub(qt_configure_add_summary_build_type_and_config)
defstub(qt_configure_add_summary_entry)
defstub(qt_configure_add_summary_section)
defstub(qt_configure_end_summary_section)
defstub(qt_extra_definition)
defstub(qt_feature_config)
defstub(qt_feature_definition)
defstub(qt_find_package)
defstub(set_package_properties)
defstub(qt_qml_find_python)


####################################################################################################
# Define functions/macros that are called in qt_cmdline.cmake files
####################################################################################################

unset(commandline_custom_handlers)
set(commandline_nr_of_prefixes 0)
set(commandline_nr_of_assignments 0)

macro(qt_commandline_subconfig subconfig)
    list(APPEND commandline_subconfigs "${subconfig}")
endmacro()

macro(qt_commandline_custom handler)
    list(APPEND commandline_custom_handlers ${handler})
endmacro()

function(qt_commandline_option name)
    set(options)
    set(oneValueArgs TYPE NAME VALUE)
    set(multiValueArgs VALUES MAPPING)
    cmake_parse_arguments(arg "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(commandline_option_${name} "${arg_TYPE}" PARENT_SCOPE)
    if(NOT "${arg_NAME}" STREQUAL "")
        set(commandline_option_${name}_variable "${arg_NAME}" PARENT_SCOPE)
    endif()
    if(NOT "${arg_VALUE}" STREQUAL "")
        set(commandline_option_${name}_value "${arg_VALUE}" PARENT_SCOPE)
    endif()
    if(arg_VALUES)
        set(commandline_option_${name}_values ${arg_VALUES} PARENT_SCOPE)
    elseif(arg_MAPPING)
        set(commandline_option_${name}_mapping ${arg_MAPPING} PARENT_SCOPE)
    endif()
endfunction()

function(qt_commandline_prefix arg var)
    set(idx ${commandline_nr_of_prefixes})
    set(commandline_prefix_${idx} "${arg}" "${var}" PARENT_SCOPE)
    math(EXPR n "${commandline_nr_of_prefixes} + 1")
    set(commandline_nr_of_prefixes ${n} PARENT_SCOPE)
endfunction()

function(qt_commandline_assignment var internal_var)
    set(idx ${commandline_nr_of_assignments})
    set(commandline_assignment_${idx} "${var}" "${internal_var}" PARENT_SCOPE)
    math(EXPR n "${commandline_nr_of_assignments} + 1")
    set(commandline_nr_of_assignments ${n} PARENT_SCOPE)
endfunction()


####################################################################################################
# Load qt_cmdline.cmake files
####################################################################################################

while(commandline_files)
    list(POP_FRONT commandline_files commandline_file)
    get_filename_component(commandline_file_directory "${commandline_file}" DIRECTORY)
    set(configure_file "${commandline_file_directory}/${configure_filename}")
    unset(commandline_subconfigs)
    if(EXISTS "${configure_file}")
        include("${configure_file}")
    endif()
    if(EXISTS "${commandline_file}")
        include("${commandline_file}")
    endif()

    if(commandline_subconfigs)
        list(TRANSFORM commandline_subconfigs PREPEND "${commandline_file_directory}/")
        list(TRANSFORM commandline_subconfigs APPEND "/${commandline_filename}")
        list(PREPEND commandline_files "${commandline_subconfigs}")
    endif()
endwhile()

get_property(commandline_known_features GLOBAL PROPERTY COMMANDLINE_KNOWN_FEATURES)

####################################################################################################
# Process the data from the qt_cmdline.cmake files
####################################################################################################

function(qtConfAddNote)
    message(${ARGV})
endfunction()

function(qtConfAddWarning)
    message(WARNING ${ARGV})
endfunction()

function(qtConfAddError)
    message(FATAL_ERROR ${ARGV})
endfunction()

set_property(GLOBAL PROPERTY CONFIG_INPUTS "")

function(qtConfCommandlineSetInput name val)
    if(NOT "${commandline_option_${name}_variable}" STREQUAL "")
        set(name "${commandline_option_${name}_variable}")
    endif()
    if(NOT "${INPUT_${name}}" STREQUAL "")
        set(oldval "${INPUT_${name}}")
        if("${oldval}" STREQUAL "${val}")
            qtConfAddNote("Option '${name}' with value '${val}' was specified twice")
        else()
            qtConfAddNote("Overriding option '${name}' with '${val}' (was: '${oldval}')")
        endif()
    endif()

    set_property(GLOBAL PROPERTY INPUT_${name} "${val}")
    set_property(GLOBAL APPEND PROPERTY CONFIG_INPUTS ${name})
endfunction()

function(qtConfCommandlineAppendInput name val)
    get_property(oldval GLOBAL PROPERTY INPUT_${name})
    if(NOT "${oldval}" STREQUAL "")
        string(PREPEND val "${oldval};")
    endif()
    qtConfCommandlineSetInput(${name} "${val}")
endfunction()

function(qtConfValidateValue opt val out_var)
    set(${out_var} TRUE PARENT_SCOPE)

    set(valid_values ${commandline_option_${arg}_values})
    list(LENGTH valid_values n)
    if(n EQUAL 0)
        return()
    endif()

    foreach(v ${valid_values})
        if(val STREQUAL v)
            return()
        endif()
    endforeach()

    set(${out_var} FALSE PARENT_SCOPE)
    qtConfAddError("Invalid value '${val}' supplied to command line option '${opt}'.")
endfunction()

function(qt_commandline_mapped_enum_value opt key out_var)
    unset(result)
    set(mapping ${commandline_option_${opt}_mapping})
    while(mapping)
        list(POP_FRONT mapping mapping_key)
        list(POP_FRONT mapping mapping_value)
        if(mapping_key STREQUAL key)
            set(result "${mapping_value}")
            break()
        endif()
    endwhile()
    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(qtConfHasNextCommandlineArg out_var)
    get_property(args GLOBAL PROPERTY UNHANDLED_ARGS)
    list(LENGTH args n)
    if(n GREATER 0)
        set(result TRUE)
    else()
        set(result FALSE)
    endif()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(qtConfPeekNextCommandlineArg out_var)
    get_property(args GLOBAL PROPERTY UNHANDLED_ARGS)
    list(GET args 0 result)
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(qtConfGetNextCommandlineArg out_var)
    get_property(args GLOBAL PROPERTY UNHANDLED_ARGS)
    list(POP_FRONT args result)
    set_property(GLOBAL PROPERTY UNHANDLED_ARGS ${args})
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(qt_commandline_boolean arg val nextok)
    if("${val}" STREQUAL "")
        set(val "yes")
    endif()
    if(NOT val STREQUAL "yes" AND NOT val STREQUAL "no")
        message(FATAL_ERROR "Invalid value '${val}' given for boolean command line option '${arg}'.")
    endif()
    qtConfCommandlineSetInput("${arg}" "${val}")
endfunction()

function(qt_commandline_string arg val nextok)
    if(nextok)
        qtConfGetNextCommandlineArg(val)
        if("${val}" MATCHES "^-")
            qtConfAddError("No value supplied to command line options '${opt}'.")
        endif()
    endif()
    qtConfValidateValue("${opt}" "${val}" success)
    if(success)
        qtConfCommandlineSetInput("${opt}" "${val}")
    endif()
endfunction()

function(qt_commandline_optionalString arg val nextok)
    if("${val}" STREQUAL "")
        if(nextok)
            qtConfPeekNextCommandlineArg(val)
        endif()
        if(val MATCHES "^-.*|[A-Z0-9_+]=.*" OR val STREQUAL "")
            set(val "yes")
        else()
            qtConfGetNextCommandlineArg(val)
        endif()
    endif()
    qtConfValidateValue("${arg}" "${val}" success)
    if(success)
        qtConfCommandlineSetInput("${arg}" "${val}")
    endif()
endfunction()

function(qt_commandline_addString arg val nextok)
    if("${val}" STREQUAL "" AND nextok)
        qtConfGetNextCommandlineArg(val)
    endif()
    if(val MATCHES "^-.*" OR val STREQUAL "")
        qtConfAddError("No value supplied to command line option '${arg}'.")
    endif()
    qtConfValidateValue("${arg}" "${val}" success)
    if(success)
        if(DEFINED command_line_option_${arg}_variable)
            set(opt ${command_line_option_${arg}_variable)
        endif()
        set_property(GLOBAL APPEND PROPERTY "INPUT_${arg}" "${val}")
        set_property(GLOBAL APPEND PROPERTY CONFIG_INPUTS ${arg})
    endif()
endfunction()

function(qt_commandline_enum arg val nextok)
    if("${val}" STREQUAL "")
        set(val "yes")
    endif()
    unset(mapped)
    if(DEFINED "commandline_option_${arg}_mapping")
        qt_commandline_mapped_enum_value("${arg}" "${val}" mapped)
    elseif(DEFINED "commandline_option_${arg}_values")
        if(val IN_LIST commandline_option_${arg}_values)
            set(mapped ${val})
        endif()
    endif()
    if("${mapped}" STREQUAL "")
        qtConfAddError("Invalid value '${val}' supplied to command line option '${arg}'.")
    endif()
    qtConfCommandlineSetInput("${arg}" "${mapped}")
endfunction()

function(qt_commandline_void arg val nextok)
    if(NOT "${val}" STREQUAL "")
        qtConfAddError("Command line option '${arg}' expects no argument ('${val}' given).")
    endif()
    if(DEFINED commandline_option_${arg}_value)
        set(val ${commandline_option_${arg}_value})
    endif()
    if("${val}" STREQUAL "")
        set(val yes)
    endif()
    qtConfCommandlineSetInput("${arg}" "${val}")
endfunction()

function(qt_call_function func)
    set(call_code "${func}(")
    math(EXPR n "${ARGC} - 1")
    foreach(i RANGE 1 ${n})
        string(APPEND call_code "\"${ARGV${i}}\" ")
    endforeach()
    string(APPEND call_code ")")
    if(${CMAKE_VERSION} VERSION_LESS "3.18.0")
        set(incfile qt_tmp_func_call.cmake)
        file(WRITE "${incfile}" "${call_code}")
        include(${incfile})
        file(REMOVE "${incfile}")
    else()
        cmake_language(EVAL CODE "${call_code}")
    endif()
endfunction()

while(1)
    qtConfHasNextCommandlineArg(has_next)
    if(NOT has_next)
        break()
    endif()
    qtConfGetNextCommandlineArg(arg)

    set(handled FALSE)
    foreach(func ${commandline_custom_handlers})
        qt_call_function("qt_commandline_${func}" handled "${arg}")
        if(handled)
            break()
        endif()
    endforeach()
    if(handled)
        continue()
    endif()

    if(arg MATCHES "^([A-Z0-9_]+)=(.*)")
        set(lhs "${CMAKE_MATCH_1}")
        set(rhs "${CMAKE_MATCH_2}")
        math(EXPR n "${commandline_nr_of_assignments} - 1")
        foreach(i RANGE ${n})
            list(GET commandline_assignment_${i} 0 var)
            list(GET commandline_assignment_${i} 1 internal_var)
            if(lhs STREQUAL var)
                set(handled TRUE)
                qtConfCommandlineSetInput("${internal_var}" "${rhs}")
                break()
            endif()
        endforeach()
        if(NOT handled)
            message(FATAL_ERROR "Assigning unknown variable '${lhs}' on command line.")
        endif()
    endif()
    if(handled)
        continue()
    endif()

    # parse out opt and val
    set(nextok FALSE)
    if(arg MATCHES "^--?enable-(.*)")
        set(opt "${CMAKE_MATCH_1}")
        set(val "yes")
    elseif(arg MATCHES "^--?(disable|no)-(.*)")
        set(opt "${CMAKE_MATCH_2}")
        set(val "no")
    elseif(arg MATCHES "^--([^=]+)=(.*)")
        set(opt "${CMAKE_MATCH_1}")
        set(val "${CMAKE_MATCH_2}")
    elseif(arg MATCHES "^--(.*)")
        set(opt "${CMAKE_MATCH_1}")
        unset(val)
    elseif(arg MATCHES "^-(.*)")
        set(nextok TRUE)
        set(opt "${CMAKE_MATCH_1}")
        unset(val)
        if(NOT DEFINED commandline_option_${opt} AND opt MATCHES "(qt|system)-(.*)")
            set(opt "${CMAKE_MATCH_2}")
            set(val "${CMAKE_MATCH_1}")
            message("opt: ${opt} val: ${val}")
        endif()
    else()
        qtConfAddError("Invalid command line parameter '${arg}'.")
    endif()

    set(type ${commandline_option_${opt}})
    if("${type}" STREQUAL "")
        # No match in the regular options, try matching the prefixes
        math(EXPR n "${commandline_nr_of_prefixes} - 1")
        foreach(i RANGE ${n})
            list(GET commandline_prefix_${i} 0 pfx)
            if(arg MATCHES "^-${pfx}(.*)")
                list(GET commandline_prefix_${i} 1 opt)
                set(val "${CMAKE_MATCH_1}")
                set(type addString)
                break()
            endif()
        endforeach()
    endif()

    # Handle builtin [-no]-feature-xxx
    if("${type}" STREQUAL "" AND opt MATCHES "^feature-(.*)")
        set(opt "${CMAKE_MATCH_1}")
        if(NOT opt IN_LIST commandline_known_features)
            qtConfAddError("Enabling/Disabling unknown feature '${opt}'.")
        endif()
        set(type boolean)
    endif()

    if("${type}" STREQUAL "")
        qtConfAddError("Unknown command line option '${arg}'.")
    endif()

    if(NOT COMMAND "qt_commandline_${type}")
        qtConfAddError("Unknown type '${type}' for command line option '${opt}'.")
    endif()
    qt_call_function("qt_commandline_${type}" "${opt}" "${val}" "${nextok}")
endwhile()

####################################################################################################
# Translate some of the INPUT_xxx values to CMake arguments
####################################################################################################

# Turn the global properties into proper variables
get_property(config_inputs GLOBAL PROPERTY CONFIG_INPUTS)
list(REMOVE_DUPLICATES config_inputs)
foreach(var ${config_inputs})
    get_property(INPUT_${var} GLOBAL PROPERTY INPUT_${var})
endforeach()

macro(drop_input name)
    list(REMOVE_ITEM config_inputs ${name})
endmacro()

macro(translate_boolean_input name cmake_var)
    if("${INPUT_${name}}" STREQUAL "yes")
        push("-D${cmake_var}=ON")
        drop_input(${name})
    elseif("${INPUT_${name}}" STREQUAL "no")
        push("-D${cmake_var}=OFF")
        drop_input(${name})
    endif()
endmacro()

macro(translate_string_input name cmake_var)
    if(DEFINED INPUT_${name})
        push("-D${cmake_var}=${INPUT_${name}}")
        drop_input(${name})
    endif()
endmacro()

macro(translate_path_input name cmake_var)
    if(DEFINED INPUT_${name})
        set(path "${INPUT_${name}}")
        string(REGEX REPLACE "^\"(.*)\"$" "\\1" path "${path}")
        file(TO_CMAKE_PATH "${path}" path)
        push("-D${cmake_var}=${path}")
        drop_input(${name})
    endif()
endmacro()

macro(translate_list_input name cmake_var)
    if(DEFINED INPUT_${name})
        list(JOIN INPUT_${name} "\\;" value)
        list(APPEND cmake_args "-D${cmake_var}=${value}")
        drop_input(${name})
    endif()
endmacro()

drop_input(commercial)
drop_input(confirm-license)
translate_boolean_input(precompile_header BUILD_WITH_PCH)
translate_boolean_input(ccache QT_USE_CCACHE)
translate_boolean_input(shared BUILD_SHARED_LIBS)
translate_boolean_input(warnings_are_errors WARNINGS_ARE_ERRORS)
translate_string_input(qt_namespace QT_NAMESPACE)
translate_string_input(qt_libinfix QT_LIBINFIX)
translate_string_input(qreal QT_COORD_TYPE)
translate_path_input(prefix CMAKE_INSTALL_PREFIX)
translate_path_input(extprefix CMAKE_STAGING_PREFIX)
foreach(kind bin lib archdata libexec qml data doc translation sysconf examples tests)
    string(TOUPPER ${kind} uc_kind)
    translate_path_input(${kind}dir INSTALL_${uc_kind}DIR)
endforeach()
translate_path_input(headerdir INSTALL_INCLUDEDIR)
translate_path_input(plugindir INSTALL_PLUGINSDIR)

if(NOT "${INPUT_device}" STREQUAL "")
    push("-DQT_QMAKE_TARGET_MKSPEC=devices/${INPUT_device}")
    drop_input(device)
endif()
translate_string_input(platform QT_QMAKE_TARGET_MKSPEC)
translate_string_input(xplatform QT_QMAKE_TARGET_MKSPEC)
translate_string_input(qpa_default_platform QT_QPA_DEFAULT_PLATFORM)
translate_list_input(sanitize ECM_ENABLE_SANITIZERS)

translate_path_input(android-sdk ANDROID_SDK_ROOT)
translate_path_input(android-ndk ANDROID_NDK_ROOT)
if(DEFINED INPUT_android-ndk-host)
    drop_input(android-ndk-host)
    qtConfAddWarning("The -android-ndk-host option is not supported with the CMake build. "
        "Determining the right host platform is handled by the CMake toolchain file that is "
        "located in your NDK.")
endif()
if(DEFINED INPUT_android-ndk-platform)
    drop_input(android-ndk-platform)
    string(REGEX REPLACE "^android-" "" INPUT_android-ndk-platform "${INPUT_android-ndk-platform}")
    push("-DANDROID_NATIVE_API_LEVEL=${INPUT_android-ndk-platform}")
endif()
if(DEFINED INPUT_android-abis)
    if(INPUT_android-abis MATCHES ",")
        qtConfAddError("The -android-abis option cannot handle more than one ABI "
            "when building with CMake.")
    endif()
    translate_string_input(android-abis ANDROID_ABI)
endif()
translate_string_input(android-javac-source QT_ANDROID_JAVAC_SOURCE)
translate_string_input(android-javac-target QT_ANDROID_JAVAC_TARGET)

translate_string_input(sdk QT_UIKIT_SDK)
if(DEFINED INPUT_sdk OR (DEFINED INPUT_xplatform AND INPUT_xplatform STREQUAL "macx-ios-clang"))
    push("-DCMAKE_SYSTEM_NAME=iOS")
endif()

drop_input(make)
drop_input(nomake)

foreach(part ${INPUT_nomake})
    if("${part}" STREQUAL "tests")
        push("-DBUILD_TESTING=OFF")
        continue()
    endif()
    if("${part}" STREQUAL "examples")
        push("-DBUILD_EXAMPLES=OFF")
        continue()
    endif()
    qtConfAddWarning("'-nomake ${part}' is not implemented yet.")
endforeach()

foreach(part ${INPUT_make})
    if("${part}" STREQUAL "tests")
        push("-DBUILD_TESTING=ON")
        continue()
    endif()
    if("${part}" STREQUAL "examples")
        push("-DBUILD_EXAMPLES=ON")
        continue()
    endif()
    if("${part}" STREQUAL "tools")
        # default
        continue()
    endif()
    qtConfAddWarning("'-make ${part}' is not implemented yet.")
endforeach()

drop_input(debug)
drop_input(release)
drop_input(debug_and_release)
drop_input(force_debug_info)
unset(build_configs)
if(INPUT_debug)
    set(build_configs Debug)
elseif("${INPUT_debug}" STREQUAL "no")
    set(build_configs Release)
elseif(INPUT_debug_and_release)
    set(build_configs Debug Release)
endif()
if(INPUT_force_debug_info)
    list(TRANSFORM build_configs REPLACE "^Release$" "RelWithDebInfo")
endif()

list(LENGTH build_configs nr_of_build_configs)
if(nr_of_build_configs EQUAL 1)
    push("-DCMAKE_BUILD_TYPE=${build_configs}")
elseif(nr_of_build_configs GREATER 1)
    set(multi_config ON)
    string(REPLACE ";" "\\;" escaped_build_configs "${build_configs}")
    # We must not use the push macro here to avoid variable expansion.
    # That would destroy our escaping.
    list(APPEND cmake_args "-DCMAKE_CONFIGURATION_TYPES=${escaped_build_configs}")
endif()

drop_input(ltcg)
if("${INPUT_ltcg}" STREQUAL "yes")
    foreach(config ${build_configs})
        string(TOUPPER "${config}" ucconfig)
        if(NOT ucconfig STREQUAL "DEBUG")
            push("-DCMAKE_INTERPROCEDURAL_OPTIMIZATION_${ucconfig}=ON")
        endif()
    endforeach()
endif()

translate_list_input(device-option QT_QMAKE_DEVICE_OPTIONS)
translate_list_input(defines QT_EXTRA_DEFINES)
translate_list_input(fpaths QT_EXTRA_FRAMEWORKPATHS)
translate_list_input(includes QT_EXTRA_INCLUDEPATHS)
translate_list_input(lpaths QT_EXTRA_LIBDIRS)
translate_list_input(rpaths QT_EXTRA_RPATHS)

foreach(feature ${commandline_known_features})
    qt_feature_normalize_name("${feature}" cmake_feature)
    if(${feature} IN_LIST config_inputs)
        translate_boolean_input(${feature} FEATURE_${cmake_feature})
    endif()
endforeach()

foreach(input ${config_inputs})
    push("-DINPUT_${input}=${INPUT_${input}}")
endforeach()

if(NOT generator AND auto_detect_generator)
    find_program(ninja ninja)
    if(ninja)
        set(generator Ninja)
        if(multi_config)
            string(APPEND generator " Multi-Config")
        endif()
    else()
        if(CMAKE_HOST_UNIX)
            set(generator "Unix Makefiles")
        elseif(CMAKE_HOST_WINDOWS)
            find_program(msvc_compiler cl.exe)
            if(msvc_compiler)
                set(generator "NMake Makefiles")
                find_program(jom jom)
                if(jom)
                    string(APPEND generator " JOM")
                endif()
            else()
                set(generator "MinGW Makefiles")
            endif()
        endif()
    endif()
endif()
if(generator)
    push(-G "${generator}")
endif()

push("${MODULE_ROOT}")

execute_process(COMMAND "${CMAKE_COMMAND}" ${cmake_args}
    COMMAND_ECHO STDOUT
    RESULT_VARIABLE exit_code)
if(NOT exit_code EQUAL 0)
    message(FATAL_ERROR "CMake exited with code ${exit_code}.")
endif()
