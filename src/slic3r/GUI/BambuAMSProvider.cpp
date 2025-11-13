#include "BambuAMSProvider.hpp"
#include "../DeviceManager.hpp"
#include "../GUI_App.hpp"
#include "libslic3r/Utils.hpp"

namespace Slic3r { namespace GUI {

BambuAMSProvider::BambuAMSProvider() : AMSProvider(AMSProviderType::BAMBU_AMS) {
}

bool BambuAMSProvider::detect_ams_support(const nlohmann::json& printer_objects) {
    // For Bambu AMS, detection happens through existing DeviceManager logic
    // This method is more for Moonraker-based systems
    // We can check if we have a Bambu printer with AMS capability
    auto obj = wxGetApp().getDeviceManager()->get_selected_machine();
    return obj && !obj->amsList.empty();
}

std::vector<AMSUnitInfo> BambuAMSProvider::get_ams_units() {
    std::vector<AMSUnitInfo> units;
    auto obj = wxGetApp().getDeviceManager()->get_selected_machine();
    if (!obj) return units;

    // Convert existing Bambu AMS data to our standardized format
    for (auto& ams_pair : obj->amsList) {
        AMSUnitInfo unit;
        unit.unit_id = ams_pair.first;
        unit.unit_name = "AMS " + ams_pair.first;
        unit.provider_type = AMSProviderType::BAMBU_AMS;

        auto ams = ams_pair.second;
        for (auto& tray_pair : ams->trayList) {
            AMSCanInfo can;
            auto tray = tray_pair.second;

            can.can_id = tray_pair.first;
            can.material_type = tray->type;
            can.material_name = tray->type;
            can.manufacturer = ""; // Bambu trays don't always have manufacturer info
            can.material_colour = parse_color(tray->color);
            can.material_remain = 100; // Bambu AMS doesn't report remaining amount typically
            can.is_empty = !tray->is_exists;

            // Store raw data for debugging
            can.raw_properties["setting_id"] = tray->setting_id;
            can.raw_properties["tag_uid"] = tray->tag_uid;
            can.raw_properties["color"] = tray->color;
            can.raw_properties["type"] = tray->type;

            unit.cans.push_back(can);
        }
        units.push_back(unit);
    }

    // Add virtual tray if supported
    if (obj->ams_support_virtual_tray) {
        AMSUnitInfo vt_unit;
        vt_unit.unit_id = "virtual_tray";
        vt_unit.unit_name = "External Spool";
        vt_unit.provider_type = AMSProviderType::BAMBU_AMS;

        AMSCanInfo vt_can;
        vt_can.can_id = "254"; // VIRTUAL_TRAY_ID
        vt_can.material_type = obj->vt_tray.type;
        vt_can.material_name = obj->vt_tray.type;
        vt_can.manufacturer = "";
        vt_can.material_colour = parse_color(obj->vt_tray.color);
        vt_can.material_remain = 100;
        vt_can.is_empty = false;

        vt_unit.cans.push_back(vt_can);
        units.push_back(vt_unit);
    }

    m_ams_units = units;
    return units;
}

bool BambuAMSProvider::sync_filament_info() {
    // For Bambu AMS, sync is handled by existing DeviceManager
    // Just refresh our internal data
    get_ams_units();
    return !m_ams_units.empty();
}

std::string BambuAMSProvider::get_provider_name() const {
    return "Bambu AMS";
}

std::map<std::string, std::string> BambuAMSProvider::get_property_mapping() const {
    // Bambu AMS uses OrcaSlicer native property names, so minimal mapping needed
    return {
        {"material_type", "type"},
        {"material_colour", "color"},
        {"can_id", "tray_id"},
        {"setting_id", "setting_id"},
        {"tag_uid", "tag_uid"},
        {"is_exists", "filament_exist"}
    };
}

bool BambuAMSProvider::convert_to_ams_info(const AMSUnitInfo& unit_info, AMSinfo& ams_info) const {
    ams_info.ams_id = unit_info.unit_id;

    // Determine AMS model based on unit properties
    if (unit_info.unit_id == "virtual_tray") {
        ams_info.ams_type = AMSModel::EXT_AMS;
    } else {
        ams_info.ams_type = AMSModel::N3F_AMS; // Default Bambu AMS type
    }

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
            can.material_state = AMSCanType::AMS_CAN_TYPE_BRAND; // Bambu brand
        }

