SET(CMAKE_SYSTEM_PROCESSOR aarch64)

# 设置头文件所在目录
include_directories(${CMAKE_SOURCE_DIR}
#        /home/leo/r328_sdk/usr/include/libnl-tiny
)
# 设置第三方库所在位置
link_directories(${CMAKE_SOURCE_DIR})

#工具链路径
SET(TOOLCHAIN_DIR  "/home/leo/xos/output/host/bin/")
# r328 设置好环境变量可以不指定工具链路径
SET(CMAKE_C_COMPILER ${TOOLCHAIN_DIR}aarch64-pc2-linux-gnu-gcc)
SET(CMAKE_CXX_COMPILER ${TOOLCHAIN_DIR}aarch64-pc2-linux-gnu-g++)
