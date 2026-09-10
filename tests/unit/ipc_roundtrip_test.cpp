// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/ipc_roundtrip_test.cpp --- myabc::ipc 编解码 + 帧读写往返自测
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.1
//       docs/architecture/system-overview.md §4.1 / §7 不变量 6/7
//
// 无第三方测试框架：断言失败即打印并以非零退出，供 CTest 判定。
// 两套工具链都要能编译运行（跨 MSVC/MinGW 的协议契约）。

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "frame.hpp"
#include "json_codec.hpp"
#include "protocol.hpp"

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}

void TestMethodNameRoundTrip() {
    using myabc::ipc::Method;
    using myabc::ipc::MethodFromName;
    using myabc::ipc::MethodName;
    Check(MethodFromName("hello") == Method::kHello, "hello name->enum");
    Check(std::string(MethodName(Method::kProcessKey)) == "processKey", "processKey enum->name");
    Check(MethodFromName("no-such-method") == Method::kUnknown, "unknown method");
    Check(std::string(MethodName(Method::kUnknown)).empty(), "unknown name is empty");
}

void TestRequestRoundTrip() {
    using namespace myabc::ipc;
    Request in;
    in.id = 42;
    in.method = Method::kHello;
    in.params = Json{{"clientVersion", "0.0.0-m0"}, {"pid", 1234}, {"arch", "x64"}};

    const std::string text = EncodeRequest(in);

    Request out;
    Check(DecodeRequest(text, out) == DecodeStatus::kOk, "decode request ok");
    Check(out.id == 42, "request id preserved");
    Check(out.method == Method::kHello, "request method preserved");
    Check(out.params.value("arch", std::string{}) == "x64", "request params preserved");
}

void TestResponseRoundTrip() {
    using namespace myabc::ipc;
    const Response ok = Response::Ok(7, Json{{"engineVersion", "0.0.0"}, {"protocol", kProtocolVersion}});
    Response ok_out;
    Check(DecodeResponse(EncodeResponse(ok), ok_out) == DecodeStatus::kOk, "decode ok-response");
    Check(ok_out.id == 7 && ok_out.ok, "ok-response fields");
    Check(ok_out.result.value("protocol", 0u) == kProtocolVersion, "ok-response result");

    const Response err = Response::Err(8, errc::kUnknownMethod, "no such method");
    Response err_out;
    Check(DecodeResponse(EncodeResponse(err), err_out) == DecodeStatus::kOk, "decode err-response");
    Check(!err_out.ok && err_out.error_code == errc::kUnknownMethod, "err-response code");
    Check(err_out.error_msg == "no such method", "err-response msg");
}

void TestProtocolMismatch() {
    using namespace myabc::ipc;
    // v = 999
    const std::string bad = R"({"v":999,"id":1,"ok":true,"result":{}})";
    Response out;
    Check(DecodeResponse(bad, out) == DecodeStatus::kProtocolMismatch, "protocol mismatch detected");

    Response garbage;
    Check(DecodeResponse("not json", garbage) == DecodeStatus::kBadJson, "bad json detected");
}

void TestFrameRoundTrip() {
    using namespace myabc::ipc;
    std::vector<unsigned char> wire;
    const WriteFn writer = [&](const void* p, std::size_t n) -> std::ptrdiff_t {
        const auto* b = static_cast<const unsigned char*>(p);
        wire.insert(wire.end(), b, b + n);
        return static_cast<std::ptrdiff_t>(n);
    };
    std::size_t read_pos = 0;
    const ReadFn reader = [&](void* p, std::size_t n) -> std::ptrdiff_t {
        const std::size_t avail = wire.size() - read_pos;
        const std::size_t take = n < avail ? n : avail;
        std::copy(wire.begin() + static_cast<std::ptrdiff_t>(read_pos),
                  wire.begin() + static_cast<std::ptrdiff_t>(read_pos + take),
                  static_cast<unsigned char*>(p));
        read_pos += take;
        return static_cast<std::ptrdiff_t>(take);
    };

    const std::string body = R"({"v":1,"id":1,"method":"hello","params":{}})";
    Check(WriteFrame(writer, body) == FrameStatus::kOk, "write frame ok");
    Check(wire.size() == body.size() + 4, "frame = 4B len + body");

    std::string got;
    Check(ReadFrame(reader, got) == FrameStatus::kOk, "read frame ok");
    Check(got == body, "frame body round-trips");

    // 读到末尾 -> kClosed
    std::string eof;
    Check(ReadFrame(reader, eof) == FrameStatus::kClosed, "read past end -> closed");
}

}  // namespace

int main() {
    TestMethodNameRoundTrip();
    TestRequestRoundTrip();
    TestResponseRoundTrip();
    TestProtocolMismatch();
    TestFrameRoundTrip();

    if (g_failures == 0) {
        std::puts("ipc_roundtrip_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "ipc_roundtrip_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
