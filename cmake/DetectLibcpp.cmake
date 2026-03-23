include(CheckCXXSourceCompiles)

function(detect_libcpp)
    set(OLD_CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS})
    set(CMAKE_REQUIRED_FLAGS "${CMAKE_REQUIRED_FLAGS} -stdlib=libc++ -std=c++23")
    
    check_cxx_source_compiles("
        #include <iostream>
        #include <expected>
        int main() {
            std::expected<int, int> e = 1;
            return 0;
        }
    " HAS_LIBCPP)
    
    set(CMAKE_REQUIRED_FLAGS ${OLD_CMAKE_REQUIRED_FLAGS})
endfunction()
