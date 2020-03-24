add_executable(hookmaker WIN32 hookmaker.cpp hookmaker_res.rc)
target_link_libraries(hookmaker comctl32 shlwapi)
