/**
 * @file utils.h
 * @author zhdotcai (caizhehong@gddi.com.cn)
 * @brief 
 * @version 1.0.0
 * @date 2024-10-18
 * 
 * @copyright Copyright (c) 2024 by GDDI
 * 
 */

#include <boost/geometry.hpp>
#include <boost/geometry/geometries/point_xy.hpp>
#include <boost/geometry/geometries/polygon.hpp>

namespace bg = boost::geometry;
using point_type = bg::model::d2::point_xy<float>;
using polygon_type = bg::model::polygon<point_type>;

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

namespace gddi {

inline cv::Rect2i scale_crop_rect(const int img_w, const int img_h, const cv::Rect2i &rect,
                                  const float scale_factor = 1.0f, const std::string scale_direction = "中心",
                                  bool dilated_area_only = false) {
    cv::Rect2i sacled_rect = rect;

    // 目标框放大
    if (scale_direction == "中心") {
        sacled_rect.width = ALIGN(int(rect.width * scale_factor), 16);
        sacled_rect.height = ALIGN(int(rect.height * scale_factor), 2);
        sacled_rect.x -= (sacled_rect.width - rect.width) / 2;
        sacled_rect.y -= (sacled_rect.height - rect.height) / 2;
    } else if (scale_direction == "上下") {
        sacled_rect.height = ALIGN(int(rect.height * scale_factor), 2);
        sacled_rect.y -= (sacled_rect.height - rect.height) / 2;
    } else if (scale_direction == "左右") {
        sacled_rect.width = ALIGN(int(rect.width * scale_factor), 16);
        sacled_rect.x -= (sacled_rect.width - rect.width) / 2;
    } else if (scale_direction == "向下") {
        sacled_rect.height += ALIGN(int(rect.height * (scale_factor - 1) * 0.5), 2);
        if (dilated_area_only) {
            sacled_rect.y += std::min(sacled_rect.height, rect.height);
            sacled_rect.height = std::abs(sacled_rect.height - rect.height);
        }
    } else if (scale_direction == "向上") {
        sacled_rect.height += ALIGN(int(rect.height * (scale_factor - 1) * 0.5), 2);
        if (dilated_area_only) {
            sacled_rect.y -= std::max(0, sacled_rect.height - rect.height);
            sacled_rect.height = std::abs(sacled_rect.height - rect.height);
        }
    } else if (scale_direction == "向左") {
        sacled_rect.width += ALIGN(int(rect.width * (scale_factor - 1) * 0.5), 16);
        if (dilated_area_only) {
            sacled_rect.x -= std::max(0, sacled_rect.width - rect.width);
            sacled_rect.width = std::abs(sacled_rect.width - rect.width);
        }
    } else if (scale_direction == "向右") {
        sacled_rect.width += ALIGN(int(rect.width * (scale_factor - 1) * 0.5), 16);
        if (dilated_area_only) {
            sacled_rect.x += std::min(sacled_rect.width, rect.width);
            sacled_rect.width = std::abs(sacled_rect.width - rect.width);
        }
    }

    // 边界判断
    if (sacled_rect.width > img_w) sacled_rect.width = img_w;
    if (sacled_rect.height > img_h) sacled_rect.height = img_h;
    if (sacled_rect.width < 16) sacled_rect.width = 16;
    if (sacled_rect.height < 16) sacled_rect.height = 16;

    // 重置顶点座标
    sacled_rect.x -= sacled_rect.x % 2;
    sacled_rect.y -= sacled_rect.y % 2;

    // 边界判断
    if (sacled_rect.x < 0) sacled_rect.x = 0;
    if (sacled_rect.y < 0) sacled_rect.y = 0;
    if (sacled_rect.x + sacled_rect.width > img_w) { sacled_rect.x = img_w - sacled_rect.width; }
    if (sacled_rect.y + sacled_rect.height > img_h) { sacled_rect.y = img_h - sacled_rect.height; }

    assert(sacled_rect.x + sacled_rect.width <= img_w);
    assert(sacled_rect.y + sacled_rect.height <= img_h);

    return sacled_rect;
}

inline polygon_type convert_polygon(const cv::Rect &rect) {
    polygon_type poly;
    bg::append(poly, bg::make<bg::model::d2::point_xy<float>>(rect.x, rect.y));
    bg::append(poly, bg::make<bg::model::d2::point_xy<float>>(rect.x + rect.width, rect.y));
    bg::append(poly, bg::make<bg::model::d2::point_xy<float>>(rect.x + rect.width, rect.y + rect.height));
    bg::append(poly, bg::make<bg::model::d2::point_xy<float>>(rect.x, rect.y + rect.height));
    bg::correct(poly);
    return poly;
}

inline float intersection(const polygon_type &poly1, const polygon_type &poly2) {
    std::vector<polygon_type> inter_output;
    bg::intersection(poly1, poly2, inter_output);
    float inter_area = 0;
    for (auto &p : inter_output) { inter_area += bg::area(p); }
    return inter_area;
}

inline float area_cover_rate(const cv::Rect &rect1, const cv::Rect &rect2) {
    polygon_type poly1 = convert_polygon(rect1);
    polygon_type poly2 = convert_polygon(rect2);

    auto inter_area = intersection(poly1, poly2);

    return inter_area / std::min(bg::area(poly1), bg::area(poly2));
}

inline std::vector<AlgoObject> find_cover_objects(const std::vector<AlgoObject> &objects,
                                                  const std::set<std::string> &include_labels,
                                                  const std::set<std::string> &exclude_labels,
                                                  const std::string &map_label, const float cover_threshold = 0.5) {
    std::vector<AlgoObject> cover_targets;

    std::set<int> include_ids;
    for (auto &target_1 : objects) {
        // 不在目标类别，在排除类别，或者在已记录的列表，直接跳过
        if (include_labels.count(target_1.label) == 0 || exclude_labels.count(target_1.label) > 0
            || include_ids.count(target_1.target_id) > 0) {
            continue;
        }

        std::map<int, AlgoObject> current_objects{{target_1.target_id, target_1}};
        std::set<std::string> target_labels{target_1.label};
        for (auto &target_2 : objects) {
            if (target_labels.count(target_2.label) > 0 || include_labels.count(target_2.label) == 0
                || include_ids.count(target_2.target_id) > 0) {
                continue;
            }

            // 计算两个目标的IOU
            auto cover_rate = area_cover_rate(target_1.rect, target_2.rect);
            if (cover_rate > 0 && exclude_labels.count(target_2.label) > 0) {
                break;
            } else if (cover_rate >= cover_threshold) {
                current_objects[target_2.target_id] = target_2;
                target_labels.emplace(target_2.label);
            }

            if (!include_labels.empty() && current_objects.size() >= include_labels.size()) {
                for (auto &[id, _] : current_objects) { include_ids.emplace(id); }

                // 合并目标
                float sum_score{0};
                cv::Rect2i rect;
                for (auto &[_, item] : current_objects) {
                    rect = rect | item.rect;
                    sum_score += item.score;
                }

                // 生成新的目标
                AlgoObject new_target;
                new_target.target_id = target_1.target_id;
                new_target.class_id = 0;
                new_target.label = map_label;
                new_target.score = sum_score / current_objects.size();
                new_target.rect = rect;
                new_target.track_id = target_1.track_id;
                cover_targets.emplace_back(new_target);

                break;
            }
        }
    }

    return cover_targets;
}

}// namespace gddi