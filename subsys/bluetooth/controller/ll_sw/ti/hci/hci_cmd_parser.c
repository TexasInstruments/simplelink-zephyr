/******************************************************************************

 @file  hci_CMD_Parser.c

 @brief This file contains the Host Controller Interface (HCI) command parser.

 Group: WCS, BTS
 $Target Device: DEVICES $

 ******************************************************************************
 $License: TI_TEXT 2024 $
 ******************************************************************************
 $Release Name: PACKAGE NAME $
 $Release Date: PACKAGE RELEASE DATE $
 *****************************************************************************/

/*******************************************************************************
 * INCLUDES
 */

#include "hci_tl.h"
#include "hci.h"
#include "icall_ble_api.h"
#include "map_direct.h"

/*******************************************************************************
 * CONSTANTS
 */

#define HCI_CMD_PACKET_TYPE_OFFSET  0
#define HCI_CMD_OPCODE_LO_OFFSET    1
#define HCI_CMD_OPCODE_HI_OFFSET    2
#define HCI_CMD_DATA_OFFSET         4
#define HCI_CMD_NUM_OF_PARSERS      16

/*******************************************************************************
 * MACROS
 */

/*******************************************************************************
 * TYPEDEFS
 */

typedef hciStatus_t (*hciCmdParsers)(uint8 *pData, uint16 cmdOpCode);

/*******************************************************************************
 * LOCAL FUNCTIONS PROTOTYPES
 */

