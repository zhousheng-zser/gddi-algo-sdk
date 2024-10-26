/**
 * @file light_glove_algo.h
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

struct LightGloveAlgoConfig {
    float statistics_interval{3};   // 每隔N统计一次
    float statistics_threshold{0.5};// 统计阈值(检测到灯亮并且未检测到防护镜时间占比)
};

class LightGloveAlgo {
public:
    LightGloveAlgo(const LightGloveAlgoConfig &config);
    ~LightGloveAlgo();

    /**
     * @brief 加载模型
     * 
     * @param models 行人+抽烟模型
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
    std::vector<AlgoObject> filter_infer_result(const gddeploy::InferResult &infer_result,
                                                const std::set<std::string> &labels, const float threshold);

private:
    LightGloveAlgoConfig config_;

    class LightGloveAlgoPrivate;
    std::unique_ptr<LightGloveAlgoPrivate> private_;
};

}// namespace gddi