idf_component_register(
    SRCS "src/dw3000_device_api.cpp"
         "src/dw3000_mac_802_15_4.cpp"
         "src/dw3000_port.cpp"
         "src/dw3000_mutex.cpp"
         "src/dw3000_shared_functions.cpp"
         "src/dw3000_uart.cpp"
         "src/dw3000_config_options.cpp"
         "src/dw3000_benchmark.cpp"
    INCLUDE_DIRS "src"
    REQUIRES driver esp_timer
)
