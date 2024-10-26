/**
 * @file light_goggle_algo.h
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

struct LightGoggleAlgoConfig {
    float statistics_interval{3};   // 每隔N统计一次
    float statistics_threshold{0.5};// 统计阈值(检测到灯亮并且未检测到防护镜时间占比)
};

class LightGoggleAlgo {
public:
    LightGoggleAlgo(const LightGoggleAlgoConfig &config);
    ~LightGoggleAlgo();

    /**
     * @brief 加载模型
     * 
     * @param models 行人+抽烟模型
     * @return true 
     * @return false 
     */
    bool load_models(const std::vector<ModelConfig> &models);

    /**
     * @brief 异步推理接口
     * 
     * @param image_id 帧ID
     * @param image    图像
     * @param callback 回调
     */
    void async_infer(const int64_t image_id, const cv::Mat &image, InferCallback callback);

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
                                                const std::set<std::string> &labels);

private:
    LightGoggleAlgoConfig config_;

    class LightGoggleAlgoPrivate;
    std::unique_ptr<LightGoggleAlgoPrivate> private_;
};

}// namespace gddi