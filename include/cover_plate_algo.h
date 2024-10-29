/**
 * @file cover_plate_algo.h
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

struct Cover_PlateAlgoConfig {
    std::set<std::string> include_labels{"uncover_plate"};// 多目标重叠标签
    std::set<std::string> exclude_labels;                 // 多目标重叠排除标签
    float cover_threshold{0.1};                           // 多目标重叠阈值
    std::string map_label{"cover_plate"};                       // 映射标签 (算法输出标签)

    float statistics_interval{1};   // 每隔N统计一次
    float statistics_threshold{0.1};// 统计阈值
};

class Cover_PlateAlgo {
public:
    Cover_PlateAlgo(const Cover_PlateAlgoConfig &config);
    ~Cover_PlateAlgo();

    /**
     * @brief 加载模型
     * 
     * @param models
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
    Cover_PlateAlgoConfig config_;

    class Cover_PlateAlgoPrivate;
    std::unique_ptr<Cover_PlateAlgoPrivate> private_;
};

}// namespace gddi