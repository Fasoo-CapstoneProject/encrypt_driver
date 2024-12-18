#include <fltKernel.h>
#include <ntifs.h>

#define SIOCTL_TYPE 40000
#define IOCTL_HELLO CTL_CODE(SIOCTL_TYPE, 0x800, METHOD_BUFFERED, FILE_READ_DATA|FILE_WRITE_DATA)

#define XOR_KEY 0xAA
#define MAX_PATH 260

const WCHAR deviceNameBuffer[] = L"\\Device\\MYDEVICE";
const WCHAR deviceSymLinkBuffer[] = L"\\DosDevices\\MyDevice";
PDEVICE_OBJECT g_MyDevice;

// Stream context structure
typedef struct _MY_STREAM_CONTEXT {
    BOOLEAN IsTargetFile;
    BOOLEAN IsEncoded;
} MY_STREAM_CONTEXT, *PMY_STREAM_CONTEXT;

PFLT_FILTER gFilterHandle = NULL;
UNICODE_STRING targetPath;  // Global variable to store the path from DeviceIoControl

// Forward declarations
DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags);
VOID OnUnload(IN PDRIVER_OBJECT DriverObject);
NTSTATUS Function_IRP_MJ_CREATE(PDEVICE_OBJECT pDeviceObject, PIRP Irp);
NTSTATUS Function_IRP_MJ_CLOSE(PDEVICE_OBJECT pDeviceObject, PIRP Irp);
NTSTATUS Function_IRP_DEVICE_CONTROL(PDEVICE_OBJECT pDeviceObject, PIRP Irp);

FLT_PREOP_CALLBACK_STATUS PreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_POSTOP_CALLBACK_STATUS PostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);
FLT_PREOP_CALLBACK_STATUS PreRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_POSTOP_CALLBACK_STATUS PostRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);
FLT_PREOP_CALLBACK_STATUS PreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext);
FLT_POSTOP_CALLBACK_STATUS PostWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags);

// Minifilter operation callbacks
CONST FLT_OPERATION_REGISTRATION Callbacks[] = {
    { IRP_MJ_CREATE, 0, PreCreate, PostCreate },
    { IRP_MJ_READ, 0, PreRead, PostRead },
    { IRP_MJ_WRITE, 0, PreWrite, PostWrite },
    { IRP_MJ_OPERATION_END }
};

// Context registration
CONST FLT_CONTEXT_REGISTRATION ContextRegistration[] = {
    {
        FLT_STREAM_CONTEXT,
        0,
        NULL,
        sizeof(MY_STREAM_CONTEXT),
        'txet'
    },
    { FLT_CONTEXT_END }
};

