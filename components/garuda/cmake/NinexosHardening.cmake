# 9xOS hardening and warning flags, applied per target.
# Kept inside the component so it builds standalone (e.g. from Buildroot).

option(NINEXOS_WERROR "Treat compiler warnings as errors" ON)

function(ninexos_harden target)
  target_compile_options(${target} PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wformat=2 -Wformat-security
    -Wstrict-prototypes -Wmissing-prototypes -Wcast-align -Wnull-dereference
    -fstack-protector-strong -fstack-clash-protection -fno-common)
  if(NINEXOS_WERROR)
    target_compile_options(${target} PRIVATE -Werror)
  endif()
  # _FORTIFY_SOURCE needs optimisation; only add it when optimising.
  target_compile_definitions(${target} PRIVATE
    $<$<NOT:$<CONFIG:Debug>>:_FORTIFY_SOURCE=2>)
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_link_options(${target} PRIVATE -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
endfunction()
