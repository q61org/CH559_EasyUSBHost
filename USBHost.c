#include "sdcc_keywords.h"
#include "CH559.h"
#include "USBHost.h"
#include "util.h"
#include <string.h>
#include <stdint.h>
#include "udev_util.h"
#include "udev_hub.h"
#include "udev_hid.h"
#include "udev_ftdi.h"

typedef const unsigned char __code *PUINT8C;

__code unsigned char GetDeviceDescriptorRequest[] = 		{USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0, USB_DESCR_TYP_DEVICE , 0, 0, sizeof(USB_DEV_DESCR), 0};
__code unsigned char GetConfigurationDescriptorRequest[] = 	{USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0, USB_DESCR_TYP_CONFIG, 0, 0, sizeof(USB_DEV_DESCR), 0};
__code unsigned char GetInterfaceDescriptorRequest[] = 		{USB_REQ_TYP_IN | USB_REQ_RECIP_INTERF, USB_GET_DESCRIPTOR, 0, USB_DESCR_TYP_INTERF, 0, 0, sizeof(USB_ITF_DESCR), 0};
__code unsigned char SetUSBAddressRequest[] = 				{USB_REQ_TYP_OUT, USB_SET_ADDRESS, USB_DEVICE_ADDR, 0, 0, 0, 0, 0};
__code unsigned char GetDeviceStringRequest[] = 			{USB_REQ_TYP_IN, USB_GET_DESCRIPTOR, 0, USB_DESCR_TYP_STRING, 9, 4, 255, 0};	//todo change language
__code unsigned char SetupSetUsbConfig[] = { USB_REQ_TYP_OUT, USB_SET_CONFIGURATION, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };


#define FTDI_SIO_SET_BAUD_RATE          3
#define FTDI_SIO_SET_LATENCY_TIMER      9
__code unsigned char FTDISetBaudRateRequest[] = { USB_REQ_TYP_OUT | USB_REQ_TYP_VENDOR, FTDI_SIO_SET_BAUD_RATE, 0x00, 0x00, 0x00 };
__code unsigned char FTDISetLatencyTimerRequest[] = { USB_REQ_TYP_OUT | USB_REQ_TYP_VENDOR, FTDI_SIO_SET_LATENCY_TIMER, 0x00 };

__at(0x0000) __xdata uint8_t RxBuffer[MAX_PACKET_SIZE];
__at(0x0040) __xdata uint8_t TxBuffer[MAX_PACKET_SIZE];
__xdata uint8_t receiveDataBuffer[RECEIVE_BUFFER_LEN];

__xdata uint8_t endpoint0Size;	//todo rly global?
__xdata unsigned char SetPort = 0;	//todo really global?


__xdata struct _RootHubDevice rootHubDevice[ROOT_HUB_COUNT];
USBDevice USBdevs[MAX_USB_DEVICES];
UDevAttachCallback attachCallback;


void disableRootHubPort(unsigned char index)
{
	rootHubDevice[index].status = ROOT_DEVICE_DISCONNECT;
	rootHubDevice[index].address = 0;
	if (index)
	UHUB1_CTRL = 0;
	else
	UHUB0_CTRL = 0;
}

void initUSB_Host()
{
	IE_USB = 0;
	USB_CTRL = bUC_HOST_MODE;
	USB_DEV_AD = 0x00;
	UH_EP_MOD = bUH_EP_TX_EN | bUH_EP_RX_EN ;
	UH_RX_DMA_H = 0;
	UH_RX_DMA_L = 0;
	UH_TX_DMA_H = 0;
	UH_TX_DMA_L = 0x40;
	UH_RX_CTRL = 0x00;
	UH_TX_CTRL = 0x00;
	USB_CTRL = bUC_HOST_MODE | bUC_INT_BUSY | bUC_DMA_EN;
	UH_SETUP = bUH_SOF_EN;
	USB_INT_FG = 0xFF;

	disableRootHubPort(0);
	disableRootHubPort(1);
	USB_INT_EN = bUIE_TRANSFER | bUIE_DETECT;

	for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
		USBdevs[i].connected = 0;
	}
	attachCallback = NULL;
}

void setHostUsbAddr(unsigned char addr)
{
	USB_DEV_AD = USB_DEV_AD & bUDA_GP_BIT | addr & 0x7F;
}

void setUsbSpeed(unsigned char fullSpeed)
{
	if (fullSpeed)
	{
		USB_CTRL &= ~ bUC_LOW_SPEED;
		UH_SETUP &= ~ bUH_PRE_PID_EN;
	}
	else
		USB_CTRL |= bUC_LOW_SPEED;
}

