#include <catch2/catch.hpp>
#include <nlohmann/json.hpp>
#include "../../src/slic3r/GUI/AMSProvider.hpp"
#include "../../src/slic3r/GUI/MoonrakerAMSProvider.hpp"
#include "../../src/slic3r/GUI/BambuAMSProvider.hpp"
#include <wx/colour.h>

using namespace Slic3r::GUI;
using json = nlohmann::json;

// Mock HTTP client that returns predefined responses
class MockHttpClient {
public:
    static std::map<std::string, json> mock_responses;

    static void set_mock_response(const std::string& url_pattern, const json& response) {
        mock_responses[url_pattern] = response;
    }

    static json get_response(const std::string& url) {
        for (const auto& [pattern, response] : mock_responses) {
            if (url.find(pattern) != std::string::npos) {
                return response;
            }
        }
        return json{};
    }

    static void clear_responses() {
        mock_responses.clear();
    }
};

std::map<std::string, json> MockHttpClient::mock_responses;

// Test MoonrakerAMSProvider with realistic user-configurable AFC names
class TestMoonrakerAMSProvider : public MoonrakerAMSProvider {
public:
    TestMoonrakerAMSProvider() : MoonrakerAMSProvider(AMSProviderType::MOONRAKER_AFC) {
        m_printer_url = "http://test.printer:7125";
    }

    // Override HTTP query to use mock responses
    nlohmann::json query_printer_objects(const std::vector<std::string>& objects) override {
        std::string url = m_printer_url + "/printer/objects/query";
        if (!objects.empty()) {
            url += "?";
            for (size_t i = 0; i < objects.size(); ++i) {
                if (i > 0) url += "&";
                url += objects[i] + "=";
            }
        }
        return MockHttpClient::get_response(url);
    }
};

TEST_CASE("AMSProvider Factory Auto-Detection", "[ams_provider]") {
    MockHttpClient::clear_responses();

    SECTION("Detects AFC with user-configured names") {
        // Mock response with user-configured extruder and lane names
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder main_extruder", "AFC_extruder secondary", "AFC_stepper front_left",
                           "AFC_stepper front_right", "AFC_stepper back_left", "AFC_stepper back_right", "heater_bed"}}
            }}
        };

        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        auto provider = AMSProvider::create_provider("http://test.printer:7125");
        REQUIRE(provider != nullptr);
        REQUIRE(provider->get_provider_type() == AMSProviderType::MOONRAKER_AFC);
        REQUIRE(provider->get_provider_name() == "AFC (Moonraker)");
    }

    SECTION("Does not detect AFC when missing components") {
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "heater_bed", "extruder"}} // Missing AFC_extruder objects
            }}
        };

        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        auto provider = AMSProvider::create_provider("http://test.printer:7125");
        REQUIRE(provider == nullptr); // Should not detect AFC without extruders
    }

    SECTION("Falls back to Bambu AMS when AFC not detected") {
        json objects_response = {
            {"result", {
                {"objects", {"heater_bed", "extruder", "toolhead"}}
            }}
        };

        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        auto provider = AMSProvider::create_provider("http://test.printer:7125");
        REQUIRE(provider != nullptr);
        REQUIRE(provider->get_provider_type() == AMSProviderType::BAMBU_AMS);
    }
}

TEST_CASE("MoonrakerAMSProvider AFC Detection with Dynamic Names", "[moonraker_ams]") {
    MockHttpClient::clear_responses();

    SECTION("Detects AFC with creative user naming") {
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder hotend_v6", "AFC_extruder mosquito_hotend",
                           "AFC_stepper spool_A", "AFC_stepper spool_B", "AFC_stepper spool_C"}}
            }}
        };

        TestMoonrakerAMSProvider provider;
        bool detected = provider.detect_ams_support(objects_response["result"]);
        REQUIRE(detected == true);
    }

    SECTION("Handles single extruder with multiple lanes") {
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder primary_head", "AFC_stepper material_bay_1",
                           "AFC_stepper material_bay_2", "AFC_stepper material_bay_3", "AFC_stepper material_bay_4"}}
            }}
        };

        TestMoonrakerAMSProvider provider;
        bool detected = provider.detect_ams_support(objects_response["result"]);
        REQUIRE(detected == true);
    }
}

