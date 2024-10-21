#include "light_goggle_algo.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

int main() {
    gddi::LightGoggleAlgoConfig config;
    auto light_goggle_algo = std::make_unique<gddi::LightGoggleAlgo>(config);

    std::string video_path = "../videos/light_goggle.mp4";
    std::vector<gddi::ModelConfig> models = {
        {"light", "../models/light.gdd", "../models/license_light.gdd", 0.3, {"light_on"}},
        {"person", "../models/person.gdd", "../models/license_person.gdd", 0.8, {"person"}},
        {"goggle", "../models/goggle.gdd", "../models/license_goggle.gdd", 0.3, {"goggle"}}};

    if (!light_goggle_algo->load_models(models)) {
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

        std::vector<gddi::AlgoObject> objects;
        light_goggle_algo->sync_infer(frame_index, frame, objects);

        if (!objects.empty()) {
            printf("=============== Frame: %ld, Objects: %ld\n", frame_index, objects.size());

            for (const auto &item : objects) {
                cv::rectangle(frame,
                              cv::Rect{(int)item.rect.x, (int)item.rect.y, (int)item.rect.width, (int)item.rect.height},
                              cv::Scalar(0, 0, 255), 2);
            }
            cv::imwrite("light_goggle_" + std::to_string(frame_index) + ".jpg", frame);
        }

        frame_index++;
    }

    printf("Finished\n");

    return 0;
}