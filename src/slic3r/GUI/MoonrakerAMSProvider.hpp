#ifndef slic3r_MoonrakerAMSProvider_hpp_
#define slic3r_MoonrakerAMSProvider_hpp_

#include "AMSProvider.hpp"
#include <functional>

namespace Slic3r { namespace GUI {

class MoonrakerAMSProvider : public AMSProvider {
public:
    MoonrakerAMSProvider(AMSProviderType type = AMSProviderType::MOONRAKER_AFC);
    virtual ~MoonrakerAMSProvider() = default;

    // AMSProvider interface
    bool detect_ams_support(const nlohmann::json& printer_objects) override;
    std::vector<AMSUnitInfo> get_ams_units() override;
    bool sync_filament_info() override;
    std::string get_provider_name() const override;
    std::map<std::string, std::string> get_property_mapping() const override;
    bool convert_to_ams_info(const AMSUnitInfo& unit_info, AMSinfo& ams_info) const override;

    // Moonraker-specific methods
    void set_printer_url(const std::string& url) { m_printer_url = url; }

private:
    std::string m_printer_url;

    // AFC-specific detection and parsing
    bool detect_afc_support(const nlohmann::json& printer_objects);
    bool detect_generic_moonraker_ams(const nlohmann::json& printer_objects);

    // Parse AFC data structures
    AMSUnitInfo parse_afc_unit(const std::string& extruder_name,
                              const nlohmann::json& afc_data,
                              const nlohmann::json& extruder_data);
    AMSCanInfo parse_afc_can(const std::string& lane_name,
                            const nlohmann::json& lane_data);

    // HTTP operations
    nlohmann::json query_printer_objects(const std::vector<std::string>& objects);
    bool is_moonraker_available();

    // Helper methods
    std::vector<std::string> extract_lanes_from_extruder(const nlohmann::json& extruder_data);
    std::string get_material_display_name(const std::string& material_type);
    int calculate_material_remaining(const nlohmann::json& lane_data);
};

}} // namespace Slic3r::GUI

#endif // slic3r_MoonrakerAMSProvider_hpp_