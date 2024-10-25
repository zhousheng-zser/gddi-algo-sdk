#include "sparks_cover_algo.h"
#include "bytetrack/BYTETracker.h"
#include "sequence_statistic.h"
#include "spdlog/spdlog.h"
#include "utils.h"
#include <api/global_config.h>
#include <bmcv_api_ext.h>
#include <common/type_convert.h>
#include <core/alg_param.h>
#include <mutex>

namespace gddi {

class SparksCoverAlgo::SparksCoverAlgoPrivate {
public:
    std::unique_ptr<BYTETracker> tracker;
    std::unique_ptr<SequenceStatistic> sequence_statistic;

    std::mutex model_mutex;
    std::vector<ModelConfig> model_configs;
    std::vector<std::unique_ptr<gddeploy::InferAPI>> model_impls;
};

SparksCoverAlgo::SparksCoverAlgo(const SparksCoverAlgoConfig &config) : config_(config) {
    gddeploy::gddeploy_init("");
    private_ = std::make_unique<SparksCoverAlgoPrivate>();

    private_->tracker = std::make_unique<BYTETracker>(0.3, 0.6, 0.8, 30);
    private_->sequence_statistic =
        std::make_unique<SequenceStatistic>(config_.statistics_interval, config_.statistics_threshold);
}

SparksCoverAlgo::~SparksCoverAlgo() {
    std::lock_guard<std::mutex> lock(private_->model_mutex);
    for (auto &impl : private_->model_impls) { impl->WaitTaskDone(); }
}

bool SparksCoverAlgo::load_models(const std::vector<ModelConfig> &models) {
    if (models.size() != 3) {
        spdlog::error("SparksCoverAlgo only support three models");
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

void SparksCoverAlgo::async_infer(const int64_t image_id, const cv::Mat &image, InferCallback infer_callback) {
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
            std::vector<AlgoObject> sparks_objects;
            if (!data->data.empty() && data->data[0]->HasMetaValue()) {
                sparks_objects = filter_infer_result(data->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                     private_->model_configs[0].labels);
            }

            // 如果一阶段没有检测目标，直接返回
            if (sparks_objects.empty() && infer_callback) {
                infer_callback(image_id, image, {});
            } else {
                // 生成目标跟踪ID
                std::vector<Object> objects;
                for (const auto &item : sparks_objects) {
                    objects.push_back(Object{
                        .class_id = item.class_id,
                        .prob = item.score,
                        .rect = {(float)item.rect.x, (float)item.rect.y, (float)item.rect.width,
                                 (float)item.rect.height},
                        .label_name = item.label,
                    });
                }

                std::vector<AlgoObject> tracked_objects;
                for (auto &item : private_->tracker->update(objects)) {
                    tracked_objects.emplace_back(
                        AlgoObject{item.target_id, item.class_id, item.label_name, item.score,
                                   cv::Rect{(int)item.tlwh[0], (int)item.tlwh[1], (int)item.tlwh[2], (int)item.tlwh[3]},
                                   item.track_id});
                }

                // 裁剪目标 & 排序
                std::sort(tracked_objects.begin(), tracked_objects.end(),
                          [](const AlgoObject &item1, const AlgoObject &item2) {
                              return item1.score > item2.score
                                  && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
                          });

                // 裁剪目标数
                if (tracked_objects.size() > private_->model_configs[1].max_crop_number) {
                    tracked_objects.resize(private_->model_configs[1].max_crop_number);
                }

                // 二阶段检测
                std::vector<AlgoObject> match_objects;
                for (const auto &item : tracked_objects) {
                    auto crop_rect = scale_crop_rect(image.cols, image.rows, item.rect,
                                                     private_->model_configs[1].crop_scale_factor);
                    auto crop_image = image(crop_rect).clone();
                    gddeploy::BufSurfWrapperPtr crop_surface;
                    convertMat2BufSurface(crop_image, crop_surface);

                    auto in_package = gddeploy::Package::Create(1);
                    auto out_package = gddeploy::Package::Create(1);
                    in_package->data[0]->Set(crop_surface);
                    in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{
                        private_->model_configs[1].threshold, private_->model_configs[1].nms_threshold});

                    private_->model_impls[1]->InferSync(in_package, out_package);

                    std::vector<AlgoObject> person_objects;
                    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                        person_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                             private_->model_configs[1].labels);
                        for (auto &person_object : person_objects) {
                            person_object.rect.x += crop_rect.x;
                            person_object.rect.y += crop_rect.y;
                        }
                    }

                    // 裁剪目标 & 排序
                    std::sort(person_objects.begin(), person_objects.end(),
                              [](const AlgoObject &item1, const AlgoObject &item2) {
                                  return item1.score > item2.score
                                      && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
                              });

                    // 裁剪目标数
                    if (person_objects.size() > private_->model_configs[2].max_crop_number) {
                        person_objects.resize(private_->model_configs[2].max_crop_number);
                    }

                    // 三阶段检测
                    for (const auto &person_object : person_objects) {
                        crop_rect = scale_crop_rect(image.cols, image.rows, person_object.rect,
                                                    private_->model_configs[2].crop_scale_factor);
                        crop_image = image(crop_rect).clone();
                        gddeploy::BufSurfWrapperPtr person_surface;
                        convertMat2BufSurface(crop_image, person_surface);

                        in_package = gddeploy::Package::Create(1);
                        out_package = gddeploy::Package::Create(1);
                        in_package->data[0]->Set(person_surface);
                        in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{
                            private_->model_configs[2].threshold, private_->model_configs[2].nms_threshold});

                        private_->model_impls[2]->InferSync(in_package, out_package);

                        std::vector<AlgoObject> cover_objects;
                        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                            cover_objects =
                                filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                    private_->model_configs[2].labels);
                            for (auto &cover_object : cover_objects) {
                                cover_object.rect.x += crop_rect.x;
                                cover_object.rect.y += crop_rect.y;
                            }
                        }

