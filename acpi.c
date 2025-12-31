#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "acpi.h"

struct cpu cpus[NCPU];
int ncpu;
uchar ioapicid;

/*
 * Calculate ACPI checksum
 * Sum of all bytes must be zero
 */
static int
acpi_checksum(void *addr, int len)
{
  uint8_t sum = 0;
  uint8_t *p = addr;

  for (int i = 0; i < len; i++)
    sum += p[i];

  return sum == 0;
}

/*
 * Search RSDP in a given physical memory range
 * Must be aligned on 16-byte boundaries
 */
static struct rsdp *
find_rsdp_range(uint32_t start, uint32_t end)
{
  for (uint32_t p = start; p < end; p += 16) {
    struct rsdp *rsdp = (struct rsdp *)P2V(p);

    if (memcmp(rsdp->signature, RSDP_SIG, 8) != 0)
      continue;

    if (!acpi_checksum(rsdp, 20))
      continue;

    // ACPI 2.0+ extended checksum
    if (rsdp->revision >= 2 &&
        !acpi_checksum(rsdp, rsdp->length))
      continue;

    return rsdp;
  }

  return 0;
}

/*
 * Locate the RSDP according to ACPI Spec 5.2.5.1
 * 1. First 1KB of EBDA
 * 2. BIOS ROM area 0xE0000 - 0xFFFFF
 */
static struct rsdp *
find_rsdp(void)
{
  uint16_t *ebda_seg;
  uint32_t ebda_addr;

  // EBDA segment pointer is stored at BIOS data area 0x40E
  ebda_seg = (uint16_t *)P2V(0x40E);
  ebda_addr = ((uint32_t)(*ebda_seg)) << 4;

  // Search first 1KB of EBDA
  if (ebda_addr) {
    struct rsdp *r = find_rsdp_range(ebda_addr, ebda_addr + 1024);
    if (r)
      return r;
  }

  // Search BIOS ROM area
  return find_rsdp_range(0xE0000, 0x100000);
}

/*
 * ACPI initialization
 * - Find RSDP
 * - Locate RSDT
 * - Find MADT
 * - Enumerate CPUs and I/O APIC
 */
void
acpi_init(void)
{
  struct rsdp *rsdp;
  struct rsdt *rsdt;
  struct madt *madt;
  uint8_t *p, *end;

  rsdp = find_rsdp();
  if (!rsdp)
    panic("ACPI: RSDP not found");

  // We only support RSDT (32-bit)
  if (rsdp->revision >= 2 && rsdp->xsdt_address)
    panic("ACPI: XSDT not supported");

  rsdt = (struct rsdt *)P2V(rsdp->rsdt_address);

  int entries = (rsdt->header.length - sizeof(struct sdth)) / 4;
  madt = 0;

  for (int i = 0; i < entries; i++) {
    struct sdth *h = (struct sdth *)P2V(rsdt->entry[i]);
    if (memcmp(h->signature, MADT_SIG, 4) == 0) {
      madt = (struct madt *)h;
      break;
    }
  }

  if (!madt)
    panic("ACPI: MADT not found");

  // Set Local APIC base address
  lapic = (volatile uint *)madt->lapic_address;

  // Parse MADT entries
  p = madt->entries;
  end = (uint8_t *)madt + madt->header.length;

  ncpu = 0;

  while (p < end) {
    uint8_t type = p[0];
    uint8_t len  = p[1];

    switch (type) {
    case 0: { // Processor Local APIC
      struct madt_lapic *lapic = (struct madt_lapic *)p;
      if (lapic->flags & 1) {
        if (ncpu < NCPU) {
          cpus[ncpu].apicid = lapic->apic_id;
          ncpu++;
        }
      }
      break;
    }

    case 1: { // I/O APIC
      struct madt_ioapic *io = (struct madt_ioapic *)p;
      ioapicid = io->ioapic_id;
      break;
    }

    default:
      break;
    }

    p += len;
  }

  if (ncpu == 0)
    panic("ACPI: no enabled CPUs");
}
