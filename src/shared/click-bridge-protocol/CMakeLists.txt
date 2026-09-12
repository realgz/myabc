# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/shared/click-bridge-protocol --- 候选窗鼠标点击跨进程契约（header-only）
#
# 依据：click_bridge_protocol.hpp 文件头 DECISION。
#
# 只给 candidate-ui / tsf-service 两个 Windows-only 域用，不进 myabc-ipc（那份要
# 供 MinGW 引擎编译，声明不碰 windows.h，见 ipc-protocol/CMakeLists.txt）。

add_library(myabc-click-bridge-protocol INTERFACE)
add_library(myabc::click_bridge_protocol ALIAS myabc-click-bridge-protocol)

target_include_directories(myabc-click-bridge-protocol INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
