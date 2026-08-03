// Phase 6.2: the log a shop counter can live with.
//
// Three things decide whether this layer is trustworthy, and none of them are
// about writing a happy line to a file. Can it be forced to forge a second
// entry from one message? Can it be persuaded to leak a password into a file
// the shop later emails to support? And can it, on a machine nobody
// administers, quietly eat the disk the shop's database lives on?
//
// Every one of those is arranged here deliberately: newline injection, control
// characters, credential-looking fields, a locked file that cannot be renamed,
// a delete that is refused, a size that cannot be read, a volume that stops
// accepting bytes, and a budget so small that the only way to honour it is to
// throw the current file away. The standard-library storage is then exercised
// against a real temporary directory, because a fake that disagrees with a
// disk proves nothing.

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <fstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include "platform/local_log_storage.hpp"
#include "platform/log_formatter.hpp"
#include "platform/log_level_policy.hpp"
#include "platform/log_record.hpp"
#include "platform/log_throttle.hpp"
#include "platform/logger.hpp"
#include "platform/rotating_log_file.hpp"
#include "platform/testing/fake_log_storage.hpp"
#include "platform/testing/manual_log_clock.hpp"
#include "platform/testing/recording_log_sink.hpp"
#include "support/check.hpp"

namespace {

namespace platform = squiflow::platform;
using platform::CategoryLevelRule;
using platform::LogThrottle;
using platform::LogThrottlePolicy;
using platform::RepeatSummary;
using platform::ThrottleDecision;
using platform::kMaxThrottledEvents;
using platform::LevelConfigurationResult;
using platform::LogField;
using platform::LogLevelPolicy;
using platform::is_valid_log_category;
using platform::kMaxLogCategoryLength;
using platform::kMaxLogCategoryRules;
using platform::LogLevel;
using platform::LogRecord;
using platform::LogRotationPolicy;
using platform::Logger;
using platform::RotatingLogFile;
using platform::testing::FakeLogStorage;
using platform::testing::ManualLogClock;
using platform::testing::RecordingLogSink;
using squiflow::testing::check;
using squiflow::testing::section;

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

LogRecord record_at(std::int64_t milliseconds, LogLevel level,
                    const std::string& category, const std::string& message) {
    LogRecord record;
    record.level = level;
    record.category = category;
    record.message = message;
    record.timestamp_milliseconds = milliseconds;
    return record;
}

LogRotationPolicy small_policy(std::uint64_t file_bytes,
                               std::uint8_t generations,
                               std::uint64_t budget) {
    LogRotationPolicy policy;
    policy.max_file_bytes = file_bytes;
    policy.generations = generations;
    policy.total_budget_bytes = budget;
    return policy;
}

std::string filler(std::size_t length, char character) {
    return std::string(length, character);
}

void the_dispatcher_is_pinned() {
    section("the dispatcher");

    check(platform::logging_backend_version() == "spdlog 1.17.0",
          "the vendored dispatcher is the reviewed version");
}

void levels_are_few_and_unambiguous() {
    section("levels");

    check(std::string(platform::level_name(LogLevel::Debug)) == "DEBUG",
          "debug is named");
    check(std::string(platform::level_name(LogLevel::Info)) == "INFO ",
          "info is padded so columns line up");
    check(std::string(platform::level_name(LogLevel::Warning)) == "WARN ",
          "warning is padded");
    check(std::string(platform::level_name(LogLevel::Error)) == "ERROR",
          "error is named");
    check(std::string(platform::level_name(LogLevel::Fatal)) == "FATAL",
          "fatal is named");
    check(std::string(platform::level_name(static_cast<LogLevel>(200))) ==
              "?????",
          "a value from outside the enum does not read off the end");

    LogLevel parsed = LogLevel::Fatal;
    check(platform::parse_log_level("info", parsed) && parsed == LogLevel::Info,
          "lower case parses");
    check(platform::parse_log_level("INFO", parsed) && parsed == LogLevel::Info,
          "upper case parses");
    check(platform::parse_log_level("Warning", parsed) &&
              parsed == LogLevel::Warning,
          "mixed case parses");
    check(platform::parse_log_level("DeBuG", parsed) && parsed == LogLevel::Debug,
          "any case parses");

    parsed = LogLevel::Error;
    check(!platform::parse_log_level("", parsed), "empty is not a level");
    check(parsed == LogLevel::Error, "a failed parse leaves the value alone");
    check(!platform::parse_log_level("inf", parsed), "a prefix is not a level");
    check(!platform::parse_log_level("informational", parsed),
          "a longer word is not a level");
    check(!platform::parse_log_level("info ", parsed),
          "a trailing space is not a level");
    check(!platform::parse_log_level(" info", parsed),
          "a leading space is not a level");
    check(!platform::parse_log_level("warn", parsed),
          "an abbreviation is refused rather than guessed");
}

void timestamps_are_exact() {
    section("timestamps");

    check(platform::format_log_timestamp(0) == "1970-01-01T00:00:00.000Z",
          "the epoch");
    check(platform::format_log_timestamp(-1) == "1970-01-01T00:00:00.000Z",
          "a clock set before the epoch is clamped, never negative");
    check(platform::format_log_timestamp(5) == "1970-01-01T00:00:00.005Z",
          "milliseconds are padded to three digits");
    check(platform::format_log_timestamp(999) == "1970-01-01T00:00:00.999Z",
          "the last millisecond of a second");
    check(platform::format_log_timestamp(946684800000LL) ==
              "2000-01-01T00:00:00.000Z",
          "the century boundary");
    check(platform::format_log_timestamp(951782400000LL) ==
              "2000-02-29T00:00:00.000Z",
          "2000 was a leap year, unlike most century years");
    check(platform::format_log_timestamp(1583020800000LL) ==
              "2020-03-01T00:00:00.000Z",
          "the day after a leap day");
    check(platform::format_log_timestamp(1234567890123LL) ==
              "2009-02-13T23:31:30.123Z",
          "a value with every field non-zero");
    check(platform::format_log_timestamp(86399999LL) ==
              "1970-01-01T23:59:59.999Z",
          "the last instant of a day");
    check(platform::format_log_timestamp(86400000LL) ==
              "1970-01-02T00:00:00.000Z",
          "the first instant of the next day");
    check(platform::format_log_timestamp(1770000000000LL).size() == 24,
          "the format is fixed width");
}

void a_message_cannot_forge_a_second_entry() {
    section("escaping");

    check(platform::escape_log_value("plain") == "\"plain\"",
          "an ordinary value is quoted");
    check(platform::escape_log_value("") == "\"\"", "an empty value is quoted");
    check(platform::escape_log_value("a\nb") == "\"a\\nb\"",
          "a newline is escaped, so one record stays one line");
    check(platform::escape_log_value("a\r\nb") == "\"a\\r\\nb\"",
          "a Windows line ending is escaped as well");
    check(platform::escape_log_value("a\tb") == "\"a\\tb\"", "a tab is escaped");
    check(platform::escape_log_value("say \"no\"") == "\"say \\\"no\\\"\"",
          "a quote cannot close the value early");
    check(platform::escape_log_value("back\\slash") == "\"back\\\\slash\"",
          "a backslash is doubled");
    check(platform::escape_log_value(std::string("bell\x07")) ==
              "\"bell\\x07\"",
          "a control character becomes a visible escape");
    check(platform::escape_log_value(std::string("nul\0end", 7)) ==
              "\"nul\\x00end\"",
          "an embedded zero byte does not end the value");
    check(platform::escape_log_value(std::string("del\x7f")) == "\"del\\x7F\"",
          "delete is escaped");

    const std::string huge = filler(platform::kMaxLogFieldValueLength + 500, 'x');
    const std::string escaped = platform::escape_log_value(huge);
    check(escaped.size() < huge.size(), "an oversized value is cut down");
    check(contains(escaped, platform::kTruncationMarker),
          "truncation is visible rather than silent");

    const std::string injected =
        platform::escape_log_value("x\n2026-01-01T00:00:00.000Z FATAL forged");
    check(injected.find('\n') == std::string::npos,
          "a forged second line cannot survive escaping");
}

void credentials_never_reach_the_file() {
    section("redaction");

    check(platform::is_sensitive_field_name("password"), "password");
    check(platform::is_sensitive_field_name("Password"), "case does not help");
    check(platform::is_sensitive_field_name("user_password_hash"),
          "a credential inside a longer name");
    check(platform::is_sensitive_field_name("api_token"), "token");
    check(platform::is_sensitive_field_name("refreshTokenValue"),
          "camel case token");
    check(platform::is_sensitive_field_name("client_secret"), "secret");
    check(platform::is_sensitive_field_name("passphrase"), "passphrase");
    check(platform::is_sensitive_field_name("apiKey"), "api key");
    check(platform::is_sensitive_field_name("privateKey"), "private key");
    check(platform::is_sensitive_field_name("credential_blob"), "credential");
    check(platform::is_sensitive_field_name("key"), "a bare key field");
    check(platform::is_sensitive_field_name("aws_accesskey"), "access key");
    check(!platform::is_sensitive_field_name("invoice_key"),
          "an ordinary business key is not a credential");
    check(!platform::is_sensitive_field_name("keyboard_layout"),
          "a word containing key is not a credential");
    check(!platform::is_sensitive_field_name("customer"),
          "an ordinary field is left alone");

    LogRecord record = record_at(0, LogLevel::Info, "auth", "signed in");
    record.fields = {LogField{"user", "counter"},
                     LogField{"password", "hunter2"},
                     LogField{"api_token", "abc.def.ghi"}};
    const std::string line = platform::format_log_record(record);
    check(contains(line, "user=\"counter\""), "the harmless field survives");
    check(!contains(line, "hunter2"), "the password is not in the line");
    check(!contains(line, "abc.def.ghi"), "the token is not in the line");
    check(contains(line, std::string("password=\"") +
                             platform::kRedactedValue + "\""),
          "the field is still recorded, with its value replaced");
}

void a_line_has_a_fixed_shape() {
    section("the line");

    LogRecord record =
        record_at(1234567890123LL, LogLevel::Warning, "storage", "disk slow");
    record.fields = {LogField{"milliseconds", "812"}};
    const std::string line = platform::format_log_record(record);
    check(line ==
              "2009-02-13T23:31:30.123Z WARN  storage \"disk slow\" "
              "milliseconds=\"812\"",
          "timestamp, level, category, message, then fields");
    check(line.find('\n') == std::string::npos, "no line ending is added here");

    LogRecord empty = record_at(0, LogLevel::Info, "", "");
    const std::string empty_line = platform::format_log_record(empty);
    check(contains(empty_line, " general "),
          "a missing category becomes a real one rather than a gap");
    check(contains(empty_line, "\"\""), "an empty message is still quoted");

    LogRecord awkward =
        record_at(0, LogLevel::Info, "stor age=\"x\"", "named oddly");
    awkward.fields = {LogField{"a b=c", "value"}};
    const std::string awkward_line = platform::format_log_record(awkward);
    check(contains(awkward_line, " stor_age__x_ "),
          "a category cannot introduce a separator");
    check(contains(awkward_line, "a_b_c=\"value\""),
          "a field name cannot introduce a separator");

    LogRecord unnamed = record_at(0, LogLevel::Info, "x", "y");
    unnamed.fields = {LogField{"", "value"}};
    check(contains(platform::format_log_record(unnamed), "unnamed=\"value\""),
          "a nameless field is still attributable");

    LogRecord many = record_at(0, LogLevel::Info, "x", "y");
    for (std::size_t index = 0; index < platform::kMaxLogFieldCount + 7;
         ++index) {
        many.fields.push_back(LogField{"f" + std::to_string(index), "v"});
    }
    const std::string many_line = platform::format_log_record(many);
    check(contains(many_line, "fields_omitted=\"7\""),
          "dropped fields are counted, never silently lost");
    check(!contains(many_line, "f35="), "the surplus fields are not written");

    LogRecord enormous = record_at(0, LogLevel::Info, "x",
                                   filler(platform::kMaxLogMessageLength * 2, 'm'));
    for (std::size_t index = 0; index < platform::kMaxLogFieldCount; ++index) {
        enormous.fields.push_back(
            LogField{"f" + std::to_string(index),
                     filler(platform::kMaxLogFieldValueLength, 'v')});
    }
    const std::string enormous_line = platform::format_log_record(enormous);
    check(enormous_line.size() <=
              platform::kMaxLogLineLength + sizeof(platform::kTruncationMarker),
          "the whole line is bounded no matter what the caller passes");
    check(enormous_line.find('\n') == std::string::npos,
          "a bounded line is still one line");
}

void the_logger_is_a_door_that_never_throws() {
    section("the logger");

    RecordingLogSink sink;
    ManualLogClock clock(1000);
    Logger logger(sink, clock, LogLevel::Info);

    check(logger.minimum_level() == LogLevel::Info, "the threshold is set");
    check(!logger.is_enabled(LogLevel::Debug), "debug is off by default");
    check(logger.is_enabled(LogLevel::Info), "info is on");
    check(logger.is_enabled(LogLevel::Fatal), "fatal is always on");

    logger.debug("startup", "not interesting");
    check(sink.lines().empty(), "a suppressed line is not written");
    check(logger.counters().suppressed == 1, "but it is counted");

    logger.info("startup", "application started",
                {LogField{"version", "0.1.0"}});
    check(sink.lines().size() == 1, "an enabled line is written");
    check(contains(sink.lines().front(), "1970-01-01T00:00:01.000Z"),
          "the injected clock decides the timestamp");
    check(contains(sink.lines().front(), "version=\"0.1.0\""),
          "fields arrive intact");
    check(logger.counters().emitted == 1, "emitted is counted");

    clock.advance(2500);
    logger.warning("sync", "retrying");
    check(contains(sink.lines().back(), "1970-01-01T00:00:03.500Z"),
          "time moves with the clock");
    check(sink.flushes() == 0, "an ordinary line is not flushed on its own");

    logger.error("storage", "write failed");
    check(sink.flushes() == 1,
          "an error is flushed immediately, because it may be the last line");
    logger.fatal("storage", "cannot continue");
    check(sink.flushes() == 2, "so is a fatal");

    // A clock that jumps backwards must not disturb anything.
    clock.set(500);
    logger.info("clock", "time moved backwards");
    check(contains(sink.lines().back(), "1970-01-01T00:00:00.500Z"),
          "the log records what the clock said, without complaint");

    sink.set_accepting(false);
    logger.info("storage", "this will not land");
    check(logger.counters().sink_failures == 1,
          "a refused write is counted, not thrown");
    sink.set_accepting(true);

    logger.set_minimum_level(LogLevel::Debug);
    check(logger.is_enabled(LogLevel::Debug), "the threshold can be lowered");
    logger.debug("startup", "now interesting");
    check(contains(sink.lines().back(), "DEBUG"), "and debug then appears");

    logger.set_minimum_level(LogLevel::Fatal);
    logger.error("storage", "suppressed at this threshold");
    check(logger.counters().suppressed == 2,
          "raising the threshold suppresses even errors");

    const std::size_t before = sink.flushes();
    logger.flush();
    check(sink.flushes() == before + 1, "flush reaches the sink");
}

void a_bad_policy_is_corrected_rather_than_obeyed() {
    section("the rotation policy");

    const auto sane = platform::sanitise_rotation_policy(
        small_policy(1024 * 1024, 5, 8 * 1024 * 1024));
    check(!sane.adjusted, "a reasonable policy is left alone");
    check(sane.message.empty(), "and says nothing");

    const auto tiny = platform::sanitise_rotation_policy(small_policy(10, 5, 20));
    check(tiny.adjusted, "an unusable file size is corrected");
    check(tiny.policy.max_file_bytes == platform::kMinimumLogFileBytes,
          "raised to the smallest useful size");
    check(tiny.policy.total_budget_bytes >= tiny.policy.max_file_bytes * 2,
          "and the budget follows it up");
    check(!tiny.message.empty(), "the correction is explained");

    const auto vast = platform::sanitise_rotation_policy(
        small_policy(4ULL * 1024 * 1024 * 1024, 250,
                     100ULL * 1024 * 1024 * 1024));
    check(vast.policy.max_file_bytes == platform::kMaximumLogFileBytes,
          "an absurd file size is capped");
    check(vast.policy.generations == platform::kMaximumLogGenerations,
          "absurd generations are capped");
    check(vast.policy.total_budget_bytes == platform::kMaximumLogBudgetBytes,
          "an absurd budget is capped");

    const auto none = platform::sanitise_rotation_policy(
        small_policy(8192, 0, 1024 * 1024));
    check(none.policy.generations == 1,
          "keeping no generations would make rotation pointless");

    const auto cramped = platform::sanitise_rotation_policy(
        small_policy(8192, 3, 8192));
    check(cramped.policy.total_budget_bytes == 16384,
          "a budget that cannot hold two files is raised until it can");
}

void generations_are_numbered_not_dated() {
    section("generation names");

    check(platform::generation_file_name("squiflow.log", 1) == "squiflow.1.log",
          "the number goes before the extension");
    check(platform::generation_file_name("squiflow.log", 12) ==
              "squiflow.12.log",
          "two digits are fine");
    check(platform::generation_file_name("squiflow", 3) == "squiflow.3",
          "a name without an extension still works");
    check(platform::generation_file_name("my.app.log", 2) == "my.app.2.log",
          "only the last dot counts");
    check(platform::generation_file_name(".log", 2) == ".log.2",
          "a leading dot is not an extension separator");
}

void rotation_keeps_the_shape_of_the_family() {
    section("rotation");

    FakeLogStorage storage;
    RotatingLogFile file(storage, small_policy(4096, 3, 4096 * 8));
    check(file.policy().max_file_bytes == 4096, "the policy survives");

    // Each line costs 201 bytes with its line ending, so forty lines is more
    // than 4096 and rotation is not a matter of opinion.
    const std::string line = filler(200, 'a');
    for (int index = 0; index < 40; ++index) {
        check(file.write_line(line), "a line is accepted");
    }
    check(storage.exists("squiflow.log"), "the live file exists");
    check(storage.exists("squiflow.1.log"), "one generation was rotated out");
    check(file.counters().rotations >= 1, "rotation was counted");
    check(*storage.size_of("squiflow.log") <= 4096,
          "the live file never exceeds its limit");

    for (int index = 0; index < 200; ++index) {
        file.write_line(line);
    }
    check(!storage.exists("squiflow.4.log"),
          "nothing is kept beyond the configured generations");
    check(storage.exists("squiflow.3.log"), "the oldest kept generation exists");
    check(file.occupied_bytes() <= file.policy().total_budget_bytes,
          "the family stays inside the budget");

    // Rotation must not lose the newest content: the last line written is
    // still in the live file.
    file.write_line("the newest line");
    check(contains(storage.contents("squiflow.log"), "the newest line"),
          "the most recent line is where a reader looks first");
}

void the_budget_is_a_promise() {
    section("the hard cap");

    FakeLogStorage storage;
    RotatingLogFile file(storage, small_policy(4096, 5, 12288));

    for (int index = 0; index < 500; ++index) {
        file.write_line(filler(300, 'b'));
        check(file.occupied_bytes() <= 12288 + 4096,
              "the family never runs away between rotations");
    }
    check(file.occupied_bytes() <= 12288,
          "after settling, the family is inside the hard cap");
    check(file.counters().discarded_files > 0,
          "old files were actually deleted to honour it");
    check(storage.total_bytes() <= 12288,
          "and the disk agrees, not just the arithmetic");

    // Pre-existing rubbish from an older version of the application, sitting
    // in the same family, is cleaned up rather than inherited.
    FakeLogStorage crowded;
    crowded.put("squiflow.log", filler(9000, 'c'));
    crowded.put("squiflow.1.log", filler(9000, 'c'));
    crowded.put("squiflow.2.log", filler(9000, 'c'));
    RotatingLogFile second(crowded, small_policy(4096, 2, 8192));
    second.write_line("first line after an upgrade");
    check(second.occupied_bytes() <= 8192,
          "an oversized inheritance is brought inside the cap");
}

void one_line_can_never_be_bigger_than_one_file() {
    section("an impossible line");

    FakeLogStorage storage;
    RotatingLogFile file(storage, small_policy(4096, 2, 16384));
    check(file.write_line(filler(20000, 'z')),
          "a line larger than the whole file is still accepted");
    check(file.counters().truncated_lines == 1, "and reported as truncated");
    check(*storage.size_of("squiflow.log") <= 4096,
          "the file limit still holds");
    check(contains(storage.contents("squiflow.log"), "[truncated]"),
          "the reader can see that something was cut");
    check(storage.line_count("squiflow.log") == 1,
          "an oversized line does not become two lines");
}

void a_disk_that_misbehaves_never_stops_the_shop() {
    section("hostile storage");

    FakeLogStorage refusing;
    RotatingLogFile refusing_file(refusing, small_policy(4096, 2, 16384));
    refusing.set_accepting_writes(false);
    check(!refusing_file.write_line("nothing lands"),
          "a refused append is reported");
    check(refusing_file.counters().storage_failures == 1, "and counted");
    check(refusing_file.counters().lines_written == 0, "and not miscounted");

    FakeLogStorage full;
    RotatingLogFile full_file(full, small_policy(4096, 2, 16384));
    full.set_volume_capacity(1000);
    bool refused_eventually = false;
    for (int index = 0; index < 50; ++index) {
        if (!full_file.write_line(filler(100, 'd'))) {
            refused_eventually = true;
        }
    }
    check(refused_eventually, "a full volume is noticed rather than ignored");
    check(full_file.counters().storage_failures > 0, "and counted");

    FakeLogStorage locked;
    RotatingLogFile locked_file(locked, small_policy(4096, 2, 16384));
    for (int index = 0; index < 10; ++index) {
        locked_file.write_line(filler(300, 'e'));
    }
    locked.lock_file("squiflow.log");
    for (int index = 0; index < 30; ++index) {
        locked_file.write_line(filler(300, 'e'));
    }
    check(locked_file.counters().storage_failures > 0,
          "a file held open by something else is reported");
    check(locked.total_bytes() <= 16384 + 4096,
          "and the cap is still honoured, even at the cost of the live file");

    FakeLogStorage stubborn;
    RotatingLogFile stubborn_file(stubborn, small_policy(4096, 2, 12288));
    stubborn.make_undeletable("squiflow.2.log");
    for (int index = 0; index < 100; ++index) {
        stubborn_file.write_line(filler(300, 'f'));
    }
    check(stubborn_file.counters().storage_failures > 0,
          "a delete that is refused is reported rather than assumed");

    FakeLogStorage blind;
    RotatingLogFile blind_file(blind, small_policy(4096, 2, 16384));
    blind.put("squiflow.log", filler(100, 'g'));
    blind.make_unmeasurable("squiflow.log");
    blind_file.write_line("one more");
    check(blind_file.counters().rotations == 1,
          "a file whose size cannot be read is rotated rather than trusted");
}

void the_real_storage_agrees_with_the_fake() {
    section("storage on a real disk");

    check(!platform::is_plain_file_name(""), "an empty name is refused");
    check(!platform::is_plain_file_name("."), "a dot is refused");
    check(!platform::is_plain_file_name(".."), "a parent reference is refused");
    check(!platform::is_plain_file_name("../escape.log"),
          "traversal is refused rather than resolved");
    check(!platform::is_plain_file_name("sub/dir.log"), "a separator is refused");
    check(!platform::is_plain_file_name("sub\\dir.log"),
          "a Windows separator is refused");
    check(!platform::is_plain_file_name("C:log.log"), "a drive is refused");
    check(!platform::is_plain_file_name(std::string("bad\nname.log")),
          "a control character is refused");
    check(!platform::is_plain_file_name(filler(200, 'h')),
          "an over-long name is refused");
    check(platform::is_plain_file_name("squiflow.log"), "an ordinary name is fine");
    check(platform::is_plain_file_name("squiflow.12.log"),
          "a generation name is fine");

    std::error_code error;
    const std::filesystem::path root =
        std::filesystem::temp_directory_path(error) /
        ("squiflow-log-" +
         std::to_string(std::chrono::steady_clock::now()
                            .time_since_epoch()
                            .count()));
    check(!error, "a temporary directory is available");
    std::filesystem::create_directories(root, error);
    check(!error, "the test directory was created");

    platform::LocalLogStorage storage(root.string());
    check(!storage.exists("squiflow.log"), "nothing exists yet");
    check(!storage.size_of("squiflow.log").has_value(),
          "a missing file has no size, which is not the same as zero");
    check(storage.append("squiflow.log", "hello\n"), "the first append works");
    check(storage.exists("squiflow.log"), "the file now exists");
    check(storage.size_of("squiflow.log").value() == 6, "the size is real");
    check(storage.append("squiflow.log", "again\n"), "a second append works");
    check(storage.size_of("squiflow.log").value() == 12,
          "append never truncates");
    check(storage.rename("squiflow.log", "squiflow.1.log"), "rename works");
    check(!storage.exists("squiflow.log"), "the old name is gone");
    check(storage.exists("squiflow.1.log"), "the new name is there");
    check(storage.remove("squiflow.1.log"), "remove works");
    check(!storage.exists("squiflow.1.log"), "and the file is gone");
    check(storage.remove("squiflow.1.log"),
          "removing something already absent is success, not an error");
    check(!storage.append("../escape.log", "no"),
          "the storage cannot be talked into writing outside its directory");
    check(!std::filesystem::exists(root.parent_path() / "escape.log"),
          "and nothing appeared out there");
    storage.flush();

    platform::LocalLogStorage confined(root.string());
    RotatingLogFile file(confined, small_policy(4096, 3, 12288));
    ManualLogClock clock(1700000000000LL);
    Logger logger(file, clock, LogLevel::Info);
    for (int index = 0; index < 400; ++index) {
        clock.advance(1);
        logger.info("orders", "invoice issued",
                    {LogField{"number", std::to_string(index)},
                     LogField{"password", "must not appear"}});
    }
    logger.flush();

    std::uint64_t on_disk = 0;
    int family_files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(root, error)) {
        on_disk += static_cast<std::uint64_t>(
            std::filesystem::file_size(entry.path(), error));
        ++family_files;
    }
    check(!error, "the directory could be read back");
    check(on_disk <= 12288, "the hard cap holds on a real disk");
    check(family_files <= 4, "no more than the live file and its generations");

