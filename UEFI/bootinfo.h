#ifndef BOOTINFO_H
#define BOOTINFO_H

/* font */
struct PSF_FONT{
    uint8_t        Signature[2];
    uint8_t        mode;
    uint8_t        charsize;
    char*        glyphBuffer[256];
}__attribute__((packed));


/* GOP */
struct Framebuffer{
	void* BaseAddress;
	void* BaseAddressBackground;
    size_t FrameBufferSize;
	unsigned int Width;
	unsigned int Height;
	unsigned int PixelsPerScanLine;
}__attribute__((packed));

/* Memory */
struct MemoryInfo{
	EFI_MEMORY_DESCRIPTOR* mMap;
	uint64_t mMapSize;
	uint64_t mMapDescSize;

	uint64_t UEFI_CR3;
	uint64_t VirtualKernelStart;
	uint64_t VirtualKernelEnd;
}__attribute__((packed));
/* Boot info stuct */

struct BootInfo{
	struct Framebuffer framebuffer;
	struct PSF_FONT* psf1_Font;
	struct MemoryInfo memoryInfo;
    void* rsdp; //read system descriptor pointer
	void* smbios; //bios info
}__attribute__((packed));

#endif