void resetRootHubPort(unsigned char rootHubIndex)
{
	endpoint0Size = DEFAULT_ENDP0_SIZE; //todo what's that?                    
	setHostUsbAddr(0);
	setUsbSpeed(1);
	 if (rootHubIndex == 0)    
    {
        UHUB0_CTRL = UHUB0_CTRL & ~ bUH_LOW_SPEED | bUH_BUS_RESET;
        delay(15);
        UHUB0_CTRL = UHUB0_CTRL & ~ bUH_BUS_RESET;
    }
    else if (rootHubIndex == 1)
    {
        UHUB1_CTRL = UHUB1_CTRL & ~ bUH_LOW_SPEED | bUH_BUS_RESET;
        delay(15);
        UHUB1_CTRL = UHUB1_CTRL & ~ bUH_BUS_RESET;
    }
	delayUs(250);
	UIF_DETECT = 0; //todo test if redundant                                       
}

unsigned char enableRootHubPort(unsigned char rootHubIndex)
{
    if ( rootHubDevice[ rootHubIndex ].status < 1 )
    {
        rootHubDevice[ rootHubIndex ].status = 1;
    }
	if (rootHubIndex == 0)
	{
		if (USB_HUB_ST & bUHS_H0_ATTACH)
		{
			if ((UHUB0_CTRL & bUH_PORT_EN) == 0x00)
			{
				if (USB_HUB_ST & bUHS_DM_LEVEL)
				{
					rootHubDevice[rootHubIndex].speed = 0;
					UHUB0_CTRL |= bUH_LOW_SPEED;
				}
				else rootHubDevice[rootHubIndex].speed = 1;
			}
			UHUB0_CTRL |= bUH_PORT_EN;
			return ERR_SUCCESS;
		}
	}
	else if (rootHubIndex == 1)
	{
		if (USB_HUB_ST & bUHS_H1_ATTACH)
		{
			if ((UHUB1_CTRL & bUH_PORT_EN ) == 0x00)
			{
				if (USB_HUB_ST & bUHS_HM_LEVEL)
				{
					rootHubDevice[rootHubIndex].speed = 0;
					UHUB1_CTRL |= bUH_LOW_SPEED;
				}
				else rootHubDevice[rootHubIndex].speed = 1;
			}
			UHUB1_CTRL |= bUH_PORT_EN;
			return ERR_SUCCESS;
		}
	}
	return ERR_USB_DISCON;
}

void selectHubPort(unsigned char rootHubIndex, unsigned char HubPortIndex)
{
	unsigned char temp = HubPortIndex;
        setHostUsbAddr(rootHubDevice[rootHubIndex].address); //todo ever != 0
        setUsbSpeed(rootHubDevice[rootHubIndex].speed); //isn't that set before?
}

void selectUSBDevice(uint8_t devIndex)
{
	USBDevice *dev = &USBdevs[devIndex];
	setHostUsbAddr(dev->address);
	setUsbSpeed(dev->speed);
	endpoint0Size = dev->endpoint0size;
	if ((dev->parentDevIndex != PARENT_NONE) && (dev->speed == 0)) {
		UH_SETUP |= bUH_PRE_PID_EN;
	}
}

