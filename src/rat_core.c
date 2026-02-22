#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rat.h"
#include "rat_internal.h"

#if defined(__GNUC__) || defined(__clang__)
#define RAT_WEAK __attribute__((weak))
#else
#define RAT_WEAK
#endif

static void rat_write_u16_le(uint8_t* out, uint16_t value)
{
  out[0] = (uint8_t)(value & 0xFFu);
  out[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void rat_write_u32_le(uint8_t* out, uint32_t value)
{
  out[0] = (uint8_t)(value & 0xFFu);
  out[1] = (uint8_t)((value >> 8u) & 0xFFu);
  out[2] = (uint8_t)((value >> 16u) & 0xFFu);
  out[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static void rat_write_u64_le(uint8_t* out, uint64_t value)
{
  for (uint32_t index = 0; index < 8u; ++index)
  {
    out[index] = (uint8_t)((value >> (8u * index)) & 0xFFu);
  }
}

static uint64_t rat_fnv1a64(const uint8_t* bytes, uint32_t len)
{
  uint64_t hash = 0xcbf29ce484222325ULL;
  for (uint32_t index = 0; index < len; ++index)
  {
    hash ^= (uint64_t)bytes[index];
    hash *= 0x100000001b3ULL;
  }
  return hash;
}

static int rat_emit_control_payload(const uint8_t* payload, uint32_t len)
{
  return rat_emit(RAT_CTRL_INIT_PACKET_ID, payload, len, false);
}

RAT_WEAK const uint8_t* rat_schema_payload(uint32_t* len, uint64_t* hash)
{
  if (len != NULL)
  {
    *len = 0u;
  }
  if (hash != NULL)
  {
    *hash = 0u;
  }
  return NULL;
}

static void rat_emit_runtime_schema(void)
{
  uint32_t schema_len = 0u;
  uint64_t schema_hash = 0u;
  const uint8_t* schema = rat_schema_payload(&schema_len, &schema_hash);

  if (schema == NULL || schema_len == 0u)
  {
    return;
  }

  if (schema_hash == 0u)
  {
    schema_hash = rat_fnv1a64(schema, schema_len);
  }

  uint8_t hello[18];
  hello[0] = RAT_CTRL_SCHEMA_HELLO_OPCODE;
  hello[1] = 'R';
  hello[2] = 'A';
  hello[3] = 'T';
  hello[4] = 'S';
  hello[5] = RAT_CTRL_SCHEMA_VERSION;
  rat_write_u32_le(&hello[6], schema_len);
  rat_write_u64_le(&hello[10], schema_hash);
  if (rat_emit_control_payload(hello, (uint32_t)sizeof(hello)) <= 0)
  {
    return;
  }

  uint8_t chunk[7u + RAT_SCHEMA_CHUNK_BYTES];
  uint32_t offset = 0u;
  while (offset < schema_len)
  {
    uint32_t remaining = schema_len - offset;
    uint16_t chunk_len = (uint16_t)(remaining > RAT_SCHEMA_CHUNK_BYTES
                                        ? RAT_SCHEMA_CHUNK_BYTES
                                        : remaining);

    chunk[0] = RAT_CTRL_SCHEMA_CHUNK_OPCODE;
    rat_write_u32_le(&chunk[1], offset);
    rat_write_u16_le(&chunk[5], chunk_len);
    memcpy(&chunk[7], &schema[offset], chunk_len);

    if (rat_emit_control_payload(chunk, 7u + (uint32_t)chunk_len) <= 0)
    {
      return;
    }

    offset += (uint32_t)chunk_len;
  }

  uint8_t commit[9];
  commit[0] = RAT_CTRL_SCHEMA_COMMIT_OPCODE;
  rat_write_u64_le(&commit[1], schema_hash);
  (void)rat_emit_control_payload(commit, (uint32_t)sizeof(commit));
}

void rat_init(void)
{
  rat_rtt_init();
  rat_emit_runtime_schema();
}

int rat_emit(uint8_t packet_id, const void* data, uint32_t len, bool in_isr)
{
  return rat_rtt_write(packet_id, data, len, in_isr);
}

void rat_info(const char* fmt, ...)
{
  char buffer[RAT_INFO_MAX_LEN];
  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (written <= 0)
  {
    return;
  }

  if ((size_t)written >= sizeof(buffer))
  {
    written = (int)(sizeof(buffer) - 1u);
    buffer[written] = '\0';
  }

  (void)rat_emit(RAT_TEXT_PACKET_ID, buffer, (uint32_t)written, false);
}

#ifdef RAT_INTERNAL_TEST
#endif
