#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/jobs/data/tables.hpp"
#include "modules/jobs/domain/job.hpp"

namespace squiflow::modules::jobs::data {

template <typename Reader>
std::optional<Job> find_job(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kJob, id);
    return row ? std::optional<Job>{job_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<Job> job_for_order_line(const Reader& reader,
                                      const std::string& order_id,
                                      const std::string& order_line_id) {
    engine::Query query{tables::kJob};
    query.where_equals("source_order_id", engine::Value::text(order_id));
    query.where_equals("source_order_line_id", engine::Value::text(order_line_id));
    query.order_by("id");
    for (const engine::Row& row : reader.select(query)) {
        return job_from_row(row);
    }
    return std::nullopt;
}

template <typename Reader>
std::vector<Job> jobs_for_party(const Reader& reader, const std::string& party_id) {
    engine::Query query{tables::kJob};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("created_at");
    query.order_by("id");
    std::vector<Job> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(job_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<Job> active_work_list(const Reader& reader) {
    std::vector<Job> result;
    for (const int state : {1, 2}) {
        engine::Query query{tables::kJob};
        query.where_equals("state", engine::Value::integer(state));
        query.order_by("deadline_at");
        query.order_by("started_at");
        query.order_by("created_at");
        query.order_by("id");
        for (const engine::Row& row : reader.select(query)) {
            result.push_back(job_from_row(row));
        }
    }
    return result;
}

void save_job(engine::Transaction& transaction, const Job& job);

}  // namespace squiflow::modules::jobs::data