unsigned char hostTransfer(unsigned char endp_pid, unsigned char tog, unsigned short timeout )
{
    unsigned short retries;
    unsigned char	r;
    unsigned short	i;
    UH_RX_CTRL = tog;
    UH_TX_CTRL = tog;
    retries = 0;
    do
    {
        UH_EP_PID = endp_pid;                               
        UIF_TRANSFER = 0;            
        for (i = 200; i != 0 && UIF_TRANSFER == 0; i--)
            delayUs(1);
        UH_EP_PID = 0x00;                                         
        if ( UIF_TRANSFER == 0 )
        {
            return ERR_USB_UNKNOWN;
        }
        if ( UIF_TRANSFER )                                    
        {
            if ( U_TOG_OK )
            {
                return( ERR_SUCCESS );
            }
            r = USB_INT_ST & MASK_UIS_H_RES;               
            if ( r == USB_PID_STALL )
            {
                return( r | ERR_USB_TRANSFER );
            }
            if ( r == USB_PID_NAK )
            {
                if ( timeout == 0 )
                {
                    return( r | ERR_USB_TRANSFER );
                }
                if ( timeout < 0xFFFF )
                {
                    timeout --;
                }
                retries--;
            }
            else switch ( endp_pid >> 4 )	//todo no return.. compare to other guy
                {
                case USB_PID_SETUP:
                case USB_PID_OUT:
                    if ( U_TOG_OK )
                    {
                        return( ERR_SUCCESS );
                    }
                    if ( r == USB_PID_ACK )
                    {
                        return( ERR_SUCCESS );
                    }
                    if ( r == USB_PID_STALL || r == USB_PID_NAK )
                    {
                        return( r | ERR_USB_TRANSFER );
                    }
                    if ( r )
                    {
                        return( r | ERR_USB_TRANSFER );          
                    }
                    break;                                    
                case USB_PID_IN:
                    if ( U_TOG_OK )
                    {
                        return( ERR_SUCCESS );
                    }
                    if ( tog ? r == USB_PID_DATA1 : r == USB_PID_DATA0 )
                    {
                        return( ERR_SUCCESS );
                    }
                    if ( r == USB_PID_STALL || r == USB_PID_NAK )
                    {
                        return( r | ERR_USB_TRANSFER );
                    }
                    if ( r == USB_PID_DATA0 && r == USB_PID_DATA1 )
                    {
                    }                                        
                    else if ( r )
                    {
                        return( r | ERR_USB_TRANSFER );     
                    }
                    break;                                     
                default:
                    return( ERR_USB_UNKNOWN );                  
                    break;
                }
        }
        else                                                    
        {
            USB_INT_FG = 0xFF;                               
        }
        delayUs(15);
    }
    while ( ++retries < 200 );
    return( ERR_USB_TRANSFER );                              
}


void fillTxBuffer_xdata(const uint8_t __xdata *data, unsigned char len)
{
	unsigned char i;
	//DEBUG_OUT(">> fillTxBuffer_x %i bytes\n", len);
	for(i = 0; i < len; i++)
		TxBuffer[i] = data[i];
	//DEBUG_OUT(">> fillTxBuffer_x done\n", len);
}


//todo request buffer
unsigned char hostCtrlTransfer(unsigned char __xdata *DataBuf, unsigned short *RetLen, unsigned short maxLenght)
{
	//unsigned char temp = maxLenght;
	unsigned short RemLen;
	unsigned char s, RxLen, i;
	unsigned char __xdata *pBuf;
	unsigned short *pLen;
	//DEBUG_OUT(">> hostCtrlTransfer\n");
	PXUSB_SETUP_REQ pSetupReq = ((PXUSB_SETUP_REQ)TxBuffer);
	pBuf = DataBuf;
	pLen = RetLen;
	delayUs(200);
	if (pLen)
		*pLen = 0;
	UH_TX_LEN = sizeof(USB_SETUP_REQ);
	s = hostTransfer((unsigned char)(USB_PID_SETUP << 4), 0, 10000);
	if (s != ERR_SUCCESS)
		return (s);
	UH_RX_CTRL = UH_TX_CTRL = bUH_R_TOG | bUH_R_AUTO_TOG | bUH_T_TOG | bUH_T_AUTO_TOG;
	UH_TX_LEN = 0x01;
	RemLen = (pSetupReq->wLengthH << 8) | (pSetupReq->wLengthL);
	if (maxLenght == 0) {
		maxLenght = sizeof(receiveDataBuffer);
	}
	if (RemLen > maxLenght) {
		RemLen = maxLenght;
	}
	if (RemLen && pBuf)
	{ 
		if (pSetupReq->bRequestType & USB_REQ_TYP_IN)
		{
			//DEBUG_OUT(">> Remaining bytes to read %d\n", RemLen);
			while (RemLen)
			{
				delayUs(300);
				s = hostTransfer((unsigned char)(USB_PID_IN << 4), UH_RX_CTRL, 10000); 
				if (s != ERR_SUCCESS)
					return (s);
				RxLen = USB_RX_LEN < RemLen ? USB_RX_LEN : RemLen;
				RemLen -= RxLen;
				if (pLen)
					*pLen += RxLen;
				for(i = 0; i < RxLen; i++)
					pBuf[i] = RxBuffer[i];
				pBuf += RxLen;
				//DEBUG_OUT(">> Received %i bytes\n", (uint16_t)USB_RX_LEN);
				if (USB_RX_LEN == 0 || (USB_RX_LEN < endpoint0Size ))
					break; 
			}
			UH_TX_LEN = 0x00;
		}
		else
		{
			//DEBUG_OUT(">> Remaining bytes to write %i", RemLen);
			//todo rework this TxBuffer overwritten
			while (RemLen)
			{
				delayUs(200);
				UH_TX_LEN = RemLen >= endpoint0Size ? endpoint0Size : RemLen;
				fillTxBuffer_xdata(pBuf, UH_TX_LEN);
				pBuf += UH_TX_LEN;
				/*if (pBuf[1] == 0x09)
				{
					SetPort = SetPort ^ 1 ? 1 : 0;
					*pBuf = SetPort;

					//DEBUG_OUT(">> SET_PORT  %02X  %02X ", *pBuf, SetPort);
				}*/
				//DEBUG_OUT(">> Sending %i bytes\n", (uint16_t)UH_TX_LEN);
				s = hostTransfer(USB_PID_OUT << 4, UH_TX_CTRL, 10000);
				if (s != ERR_SUCCESS)
					return (s);
				RemLen -= UH_TX_LEN;
				if (pLen)
					*pLen += UH_TX_LEN;
			}
		}
	}
	delayUs(200);
	s = hostTransfer((UH_TX_LEN ? USB_PID_IN << 4 : USB_PID_OUT << 4), bUH_R_TOG | bUH_T_TOG, 10000);
	if (s != ERR_SUCCESS)
		return (s);
	if (UH_TX_LEN == 0)
		return (ERR_SUCCESS);
	if (USB_RX_LEN == 0)
		return (ERR_SUCCESS);
	return (ERR_USB_BUF_OVER);
}

