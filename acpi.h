#ifndef ACPI_H
#define ACPI_H

#include "types.h"

/*
 * ACPI table signatures
 */
#define RSDP_SIG "RSD PTR "
#define RSDT_SIG "RSDT"
#define MADT_SIG "APIC"

/*
 * Root System Description Pointer (RSDP)
 * ACPI 1.0 uses only the first 20 bytes.
 * ACPI 2.0+ extends this structure.
 */
struct rsdp {
  char     signature[8];      // "RSD PTR "
  uint8_t  checksum;          // checksum of first 20 bytes
  char     oemid[6];
  uint8_t  revision;          // 0 = ACPI 1.0, >=2 = ACPI 2.0+
  uint32_t rsdt_address;      // physical address of RSDT

  // ACPI 2.0+ fields
  uint32_t length;
  uint64_t xsdt_address;
  uint8_t  extended_checksum;
  uint8_t  reserved[3];
} __attribute__((packed));

/*
 * System Description Table Header
 * Common header for all ACPI tables
 */
struct sdth {
  char     signature[4];
  uint32_t length;            // total table length
  uint8_t  revision;
  uint8_t  checksum;
  char     oemid[6];
  char     oem_table_id[8];
  uint32_t oem_revision;
  uint32_t creator_id;
  uint32_t creator_revision;
} __attribute__((packed));

/*
 * Root System Description Table (RSDT)
 * Followed by an array of uint32_t physical addresses
 */
struct rsdt {
  struct sdth header;
  uint32_t entry[0];
} __attribute__((packed));

/*
 * Multiple APIC Description Table (MADT)
 */
struct madt {
  struct sdth header;
  uint32_t lapic_address;     // Local APIC base address
  uint32_t flags;
  uint8_t  entries[0];        // Interrupt Controller Structures
} __attribute__((packed));

/*
 * MADT Entry Type 0: Processor Local APIC
 */
struct madt_lapic {
  uint8_t  type;              // = 0
  uint8_t  length;
  uint8_t  acpi_processor_id;
  uint8_t  apic_id;
  uint32_t flags;             // bit 0 = enabled
} __attribute__((packed));

/*
 * MADT Entry Type 1: I/O APIC
 */
struct madt_ioapic {
  uint8_t  type;              // = 1
  uint8_t  length;
  uint8_t  ioapic_id;
  uint8_t  reserved;
  uint32_t ioapic_address;
  uint32_t gsib;              // Global System Interrupt Base
} __attribute__((packed));

#endif