    std::ifstream live((root / "squiflow.log").string());
    std::string content((std::istreambuf_iterator<char>(live)),
                        std::istreambuf_iterator<char>());
    check(!content.empty(), "the live file has content");
    check(!contains(content, "must not appear"),
          "no credential reached the disk");
    check(contains(content, "invoice issued"), "the events are readable");

    std::filesystem::remove_all(root, error);
    check(!error, "the test directory was cleaned up");
}

void many_threads_produce_whole_lines() {
    section("concurrency");

    RecordingLogSink sink;
    ManualLogClock clock(1000);
    Logger logger(sink, clock, LogLevel::Info);

    constexpr int kThreads = 8;
    constexpr int kLinesPerThread = 250;
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int worker = 0; worker < kThreads; ++worker) {
        workers.emplace_back([&logger, worker]() {
            for (int index = 0; index < kLinesPerThread; ++index) {
                logger.info("worker", "tick",
                            {LogField{"worker", std::to_string(worker)},
                             LogField{"index", std::to_string(index)}});
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    check(sink.lines().size() ==
              static_cast<std::size_t>(kThreads * kLinesPerThread),
          "every line from every thread arrived exactly once");
    check(logger.counters().emitted ==
              static_cast<std::uint64_t>(kThreads * kLinesPerThread),
          "and the counter agrees");

    bool every_line_whole = true;
    for (const std::string& line : sink.lines()) {
        if (line.find("1970-01-01T00:00:01.000Z INFO  worker \"tick\" worker=") !=
                0 ||
            line.find('\n') != std::string::npos) {
            every_line_whole = false;
        }
    }
    check(every_line_whole, "no line was interleaved with another");

    std::vector<int> per_worker(kThreads, 0);
    for (const std::string& line : sink.lines()) {
        for (int worker = 0; worker < kThreads; ++worker) {
            if (contains(line, "worker=\"" + std::to_string(worker) + "\"")) {
                ++per_worker[static_cast<std::size_t>(worker)];
            }
        }
    }
    bool balanced = true;
    for (const int count : per_worker) {
        if (count != kLinesPerThread) {
            balanced = false;
        }
    }
    check(balanced, "each thread contributed exactly its own lines");
}

void a_concurrent_file_stays_inside_its_budget() {
    section("concurrent rotation");

    FakeLogStorage storage;
    RotatingLogFile file(storage, small_policy(4096, 3, 12288));
    ManualLogClock clock(0);
    Logger logger(file, clock, LogLevel::Info);

    std::vector<std::thread> workers;
    workers.reserve(4);
    for (int worker = 0; worker < 4; ++worker) {
        workers.emplace_back([&logger]() {
            for (int index = 0; index < 300; ++index) {
                logger.info("stress", "line",
                            {LogField{"payload", std::string(120, 'p')}});
            }
        });
    }
    for (std::thread& worker : workers) {
        worker.join();
    }

    check(storage.total_bytes() <= 12288,
          "rotation under contention still honours the cap");
    check(logger.counters().emitted == 1200, "no line was lost on the way in");
    check(file.counters().rotations > 0, "rotation actually happened");
}

void verbosity_is_chosen_per_category() {
    section("per-category levels");

    // A policy with no rules is exactly the old behaviour: one threshold.
    {
        LogLevelPolicy policy(LogLevel::Info);
        check(policy.default_level() == LogLevel::Info, "the default is the default");
        check(policy.rule_count() == 0, "a new policy carries no rules");
        check(policy.level_for("anything") == LogLevel::Info,
              "an unmentioned category takes the default");
        check(!policy.is_enabled("anything", LogLevel::Debug),
              "debug is off under an info default");
        check(policy.is_enabled("anything", LogLevel::Info),
              "info is on at the boundary");
        check(policy.is_enabled("anything", LogLevel::Fatal),
              "fatal is never filtered out by a lower default");
    }

    // The support case this exists for: one area turned up, everything else
    // left alone.
    {
        LogLevelPolicy policy(LogLevel::Warning);
        check(policy.set_category_level("sync", LogLevel::Debug),
              "a rule can be set");
        check(policy.rule_count() == 1, "one rule is one rule");
        check(policy.level_for("sync") == LogLevel::Debug,
              "the rule governs its own category");
        check(policy.level_for("storage") == LogLevel::Warning,
              "an untouched category keeps the default");
        check(policy.is_enabled("sync", LogLevel::Debug),
              "the raised category speaks at debug");
        check(!policy.is_enabled("storage", LogLevel::Info),
              "the rest of the application stays quiet");
    }

    // Dotted families, and the boundary that stops a rule leaking sideways.
    {
        LogLevelPolicy policy(LogLevel::Info);
        check(policy.set_category_level("storage", LogLevel::Error), "family rule set");
        check(policy.level_for("storage.migrate") == LogLevel::Error,
              "a family rule covers its children");
        check(policy.level_for("storage.migrate.step") == LogLevel::Error,
              "and its grandchildren");
        check(policy.level_for("storagecleanup") == LogLevel::Info,
              "a family rule stops at the dot, never mid-word");
        check(policy.level_for("storag") == LogLevel::Info,
              "a shorter name is not a prefix match");
        check(policy.level_for("other.storage") == LogLevel::Info,
              "a rule matches a prefix, not a substring");
    }

    // Longest wins, and the order rules arrive in must not matter.
    {
        LogLevelPolicy specific_first(LogLevel::Info);
        check(specific_first.set_category_level("storage.migrate", LogLevel::Debug),
              "child first");
        check(specific_first.set_category_level("storage", LogLevel::Error),
              "then parent");

        LogLevelPolicy family_first(LogLevel::Info);
        check(family_first.set_category_level("storage", LogLevel::Error),
              "parent first");
        check(family_first.set_category_level("storage.migrate", LogLevel::Debug),
              "then child");

        check(specific_first.level_for("storage.migrate") == LogLevel::Debug,
              "the specific rule wins when it was added first");
        check(family_first.level_for("storage.migrate") == LogLevel::Debug,
              "and also when it was added last");
        check(specific_first.level_for("storage.backup") == LogLevel::Error,
              "a sibling still takes the family rule");
        check(family_first.level_for("storage.backup") == LogLevel::Error,
              "in either order");
    }

    // Replacing a rule is an edit, not a second rule.
    {
        LogLevelPolicy policy(LogLevel::Info);
        check(policy.set_category_level("sync", LogLevel::Debug), "set once");
        check(policy.set_category_level("sync", LogLevel::Fatal), "set again");
        check(policy.rule_count() == 1, "the same category is one entry");
        check(policy.level_for("sync") == LogLevel::Fatal, "the later value wins");
        check(policy.clear_category_level("sync"), "a present rule is removed");
        check(!policy.clear_category_level("sync"),
              "removing it twice is refused, not repeated");
        check(policy.rule_count() == 0, "and the table is empty again");
        check(policy.level_for("sync") == LogLevel::Info,
              "the default governs once more");
    }

    // Hostile and merely careless category names.
    {
        check(is_valid_log_category("a"), "one letter is a category");
        check(is_valid_log_category("storage.migrate_2-b"),
              "dots, digits, underscore and dash");
        check(is_valid_log_category(std::string(kMaxLogCategoryLength, 'a')),
              "a category may be exactly as long as the cap");
        check(!is_valid_log_category(std::string(kMaxLogCategoryLength + 1, 'a')),
              "one character longer is refused");
        check(!is_valid_log_category(""), "empty is not a category");
        check(!is_valid_log_category(".sync"), "a leading dot is refused");
        check(!is_valid_log_category("sync."), "a trailing dot is refused");
        check(!is_valid_log_category("a..b"), "a doubled dot is refused");
        check(!is_valid_log_category("Sync"),
              "upper case is refused, so lookup needs no case rules");
        check(!is_valid_log_category("a b"), "a space is refused");
        check(!is_valid_log_category("a=b"), "the separator character is refused");
        check(!is_valid_log_category("a,b"), "the term separator is refused");
        check(!is_valid_log_category("a\"b"), "a quote is refused");
        check(!is_valid_log_category("a\nb"), "a newline is refused");
        check(!is_valid_log_category("caf\xc3\xa9"), "a non-ASCII byte is refused");

        LogLevelPolicy policy(LogLevel::Info);
        check(!policy.set_category_level("", LogLevel::Debug),
              "an empty rule is refused");
        check(!policy.set_category_level("Sync", LogLevel::Debug),
              "a mixed-case rule is refused");
        check(policy.rule_count() == 0, "a refused rule leaves nothing behind");
    }

    // The table is bounded, and being full must not corrupt what is there.
    {
        LogLevelPolicy policy(LogLevel::Info);
        for (std::size_t index = 0; index < kMaxLogCategoryRules; ++index) {
            const std::string name = "c" + std::to_string(index);
            check(policy.set_category_level(name, LogLevel::Debug),
                  "rules fit up to the cap");
        }
        check(policy.rule_count() == kMaxLogCategoryRules, "the table is full");
        check(!policy.set_category_level("one_too_many", LogLevel::Debug),
              "a new rule beyond the cap is refused");
        check(policy.rule_count() == kMaxLogCategoryRules, "and nothing was evicted");
        check(policy.set_category_level("c0", LogLevel::Fatal),
              "a full table can still be edited");
        check(policy.level_for("c0") == LogLevel::Fatal, "the edit took effect");
        policy.clear_all_category_levels();
        check(policy.rule_count() == 0, "clearing empties the table");
    }

    // Rules come back in a stable order whatever order they went in.
    {
        LogLevelPolicy policy(LogLevel::Info);
        check(policy.set_category_level("sync", LogLevel::Debug), "rule one");
        check(policy.set_category_level("backup", LogLevel::Error), "rule two");
        check(policy.set_category_level("storage", LogLevel::Warning), "rule three");
        const std::vector<CategoryLevelRule> ordered = policy.rules();
        check(ordered.size() == 3, "three rules come back");
        check(ordered[0].category == "backup", "sorted, first");
        check(ordered[1].category == "storage", "sorted, second");
        check(ordered[2].category == "sync", "sorted, third");
        check(ordered[0].level == LogLevel::Error, "with its level");
    }

    // A settings string a human typed.
    {
        LogLevelPolicy policy(LogLevel::Info);
        const LevelConfigurationResult result =
            policy.apply_configuration("warning, sync=debug, storage.migrate=error");
        check(result.applied == 3, "three terms applied");
        check(result.rejected == 0, "nothing rejected");
        check(result.fully_understood(), "and the string was understood in full");
        check(policy.default_level() == LogLevel::Warning,
              "the bare term set the default");
        check(policy.level_for("sync") == LogLevel::Debug, "the first rule applied");
        check(policy.level_for("storage.migrate") == LogLevel::Error,
              "the second rule applied");
        check(policy.level_for("storage") == LogLevel::Warning,
              "an unmentioned parent takes the default");
    }

    // Sloppy but well-meant input is accepted rather than argued with.
    {
        LogLevelPolicy policy(LogLevel::Info);
        const LevelConfigurationResult result =
            policy.apply_configuration("  SYNC = Debug ;; storage=WARNING,,");
        check(result.applied == 2,
              "whitespace, case and stray separators are tolerated");
        check(result.rejected == 0, "and none of it counts as an error");
        check(policy.level_for("sync") == LogLevel::Debug,
              "the category was lower-cased");
        check(policy.level_for("storage") == LogLevel::Warning,
              "the level name was case-insensitive");
    }

    // Nonsense is dropped, named, and does not take the good terms with it.
    {
        LogLevelPolicy policy(LogLevel::Info);
        const LevelConfigurationResult result = policy.apply_configuration(
            "sync=debug, storage=verbose, =warning, bad category=info, chatty");
        check(result.applied == 1, "only the sound term applied");
        check(result.rejected == 4, "four terms were refused");
        check(!result.fully_understood(), "and the result says so");
        check(result.rejected_terms.size() == 4, "each refusal is named");
        check(result.rejected_terms[0] == "storage=verbose", "an unknown level is named");
        check(result.rejected_terms[1] == "=warning", "an empty category is named");
        check(result.rejected_terms[2] == "bad category=info",
              "an illegal category is named");
        check(result.rejected_terms[3] == "chatty", "an unknown bare term is named");
        check(policy.level_for("sync") == LogLevel::Debug,
              "the good term survived the bad ones");
        check(policy.default_level() == LogLevel::Info,
              "and the default was left alone");
    }

    // Empty and absurd input.
    {
        LogLevelPolicy policy(LogLevel::Info);
        const LevelConfigurationResult empty = policy.apply_configuration("");
        check(empty.applied == 0 && empty.rejected == 0,
              "an empty string changes nothing");
        const LevelConfigurationResult blanks = policy.apply_configuration("  ,; ,  ");
        check(blanks.applied == 0 && blanks.rejected == 0,
              "separators alone change nothing");
        check(policy.default_level() == LogLevel::Info, "and the default is untouched");

        std::string enormous;
        for (int index = 0; index < 500; ++index) {
            enormous.append("x").append(std::to_string(index)).append("=debug,");
        }
        const LevelConfigurationResult flood = policy.apply_configuration(enormous);
        check(flood.rejected_terms.size() <= 130,
              "a flood of terms cannot make the parser allocate without limit");
        check(!flood.rejected_terms.empty() &&
                  flood.rejected_terms.back() == "[too many terms]",
              "and the parser says why it stopped");
        check(policy.rule_count() <= kMaxLogCategoryRules,
              "the rule table still respects its cap under flooding");

        const std::string long_term = std::string(300, 'z') + "=nonsense";
        LogLevelPolicy second(LogLevel::Info);
        const LevelConfigurationResult clipped = second.apply_configuration(long_term);
        check(clipped.rejected == 1, "an over-long term is one refusal");
        check(clipped.rejected_terms[0].size() <= 84,
              "and the refusal quoted back is clipped, not echoed whole");
    }

    // What is written into the support file must read back as the same policy.
    {
        LogLevelPolicy policy(LogLevel::Warning);
        check(policy.set_category_level("sync", LogLevel::Debug), "rule one");
        check(policy.set_category_level("storage.migrate", LogLevel::Fatal), "rule two");
        const std::string text = policy.to_configuration();
        check(text == "warning, storage.migrate=fatal, sync=debug",
              "the policy describes itself in the form it accepts");

        LogLevelPolicy restored(LogLevel::Debug);
        const LevelConfigurationResult result = restored.apply_configuration(text);
        check(result.fully_understood(), "its own output is understood in full");
        check(restored.default_level() == LogLevel::Warning, "the default round-trips");
        check(restored.level_for("sync") == LogLevel::Debug, "the first rule round-trips");
        check(restored.level_for("storage.migrate") == LogLevel::Fatal,
              "the second rule round-trips");
        check(restored.to_configuration() == text, "and a second pass is identical");
    }
}

void the_logger_obeys_the_policy() {
    section("the logger and per-category levels");

    // The plain case: one area turned up, the rest left alone.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);

        logger.debug("sync", "chatty");
        check(sink.lines().empty(), "debug is suppressed under an info default");
        check(logger.counters().suppressed == 1, "and the suppression is counted");

        check(logger.set_category_level("sync", LogLevel::Debug),
              "one category can be turned up");
        logger.debug("sync", "chatty");
        check(sink.lines().size() == 1, "and then it speaks");
        check(sink.contains("sync"), "the line names the category");

        logger.debug("storage", "chatty");
        check(sink.lines().size() == 1, "while the rest stays quiet");
        check(logger.counters().suppressed == 2, "the second suppression is counted too");
        check(logger.counters().emitted == 1, "exactly one record was emitted");

        check(logger.level_for("sync") == LogLevel::Debug, "the raised level is reported");
        check(logger.level_for("storage") == LogLevel::Info, "and so is the default");
        check(logger.is_enabled("sync", LogLevel::Debug), "the guard agrees for the rule");
        check(!logger.is_enabled("storage", LogLevel::Debug), "and for the default");
        check(logger.minimum_level() == LogLevel::Info, "the default level is unchanged");
        check(logger.is_enabled(LogLevel::Info), "the category-free guard uses the default");
        check(!logger.is_enabled(LogLevel::Debug), "in both directions");
    }

    // The trap. The dispatcher underneath holds a single threshold of its own.
    // If that threshold were left at the default, a category turned up to Debug
    // would be discarded before the policy was ever consulted, and the support
    // session would produce an empty file with nothing to explain it.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Error);

        check(logger.set_category_level("sync", LogLevel::Debug),
              "a category is raised far below the default");
        logger.debug("sync", "detail");
        check(sink.lines().size() == 1,
              "the raised category is not swallowed by the dispatcher threshold");

        logger.info("storage", "routine");
        check(sink.lines().size() == 1, "an unraised category is still filtered");
        logger.error("storage", "broken");
        check(sink.lines().size() == 2, "and still speaks at its own level");
        check(logger.counters().emitted == 2, "two records emitted");
        check(logger.counters().suppressed == 1, "one suppressed");

        // Lowering the raised category again must also lower the dispatcher,
        // or the filter would stay permanently open after one support session.
        check(logger.clear_category_level("sync"), "the rule is removed");
        logger.debug("sync", "detail");
        check(sink.lines().size() == 2, "and the category falls back to silence");
        check(logger.counters().suppressed == 2, "counted as suppressed, not lost");
    }

    // Families, and the sideways leak that must not happen.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);

        check(logger.set_category_level("storage", LogLevel::Debug), "a family is raised");
        logger.debug("storage.migrate", "step");
        check(sink.lines().size() == 1, "a child of the family speaks");
        logger.debug("storagecleanup", "step");
        check(sink.lines().size() == 1, "a similarly spelled stranger does not");
    }

    // A record with no category is filed as "general" by the formatter, so a
    // rule on "general" has to govern it. Anything else would be a rule that
    // silently does nothing.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);

        check(logger.level_for("") == LogLevel::Info, "an empty category takes the default");
        check(logger.set_category_level("general", LogLevel::Debug), "general is raised");
        check(logger.level_for("") == LogLevel::Debug,
              "and an empty category follows the rule the formatter will file it under");
        logger.debug("", "nameless");
        check(sink.lines().size() == 1, "so the nameless record is written");
        check(sink.contains("general"), "under the name the formatter gives it");
    }

    // Configuration arriving from settings, including the part of it that is
    // wrong. Nothing throws, the good terms apply, the bad ones are named.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);

        const LevelConfigurationResult result =
            logger.apply_level_configuration("warning, sync=debug, junk=nonsense");
        check(result.applied == 2, "two terms applied");
        check(result.rejected == 1, "one term refused");
        check(result.rejected_terms.size() == 1 &&
                  result.rejected_terms[0] == "junk=nonsense",
              "and the refusal is named so startup can log it");

        check(logger.minimum_level() == LogLevel::Warning, "the default moved");
        check(logger.level_configuration() == "warning, sync=debug",
              "the logger can state its own verbosity");

        logger.info("storage", "routine");
        check(sink.lines().empty(), "info is now below the default");
        logger.debug("sync", "detail");
        check(sink.lines().size() == 1, "and the configured category still speaks");

        check(!logger.set_category_level("Bad Name", LogLevel::Debug),
              "an illegal category is refused through the logger too");
        check(logger.level_configuration() == "warning, sync=debug",
              "and the refusal left the policy untouched");

        logger.clear_all_category_levels();
        check(logger.level_configuration() == "warning", "clearing leaves only the default");
        logger.debug("sync", "detail");
        check(sink.lines().size() == 1, "and the raised category is quiet again");
    }

    // Turning a category up must not disturb what the logger already promised:
    // errors still flush immediately, and a refusing sink is still counted
    // rather than thrown.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);
        check(logger.set_category_level("sync", LogLevel::Debug), "raise a category");

        logger.error("sync", "failed");
        check(sink.flushes() >= 1, "an error still flushes at once");

        sink.set_accepting(false);
        logger.debug("sync", "detail");
        check(logger.counters().sink_failures == 1, "a refused write is counted");
        sink.set_accepting(true);
        logger.debug("sync", "detail");
        check(logger.counters().sink_failures == 1, "and recovery is not counted as failure");
    }
}