                        // if (!cover_objects.empty()) {
                        //     cv::rectangle(image, crop_rect, cv::Scalar(0, 0, 255), 2);
                        //     for (auto &cover_object : cover_objects) {
                        //         spdlog::info("cover_object: {}", cover_object.label);
                        //         cv::rectangle(image, cover_object.rect, cv::Scalar(0, 255, 0), 2);
                        //     }
                        //     cv::imwrite("crop_image-1.jpg", image);
                        // }

                        if (cover_objects.empty()) { match_objects.emplace_back(person_object); }
                    }
                }

                if (infer_callback) {
                    infer_callback(image_id, image, private_->sequence_statistic->update(match_objects));
                }
            }
        });
}

bool SparksCoverAlgo::sync_infer(const int64_t image_id, const cv::Mat &image,
                                 std::vector<AlgoObject> &statistic_objects) {
    gddeploy::BufSurfWrapperPtr surface;
    convertMat2BufSurface(const_cast<cv::Mat &>(image), surface);

    // 一阶段检测
    auto in_package = gddeploy::Package::Create(1);
    in_package->data[0]->Set(surface);
    in_package->data[0]->SetAlgParam(
        gddeploy::AlgDetectParam{private_->model_configs[0].threshold, private_->model_configs[0].nms_threshold});

    auto out_package = gddeploy::Package::Create(1);
    if (private_->model_impls[0]->InferSync(in_package, out_package) != 0) { return false; }

    std::vector<AlgoObject> sparks_objects;
    if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
        sparks_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                             private_->model_configs[0].labels);
    }

    // 生成目标跟踪ID
    std::vector<Object> objects;
    for (const auto &item : sparks_objects) {
        objects.push_back(Object{
            .class_id = item.class_id,
            .prob = item.score,
            .rect = {(float)item.rect.x, (float)item.rect.y, (float)item.rect.width, (float)item.rect.height},
            .label_name = item.label,
        });
    }

    std::vector<AlgoObject> tracked_objects;
    for (auto &item : private_->tracker->update(objects)) {
        tracked_objects.emplace_back(AlgoObject{
            item.target_id, item.class_id, item.label_name, item.score,
            cv::Rect{(int)item.tlwh[0], (int)item.tlwh[1], (int)item.tlwh[2], (int)item.tlwh[3]}, item.track_id});
    }

    // 裁剪目标 & 排序
    std::sort(tracked_objects.begin(), tracked_objects.end(), [](const AlgoObject &item1, const AlgoObject &item2) {
        return item1.score > item2.score && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
    });

    // 裁剪目标数
    if (tracked_objects.size() > private_->model_configs[1].max_crop_number) {
        tracked_objects.resize(private_->model_configs[1].max_crop_number);
    }

    // 二阶段检测
    for (const auto &item : tracked_objects) {
        auto crop_rect =
            scale_crop_rect(image.cols, image.rows, item.rect, private_->model_configs[1].crop_scale_factor);
        auto crop_image = image(crop_rect).clone();
        gddeploy::BufSurfWrapperPtr crop_surface;
        convertMat2BufSurface(crop_image, crop_surface);

        in_package = gddeploy::Package::Create(1);
        out_package = gddeploy::Package::Create(1);
        in_package->data[0]->Set(crop_surface);
        in_package->data[0]->SetAlgParam(
            gddeploy::AlgDetectParam{private_->model_configs[1].threshold, private_->model_configs[1].nms_threshold});

        private_->model_impls[1]->InferSync(in_package, out_package);

        std::vector<AlgoObject> person_objects;
        if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
            person_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                 private_->model_configs[1].labels);
            for (auto &person_object : person_objects) {
                person_object.rect.x += crop_rect.x;
                person_object.rect.y += crop_rect.y;
            }
        }

        // 裁剪目标 & 排序
        std::sort(person_objects.begin(), person_objects.end(), [](const AlgoObject &item1, const AlgoObject &item2) {
            return item1.score > item2.score
                && item1.rect.width * item1.rect.height > item2.rect.width * item2.rect.height;
        });

        // 裁剪目标数
        if (person_objects.size() > private_->model_configs[2].max_crop_number) {
            person_objects.resize(private_->model_configs[2].max_crop_number);
        }

        // 三阶段检测
        std::vector<AlgoObject> match_objects;
        for (const auto &person_object : person_objects) {
            crop_rect = scale_crop_rect(image.cols, image.rows, person_object.rect,
                                        private_->model_configs[2].crop_scale_factor);
            crop_image = image(crop_rect).clone();
            gddeploy::BufSurfWrapperPtr person_surface;
            convertMat2BufSurface(crop_image, person_surface);

            in_package = gddeploy::Package::Create(1);
            out_package = gddeploy::Package::Create(1);
            in_package->data[0]->Set(person_surface);
            in_package->data[0]->SetAlgParam(gddeploy::AlgDetectParam{private_->model_configs[2].threshold,
                                                                      private_->model_configs[2].nms_threshold});

            private_->model_impls[2]->InferSync(in_package, out_package);

            std::vector<AlgoObject> cover_objects;
            if (!out_package->data.empty() && out_package->data[0]->HasMetaValue()) {
                cover_objects = filter_infer_result(out_package->data[0]->GetMetaData<gddeploy::InferResult>(),
                                                    private_->model_configs[2].labels);
                for (auto &cover_object : cover_objects) {
                    cover_object.rect.x += crop_rect.x;
                    cover_object.rect.y += crop_rect.y;
                }
            }

            // if (!cover_objects.empty()) {
            //     cv::rectangle(image, crop_rect, cv::Scalar(0, 0, 255), 2);
            //     for (auto &cover_object : cover_objects) {
            //         spdlog::info("cover_object: {}", cover_object.label);
            //         cv::rectangle(image, cover_object.rect, cv::Scalar(0, 255, 0), 2);
            //     }
            //     cv::imwrite("crop_image-1.jpg", image);
            // }

            if (cover_objects.empty()) { match_objects.emplace_back(person_object); }
        }

        statistic_objects = private_->sequence_statistic->update(match_objects);
    }

    return true;
}

std::vector<AlgoObject> SparksCoverAlgo::filter_infer_result(const gddeploy::InferResult &infer_result,
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