/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SERVICES_URIBEACONCONFIGSERVICE_H_
#define SERVICES_URIBEACONCONFIGSERVICE_H_

#include "BLEDevice.h"
#include "mbed.h"

#define UUID_URI_BEACON(FIRST, SECOND) {                         \
        0xee, 0x0c, FIRST, SECOND, 0x87, 0x86, 0x40, 0xba,       \
        0xab, 0x96, 0x99, 0xb9, 0x1a, 0xc9, 0x81, 0xd8,          \
}

static const uint8_t UUID_URI_BEACON_SERVICE[]    = UUID_URI_BEACON(0x20, 0x80);
static const uint8_t UUID_LOCK_STATE_CHAR[]       = UUID_URI_BEACON(0x20, 0x81);
static const uint8_t UUID_LOCK_CHAR[]             = UUID_URI_BEACON(0x20, 0x82);
static const uint8_t UUID_UNLOCK_CHAR[]           = UUID_URI_BEACON(0x20, 0x83);
static const uint8_t UUID_URI_DATA_CHAR[]         = UUID_URI_BEACON(0x20, 0x84);
static const uint8_t UUID_FLAGS_CHAR[]            = UUID_URI_BEACON(0x20, 0x85);
static const uint8_t UUID_ADV_POWER_LEVELS_CHAR[] = UUID_URI_BEACON(0x20, 0x86);
static const uint8_t UUID_TX_POWER_MODE_CHAR[]    = UUID_URI_BEACON(0x20, 0x87);
static const uint8_t UUID_BEACON_PERIOD_CHAR[]    = UUID_URI_BEACON(0x20, 0x88);
static const uint8_t UUID_RESET_CHAR[]            = UUID_URI_BEACON(0x20, 0x89);
static const uint8_t BEACON_UUID[] = {0xD8, 0xFE};

/**
* @class URIBeaconConfigService
* @brief UriBeacon Configuration Service. Can be used to set URL, adjust power levels, and set flags.
* See http://uribeacon.org
*
*/
class URIBeaconConfigService {
  public:
    /**
     * @brief Transmission Power Modes for UriBeacon
     */
    static const uint8_t TX_POWER_MODE_LOWEST = 0; /*!< Lowest TX power mode */
    static const uint8_t TX_POWER_MODE_LOW    = 1; /*!< Low TX power mode */
    static const uint8_t TX_POWER_MODE_MEDIUM = 2; /*!< Medium TX power mode */
    static const uint8_t TX_POWER_MODE_HIGH   = 3; /*!< High TX power mode */
    static const unsigned int NUM_POWER_MODES = 4; /*!< Number of Power Modes defined */

    static const int ADVERTISING_INTERVAL_MSEC = 1000;  // Advertising interval for config service.
    static const int SERVICE_DATA_MAX = 31;             // Maximum size of service data in ADV packets

    typedef uint8_t Lock_t[16];               /* 128 bits */
    typedef int8_t PowerLevels_t[NUM_POWER_MODES];

    static const int URI_DATA_MAX = 18;
    typedef uint8_t  UriData_t[URI_DATA_MAX];

    struct Params_t {
        Lock_t        lock;
        uint8_t       uriDataLength;
        UriData_t     uriData;
        uint8_t       flags;
        PowerLevels_t advPowerLevels; // Current value of AdvertisedPowerLevels
        uint8_t       txPowerMode;    // Firmware power levels used with setTxPower()
        uint16_t      beaconPeriod;
    };

