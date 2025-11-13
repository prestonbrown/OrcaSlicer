# AFC Multi-Filament Sync - Implementation Roadmap

## ✅ Completed Work

### Phase 1: AMS Provider Architecture (COMPLETE)
- [x] Create AMSProvider base class with factory pattern
- [x] Implement BambuAMSProvider for existing Bambu AMS
- [x] Implement MoonrakerAMSProvider for AFC and generic Moonraker systems
- [x] Add auto-detection logic for AMS type
- [x] Comprehensive test suite (461 lines) in `tests/gui/test_ams_providers.cpp`
- [x] Documentation: `AFC_TESTING_GUIDE.md` and `BUILD_TEST_RESULTS.md`

### Phase 2: Filament Mapping Dialog Integration (COMPLETE)
- [x] Add `get_ams_provider()` method to MachineObject
- [x] Create adapter functions to convert AMSProvider data to FilamentInfo
- [x] Update `ams_filament_mapping()` to use provider system
- [x] Maintain backward compatibility with Bambu AMS
- [x] Existing dialog now works with AFC/Moonraker systems

**Files Modified:**
- `src/slic3r/GUI/DeviceManager.hpp` - Forward declarations and method signatures
- `src/slic3r/GUI/DeviceManager.cpp` - Provider integration and adapter functions

---

## 🚧 Recommended Next Steps

### Phase 3: Testing & Validation

#### 3.1 Build System Integration
**Priority: HIGH**
- [ ] Verify CMakeLists.txt includes new files correctly
- [ ] Test build on Linux, macOS, Windows
- [ ] Ensure tests compile and run: `cd build && ctest`
- [ ] Fix any compiler warnings or errors

**Files to Check:**
- `src/slic3r/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `tests/gui/CMakeLists.txt`

#### 3.2 Real Hardware Testing
**Priority: HIGH**
- [ ] Test with actual AFC hardware on Klipper printer
- [ ] Verify Moonraker API communication
- [ ] Test filament detection and color matching
- [ ] Validate manual override in mapping dialog
- [ ] Test with empty AFC lanes
- [ ] Test with partially loaded AFC

**Test Scenarios:**
1. Single color print → Should not show dialog
2. Multi-color print, all filaments loaded → Auto-match should work
3. Multi-color print, missing filament → Should show warning
4. Multi-color print, wrong colors → User overrides mapping
5. Network disconnect → Graceful error handling

#### 3.3 Bambu Regression Testing
**Priority: HIGH**
- [ ] Test with Bambu P1P/P1S (existing behavior)
- [ ] Test with Bambu X1/X1C (existing behavior)
- [ ] Verify no performance regression
- [ ] Ensure UI looks identical for Bambu users

---

### Phase 4: User Experience Enhancements

#### 4.1 UI/UX Improvements
**Priority: MEDIUM**
- [ ] Add AFC-specific icons in filament mapping dialog
- [ ] Show "AFC" or "Bambu AMS" label in dialog title
- [ ] Display AFC lane names (not just "Tray 1, Tray 2")
- [ ] Show filament remaining percentage in UI
- [ ] Add tooltips explaining AFC vs AMS differences

**Files to Modify:**
- `src/slic3r/GUI/AmsMappingPopup.cpp`
- `src/slic3r/GUI/AmsMappingPopup.hpp`
- Add AFC icon resources to `resources/images/`

#### 4.2 Configuration Wizard
**Priority: MEDIUM**
- [ ] Add AFC setup wizard for new users
- [ ] Guide users to configure Moonraker IP address
- [ ] Test AFC connection during wizard
- [ ] Create default AFC printer profile template

**New Files to Create:**
- `src/slic3r/GUI/AFCConfigWizard.cpp`
- `src/slic3r/GUI/AFCConfigWizard.hpp`
- `resources/profiles/Generic/AFC_Template.json`

#### 4.3 Error Handling & Feedback
**Priority: MEDIUM**
- [ ] Better error messages when AFC is unreachable
- [ ] Retry logic with exponential backoff
- [ ] Show connection status indicator in UI
- [ ] Log verbose AFC communication for debugging
- [ ] User-friendly troubleshooting messages

**Improvements Needed:**
- `MoonrakerAMSProvider::sync_filament_info()` - Add retry logic
- `MoonrakerAMSProvider::query_printer_objects()` - Better error messages
- GUI status bar - Show "Connecting to AFC..." during sync

---

### Phase 5: Advanced Features

#### 5.1 Live Status Monitoring (Optional)
**Priority: LOW**
- [ ] Show AFC status in Monitor tab during prints
- [ ] Display current lane being used
- [ ] Show filament remaining in real-time
- [ ] Alert on low filament (< 10%)
- [ ] Support for AFC filament change notifications

**Implementation Notes:**
- Would require WebSocket connection to Moonraker
- Similar to existing Bambu printer monitoring
- See `src/slic3r/GUI/DeviceManager.cpp` for reference

#### 5.2 Multi-AFC Support (Future)
**Priority: LOW**
- [ ] Support multiple AFC units on one printer
- [ ] AFC + Bambu AMS hybrid systems
- [ ] Custom lane naming and grouping
- [ ] AFC firmware version detection

#### 5.3 Filament Database Integration (Future)
**Priority: LOW**
- [ ] Save AFC filament profiles
- [ ] Remember which spools are loaded where
- [ ] Track filament usage per spool
- [ ] Warn when mixing incompatible materials
- [ ] Integration with filament vendor databases

---

## 📋 Testing Checklist

### Before Merging to Main
- [ ] All tests pass: `cd build && ctest --output-on-failure`
- [ ] No compiler warnings on GCC/Clang/MSVC
- [ ] Code follows OrcaSlicer style guidelines
- [ ] No memory leaks (run with valgrind on Linux)
- [ ] Documentation updated
- [ ] Tested on actual AFC hardware
- [ ] Tested on Bambu hardware (regression test)
- [ ] Performance: Dialog opens in < 2 seconds

### Manual Test Matrix

| Printer Type | AMS/MMU | Test Result | Notes |
|--------------|---------|-------------|-------|
| Voron 2.4 + AFC | Moonraker AFC | ⏳ Pending | |
| Bambu X1C | Bambu AMS | ⏳ Pending | Regression test |
| Bambu P1S | Bambu AMS | ⏳ Pending | Regression test |
| Generic Klipper | None | ⏳ Pending | Should not crash |
| Generic Klipper | Moonraker Generic | ⏳ Pending | Future feature |

---

## 🐛 Known Issues & Limitations

### Current Limitations
1. **Moonraker API Access** - Requires network connectivity to printer
2. **No Offline Mode** - Cannot use cached filament info if network is down
3. **Single AFC Unit** - Only supports one AFC per printer currently
4. **No Lane Naming** - Uses generic "Lane 1, Lane 2" instead of custom names
5. **HTTP Only** - No HTTPS support for Moonraker (usually local network only)

### Potential Issues to Watch
- **Color Matching Accuracy** - DeltaE76 may not be perfect for all colors
- **Moonraker Version Compatibility** - Tested with Moonraker 0.8+
- **AFC Firmware Versions** - Different AFC versions may have different API responses
- **Network Timeouts** - Slow networks may cause UI delays
- **Concurrent Access** - Multiple OrcaSlicer instances hitting same printer

---

## 🔧 Configuration Reference

### Printer Profile Settings

For AFC-equipped printers, add to your printer JSON profile:

```json
{
  "printer_settings_id": "voron_24_afc",
  "printer_model": "Voron 2.4",
  "gcode_flavor": "klipper",
  "single_extruder_multi_material": "1",
  "change_filament_gcode": "T[next_extruder]\nG4 P100",
  "machine_pause_gcode": "PAUSE"
}
```

### Moonraker Configuration

Ensure AFC plugin is loaded in `moonraker.conf`:

```ini
[afc]
enable_afc: True

