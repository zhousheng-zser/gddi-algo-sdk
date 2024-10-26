#include "hoisting_operation_algo.h"
#include "spdlog/spdlog.h"
#include "utils.h"
#include <api/global_config.h>
#include <bmcv_api_ext.h>
#include <common/type_convert.h>
#include <core/alg_param.h>
#include <mutex>

namespace gddi {

class HoistingOperationAlgo::HoistingOperationAlgoPrivate {
public:
    std::mutex model_mutex;
    std::vector<ModelConfig> model_configs;
    std::vector<std::unique_ptr<gddeploy::InferAPI>> model_impls;
};

HoistingOperationAlgo::HoistingOperationAlgo(const HoistingOperationAlgoConfig &config) : config_(config) {
    gddeploy::gddeploy_init("");
    private_ = std::make_unique<HoistingOperationAlgoPrivate>();
}

HoistingOperationAlgo::~HoistingOperationAlgo() {
    std::lock_guard<std::mutex> lock(private_->model_mutex);
    for (auto &impl : private_->model_impls) { impl->WaitTaskDone(); }
}

bool HoistingOperationAlgo::load_models(const std::vector<ModelConfig> &models) {
    if (models.size() != 3) {
        spdlog::error("HoistingOperationAlgo requires exactly three models");
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

void HoistingOperationAlgo::async_infer(const int64_t image_id, const cv::Mat &image, InferCallback infer_callback) {
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
            std::vector<AlgoObject> infer_objects;
            if (!data->data.empty() && data->data[0]->HasMetaValue()) {
                infer_objects = filter_infer_result(data->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                    private_->model_configs[0].labels);
            }

            // 如果一阶段没有检测目标，直接返回
            if (infer_objects.empty() && infer_callback) {
                infer_callback(image_id, image, {});
            } else {
                auto in_package = gddeploy::Package::Create(1);
                in_package->data[0]->Set(surface);
                in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{private_->model_configs[1].threshold,
                                                                          private_->model_configs[1].nms_threshold});

                auto out_package = gddeploy::Package::Create(1);
                private_->model_impls[1]->InferSync(in_package, out_package);
                if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                    infer_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                        private_->model_configs[1].labels);
                }

                std::vector<AlgoObject> match_objects;
                if (!infer_objects.empty()) {
                    // 裁剪目标 & 排序
                    std::sort(infer_objects.begin(), infer_objects.end(),
                              [](const AlgoObject &item1, const AlgoObject &item2) {
                                  return item1.score > item2.score
                                      && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
                              });

                    // 裁剪目标数
                    if (infer_objects.size() > private_->model_configs[2].max_crop_number) {
                        infer_objects.resize(private_->model_configs[2].max_crop_number);
                    }

                    for (const auto &item : infer_objects) {
                        auto crop_rect = scale_crop_rect(image.cols, image.rows, item.rect,
                                                         private_->model_configs[2].crop_scale_factor);
                        auto crop_image = image(crop_rect).clone();

                        gddeploy::BufSurfWrapperPtr crop_surface;
                        convertMat2BufSurface(crop_image, crop_surface);
                        in_package = gddeploy::Package::Create(1);
                        out_package = gddeploy::Package::Create(1);
                        in_package->data[0]->Set(crop_surface);
                        in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{
                            private_->model_configs[2].threshold, private_->model_configs[2].nms_threshold});

                        private_->model_impls[2]->InferSync(in_package, out_package);

                        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                            auto objects =
                                filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                    private_->model_configs[2].labels);
                            for (auto &obj : objects) {
                                obj.rect.x += crop_rect.x;
                                obj.rect.y += crop_rect.y;
                                match_objects.emplace_back(obj);
                            }
                        }
                    }
                }

                if (infer_callback) { infer_callback(image_id, image, match_objects); }
            }
        });
}

bool HoistingOperationAlgo::sync_infer(const int64_t image_id, const cv::Mat &image,
                                       std::vector<AlgoObject> &match_objects) {
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

    // 二阶段检测
    if (!infer_objects.empty()) {
        in_package = gddeploy::Package::Create(1);
        in_package->data[0]->Set(surface);
        in_package->data[0]->SetAlgParam(
            gddeploy::AlgDetectParam{private_->model_configs[1].threshold, private_->model_configs[1].nms_threshold});

        out_package = gddeploy::Package::Create(1);
        private_->model_impls[1]->InferSync(in_package, out_package);
        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
            infer_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                private_->model_configs[1].labels);
        }

        if (!infer_objects.empty()) {
            // 裁剪目标 & 排序
            std::sort(infer_objects.begin(), infer_objects.end(), [](const AlgoObject &item1, const AlgoObject &item2) {
                return item1.score > item2.score
                    && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
            });

            // 裁剪目标数
            if (infer_objects.size() > private_->model_configs[2].max_crop_number) {
                infer_objects.resize(private_->model_configs[2].max_crop_number);
            }

            for (const auto &item : infer_objects) {
                auto crop_rect =
                    scale_crop_rect(image.cols, image.rows, item.rect, private_->model_configs[2].crop_scale_factor);
                auto crop_image = image(crop_rect).clone();

                gddeploy::BufSurfWrapperPtr crop_surface;
                convertMat2BufSurface(crop_image, crop_surface);
                in_package = gddeploy::Package::Create(1);
                out_package = gddeploy::Package::Create(1);
                in_package->data[0]->Set(crop_surface);
                in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{private_->model_configs[2].threshold,
                                                                          private_->model_configs[2].nms_threshold});

                private_->model_impls[2]->InferSync(in_package, out_package);

                if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                    auto objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                       private_->model_configs[2].labels);
                    for (auto &obj : objects) {
                        obj.rect.x += crop_rect.x;
                        obj.rect.y += crop_rect.y;
                        match_objects.emplace_back(obj);
                    }
                }
            }
        }
    }

    return true;
}

std::vector<AlgoObject> HoistingOperationAlgo::filter_infer_result(const gddeploy::InferResult &infer_result,
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