void fillTxBuffer(PUINT8C data, unsigned char len)
{
	unsigned char i;
	//DEBUG_OUT(">> fillTxBuffer %i bytes\n", len);
	for(i = 0; i < len; i++)
		TxBuffer[i] = data[i];
	//DEBUG_OUT(">> fillTxBuffer done\n", len);
}

unsigned char getDeviceDescriptor()
{
    unsigned char s;
    unsigned short len;
    endpoint0Size = DEFAULT_ENDP0_SIZE;		//TODO again?
	//DEBUG_OUT("getDeviceDescriptor\n");
	fillTxBuffer(GetDeviceDescriptorRequest, sizeof(GetDeviceDescriptorRequest));
    s = hostCtrlTransfer(receiveDataBuffer, &len, RECEIVE_BUFFER_LEN);          
    if (s != ERR_SUCCESS)
        return s;

	//DEBUG_OUT("Device descriptor request sent successfully\n");
    endpoint0Size = ((PXUSB_DEV_DESCR)receiveDataBuffer)->bMaxPacketSize0;
    if (len < ((PUSB_SETUP_REQ)GetDeviceDescriptorRequest)->wLengthL)
    {
		//DEBUG_OUT("Received packet is smaller than expected\n")
        return ERR_USB_BUF_OVER;                
    }
    return ERR_SUCCESS;
}

unsigned char setUsbAddress(unsigned char addr)
{
    unsigned char s;
	PXUSB_SETUP_REQ pSetupReq = ((PXUSB_SETUP_REQ)TxBuffer);
    fillTxBuffer(SetUSBAddressRequest, sizeof(SetUSBAddressRequest));
    pSetupReq->wValueL = addr;          
    s = hostCtrlTransfer(0, 0, 0);   
    if (s != ERR_SUCCESS) return s;
    DEBUG_OUT( "SetAddress: %i\n" , addr);
    setHostUsbAddr(addr);
    delay(100);         
    return ERR_SUCCESS;
}

unsigned char setUsbConfig( unsigned char cfg )
{
	PXUSB_SETUP_REQ pSetupReq = ((PXUSB_SETUP_REQ)TxBuffer);
    fillTxBuffer(SetupSetUsbConfig, sizeof(SetupSetUsbConfig));
    pSetupReq->wValueL = cfg;                          
    return( hostCtrlTransfer(0, 0, 0) );            
}

void DEBUG_OUT_USB_BUFFER(unsigned char __xdata *usbBuffer);

unsigned char getDeviceString(uint8_t index)
{
    fillTxBuffer(GetDeviceStringRequest, sizeof(GetDeviceStringRequest));
	PXUSB_SETUP_REQ pSetupReq = ((PXUSB_SETUP_REQ)TxBuffer);
	pSetupReq->wValueL = index;
    uint8_t s = hostCtrlTransfer(receiveDataBuffer, 0, RECEIVE_BUFFER_LEN);
	//DEBUG_OUT_USB_BUFFER(receiveDataBuffer);
	return s;
}

