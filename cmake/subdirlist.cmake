macro(subdirlist result curdir)
    file(GLOB children RELATIVE ${curdir} ${curdir}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${curdir}/${child})
            list(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

function(to_kebab_case INPUT OUTPUT_VAR)
    # Step 1: Insert dashes before uppercase letters
    string(REGEX REPLACE "([A-Z])" "-\\1" _tmp "${INPUT}")
    # Step 2: Remove leading dash (if any)
    string(REGEX REPLACE "^-+" "" _tmp "${_tmp}")
    # Step 3: Convert to lowercase
    string(TOLOWER "${_tmp}" _kebab)
    # Step 4: Return via output variable
    set(${OUTPUT_VAR} "${_kebab}" PARENT_SCOPE)
endfunction()