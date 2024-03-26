//
// Created by draco on 1/25/24.
//

#ifndef SMT_TRANSACTIONAL_CONSISTENCY_ARTIFACT_API_H
#define SMT_TRANSACTIONAL_CONSISTENCY_ARTIFACT_API_H

extern "C" {
    bool verify(const char *filepath, const char *log_level, bool pruning, const char *solver, const char *history_type, bool perf, const char *perf_path);
}

#endif //SMT_TRANSACTIONAL_CONSISTENCY_ARTIFACT_API_H
