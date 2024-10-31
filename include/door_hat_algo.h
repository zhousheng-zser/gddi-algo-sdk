/**
 * @file door_hat_algo.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-20
 * 
 * @copyright Copyright (c) 2024 by GDDI
 * 
 */

#pragma once

#include "struct_def.h"
#include <api/infer_api.h>
#include <core/result_def.h>

namespace gddi {

struct DoorHatAlgoConfig {
    float statistics_interval{3};   // 每隔N统计一次
    float statistics_threshold{0.5};// 统计阈值(检测到关们并且检测到防护帽时间占比)
};

class DoorHatAlgo {
public:
    DoorHatAlgo(const DoorHatAlgoConfig &config);
    ~DoorHatAlgo();

    /**
     * @brief 加载模型
     * 
     * @param models 大门+防护帽模型
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
    DoorHatAlgoConfig config_;

    class DoorHatAlgoPrivate;
    std::unique_ptr<DoorHatAlgoPrivate> private_;
};

}// namespace gddi