#include "safety_belt_algo.h"
#include "core/infer_server.h"
#include "spdlog/spdlog.h"
#include "utils.h"
#include <api/global_config.h>
#include <bmcv_api_ext.h>
#include <common/type_convert.h>
#include <core/alg_param.h>
#include <mutex>
#include <utility>

namespace gddi {

class SafetyBeltAlgo::SafetyBeltAlgoPrivate {
public:
    std::vector<int> light_group;
    std::time_t last_light_time{0};

    std::vector<std::pair<int, int>> safety_belt_group;

    std::mutex model_mutex;
    std::vector<ModelConfig> model_configs;
    std::vector<std::unique_ptr<gddeploy::InferAPI>> model_impls;
};

SafetyBeltAlgo::SafetyBeltAlgo(const SafetyBeltAlgoConfig &config) : config_(config) {
    gddeploy::gddeploy_init("");
    private_ = std::make_unique<SafetyBeltAlgoPrivate>();
}

SafetyBeltAlgo::~SafetyBeltAlgo() {
    std::lock_guard<std::mutex> lock(private_->model_mutex);
    for (auto &impl : private_->model_impls) { impl->WaitTaskDone(); }
}

bool SafetyBeltAlgo::load_models(const std::vector<ModelConfig> &models) {
    if (models.size() != 3) {
        spdlog::error("SafetyBeltAlgo requires exactly three models");
        return false;
    }

    std::lock_guard<std::mutex> lock(private_->model_mutex);
    private_->model_impls.clear();

    private_->model_configs = models;
    for (const auto &model : models) {
        auto algo_impl = std::make_unique<gddeploy::InferAPI>();
        if (algo_impl->Init("", model.path, model.license, gddeploy::ENUM_API_TYPE::ENUM_API_SESSION_API) != 0) {
            spdlog::error("Failed to load model: {} - {}", model.name, model.path);
            return false;
        }
        private_->model_impls.emplace_back(std::move(algo_impl));
    }

    return true;
}

void SafetyBeltAlgo::async_infer(const int64_t image_id, const cv::Mat &image, InferCallback infer_callback) {
    gddeploy::BufSurfWrapperPtr surface;
    convertMat2BufSurface(const_cast<cv::Mat &>(image), surface);

    auto package = gddeploy::Package::Create(1);
    package->data[0]->Set(surface);
    package->data[0]->SetAlgParam(
        gddeploy::AlgDetectParam{private_->model_configs[0].threshold, private_->model_configs[0].nms_threshold});

    private_->model_impls[0]->InferAsync(
        package,
        [this, image_id, image, surface, infer_callback](gddeploy::Status status, gddeploy::PackagePtr data,
                                                         gddeploy::any user_data) {
            std::vector<AlgoObject> person_objects;
            if (!data->data.empty() && data->data[0]->HasMetaValue()) {
                person_objects = filter_infer_result(data->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                     private_->model_configs[0].labels);
            }

            // 检测人数
            if (person_objects.size() < 2) {
                // 如果人数少于2，直接返回检测到的人员信息
                if (infer_callback) { infer_callback(image_id, image, person_objects); }
                return true;
            }

            // 对每个检测到的人进行安全带检测
            std::vector<AlgoObject> belt_objects;
            for (const auto &person : person_objects) {
                auto crop_rect =
                    scale_crop_rect(image.cols, image.rows, person.rect, private_->model_configs[1].crop_scale_factor);
                auto crop_image = image(crop_rect).clone();

                gddeploy::BufSurfWrapperPtr crop_surface;
                convertMat2BufSurface(crop_image, crop_surface);
                auto in_package = gddeploy::Package::Create(1);
                auto out_package = gddeploy::Package::Create(1);
                in_package->data[0]->Set(crop_surface);
                in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{private_->model_configs[1].threshold,
                                                                          private_->model_configs[1].nms_threshold});

                private_->model_impls[1]->InferSync(in_package, out_package);

                if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                    auto objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                       private_->model_configs[1].labels);
                    belt_objects.insert(belt_objects.end(), objects.begin(), objects.end());
                }
            }

            // 如果安全带统计小于阈值，则认为未戴安全带
            private_->safety_belt_group.emplace_back(belt_objects.empty() ? 0 : 1, std::time(nullptr));
            float safety_belt_count =
                std::count_if(private_->safety_belt_group.begin(), private_->safety_belt_group.end(),
                              [](const auto &pair) { return pair.first == 1; });
            if (safety_belt_count / private_->safety_belt_group.size() < config_.safety_belt_threshold) {
                if (infer_callback) { infer_callback(image_id, image, person_objects); }

                // 重置灯光统计
                private_->light_group.clear();
                private_->last_light_time = 0;
                return true;
            }

            if (std::time(nullptr) - private_->safety_belt_group.front().second >= config_.statistics_time) {
                private_->safety_belt_group.erase(private_->safety_belt_group.begin());
            }

            // 检测灯光
            if (private_->last_light_time == 0) { private_->last_light_time = std::time(nullptr); }

