#include "weld_glove_algo.h"
#include "bytetrack/BYTETracker.h"
#include "sequence_statistic.h"
//#include "spdlog/spdlog.h"
#include "utils.h"
#include <api/global_config.h>
#include <bmcv_api_ext.h>
#include <common/type_convert.h>
#include <mutex>

namespace gddi {

class WeldGloveAlgo::WeldGloveAlgoPrivate {
public:
    std::unique_ptr<BYTETracker> tracker;
    std::unique_ptr<SequenceStatistic> sequence_statistic;

    std::mutex model_mutex;
    std::vector<ModelConfig> model_configs;
    std::vector<std::unique_ptr<gddeploy::InferAPI>> model_impls;
};

WeldGloveAlgo::WeldGloveAlgo(const WeldGloveAlgoConfig &config) : config_(config) {
    gddeploy::gddeploy_init("");
    private_ = std::make_unique<WeldGloveAlgoPrivate>();

    private_->tracker = std::make_unique<BYTETracker>(0.3, 0.6, 0.8, 30);
    private_->sequence_statistic =
        std::make_unique<SequenceStatistic>(config_.statistics_interval, config_.statistics_threshold);
}

WeldGloveAlgo::~WeldGloveAlgo() {
    for (auto &impl : private_->model_impls) { impl->WaitTaskDone(); }
}

bool WeldGloveAlgo::load_models(const std::vector<ModelConfig> &models) {
    std::lock_guard<std::mutex> lock(private_->model_mutex);
    private_->model_impls.clear();

    private_->model_configs = models;
    for (const auto &model : models) {
        auto algo_impl = std::make_unique<gddeploy::InferAPI>();
        if (algo_impl->Init("", model.path, model.license, gddeploy::ENUM_API_TYPE::ENUM_API_SESSION_API) != 0) {
            //spdlog::error("Failed to load model: {} - {}", model.name, model.path);
            return false;
        }
        private_->model_impls.emplace_back(std::move(algo_impl));
    }

    return true;
}

bool WeldGloveAlgo::sync_infer(const int64_t image_id, const cv::Mat &image,
                                 std::vector<AlgoObject> &statistic_objects) {
    gddeploy::BufSurfWrapperPtr surface;
    convertMat2BufSurface(const_cast<cv::Mat &>(image), surface);

    auto in_package = gddeploy::Package::Create(1);
    in_package->data[0]->Set(surface);

    auto out_package = gddeploy::Package::Create(1);
    if (private_->model_impls[0]->InferSync(in_package, out_package) != 0) { return false; }

    std::vector<AlgoObject> infer_objects;
    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
        infer_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                            private_->model_configs[0].labels, private_->model_configs[0].threshold);
    }

    // 二阶段检测
    if (!infer_objects.empty()) {
        auto in_package = gddeploy::Package::Create(1);
        in_package->data[0]->Set(surface);

        auto out_package = gddeploy::Package::Create(1);
        private_->model_impls[1]->InferSync(in_package, out_package);
        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
            infer_objects =
                filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                    private_->model_configs[1].labels, private_->model_configs[1].threshold);
        }

        // 生成目标跟踪ID
        std::vector<Object> objects;
        for (const auto &item : infer_objects) {
            Object temp;
                temp.class_id = item.class_id;
                temp.prob = item.score;
                temp.rect = {(float)item.rect.x, (float)item.rect.y, (float)item.rect.width, (float)item.rect.height};
                temp.label_name = item.label;
            objects.push_back(temp);
        }

        std::vector<AlgoObject> tracked_objects;
        for (auto &item : private_->tracker->update(objects)) {
            tracked_objects.emplace_back(AlgoObject{
                item.target_id, item.class_id, item.label_name, item.score,
                cv::Rect{(int)item.tlwh[0], (int)item.tlwh[1], (int)item.tlwh[2], (int)item.tlwh[3]}, item.track_id});
        }

        if (!tracked_objects.empty()) {
            // 裁剪目标 & 排序
            std::sort(tracked_objects.begin(), tracked_objects.end(),
                      [](const AlgoObject &item1, const AlgoObject &item2) {
                          return item1.score > item2.score
                              && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
                      });

            // 裁剪目标数
            if (tracked_objects.size() > private_->model_configs[2].max_crop_number) {
                tracked_objects.resize(private_->model_configs[2].max_crop_number);
            }

            std::vector<AlgoObject> match_objects;
            for (const auto &tracked_object : tracked_objects) {
                auto crop_rect = scale_crop_rect(image.cols, image.rows, tracked_object.rect,
                                                 private_->model_configs[2].crop_scale_factor);
                auto crop_image = image(crop_rect).clone();

                gddeploy::BufSurfWrapperPtr crop_surface;
                convertMat2BufSurface(crop_image, crop_surface);
                in_package = gddeploy::Package::Create(1);
                out_package = gddeploy::Package::Create(1);
                in_package->data[0]->Set(crop_surface);
                private_->model_impls[2]->InferSync(in_package, out_package);

                std::vector<AlgoObject> mask_objects;
                if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                    mask_objects =
                        parse_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),private_->model_configs[2].threshold);
                }

                if (mask_objects.empty()) { match_objects.emplace_back(tracked_object); }
            }

            statistic_objects = private_->sequence_statistic->update(match_objects);
        }
    }

    return true;
}

std::vector<AlgoObject> WeldGloveAlgo::parse_infer_result(const gddeploy::InferResult &infer_result,
                                                      const float threshold) {
    std::vector<AlgoObject> objects;

    for (auto result_type : infer_result.result_type) {
        if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
            for (const auto &item : infer_result.detect_result.detect_imgs) {
                int index = 1;
                for (auto &obj : item.detect_objs) {
                    if (obj.score < threshold || obj.label == "glove") { continue; }

                    objects.emplace_back(
                        AlgoObject{index++, obj.class_id, obj.label, obj.score,
                                   cv::Rect{(int)obj.bbox.x, (int)obj.bbox.y, (int)obj.bbox.w, (int)obj.bbox.h}});
                }
            }
        }
    }

    return objects;
}

std::vector<AlgoObject> WeldGloveAlgo::filter_infer_result(const gddeploy::InferResult &infer_result,
                                                             const std::set<std::string> &labels,
                                                             const float threshold) {
    std::vector<AlgoObject> objects;

    for (auto result_type : infer_result.result_type) {
        if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
            for (const auto &item : infer_result.detect_result.detect_imgs) {
                int index = 1;
                for (auto &obj : item.detect_objs) {
                    if (labels.count(obj.label) == 0 || obj.score < threshold) { continue; }

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