/**
 * @file smoke_algo.h
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

struct SmokeAlgoConfig {
    std::set<std::string> include_labels{"hand", "smoke"};// 多目标重叠标签
    std::set<std::string> exclude_labels;                 // 多目标重叠排除标签
    float cover_threshold{0.1};                           // 多目标重叠阈值
    std::string map_label{"smoke"};                       // 映射标签 (算法输出标签)

    float statistics_interval{1};   // 每隔N秒统计一次
    float statistics_threshold{0.5};// 统计阈值(手与香烟重叠时间占比)
};

class SmokeAlgo {
public:
    SmokeAlgo(const SmokeAlgoConfig &config);
    ~SmokeAlgo();

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
    std::vector<AlgoObject> parse_infer_result(const gddeploy::InferResult &infer_result);

private:
    SmokeAlgoConfig config_;

    class SmokeAlgoPrivate;
    std::unique_ptr<SmokeAlgoPrivate> private_;
};

}// namespace gddi