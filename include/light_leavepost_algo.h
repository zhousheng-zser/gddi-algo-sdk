/**
 * @file light_leavepost_algo.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-17
 * 
 * @copyright Copyright (c) 2024 by GDDI
 * 
 */

#pragma once

#include "struct_def.h"
#include <api/infer_api.h>
#include <core/result_def.h>
#include <set>

namespace gddi {

struct Light_LeavepostAlgoConfig {
    std::set<std::string> include_labels{"light", "person"};// 多目标重叠标签
    std::set<std::string> exclude_labels;                 // 多目标重叠排除标签
    float cover_threshold{0.1};                           // 多目标重叠阈值
    std::string map_label{"light_leavepost"};                       // 映射标签 (算法输出标签)

    float statistics_interval{1};   // 每隔N统计一次
    float statistics_threshold{0.1};// 统计阈值(手与香烟重叠时间占比)
};

class Light_LeavepostAlgo {
public:
    Light_LeavepostAlgo(const Light_LeavepostAlgoConfig &config);
    ~Light_LeavepostAlgo();

    /**
     * @brief 加载模型
     * 
     * @param models 行人+灯光模型
     * @return true 
     * @return false 
     */
    bool load_models(const std::vector<ModelConfig> &models);

    /**
     * @brief 同步推理接口
     * 
     * @param image_id 
     * @param image 
     * @param objects 
     * @return true 
     * @return false 
     */
    bool sync_infer(const int64_t image_id, const cv::Mat &image, std::vector<AlgoObject> &objects);

protected:
    std::vector<AlgoObject> parse_infer_result(const gddeploy::InferResult &infer_result, const float threshold);

private:
    Light_LeavepostAlgoConfig config_;

    class Light_LeavepostAlgoPrivate;
    std::unique_ptr<Light_LeavepostAlgoPrivate> private_;
};

}// namespace gddi