char convertStringDescriptor(unsigned char __xdata *usbBuffer, unsigned char __xdata *strBuffer, unsigned short bufferLength)
{
	//supports using source as target buffer
	unsigned char i = 0, len = (usbBuffer[0] - 2) >> 1;
	if(usbBuffer[1] != USB_DESCR_TYP_STRING) return 0;	//check if device string
	for(; (i < len) && (i < bufferLength - 1); i++)
		if(usbBuffer[2 + 1 + (i << 1)])
			strBuffer[i] = '?';
		else
			strBuffer[i] = usbBuffer[2 + (i << 1)];
	strBuffer[i] = 0;
	//sendProtocolMSG(MSG_TYPE_DEVICE_STRING,(unsigned short)len, index+1, 0x34, 0x56, strBuffer);
	return 1;
}

void DEBUG_OUT_USB_BUFFER(unsigned char __xdata *usbBuffer)
{
	int i;
	for(i = 0; i < usbBuffer[0]; i++)
	{
		DEBUG_OUT("0x%02X ", (uint16_t)(usbBuffer[i]));
	}
	DEBUG_OUT("\n");
}

void DEBUG_OUT_USB_BUFFER_LEN(unsigned char __xdata *usbBuffer, uint8_t len)
{
	int i;
	for(i = 0; i < len; i++)
	{
		DEBUG_OUT("0x%02X ", (uint16_t)(usbBuffer[i]));
	}
	DEBUG_OUT("\n");
}

unsigned char getConfigurationDescriptor()
{
    unsigned char s;
    unsigned short len, total;
	fillTxBuffer(GetConfigurationDescriptorRequest, sizeof(GetConfigurationDescriptorRequest));

    s = hostCtrlTransfer(receiveDataBuffer, &len, RECEIVE_BUFFER_LEN);             
    if(s != ERR_SUCCESS)
        return s;
	//todo didnt send reqest completely
    if(len < ((PUSB_SETUP_REQ)GetConfigurationDescriptorRequest)->wLengthL)
        return ERR_USB_BUF_OVER;

	//todo fix 16bits
	PXUSB_CFG_DESCR cfgd = (PXUSB_CFG_DESCR)receiveDataBuffer;
	DEBUG_OUT("config total length: 0x%02x%02x\n", cfgd->wTotalLengthH, cfgd->wTotalLengthL);
    total = ((PXUSB_CFG_DESCR)receiveDataBuffer)->wTotalLengthL + (((PXUSB_CFG_DESCR)receiveDataBuffer)->wTotalLengthH << 8);
	fillTxBuffer(GetConfigurationDescriptorRequest, sizeof(GetConfigurationDescriptorRequest));
	if (total > sizeof(receiveDataBuffer)) total = sizeof(receiveDataBuffer);
    ((PUSB_SETUP_REQ)TxBuffer)->wLengthL = (unsigned char)(total & 255);
    ((PUSB_SETUP_REQ)TxBuffer)->wLengthH = (unsigned char)(total >> 8);
    s = hostCtrlTransfer(receiveDataBuffer, &len, RECEIVE_BUFFER_LEN);             
    if(s != ERR_SUCCESS)
        return s;
	//todo 16bit and fix received length check
	//if (len < total || len < ((PXUSB_CFG_DESCR)receiveDataBuffer)->wTotalLengthL)
    //    return( ERR_USB_BUF_OVER );                             
    return ERR_SUCCESS;
}

uint8_t allocateNewDeviceAddress(uint8_t hubIndex, uint8_t portIndex, uint8_t is_hub)
{
	if (is_hub) {
		// if this is a hub, allocate highest address available
		for (int8_t i = MAX_USB_DEVICES - 1; i >= 0; i--) {
			if (!USBdevs[i].connected) {
				return i + FIRST_USB_DEV_ID;
			}
		}
		// no more device can be connected
		return 0;
	}

	if (hubIndex == PARENT_NONE) {
		// tier 2 == one device only, your address is the lowest address
		return FIRST_USB_DEV_ID;
	}

	// trace back to tier 2 hub to find out which port it is connected to
	USBDevice *dev = &USBdevs[hubIndex];
	uint8_t parentPortIndex = portIndex;
	while (dev->parentDevIndex != PARENT_NONE) {
		parentPortIndex = dev->parentDevPortIndex;
		dev = &USBdevs[dev->parentDevIndex];
	}
	// if fixed address for that port is free, that is your address
	if (!USBdevs[parentPortIndex - 1].connected) {
		return FIRST_USB_DEV_ID + parentPortIndex - 1;
	}

	// the port already has a non-hub device, allocate some dynamic address
	dev = NULL;
	for (int8_t i = MAX_USB_DEVICES - 1; i >= 0; i--) {
		// look for tier 2 hub, it should hit on first try, but just in case...
		if (!USBdevs[i].connected) continue;
		if (USBdevs[i].num_ifaces == 0) continue;
		if (USBdevs[i].iface[0].class != USB_DEV_CLASS_HUB) continue;
		if (USBdevs[i].parentDevIndex != PARENT_NONE) continue;
		dev = &USBdevs[i];
		break;
	}
	uint8_t devIdx = 0;
	if (dev != NULL) {
		devIdx = dev->iface[0].spec.hub.num_ports;
	}
	for (; devIdx < MAX_USB_DEVICES; devIdx++) {
		if (!USBdevs[devIdx].connected) {
			return FIRST_USB_DEV_ID + devIdx;
		}
	}
	return 0;
}

