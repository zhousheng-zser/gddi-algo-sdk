/**
 * @file play_phone_algo.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-18
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

struct PlayPhoneAlgoConfig {
    std::set<std::string> include_labels{"hand", "phone"};// 多目标重叠标签
    std::set<std::string> exclude_labels{"head"};         // 多目标重叠排除标签
    float cover_threshold{0.1};                           // 多目标重叠阈值
    std::string map_label{"phone"};                       // 映射标签 (算法输出标签)

    float statistics_interval{3};    // 每隔N统计一次
    float statistics_threshold{0.5f};// 统计阈值(手与手机重叠时间占比)
};

class PlayPhoneAlgo {
public:
    PlayPhoneAlgo(const PlayPhoneAlgoConfig &config);
    ~PlayPhoneAlgo();

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
    std::vector<AlgoObject> parse_infer_result(const gddeploy::InferResult &infer_result, const float threshold);

private:
    PlayPhoneAlgoConfig config_;

    class PlayPhoneAlgoPrivate;
    std::unique_ptr<PlayPhoneAlgoPrivate> private_;
};

}// namespace gddi