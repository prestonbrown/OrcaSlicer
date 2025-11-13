# AFC Testing Guide for OrcaSlicer

This guide provides instructions for testing the AMS provider implementation with real AFC (Automated Filament Changer) hardware.

## Prerequisites

### Hardware Requirements
- 3D printer running Klipper firmware
- AFC (Automated Filament Changer) system installed and configured
- Moonraker API server running on the printer
- Network connectivity between OrcaSlicer and the printer

### Software Requirements
- OrcaSlicer with AMS provider implementation
- AFC plugin installed and configured in Klipper
- Moonraker configured with proper API access

## AFC Configuration Verification

Before testing with OrcaSlicer, verify your AFC system is working correctly:

### 1. Check AFC Objects in Moonraker

Query the printer objects to ensure AFC is properly detected:

```bash
curl -X GET "http://your-printer-ip:7125/printer/objects/query"
```

Expected response should include:
- `"AFC"` - Main AFC object
- `"AFC_extruder <your_extruder_name>"` - Your configured extruder(s)
- `"AFC_stepper <your_lane_names>"` - Your configured stepper lanes

Example response:
```json
{
  "result": {
    "objects": [
      "AFC",
      "AFC_extruder hotend_v6",
      "AFC_stepper left_spool",
      "AFC_stepper right_spool",
      "AFC_stepper top_spool",
      "AFC_stepper bottom_spool"
    ]
  }
}
```

### 2. Verify AFC Data Structure

Query specific AFC data:

```bash
curl -X GET "http://your-printer-ip:7125/printer/objects/query?AFC=&AFC_extruder%20hotend_v6="
```

Expected response:
```json
{
  "result": {
    "status": {
      "AFC": {},
      "AFC_extruder hotend_v6": {
        "lanes": ["left_spool", "right_spool", "top_spool", "bottom_spool"]
      }
    }
  }
}
```

### 3. Check Lane/Stepper Data

Query stepper information:

```bash
curl -X GET "http://your-printer-ip:7125/printer/objects/query?AFC_stepper%20left_spool=&AFC_stepper%20right_spool="
```

Expected response:
```json
{
  "result": {
    "status": {
      "AFC_stepper left_spool": {
        "load": true,
        "material": "PLA",
        "color": "#FF5733",
        "weight": 750.5,
        "spool_id": "spool_001"
      },
      "AFC_stepper right_spool": {
        "load": false,
        "material": "",
        "color": "",
        "weight": 0
      }
    }
  }
}
```

## OrcaSlicer Configuration

### 1. Printer Profile Setup

Add or modify your printer profile to include AMS configuration:

```json
{
  "printer_model": "Your Printer Model",
  "printer_variant": "AFC",
  "use_ams_type": "moonraker_afc",
  "connection_type": "moonraker",
  "print_host": "http://your-printer-ip:7125"
}
```

### 2. Connection Settings

In OrcaSlicer printer settings:
1. Set **Print Host** to your printer's Moonraker URL (e.g., `http://192.168.1.100:7125`)
2. Set **Connection Type** to "Moonraker API"
3. Enable **Use AMS** option
4. Set **AMS Type** to "AFC (Moonraker)" if available in UI

## Testing Procedures

### Phase 1: Detection Testing

1. **Start OrcaSlicer** with your AFC printer profile
2. **Connect to Printer** - Verify connection establishes successfully
3. **Check AMS Detection** - Look for AFC units appearing in the AMS panel
4. **Verify Object Names** - Confirm your custom AFC object names are detected

Expected behavior:
- AFC system should be automatically detected
- Custom extruder and lane names should be displayed correctly
- No hardcoded "lane1", "lane2" names should appear

### Phase 2: Filament Information Sync

1. **Load Filament** into one or more AFC lanes using your AFC interface
2. **Set Material Properties** in AFC (material type, color, weight, etc.)
3. **Click Sync Button** in OrcaSlicer AMS panel
4. **Verify Information** appears correctly in OrcaSlicer

Expected behavior:
- Material types (PLA, PETG, ABS, etc.) display correctly
- Colors are parsed and displayed accurately
- Material remaining percentage is calculated from weight
- Empty lanes show as "Empty"

### Phase 3: Multi-Extruder Testing

If you have multiple extruders configured:

1. **Configure Multiple Extruders** in AFC with different lane assignments
2. **Load Different Materials** in each extruder's lanes
3. **Sync in OrcaSlicer** and verify each extruder unit appears separately
4. **Check Lane Assignments** are correct for each extruder

Expected behavior:
- Each extruder appears as a separate AMS unit
- Lane assignments match AFC configuration
- Materials are correctly associated with their respective extruders

### Phase 4: Error Handling

Test error conditions:

1. **Network Disconnection** - Disconnect network, verify graceful handling
2. **AFC Service Restart** - Restart AFC plugin, test reconnection
3. **Invalid Data** - Temporarily corrupt AFC data, verify error handling
4. **Empty Configuration** - Test with no materials loaded

Expected behavior:
- Network errors don't crash OrcaSlicer
- Invalid data is handled gracefully
- Error messages are informative and helpful

## Troubleshooting

### Common Issues

#### AFC Not Detected
- **Check Moonraker URL**: Ensure correct IP and port (usually 7125)
- **Verify AFC Plugin**: Confirm AFC is loaded in Klipper
- **Check Object Names**: Ensure objects follow "AFC_extruder " and "AFC_stepper " patterns

#### Materials Not Syncing
- **Check AFC Data**: Verify AFC is reporting material information correctly
- **Weight Information**: Ensure AFC is configured to track spool weights
- **Material Types**: Verify material names are set in AFC configuration

#### Custom Names Not Working
- **Object Naming**: Confirm AFC objects use proper prefixes
- **Lane Configuration**: Check extruder lane assignments in AFC
- **JSON Structure**: Verify AFC data follows expected JSON format

### Debug Information

Enable debug logging in OrcaSlicer to see AFC communication:

1. **Enable Debug Mode** in OrcaSlicer settings
2. **Check Console Output** for AFC-related messages
3. **Monitor Network Traffic** to see API calls to Moonraker

Look for log entries containing:
- `[MoonrakerAMS]` - AFC provider activities
- `HTTP request` - API communications
- `JSON parse` - Data parsing activities

### API Testing Commands

Use these commands to manually test AFC API endpoints:

```bash
# Test connection
curl -I "http://your-printer-ip:7125/printer/info"

# List all objects
curl "http://your-printer-ip:7125/printer/objects/query" | jq '.'

# Get AFC status
curl "http://your-printer-ip:7125/printer/objects/query?AFC=" | jq '.'

# Get specific extruder (replace 'your_extruder' with actual name)
curl "http://your-printer-ip:7125/printer/objects/query?AFC_extruder%20your_extruder=" | jq '.'

# Get stepper info (replace 'your_lane' with actual name)
curl "http://your-printer-ip:7125/printer/objects/query?AFC_stepper%20your_lane=" | jq '.'
```

## Expected Results

### Successful Integration
- ✅ AFC system detected automatically
- ✅ Custom object names handled correctly
- ✅ Material information syncs accurately
- ✅ Colors parsed and displayed properly
- ✅ Material remaining calculated from weight
- ✅ Multi-extruder setups work correctly
- ✅ Error conditions handled gracefully

### Performance Expectations
- **Detection Time**: Should occur within 2-3 seconds of connection
- **Sync Speed**: Material sync should complete within 5 seconds
- **Memory Usage**: Minimal impact on OrcaSlicer memory consumption
- **Network Traffic**: Efficient API usage with minimal repeated calls

## Reporting Issues

When reporting issues, please include:

1. **System Information**:
   - OrcaSlicer version
   - Operating system
   - Printer model and firmware version
   - AFC version and configuration

2. **Network Details**:
   - Printer IP address (sanitized)
   - Moonraker version
   - Network topology (WiFi/Ethernet)

3. **AFC Configuration**:
   - Number of extruders
   - Number of lanes per extruder
   - Custom object names used
   - Material types being tested

4. **Debug Logs**:
   - OrcaSlicer debug output
   - Moonraker logs (if accessible)
   - Network traffic captures (if available)

5. **API Responses**:
   - Output from manual API testing commands
   - Any error messages or unexpected responses

## Contributing

If you discover issues or have improvements:

1. **Document the Problem** thoroughly using the reporting template above
2. **Test Fixes** with your specific AFC configuration
3. **Verify Compatibility** with different AFC setups if possible
4. **Submit Issues** to the OrcaSlicer GitHub repository

Remember: AFC configurations vary significantly between users, so testing with multiple configurations helps ensure robust implementation.