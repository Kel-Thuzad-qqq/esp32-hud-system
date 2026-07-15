file(GLOB IMMU_SOURCES "${CMAKE_SOURCE_DIR}/imu/src/*.c" )



message("${IMMU_SOURCES}")
idf_component_register(SRCS 
                   "imu/src/imu_data_decode.c"
                   "imu/src/packet.c"
                    "i2c_oled_example_main.c" 
                    INCLUDE_DIRS "."
                    "imu/inc"
                       )