unsigned char initializeRootHubConnection(unsigned char rootHubIndex, uint8_t parentIndex, uint8_t parentPortIndex, uint8_t speed)
{
	unsigned char retry, i, s = ERR_SUCCESS, cfg, dv_cls, addr;

	for(retry = 0; retry < 10; retry++) //todo test fewer retries
	{
		delay( 100 );
		delay(100); //todo test lower delay
		if (parentIndex == PARENT_NONE) {
			resetRootHubPort(rootHubIndex);                      
			for (i = 0; i < 100; i++) //todo test fewer retries
			{
				delay(1);
				if (enableRootHubPort(rootHubIndex) == ERR_SUCCESS)  
					break;
			}
			if (i == 100)                                              
			{
				disableRootHubPort(rootHubIndex);
				DEBUG_OUT("Failed to enable root hub port %i\n", rootHubIndex);
				continue;
			}
			selectHubPort(rootHubIndex, 0);
			speed = rootHubDevice[rootHubIndex].speed;
		}
		if (parentIndex != PARENT_NONE) { // why separate IF?
			setUsbSpeed(speed);
			if (speed == 0) {
				UH_SETUP |= bUH_PRE_PID_EN;
			}
			setHostUsbAddr(0);
		}
		DEBUG_OUT("initializing device at root hub port %i\n", rootHubIndex);
		s = getDeviceDescriptor();
		if (s != ERR_SUCCESS) {
			DEBUG_OUT( "getDeviceDescriptor Error = %02X\n", s);
			continue;
		}
		PXUSB_DEV_DESCR descr = (PXUSB_DEV_DESCR)receiveDataBuffer;
		addr = allocateNewDeviceAddress(parentIndex, parentPortIndex, descr->bDeviceClass == USB_DEV_CLASS_HUB);
		if (addr == 0) {
			s = ERR_USB_CONNECT;
			DEBUG_OUT("no more devices can be connected.\n");
			break;
		}
		USBDevice *dev = &USBdevs[addr - FIRST_USB_DEV_ID]; // FIXME: indexing
		dev->address = addr;
		dev->class = dv_cls = descr->bDeviceClass;
		DEBUG_OUT( "Device class %i\n", dv_cls);
		DEBUG_OUT( "Max packet size %i\n", descr->bMaxPacketSize0);
		dev->vid_l = descr->idVendorL;
		dev->vid_h = descr->idVendorH;
		dev->pid_l = descr->idProductL;
		dev->pid_h = descr->idProductH;
		DEBUG_OUT( "VID/PID: %02x%02x/%02x%02x\n", dev->vid_h, dev->vid_l, dev->pid_h, dev->pid_l);
		dev->endpoint0size = descr->bMaxPacketSize0;
		DEBUG_OUT_USB_BUFFER(receiveDataBuffer);
		s = setUsbAddress(addr);
		if (s != ERR_SUCCESS) {
			DEBUG_OUT("setUsbAddress Error = %02X\n", s);
			continue;
		}
		rootHubDevice[rootHubIndex].address = addr;
		dev->rootHubIndex = rootHubIndex;
		dev->speed = speed;
		dev->parentDevIndex = parentIndex;
		dev->parentDevPortIndex = parentPortIndex;
		DEBUG_OUT("Manufacturer String: ");
		DEBUG_OUT_DEVICE_STRING(1);
		DEBUG_OUT("\n");
		DEBUG_OUT("Model String: ");
		DEBUG_OUT_DEVICE_STRING(2);
		DEBUG_OUT("\n");

		s = getConfigurationDescriptor();
		if (s != ERR_SUCCESS) {
			DEBUG_OUT("getConfigurationDescriptor Error = %02X\n", s);
			continue;
		}
		//sendProtocolMSG(MSG_TYPE_DEVICE_INFO, (receiveDataBuffer[2] + (receiveDataBuffer[3] << 8)), addr, rootHubIndex+1, 0xAA, receiveDataBuffer);
		unsigned short i, total;
		PXUSB_ITF_DESCR currentInterface = 0;
		int interfaces;
		//DEBUG_OUT_USB_BUFFER(receiveDataBuffer);
		/*for(i = 0; i < receiveDataBuffer[2] + (receiveDataBuffer[3] << 8); i++)
		{
			DEBUG_OUT("0x%02X ", (uint16_t)(receiveDataBuffer[i]));
		}
		DEBUG_OUT("\n");*/

		cfg = ((PXUSB_CFG_DESCR)receiveDataBuffer)->bConfigurationValue;
		DEBUG_OUT("Configuration value: %i\n", cfg);

		interfaces = ((PXUSB_CFG_DESCR_LONG)receiveDataBuffer)->cfg_descr.bNumInterfaces;
		DEBUG_OUT("Interface count: %i\n", interfaces);

		s = setUsbConfig( cfg ); 
		if (s != ERR_SUCCESS) {
			DEBUG_OUT("setUsbConfig Error = %02X\n", s);
			continue;
		}
		//parse descriptors
		static __xdata unsigned char temp[512];
		total = ((PXUSB_CFG_DESCR)receiveDataBuffer)->wTotalLengthL + (((PXUSB_CFG_DESCR)receiveDataBuffer)->wTotalLengthH << 8);
		if (total > sizeof(temp)) {
			DEBUG_OUT("config descriptor too big (%d), cripping at %d bytes.\n", total, sizeof(temp));
			total = sizeof(temp);
		}
		for(i = 0; i < total; i++)
			temp[i] = receiveDataBuffer[i];
		i = ((PXUSB_CFG_DESCR)receiveDataBuffer)->bLength;
		uint8_t iIndex = 0;
		uint8_t doCallback = 0;

		while (i < total - 1 && iIndex < MAX_INTERFACES_PER_DEVICE)
		{
			unsigned char __xdata *desc = &(temp[i]);
			if (desc[0] == 0) {
				DEBUG_OUT("zero-length descriptor? @%d\n", i);
				i++;
				continue;
			}
			if ((uint16_t)i + desc[0] > total) {
				DEBUG_OUT("descriptor extends beyond EOF, dropping.\n");
				break;
			}
			switch(desc[1])
			{
				case USB_DESCR_TYP_INTERF:
					DEBUG_OUT("Interface descriptor found\n");
					//DEBUG_OUT_USB_BUFFER(desc);
					currentInterface = ((PXUSB_ITF_DESCR)desc);
					readInterface(rootHubIndex, currentInterface);
					break;
				case USB_DESCR_TYP_ENDP:
					DEBUG_OUT("Endpoint descriptor found\n");
					{
						PXUSB_ENDP_DESCR d = (PXUSB_ENDP_DESCR)desc;
						UDevInterface *iface = &dev->iface[iIndex++];
						dev->num_ifaces = iIndex;
						iface->interface = currentInterface->bInterfaceNumber;
						iface->class = currentInterface->bInterfaceClass;
						iface->subclass = currentInterface->bInterfaceSubClass;
						if (d->bEndpointAddress & 0x80) {
							iface->ep_in = d->bEndpointAddress & 0x7f;
							iface->ep_out = 0;
						} else {
							iface->ep_in = 0;
							iface->ep_out = d->bEndpointAddress & 0x7f;
						}

						if (currentInterface->bInterfaceClass == USB_DEV_CLASS_VEN_SPEC) {
							if (ftdidevice_init_endpoint(dev, iface, d->bEndpointAddress) == 0) {
								DEBUG_OUT("FTDI device registered\n");
								doCallback = 1;
							}
						/*if (currentInterface->bInterfaceClass == USB_DEV_CLASS_HID) {
							hiddevice_init_endpoint(dev, iface, d->bEndpointAddress);*/
						} else if (currentInterface->bInterfaceClass == USB_DEV_CLASS_HUB) {
							hubdevice_init_endpoint(dev, iface, d->bEndpointAddress);
						}
					}
					break;
				case USB_DESCR_TYP_HID:
					DEBUG_OUT("HID descriptor found\n");
					//DEBUG_OUT_USB_BUFFER(desc);
					if(currentInterface == 0) break;
					readHIDInterface(currentInterface, (PXUSB_HID_DESCR)desc);
					break;
				case USB_DESCR_TYP_CS_INTF:
					DEBUG_OUT("Class specific header descriptor found\n");
					DEBUG_OUT_USB_BUFFER(desc);
					//if(currentInterface == 0) break;
					//readHIDInterface(currentInterface, (PXUSB_HID_DESCR)desc);
					break;
				case USB_DESCR_TYP_CS_ENDP:
					DEBUG_OUT("Class specific endpoint descriptor found\n");
					DEBUG_OUT_USB_BUFFER(desc);
					//if(currentInterface == 0) break;
					//readHIDInterface(currentInterface, (PXUSB_HID_DESCR)desc);
					break;
				default:
					DEBUG_OUT("Unexpected descriptor type: %02X\n", desc[1]);
					DEBUG_OUT_USB_BUFFER(desc);
			}
			i += desc[0];
		}
		dev->connected = 1;
		if (doCallback) {
			callAttachCallback(addr - FIRST_USB_DEV_ID, dev, 1);
		}
		return ERR_SUCCESS;
	}
	DEBUG_OUT("connecting device failed. Error = %02X\n", s);
	//sendProtocolMSG(MSG_TYPE_ERROR,0, rootHubIndex+1, s, 0xEE, 0);
	rootHubDevice[rootHubIndex].status = ROOT_DEVICE_FAILED;
	setUsbSpeed(1);	//TODO define speeds
	return s;
}