    /**
     * @param[ref]    ble
     *                    BLEDevice object for the underlying controller.
     * @param[in/out] paramsIn
     *                    Reference to application-visible beacon state, loaded
     *                    from persistent storage at startup.
     * @paramsP[in]   resetToDefaultsFlag
     *                    Applies to the state of the 'paramsIn' parameter.
     *                    If true, it indicates that paramsIn is potentially
     *                    un-initialized, and default values should be used
     *                    instead. Otherwise, paramsIn overrides the defaults.
     * @param[in]     defaultUriDataIn
     *                    Default un-encoded URI; applies only if the resetToDefaultsFlag is true.
     * @param[in]     defaultAdvPowerLevelsIn
     *                    Default power-levels array; applies only if the resetToDefaultsFlag is true.
     */
    URIBeaconConfigService(BLEDevice     &bleIn,
                           Params_t      &paramsIn,
                           bool          resetToDefaultsFlag,
                           const char   *defaultURIDataIn,
                           PowerLevels_t &defaultAdvPowerLevelsIn) :
        ble(bleIn),
        params(paramsIn),
        defaultUriDataLength(),
        defaultUriData(),
        defaultAdvPowerLevels(defaultAdvPowerLevelsIn),
        initSucceeded(false),
        resetFlag(),
        lockedStateChar(UUID_LOCK_STATE_CHAR, &lockedState),
        lockChar(UUID_LOCK_CHAR, &params.lock),
        uriDataChar(UUID_URI_DATA_CHAR, params.uriData, 0, URI_DATA_MAX,
                    GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE),
        unlockChar(UUID_UNLOCK_CHAR, &params.lock),
        flagsChar(UUID_FLAGS_CHAR, &params.flags),
        advPowerLevelsChar(UUID_ADV_POWER_LEVELS_CHAR, &params.advPowerLevels),
        txPowerModeChar(UUID_TX_POWER_MODE_CHAR, &params.txPowerMode),
        beaconPeriodChar(UUID_BEACON_PERIOD_CHAR, &params.beaconPeriod),
        resetChar(UUID_RESET_CHAR, &resetFlag) {

        encodeURI(defaultURIDataIn, defaultUriData, defaultUriDataLength);
        if (defaultUriDataLength > URI_DATA_MAX) {
            return;
        }

        if (!resetToDefaultsFlag && (params.uriDataLength > URI_DATA_MAX)) {
            resetToDefaultsFlag = true;
        }
        if (resetToDefaultsFlag) {
            resetToDefaults();
        } else {
            updateCharacteristicValues();
        }

        lockedState = isLocked();

        lockChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::lockAuthorizationCallback);
        unlockChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::unlockAuthorizationCallback);
        uriDataChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::uriDataWriteAuthorizationCallback);
        flagsChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::flagsAuthorizationCallback);
        advPowerLevelsChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::denyGATTWritesIfLocked);
        txPowerModeChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::denyGATTWritesIfLocked);
        beaconPeriodChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::denyGATTWritesIfLocked);
        resetChar.setWriteAuthorizationCallback(this, &URIBeaconConfigService::denyGATTWritesIfLocked);

        static GattCharacteristic *charTable[] = {
            &lockedStateChar, &lockChar, &unlockChar, &uriDataChar,
            &flagsChar, &advPowerLevelsChar, &txPowerModeChar, &beaconPeriodChar, &resetChar
        };

        GattService configService(UUID_URI_BEACON_SERVICE, charTable, sizeof(charTable) / sizeof(GattCharacteristic *));

        ble.addService(configService);
        ble.onDataWritten(this, &URIBeaconConfigService::onDataWrittenCallback);

        setupURIBeaconConfigAdvertisements(); /* Setup advertising for the configService. */

        initSucceeded = true;
    }

    bool configuredSuccessfully(void) const {
        return initSucceeded;
    }

    /* Start out by advertising the configService for a limited time after
     * startup; and switch to the normal non-connectible beacon functionality
     * afterwards. */
    void setupURIBeaconConfigAdvertisements()
    {
        const char DEVICE_NAME[] = "mUriBeacon Config";

        ble.clearAdvertisingPayload();

        ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);

        // UUID is in different order in the ADV frame (!)
        uint8_t reversedServiceUUID[sizeof(UUID_URI_BEACON_SERVICE)];
        for (unsigned int i = 0; i < sizeof(UUID_URI_BEACON_SERVICE); i++) {
            reversedServiceUUID[i] = UUID_URI_BEACON_SERVICE[sizeof(UUID_URI_BEACON_SERVICE) - i - 1];
        }
        ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS, reversedServiceUUID, sizeof(reversedServiceUUID));
        ble.accumulateAdvertisingPayload(GapAdvertisingData::GENERIC_TAG);
        ble.accumulateScanResponse(GapAdvertisingData::COMPLETE_LOCAL_NAME, reinterpret_cast<const uint8_t *>(&DEVICE_NAME), sizeof(DEVICE_NAME));
        ble.accumulateScanResponse(
            GapAdvertisingData::TX_POWER_LEVEL,
            reinterpret_cast<uint8_t *>(&defaultAdvPowerLevels[URIBeaconConfigService::TX_POWER_MODE_LOW]),
            sizeof(uint8_t));

        ble.setTxPower(params.advPowerLevels[params.txPowerMode]);
        ble.setDeviceName(reinterpret_cast<const uint8_t *>(&DEVICE_NAME));
        ble.setAdvertisingType(GapAdvertisingParams::ADV_CONNECTABLE_UNDIRECTED);
        ble.setAdvertisingInterval(Gap::MSEC_TO_ADVERTISEMENT_DURATION_UNITS(ADVERTISING_INTERVAL_MSEC));
    }

    /* Helper function to switch to the non-connectible normal mode for URIBeacon. This gets called after a timeout. */
    void setupURIBeaconAdvertisements()
    {
        uint8_t serviceData[SERVICE_DATA_MAX];
        unsigned serviceDataLen = 0;

        /* Reinitialize the BLE stack. This will clear away the existing services and advertising state. */
        ble.shutdown();
        ble.init();

        // Fields from the Service
        unsigned beaconPeriod                                 = params.beaconPeriod;
        unsigned txPowerMode                                  = params.txPowerMode;
        unsigned uriDataLength                                = params.uriDataLength;
        URIBeaconConfigService::UriData_t &uriData            = params.uriData;
        URIBeaconConfigService::PowerLevels_t &advPowerLevels = params.advPowerLevels;
        uint8_t flags                                         = params.flags;

        extern void saveURIBeaconConfigParams(const Params_t *paramsP); /* forward declaration; necessary to avoid a circular dependency. */
        saveURIBeaconConfigParams(&params);

        ble.clearAdvertisingPayload();
        ble.setTxPower(params.advPowerLevels[params.txPowerMode]);
        ble.setAdvertisingType(GapAdvertisingParams::ADV_NON_CONNECTABLE_UNDIRECTED);
        ble.setAdvertisingInterval(Gap::MSEC_TO_ADVERTISEMENT_DURATION_UNITS(beaconPeriod));
        ble.accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE);
        ble.accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_16BIT_SERVICE_IDS, BEACON_UUID, sizeof(BEACON_UUID));

        serviceData[serviceDataLen++] = BEACON_UUID[0];
        serviceData[serviceDataLen++] = BEACON_UUID[1];
        serviceData[serviceDataLen++] = flags;
        serviceData[serviceDataLen++] = advPowerLevels[txPowerMode];
        for (unsigned j = 0; j < uriDataLength; j++) {
            serviceData[serviceDataLen++] = uriData[j];
        }
        ble.accumulateAdvertisingPayload(GapAdvertisingData::SERVICE_DATA, serviceData, serviceDataLen);
    }

  private:
    // True if the lock bits are non-zero
    bool isLocked() {
        Lock_t testLock;
        memset(testLock, 0, sizeof(Lock_t));
        return memcmp(params.lock, testLock, sizeof(Lock_t));
    }

    /*
     * This callback is invoked when a GATT client attempts to modify any of the
     * characteristics of this service. Attempts to do so are also applied to
     * the internal state of this service object.
     */
    void onDataWrittenCallback(const GattCharacteristicWriteCBParams *writeParams) {
        uint16_t handle = writeParams->charHandle;

        if (handle == lockChar.getValueHandle()) {
            // Validated earlier
            memcpy(params.lock, writeParams->data, sizeof(Lock_t));
            // use isLocked() in case bits are being set to all 0's
            lockedState = isLocked();
        } else if (handle == unlockChar.getValueHandle()) {
            // Validated earlier
            memset(params.lock, 0, sizeof(Lock_t));
            lockedState = false;
        } else if (handle == uriDataChar.getValueHandle()) {
            params.uriDataLength = writeParams->len;
            memcpy(params.uriData, writeParams->data, params.uriDataLength);
        } else if (handle == flagsChar.getValueHandle()) {
            params.flags = *(writeParams->data);
        } else if (handle == advPowerLevelsChar.getValueHandle()) {
            memcpy(params.advPowerLevels, writeParams->data, sizeof(PowerLevels_t));
        } else if (handle == txPowerModeChar.getValueHandle()) {
            params.txPowerMode = *(writeParams->data);
        } else if (handle == beaconPeriodChar.getValueHandle()) {
            params.beaconPeriod = *((uint16_t *)(writeParams->data));
        } else if (handle == resetChar.getValueHandle()) {
            resetToDefaults();
        }
    }

    /*
     * Reset the default values.
     */
    void resetToDefaults(void) {
        lockedState             = false;
        memset(params.lock, 0, sizeof(Lock_t));
        memcpy(params.uriData, defaultUriData, URI_DATA_MAX);
        params.uriDataLength    = defaultUriDataLength;
        params.flags            = 0;
        memcpy(params.advPowerLevels, defaultAdvPowerLevels, sizeof(PowerLevels_t));
        params.txPowerMode      = TX_POWER_MODE_LOW;
        params.beaconPeriod     = 1000;
        updateCharacteristicValues();
    }

    /*
     * Internal helper function used to update the GATT database following any
     * change to the internal state of the service object.
     */
    void updateCharacteristicValues(void) {
        ble.updateCharacteristicValue(lockedStateChar.getValueHandle(), &lockedState, 1);
        ble.updateCharacteristicValue(uriDataChar.getValueHandle(), params.uriData, params.uriDataLength);
        ble.updateCharacteristicValue(flagsChar.getValueHandle(), &params.flags, 1);
        ble.updateCharacteristicValue(beaconPeriodChar.getValueHandle(),
                                      reinterpret_cast<uint8_t *>(&params.beaconPeriod), sizeof(uint16_t));
        ble.updateCharacteristicValue(txPowerModeChar.getValueHandle(), &params.txPowerMode, 1);
        ble.updateCharacteristicValue(advPowerLevelsChar.getValueHandle(),
                                      reinterpret_cast<uint8_t *>(params.advPowerLevels), sizeof(PowerLevels_t));
    }

  private:
    void lockAuthorizationCallback(GattCharacteristicWriteAuthCBParams *authParams) {
        authParams->authorizationReply = (authParams->len == sizeof(Lock_t)) && !lockedState;
    }


    void unlockAuthorizationCallback(GattCharacteristicWriteAuthCBParams *authParams) {
        if (!lockedState || (authParams->len == sizeof(Lock_t) && (memcmp(authParams->data, params.lock, sizeof(Lock_t)) == 0))) {
            authParams->authorizationReply = true;
        } else {
            authParams->authorizationReply = false;
        }
    }

    void uriDataWriteAuthorizationCallback(GattCharacteristicWriteAuthCBParams *authParams) {
        if (lockedState || (authParams->offset != 0) || (authParams->len > URI_DATA_MAX)) {
            authParams->authorizationReply = false;
        }
    }

    void flagsAuthorizationCallback(GattCharacteristicWriteAuthCBParams *authParams) {
        if (lockedState || (authParams->len != 1)) {
            authParams->authorizationReply = false;
        }
    }

    void denyGATTWritesIfLocked(GattCharacteristicWriteAuthCBParams *authParams) {
        if (lockedState) {
            authParams->authorizationReply = false;
        }
    }

    BLEDevice     &ble;
    Params_t      &params;
    // Default value that is restored on reset
    size_t        defaultUriDataLength;
    UriData_t     defaultUriData;
    // Default value that is restored on reset
    PowerLevels_t &defaultAdvPowerLevels;
    uint8_t       lockedState;
    bool          initSucceeded;
    uint8_t       resetFlag;

    ReadOnlyGattCharacteristic<uint8_t>        lockedStateChar;
    WriteOnlyGattCharacteristic<Lock_t>        lockChar;
    GattCharacteristic                         uriDataChar;
    WriteOnlyGattCharacteristic<Lock_t>        unlockChar;
    ReadWriteGattCharacteristic<uint8_t>       flagsChar;
    ReadWriteGattCharacteristic<PowerLevels_t> advPowerLevelsChar;
    ReadWriteGattCharacteristic<uint8_t>       txPowerModeChar;
    ReadWriteGattCharacteristic<uint16_t>      beaconPeriodChar;
    WriteOnlyGattCharacteristic<uint8_t>       resetChar;

  public:
    /*
     *  Encode a human-readable URI into the binary format defined by URIBeacon spec (https://github.com/google/uribeacon/tree/master/specification).
     */
    static void encodeURI(const char *uriDataIn, UriData_t uriDataOut, size_t &sizeofURIDataOut) {
        const char *prefixes[] = {
            "http://www.",
            "https://www.",
            "http://",
            "https://",
            "urn:uuid:"
        };
        const size_t NUM_PREFIXES = sizeof(prefixes) / sizeof(char *);
        const char *suffixes[] = {
            ".com/",
            ".org/",
            ".edu/",
            ".net/",
            ".info/",
            ".biz/",
            ".gov/",
            ".com",
            ".org",
            ".edu",
            ".net",
            ".info",
            ".biz",
            ".gov"
        };
        const size_t NUM_SUFFIXES = sizeof(suffixes) / sizeof(char *);

        sizeofURIDataOut = 0;
        memset(uriDataOut, 0, sizeof(UriData_t));

        if ((uriDataIn == NULL) || (strlen(uriDataIn) == 0)) {
            return;
        }

        /*
         * handle prefix
         */
        for (unsigned i = 0; i < NUM_PREFIXES; i++) {
            size_t prefixLen = strlen(prefixes[i]);
            if (strncmp(uriDataIn, prefixes[i], prefixLen) == 0) {
                uriDataOut[sizeofURIDataOut++]  = i;
                uriDataIn                      += prefixLen;
                break;
            }
        }

        /*
         * handle suffixes
         */
        while (*uriDataIn && (sizeofURIDataOut < URI_DATA_MAX)) {
            /* check for suffix match */
            unsigned i;
            for (i = 0; i < NUM_SUFFIXES; i++) {
                size_t suffixLen = strlen(suffixes[i]);
                if (strncmp(uriDataIn, suffixes[i], suffixLen) == 0) {
                    uriDataOut[sizeofURIDataOut++]  = i;
                    uriDataIn                      += suffixLen;
                    break; /* from the for loop for checking against suffixes */
                }
            }
            /* This is the default case where we've got an ordinary character which doesn't match a suffix. */
            if (i == NUM_SUFFIXES) {
                uriDataOut[sizeofURIDataOut++] = *uriDataIn;
                ++uriDataIn;
            }
        }
    }
};

#endif  // SERVICES_URIBEACONCONFIGSERVICE_H_