/**
 * @file sequence_statistic.hpp
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-17
 * 
 * @copyright Copyright (c) 2024 by GDDI
 * 
 */

#pragma one

#include "struct_def.h"
#include <ctime>
#include <map>

namespace gddi {

struct EventSqeuence {
    int last_group_status{0};
    std::vector<int> event_group;
    std::time_t last_event_time{std::time(nullptr)};
    std::time_t last_update_time{std::time(nullptr)};
};

class SequenceStatistic {

public:
    SequenceStatistic(const uint32_t interval = 3, const float threshold = 0.5)
        : interval_(interval), threshold_(threshold) {}
    virtual ~SequenceStatistic() = default;

    std::vector<AlgoObject> update(const std::vector<AlgoObject> &objects);

private:
    uint32_t interval_;
    float threshold_;
    std::map<int, EventSqeuence> event_map_;
};

}// namespace gddi