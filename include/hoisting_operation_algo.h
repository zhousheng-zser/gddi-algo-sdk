#pragma once

#include "struct_def.h"
#include <api/infer_api.h>
#include <core/result_def.h>
#include <opencv2/core/mat.hpp>
#include <set>
#include <vector>

namespace gddi {

struct HoistingOperationAlgoConfig {
    std::set<std::string> light_labels{"light"};             // 灯的标签
    std::set<std::string> hoisting_labels{"hoisting_object"};// 吊装物的标签
    std::set<std::string> person_labels{"person"};           // 人的标签
    float light_threshold{0.5};                              // 灯亮的阈值
    float hoisting_threshold{0.5};                           // 吊装物检测阈值
    float person_threshold{0.5};                             // 人员检测阈值

    float statistics_interval{1};   // 每隔N秒统计一次
    float statistics_threshold{0.5};// 统计阈值
};

class HoistingOperationAlgo {
public:
    HoistingOperationAlgo(const HoistingOperationAlgoConfig &config);
    ~HoistingOperationAlgo();

    bool load_models(const std::vector<ModelConfig> &models);

    void async_infer(const int64_t image_id, const cv::Mat &image, InferCallback infer_callback);
    bool sync_infer(const int64_t image_id, const cv::Mat &image, std::vector<AlgoObject> &objects);

protected:
    std::vector<AlgoObject> filter_infer_result(const gddeploy::InferResult &infer_result,
                                                const std::set<std::string> &labels);

private:
    HoistingOperationAlgoConfig config_;

    class HoistingOperationAlgoPrivate;
    std::unique_ptr<HoistingOperationAlgoPrivate> private_;
};

}// namespace gddi
