add_executable(hookmaker WIN32 hookmaker.cpp hookmaker_res.rc)

option(USE_ROSBE "Use ReactOS Build Environment" FALSE)

if(USE_ROSBE)
    target_link_libraries(hookmaker comctl32 shlwapi cr2 mingwex)
else()
    target_link_libraries(hookmaker comctl32 shlwapi cr2)
endif()

target_compile_options(hookmaker PUBLIC -DUNICODE -D_UNICODE)
