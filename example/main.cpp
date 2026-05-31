#include <ntddk.h>
#include <cstdint>

extern "C" uint64_t HvCall(uint64_t call);

auto HyperCall(uint64_t call) -> uint64_t
{
    GROUP_AFFINITY new_affinity = {};
    GROUP_AFFINITY old_affinity = {};

    new_affinity.Mask = 1;
    old_affinity.Group = 0;

    KeSetSystemGroupAffinityThread(&new_affinity, &old_affinity);

    uint64_t result = HvCall(call);

    KeRevertToUserGroupAffinityThread(&old_affinity);

    return result;
}

extern "C" auto
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
-> NTSTATUS;

auto DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) -> NTSTATUS
{
	UNREFERENCED_PARAMETER(DriverObject);
    UNREFERENCED_PARAMETER(RegistryPath);

    uint64_t ver = HyperCall(0x1);
    DbgPrint("Version: 0x%llx\n", ver);
    uint64_t ping = HyperCall(0x2);
    DbgPrint("ping: 0x%llx\n", ping);
	return STATUS_SUCCESS;
}