void repetition_is_held_back() {
    section("the throttle");

    // Off by default, and remembering nothing while off. A feature nobody
    // switched on must not quietly accumulate state for every message the
    // application has ever written.
    {
        LogThrottle throttle;
        check(!throttle.policy().engaged(), "throttling is off by default");
        for (int index = 0; index < 100; ++index) {
            const ThrottleDecision decision =
                throttle.consider(LogLevel::Info, "sync", "same thing", 0);
            check(decision.emit, "every record passes while it is off");
        }
        check(throttle.watched_event_count() == 0, "and nothing is remembered");
        check(throttle.counters().held_back == 0, "nothing was held back");
    }

    // The interval rule.
    {
        LogThrottle throttle(LogThrottlePolicy{1000, 0});
        check(throttle.policy().engaged(), "an interval engages the throttle");

        const ThrottleDecision first =
            throttle.consider(LogLevel::Error, "sync", "refused", 0);
        check(first.emit, "the first sighting is always written");
        check(first.suppressed_since_last == 0, "and stands for itself alone");

        check(!throttle.consider(LogLevel::Error, "sync", "refused", 500).emit,
              "a repeat inside the interval is held");
        check(!throttle.consider(LogLevel::Error, "sync", "refused", 999).emit,
              "and so is one at the last instant of it");

        const ThrottleDecision released =
            throttle.consider(LogLevel::Error, "sync", "refused", 1000);
        check(released.emit, "the interval ends inclusively");
        check(released.suppressed_since_last == 2,
              "and the released record accounts for both held records");
        check(throttle.counters().held_back == 2, "the counter agrees");

        check(!throttle.consider(LogLevel::Error, "sync", "refused", 1001).emit,
              "the interval restarts from the record written");
    }

    // The count rule, which exists so that a fast loop still reports progress
    // even when the interval would keep it silent for a long time.
    {
        LogThrottle throttle(LogThrottlePolicy{10000, 3});
        check(throttle.consider(LogLevel::Info, "sync", "attempt", 0).emit,
              "the first is written");
        check(!throttle.consider(LogLevel::Info, "sync", "attempt", 0).emit,
              "the second is held");
        check(!throttle.consider(LogLevel::Info, "sync", "attempt", 0).emit,
              "the third is held");
        const ThrottleDecision third =
            throttle.consider(LogLevel::Info, "sync", "attempt", 0);
        check(third.emit,
              "every third occurrence since the last written one speaks");
        check(third.suppressed_since_last == 2, "carrying the two it stands for");
    }

    // A count of one would mean "every occurrence", which is the same as no
    // rule; it must not be mistaken for "hold everything".
    {
        LogThrottle throttle(LogThrottlePolicy{0, 1});
        check(!throttle.policy().engaged(), "a count of one is not a rule");
        for (int index = 0; index < 10; ++index) {
            check(throttle.consider(LogLevel::Info, "sync", "attempt", 0).emit,
                  "so everything is written");
        }
    }

    // A negative interval is nonsense arriving from a settings file. It must
    // not switch throttling on, and above all must not silence the log.
    {
        LogThrottle throttle(LogThrottlePolicy{-5000, 0});
        check(!throttle.policy().engaged(), "a negative interval is not a rule");
        check(throttle.policy().minimum_interval_milliseconds == 0,
              "and is normalised away at the door");
        check(throttle.consider(LogLevel::Info, "sync", "attempt", 0).emit,
              "nothing is silenced by it");
    }

    // Fatal is the last thing the application ever says. It is never held.
    {
        LogThrottle throttle(LogThrottlePolicy{60000, 0});
        for (int index = 0; index < 5; ++index) {
            check(throttle.consider(LogLevel::Fatal, "boot", "dying", 0).emit,
                  "fatal is never held back");
        }
        check(throttle.watched_event_count() == 0, "and is not even remembered");
    }

    // Identity is level, category and message together. Anything else would
    // fold together records that mean different things.
    {
        LogThrottle throttle(LogThrottlePolicy{1000, 0});
        check(throttle.consider(LogLevel::Info, "sync", "same", 0).emit, "first");
        check(throttle.consider(LogLevel::Warning, "sync", "same", 0).emit,
              "a different level is a different event");
        check(throttle.consider(LogLevel::Info, "storage", "same", 0).emit,
              "a different category is a different event");
        check(throttle.consider(LogLevel::Info, "sync", "other", 0).emit,
              "a different message is a different event");
        check(throttle.watched_event_count() == 4, "four events are watched");
        check(!throttle.consider(LogLevel::Info, "sync", "same", 1).emit,
              "and each throttles only itself");
    }

    // The shop machine syncs its clock and the time jumps backwards. Being a
    // little too talkative afterwards is a nuisance; being silent until the
    // clock catches up could cost an afternoon of evidence.
    {
        LogThrottle throttle(LogThrottlePolicy{1000, 0});
        check(throttle.consider(LogLevel::Error, "sync", "refused", 10000).emit,
              "written at the later time");
        check(throttle.consider(LogLevel::Error, "sync", "refused", 5000).emit,
              "a backwards clock releases rather than silences");
    }

    // The table is bounded. A program logging endlessly varied messages must
    // not turn the throttle into a record of every line it ever wrote.
    {
        LogThrottle throttle(LogThrottlePolicy{60000, 0});
        check(throttle.consider(LogLevel::Warning, "sync", "oldest", 0).emit,
              "the event that will be evicted is written once");
        check(!throttle.consider(LogLevel::Warning, "sync", "oldest", 1).emit,
              "and then owes one held record");

        std::optional<RepeatSummary> evicted;
        for (std::size_t index = 0; index < kMaxThrottledEvents; ++index) {
            const ThrottleDecision decision = throttle.consider(
                LogLevel::Warning, "sync", "distinct " + std::to_string(index),
                static_cast<std::int64_t>(10 + index));
            check(decision.emit, "each new event is written");
            if (decision.evicted.has_value()) {
                check(!evicted.has_value(), "only one event is evicted");
                evicted = decision.evicted;
            }
        }

        check(throttle.watched_event_count() == kMaxThrottledEvents,
              "the table never exceeds its bound");
        check(evicted.has_value(), "the quietest event was pushed out");
        check(evicted.has_value() && evicted->message == "oldest",
              "and it was the one that had been quiet longest");
        check(evicted.has_value() && evicted->suppressed == 1,
              "its held record is handed back rather than lost");
        check(throttle.counters().evictions == 1, "one eviction is counted");
    }

    // Draining settles every outstanding debt exactly once.
    {
        LogThrottle throttle(LogThrottlePolicy{60000, 0});
        throttle.consider(LogLevel::Error, "sync", "refused", 0);
        throttle.consider(LogLevel::Error, "sync", "refused", 1);
        throttle.consider(LogLevel::Error, "sync", "refused", 2);
        throttle.consider(LogLevel::Info, "jobs", "queued", 3);

        const std::vector<RepeatSummary> owed = throttle.drain();
        check(owed.size() == 1, "only events with something outstanding report");
        check(owed.size() == 1 && owed[0].suppressed == 2, "with the right count");
        check(owed.size() == 1 && owed[0].level == LogLevel::Error,
              "at the level the held records had");

        check(throttle.drain().empty(), "a second drain owes nothing");
    }
}

