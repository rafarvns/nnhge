// nnh::Result<T> and nnh::Error.

#include <cstring>

#include "nnh/result.hpp"
#include "test.hpp"

using nnh::Error;
using nnh::ErrorCode;
using nnh::Result;

namespace {

Result<int> parse_positive(int v) {
    if (v <= 0) return Error::make(ErrorCode::InvalidArgument, "value %d is not positive", v);
    return v * 2;
}

Result<void> check(bool ok) {
    if (!ok) return Error::make(ErrorCode::NotFound, "missing");
    return {};
}

} // namespace

NNH_TEST(result_holds_a_value_on_success) {
    const Result<int> r = parse_positive(21);
    CHECK(r.ok());
    CHECK(static_cast<bool>(r));
    CHECK(r.value() == 42);
}

NNH_TEST(result_holds_code_and_message_on_failure) {
    const Result<int> r = parse_positive(-3);
    CHECK(!r);
    CHECK(r.error().code == ErrorCode::InvalidArgument);
    CHECK(std::strcmp(r.error().message, "value -3 is not positive") == 0);
}

NNH_TEST(value_or_falls_back_on_failure) {
    CHECK(parse_positive(-1).value_or(7) == 7);
    CHECK(parse_positive(2).value_or(7) == 4);
}

NNH_TEST(void_result_reports_success_or_error) {
    CHECK(check(true).ok());
    const Result<void> r = check(false);
    CHECK(!r.ok());
    CHECK(r.error().code == ErrorCode::NotFound);
}

NNH_TEST(long_error_messages_are_truncated) {
    char big[200];
    std::memset(big, 'e', sizeof big - 1);
    big[sizeof big - 1] = '\0';
    const Error e = Error::make(ErrorCode::IoError, "%s", big);
    CHECK(std::strlen(e.message) == sizeof e.message - 1);
}

NNH_TEST(error_codes_have_names) {
    CHECK(std::strcmp(nnh::error_code_name(ErrorCode::Corrupted), "Corrupted") == 0);
}

namespace {

struct MoveOnly {
    int value = 0;
    explicit MoveOnly(int v) : value(v) {}
    MoveOnly(MoveOnly&& other) noexcept : value(other.value) { other.value = -1; }
    MoveOnly& operator=(MoveOnly&& other) noexcept {
        value = other.value;
        other.value = -1;
        return *this;
    }
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
};

Result<MoveOnly> make_move_only(bool ok) {
    if (!ok) return Error::make(ErrorCode::NotFound, "none");
    return MoveOnly{7};
}

} // namespace

NNH_TEST(result_holds_a_move_only_value) {
    Result<MoveOnly> r = make_move_only(true);
    CHECK(r.ok() && r.value().value == 7);
    Result<MoveOnly> moved = std::move(r);
    CHECK(moved.ok() && moved.value().value == 7);
    MoveOnly taken = std::move(moved.value());
    CHECK(taken.value == 7);
    Result<MoveOnly> failed = make_move_only(false);
    failed = make_move_only(true); // move assignment replaces the error
    CHECK(failed.ok() && failed.value().value == 7);
    failed = make_move_only(false);
    CHECK(!failed.ok() && failed.error().code == ErrorCode::NotFound);
}
