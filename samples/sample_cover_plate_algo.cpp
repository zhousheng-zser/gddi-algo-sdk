#include "cover_plate_algo.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

int main() {
    gddi::Cover_PlateAlgoConfig config;
    auto cover_plate_algo = std::make_unique<gddi::Cover_PlateAlgo>(config);

    std::string video_path = "../video/cover_plate.mp4";
    std::vector<gddi::ModelConfig> models = {
        {"cover_plate", "/opt/glasssix/edgebox/cpp/ai-sdk/model/gx_pump_cover_plate_api_model.gdd", "/opt/glasssix/edgebox/cpp/ai-sdk/license/gx_pump_cover_plate_api_license", 0.3, {"uncover_plate"}} };

    if (!cover_plate_algo->load_models(models)) {
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
        cover_plate_algo->sync_infer(frame_index, frame, objects);

        if (!objects.empty()) {
            printf("=============== Frame: %ld, Objects: %ld\n", frame_index, objects.size());

            for (const auto &item : objects) {
                cv::rectangle(frame,
                              cv::Rect{(int)item.rect.x, (int)item.rect.y, (int)item.rect.width, (int)item.rect.height},
                              cv::Scalar(0, 0, 255), 2);
            }
            cv::imwrite("cover_plate_" + std::to_string(frame_index) + ".jpg", frame);
        }

        frame_index++;
    }

    printf("Finished\n");

    return 0;
}