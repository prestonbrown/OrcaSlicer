#include "MoonrakerAMSProvider.hpp"
#include "libslic3r/Utils.hpp"
#include "../Utils/Http.hpp"
#include <algorithm>
#include <boost/log/trivial.hpp>
#include <boost/format.hpp>

namespace Slic3r { namespace GUI {

MoonrakerAMSProvider::MoonrakerAMSProvider(AMSProviderType type) : AMSProvider(type) {
}

bool MoonrakerAMSProvider::detect_ams_support(const nlohmann::json& printer_objects) {
    if (m_provider_type == AMSProviderType::MOONRAKER_AFC) {
        return detect_afc_support(printer_objects);
    } else if (m_provider_type == AMSProviderType::MOONRAKER_GENERIC) {
        return detect_generic_moonraker_ams(printer_objects);
    }
    return false;
}

bool MoonrakerAMSProvider::detect_afc_support(const nlohmann::json& printer_objects) {
    if (!printer_objects.contains("objects") || !printer_objects["objects"].is_array()) {
        return false;
    }

    bool has_afc = false;
    bool has_afc_extruder = false;

    for (const auto& obj : printer_objects["objects"]) {
        std::string obj_name = obj.get<std::string>();

        if (obj_name == "AFC") {
            has_afc = true;
        } else if (obj_name.find("AFC_extruder ") == 0) {
            has_afc_extruder = true;
        }
    }

    return has_afc && has_afc_extruder;
}

bool MoonrakerAMSProvider::detect_generic_moonraker_ams(const nlohmann::json& printer_objects) {
    // For future implementation of other Moonraker-based AMS systems
    // Look for other patterns like "filament_changer", "mmu", etc.
    return false;
}

std::vector<AMSUnitInfo> MoonrakerAMSProvider::get_ams_units() {
    if (!is_moonraker_available()) {
        return {};
    }

    // Query for AFC objects
    std::vector<std::string> objects_to_query = {"AFC"};

    // Find all AFC_extruder objects
    auto objects_list = query_printer_objects({});
    if (objects_list.contains("result") && objects_list["result"].contains("objects")) {
        for (const auto& obj : objects_list["result"]["objects"]) {
            std::string obj_name = obj.get<std::string>();
            if (obj_name.find("AFC_extruder ") == 0) {
                objects_to_query.push_back(obj_name);
            }
        }
    }

    auto afc_data = query_printer_objects(objects_to_query);
    if (!afc_data.contains("result") || !afc_data["result"].contains("status")) {
        return {};
    }

    std::vector<AMSUnitInfo> units;
    auto status = afc_data["result"]["status"];

    // Parse AFC units based on extruders
    for (const auto& [key, value] : status.items()) {
        if (key.find("AFC_extruder ") == 0) {
            std::string extruder_name = key.substr(13); // Remove "AFC_extruder " prefix

            // Get AFC main data
            nlohmann::json afc_main = status.contains("AFC") ? status["AFC"] : nlohmann::json{};

            AMSUnitInfo unit = parse_afc_unit(extruder_name, afc_main, value);
            if (!unit.slots.empty()) {
                units.push_back(unit);
            }
        }
    }

    m_ams_units = units;
    return units;
}

AMSUnitInfo MoonrakerAMSProvider::parse_afc_unit(const std::string& extruder_name,
                                                const nlohmann::json& afc_data,
                                                const nlohmann::json& extruder_data) {
    AMSUnitInfo unit;
    unit.unit_id = extruder_name;
    unit.unit_name = "AFC " + extruder_name;
    unit.provider_type = m_provider_type;
    unit.raw_data = extruder_data;

    // Extract lanes from extruder data
    std::vector<std::string> lanes = extract_lanes_from_extruder(extruder_data);

    // Query lane data
    std::vector<std::string> lane_objects;
    for (const auto& lane : lanes) {
        lane_objects.push_back("AFC_stepper " + lane);
    }

    auto lane_data = query_printer_objects(lane_objects);
    if (!lane_data.contains("result") || !lane_data["result"].contains("status")) {
        return unit;
    }

    auto lane_status = lane_data["result"]["status"];

    // Parse each lane
    for (const auto& lane : lanes) {
        std::string lane_key = "AFC_stepper " + lane;
        if (lane_status.contains(lane_key)) {
            AMSCanInfo can = parse_afc_can(lane, lane_status[lane_key]);
            unit.cans.push_back(can);
        }
    }

    return unit;
}

std::vector<std::string> MoonrakerAMSProvider::extract_lanes_from_extruder(const nlohmann::json& extruder_data) {
    std::vector<std::string> lanes;

    if (extruder_data.contains("lanes") && extruder_data["lanes"].is_array()) {
        for (const auto& lane : extruder_data["lanes"]) {
            if (lane.is_string()) {
                lanes.push_back(lane.get<std::string>());
            }
        }
    }

    return lanes;
}

AMSCanInfo MoonrakerAMSProvider::parse_afc_can(const std::string& lane_name,
                                              const nlohmann::json& lane_data) {
    AMSCanInfo can;
    can.can_id = lane_name;
    can.raw_properties = lane_data;

    // Check if can is empty
    bool is_loaded = lane_data.value("load", false);
    can.is_empty = !is_loaded;

    if (can.is_empty) {
        can.material_type = "";
        can.material_name = "Empty";
        can.manufacturer = "";
        can.material_colour = *wxWHITE;
        can.material_remain = 0;
        return can;
    }

    // Parse material information
    std::string material = lane_data.value("material", "");
    can.material_type = material;
    can.material_name = get_material_display_name(material);

    // Parse color
    std::string color_str = lane_data.value("color", "");
    can.material_colour = parse_color(color_str);

    // Calculate remaining material (use weight as heuristic)
    can.material_remain = calculate_material_remaining(lane_data);

    // Try to get manufacturer from Spoolman if spool_id is available
    if (lane_data.contains("spool_id") && !lane_data["spool_id"].is_null()) {
        // TODO: Implement Spoolman lookup for manufacturer info
        // For now, leave manufacturer empty
        can.manufacturer = "";
    }

    return can;
}

int MoonrakerAMSProvider::calculate_material_remaining(const nlohmann::json& lane_data) {
    if (!lane_data.contains("weight") || lane_data["weight"].is_null()) {
        return 100; // Default to full if no weight info
    }

    double weight = lane_data["weight"].get<double>();

    // Use weight as a heuristic for remaining material
    // Since we don't know original spool weight, we'll use some reasonable assumptions:
    // - Standard spool is ~1000g of filament
    // - Anything over 800g = 100%
    // - Scale linearly down to 0g = 0%

    if (weight >= 800.0) {
        return 100;
    } else if (weight <= 0.0) {
        return 0;
    } else {
        return static_cast<int>((weight / 800.0) * 100);
    }
}

std::string MoonrakerAMSProvider::get_material_display_name(const std::string& material_type) {
    if (material_type.empty()) {
        return "Unknown";
    }

    // Convert to uppercase for consistency with OrcaSlicer naming
    std::string upper_material = material_type;
    std::transform(upper_material.begin(), upper_material.end(), upper_material.begin(), ::toupper);

    return upper_material;
}

bool MoonrakerAMSProvider::sync_filament_info() {
    // Refresh AMS units data
    auto units = get_ams_units();
    return !units.empty();
}

std::string MoonrakerAMSProvider::get_provider_name() const {
    switch (m_provider_type) {
        case AMSProviderType::MOONRAKER_AFC:
            return "AFC (Moonraker)";
        case AMSProviderType::MOONRAKER_GENERIC:
            return "Generic Moonraker AMS";
        default:
            return "Unknown Moonraker AMS";
    }
}

std::map<std::string, std::string> MoonrakerAMSProvider::get_property_mapping() const {
    // Map OrcaSlicer property names to AFC property names
    return {
        {"material_type", "material"},
        {"material_colour", "color"},
        {"can_id", "name"},
        {"is_loaded", "load"},
        {"spool_id", "spool_id"},
        {"weight", "weight"},
        {"extruder_temp", "extruder_temp"},
        {"status", "status"}
    };
}

bool MoonrakerAMSProvider::convert_to_ams_info(const AMSUnitInfo& unit_info, AMSinfo& ams_info) const {
    ams_info.ams_id = unit_info.unit_id;
    ams_info.ams_type = AMSModel::GENERIC_AMS; // Use generic model for non-Bambu AMS
    ams_info.current_action = AMSAction::AMS_ACTION_NORMAL;
    ams_info.current_step = AMSPassRoadSTEP::AMS_ROAD_STEP_NONE;
    ams_info.current_can_id = "";

    // Convert cans
    ams_info.cans.clear();
    for (const auto& can_info : unit_info.cans) {
        Caninfo can;
        can.can_id = can_info.can_id;
        can.material_name = wxString::FromUTF8(can_info.material_name);
        can.material_colour = can_info.material_colour;
        can.material_remain = can_info.material_remain;

        if (can_info.is_empty) {
            can.material_state = AMSCanType::AMS_CAN_TYPE_EMPTY;
        } else {
            can.material_state = AMSCanType::AMS_CAN_TYPE_THIRDBRAND; // Third-party material
        }

        ams_info.cans.push_back(can);
    }

    return true;
}

nlohmann::json MoonrakerAMSProvider::query_printer_objects(const std::vector<std::string>& objects) {
    if (m_printer_url.empty()) {
        return nlohmann::json{};
    }

    // Build URL for Moonraker printer/objects/query API
    std::string url = m_printer_url;
    if (url.back() != '/') url += "/";
    url += "printer/objects/query";

    // Add objects as query parameters if provided
    if (!objects.empty()) {
        url += "?";
        for (size_t i = 0; i < objects.size(); ++i) {
            if (i > 0) url += "&";
            url += objects[i] + "=";
        }
    }

    // Use existing HTTP client infrastructure
    auto http = Http::get(url);

    std::string response_body;
    bool request_successful = false;

    // Set up callbacks
    http.on_complete([&](std::string body, unsigned http_status) {
        if (http_status == 200) {
            response_body = body;
            request_successful = true;
        }
    });

    http.on_error([&](std::string body, std::string error, unsigned http_status) {
        BOOST_LOG_TRIVIAL(error) << boost::format("[MoonrakerAMS] HTTP request failed: %1% (status: %2%)") % error % http_status;
        request_successful = false;
    });

    // Execute synchronously
    http.perform_sync();

    if (!request_successful || response_body.empty()) {
        return nlohmann::json{};
    }

    try {
        return nlohmann::json::parse(response_body);
    } catch (const std::exception& e) {
        BOOST_LOG_TRIVIAL(error) << boost::format("[MoonrakerAMS] Failed to parse JSON response: %1%") % e.what();
        return nlohmann::json{};
    }
}

bool MoonrakerAMSProvider::is_moonraker_available() {
    return !m_printer_url.empty();
}

}} // namespace Slic3r::GUI