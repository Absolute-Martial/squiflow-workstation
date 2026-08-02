// The payload codec. Most of these tests feed it damaged input, because that
// is the input it will actually get: a half-written row on a machine that lost
// power, or a file a disk returned wrongly.

#include <string>
#include <vector>

#include "engine/records/payload.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace engine = squiflow::engine;

namespace {

engine::Row sample() {
    engine::Row row;
    row.set("name", engine::Value::text("Ram Printing Press"));
    row.set("rate_minor", engine::Value::integer(1250));
    row.set("quantity", engine::Value::integer(-3));
    row.set("note", engine::Value::null());
    row.set("approximate", engine::Value::real(1.5));
    row.set("signature", engine::Value::binary(engine::Blob{1, 2, 3, 250}));
    return row;
}

bool refuses(const engine::Blob& bytes, std::string& message) {
    try {
        engine::decode_payload(bytes);
    } catch (const engine::PayloadError& error) {
        message = error.what();
        return true;
    }
    return false;
}

}  // namespace

int main() {
    section("a row survives the round trip");
    {
        const engine::Row original = sample();
        const engine::Row returned = engine::decode_payload(engine::encode_payload(original));

        check(returned.size() == original.size(), "every field came back");
        check(returned.get("name").text_or({}) == "Ram Printing Press", "text");
        check(returned.get("rate_minor").integer_or(0) == 1250, "a positive integer");
        check(returned.get("quantity").integer_or(0) == -3, "and a negative one");
        check(returned.get("note").is_null(), "null stays null");
        check(returned.get("approximate").as_real().value_or(0.0) == 1.5, "real");
        const engine::Blob* signature = returned.get("signature").as_binary();
        check(signature != nullptr && signature->size() == 4 && (*signature)[3] == 250,
              "binary, including a byte above 127");

        check(returned.columns() == original.columns(), "in the order they were written");
    }

    section("the same row encodes to the same bytes");
    {
        check(engine::encode_payload(sample()) == engine::encode_payload(sample()),
              "encoding is deterministic");

        engine::Row reordered;
        reordered.set("b", engine::Value::integer(1));
        reordered.set("a", engine::Value::integer(2));
        engine::Row other;
        other.set("a", engine::Value::integer(2));
        other.set("b", engine::Value::integer(1));
        check(engine::encode_payload(reordered) != engine::encode_payload(other),
              "field order is part of the payload, not lost");
    }

    section("empty things are still things");
    {
        const engine::Row empty = engine::decode_payload(engine::encode_payload(engine::Row{}));
        check(empty.empty(), "a row with no fields round trips");

        engine::Row blanks;
        blanks.set("text", engine::Value::text(""));
        blanks.set("binary", engine::Value::binary(engine::Blob{}));
        const engine::Row back = engine::decode_payload(engine::encode_payload(blanks));
        check(back.has("text") && back.get("text").text_or("x").empty(),
              "an empty string is not the same as an absent field");
        check(back.has("binary"), "nor is an empty blob");
    }

    section("awkward text");
    {
        engine::Row row;
        // Written as bytes rather than as letters: the source stays ASCII so
        // that no editor, terminal or build machine can re-encode it on the
        // way past and turn a real test into an accidental one. These bytes
        // are the UTF-8 for a shop name in Devanagari.
        const std::string nepali =
            "\xe0\xa4\xb0\xe0\xa4\xbe\xe0\xa4\xae"
            " "
            "\xe0\xa4\xaa\xe0\xa5\x8d\xe0\xa4\xb0\xe0\xa4\xbf\xe0\xa4\xa8"
            "\xe0\xa5\x8d\xe0\xa4\x9f\xe0\xa4\xbf\xe0\xa4\x99"
            " "
            "\xe0\xa4\xaa\xe0\xa5\x8d\xe0\xa4\xb0\xe0\xa5\x87\xe0\xa4\xb8";
        row.set("nepali", engine::Value::text(nepali));
        row.set("embedded", engine::Value::text(std::string("before\0after", 12)));
        const engine::Row back = engine::decode_payload(engine::encode_payload(row));
        check(back.get("nepali").text_or({}) == nepali,
              "text that is not ASCII comes back byte for byte");
        check(back.get("embedded").text_or({}).size() == 12,
              "text is length-prefixed, so an embedded zero is data, not an ending");
    }

    section("damaged input is refused, not guessed at");
    {
        const engine::Blob good = engine::encode_payload(sample());
        std::string message;

        check(refuses(engine::Blob{}, message), "nothing at all");
        check(refuses(engine::Blob{'x', 'y', 'z', '1'}, message), "wrong magic");
        check(message.find("not a call payload") != std::string::npos, "and says so plainly");

        engine::Blob truncated(good.begin(), good.end() - 5);
        check(refuses(truncated, message), "truncated at the end");

        engine::Blob trailing = good;
        trailing.push_back(0);
        check(refuses(trailing, message), "one byte too many");
        check(message.find("after the last field") != std::string::npos,
              "trailing bytes are an error, not something to ignore");

        // A length field claiming four gigabytes. The machine has eight, and
        // the answer must be an error rather than an allocation.
        engine::Blob greedy;
        greedy.insert(greedy.end(), {'S', 'Q', 'F', '1'});
        greedy.insert(greedy.end(), {1, 0, 0, 0});        // one field
        greedy.insert(greedy.end(), {4, 0});              // name length 4
        greedy.insert(greedy.end(), {'n', 'a', 'm', 'e'});
        greedy.push_back(static_cast<unsigned char>(engine::ValueKind::Text));
        greedy.insert(greedy.end(), {0xFF, 0xFF, 0xFF, 0xFF});
        check(refuses(greedy, message), "a length larger than the payload");

        engine::Blob many_fields;
        many_fields.insert(many_fields.end(), {'S', 'Q', 'F', '1'});
        many_fields.insert(many_fields.end(), {0xFF, 0xFF, 0xFF, 0xFF});
        check(refuses(many_fields, message), "a field count larger than the payload");

        engine::Blob unknown_kind;
        unknown_kind.insert(unknown_kind.end(), {'S', 'Q', 'F', '1'});
        unknown_kind.insert(unknown_kind.end(), {1, 0, 0, 0});
        unknown_kind.insert(unknown_kind.end(), {1, 0});
        unknown_kind.push_back('a');
        unknown_kind.push_back(99);
        check(refuses(unknown_kind, message), "an unknown kind");

        engine::Blob nameless;
        nameless.insert(nameless.end(), {'S', 'Q', 'F', '1'});
        nameless.insert(nameless.end(), {1, 0, 0, 0});
        nameless.insert(nameless.end(), {0, 0});
        nameless.push_back(static_cast<unsigned char>(engine::ValueKind::Null));
        check(refuses(nameless, message), "a field with no name");

        engine::Blob twice;
        twice.insert(twice.end(), {'S', 'Q', 'F', '1'});
        twice.insert(twice.end(), {2, 0, 0, 0});
        for (int repeat = 0; repeat < 2; ++repeat) {
            twice.insert(twice.end(), {1, 0});
            twice.push_back('a');
            twice.push_back(static_cast<unsigned char>(engine::ValueKind::Null));
        }
        check(refuses(twice, message), "the same field named twice");
    }

    section("every byte pattern is safe to feed it");
    {
        // Not a fuzzer - that arrives in phase 9 - but enough to prove the
        // decoder answers rather than crashes for arbitrary input.
        int refused = 0;
        int accepted = 0;
        for (int seed = 0; seed < 512; ++seed) {
            engine::Blob bytes;
            unsigned state = static_cast<unsigned>(seed) * 2654435761U + 1U;
            const std::size_t length = static_cast<std::size_t>(seed % 37);
            for (std::size_t index = 0; index < length; ++index) {
                state = state * 1103515245U + 12345U;
                bytes.push_back(static_cast<unsigned char>((state >> 16) & 0xFFU));
            }
            std::string message;
            if (refuses(bytes, message)) {
                ++refused;
            } else {
                ++accepted;
            }
        }
        check(refused + accepted == 512, "nothing crashed and nothing hung");
        check(refused > 500, "and essentially all of it was refused");
    }

    section("the one-field convenience");
    {
        const engine::Row row = engine::decode_payload(engine::encode_field("name", "Sita"));
        check(row.size() == 1 && row.get("name").text_or({}) == "Sita", "one text field");
    }

    return squiflow::testing::report();
}
