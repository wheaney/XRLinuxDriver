# FindOpenSSL.cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENSSL REQUIRED openssl)

if(OPENSSL_FOUND)
  message(STATUS "OpenSSL found: ${OPENSSL_VERSION}")
else()
  message(FATAL_ERROR "OpenSSL not found")
endif()

add_library(OpenSSL::SSL UNKNOWN IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
  IMPORTED_LOCATION "${OPENSSL_LIBRARIES}"
  INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIRS}"
)
