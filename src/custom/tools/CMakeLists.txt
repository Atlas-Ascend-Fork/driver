add_executable(parse_type131
    parse_type131.c
)
 
target_include_directories(parse_type131 PRIVATE
    ${C_SEC_INCLUDE}
)
 
add_dependencies(parse_type131 c_sec)
target_link_libraries(parse_type131 PRIVATE c_sec)
 
target_compile_options(parse_type131 PRIVATE
    -Wall -Werror -fstack-protector-strong -fPIC -fPIE
)
 
target_link_options(parse_type131 PRIVATE
    -Wl,-z,relro,-z,now,-z,noexecstack -pie -s
)

set(INSTALL_RUNTIME_DIR bin)
install(TARGETS parse_type131
    RUNTIME DESTINATION ${INSTALL_RUNTIME_DIR} OPTIONAL
)