            auto in_package = gddeploy::Package::Create(1);
            in_package->data[0]->Set(surface);
            in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{private_->model_configs[2].threshold,
                                                                      private_->model_configs[2].nms_threshold});

            auto out_package = gddeploy::Package::Create(1);
            if (private_->model_impls[2]->InferSync(in_package, out_package) != 0) { return true; }

            if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                auto objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                   private_->model_configs[2].labels);
                private_->light_group.emplace_back(objects.empty() ? 0 : 1);
                if (std::time(nullptr) - private_->light_group.front() >= config_.light_threshold) {
                    private_->light_group.erase(private_->light_group.begin());
                }
            }

            // 灯光判断逻辑
            if (std::time(nullptr) - private_->last_light_time >= config_.delay_time) {
                if (!private_->light_group.empty()) {
                    float count = std::count(private_->light_group.begin(), private_->light_group.end(), 1);
                    if (count / private_->light_group.size() >= config_.light_threshold) {
                        // 如果灯亮了，返回空结果（表示条件都满足）
                        if (infer_callback) { infer_callback(image_id, image, {}); }
                    } else {
                        // 如果灯没亮，返回原始的人员检测结果
                        if (infer_callback) { infer_callback(image_id, image, person_objects); }
                    }
                } else {
                    if (infer_callback) { infer_callback(image_id, image, person_objects); }
                }

                // 重置灯光统计
                private_->light_group.clear();
                private_->last_light_time = 0;
            }

            return true;
        });
}

bool SafetyBeltAlgo::sync_infer(const int64_t image_id, const cv::Mat &image, std::vector<AlgoObject> &person_objects) {
    gddeploy::BufSurfWrapperPtr surface;
    convertMat2BufSurface(const_cast<cv::Mat &>(image), surface);

    auto in_package = gddeploy::Package::Create(1);
    in_package->data[0]->Set(surface);
    in_package->data[0]->SetAlgParam(
        gddeploy::AlgDetectParam{private_->model_configs[0].threshold, private_->model_configs[0].nms_threshold});

    auto out_package = gddeploy::Package::Create(1);
    if (private_->model_impls[0]->InferSync(in_package, out_package) != 0) { return false; }

    std::vector<AlgoObject> infer_objects;
    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
        infer_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                            private_->model_configs[0].labels);
    }

    // 检测人数
    if (infer_objects.size() < 2) {
        // 如果人数少于2，直接返回检测到的人员信息
        person_objects = infer_objects;
        return true;
    }

    // 对每个检测到的人进行安全带检测
    std::vector<AlgoObject> belt_objects;
    for (const auto &person : infer_objects) {
        auto crop_rect =
            scale_crop_rect(image.cols, image.rows, person.rect, private_->model_configs[1].crop_scale_factor);
        auto crop_image = image(crop_rect).clone();

        gddeploy::BufSurfWrapperPtr crop_surface;
        convertMat2BufSurface(crop_image, crop_surface);
        in_package = gddeploy::Package::Create(1);
        in_package->data[0]->Set(crop_surface);
        in_package->data[0]->SetAlgParam(
            gddeploy::AlgDetectParam{private_->model_configs[1].threshold, private_->model_configs[1].nms_threshold});

        out_package = gddeploy::Package::Create(1);
        if (private_->model_impls[1]->InferSync(in_package, out_package) != 0) { return false; }

        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
            auto objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                               private_->model_configs[1].labels);
            belt_objects.insert(belt_objects.end(), objects.begin(), objects.end());
        }
    }

    // 如果安全带统计小于阈值，则认为未戴安全带
    private_->safety_belt_group.emplace_back(belt_objects.empty() ? 0 : 1, std::time(nullptr));
    float safety_belt_count = std::count_if(private_->safety_belt_group.begin(), private_->safety_belt_group.end(),
                                            [](const auto &pair) { return pair.first == 1; });
    if (safety_belt_count / private_->safety_belt_group.size() < config_.safety_belt_threshold) {
        person_objects = infer_objects;

        // 重置灯光统计
        private_->light_group.clear();
        private_->last_light_time = 0;
        return true;
    }

    if (std::time(nullptr) - private_->safety_belt_group.front().second >= config_.statistics_time) {
        private_->safety_belt_group.erase(private_->safety_belt_group.begin());
    }

    // 检测灯光
    if (private_->last_light_time == 0) { private_->last_light_time = std::time(nullptr); }

    in_package = gddeploy::Package::Create(1);
    in_package->data[0]->Set(surface);
    in_package->data[0]->SetAlgParam(
        gddeploy::AlgDetectParam{private_->model_configs[2].threshold, private_->model_configs[2].nms_threshold});

    out_package = gddeploy::Package::Create(1);
    if (private_->model_impls[2]->InferSync(in_package, out_package) != 0) { return false; }

    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
        auto objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                           private_->model_configs[2].labels);
        private_->light_group.emplace_back(objects.empty() ? 0 : 1);
        if (std::time(nullptr) - private_->light_group.front() >= config_.light_threshold) {
            private_->light_group.erase(private_->light_group.begin());
        }
    }

    // 修改灯光判断逻辑
    if (std::time(nullptr) - private_->last_light_time >= config_.delay_time) {
        if (!private_->light_group.empty()) {
            float count = std::count(private_->light_group.begin(), private_->light_group.end(), 1);
            if (count / private_->light_group.size() >= config_.light_threshold) {
                // 如果灯亮了，返回空结果（表示条件都满足）
                person_objects = {};
            } else {
                // 如果灯没亮，返回原始的人员检测结果
                person_objects = infer_objects;
            }
        } else {
            person_objects = infer_objects;
        }

        // 重置灯光统计
        private_->light_group.clear();
        private_->last_light_time = 0;
    }

    return true;
}

std::vector<AlgoObject> SafetyBeltAlgo::filter_infer_result(const gddeploy::InferResult &infer_result,
                                                            const std::set<std::string> &labels) {
    std::vector<AlgoObject> objects;

    for (auto result_type : infer_result.result_type) {
        if (result_type == gddeploy::GDD_RESULT_TYPE_DETECT) {
            for (const auto &item : infer_result.detect_result.detect_imgs) {
                int index = 1;
                for (auto &obj : item.detect_objs) {
                    if (labels.count(obj.label) == 0) { continue; }

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