        ams_info.cans.push_back(can);
    }

    return true;
}

std::map<int, DynamicPrintConfig> BambuAMSProvider::build_filament_ams_list(MachineObject* obj) {
    // Use existing logic from Plater.cpp - this is the key integration point
    std::map<int, DynamicPrintConfig> filament_ams_list;
    if (!obj) return filament_ams_list;

    // Virtual tray handling
    auto vt_tray = obj->vt_tray;
    if (obj->ams_support_virtual_tray) {
        DynamicPrintConfig vt_tray_config;
        vt_tray_config.set_key_value("filament_id", new ConfigOptionStrings{ vt_tray.setting_id });
        vt_tray_config.set_key_value("tag_uid", new ConfigOptionStrings{ vt_tray.tag_uid });
        vt_tray_config.set_key_value("filament_type", new ConfigOptionStrings{ vt_tray.type });
        vt_tray_config.set_key_value("tray_name", new ConfigOptionStrings{ std::string("Ext") });
        vt_tray_config.set_key_value("filament_colour", new ConfigOptionStrings{ into_u8(wxColour("#" + vt_tray.color).GetAsString(wxC2S_HTML_SYNTAX)) });
        vt_tray_config.set_key_value("filament_exist", new ConfigOptionBools{ true });

        vt_tray_config.set_key_value("filament_multi_colors", new ConfigOptionStrings{});
        for (int i = 0; i < vt_tray.cols.size(); ++i) {
            vt_tray_config.opt<ConfigOptionStrings>("filament_multi_colors")->values.push_back(into_u8(wxColour("#" + vt_tray.cols[i]).GetAsString(wxC2S_HTML_SYNTAX)));
        }
        filament_ams_list.emplace(VIRTUAL_TRAY_ID, std::move(vt_tray_config));
    }

    // AMS trays handling
    auto list = obj->amsList;
    for (auto ams : list) {
        char n = ams.first.front() - '0' + 'A';
        for (auto tray : ams.second->trayList) {
            char t = tray.first.front() - '0' + '1';
            DynamicPrintConfig tray_config;
            tray_config.set_key_value("filament_id", new ConfigOptionStrings{ tray.second->setting_id });
            tray_config.set_key_value("tag_uid", new ConfigOptionStrings{ tray.second->tag_uid });
            tray_config.set_key_value("filament_type", new ConfigOptionStrings{ tray.second->type });
            tray_config.set_key_value("tray_name", new ConfigOptionStrings{ std::string(1, n) + std::string(1, t) });
            tray_config.set_key_value("filament_colour", new ConfigOptionStrings{ into_u8(wxColour("#" + tray.second->color).GetAsString(wxC2S_HTML_SYNTAX)) });
            tray_config.set_key_value("filament_exist", new ConfigOptionBools{ tray.second->is_exists });

            tray_config.set_key_value("filament_multi_colors", new ConfigOptionStrings{});
            for (int i = 0; i < tray.second->cols.size(); ++i) {
                tray_config.opt<ConfigOptionStrings>("filament_multi_colors")->values.push_back(into_u8(wxColour("#" + tray.second->cols[i]).GetAsString(wxC2S_HTML_SYNTAX)));
            }
            filament_ams_list.emplace(((n - 'A') * 4 + t - '1'), std::move(tray_config));
        }
    }
    return filament_ams_list;
}

}} // namespace Slic3r::GUI