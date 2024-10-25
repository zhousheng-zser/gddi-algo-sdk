/**
 * @file struct_def.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-17
 * 
 * @copyright Copyright (c) 2024 by GDDI
 * 
 */

#pragma once

#include <opencv2/core/mat.hpp>
#include <set>

namespace gddi {

struct ModelConfig {
    std::string name;            // 模型名称
    std::string path;            // 模型路径
    std::string license;         // 模型授权文件路径
    float threshold{0.3};        // 置信度阈值
    std::set<std::string> labels;// 保留标签

    // 以下为多阶段裁剪参数
    float crop_scale_factor{1.0f};// 输入目标框缩放系数
    uint32_t max_crop_number{8};  // 最多裁剪目标数 (默认按置信度+目标框面积排序)

    float nms_threshold{0.1f};// NMS阈值
};

struct AlgoObject {
    int target_id;
    int class_id;
    std::string label;
    float score;
    cv::Rect rect;
    int track_id;
};

using InferCallback = std::function<void(const int64_t, const cv::Mat &, const std::vector<AlgoObject> &)>;

}// namespace gddi