idf_component_register( SRCS "job_manager.cpp"
                        PRIV_REQUIRES iot_is freertos esp_event esp-idf-cxx
                        INCLUDE_DIRS ".")