#include "kernelInit.h"

graphics r = graphics(NULL);
CPU cpu;

uint64_t LastVirtualAddressUsed = 0;
uint64_t memorySize = 0;

PageTable* InitializeMemory(BootInfo* bootInfo){
    uint64_t mMapEntries = bootInfo->memoryInfo.mMapSize / bootInfo->memoryInfo.mMapDescSize;

    globalAllocator = PageFrameAllocator();

    globalAllocator.ReadEFIMemoryMap(bootInfo->memoryInfo.mMap, bootInfo->memoryInfo.mMapSize, bootInfo->memoryInfo.mMapDescSize);
    uint64_t fbBase = (uint64_t)bootInfo->framebuffer.BaseAddress;
    uint64_t fbSize = (uint64_t)bootInfo->framebuffer.FrameBufferSize + 0x1000;
    globalAllocator.LockPages((void*)fbBase, Divide(fbSize, 0x1000));

    PageTable* PML4 = (PageTable*)globalAllocator.RequestPage();
    memset(PML4, 0, 0x1000);
    globalPageTableManager[0].PageTableManagerInit(PML4);

    PageTableManager UEFI_Table;
    UEFI_Table.PageTableManagerInit((PageTable*)bootInfo->memoryInfo.UEFI_CR3);

    //map kernel
    uint64_t KernelPageSize = Divide(bootInfo->memoryInfo.VirtualKernelEnd - bootInfo->memoryInfo.VirtualKernelStart, 0x1000);
    LastVirtualAddressUsed = bootInfo->memoryInfo.VirtualKernelEnd;

    LastVirtualAddressUsed += 0x1000 - (LastVirtualAddressUsed % 0x1000);
    globalPageTableManager[0].DefinePhysicalMemoryLocation((void*)(LastVirtualAddressUsed + 0x1000));

    for(uint64_t i = 0; i < KernelPageSize; i++){
        void* VirtualAddress = (void*)(bootInfo->memoryInfo.VirtualKernelStart + i * 0x1000);
        void* PhysicalAddress = UEFI_Table.GetPhysicalAddress(VirtualAddress);
        globalPageTableManager[0].MapMemory(VirtualAddress, PhysicalAddress);
    }
    
    memorySize = GetMemorySize(bootInfo->memoryInfo.mMap, mMapEntries, bootInfo->memoryInfo.mMapDescSize);

    //map all the memory

    for(uint64_t i = 0; i < memorySize; i += 0x1000){
        LastVirtualAddressUsed += 0x1000;
        globalPageTableManager[0].MapMemory((void*)LastVirtualAddressUsed, (void*)i);
    }
    LastVirtualAddressUsed += 0x1000;

    globalPageTableManager[0].DefineVirtualTableLocation();

    return PML4;
}

void InitializeACPI(BootInfo* bootInfo){
    bootInfo->rsdp = (ACPI::RSDP2*)globalPageTableManager[0].GetVirtualAddress(bootInfo->rsdp);
    
    ACPI::SDTHeader* xsdt = (ACPI::SDTHeader*)globalPageTableManager[0].GetVirtualAddress((void*)bootInfo->rsdp->XSDTAddress);
    ACPI::MCFGHeader* mcfg = (ACPI::MCFGHeader*)ACPI::FindTable(xsdt, (char*)"MCFG");
    PCI::EnumeratePCI(mcfg);

    ACPI::MADTHeader* madt = (ACPI::MADTHeader*)ACPI::FindTable(xsdt, (char*)"APIC");
    APIC::InitializeMADT(madt);

    ACPI::FADTHeader* fadt = (ACPI::FADTHeader*)ACPI::FindTable(xsdt, (char*)"FACP");
    ACPI::InitializeFADT(fadt);

    ACPI::HPETHeader* hpet = (ACPI::HPETHeader*)ACPI::FindTable(xsdt, (char*)"HPET");
    HPET::InitialiseHPET(hpet);
}
  

void InitializeKernel(BootInfo* bootInfo){   
    asm("cli");

    SaveCoreID();

    uint8_t CoreID = GetCoreID();

    globalCOM1->Initialize();
    globalCOM1->ClearMonitor();
    globalLogs->Message("(c) Kot Corporation. All rights reserved");
    
    gdtInit();
    globalLogs->Successful("GDT intialize");


    InitializeInterrupts();  
    globalLogs->Successful("IDT intialize");
    memset(bootInfo->framebuffer.BaseAddress, 0xff, bootInfo->framebuffer.FrameBufferSize);

    PageTable* PML4 = InitializeMemory(bootInfo);
    LoadPaging(PML4, globalPageTableManager[0].PhysicalMemoryVirtualAddress);
    globalLogs->Successful("Memory intialize");

    //Update bootinfo location
    bootInfo = (BootInfo*)globalPageTableManager[0].GetVirtualAddress(bootInfo);


    globalLogs->Message("CPU : %s %s", globalCPU.getName(), globalCPU.getVendorID());
    globalCPU.getFeatures();
    globalLogs->Message("CPU features :");
    for(int i = 0; i < globalCPU.cpuFeatures; i++){
        globalCOM1->Print(globalCPU.features[i]);
        globalCOM1->Print(" | ");
    }
    globalCOM1->Print("\n");

    InitializeHeap((void*)LastVirtualAddressUsed, 0x10);
    globalLogs->Successful("Heap intialize");

    r = graphics(bootInfo);
    globalGraphics = &r;
    globalGraphics->framebuffer->BaseAddressBackground = malloc(globalGraphics->framebuffer->FrameBufferSize);
    memset(globalGraphics->framebuffer->BaseAddressBackground, 0, globalGraphics->framebuffer->FrameBufferSize);
    globalGraphics->Update();
    globalLogs->Successful("Graphics intialize");

    if(EnabledSSE() == 0){
        FPUInit();
        globalLogs->Successful("FPU intialize");
    }else{
        globalLogs->Successful("SSE intialize");
    }
    
    InitializeACPI(bootInfo);

    //Init file system
    fileSystem = new OSFileSystem(AHCI::ahciDriver->PartitionsList);
    
    InitPS2Mouse();

    globalTaskManager = (TaskManager*)calloc(sizeof(TaskManager));
    globalTaskManager->InitScheduler(APIC::ProcessorCount);

    fileSystem->mkdir("Alpha:/system", 777);
    fileSystem->mkdir("Alpha:/system/background", 777);   
    fileSystem->mkdir("Alpha:/system/apps", 777);  
    
    FileSystem::File* app = (FileSystem::File*)malloc(sizeof(FileSystem::File));
    fileSystem->fopen("Alpha:/system/apps/main.elf", "r", app);
    void* appBuffer = malloc(app->fileInfo.BytesSize);
    app->Read(0, app->fileInfo.BytesSize, appBuffer);
    Parameters FunctionParameters;
    FunctionParameters.Parameter0 = (uint64_t)0xff;
    ELF::loadElf(appBuffer, 1, "System", &FunctionParameters);

    APIC::EnableAPIC(CoreID);
    APIC::localApicEOI(CoreID);
    APIC::StartLapicTimer();

    //Load Kernel Service
    KernelIPC::Initialize();

    APIC::LoadCores(); 

    globalTaskManager->EnabledScheduler(CoreID);
    asm("sti");

    return;
}