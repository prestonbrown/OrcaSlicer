#include "AMSProvider.hpp"
#include "MoonrakerAMSProvider.hpp"
#include "BambuAMSProvider.hpp"
#include <regex>
#include <sstream>

namespace Slic3r { namespace GUI {

std::map<AMSProviderType, std::function<std::unique_ptr<AMSProvider>()>> AMSProviderFactory::s_detectors;

std::unique_ptr<AMSProvider> AMSProvider::create_provider(AMSProviderType type) {
    switch (type) {
        case AMSProviderType::MOONRAKER_AFC:
        case AMSProviderType::MOONRAKER_GENERIC:
            return std::make_unique<MoonrakerAMSProvider>(type);
        case AMSProviderType::BAMBU_AMS:
            return std::make_unique<BambuAMSProvider>();
        default:
            return nullptr;
    }
}

std::unique_ptr<AMSProvider> AMSProvider::auto_detect_provider(const nlohmann::json& printer_objects) {
    // Try AFC detection first (most specific)
    auto afc_provider = std::make_unique<MoonrakerAMSProvider>(AMSProviderType::MOONRAKER_AFC);
    if (afc_provider->detect_ams_support(printer_objects)) {
        return std::move(afc_provider);
    }

    // Try generic Moonraker detection
    auto generic_provider = std::make_unique<MoonrakerAMSProvider>(AMSProviderType::MOONRAKER_GENERIC);
    if (generic_provider->detect_ams_support(printer_objects)) {
        return std::move(generic_provider);
    }

    // Bambu AMS detection would happen elsewhere in existing code path
    return nullptr;
}

std::string AMSProvider::map_property(const std::map<std::string, std::string>& source_data,
                                    const std::string& target_property) const {
    auto mapping = get_property_mapping();
    auto it = mapping.find(target_property);
    if (it != mapping.end()) {
        auto source_it = source_data.find(it->second);
        if (source_it != source_data.end()) {
            return source_it->second;
        }
    }
    return "";
}

wxColour AMSProvider::parse_color(const std::string& color_str) const {
    if (color_str.empty()) {
        return *wxWHITE;
    }

    // Try hex format (#RRGGBB or #RGB)
    if (color_str[0] == '#') {
        std::string hex = color_str.substr(1);
        if (hex.length() == 6) {
            try {
                unsigned long rgb = std::stoul(hex, nullptr, 16);
                return wxColour((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
            } catch (...) {
                return *wxWHITE;
            }
        }
        else if (hex.length() == 3) {
            try {
                unsigned long rgb = std::stoul(hex, nullptr, 16);
                int r = (rgb >> 8) & 0xF; r |= (r << 4);
                int g = (rgb >> 4) & 0xF; g |= (g << 4);
                int b = rgb & 0xF; b |= (b << 4);
                return wxColour(r, g, b);
            } catch (...) {
                return *wxWHITE;
            }
        }
    }

    // Try RGB format rgb(r,g,b)
    std::regex rgb_regex(R"(rgb\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\))");
    std::smatch matches;
    if (std::regex_match(color_str, matches, rgb_regex)) {
        try {
            int r = std::stoi(matches[1].str());
            int g = std::stoi(matches[2].str());
            int b = std::stoi(matches[3].str());
            return wxColour(r, g, b);
        } catch (...) {
            return *wxWHITE;
        }
    }

    // Try common color names
    std::map<std::string, wxColour> color_names = {
        {"red", *wxRED}, {"green", *wxGREEN}, {"blue", *wxBLUE},
        {"yellow", *wxYELLOW}, {"black", *wxBLACK}, {"white", *wxWHITE},
        {"cyan", *wxCYAN}, {"magenta", wxColour(255, 0, 255)},
        {"orange", wxColour(255, 165, 0)}, {"purple", wxColour(128, 0, 128)}
    };

    std::string lower_color = color_str;
    std::transform(lower_color.begin(), lower_color.end(), lower_color.begin(), ::tolower);

    auto it = color_names.find(lower_color);
    if (it != color_names.end()) {
        return it->second;
    }

    return *wxWHITE;
}

void AMSProviderFactory::register_provider_detector(AMSProviderType type,
                                                   std::function<std::unique_ptr<AMSProvider>()> creator) {
    s_detectors[type] = creator;
}

std::unique_ptr<AMSProvider> AMSProviderFactory::auto_detect(const nlohmann::json& printer_objects) {
    return AMSProvider::auto_detect_provider(printer_objects);
}

}} // namespace Slic3r::GUI