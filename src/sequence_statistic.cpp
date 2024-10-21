#include "sequence_statistic.h"

namespace gddi {

std::vector<AlgoObject> SequenceStatistic::update(const std::vector<AlgoObject> &objects) {
    for (auto &item : objects) {
        if (event_map_.count(item.track_id) == 0) { event_map_[item.track_id] = EventSqeuence{}; }

        event_map_.at(item.track_id).event_group.emplace_back(1);
        event_map_.at(item.track_id).last_update_time = std::time(nullptr);
    }

    // 处理事件
    std::vector<AlgoObject> update_objects;
    for (auto iter = event_map_.begin(); iter != event_map_.end();) {
        auto find_iter = std::find_if(objects.begin(), objects.end(),
                                      [iter](const auto &item) { return item.track_id == iter->first; });
        if (find_iter == objects.end()) {
            iter->second.event_group.push_back(0);
        } else if (time(nullptr) - iter->second.last_event_time >= interval_) {
            iter->second.last_event_time = std::time(nullptr);

            int new_group_status = 0;
            float count = std::count(iter->second.event_group.begin(), iter->second.event_group.end(), 1);
            if (count / iter->second.event_group.size() >= threshold_) { new_group_status = 1; }

            if (iter->second.last_group_status == 0 && new_group_status == 1) {
                update_objects.emplace_back(*find_iter);
            }

            iter->second.last_group_status = new_group_status;
            iter->second.event_group.clear();
        }

        // 删除超过两倍间隔时间未更新的目标
        if (std::time(nullptr) - iter->second.last_update_time > interval_ * 2) {
            iter = event_map_.erase(iter);
        } else {
            ++iter;
        }
    }

    return update_objects;
}

}// namespace gddi