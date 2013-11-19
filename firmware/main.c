#include "Framework.h"

int main(void){
	USB_ConfigureClock();
	USB_Init();
	USB.INTCTRLA = USB_BUSEVIE_bm | USB_INTLVL_MED_gc;
	USB.INTCTRLB = USB_TRNIE_bm | USB_SETUPIE_bm;
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm;
	sei();

	PORTE.DIRSET = (1 << 3) | (1 << 1) | (1 << 0);
	PORTE.DIRCLR = (1 << 2);
	//PORTE.PIN3CTRL = PORT_INVEN_bm;
	USARTE0.BAUDCTRLA = 0x07;
	USARTE0.BAUDCTRLB = 0x00;
	USARTE0.CTRLC = USART_CMODE_MSPI_gc;
	USARTE0.CTRLB = USART_RXEN_bm | USART_TXEN_bm;

	DMA.CTRL = DMA_ENABLE_bm | DMA_DBUFMODE_CH01CH23_gc | DMA_PRIMODE_RR0123_gc;
	// BUFFER -> .DATA
	DMA.CH0.REPCNT = 1;
	DMA.CH0.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
	DMA.CH0.ADDRCTRL = DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_DRE_gc;
	DMA.CH0.DESTADDR0 = (uint16_t)(&USARTE0.DATA) & 0xFF;
	DMA.CH0.DESTADDR1 = ((uint16_t)(&USARTE0.DATA) >> 8) & 0xFF;
	DMA.CH0.DESTADDR2 = 0x00;

	// .DATA -> BUFFER
	DMA.CH2.REPCNT = 1;
	DMA.CH2.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
	DMA.CH2.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc | DMA_CH_DESTDIR_INC_gc;
	DMA.CH2.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_RXC_gc;
	DMA.CH2.SRCADDR0 = (uint16_t)(&USARTE0.DATA) & 0xFF;
	DMA.CH2.SRCADDR1 = ((uint16_t)(&USARTE0.DATA) >> 8) & 0xFF;
	DMA.CH2.SRCADDR2 = 0x00;

	// BUFFER -> .DATA
	DMA.CH1.REPCNT = 1;
	DMA.CH1.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
	DMA.CH1.ADDRCTRL = DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;
	DMA.CH1.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_DRE_gc;
	DMA.CH1.DESTADDR0 = (uint16_t)(&USARTE0.DATA) & 0xFF;
	DMA.CH1.DESTADDR1 = ((uint16_t)(&USARTE0.DATA) >> 8) & 0xFF;
	DMA.CH1.DESTADDR2 = 0x00;

	// .DATA -> BUFFER
	DMA.CH3.REPCNT = 1;
	DMA.CH3.CTRLA = DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc | DMA_CH_REPEAT_bm;
	DMA.CH3.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc | DMA_CH_DESTDIR_INC_gc;
	DMA.CH3.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_RXC_gc;
	DMA.CH3.SRCADDR0 = (uint16_t)(&USARTE0.DATA) & 0xFF;
	DMA.CH3.SRCADDR1 = ((uint16_t)(&USARTE0.DATA) >> 8) & 0xFF;
	DMA.CH3.SRCADDR2 = 0x00;
	bool aRunning = 0;
	bool bRunning = 0;
	bool aDone = 1;
	bool bDone = 1;
	for (;;) {
		if ((DMA.INTFLAGS & (DMA_CH0TRNIF_bm|DMA_CH2TRNIF_bm)) == (DMA_CH0TRNIF_bm|DMA_CH2TRNIF_bm)) {
			aDone = 1;
		}
		if ((DMA.INTFLAGS & (DMA_CH1TRNIF_bm|DMA_CH3TRNIF_bm)) == (DMA_CH1TRNIF_bm|DMA_CH3TRNIF_bm)) {
			bDone = 0;
		}
		if ((usb_pipe_can_read(&ep_out) &&
			usb_pipe_can_write(&ep_in)) &&
			aDone &&
			((DMA.STATUS & (DMA_CH0BUSY_bm|DMA_CH2BUSY_bm)) == 0x00)) {
			DMA.INTFLAGS = 0xff;
			if ( aRunning ) {
				usb_pipe_done_read(&ep_out);
				usb_pipe_done_write(&ep_in);
			}
			aRunning = 1;
			uint8_t *DMASRC = usb_pipe_read_ptr(&ep_out);
			uint8_t *DMADST = usb_pipe_write_ptr(&ep_in);

			DMA.CH0.SRCADDR0 = (uint16_t)DMASRC & 0xFF;
			DMA.CH0.SRCADDR1 = ((uint16_t)(DMASRC) >> 8) & 0xFF;
			DMA.CH0.SRCADDR2 = 0x00;
			DMA.CH0.TRFCNT = 64;
			DMA.CH2.DESTADDR0 = (uint16_t)DMADST & 0xFF;
			DMA.CH2.DESTADDR1 = (((uint16_t)(DMADST)) >> 8) & 0xFF;
			DMA.CH2.DESTADDR2 = 0x00;
			DMA.CH2.TRFCNT = 64;
			DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm;
			DMA.CH2.CTRLA |= DMA_CH_ENABLE_bm;
			aDone = 0;
		}
		if ((usb_pipe_can_read(&ep_out) &&
			usb_pipe_can_write(&ep_in)) &&
			bDone  &&
			((DMA.STATUS & (DMA_CH1BUSY_bm|DMA_CH3BUSY_bm)) == 0x00)) {
			PORTE.OUTTGL = 1;
			if ( bRunning ) {
				usb_pipe_done_read(&ep_out);
				usb_pipe_done_write(&ep_in);
			}
			bRunning = 1;
			uint8_t *DMADST = usb_pipe_write_ptr(&ep_in);
			uint8_t *DMASRC = usb_pipe_read_ptr(&ep_out);

			DMA.CH1.SRCADDR0 = (uint16_t)DMASRC & 0xFF;
			DMA.CH1.SRCADDR1 = ((uint16_t)(DMASRC) >> 8) & 0xFF;
			DMA.CH1.SRCADDR2 = 0x00;
			DMA.CH1.TRFCNT = 64;
			DMA.CH3.DESTADDR0 = (uint16_t)DMADST & 0xFF;
			DMA.CH3.DESTADDR1 = (((uint16_t)(DMADST)) >> 8) & 0xFF;
			DMA.CH3.DESTADDR2 = 0x00;
			DMA.CH3.TRFCNT = 64;
			PORTE.OUTTGL = 1;
			bDone = 0;
		}
		else {
			usb_pipe_handle(&ep_in);
			usb_pipe_handle(&ep_out);
		}
	}
}

