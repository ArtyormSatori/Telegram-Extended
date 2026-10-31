#pragma once
#include "telegram_media/media_types.hpp"
namespace telegram_media {
[[nodiscard]] PlanResult create_plan(const MediaInfo& source, const ConversionRequest& request);
[[nodiscard]] std::vector<std::string> validate_plan(const MediaInfo& source, const ConversionRequest& request, const ConversionPlan& plan);
} // namespace telegram_media
