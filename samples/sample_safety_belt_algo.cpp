#include "safety_belt_algo.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

int main() {
    gddi::SafetyBeltAlgoConfig config;
    auto safety_belt_algo = std::make_unique<gddi::SafetyBeltAlgo>(config);

    std::string video_path = "../videos/safety_belt.mp4";
    std::vector<gddi::ModelConfig> models = {
        {"person", "../models/person.gdd", "../models/license_person.gdd", 0.3, {"person"}},
        {"safety_belt", "../models/safety_belt.gdd", "../models/license_safety_belt.gdd", 0.3, {"safety_belt"}},
        {"safety_belt_light",
         "../models/safety_belt_light.gdd",
         "../models/license_safety_belt_light.gdd",
         0.3,
         {"light_on"}}};

    if (!safety_belt_algo->load_models(models)) {
        printf("Failed to load models\n");
        return -1;
    }

    // 读取视频, 进行推理
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
        safety_belt_algo->sync_infer(frame_index, frame, objects);

        if (!objects.empty()) {
            printf("=============== Frame: %ld, Objects: %ld\n", frame_index, objects.size());

            for (const auto &item : objects) {
                cv::rectangle(frame,
                              cv::Rect{(int)item.rect.x, (int)item.rect.y, (int)item.rect.width, (int)item.rect.height},
                              cv::Scalar(0, 0, 255), 2);
            }
            cv::imwrite("safety_belt_" + std::to_string(frame_index) + ".jpg", frame);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(
            40
            - std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count()));

        frame_index++;
    }

    printf("Finished\n");

    return 0;
}