void the_logger_holds_back_repetition() {
    section("the logger and repetition");

    // The flood this was built for: a retry loop writing the same error faster
    // than anyone could read it.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);
        logger.set_throttle_policy(LogThrottlePolicy{1000, 0});
        check(logger.throttle_policy().minimum_interval_milliseconds == 1000,
              "the policy is readable back");

        logger.warning("sync", "connection refused");
        check(sink.lines().size() == 1, "the first report is written");

        for (int index = 0; index < 500; ++index) {
            logger.warning("sync", "connection refused");
        }
        check(sink.lines().size() == 1, "five hundred repeats add nothing");
        check(logger.counters().rate_limited == 500,
              "and all of them are counted");

        clock.advance(1000);
        logger.warning("sync", "connection refused");
        check(sink.lines().size() == 2, "the interval releases one line");
        check(sink.lines()[1].find("repeated=\"500\"") != std::string::npos,
              "which states how many it stands for");
    }

    // A gap must never outlive the run that caused it.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);
        logger.set_throttle_policy(LogThrottlePolicy{60000, 0});

        logger.error("sync", "refused");
        logger.error("sync", "refused");
        logger.error("sync", "refused");
        check(sink.lines().size() == 1, "two repeats are held");

        logger.flush();
        check(sink.lines().size() == 2, "flushing reports what is owed");
        check(sink.lines()[1].find("throttled=\"summary\"") != std::string::npos,
              "marked as a summary rather than a fresh occurrence");
        check(sink.lines()[1].find("repeated=\"2\"") != std::string::npos,
              "with the count it is accounting for");

        logger.flush();
        check(sink.lines().size() == 2, "a second flush owes nothing");
    }

    // Shutdown is the last chance to be honest about the gap.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        {
            Logger logger(sink, clock, LogLevel::Info);
            logger.set_throttle_policy(LogThrottlePolicy{60000, 0});
            logger.warning("sync", "refused");
            logger.warning("sync", "refused");
            check(sink.lines().size() == 1, "one held back while running");
        }
        check(sink.lines().size() == 2, "and reported as the logger shuts down");
        check(sink.lines()[1].find("repeated=\"1\"") != std::string::npos,
              "with the outstanding count");
    }

    // Changing the policy must not carry a debt across into a rule it was
    // never measured under.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);
        logger.set_throttle_policy(LogThrottlePolicy{60000, 0});
        logger.warning("sync", "refused");
        logger.warning("sync", "refused");

        logger.set_throttle_policy(LogThrottlePolicy{});
        check(sink.lines().size() == 2, "the debt is settled at the change");
        check(!logger.throttle_policy().engaged(), "and throttling is now off");

        logger.warning("sync", "refused");
        logger.warning("sync", "refused");
        check(sink.lines().size() == 4, "after which nothing is held");
    }

    // Throttling must not interfere with what the logger already promised.
    {
        RecordingLogSink sink;
        ManualLogClock clock(1000);
        Logger logger(sink, clock, LogLevel::Info);
        logger.set_throttle_policy(LogThrottlePolicy{60000, 0});

        logger.fatal("boot", "cannot continue");
        logger.fatal("boot", "cannot continue");
        check(sink.lines().size() == 2, "fatal is never held back");

        logger.debug("sync", "detail");
        check(logger.counters().suppressed == 1, "the level filter still counts");
        check(logger.counters().rate_limited == 0,
              "and a level-filtered record never reaches the throttle");
    }
}

}  // namespace

int main() {
    the_dispatcher_is_pinned();
    verbosity_is_chosen_per_category();
    the_logger_obeys_the_policy();
    repetition_is_held_back();
    the_logger_holds_back_repetition();
    levels_are_few_and_unambiguous();
    timestamps_are_exact();
    a_message_cannot_forge_a_second_entry();
    credentials_never_reach_the_file();
    a_line_has_a_fixed_shape();
    the_logger_is_a_door_that_never_throws();
    a_bad_policy_is_corrected_rather_than_obeyed();
    generations_are_numbered_not_dated();
    rotation_keeps_the_shape_of_the_family();
    the_budget_is_a_promise();
    one_line_can_never_be_bigger_than_one_file();
    a_disk_that_misbehaves_never_stops_the_shop();
    the_real_storage_agrees_with_the_fake();
    many_threads_produce_whole_lines();
    a_concurrent_file_stays_inside_its_budget();
    return squiflow::testing::report();
}