// Filter registration
CONST FLT_REGISTRATION FilterRegistration = {
    sizeof(FLT_REGISTRATION),
    FLT_REGISTRATION_VERSION,
    0,
    ContextRegistration,
    Callbacks,
    DriverUnload,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

NTSTATUS Function_IRP_MJ_CREATE(PDEVICE_OBJECT pDeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    DbgPrint("IRP MJ CREATE received.\n");
    return STATUS_SUCCESS;
}

NTSTATUS Function_IRP_MJ_CLOSE(PDEVICE_OBJECT pDeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    UNREFERENCED_PARAMETER(Irp);
    DbgPrint("IRP MJ CLOSE received.\n");
    return STATUS_SUCCESS;
}

NTSTATUS Function_IRP_DEVICE_CONTROL(PDEVICE_OBJECT pDeviceObject, PIRP Irp)
{
    UNREFERENCED_PARAMETER(pDeviceObject);
    
    PIO_STACK_LOCATION pIoStackLocation = IoGetCurrentIrpStackLocation(Irp);
    PVOID pBuf = Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_SUCCESS;

    switch (pIoStackLocation->Parameters.DeviceIoControl.IoControlCode)
    {
    case IOCTL_HELLO:
        {
            // 입력 버퍼 길이 확인 (WCHAR 문자열)
            ULONG inputLength = pIoStackLocation->Parameters.DeviceIoControl.InputBufferLength;
            if (inputLength < sizeof(WCHAR) || pBuf == NULL) {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            // NULL 종료 WCHAR 문자열인지 확인
            PWCHAR inputPath = (PWCHAR)pBuf;
            ULONG charCount = inputLength / sizeof(WCHAR);
            if (inputPath[charCount - 1] != L'\0') {
                status = STATUS_INVALID_PARAMETER;
                break;
            }

            DbgPrint("IOCTL HELLO received with full path: %ws\n", inputPath);
            
            // Free previous target path if exists
            if (targetPath.Buffer != NULL) {
                RtlFreeUnicodeString(&targetPath);
            }

            // Calculate required buffer size
            SIZE_T pathLen = wcslen(inputPath) * sizeof(WCHAR);
            SIZE_T bufferSize = pathLen + sizeof(WCHAR);  // Add space for null terminator

            // Allocate new buffer
            PWCHAR newBuffer = (PWCHAR)ExAllocatePoolWithTag(NonPagedPool, bufferSize, 'tPaT');
            if (newBuffer == NULL) {
                status = STATUS_INSUFFICIENT_RESOURCES;
                DbgPrint("Failed to allocate memory for target path\n");
                break;
            }

            // Copy the string
            RtlCopyMemory(newBuffer, inputPath, pathLen);
            newBuffer[pathLen/sizeof(WCHAR)] = L'\0';  // Ensure null termination

            // Set up the UNICODE_STRING
            targetPath.Length = (USHORT)pathLen;
            targetPath.MaximumLength = (USHORT)bufferSize;
            targetPath.Buffer = newBuffer;

            DbgPrint("Target path set to: %wZ\n", &targetPath);
        }
        break;
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

FLT_PREOP_CALLBACK_STATUS PreCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS PostCreate(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    if (!NT_SUCCESS(Data->IoStatus.Status)) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // Skip if target path is not set
    if (targetPath.Buffer == NULL || targetPath.Length == 0) {
        DbgPrint("[PostCreate] Target path is not set yet\n");
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    PMY_STREAM_CONTEXT context = NULL;
    NTSTATUS status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    if (NT_SUCCESS(status)) {
        DbgPrint("[PostCreate] Context already exists for the file\n");
        FltReleaseContext(context);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    PFLT_FILE_NAME_INFORMATION nameInfo;
    status = FltGetFileNameInformation(Data, FLT_FILE_NAME_NORMALIZED | FLT_FILE_NAME_QUERY_DEFAULT, &nameInfo);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[PostCreate] Failed to get file name information. Status: 0x%X\n", status);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    status = FltParseFileNameInformation(nameInfo);
    if (!NT_SUCCESS(status)) {
        DbgPrint("[PostCreate] Failed to parse file name information. Status: 0x%X\n", status);
        FltReleaseFileNameInformation(nameInfo);
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    // Print file name for debugging
    DbgPrint("[PostCreate] Processing file: %wZ\n", &nameInfo->Name);
    DbgPrint("[PostCreate] Target path is: %wZ\n", &targetPath);
    DbgPrint("[PostCreate] File extension: %wZ\n", &nameInfo->Extension);
    DbgPrint("[PostCreate] Extension length: %d\n", nameInfo->Extension.Length);

    // Check if the file has .txt extension
    if (nameInfo->Extension.Length > 0) {
        UNICODE_STRING txtExtension;
        RtlInitUnicodeString(&txtExtension, L"txt");  // 점(.) 제거
        
        DbgPrint("[PostCreate] Comparing extensions - File: %wZ (len: %d) vs Expected: %wZ (len: %d)\n",
                &nameInfo->Extension, nameInfo->Extension.Length,
                &txtExtension, txtExtension.Length);
        
        if (RtlEqualUnicodeString(&nameInfo->Extension, &txtExtension, TRUE)) {
            DbgPrint("[PostCreate] File has .txt extension\n");
            
            // Get the file's directory
            UNICODE_STRING directory;
            directory.Length = nameInfo->ParentDir.Length;
            directory.MaximumLength = nameInfo->ParentDir.MaximumLength;
            directory.Buffer = nameInfo->ParentDir.Buffer;
            
            DbgPrint("[PostCreate] File directory: %wZ\n", &directory);

            // Check if target path contains wildcard
            BOOLEAN isWildcard = FALSE;
            PWCHAR lastBackslash = NULL;
            PWCHAR targetPathStr = targetPath.Buffer;
            SIZE_T targetLen = targetPath.Length / sizeof(WCHAR);

            // Find the last backslash and check for wildcard
            for (SIZE_T i = 0; i < targetLen; i++) {
                if (targetPathStr[i] == L'\\') {
                    lastBackslash = &targetPathStr[i];
                }
                if (targetPathStr[i] == L'*') {
                    isWildcard = TRUE;
                }
            }

            if (isWildcard && lastBackslash != NULL) {
                // Extract directory path from target path (up to last backslash)
                UNICODE_STRING targetDir;
                targetDir.Buffer = targetPath.Buffer;
                targetDir.Length = (USHORT)((ULONG_PTR)lastBackslash - (ULONG_PTR)targetPath.Buffer + sizeof(WCHAR));  // Include the last backslash
                targetDir.MaximumLength = targetDir.Length;

                DbgPrint("[PostCreate] Converted DOS path length: %d\n", directory.Length);
                DbgPrint("[PostCreate] Target directory length: %d\n", targetDir.Length);

                // Convert NT path to DOS path format for comparison
                WCHAR dosPathBuffer[MAX_PATH];
                UNICODE_STRING dosPath;
                UNICODE_STRING usersStr;
                UNICODE_STRING temp;
                RtlZeroMemory(dosPathBuffer, sizeof(dosPathBuffer));
                dosPath.Buffer = dosPathBuffer;
                dosPath.MaximumLength = sizeof(dosPathBuffer);

                // Find \Device\HarddiskVolumeX\ prefix
                PWCHAR ntPath = directory.Buffer;
                ULONG ntPathLen = directory.Length / sizeof(WCHAR);
                ULONG startIndex = 0;

                // Find Users directory in path
                RtlInitUnicodeString(&usersStr, L"\\Users\\");
                
                for (ULONG i = 0; i < ntPathLen - (usersStr.Length / sizeof(WCHAR)); i++) {
                    temp.Buffer = &ntPath[i];
                    temp.Length = usersStr.Length;
                    temp.MaximumLength = usersStr.Length;

                    if (RtlCompareUnicodeString(&temp, &usersStr, TRUE) == 0) {
                        startIndex = i;
                        break;
                    }
                }

                // Construct DOS path
                WCHAR driveLetter = L'C';  // Assuming C: drive
                dosPath.Buffer[0] = driveLetter;
                dosPath.Buffer[1] = L':';

                // Copy from \Users\... to the end
                RtlCopyMemory(&dosPath.Buffer[2],
                            &directory.Buffer[startIndex],
                            directory.Length - (startIndex * sizeof(WCHAR)));

                dosPath.Length = (USHORT)(4 + directory.Length - (startIndex * sizeof(WCHAR)));

                DbgPrint("[PostCreate] Converted DOS path: %wZ\n", &dosPath);
                DbgPrint("[PostCreate] Target directory: %wZ\n", &targetDir);

                // Compare the paths
                if (RtlCompareUnicodeString(&dosPath, &targetDir, TRUE) == 0) {
                    DbgPrint("[PostCreate] Directory match found!\n");
                    goto CreateContext;
                } else {
                    DbgPrint("[PostCreate] Directory match failed - different lengths or content\n");
                }
            } else {
                // Original exact path matching logic
                if (RtlEqualUnicodeString(&nameInfo->Name, &targetPath, TRUE)) {
                    DbgPrint("[PostCreate] Exact file path match found\n");
                    goto CreateContext;
                }
            }
        } else {
            DbgPrint("[PostCreate] File does not have .txt extension\n");
        }
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_POSTOP_FINISHED_PROCESSING;

CreateContext:
    // Allocate and set up the context
    status = FltAllocateContext(FltObjects->Filter,
                              FLT_STREAM_CONTEXT,
                              sizeof(MY_STREAM_CONTEXT),
                              NonPagedPool,
                              (PFLT_CONTEXT*)&context);
    
    if (NT_SUCCESS(status)) {
        context->IsTargetFile = TRUE;
        context->IsEncoded = FALSE;
        
        status = FltSetStreamContext(FltObjects->Instance,
                                   FltObjects->FileObject,
                                   FLT_SET_CONTEXT_KEEP_IF_EXISTS,
                                   context,
                                   NULL);
        
        if (NT_SUCCESS(status)) {
            DbgPrint("[PostCreate] Successfully set stream context\n");
        } else {
            DbgPrint("[PostCreate] Failed to set stream context. Status: 0x%X\n", status);
        }
        
        FltReleaseContext(context);
    } else {
        DbgPrint("[PostCreate] Failed to allocate context. Status: 0x%X\n", status);
    }

    FltReleaseFileNameInformation(nameInfo);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS PreRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    
    PMY_STREAM_CONTEXT context = NULL;
    NTSTATUS status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    
    if (NT_SUCCESS(status)) {
        if (context->IsTargetFile) {
            *CompletionContext = (PVOID)XOR_KEY;
            FltReleaseContext(context);
            return FLT_PREOP_SUCCESS_WITH_CALLBACK;
        }
        FltReleaseContext(context);
    }
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS PostRead(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(Flags);

    if (!CompletionContext || !NT_SUCCESS(Data->IoStatus.Status) || Data->IoStatus.Information == 0) {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    PVOID buffer = NULL;
    if (Data->Iopb->Parameters.Read.MdlAddress != NULL) {
        buffer = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Read.MdlAddress, NormalPagePriority);
    } else {
        buffer = Data->Iopb->Parameters.Read.ReadBuffer;
    }

    if (buffer) {
        PUCHAR byteBuffer = (PUCHAR)buffer;
        for (SIZE_T i = 0; i < Data->IoStatus.Information; i++) {
            byteBuffer[i] ^= (UCHAR)(ULONG_PTR)CompletionContext;
        }
    }

    return FLT_POSTOP_FINISHED_PROCESSING;
}

FLT_PREOP_CALLBACK_STATUS PreWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID *CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    
    PMY_STREAM_CONTEXT context = NULL;
    NTSTATUS status = FltGetStreamContext(FltObjects->Instance, FltObjects->FileObject, (PFLT_CONTEXT*)&context);
    
    if (NT_SUCCESS(status)) {
        if (context->IsTargetFile && !context->IsEncoded) {
            PVOID buffer = NULL;
            if (Data->Iopb->Parameters.Write.MdlAddress != NULL) {
                buffer = MmGetSystemAddressForMdlSafe(Data->Iopb->Parameters.Write.MdlAddress, NormalPagePriority);
            } else {
                buffer = Data->Iopb->Parameters.Write.WriteBuffer;
            }

            if (buffer) {
                PUCHAR byteBuffer = (PUCHAR)buffer;
                for (SIZE_T i = 0; i < Data->Iopb->Parameters.Write.Length; i++) {
                    byteBuffer[i] ^= XOR_KEY;
                }
                context->IsEncoded = TRUE;
            }
        }
        FltReleaseContext(context);
    }
    
    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS PostWrite(PFLT_CALLBACK_DATA Data, PCFLT_RELATED_OBJECTS FltObjects, PVOID CompletionContext, FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

NTSTATUS DriverUnload(_In_ FLT_FILTER_UNLOAD_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Flags);
    DbgPrint("Driver unload started\n");

    // Free the target path if allocated
    if (targetPath.Buffer != NULL) {
        RtlFreeUnicodeString(&targetPath);
        targetPath.Buffer = NULL;
        targetPath.Length = 0;
        targetPath.MaximumLength = 0;
    }

    // Unregister the filter
    if (gFilterHandle != NULL) {
        FltUnregisterFilter(gFilterHandle);
        gFilterHandle = NULL;
    }

    DbgPrint("Driver unload completed\n");
    return STATUS_SUCCESS;
}

VOID OnUnload(IN PDRIVER_OBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);
    UNICODE_STRING symbolicLinkUnicodeString;
    
    DbgPrint("OnUnload started\n");

    // Delete symbolic link
    RtlInitUnicodeString(&symbolicLinkUnicodeString, deviceSymLinkBuffer);
    IoDeleteSymbolicLink(&symbolicLinkUnicodeString);

    // Delete device object
    if (g_MyDevice != NULL) {
        IoDeleteDevice(g_MyDevice);
        g_MyDevice = NULL;
    }

    // Free the target path if allocated
    if (targetPath.Buffer != NULL) {
        RtlFreeUnicodeString(&targetPath);
        targetPath.Buffer = NULL;
        targetPath.Length = 0;
        targetPath.MaximumLength = 0;
    }

    DbgPrint("OnUnload completed\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;
    UNICODE_STRING deviceNameUnicodeString, deviceSymLinkUnicodeString;

    DbgPrint("Driver entry started\n");

    // Initialize targetPath
    RtlInitUnicodeString(&targetPath, NULL);

    // Initialize device name and symlink
    RtlInitUnicodeString(&deviceNameUnicodeString, deviceNameBuffer);
    RtlInitUnicodeString(&deviceSymLinkUnicodeString, deviceSymLinkBuffer);

    // Create device
    status = IoCreateDevice(DriverObject,
                          0,
                          &deviceNameUnicodeString,
                          FILE_DEVICE_UNKNOWN,
                          FILE_DEVICE_SECURE_OPEN,
                          FALSE,
                          &g_MyDevice);

    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create device. Status: 0x%X\n", status);
        return status;
    }

    // Create symbolic link
    status = IoCreateSymbolicLink(&deviceSymLinkUnicodeString, &deviceNameUnicodeString);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to create symbolic link. Status: 0x%X\n", status);
        IoDeleteDevice(g_MyDevice);
        return status;
    }

    // Set up dispatch routines
    DriverObject->MajorFunction[IRP_MJ_CREATE] = Function_IRP_MJ_CREATE;
    DriverObject->MajorFunction[IRP_MJ_CLOSE] = Function_IRP_MJ_CLOSE;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = Function_IRP_DEVICE_CONTROL;
    DriverObject->DriverUnload = OnUnload;

    // Register with the filter manager
    status = FltRegisterFilter(DriverObject, &FilterRegistration, &gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to register filter. Status: 0x%X\n", status);
        IoDeleteSymbolicLink(&deviceSymLinkUnicodeString);
        IoDeleteDevice(g_MyDevice);
        return status;
    }

    // Start filtering
    status = FltStartFiltering(gFilterHandle);
    if (!NT_SUCCESS(status)) {
        DbgPrint("Failed to start filtering. Status: 0x%X\n", status);
        FltUnregisterFilter(gFilterHandle);
        IoDeleteSymbolicLink(&deviceSymLinkUnicodeString);
        IoDeleteDevice(g_MyDevice);
        return status;
    }

    DbgPrint("Driver entry completed successfully\n");
    return status;
}
