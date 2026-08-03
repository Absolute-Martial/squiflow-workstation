#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "platform/secret_envelope.hpp"
#include "platform/secret_identity.hpp"
#include "platform/secrets.hpp"
#include "platform/testing/fake_secret_store.hpp"

namespace p = squiflow::platform;

TEST(SecretBuffer, PreservesBinaryDataAndMovesOwnership) {
    const std::array<std::byte, 4> source{
        std::byte{0x01}, std::byte{0x00}, std::byte{0xff}, std::byte{0x02}};
    p::SecretBuffer first(source);
    p::SecretBuffer second(std::move(first));
    EXPECT_TRUE(first.empty());
    ASSERT_EQ(second.size(), source.size());
    EXPECT_TRUE(std::equal(second.bytes().begin(), second.bytes().end(), source.begin()));
    second.clear();
    EXPECT_TRUE(second.empty());
    second.clear();
    EXPECT_TRUE(second.empty());
}

class InvalidEnvelope : public testing::TestWithParam<std::vector<std::byte>> {};

TEST_P(InvalidEnvelope, IsRejectedBeforeUnprotect) {
    EXPECT_FALSE(p::decode_secret_envelope(GetParam()).ok);
}

INSTANTIATE_TEST_SUITE_P(
    Malformed,
    InvalidEnvelope,
    testing::Values(
        std::vector<std::byte>{},
        std::vector<std::byte>(5),
        std::vector<std::byte>(p::kSecretEnvelopeHeaderSize)));

TEST(FakeSecretStore, FailedReplacementPreservesOldCredential) {
    p::testing::FakeSecretStore store;
    const std::array<std::byte, 2> old_value{std::byte{1}, std::byte{2}};
    const std::array<std::byte, 1> new_value{std::byte{9}};
    ASSERT_TRUE(store.store("sync.token", old_value).ok);
    store.next_fault = p::SecretFault::ReplaceFailed;
    EXPECT_FALSE(store.store("sync.token", new_value).ok);
    const auto loaded = store.load("sync.token");
    ASSERT_TRUE(loaded.ok);
    EXPECT_EQ(loaded.value.size(), old_value.size());
}

TEST(SecretIdentity, RefusesTraversalAndAmbiguousNames) {
    EXPECT_FALSE(p::is_valid_secret_key("../token"));
    EXPECT_FALSE(p::is_valid_secret_key("a..b"));
    EXPECT_FALSE(p::is_valid_secret_key("UPPER"));
    EXPECT_TRUE(p::is_valid_secret_key("sync.refresh-token"));
}