#define stringify(s) #s

const char PROGMEM hwversion[] = stringify(HW_VERSION);
const char PROGMEM fwversion[] = stringify(FW_VERSION);

uint8_t usb_cmd = 0;
uint8_t cmd_data = 0;

/** Event handler for the library USB Control Request reception event. */
bool EVENT_USB_Device_ControlRequest(USB_Request_Header_t* req){
	// zero out ep0_buf_in
	for (uint8_t i = 0; i < 64; i++) ep0_buf_in[i] = 0;
	usb_cmd = 0;
	if ((req->bmRequestType & CONTROL_REQTYPE_TYPE) == REQTYPE_VENDOR){
		switch(req->bRequest){
			case 0x00: // Info
				if (req->wIndex == 0){
					USB_ep0_send_progmem((uint8_t*)hwversion, sizeof(hwversion));
				}else if (req->wIndex == 1){
					USB_ep0_send_progmem((uint8_t*)fwversion, sizeof(fwversion));
				}
				return true;
			case 0x08:
				* ((uint8_t *) req->wIndex) = req->wValue;
				USB_ep0_send(0);
				return true;
			case 0x09:
				ep0_buf_in[0] = * ((uint8_t *) req->wIndex);
				USB_ep0_send(1);
				return true;
			case 0x16:
				* ((uint16_t *) req->wIndex) = req->wValue;
				USB_ep0_send(0);
				return true;
			case 0x17:{
				uint16_t *addr;
				addr = (uint16_t *) req->wIndex;
				ep0_buf_in[0] = *addr & 0xFF;
				ep0_buf_in[1] = *addr >> 8;
				USB_ep0_send(2);}
				return true;
			// read EEPROM
			case 0xE0:
				eeprom_read_block(ep0_buf_in, (void*)(req->wIndex*64), 64);
				USB_ep0_send(64);
				return true;

			// write EEPROM
			case 0xE1:
				usb_cmd = req->bRequest;
				cmd_data = req->wIndex;
				USB_ep0_send(0);
				return true; // Wait for OUT data (expecting an OUT transfer)

			// disconnect from USB, jump to bootloader
			case 0xBB:
				USB_enter_bootloader();
				return true;
		}
	}
	return false;
}

void EVENT_USB_Device_ControlOUT(uint8_t* buf, uint8_t count){
	switch (usb_cmd){
		case 0xE1: // Write EEPROM
			eeprom_update_block(buf, (void*)(cmd_data*64), count);
			break;
	}
}