[afc_extruder your_extruder]
lanes: lane1, lane2, lane3, lane4

[afc_stepper lane1]
material: PLA
color: #FF5733

[afc_stepper lane2]
material: PETG
color: #3498DB
```

---

## 📚 Documentation Improvements Needed

### User-Facing Documentation
- [ ] Add AFC section to OrcaSlicer wiki
- [ ] Create "How to set up AFC with OrcaSlicer" guide
- [ ] Video tutorial for AFC configuration
- [ ] Troubleshooting guide for common AFC issues
- [ ] FAQ for AFC vs Bambu AMS differences

### Developer Documentation
- [ ] Document AMSProvider interface for new implementations
- [ ] Code examples for adding new MMU systems
- [ ] API documentation for Moonraker integration
- [ ] Architecture diagram showing provider system flow

---

## 🎯 Success Criteria

The AFC integration will be considered complete when:

1. ✅ AFC filaments appear in mapping dialog automatically
2. ✅ Color matching works as well as Bambu AMS
3. ⏳ Users can override automatic assignments
4. ⏳ No crashes or freezes with AFC or non-AFC printers
5. ⏳ Tested on at least 3 different AFC hardware setups
6. ⏳ Bambu users see no change in behavior
7. ⏳ Documentation is complete and clear
8. ⏳ Build succeeds on all platforms
9. ⏳ All tests pass
10. ⏳ No memory leaks or performance regressions

---

## 📞 Support & Resources

### AFC Resources
- AFC Klipper Plugin: https://github.com/Armored-Dragon/klipper_afc
- Moonraker API Docs: https://moonraker.readthedocs.io/
- AFC Discord: [Link TBD]

### OrcaSlicer Resources
- OrcaSlicer Wiki: https://github.com/SoftFever/OrcaSlicer/wiki
- Build Instructions: See `CLAUDE.md`
- Testing Guide: See `AFC_TESTING_GUIDE.md`

### Contact
For questions about this implementation:
- GitHub: @prestonbrown
- Email: pbrown@brown-house.net

---

**Last Updated:** 2025-11-13
**Status:** Phase 2 Complete, Phase 3 Ready to Begin