TEST_CASE("MoonrakerAMSProvider Unit Parsing with User Names", "[moonraker_ams]") {
    MockHttpClient::clear_responses();
    TestMoonrakerAMSProvider provider;

    SECTION("Parses AFC unit with custom extruder and lane names") {
        // Mock objects list response
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder custom_hotend", "AFC_stepper drawer_1",
                           "AFC_stepper drawer_2", "AFC_stepper drawer_3", "AFC_stepper drawer_4"}}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        // Mock AFC data response with custom names
        json afc_data_response = {
            {"result", {
                {"status", {
                    {"AFC", {"some_afc_data", "value"}},
                    {"AFC_extruder custom_hotend", {
                        {"lanes", {"drawer_1", "drawer_2", "drawer_3", "drawer_4"}}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC=&AFC_extruder custom_hotend=", afc_data_response);

        // Mock lane data response with filament details
        json lane_data_response = {
            {"result", {
                {"status", {
                    {"AFC_stepper drawer_1", {
                        {"load", true},
                        {"material", "PLA"},
                        {"color", "#FF5733"},
                        {"weight", 750.5},
                        {"spool_id", "spool_123"}
                    }},
                    {"AFC_stepper drawer_2", {
                        {"load", false},
                        {"material", ""},
                        {"color", ""},
                        {"weight", 0}
                    }},
                    {"AFC_stepper drawer_3", {
                        {"load", true},
                        {"material", "PETG"},
                        {"color", "#3366CC"},
                        {"weight", 920.0}
                    }},
                    {"AFC_stepper drawer_4", {
                        {"load", true},
                        {"material", "ABS"},
                        {"color", "#FFFFFF"},
                        {"weight", 800.0}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC_stepper drawer_1=&AFC_stepper drawer_2=&AFC_stepper drawer_3=&AFC_stepper drawer_4=", lane_data_response);

        auto units = provider.get_ams_units();
        REQUIRE(units.size() == 1);

        const auto& unit = units[0];
        REQUIRE(unit.unit_id == "custom_hotend");
        REQUIRE(unit.unit_name == "AFC custom_hotend");
        REQUIRE(unit.cans.size() == 4);

        // Check loaded PLA can
        const auto& can1 = unit.cans[0];
        REQUIRE(can1.can_id == "drawer_1");
        REQUIRE(can1.material_type == "PLA");
        REQUIRE(can1.material_name == "PLA");
        REQUIRE(can1.is_empty == false);
        REQUIRE(can1.material_remain == 93); // 750.5g should be ~93%

        // Check empty can
        const auto& can2 = unit.cans[1];
        REQUIRE(can2.can_id == "drawer_2");
        REQUIRE(can2.is_empty == true);
        REQUIRE(can2.material_name == "Empty");
        REQUIRE(can2.material_remain == 0);

        // Check loaded PETG can
        const auto& can3 = unit.cans[2];
        REQUIRE(can3.can_id == "drawer_3");
        REQUIRE(can3.material_type == "PETG");
        REQUIRE(can3.is_empty == false);
        REQUIRE(can3.material_remain == 100); // 920g should be 100%

        // Check loaded ABS can
        const auto& can4 = unit.cans[3];
        REQUIRE(can4.can_id == "drawer_4");
        REQUIRE(can4.material_type == "ABS");
        REQUIRE(can4.is_empty == false);
        REQUIRE(can4.material_remain == 100); // 800g exactly at threshold
    }
}

TEST_CASE("MoonrakerAMSProvider Multi-Extruder with Dynamic Names", "[moonraker_ams]") {
    MockHttpClient::clear_responses();
    TestMoonrakerAMSProvider provider;

    SECTION("Handles multi-extruder setup with user names") {
        // Mock objects list response with multiple extruders
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder left_head", "AFC_extruder right_head",
                           "AFC_stepper feed_A", "AFC_stepper feed_B", "AFC_stepper feed_C", "AFC_stepper feed_D"}}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        // Mock AFC data for both extruders
        json afc_data_response = {
            {"result", {
                {"status", {
                    {"AFC", {}},
                    {"AFC_extruder left_head", {
                        {"lanes", {"feed_A", "feed_B"}}
                    }},
                    {"AFC_extruder right_head", {
                        {"lanes", {"feed_C", "feed_D"}}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC=&AFC_extruder left_head=&AFC_extruder right_head=", afc_data_response);

        // Mock lane data for left extruder
        json left_lane_data = {
            {"result", {
                {"status", {
                    {"AFC_stepper feed_A", {
                        {"load", true},
                        {"material", "PLA"},
                        {"color", "#FF0000"},
                        {"weight", 500.0}
                    }},
                    {"AFC_stepper feed_B", {
                        {"load", true},
                        {"material", "TPU"},
                        {"color", "#00FF00"},
                        {"weight", 300.0}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC_stepper feed_A=&AFC_stepper feed_B=", left_lane_data);

        // Mock lane data for right extruder
        json right_lane_data = {
            {"result", {
                {"status", {
                    {"AFC_stepper feed_C", {
                        {"load", true},
                        {"material", "PETG"},
                        {"color", "#0000FF"},
                        {"weight", 850.0}
                    }},
                    {"AFC_stepper feed_D", {
                        {"load", false}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC_stepper feed_C=&AFC_stepper feed_D=", right_lane_data);

        auto units = provider.get_ams_units();
        REQUIRE(units.size() == 2);

        // Verify left extruder unit
        const auto& left_unit = std::find_if(units.begin(), units.end(),
            [](const AMSUnitInfo& u) { return u.unit_id == "left_head"; });
        REQUIRE(left_unit != units.end());
        REQUIRE(left_unit->cans.size() == 2);
        REQUIRE(left_unit->cans[0].can_id == "feed_A");
        REQUIRE(left_unit->cans[0].material_type == "PLA");
        REQUIRE(left_unit->cans[1].can_id == "feed_B");
        REQUIRE(left_unit->cans[1].material_type == "TPU");

        // Verify right extruder unit
        const auto& right_unit = std::find_if(units.begin(), units.end(),
            [](const AMSUnitInfo& u) { return u.unit_id == "right_head"; });
        REQUIRE(right_unit != units.end());
        REQUIRE(right_unit->cans.size() == 2);
        REQUIRE(right_unit->cans[0].can_id == "feed_C");
        REQUIRE(right_unit->cans[0].material_type == "PETG");
        REQUIRE(right_unit->cans[1].can_id == "feed_D");
        REQUIRE(right_unit->cans[1].is_empty == true);
    }
}

TEST_CASE("MoonrakerAMSProvider Color Parsing", "[moonraker_ams]") {
    TestMoonrakerAMSProvider provider;

    SECTION("Parses various color formats") {
        REQUIRE(provider.parse_color("#FF5733") == wxColour(255, 87, 51));
        REQUIRE(provider.parse_color("#ff5733") == wxColour(255, 87, 51)); // lowercase
        REQUIRE(provider.parse_color("FF5733") == wxColour(255, 87, 51));   // no hash
        REQUIRE(provider.parse_color("#FFF") == wxColour(255, 255, 255));   // 3-digit
        REQUIRE(provider.parse_color("") == *wxWHITE);                      // empty
        REQUIRE(provider.parse_color("invalid") == *wxWHITE);               // invalid
    }
}

TEST_CASE("MoonrakerAMSProvider Material Remaining Calculation", "[moonraker_ams]") {
    TestMoonrakerAMSProvider provider;

    SECTION("Calculates remaining percentage from weight") {
        json lane_data_full = {{"weight", 900.0}};
        REQUIRE(provider.calculate_material_remaining(lane_data_full) == 100);

        json lane_data_half = {{"weight", 400.0}};
        REQUIRE(provider.calculate_material_remaining(lane_data_half) == 50);

        json lane_data_empty = {{"weight", 0.0}};
        REQUIRE(provider.calculate_material_remaining(lane_data_empty) == 0);

        json lane_data_no_weight = {};
        REQUIRE(provider.calculate_material_remaining(lane_data_no_weight) == 100); // default
    }
}

TEST_CASE("MoonrakerAMSProvider Error Handling", "[moonraker_ams]") {
    MockHttpClient::clear_responses();
    TestMoonrakerAMSProvider provider;

    SECTION("Handles network errors gracefully") {
        // No mock response set - simulates network failure
        auto units = provider.get_ams_units();
        REQUIRE(units.empty());
    }

    SECTION("Handles malformed JSON responses") {
        json malformed_response = {
            {"result", {
                {"status", "not_an_object"}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query", malformed_response);

        auto units = provider.get_ams_units();
        REQUIRE(units.empty());
    }

    SECTION("Handles missing lane data") {
        json objects_response = {
            {"result", {
                {"objects", {"AFC", "AFC_extruder test_head", "AFC_stepper missing_lane"}}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query", objects_response);

        json afc_data_response = {
            {"result", {
                {"status", {
                    {"AFC", {}},
                    {"AFC_extruder test_head", {
                        {"lanes", {"missing_lane"}}
                    }}
                }}
            }}
        };
        MockHttpClient::set_mock_response("/printer/objects/query?AFC=&AFC_extruder test_head=", afc_data_response);

        // No mock response for lane data - simulates missing stepper data

        auto units = provider.get_ams_units();
        REQUIRE(units.size() == 1);
        REQUIRE(units[0].cans.empty()); // Should handle missing lane data gracefully
    }
}

TEST_CASE("BambuAMSProvider Integration", "[bambu_ams]") {
    BambuAMSProvider provider;

    SECTION("Reports correct provider type") {
        REQUIRE(provider.get_provider_type() == AMSProviderType::BAMBU_AMS);
        REQUIRE(provider.get_provider_name() == "Bambu Lab AMS");
    }

    SECTION("Has expected property mapping") {
        auto mapping = provider.get_property_mapping();
        REQUIRE(mapping.find("material_type") != mapping.end());
        REQUIRE(mapping.find("material_colour") != mapping.end());
    }
}

TEST_CASE("AMSProvider Base Functionality", "[ams_provider]") {
    TestMoonrakerAMSProvider provider;

    SECTION("Converts provider info to AMS info") {
        AMSUnitInfo unit_info;
        unit_info.unit_id = "test_unit";
        unit_info.provider_type = AMSProviderType::MOONRAKER_AFC;

        AMSCanInfo can1;
        can1.can_id = "test_can_1";
        can1.material_name = "PLA";
        can1.material_colour = wxColour(255, 0, 0);
        can1.material_remain = 75;
        can1.is_empty = false;
        unit_info.cans.push_back(can1);

        AMSCanInfo can2;
        can2.can_id = "test_can_2";
        can2.is_empty = true;
        unit_info.cans.push_back(can2);

        AMSinfo ams_info;
        bool result = provider.convert_to_ams_info(unit_info, ams_info);

        REQUIRE(result == true);
        REQUIRE(ams_info.ams_id == "test_unit");
        REQUIRE(ams_info.ams_type == AMSModel::GENERIC_AMS);
        REQUIRE(ams_info.cans.size() == 2);

        REQUIRE(ams_info.cans[0].can_id == "test_can_1");
        REQUIRE(ams_info.cans[0].material_state == AMSCanType::AMS_CAN_TYPE_THIRDBRAND);
        REQUIRE(ams_info.cans[0].material_remain == 75);

        REQUIRE(ams_info.cans[1].can_id == "test_can_2");
        REQUIRE(ams_info.cans[1].material_state == AMSCanType::AMS_CAN_TYPE_EMPTY);
    }
}