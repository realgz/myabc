// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/engine_client.hpp --- deployer 侧命名管道客户端
//
// 依据：docs/plan/06-m5-user-dict-learning-plan.md §3.4
//
// DECISION: docs/decisions/_debt-log.md 2026-09-11——跟 tsf-service/backend/ipc_client
// 是同一套协议（myabc::ipc 长度前缀 JSON 帧），但不复用那份实现：这里是一次性管理
// CLI 调用（--export/import/clear-userdict），没有"不能阻塞宿主 UI 线程"的约束
// （tsf-service 那份的 overlapped IO + 50ms 有界等待就是为这个约束存在的），用简单的
// 阻塞 ReadFile/WriteFile + 慷慨超时足够，没必要跨领域引用 tsf-service 的 backend
// （领域边界：跨领域调用应走对方 logic 层，tsf-service 没有为此暴露 logic 接口，
// 这里的需求也足够小，独立实现比硬拉一条跨域依赖更干净）。
//
// 不变量 1（TIP DLL 不链 libpinyin）在这里同样成立：本类只发 JSON 到已运行/新拉起的
// myabc-engine.exe，deployer 自己不链 libpinyin/glib。

#ifndef MYABC_DEPLOY_ENGINE_CLIENT_HPP
#define MYABC_DEPLOY_ENGINE_CLIENT_HPP

#include <windows.h>

#include <cstdint>
#include <string>

#include "json_codec.hpp"

namespace myabc::deploy {

struct EngineClientConfig {
    std::wstring pipe_name;         // 已展开 {sid}
    std::wstring engine_exe_path;   // 连接失败时用于拉起引擎
    std::uint32_t connect_timeout_ms = 5000;   // 管理操作不追求热路径延迟，给足时间
    std::uint32_t request_timeout_ms = 5000;   // 导出/导入涉及文件 IO，比按键请求宽松
};

class EngineClient {
public:
    explicit EngineClient(EngineClientConfig cfg) : cfg_(std::move(cfg)) {}
    ~EngineClient() { Disconnect(); }

    EngineClient(const EngineClient&) = delete;
    EngineClient& operator=(const EngineClient&) = delete;

    bool Connect();
    void Disconnect();

    // 一次性管理调用："成功" = 连上 + 收到 ok:true 的响应。失败时 out_error 填错误信息
    // （来自响应 error.msg，或本地连接/超时描述），供 CLI 直接打印。
    bool UserDictExport(const std::string& path, std::string& out_error);
    bool UserDictImport(const std::string& path, std::string& out_error);
    bool UserDictClear(std::string& out_error);

private:
    bool TryOpenPipe();
    bool LaunchEngine();
    bool CallMethod(ipc::Method method, const ipc::Json& params, ipc::Response& out,
                    std::string& out_error);

    EngineClientConfig cfg_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::uint32_t next_id_ = 1;
};

}  // namespace myabc::deploy

#endif  // MYABC_DEPLOY_ENGINE_CLIENT_HPP
