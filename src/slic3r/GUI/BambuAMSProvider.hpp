#ifndef slic3r_BambuAMSProvider_hpp_
#define slic3r_BambuAMSProvider_hpp_

#include "AMSProvider.hpp"
#include "../DeviceManager.hpp"
#include "libslic3r/Config.hpp"

namespace Slic3r { namespace GUI {

// Implementation for existing Bambu AMS functionality
// Wraps existing Bambu AMS code with provider interface
class BambuAMSProvider : public AMSProvider {
public:
    BambuAMSProvider();
    virtual ~BambuAMSProvider() = default;

    // AMSProvider interface
    bool detect_ams_support(const nlohmann::json& printer_objects) override;
    std::vector<AMSUnitInfo> get_ams_units() override;
    bool sync_filament_info() override;
    std::string get_provider_name() const override;
    std::map<std::string, std::string> get_property_mapping() const override;
    bool convert_to_ams_info(const AMSUnitInfo& unit_info, AMSinfo& ams_info) const override;

    // Bambu-specific method for building filament AMS list (used by existing sync code)
    std::map<int, DynamicPrintConfig> build_filament_ams_list(MachineObject* obj);
};

}} // namespace Slic3r::GUI

#endif // slic3r_BambuAMSProvider_hpp_