#ifndef slic3r_AMSProvider_hpp_
#define slic3r_AMSProvider_hpp_

#include <string>
#include <vector>
#include <map>
#include <memory>
#include "Widgets/AMSItem.hpp"
#include "nlohmann/json.hpp"

namespace Slic3r { namespace GUI {

enum class AMSProviderType {
    BAMBU_AMS,
    MOONRAKER_AFC,
    MOONRAKER_GENERIC,
    UNKNOWN
};

struct AMSCanInfo {
    std::string can_id;
    std::string material_type;
    std::string material_name;
    std::string manufacturer;
    wxColour material_colour = *wxWHITE;
    int material_remain = 100;
    bool is_empty = true;
    std::map<std::string, std::string> raw_properties; // Store original properties for debugging
};

struct AMSUnitInfo {
    std::string unit_id;
    std::string unit_name;
    std::vector<AMSCanInfo> cans;
    AMSProviderType provider_type;
    std::map<std::string, std::string> raw_data; // Store original data for debugging
};

class AMSProvider {
public:
    AMSProvider(AMSProviderType type) : m_provider_type(type) {}
    virtual ~AMSProvider() = default;

    // Core AMS operations
    virtual bool detect_ams_support(const nlohmann::json& printer_objects) = 0;
    virtual std::vector<AMSUnitInfo> get_ams_units() = 0;
    virtual bool sync_filament_info() = 0;
    virtual std::string get_provider_name() const = 0;

    // Property mapping for different AMS systems
    virtual std::map<std::string, std::string> get_property_mapping() const = 0;

    // Convert provider-specific data to standardized AMSinfo for existing GUI
    virtual bool convert_to_ams_info(const AMSUnitInfo& unit_info, AMSinfo& ams_info) const = 0;

    AMSProviderType get_provider_type() const { return m_provider_type; }

    // Factory method for creating providers
    static std::unique_ptr<AMSProvider> create_provider(AMSProviderType type);

    // Auto-detection factory - tries each provider type to find compatible one
    static std::unique_ptr<AMSProvider> auto_detect_provider(const nlohmann::json& printer_objects);

protected:
    AMSProviderType m_provider_type;
    std::vector<AMSUnitInfo> m_ams_units;

    // Helper methods for property mapping
    std::string map_property(const std::map<std::string, std::string>& source_data,
                           const std::string& target_property) const;
    wxColour parse_color(const std::string& color_str) const;
};

// Factory for provider auto-detection
class AMSProviderFactory {
public:
    static void register_provider_detector(AMSProviderType type,
                                         std::function<std::unique_ptr<AMSProvider>()> creator);
    static std::unique_ptr<AMSProvider> auto_detect(const nlohmann::json& printer_objects);

private:
    static std::map<AMSProviderType, std::function<std::unique_ptr<AMSProvider>()>> s_detectors;
};

}} // namespace Slic3r::GUI

#endif // slic3r_AMSProvider_hpp_