hciStatus_t hciCmdParserLegacy( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserCommon( uint8 *pData, uint16 cmdOpCode );

/**************************************************************
 *         Parsers function for specific role/feature         *
 **************************************************************/
hciStatus_t hciCmdParserConnection( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserAdvertiser( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserInitiator( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserPeripheral( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserPeriodicAdv( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserPeriodicScan( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserCte( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserChannelSounding( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserHost( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserVendorSpecificCommon( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserVendorSpecificConnection( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserVendorSpecificInitiator( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserVendorSpecificPeripheral( uint8 *pData, uint16 cmdOpCode );
hciStatus_t hciCmdParserVendorSpecificBroadcaster( uint8 *pData, uint16 cmdOpCode );

/**************************************************************
 *         Parsers function for a specific opcode             *
 **************************************************************/

/*
  Dedicated parser function is implemented only for opcodes that
  have multiple arguments and require special handling.
*/
static inline hciStatus_t hciCmdParserHostBufferSize( uint8 *pData );
static inline hciStatus_t hciCmdParserHostNumCompletedPkt( uint8 *pData );
static inline hciStatus_t hciCmdParserReadAuthPayloadTimeout( uint8 *pData );
static inline hciStatus_t hciCmdParserWriteAuthPayloadTimeout( uint8 *pData );
static inline hciStatus_t hciCmdParserConnUpdate( uint8 *pData );
static inline hciStatus_t hciCmdParserEncrypt( uint8 *pData );
static inline hciStatus_t hciCmdParserStartEncypt( uint8 *pData );
static inline hciStatus_t hciCmdParserLtkReqReply( uint8 *pData );
static inline hciStatus_t hciCmdParserRemoteConnParamReqReply( uint8 *pData );
static inline hciStatus_t hciCmdParserRemoteConnParamReqNegReply( uint8 *pData );
static inline hciStatus_t hciCmdParserSetDataLen( uint8 *pData );
static inline hciStatus_t hciCmdParserWriteSuggestedDefaultDataLen( uint8 *pData );
static inline hciStatus_t hciCmdParserAddDeviceToResolvingList( uint8 *pData );
static inline hciStatus_t hciCmdParserSetPrivacyMode( uint8 *pData );
static inline hciStatus_t hciCmdParserSetPhy( uint8 *pData );
static inline hciStatus_t hciCmdParserPeriodicAdvCreateSync( uint8 *pData );
static inline hciStatus_t hciCmdParserAddDeviceToPeriodicAdvList( uint8 *pData );
static inline hciStatus_t hciCmdParserRemoveDeviceFromPeriodicAdvList( uint8 *pData );

/*******************************************************************************
 * GLOBAL VARIABLES
 */

/*
  That parser array is iterated by HCI_CMD_Parser to find a match
  with the input packet�s opcode. Part of the parser functions is
  called with MAP_ that parser functions can be optimized with rom_init.c
  hooks based on project defines.
*/
hciCmdParsers hciCmdParsersArray[HCI_CMD_NUM_OF_PARSERS] = { MAP_hciCmdParserLegacy,
                                                             hciCmdParserCommon,
                                                             MAP_hciCmdParserConnection,
                                                             MAP_hciCmdParserAdvertiser,
                                                             MAP_hciCmdParserInitiator,
                                                             MAP_hciCmdParserPeripheral,
                                                             MAP_hciCmdParserPeriodicAdv,
                                                             MAP_hciCmdParserPeriodicScan,
                                                             hciCmdParserCte,
                                                             MAP_hciCmdParserChannelSounding,
                                                             MAP_hciCmdParserHost,
                                                             hciCmdParserVendorSpecificCommon,
                                                             MAP_hciCmdParserVendorSpecificConnection,
                                                             MAP_hciCmdParserVendorSpecificInitiator,
                                                             MAP_hciCmdParserVendorSpecificPeripheral,
                                                             MAP_hciCmdParserVendorSpecificBroadcaster,
};

/*******************************************************************************
 * API FUNCTIONS
 */

/*******************************************************************************
 * @fn          HCI_CMD_Parser
 *
 * @brief       This API is called by an external or internal host to send
 *              a raw HCI packet command to the controller. The API runs on all
 *              parser functions and looks for opcode matches. Inside the parser
 *              function, a call is made to the HCI function, which returns the
 *              HCI status or HCI_ERROR_CODE_UNKNOWN_HCI_CMD if the opcode
 *              is not found.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI/LL status
 *              HCI_ERROR_CODE_UNKNOWN_HCI_CMD.
 *
 */
hciStatus_t HCI_CMD_Parser( uint8 *pData )
{
  uint8 packetType;
  uint16 cmdOpCode;
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  // Retrieve packet type
  packetType = pData[0];

  // Sanity check
  if ( packetType == HCI_CMD_PACKET )
  {
    // Retrieve command opcode
    cmdOpCode = BUILD_UINT16( pData[HCI_CMD_OPCODE_LO_OFFSET],
                              pData[HCI_CMD_OPCODE_HI_OFFSET] );

    pData += HCI_CMD_DATA_OFFSET;

    for ( uint8 i = 0; i < (uint8)HCI_CMD_NUM_OF_PARSERS; i++ )
    {
      status = hciCmdParsersArray[i](pData, cmdOpCode);

      if ( status != (hciStatus_t)HCI_ERROR_CODE_UNKNOWN_HCI_CMD )
      {
        // Found match!
        break;
      }
    }
  }

  return status;
}

/*******************************************************************************
 * LOCAL FUNCTIONS
 */

/*******************************************************************************
 * @fn          hciCmdParserLegacy
 *
 * @brief       This CMD parser function is called first. If LEGACY_CMD is
 *              enabled, the service API MAP_checkLegacyHCICmdStatus() is called
 *              to check if the opcode is allowed in Legacy mode. If the opcode
 *              is not allowed, it will return HCI_ERROR_CODE_CMD_DISALLOWED.
 *              Otherwise, it will return HCI_ERROR_CODE_UNKNOWN_HCI_CMD for
 *              further handling in the next parser function.
 *
 * input parameters
 *
 * @param       pData     - Pointer to packet's data.
 *              cmdOpCode - Packet's HCI command opcode
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI_ERROR_CODE_CMD_DISALLOWED,
 *              HCI_ERROR_CODE_UNKNOWN_HCI_CMD.
 *
 */
hciStatus_t hciCmdParserLegacy( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

#ifdef LEGACY_CMD

  // Check if a legacy/extended command mixing is allowed
  if ( MAP_checkLegacyHCICmdStatus( cmdOpCode ) == FAILURE )
  {
    status = HCI_ERROR_CODE_CMD_DISALLOWED;
    HCI_CommandCompleteEvent( cmdOpCode, sizeof ( status ), &status );
  }

#endif

  return status;
}

/*******************************************************************************
 * @fn          hciCmdParserCommon
 *
 * @brief       This CMD parser function includes all opcodes that are common
 *              for all BLE roles
 *
 * input parameters
 *
 * @param       pData     - Pointer to packet's data.
 *              cmdOpCode - Packet's HCI command opcode
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI/LL status
 *              HCI_ERROR_CODE_UNKNOWN_HCI_CMD.
 *
 */
hciStatus_t hciCmdParserCommon( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_SET_EVENT_MASK:
    {
      // Function input: uint8 *pMask
      status = HCI_SetEventMaskCmd( pData );
      break;
    }
    case HCI_RESET:
    {
      status = HCI_ResetCmd( );
      break;
    }
    case HCI_SET_EVENT_MASK_PAGE_2:
    {
      // Function input: uint8 *pMask
      status = HCI_SetEventMaskPage2Cmd( pData );
      break;
    }
    case HCI_READ_LOCAL_VERSION_INFO:
    {
      status = HCI_ReadLocalVersionInfoCmd( );
      break;
    }
    case HCI_READ_LOCAL_SUPPORTED_COMMANDS:
    {
      status = HCI_ReadLocalSupportedCommandsCmd( );
      break;
    }
    case HCI_READ_LOCAL_SUPPORTED_FEATURES:
    {
      status = HCI_ReadLocalSupportedFeaturesCmd( );
      break;
    }
    case HCI_READ_BDADDR:
    {
      status = HCI_ReadBDADDRCmd( );
      break;
    }
    case HCI_READ_RSSI:
    {
      // Function input: uint16 connHandle
      status = HCI_ReadRssiCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_SET_EVENT_MASK:
    {
      // Function input: uint8 *pEventMask
      status = HCI_LE_SetEventMaskCmd( pData );
      break;
    }
    case HCI_LE_READ_BUFFER_SIZE:
    {
      status = HCI_LE_ReadBufSizeCmd( );
      break;
    }
    case HCI_LE_READ_LOCAL_SUPPORTED_FEATURES:
    {
      status = HCI_LE_ReadLocalSupportedFeaturesCmd( );
      break;
    }
    case HCI_LE_SET_RANDOM_ADDR:
    {
      // Function input: uint8 *pRandAddr
      status = HCI_LE_SetRandomAddressCmd( pData );
      break;
    }
    case HCI_LE_READ_ACCEPT_LIST_SIZE:
    {
      status = HCI_LE_ReadAcceptListSizeCmd( );
      break;
    }
    case HCI_LE_CLEAR_ACCEPT_LIST:
    {
      status = HCI_LE_ClearAcceptListCmd( );
      break;
    }
    case HCI_LE_ADD_ACCEPT_LIST:
    {
      // Function inputs: uint8 addrType, uint8 *devAddr
      status = HCI_LE_AddAcceptListCmd( pData[0], &pData[1] );
      break;
    }
    case HCI_LE_REMOVE_ACCEPT_LIST:
    {
      // Function inputs: uint8 addrType, uint8 *devAddr
      status = HCI_LE_RemoveAcceptListCmd( pData[0], &pData[1] );
      break;
    }
    case HCI_LE_ENCRYPT:
    {
      // Call for dedicated parser function
      status = hciCmdParserEncrypt( pData );
      break;
    }
    case HCI_LE_RAND:
    {
      status = HCI_LE_RandCmd( );
      break;
    }
    case HCI_LE_READ_SUPPORTED_STATES:
    {
      status = HCI_LE_ReadSupportedStatesCmd( );
      break;
    }
    case HCI_LE_RECEIVER_TEST:
    {
      // Function input: uint8 rxChan
      status = HCI_LE_ReceiverTestCmd( pData[0] );
      break;
    }
    case HCI_LE_TRANSMITTER_TEST:
    {
      // Function inputs: uint8 txChan, uint8 dataLen, uint8 payloadType
      status = HCI_LE_TransmitterTestCmd( pData[0], pData[1], pData[2] );
      break;
    }
    case HCI_LE_TEST_END:
    {
      status = HCI_LE_TestEndCmd( );
      break;
    }
    case HCI_LE_READ_LOCAL_P256_PUBLIC_KEY:
    {
      status = HCI_LE_ReadLocalP256PublicKeyCmd( );
      break;
    }
    case HCI_LE_GENERATE_DHKEY:
    {
      // Function input: uint8 *publicKey
      status = HCI_LE_GenerateDHKeyCmd( pData );
      break;
    }
    case HCI_LE_ADD_DEVICE_TO_RESOLVING_LIST:
    {
      // Call for dedicated parser function
      status = hciCmdParserAddDeviceToResolvingList( pData );
      break;
    }
    case HCI_LE_REMOVE_DEVICE_FROM_RESOLVING_LIST:
    {
      // Function inputs: uint8 peerIdAddrType, uint8 *peerIdAddr
      status = HCI_LE_RemoveDeviceFromResolvingListCmd( pData[0], &pData[1] );
      break;
    }
    case HCI_LE_CLEAR_RESOLVING_LIST:
    {
      status = HCI_LE_ClearResolvingListCmd( );
      break;
    }
    case HCI_LE_READ_RESOLVING_LIST_SIZE:
    {
      status = HCI_LE_ReadResolvingListSizeCmd( );
      break;
    }
    case HCI_LE_READ_PEER_RESOLVABLE_ADDRESS:
    {
      // Function inputs: uint8 peerIdAddrType, uint8 *peerIdAddr
      status = HCI_LE_ReadPeerResolvableAddressCmd( pData[0], &pData[1] );
      break;
    }
    case HCI_LE_READ_LOCAL_RESOLVABLE_ADDRESS:
    {
      // Function inputs: uint8 localIdAddrType, uint8 *localIdAddr
      status = HCI_LE_ReadLocalResolvableAddressCmd( pData[0], &pData[1] );
      break;
    }
    case HCI_LE_SET_ADDRESS_RESOLUTION_ENABLE:
    {
      // Function inputs: uint8 addrResolutionEnable
      status = HCI_LE_SetAddressResolutionEnableCmd( pData[0] );
      break;
    }
    case HCI_LE_SET_RESOLVABLE_PRIVATE_ADDRESS_TIMEOUT:
    {
      // Function input: uint16 rpaTimeout
      status = HCI_LE_SetResolvablePrivateAddressTimeoutCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_SET_PRIVACY_MODE:
    {
      // Call for dedicated parser function
      status = hciCmdParserSetPrivacyMode( pData );
      break;
    }
    case HCI_LE_READ_TX_POWER:
    {
      status = HCI_LE_ReadTxPowerCmd( );
      break;
    }
    case HCI_LE_READ_RF_PATH_COMPENSATION:
    {
      status = HCI_LE_ReadRfPathCompCmd( );
      break;
    }
    case HCI_LE_WRITE_RF_PATH_COMPENSATION:
    {
      // Function input: int16 txPathParam, int16 rxPathParam
      status = HCI_LE_WriteRfPathCompCmd( BUILD_INT16( pData[0], pData[1] ),
                                          BUILD_INT16( pData[2], pData[3] ) );
      break;
    }
    case HCI_LE_GENERATE_DHKEY_V2:
    {
      // Function input: uint8 *publicKey, uint8 keyType
      status = HCI_LE_GenerateDHKeyV2Cmd( pData, pData[LL_SC_P256_KEY_LEN] );
      break;
    }
    case HCI_WRITE_LOCAL_NAME:
    case HCI_READ_SCAN_ENABLE:
    case HCI_WRITE_SCAN_ENABLE:
    case HCI_LE_SET_ADV_PARAM:
    case HCI_LE_SET_ADV_DATA:
    case HCI_LE_SET_SCAN_RSP_DATA:
    case HCI_LE_SET_ADV_ENABLE:
    case HCI_LE_SET_SCAN_PARAM:
    case HCI_LE_SET_SCAN_ENABLE:
    case HCI_LE_CREATE_CONNECTION:
    {
      status = HCI_ERROR_CODE_CMD_DISALLOWED;
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}


/**************************************************************
 *         Parsers function for specific role/feature         *
 **************************************************************
 *
 * @brief       The following CMD parser functions are
 *              dedicated for opcodes that are relevant for role
 *              or features based on pre-defined configurations
 *              If role/feature defined is enabled,
 *              the parser function will link in with
 *              rom_init patch and go into the inner
 *              function implementation
 * input parameters
 *
 * @param       pData     - Pointer to packet's data.
 *              cmdOpCode - Packet's HCI command opcode
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI/LL status
 *              HCI_ERROR_CODE_UNKNOWN_HCI_CMD.
 *
 */
hciStatus_t hciCmdParserConnection( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_DISCONNECT:
    {
      // Function input: uint16 connHandle, uint8 reason
      status = HCI_DisconnectCmd( BUILD_UINT16( pData[0], pData[1] ), pData[2] );
      break;
    }
    case HCI_READ_REMOTE_VERSION_INFO:
    {
      // Function input: uint16 connHandle
      status = HCI_ReadRemoteVersionInfoCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_READ_TRANSMIT_POWER:
    {
      // Function input: uint16 connHandle, uint8 txPwrType
      status = HCI_ReadTransmitPowerLevelCmd( BUILD_UINT16( pData[0], pData[1] ),
                                              pData[2] );
      break;
    }
    case HCI_SET_CONTROLLER_TO_HOST_FLOW_CONTROL:
    {
      // Function inputs: uint8 flowControlEnable
      status = HCI_SetControllerToHostFlowCtrlCmd( pData[0] );
      break;
    }
    case HCI_HOST_BUFFER_SIZE:
    {
      // Call for dedicated parser function
      status = hciCmdParserHostBufferSize( pData );
      break;
    }
    case HCI_HOST_NUM_COMPLETED_PACKETS:
    {
      // Call for dedicated parser function
      status = hciCmdParserHostNumCompletedPkt( pData );
      break;
    }
    case HCI_READ_AUTH_PAYLOAD_TIMEOUT:
    {
      // Call for dedicated parser function
      status = hciCmdParserReadAuthPayloadTimeout( pData );
      break;
    }
    case HCI_WRITE_AUTH_PAYLOAD_TIMEOUT:
    {
      // Call for dedicated parser function
      status = hciCmdParserWriteAuthPayloadTimeout( pData );
      break;
    }
    case HCI_LE_CONNECTION_UPDATE:
    {
      // Call for dedicated parser function
      status = hciCmdParserConnUpdate( pData );
      break;
    }
    case HCI_LE_READ_CHANNEL_MAP:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_ReadChannelMapCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_READ_REMOTE_USED_FEATURES:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_ReadRemoteUsedFeaturesCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_REMOTE_CONN_PARAM_REQ_REPLY:
    {
      // Call for dedicated parser function
      status = hciCmdParserRemoteConnParamReqReply( pData );
      break;
    }
    case HCI_LE_REMOTE_CONN_PARAM_REQ_NEG_REPLY:
    {
      // Call for dedicated parser function
      status = hciCmdParserRemoteConnParamReqNegReply( pData );
      break;
    }
    case HCI_LE_SET_DATA_LENGTH:
    {
      // Call for dedicated parser function
      status = hciCmdParserSetDataLen( pData );
      break;
    }
    case HCI_LE_READ_SUGGESTED_DEFAULT_DATA_LENGTH:
    {
      status = HCI_LE_ReadSuggestedDefaultDataLenCmd( );
      break;
    }
    case HCI_LE_WRITE_SUGGESTED_DEFAULT_DATA_LENGTH:
    {
      // Call for dedicated parser function
      status = hciCmdParserWriteSuggestedDefaultDataLen( pData );
      break;
    }
    case HCI_LE_READ_MAX_DATA_LENGTH:
    {
      status = HCI_LE_ReadMaxDataLenCmd( );
      break;
    }
    case HCI_LE_READ_PHY:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_ReadPhyCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_SET_DEFAULT_PHY:
    {
      // Function input: uint8 allPhys, uint8 txPhy, uint8 rxPhy
      status = HCI_LE_SetDefaultPhyCmd( pData[0], pData[1], pData[2] );
      break;
    }
    case HCI_LE_SET_PHY:
    {
      // Call for dedicated parser function
      status = hciCmdParserSetPhy( pData );
      break;
    }
    case HCI_LE_ENHANCED_RECEIVER_TEST:
    {
      // Function input: uint8 rxChan, uint8 rxPhy, uint8 modIndex
      status = HCI_LE_EnhancedRxTestCmd( pData[0], pData[1], pData[2] );
      break;
    }
    case HCI_LE_ENHANCED_TRANSMITTER_TEST:
    {
      // Function input: uint8 txChan, uint8 payloadLen, uint8 payloadType, uint8 txPhy
      status = HCI_LE_EnhancedTxTestCmd( pData[0], pData[1], pData[2], pData[3] );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserAdvertiser( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  if ( cmdOpCode == (uint16)HCI_LE_READ_ADV_CHANNEL_TX_POWER )
  {
    status = HCI_LE_ReadAdvChanTxPowerCmd( );
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserInitiator( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_LE_CREATE_CONNECTION_CANCEL:
    {
      status = HCI_LE_CreateConnCancelCmd( );
      break;
    }
    case HCI_LE_SET_HOST_CHANNEL_CLASSIFICATION:
    {
      // Function input: uint8 *chanMap
      status = HCI_LE_SetHostChanClassificationCmd( pData );
      break;
    }
    case HCI_LE_START_ENCRYPTION:
    {
      // Call for dedicated parser function
      status = hciCmdParserStartEncypt( pData );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserPeripheral( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_LE_LTK_REQ_REPLY:
    {
      // Call for dedicated parser function
      status = hciCmdParserLtkReqReply( pData );
      break;
    }
    case HCI_LE_LTK_REQ_NEG_REPLY:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_LtkReqNegReplyCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserPeriodicAdv( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_LE_SET_PERIODIC_ADV_PARAMETERS:
    {
      // Function input: uint8 advHandle, uint16 periodicAdvIntervalMin, uint16 periodicAdvIntervalMax, uint16 periodicAdvProp
      status = HCI_LE_SetPeriodicAdvParamsCmd( pData[0],
                                               BUILD_UINT16( pData[1], pData[2] ),
                                               BUILD_UINT16( pData[3], pData[4] ),
                                               BUILD_UINT16( pData[5], pData[6] ) );
      break;
    }
    case HCI_LE_SET_PERIODIC_ADV_DATA:
    {
      // Function input: uint8 advHandle, uint8 operation, uint8 dataLength, uint8* data
      status = HCI_LE_SetPeriodicAdvDataCmd( pData[0], pData[1], pData[2], &pData[3] );
      break;
    }
    case HCI_LE_SET_PERIODIC_ADV_ENABLE:
    {
      // Function input: uint8 enable, uint8 advHandle
      status = HCI_LE_SetPeriodicAdvEnableCmd( pData[0], pData[1] );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserPeriodicScan( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_LE_PERIODIC_ADV_CREATE_SYNC:
    {
      // Call for dedicated parser function
      status = hciCmdParserPeriodicAdvCreateSync( pData );
      break;
    }
    case HCI_LE_PERIODIC_ADV_CREATE_SYNC_CANCEL:
    {
      status = HCI_LE_PeriodicAdvCreateSyncCancelCmd( );
      break;
    }
    case HCI_LE_PERIODIC_ADV_TERMINATE_SYNC:
    {
      // Function input: uint16 syncHandle
      status = HCI_LE_PeriodicAdvTerminateSyncCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_ADD_DEVICE_TO_PERIODIC_ADV_LIST:
    {
      // Call for dedicated parser function
      status = hciCmdParserAddDeviceToPeriodicAdvList( pData );
      break;
    }
    case HCI_LE_REMOVE_DEVICE_FROM_PERIODIC_ADV_LIST:
    {
      // Call for dedicated parser function
      status = hciCmdParserRemoveDeviceFromPeriodicAdvList( pData );
      break;
    }
    case HCI_LE_CLEAR_PERIODIC_ADV_LIST:
    {
      status = HCI_LE_ClearPeriodicAdvListCmd( );
      break;
    }
    case HCI_LE_READ_PERIODIC_ADV_LIST_SIZE:
    {
      status = HCI_LE_ReadPeriodicAdvListSizeCmd( );
      break;
    }
    case HCI_LE_SET_PERIODIC_ADV_RECEIVE_ENABLE:
    {
      // Function input: uint16 syncHandle, uint8 enable
      status = HCI_LE_SetPeriodicAdvReceiveEnableCmd( BUILD_UINT16( pData[0], pData[1] ),
                                                      pData[2] );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserCte( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  // CTE opcode currently disallowed
  // List of CTE opcodes and HCI functions:
  // HCI_LE_ENHANCED_CTE_RECEIVER_TEST: HCI_LE_EnhancedCteRxTestCmd
  // HCI_LE_ENHANCED_CTE_TRANSMITTER_TEST: HCI_LE_EnhancedCteTxTestCmd
  // HCI_LE_SET_CONNECTIONLESS_CTE_TRANSMIT_PARAMS: HCI_LE_SetConnectionlessCteTransmitParamsCmd
  // HCI_LE_SET_CONNECTIONLESS_CTE_TRANSMIT_ENABLE:  HCI_LE_SetConnectionlessCteTransmitEnableCmd
  // HCI_LE_SET_CONNECTIONLESS_IQ_SAMPLING_ENABLE: HCI_LE_SetConnectionlessIqSamplingEnableCmd
  // HCI_LE_SET_CONNECTION_CTE_RECEIVE_PARAMS: HCI_LE_SetConnectionCteReceiveParamsCmd
  // HCI_LE_SET_CONNECTION_CTE_TRANSMIT_PARAMS: HCI_LE_SetConnectionCteTransmitParamsCmd
  // HCI_LE_SET_CONNECTION_CTE_REQUEST_ENABLE: HCI_LE_SetConnectionCteRequestEnableCmd
  // HCI_LE_SET_CONNECTION_CTE_RESPONSE_ENABLE: HCI_LE_SetConnectionCteResponseEnableCmd
  // HCI_LE_READ_ANTENNA_INFORMATION: HCI_LE_ReadAntennaInformationCmd
  if ( cmdOpCode >= (uint16) HCI_LE_ENHANCED_CTE_RECEIVER_TEST
      && cmdOpCode <= (uint16) HCI_LE_READ_ANTENNA_INFORMATION )
  {
    status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserChannelSounding( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_LE_CS_READ_LOCAL_SUPPORTED_CAPABILITIES:
    {
      status = HCI_LE_CS_ReadLocalSupportedCapabilities( );
      break;
    }
    case HCI_LE_CS_READ_REMOTE_SUPPORTED_CAPABILITIES:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_CS_ReadRemoteSupportedCapabilities( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_CS_SECURITY_ENABLE:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_CS_SecurityEnable( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_CS_SET_DEFAULT_SETTINGS:
    {
      // Function input: uint16 connHandle, uint8 roleEnable, uint8 csSyncAntennaSelection, int8 maxTxPower
      status = HCI_LE_CS_SetDefaultSettings( BUILD_UINT16( pData[0], pData[1] ), pData[2],
                                             pData[3], (int8 )pData[4] );
      break;
    }
    case HCI_LE_CS_READ_REMOTE_FAE_TABLE:
    {
      // Function input: uint16 connHandle
      status = HCI_LE_CS_ReadRemoteFAETable( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_LE_CS_WRITE_REMOTE_FAE_TABLE:
    {
      // Function input: uint16 connHandle, void* reflectorFaeTable
      status = HCI_LE_CS_WriteRemoteFAETable( BUILD_UINT16( pData[0], pData[1] ),
                                              (void* )&pData[2] );
      break;
    }
    case HCI_LE_CS_CREATE_CONFIG:
    {
      // Function input: uint16 connHandle, uint8 configID, uint8 createContext, uint8* pBufConfig
      status = HCI_LE_CS_CreateConfig( BUILD_UINT16( pData[0], pData[1] ), pData[2],
                                       pData[3], &pData[4] );
      break;
    }
    case HCI_LE_CS_REMOVE_CONFIG:
    {
      // Function input: uint16 connHandle, uint8 configID
      status = HCI_LE_CS_RemoveConfig( BUILD_UINT16( pData[0], pData[1] ), pData[2] );
      break;
    }
    case HCI_LE_CS_SET_CHANNEL_CLASSIFICATION:
    {
      // Function input: uint8 channelClassification
      status = HCI_LE_CS_SetChannelClassification( pData );
      break;
    }
    case HCI_LE_CS_SET_PROCEDURE_PARAMS:
    {
      // Function input: uint16 connHandle, uint8 configID, uint8* pParams
      status = HCI_LE_CS_SetProcedureParameters( BUILD_UINT16( pData[0], pData[1] ),
                                                 pData[2], &pData[3] );
      break;
    }
    case HCI_LE_CS_PROCEDURE_ENABLE:
    {
      // Function input: uint16 connHandle, uint8 enable, uint8 configID
      status = HCI_LE_CS_ProcedureEnable( BUILD_UINT16( pData[0], pData[1] ), pData[2],
                                          pData[3] );
      break;
    }
    case HCI_LE_CS_TEST:
    {
      status = HCI_LE_CS_Test( pData );
      break;
    }
    case HCI_LE_CS_TEST_END:
    {
      status = HCI_LE_CS_TestEnd( );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserHost( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  if ( cmdOpCode == (uint16)HCI_LE_SET_HOST_FEATURE )
  {
    // Function input: uint8 bitNumber, uint8 bitValue
    status = HCI_LE_SetHostFeature( pData[0], pData[1] );
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserVendorSpecificCommon( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_EXT_SET_RX_GAIN:
    {
      // Function input: uint8 rxGain
      status = HCI_EXT_SetRxGainCmd( pData[0] );
      break;
    }
    case HCI_EXT_DECRYPT:
    {
      // Function input: uint8 *key, uint8 *encText
      status = HCI_EXT_DecryptCmd( pData, &pData[KEYLEN] );
      break;
    }
    case HCI_EXT_SET_LOCAL_SUPPORTED_FEATURES:
    {
      // Function input: uint8 *localFeatures
      status = HCI_EXT_SetLocalSupportedFeaturesCmd( pData );
      break;
    }
    case HCI_EXT_MODEM_TEST_TX:
    {
      // Function input: uint8 cwMode, uint8 txChan
      status = HCI_EXT_ModemTestTxCmd( pData[0], pData[1] );
      break;
    }
    case HCI_EXT_MODEM_TEST_RX:
    {
      // Function input: uint8 rxChan
      status = HCI_EXT_ModemTestRxCmd( pData[0] );
      break;
    }
    case HCI_EXT_END_MODEM_TEST:
    {
      status = HCI_EXT_EndModemTestCmd( );
      break;
    }
    case HCI_EXT_SET_BDADDR:
    {
      // Function input: uint8 *bdAddr
      status = HCI_EXT_SetBDADDRCmd( pData );
      break;
    }
    case HCI_EXT_ENABLE_PTM:
    {
      status = HCI_EXT_EnablePTMCmd( );
      break;
    }
    case HCI_EXT_SET_FREQ_TUNE:
    {
      // Function input: uint8 step
      status = HCI_EXT_SetFreqTuneCmd( pData[0] );
      break;
    }
    case HCI_EXT_SAVE_FREQ_TUNE:
    {
      status = HCI_EXT_SaveFreqTuneCmd( );
      break;
    }
    case HCI_EXT_MAP_PM_IO_PORT:
    {
      // Function input: uint8 ioPort, uint8 ioPin
      status = HCI_EXT_MapPmIoPortCmd( pData[0], pData[1] );
      break;
    }
    case HCI_EXT_EXTEND_RF_RANGE:
    {
      status = LL_STATUS_ERROR_COMMAND_DISALLOWED;
      break;
    }
    case HCI_EXT_HALT_DURING_RF:
    {
      // Function input: uint8 mode
      status = LL_STATUS_ERROR_COMMAND_DISALLOWED;
      break;
    }
    case HCI_EXT_RESET_SYSTEM:
    {
      // Function input: uint8 mode
      status = HCI_EXT_ResetSystemCmd( pData[0] );
      break;
    }
    case HCI_EXT_SET_DTM_TX_PKT_CNT:
    {
      // Function input: uint16 txPktCnt
      status = HCI_EXT_SetDtmTxPktCntCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_EXT_ENHANCED_MODEM_TEST_TX:
    {
      // Function input: uint8 cwMode, uint8 rfPhy, uint8 rfChan
      status = HCI_EXT_EnhancedModemTestTxCmd( pData[0], pData[1], pData[2] );
      break;
    }
    case HCI_EXT_ENHANCED_MODEM_TEST_RX:
    {
      // Function input: uint8 rfPhy, uint8 rfChan
      status = HCI_EXT_EnhancedModemTestRxCmd( pData[0], pData[1] );
      break;
    }
    case HCI_EXT_SET_PIN_OUTPUT:
    {
      // Function input: uint8 dio, uint8 value
      status = HCI_EXT_SetPinOutputCmd( pData[0], pData[1] );
      break;
    }
    case HCI_EXT_SET_LOCATIONING_ACCURACY:
    {
      // Function input: uint16 handle, uint8 sampleRate1M, uint8 sampleSize1M,
      //                 uint8 sampleRate2M, uint8 sampleSize2M, uint8 sampleCtrl
      status = HCI_EXT_SetLocationingAccuracyCmd( BUILD_UINT16( pData[0], pData[1] ),
                                                  pData[2], pData[3],
                                                  pData[4], pData[5],
                                                  pData[6] );
      break;
    }
    case HCI_EXT_SET_TX_POWER_DBM:
    {
      // Function input: int8 txPower, uint8 fraction
      status = HCI_EXT_SetTxPowerDbmCmd( (int8 )pData[0], pData[1] );
      break;
    }
    case HCI_EXT_SET_MAX_DTM_TX_POWER_DBM:
    {
      // Function input: int8 txPower, uint8 fraction
      status = HCI_EXT_SetMaxDtmTxPowerDbmCmd( (int8 )pData[0], pData[1] );
      break;
    }
    case HCI_EXT_GET_RX_STATS:
    {
      // Function input: uint16 connHandle, uint8 command
      status = HCI_EXT_GetRxStatisticsCmd( BUILD_UINT16( pData[0], pData[1] ), pData[2] );
      break;
    }
    case HCI_EXT_GET_TX_STATS:
    {
      // Function input: uint16 connHandle, uint8 command
      status = HCI_EXT_GetTxStatisticsCmd( BUILD_UINT16( pData[0], pData[1] ), pData[2] );
      break;
    }
    case HCI_EXT_GET_COEX_STATS:
    {
      // Function input: uint8 command
      status = HCI_EXT_GetCoexStatisticsCmd( pData[0] );
      break;
    }
    case HCI_EXT_HOST_TO_CONTROLLER:
    {
      // needs more investigation
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
#ifdef LL_TEST_MODE
    case HCI_EXT_LL_TEST_MODE:
    {
      // Function input: uint8 testCase
      status = HCI_EXT_LLTestModeCmd( pData[0] );
      break;
    }
#endif // LL_TEST_MODE
    case HCI_EXT_SET_QOS_PARAMETERS:
    {
      // Function input: uint8 taskType, uint8 paramType, uint32 paramVal, uint16 taskHandle
      status = HCI_EXT_SetQOSParameters(
          pData[0], pData[1], BUILD_UINT32( pData[2], pData[3], pData[4], pData[5] ),
          BUILD_UINT16( pData[6], pData[7] ) );
      break;
    }
    case HCI_EXT_SET_QOS_DEFAULT_PARAMETERS:
    {
      // Function input: uint32 paramDefaultVal, uint8 paramType, uint8 taskType
      status = HCI_EXT_SetQOSDefaultParameters(BUILD_UINT32( pData[0], pData[1],
                                                             pData[2], pData[3] ),
                                               pData[4],
                                               pData[5] );
      break;
    }
    case HCI_EXT_ENHANCED_MODEM_HOP_TEST_TX:
    {
        status = HCI_EXT_EnhancedModemHopTestTxCmd( pData[0], pData[1], pData[2] );
        break;
    }
    // Opcodes not supported in LPF3
    case HCI_EXT_ONE_PKT_PER_EVT:
    case HCI_EXT_CLK_DIVIDE_ON_HALT:
    case HCI_EXT_DECLARE_NV_USAGE:
    case HCI_EXT_MODEM_HOP_TEST_TX:
    case HCI_EXT_DELAY_SLEEP: // Was HCI_ERROR_CODE_CMD_DISALLOWED
    case HCI_EXT_READ_RAND_ADDR:
    case HCI_EXT_COEX_ENABLE:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }

  }
  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserVendorSpecificConnection( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_EXT_SET_SCA:
    {
      // Function input: uint16 scaInPPM
      status = HCI_EXT_SetSCACmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_EXT_DISCONNECT_IMMED:
    {
      // Function input: uint16 connHandle
      status = HCI_EXT_DisconnectImmedCmd( BUILD_UINT16( pData[0], pData[1] ) );
      break;
    }
    case HCI_EXT_PER:
    {
      // Function input: uint16 connHandle, uint8 command
      status = HCI_EXT_PacketErrorRateCmd( BUILD_UINT16( pData[0], pData[1] ), pData[2] );
      break;
    }
    case HCI_EXT_OVERLAPPED_PROCESSING:
    {
      // Function input: uint8 mode
      status = HCI_EXT_OverlappedProcessingCmd( pData[0] );
      break;
    }
    case HCI_EXT_NUM_COMPLETED_PKTS_LIMIT:
    {
      // Function input: uint8 limit, uint8 flushOnEvt
      status = HCI_EXT_NumComplPktsLimitCmd( pData[0], pData[1] );
      break;
    }
    case HCI_EXT_GET_CONNECTION_INFO:
    {
      // NULL input is used because the function requires 3 pointers to be filled
      // if they are not NULL, or not used if they are NULL.
      status = HCI_EXT_GetConnInfoCmd( NULL,NULL,NULL );
      break;
    }
    case HCI_EXT_SET_MAX_DATA_LENGTH:
    {
      // Function input: uint16 txOctets, uint16 txTime, uint16 rxOctets, uint16 rxTime
      status = HCI_EXT_SetMaxDataLenCmd( BUILD_UINT16( pData[0], pData[1] ),
                                         BUILD_UINT16( pData[2], pData[3] ),
                                         BUILD_UINT16( pData[4], pData[5] ),
                                         BUILD_UINT16( pData[6], pData[7] ) );
      break;
    }
    case HCI_EXT_GET_ACTIVE_CONNECTION_INFO:
    {
      // Function input: uint8 connId, hciActiveConnInfo_t *activeConnInfo
      status = HCI_EXT_GetActiveConnInfoCmd( pData[0], &pData[1] );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserVendorSpecificInitiator( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_EXT_SET_HOST_DEFAULT_CHANNEL_CLASSIFICATION:
    {
      // Function input: uint8 *chanMap
      status = HCI_EXT_SetHostDefChanClassificationCmd( pData );
      break;
    }
    case HCI_EXT_SET_HOST_CONNECTION_CHANNEL_CLASSIFICATION:
    {
      // Function input: uint8 *chanMap , uint16 connID
      status = HCI_EXT_SetHostConnChanClassificationCmd(&pData[0],
                                                        BUILD_UINT16( pData[LL_NUM_BYTES_FOR_CHAN_MAP],
                                                                      pData[LL_NUM_BYTES_FOR_CHAN_MAP + 1] ) );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }

  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserVendorSpecificPeripheral( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  switch ( cmdOpCode )
  {
    case HCI_EXT_SET_FAST_TX_RESP_TIME:
    {
      // Function input: uint8 control
      status = HCI_EXT_SetFastTxResponseTimeCmd( pData[0] );
      break;
    }
    case HCI_EXT_OVERRIDE_PL:
    {
      // Function input: uint8 control
      status = HCI_EXT_SetPeripheralLatencyOverrideCmd( pData[0] );
      break;
    }
    case HCI_EXT_BUILD_REVISION:
    {
      // Function input: uint8 control, uint16 userRevNum
      status = HCI_EXT_BuildRevisionCmd( pData[0], BUILD_UINT16( pData[1], pData[2] ) );
      break;
    }
    default:
    {
      status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;
      break;
    }
  }
  return status;
}

/*
  For more details look for hciCmdParserConnection() headline
*/
hciStatus_t hciCmdParserVendorSpecificBroadcaster( uint8 *pData, uint16 cmdOpCode )
{
  hciStatus_t status = HCI_ERROR_CODE_UNKNOWN_HCI_CMD;

  if ( cmdOpCode == (uint16)HCI_EXT_LE_SET_EXT_VIRTUAL_ADV_ADDRESS )
  {
    // Function input: uint8 advHandle, uint8 *bdAddr
    status = HCI_EXT_SetVirtualAdvAddrCmd( pData[0], &pData[1] );
  }

  return status;
}

/**************************************************************
 *         Parsers function for a specific opcode             *
 **************************************************************/

/*******************************************************************************
 * @fn          hciCmdParserHostBufferSize
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_HostBufferSizeCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI_SUCCESS,
 *              HCI_ERROR_CODE_INVALID_HCI_CMD_PARAMS.
 */
static inline hciStatus_t hciCmdParserHostBufferSize( uint8 *pData )
{
  uint16 hostAclPktLen = BUILD_UINT16( pData[0], pData[1] );
  uint8 hostSyncPktLen = pData[2];
  uint16 hostTotalNumAclPkts = BUILD_UINT16( pData[3], pData[4] );
  uint16 hostTotalNumSyncPkts = BUILD_UINT16( pData[5], pData[6] );

  return HCI_HostBufferSizeCmd( hostAclPktLen, hostSyncPktLen, hostTotalNumAclPkts,
                                hostTotalNumSyncPkts );
}

/*******************************************************************************
 * @fn          hciCmdParserHostNumCompletedPkt
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_HostBufferSizeCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      HCI_SUCCESS,
 *              HCI_ERROR_CODE_INVALID_HCI_CMD_PARAMS.
 */
static inline hciStatus_t hciCmdParserHostNumCompletedPkt( uint8 *pData )
{
  uint8 numHandles = *pData;
  uint16 connHandles = BUILD_UINT16( pData[1], pData[2] );
  uint16 numCompletedPkts = BUILD_UINT16( pData[3], pData[4] );

  return HCI_HostNumCompletedPktCmd( numHandles, &connHandles, &numCompletedPkts );
}

/*******************************************************************************
 * @fn          hciCmdParserReadAuthPayloadTimeout
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_ReadAuthPayloadTimeoutCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION.
 */
static inline hciStatus_t hciCmdParserReadAuthPayloadTimeout( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );

  uint16 apto = 0; // NOT IN USE, JUST TO BE ALIGNED WITH THE FUNCTION API

  return HCI_ReadAuthPayloadTimeoutCmd( connHandle, &apto );
}

/*******************************************************************************
 * @fn          hciCmdParserWriteAuthPayloadTimeout
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_WriteAuthPayloadTimeoutCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION.
 */
static inline hciStatus_t hciCmdParserWriteAuthPayloadTimeout( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );

  uint16 aptoValue = BUILD_UINT16( pData[2], pData[3] );

  return HCI_WriteAuthPayloadTimeoutCmd( connHandle, aptoValue );
}

/*******************************************************************************
 * @fn          hciCmdParserConnUpdate
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_ConnUpdateCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION,
 *              LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE,
 *              LL_STATUS_ERROR_ILLEGAL_PARAM_COMBINATION.
 */
static inline hciStatus_t hciCmdParserConnUpdate( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );
  uint16 connIntervalMin = BUILD_UINT16( pData[2], pData[3] );
  uint16 connIntervalMax = BUILD_UINT16( pData[4], pData[5] );
  uint16 connLatency = BUILD_UINT16( pData[6], pData[7] );
  uint16 connTimeout = BUILD_UINT16( pData[8], pData[9] );
  uint16 minLen = BUILD_UINT16( pData[10], pData[11] );
  uint16 maxLen = BUILD_UINT16( pData[12], pData[13] );

  return HCI_LE_ConnUpdateCmd( connHandle, connIntervalMin, connIntervalMax, connLatency,
                               connTimeout, minLen, maxLen );
}

/*******************************************************************************
 * @fn          hciCmdParserEncrypt
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_EncryptCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER.
 */
static inline hciStatus_t hciCmdParserEncrypt( uint8 *pData )
{
  uint8 *key = pData;
  uint8 *plainText = &pData[KEYLEN];

  return HCI_LE_EncryptCmd( key, plainText );
}

/*******************************************************************************
 * @fn          hciCmdParserStartEncypt
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_StartEncyptCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED.
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED,
 *              LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE.
 */
static inline hciStatus_t hciCmdParserStartEncypt( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );

  pData += sizeof(uint16);

  uint8 *random = pData;

  pData += LL_ENC_RAND_LEN;

  uint8 *encDiv = pData;

  pData += LL_ENC_EDIV_LEN;

  uint8 *ltk = pData;

  return HCI_LE_StartEncyptCmd( connHandle, random, encDiv, ltk );
}

/*******************************************************************************
 * @fn          hciCmdParserLtkReqReply
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_LtkReqReplyCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER.
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED.
 */
static inline hciStatus_t hciCmdParserLtkReqReply( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );

  pData += sizeof(uint16);

  uint8 *ltk = pData;

  return HCI_LE_LtkReqReplyCmd( connHandle, ltk );
}

/*******************************************************************************
 * @fn          hciCmdParserRemoteConnParamReqReply
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_RemoteConnParamReqReplyCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION.
 */
static inline hciStatus_t hciCmdParserRemoteConnParamReqReply( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );
  uint16 connIntervalMin = BUILD_UINT16( pData[2], pData[3] );
  uint16 connIntervalMax = BUILD_UINT16( pData[4], pData[5] );
  uint16 connLatency = BUILD_UINT16( pData[6], pData[7] );
  uint16 connTimeout = BUILD_UINT16( pData[8], pData[9] );
  uint16 minLen = BUILD_UINT16( pData[10], pData[11] );
  uint16 maxLen = BUILD_UINT16( pData[12], pData[13] );

  return HCI_LE_RemoteConnParamReqReplyCmd( connHandle, connIntervalMin, connIntervalMax,
                                            connLatency, connTimeout, minLen, maxLen );
}

/*******************************************************************************
 * @fn          hciCmdParserRemoteConnParamReqNegReply
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_RemoteConnParamReqNegReplyCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_INACTIVE_CONNECTION.
 */
static inline hciStatus_t hciCmdParserRemoteConnParamReqNegReply( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );
  uint8 reason = pData[2];

  return HCI_LE_RemoteConnParamReqNegReplyCmd( connHandle, reason );
}

/*******************************************************************************
 * @fn          hciCmdParserSetDataLen
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_SetDataLenCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED,
 *              LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE,
 *              LL_STATUS_ERROR_CTRL_PROC_ALREADY_ACTIVE.
 */
static inline hciStatus_t hciCmdParserSetDataLen( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );
  uint16 txOctets = BUILD_UINT16( pData[2], pData[3] );
  uint16 txTime = BUILD_UINT16( pData[4], pData[5] );

  return HCI_LE_SetDataLenCmd( connHandle, txOctets, txTime );
}

/*******************************************************************************
 * @fn          hciCmdParserWriteSuggestedDefaultDataLen
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_WriteSuggestedDefaultDataLenCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED.
 */
static inline hciStatus_t hciCmdParserWriteSuggestedDefaultDataLen( uint8 *pData )
{
  uint16 txOctets = BUILD_UINT16( pData[0], pData[1] );
  uint16 txTime = BUILD_UINT16( pData[2], pData[3] );

  return HCI_LE_WriteSuggestedDefaultDataLenCmd( txOctets, txTime );
}

/*******************************************************************************
 * @fn          hciCmdParserAddDeviceToResolvingList
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_AddDeviceToResolvingListCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *              LL_STATUS_ERROR_INVALID_PARAMS,
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED,
 *              LL_STATUS_ERROR_OUT_OF_RESOLVING_LIST.
 */
static inline hciStatus_t hciCmdParserAddDeviceToResolvingList( uint8 *pData )
{
  uint8 peerIdAddrType = *pData++;
  uint8 *peerIdAddr = pData;

  pData += B_ADDR_LEN;

  uint8 *peerIRK = pData;

  pData += KEYLEN;

  uint8 *localIRK = pData;

  return HCI_LE_AddDeviceToResolvingListCmd( peerIdAddrType, peerIdAddr, peerIRK,
                                             localIRK );
}

/*******************************************************************************
 * @fn          hciCmdParserSetPrivacyMode
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_SetPrivacyModeCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *              LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE.
 *
 */
static inline hciStatus_t hciCmdParserSetPrivacyMode( uint8 *pData )
{
  uint8 peerIdAddrType = *pData++;
  uint8 *peerIdAddr = pData;

  pData += B_ADDR_LEN;

  uint8 privacyMode = *pData;

  return HCI_LE_SetPrivacyModeCmd( peerIdAddrType, peerIdAddr, privacyMode );
}

/*******************************************************************************
 * @fn          hciCmdParserSetPhy
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_SetPhyCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return      LL_STATUS_SUCCESS,
 *              LL_STATUS_ERROR_BAD_PARAMETER,
 *              LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *              LL_STATUS_ERROR_UNKNOWN_CONN_HANDLE,
 *              LL_STATUS_ERROR_FEATURE_NOT_SUPPORTED,
 *              LL_STATUS_ERROR_UNSUPPORTED_REMOTE_FEATURE.
 *
 */
static inline hciStatus_t hciCmdParserSetPhy( uint8 *pData )
{
  uint16 connHandle = BUILD_UINT16( pData[0], pData[1] );
  uint8  allPhys = pData[2];
  uint8  txPhy = pData[3];
  uint8  rxPhy = pData[4];
  uint16 phyOpts = BUILD_UINT16( pData[5], pData[6] );

  return HCI_LE_SetPhyCmd( connHandle, allPhys, txPhy, rxPhy, phyOpts );
}

/*******************************************************************************
 * @fn          hciCmdParserPeriodicAdvCreateSync
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_PeriodicAdvCreateSyncCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return        LL_STATUS_SUCCESS,
 *                LL_STATUS_ERROR_BAD_PARAMETER,
 *                LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *                LL_STATUS_ERROR_UNEXPECTED_STATE_ROLE,
 *                LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED,
 *                LL_STATUS_ERROR_CONNECTION_ALREADY_EXISTS,
 *
 */
static inline hciStatus_t hciCmdParserPeriodicAdvCreateSync( uint8 *pData )
{
  uint8 options = *pData++;
  uint8 advSID = *pData++;
  uint8 advAddrType = *pData++;
  uint8 *advAddress = pData;

  pData += LL_DEVICE_ADDR_LEN;

  uint16 skip = BUILD_UINT16( pData[0], pData[1] );

  pData += sizeof(uint16);

  uint16 syncTimeout = BUILD_UINT16( pData[0], pData[1] );

  pData += sizeof(uint16);

  uint8 syncCteType = *pData;

  return HCI_LE_PeriodicAdvCreateSyncCmd( options, advSID, advAddrType, advAddress, skip,
                                          syncTimeout, syncCteType );
}

/*******************************************************************************
 * @fn          hciCmdParserAddDeviceToPeriodicAdvList
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_AddDeviceToPeriodicAdvListCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return        LL_STATUS_SUCCESS,
 *                LL_STATUS_ERROR_BAD_PARAMETER,
 *                LL_STATUS_ERROR_AL_TABLE_FULL,
 *                LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *                LL_STATUS_ERROR_UNEXPECTED_PARAMETER,
 *                LL_STATUS_ERROR_MEM_CAPACITY_EXCEEDED,
 *
 */
static inline hciStatus_t hciCmdParserAddDeviceToPeriodicAdvList( uint8 *pData )
{
  uint8 advAddrType = *pData++;
  uint8 *advAddress = pData;

  pData += LL_DEVICE_ADDR_LEN;

  uint8 advSID = *pData;

  return HCI_LE_AddDeviceToPeriodicAdvListCmd( advAddrType, advAddress, advSID );
}

/*******************************************************************************
 * @fn          hciCmdParserRemoveDeviceFromPeriodicAdvList
 *
 * @brief       This function used for parsing the pData and parsing it to
 *              HCI_LE_RemoveDeviceFromPeriodicAdvListCmd input arguments.
 *
 * input parameters
 *
 * @param       pData - Pointer to packet's data.
 *
 * output parameters
 *
 * @param       None.
 *
 * @return        LL_STATUS_SUCCESS,
 *                LL_STATUS_ERROR_BAD_PARAMETER,
 *                LL_STATUS_ERROR_COMMAND_DISALLOWED,
 *                LL_STATUS_ERROR_UNKNOWN_ADVERTISING_IDENTIFIER,
 *
 */
static inline hciStatus_t hciCmdParserRemoveDeviceFromPeriodicAdvList( uint8 *pData )
{

  uint8 advAddrType = *pData++;
  uint8 *advAddress = pData;

  pData += LL_DEVICE_ADDR_LEN;

  uint8 advSID = *pData;

  return HCI_LE_RemoveDeviceFromPeriodicAdvListCmd( advAddrType, advAddress, advSID );
}
/***************************************************************************************************
 */
