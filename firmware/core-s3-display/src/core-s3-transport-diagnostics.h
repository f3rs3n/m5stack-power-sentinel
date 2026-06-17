#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Transport diagnostics are intentionally source-centric: they describe the
// CoreS3 <-> LLM Module StackFlow route without mutating the last good payload.
enum CoreS3TransportSource : uint8_t {
  CORE_S3_TRANSPORT_NONE = 0,
  CORE_S3_TRANSPORT_STACKFLOW_SERIAL = 1,
};

enum CoreS3TransportFailureKind : uint8_t {
  CORE_S3_TRANSPORT_FAILURE_NONE = 0,
  CORE_S3_TRANSPORT_FAILURE_TIMEOUT = 1,
  CORE_S3_TRANSPORT_FAILURE_JSON_PARSE = 2,
  CORE_S3_TRANSPORT_FAILURE_STACKFLOW_ERROR = 3,
  CORE_S3_TRANSPORT_FAILURE_STALE_RESPONSE = 4,
};

struct CoreS3TransportDiagnostics {
  uint32_t attemptCount = 0;
  uint32_t okCount = 0;
  uint32_t failCount = 0;
  uint32_t consecutiveFailCount = 0;
  uint32_t recoveredFailureCount = 0;
  uint32_t lastGoodMillis = 0;
  uint32_t lastFailureMillis = 0;
  CoreS3TransportSource lastGoodSource = CORE_S3_TRANSPORT_NONE;
  CoreS3TransportFailureKind lastFailureKind = CORE_S3_TRANSPORT_FAILURE_NONE;
};

inline const char *coreS3TransportSourceText(CoreS3TransportSource source) {
  switch (source) {
    case CORE_S3_TRANSPORT_STACKFLOW_SERIAL: return "StackFlow serial";
    case CORE_S3_TRANSPORT_NONE:
    default: return "transport";
  }
}

inline const char *coreS3TransportFailureText(CoreS3TransportFailureKind kind) {
  switch (kind) {
    case CORE_S3_TRANSPORT_FAILURE_TIMEOUT: return "timeout";
    case CORE_S3_TRANSPORT_FAILURE_JSON_PARSE: return "JSON parse";
    case CORE_S3_TRANSPORT_FAILURE_STACKFLOW_ERROR: return "StackFlow error";
    case CORE_S3_TRANSPORT_FAILURE_STALE_RESPONSE: return "stale response";
    case CORE_S3_TRANSPORT_FAILURE_NONE:
    default: return "failure";
  }
}

inline void coreS3TransportRecordSuccess(CoreS3TransportDiagnostics &diag, CoreS3TransportSource source, uint32_t nowMillis) {
  ++diag.attemptCount;
  ++diag.okCount;
  diag.recoveredFailureCount = diag.consecutiveFailCount;
  diag.consecutiveFailCount = 0;
  diag.lastGoodMillis = nowMillis;
  diag.lastGoodSource = source;
  diag.lastFailureKind = CORE_S3_TRANSPORT_FAILURE_NONE;
}

inline void coreS3TransportRecordFailure(CoreS3TransportDiagnostics &diag,
                                         CoreS3TransportFailureKind kind,
                                         uint32_t nowMillis,
                                         uint8_t attempt) {
  (void)attempt;
  ++diag.attemptCount;
  ++diag.failCount;
  ++diag.consecutiveFailCount;
  diag.recoveredFailureCount = 0;
  diag.lastFailureMillis = nowMillis;
  diag.lastFailureKind = kind;
}

inline bool coreS3TransportRecovered(const CoreS3TransportDiagnostics &diag) {
  return diag.recoveredFailureCount > 0 && diag.consecutiveFailCount == 0;
}

inline bool coreS3TransportShouldLogSuccess(const CoreS3TransportDiagnostics &diag) {
  return (diag.okCount == 1 && diag.failCount == 0) || coreS3TransportRecovered(diag);
}

inline bool coreS3TransportShouldLogFailure(const CoreS3TransportDiagnostics &diag, uint8_t failureLogInterval) {
  if (diag.consecutiveFailCount == 0) return false;
  if (diag.consecutiveFailCount == 1) return true;
  if (failureLogInterval == 0) return false;
  return (diag.consecutiveFailCount % failureLogInterval) == 0;
}

inline void coreS3TransportFormatStatus(const CoreS3TransportDiagnostics &diag, char *dst, size_t dstSize) {
  if (!dst || dstSize == 0) return;
  if (diag.consecutiveFailCount > 0) {
    const uint32_t lastOkAge = diag.lastGoodMillis == 0 || diag.lastFailureMillis < diag.lastGoodMillis
                                   ? 0
                                   : diag.lastFailureMillis - diag.lastGoodMillis;
    snprintf(dst,
             dstSize,
             "StackFlow %s fail x%lu last_ok=%lums",
             coreS3TransportFailureText(diag.lastFailureKind),
             static_cast<unsigned long>(diag.consecutiveFailCount),
             static_cast<unsigned long>(lastOkAge));
    return;
  }
  if (coreS3TransportRecovered(diag)) {
    snprintf(dst,
             dstSize,
             "%s OK recovered x%lu",
             coreS3TransportSourceText(diag.lastGoodSource),
             static_cast<unsigned long>(diag.recoveredFailureCount));
    return;
  }
  snprintf(dst, dstSize, "%s OK", coreS3TransportSourceText(diag.lastGoodSource));
}