unsigned char checkRootHubConnections()
{
	unsigned char s;
	s = ERR_SUCCESS;
	if (UIF_DETECT)                                                        
	{
		UIF_DETECT = 0;    
			if(USB_HUB_ST & bUHS_H0_ATTACH)
			{
				if(rootHubDevice[0].status == ROOT_DEVICE_DISCONNECT || (UHUB0_CTRL & bUH_PORT_EN) == 0x00)
				{
					disableRootHubPort(0);	//todo really need to reset register?
					rootHubDevice[0].status = ROOT_DEVICE_CONNECTED;
					DEBUG_OUT("Device at root hub %i connected\n", 0);
					//sendProtocolMSG(MSG_TYPE_CONNECTED,0, 0x01, 0x01, 0x01, 0);
					s = initializeRootHubConnection(0, PARENT_NONE, 0, 0);
				}
			}
			else
			if(rootHubDevice[0].status >= ROOT_DEVICE_CONNECTED)
			{
    			//resetHubDevices(0);
				disableRootHubPort(0);
				DEBUG_OUT("Device at root hub %i disconnected\n", 0);
					//sendProtocolMSG(MSG_TYPE_DISCONNECTED,0, 0x01, 0x01, 0x01, 0);
				s = ERR_USB_DISCON;
			}
			if(USB_HUB_ST & bUHS_H1_ATTACH)
			{
				
				if(rootHubDevice[1].status == ROOT_DEVICE_DISCONNECT || (UHUB1_CTRL & bUH_PORT_EN) == 0x00)
				{
					disableRootHubPort(1);	//todo really need to reset register?
					rootHubDevice[1].status = ROOT_DEVICE_CONNECTED;
					DEBUG_OUT("Device at root hub %i connected\n", 1);
					//sendProtocolMSG(MSG_TYPE_CONNECTED,0, 0x02, 0x02, 0x02, 0);
					s = initializeRootHubConnection(1, PARENT_NONE, 0, 0);
				}
			}
			else
			if(rootHubDevice[1].status >= ROOT_DEVICE_CONNECTED)
			{
    			//resetHubDevices(1);
				disableRootHubPort(1);
				DEBUG_OUT("Device at root hub %i disconnected\n", 1);
					//sendProtocolMSG(MSG_TYPE_DISCONNECTED,0, 0x02, 0x02, 0x02, 0);
				for (uint8_t i = 0; i < MAX_USB_DEVICES; i++) {
					if (!USBdevs[i].connected) continue;
					USBdevs[i].connected = 0;
					callAttachCallback(i, &USBdevs[i], 0);
				}
				s = ERR_USB_DISCON;
			}
	}
	return s;
}


void setAttachCallback(UDevAttachCallback func) {
	attachCallback = func;
}
void callAttachCallback(uint8_t devIndex, USBDevice *dev, uint8_t is_attach) {
	if (attachCallback == NULL) return;
	attachCallback(devIndex, dev, is_attach);
}