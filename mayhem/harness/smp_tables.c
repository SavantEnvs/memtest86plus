// The consumers of the ACPI tables acpi_init() locates: the MADT (CPU list) and SRAT (NUMA
// ranges) parsers of system/smp.c, linked into fuzz_acpi only.
//
// Those parsers are static, so upstream system/smp.c is compiled unmodified inside this
// translation unit. mayhem_smp_parse_acpi_tables() runs the table-parsing part of smp_init()
// in the same order and with the same per-call state reset: find_cpus_in_madt(),
// verify_bsp_is_cpu0() (only when a local APIC was mapped), find_numa_nodes_in_srat(),
// check_if_needs_to_map() and sort_and_validate_memory_affinity_ranges().
//
// Left out of smp_init(), because a host process cannot model them: the x2APIC probe (rdmsr; it
// is skipped because cpuid_info reports no x2APIC, as on the boot path when the CPU lacks it),
// the MP floating-pointer fallback when the MADT is missing or invalid (it scans physical page 0,
// which a host process cannot map), and the AP trampoline/heap setup (no tables are read there).
#include "smp.c"

// Link-time symbols smp.c references from code this harness never calls (AP start-up, heap).
uint8_t     startup[1];
uint8_t     ap_trampoline[1];
uint8_t     ap_trampoline_end[1];
uint32_t    ap_startup_addr;
cpuid_info_t cpuid_info;   // zeroed: no x2APIC, so apic_x2apic stays false
quirk_t     quirk;         // zeroed: no SMP quirk
void barrier_init(barrier_t *barrier, int num_threads) { (void)barrier; (void)num_threads; }
uintptr_t heap_alloc(heap_type_t heap_id, size_t size, uintptr_t alignment)
{
    (void)heap_id; (void)size; (void)alignment;
    return 0;
}

void mayhem_smp_parse_acpi_tables(void)
{
    // smp_init()'s reset of the topology state, so each input starts from a clean slate.
    memset(cpu_num_to_proximity_domain_idx, 0, sizeof cpu_num_to_proximity_domain_idx);
    memset(cpu_num_to_apic_id, 0, sizeof cpu_num_to_apic_id);
    memset(cpu_num_to_domain_assigned, 0, sizeof cpu_num_to_domain_assigned);
    memset(proximity_domains, 0, sizeof proximity_domains);
    for (int i = 0; i < (int)ARRAY_SIZE(memory_affinity_ranges); i++) {
        memory_affinity_ranges[i].proximity_domain_idx = UINT32_C(0xFFFFFFFF);
        memory_affinity_ranges[i].start = 0;
        memory_affinity_ranges[i].end = 0;
    }
    num_available_cpus = 1;
    num_memory_affinity_ranges = 0;
    num_proximity_domains = 0;
    smp_topology_too_large = false;
    apic = NULL;
    apic_x2apic = false;

    if (find_cpus_in_madt() && apic != NULL) {
        verify_bsp_is_cpu0();
    }

    int srat_status = find_numa_nodes_in_srat();
    if (srat_status > 0) {
        check_if_needs_to_map();
        if (!sort_and_validate_memory_affinity_ranges()) {
            srat_status = 0;
        }
    }
    if (srat_status <= 0) {
        num_proximity_domains = 0;
        num_memory_affinity_ranges = 0;
        smp_topology_too_large = (srat_status < 0);
    }
}
