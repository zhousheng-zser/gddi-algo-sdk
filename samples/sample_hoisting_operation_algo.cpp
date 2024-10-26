#include "hoisting_operation_algo.h"
#include <chrono>
#include <opencv2/opencv.hpp>
#include <spdlog/spdlog.h>
#include <thread>

using namespace gddi;

int main(int argc, char *argv[]) {
    // 初始化算法
    HoistingOperationAlgoConfig config;
    auto hoisting_algo = std::make_unique<gddi::HoistingOperationAlgo>(config);

    // 加载模型, 注意: 模型加载顺序会影响推理结果, 1.5fbiao shi
    std::vector<ModelConfig> models = {
        {"light_model", "../models/hoisting_light.gdd", "../models/license_hoisting_light.gdd", 0.3, {"light_on"}},
        {"hoisting_model",
         "../models/hoisting_body.gdd",
         "../models/license_hoisting_body.gdd",
         0.3,
         {"structure_body"}},
        {"person_model", "../models/person.gdd", "../models/license_person.gdd", 0.3, {"person"}, 1.5f, 8}};

    if (!hoisting_algo->load_models(models)) {
        spdlog::error("Failed to load models");
        return -1;
    }

    // 读取视频, 进行推理
    std::string video_path = "../videos/hoisting_operation.mp4";
    auto image = cv::VideoCapture(video_path);
    if (!image.isOpened()) {
        printf("Failed to open video: %s\n", video_path.c_str());
        return -1;
    }

    int64_t frame_index = 0;
    while (true) {
        cv::Mat frame;
        image.read(frame);
        if (frame.empty()) { break; }

        auto start = std::chrono::steady_clock::now();

        std::vector<gddi::AlgoObject> objects;
        hoisting_algo->sync_infer(frame_index, frame, objects);

        if (!objects.empty()) {
            printf("=============== Frame: %ld, Objects: %ld\n", frame_index, objects.size());

            for (const auto &item : objects) {
                cv::rectangle(frame,
                              cv::Rect{(int)item.rect.x, (int)item.rect.y, (int)item.rect.width, (int)item.rect.height},
                              cv::Scalar(0, 0, 255), 2);
            }
            cv::imwrite("hoisting_" + std::to_string(frame_index) + ".jpg", frame);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            40
            - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));

        frame_index++;
    }

    printf("Finished\n");

    return 0;
}
