#include "person_algo.h"
#include "bytetrack/BYTETracker.h"
#include "sequence_statistic.h"
//#include "spdlog/spdlog.h"
#include "utils.h"
#include <api/global_config.h>
#include <bmcv_api_ext.h>
#include <common/type_convert.h>
#include <mutex>

namespace gddi {

class PersonAlgo::PersonAlgoPrivate {
public:
    std::unique_ptr<BYTETracker> tracker;
    std::unique_ptr<SequenceStatistic> sequence_statistic;

    std::mutex model_mutex;
    std::vector<ModelConfig> model_configs;
    std::vector<std::unique_ptr<gddeploy::InferAPI>> model_impls;
};

PersonAlgo::PersonAlgo(const PersonAlgoConfig &config) : config_(config) {
    gddeploy::gddeploy_init("");
    private_ = std::make_unique<PersonAlgoPrivate>();

    private_->tracker = std::make_unique<BYTETracker>(0.3, 0.6, 0.8, 30);
    private_->sequence_statistic =
        std::make_unique<SequenceStatistic>(config_.statistics_interval, config_.statistics_threshold);
}

PersonAlgo::~PersonAlgo() {
    for (auto &impl : private_->model_impls) { impl->WaitTaskDone(); }
}

bool PersonAlgo::load_models(const std::vector<ModelConfig> &models) {
    std::lock_guard<std::mutex> lock(private_->model_mutex);
    private_->model_impls.clear();

    for (const auto &model : models) {
        private_->model_configs.push_back(model);
        auto algo_impl = std::make_unique<gddeploy::InferAPI>();
        if (algo_impl->Init("", model.path, model.license, gddeploy::ENUM_API_TYPE::ENUM_API_SESSION_API) != 0) {
            printf("Failed to load model: %s - %s", model.name.c_str(), model.path.c_str());
            return false;
        }
        private_->model_impls.emplace_back(std::move(algo_impl));
    }

    return true;
}

bool PersonAlgo::sync_infer(const int64_t image_id, const cv::Mat &image, std::vector<AlgoObject> &statistic_objects) {
    gddeploy::BufSurfWrapperPtr surface;
    convertMat2BufSurface(const_cast<cv::Mat &>(image), surface);

    auto in_package = gddeploy::Package::Create(1);
    in_package->data[0]->Set(surface);

    auto out_package = gddeploy::Package::Create(1);
    if (private_->model_impls[0]->InferSync(in_package, out_package) != 0) { return false; }

    std::vector<AlgoObject> infer_objects;
    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
        infer_objects = parse_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                           private_->model_configs[0].threshold);
    }
    for(auto &item : infer_objects)
    {
        if(item.label == "person")
        {
            statistic_objects.push_back(item);
        }
    }
    return true;
}

std::vector<AlgoObject> PersonAlgo::parse_infer_result(const gddeploy::InferResult &infer_result,
                                                      const float threshold) {
    std::vector<AlgoObject> objects;

    for (auto result_type : infer_result.result_type) {
        if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
            for (const auto &item : infer_result.detect_result.detect_imgs) {
                int index = 1;
                for (auto &obj : item.detect_objs) {
                    if (obj.score < threshold) { continue; }

                    objects.emplace_back(
                        AlgoObject{index++, obj.class_id, obj.label, obj.score,
                                   cv::Rect{(int)obj.bbox.x, (int)obj.bbox.y, (int)obj.bbox.w, (int)obj.bbox.h}});
                }
            }
        }
    }

    return objects;
}

}// namespace gddi