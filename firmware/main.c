#include "Framework.h"

int main(void){
	USB_ConfigureClock();
	USB_Init();
	USB.INTCTRLA = USB_BUSEVIE_bm | USB_INTLVL_MED_gc;
	USB.INTCTRLB = USB_TRNIE_bm | USB_SETUPIE_bm;
	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm;
	sei();

	// tx & sck
	PORTE.DIRSET = 1 << 3 | 1 << 1 | 1 << 0;
	// rx
	PORTE.DIRCLR = 1 << 2;
	// 1mhz?
	USARTE0.BAUDCTRLA = 1;
	USARTE0.BAUDCTRLB = 1;
	// spi master, msb first, sample rising edge
	USARTE0.CTRLC = USART_CMODE_MSPI_gc;
	USARTE0.CTRLB |= USART_RXEN_bm | USART_TXEN_bm;

	DMA.CTRL = DMA_ENABLE_bm | DMA_DBUFMODE_DISABLED_gc | DMA_PRIMODE_RR0123_gc;

	// BUFFER -> .DATA
	DMA.CH0.ADDRCTRL = DMA_CH_SRCRELOAD_TRANSACTION_gc | DMA_CH_SRCDIR_INC_gc | DMA_CH_DESTRELOAD_NONE_gc | DMA_CH_DESTDIR_FIXED_gc;
	DMA.CH0.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_DRE_gc;
	DMA.CH0.DESTADDR0 = ((uint32_t)(&USARTE0.DATA) >> (8*0)) & 0xFF;
	DMA.CH0.DESTADDR1 = ((uint32_t)(&USARTE0.DATA) >> (8*1)) & 0xFF;
	DMA.CH0.DESTADDR2 = ((uint32_t)(&USARTE0.DATA) >> (8*2)) & 0xFF;
	DMA.CH0.CTRLA = DMA_CH_ENABLE_bm | DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc;

	// .DATA -> BUFFER
	DMA.CH1.ADDRCTRL = DMA_CH_SRCRELOAD_NONE_gc | DMA_CH_SRCDIR_FIXED_gc | DMA_CH_DESTRELOAD_TRANSACTION_gc | DMA_CH_DESTDIR_INC_gc;
	DMA.CH1.TRIGSRC = DMA_CH_TRIGSRC_USARTE0_RXC_gc;
	DMA.CH1.SRCADDR0 = ((uint32_t)(&USARTE0.DATA) >> (8*0)) & 0xFF;
	DMA.CH1.SRCADDR1 = ((uint32_t)(&USARTE0.DATA) >> (8*1)) & 0xFF;
	DMA.CH1.SRCADDR2 = ((uint32_t)(&USARTE0.DATA) >> (8*2)) & 0xFF;
	DMA.CH1.CTRLA = DMA_CH_ENABLE_bm | DMA_CH_SINGLE_bm | DMA_CH_BURSTLEN_1BYTE_gc;

	bool running = 0;
	for (;;) {
		if ((usb_pipe_can_read(&ep_out) && usb_pipe_can_write(&ep_in)) && ((DMA.STATUS & (DMA_CH0BUSY_bm|DMA_CH1BUSY_bm)) == 0x00)) {
			if ( running ) {
				PORTE.OUTTGL = 1;
				usb_pipe_done_read(&ep_out);
				usb_pipe_done_write(&ep_in);
			}
			uint8_t *DMASRC = usb_pipe_read_ptr(&ep_out);
			DMA.CH0.SRCADDR0 = ((uint32_t)(&DMASRC) >> (8*0)) & 0xFF;
			DMA.CH0.SRCADDR1 = ((uint32_t)(&DMASRC) >> (8*1)) & 0xFF;
			DMA.CH0.SRCADDR2 = ((uint32_t)(&DMASRC) >> (8*2)) & 0xFF;
			DMA.CH0.TRFCNT = 64;
			DMA.CH0.CTRLA |= DMA_CH_ENABLE_bm;
			DMA.CH0.CTRLA |= DMA_CH_TRFREQ_bm;
			usb_pipe_write_byte(&ep_in, 0x02);
			uint8_t *DMADST = usb_pipe_write_ptr(&ep_in);
			DMA.CH1.DESTADDR0 = ((uint32_t)(&DMADST) >> (8*0)) & 0xFF;
			DMA.CH1.DESTADDR1 = ((uint32_t)(&DMADST) >> (8*1)) & 0xFF;
			DMA.CH1.DESTADDR2 = ((uint32_t)(&DMADST) >> (8*2)) & 0xFF;
			DMA.CH1.TRFCNT = 64;
			DMA.CH1.CTRLA |= DMA_CH_ENABLE_bm;
			running = 1;
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
