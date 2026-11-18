// SHA-256 and HMAC-SHA-256 (sha256.hpp, spec 011): FIPS 180-4 examples, RFC 4231 test cases, streaming,
// constant-time compare and hex.

#include <string>
#include <vector>

#include "nnh_link/sha256.hpp"
#include "test.hpp"

using namespace nnh::link;

namespace {

std::string hex(const Digest& d) { return to_hex(d.data(), d.size()); }

std::vector<std::uint8_t> bytes(std::size_t n, std::uint8_t value) { return std::vector<std::uint8_t>(n, value); }

std::string hmac_hex(const std::vector<std::uint8_t>& key, const std::string& data) {
    return hex(hmac_sha256(key.data(), key.size(), data.data(), data.size()));
}

} // namespace

NNH_TEST(sha256_matches_the_fips_examples) {
    CHECK(hex(sha256("abc", 3)) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hex(sha256("", 0)) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string two_blocks = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(hex(sha256(two_blocks.data(), two_blocks.size())) ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

NNH_TEST(sha256_of_a_million_a_streamed_in_odd_pieces) {
    Sha256 h;
    const std::string piece(997, 'a');
    std::size_t left = 1000000;
    while (left > 0) {
        const std::size_t n = left < piece.size() ? left : piece.size();
        h.update(piece.data(), n);
        left -= n;
    }
    CHECK(hex(h.finish()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

NNH_TEST(sha256_lengths_around_the_padding_boundary_agree_streamed_and_whole) {
    for (std::size_t n : {std::size_t{55}, std::size_t{56}, std::size_t{63}, std::size_t{64}, std::size_t{65}, std::size_t{119}}) {
        const std::vector<std::uint8_t> data = bytes(n, 0x5C);
        Sha256 h;
        for (std::uint8_t b : data) h.update(&b, 1);
        CHECK(h.finish() == sha256(data.data(), data.size()));
    }
}

NNH_TEST(hmac_sha256_matches_rfc_4231) {
    CHECK(hmac_hex(bytes(20, 0x0b), "Hi There") == "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7");
    const std::string jefe = "Jefe";
    CHECK(hex(hmac_sha256(jefe.data(), jefe.size(), "what do ya want for nothing?", 28)) ==
          "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843");
    CHECK(hmac_hex(bytes(20, 0xaa), std::string(50, '\xdd')) ==
          "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe");
    // Case 6: a key longer than the block size is hashed first.
    CHECK(hmac_hex(bytes(131, 0xaa), "Test Using Larger Than Block-Size Key - Hash Key First") ==
          "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54");
    CHECK(hmac_hex(bytes(131, 0xaa),
                   "This is a test using a larger than block-size key and a larger than block-size data. The key needs "
                   "to be hashed before being used by the HMAC algorithm.") ==
          "9b09ffa71b942fcb27635fbcd5b0e944bfdc63644f0713938a7f51535c3a35e2");
}

NNH_TEST(constant_time_compare_and_hex) {
    const std::uint8_t a[] = {1, 2, 3};
    const std::uint8_t b[] = {1, 2, 4};
    CHECK(equal_constant_time(a, a, 3) && !equal_constant_time(a, b, 3) && equal_constant_time(a, b, 2));
    CHECK(to_hex(a, 3) == "010203");
    std::uint8_t back[3];
    CHECK(from_hex("0a0BfF", back, 3) && back[0] == 0x0a && back[1] == 0x0b && back[2] == 0xff);
    CHECK(!from_hex("0a0b", back, 3) && !from_hex("0a0bzz", back, 3));
}
