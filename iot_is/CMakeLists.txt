idf_component_register( SRCS "iot_is.cpp"
                        REQUIRES mqtt jobs esp-idf-cxx
                        INCLUDE_DIRS "." "fbs" "include")

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)