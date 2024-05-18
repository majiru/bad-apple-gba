#include <u.h>

typedef struct Frame {
	int l;
	uchar *d;
} Frame;

extern uchar audio[];
extern Frame frames[];

typedef u16int u16;
typedef u32int u32;
typedef volatile u32 vu32;
typedef volatile u16 vu16;
typedef u16 COLOR;
typedef unsigned int uint;

#define MEM_IO		0x04000000
#define REG_BASE	MEM_IO
#define REG_DISPCNT	*(vu32*)(REG_BASE+0x0000)
#define REG_VCOUNT			*(vu16*)(REG_BASE+0x0006)
#define DCNT_PAGE			0x0010

#define MEM_VRAM 0x06000000
#define VRAM_PAGE_SIZE	0x0A000
#define MEM_VRAM_FRONT	(MEM_VRAM)
#define MEM_VRAM_BACK	(MEM_VRAM + VRAM_PAGE_SIZE)
#define M4_WIDTH 240

#define RGB8(r,g,b)			( ((r)>>3) | (((g)>>3)<<5) | (((b)>>3)<<10) )

#define vid_mem_front	((COLOR*)MEM_VRAM)
#define vid_mem_back	((COLOR*)MEM_VRAM_BACK)

enum{
	wram = 0x03000000,
};
volatile int *frame_w = (int*)0x030000F0;
volatile int *t_w = (int*)(0x03000100);

COLOR*
vid_flip(COLOR *vid_page)
{
	if(vid_page == vid_mem_front)
		vid_page = vid_mem_back;
	else
		vid_page = vid_mem_front;
	REG_DISPCNT = REG_DISPCNT ^ DCNT_PAGE;	// update control register	

	return vid_page;
}

#define x_offset ((240 - 211) / 2)

void
bmp8_hline(int x1, int y, int x2, u32 clr, uchar *dstBase, uint dstP)
{
	u16 val, *dstL;
	int tmp;
	uint width;

	clr &= 0xFF;
	if(x2<x1){
		tmp= x1;
		x1= x2;
		x2= tmp;
	}
		
	width = x2-x1+1;
	dstL = (u16*)(dstBase+y*dstP + (x1&~1));

	if(x1&1){
		*dstL = (*dstL & 0xFF) + (clr<<8);
		width--;
		dstL++;
	}

	if(width&1)
		dstL[width/2] = (dstL[width/2]&~0xFF) + clr;
	width /= 2;

	val = clr|(clr<<8);
	for(;width > 0;width--)
		*dstL++ = val;
}

void
vid_vsync(void)
{
	while(REG_VCOUNT >= 160);
	while(REG_VCOUNT < 160);
}

void
refresh(int frame, void *vid_page)
{
	uchar *frame_data, value;
	uint frame_length;
	int x, y, amount;
	uint i;

	frame_data = frames[frame].d;
	frame_length = frames[frame].l;
	for (i = y = x = 0; i < frame_length; i += 2){
		amount = *(frame_data + i);
		value = *(frame_data + i + 1);
		bmp8_hline(x + x_offset, y, x + amount + x_offset, value, vid_page, M4_WIDTH);
		x += amount;
		if (x >= 211){
			x = 0;
			y++;
		}
	}
}

typedef void (*fnptr)(void);
#define REG_ISR_MAIN *(fnptr*)(0x03007FFC)
#define DSTAT_VBL_IRQ	0x0008	//!< Enable VBlank irq
#define REG_IE		*(vu16*)(REG_BASE+0x0200)	//!< IRQ enable
#define IRQ_VBLANK	0x0001
#define REG_IME				*(vu16*)(REG_BASE+0x0208)	//!< IRQ master enable
#define REG_IF				*(vu16*)(REG_BASE+0x0202)	//!< IRQ status/acknowledge

void
isr(void)
{
	(*t_w) = *t_w+1;
	if(*t_w % 2 == 0)
		*frame_w = *frame_w+1;
	REG_IF = IRQ_VBLANK;
}

void _isr(void);

void
main(void)
{
	COLOR *vid_page;

	vid_page = vid_mem_back;
	*t_w = 0;
	*frame_w = 0;

	REG_ISR_MAIN = _isr;

	/* Setup */
	*(vu32*)(0x04000000 +0x0000) = 0x0004 | 0x0400;
	*(vu32*)(0x04000004 +0x0000) = DSTAT_VBL_IRQ;
	((COLOR*)0x05000000)[0] = RGB8(0, 0, 0);
	((COLOR*)0x05000000)[1] = RGB8(128, 128, 128);
	((COLOR*)0x05000000)[2] = RGB8(255, 255, 255);
	*(vu16*)(0x04000000 +0x0084) = 0x8080;
	*(vu16*)(0x04000000 +0x0082) = 0x0b0f;
	*(vu32*)(0x04000000 +0x00BC) = (unsigned int)audio;
	*(vu32*)(0x04000000 +0x00C0) = (unsigned int)&*(vu32*)(0x04000000 +0x00A0);
	*(vu16*)(0x04000000 +0x00C6) = 0xb600;
	*(vu16*)(0x04000000 +0x0100) = 64775;
	*(vu16*)(0x04000000 +0x0102) = 0x0080;

	REG_IE = IRQ_VBLANK;
	REG_IME = 1;

	for(;;){
		refresh(*frame_w, vid_page);
		vid_vsync();
		vid_page = vid_flip(vid_page);
		if (*frame_w >= 6572) break;
	}
}
